#include "cleanup/CleanupExecutor.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <optional>
#include <utility>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QUuid>

#ifdef Q_OS_WIN
#include <ShObjIdl.h>
#include <windows.h>
#endif

#include "domain/DomainError.h"
#include "platform/windows/NativeFileMetadata.h"

namespace foldersnap
{
namespace
{
constexpr qsizetype kMaximumCleanupAuditBytes = 64 * 1024 * 1024;

void checkCancelled(const CleanupExecutor::CancellationCallback &cancelled)
{
    if (cancelled && cancelled())
    {
        throw DomainError(ErrorCode::Cancelled, "Cleanup was cancelled.");
    }
}

int pathDepth(const QString &path)
{
    return path.count('/');
}

bool isSuccessful(const CleanupExecutionItem &item)
{
    return item.status == CleanupStatus::MovedToRecycleBin ||
           item.status == CleanupStatus::AlreadyMissing;
}

void summarize(CleanupExecutionResult &result)
{
    result.summary = {};
    for (const CleanupExecutionItem &item : result.items)
    {
        switch (item.status)
        {
        case CleanupStatus::MovedToRecycleBin:
            ++result.summary.movedCount;
            break;
        case CleanupStatus::AlreadyMissing:
            ++result.summary.alreadyMissingCount;
            break;
        case CleanupStatus::Ready:
            ++result.summary.blockedCount;
            break;
        case CleanupStatus::Failed:
            ++result.summary.failedCount;
            break;
        default:
            ++result.summary.blockedCount;
            break;
        }
    }
}

CleanupExecutionItem itemFor(const CleanupPreflightItem &item)
{
    return {item.path, item.status, item.detail};
}

CleanupMoveResult unsupportedMove(const QString &)
{
    return {false, false, false, "Moving items to the Recycle Bin is only available on Windows."};
}

#ifdef Q_OS_WIN
QString hresultDetail(const QString &operation, HRESULT result)
{
    return QString("%1 failed (0x%2).")
        .arg(operation)
        .arg(QString::number(static_cast<quint32>(result), 16).rightJustified(8, '0'));
}

bool isMissingResult(HRESULT result)
{
    return result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
           result == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND) ||
           result == HRESULT_FROM_WIN32(ERROR_INVALID_NAME);
}

bool isMissingWindowsError(DWORD error)
{
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ||
           error == ERROR_INVALID_NAME;
}

class FileOperationProgressSink final : public IFileOperationProgressSink
{
  public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interfaceId, void **object) override
    {
        if (!object)
        {
            return E_POINTER;
        }
        *object = nullptr;
        if (interfaceId == IID_IUnknown || interfaceId == IID_IFileOperationProgressSink)
        {
            *object = static_cast<IFileOperationProgressSink *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return ++m_references;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG remaining = --m_references;
        if (remaining == 0)
        {
            delete this;
        }
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE StartOperations() override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE FinishOperations(HRESULT result) override
    {
        m_finishResult = result;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE PreRenameItem(DWORD, IShellItem *, LPCWSTR) override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE PostRenameItem(DWORD, IShellItem *, LPCWSTR, HRESULT,
                                             IShellItem *) override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE PreMoveItem(DWORD, IShellItem *, IShellItem *, LPCWSTR) override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE PostMoveItem(DWORD, IShellItem *, IShellItem *, LPCWSTR, HRESULT,
                                           IShellItem *) override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE PreCopyItem(DWORD, IShellItem *, IShellItem *, LPCWSTR) override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE PostCopyItem(DWORD, IShellItem *, IShellItem *, LPCWSTR, HRESULT,
                                           IShellItem *) override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE PreDeleteItem(DWORD, IShellItem *) override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE PostDeleteItem(DWORD, IShellItem *, HRESULT result,
                                             IShellItem *) override
    {
        m_deleteResult = result;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE PreNewItem(DWORD, IShellItem *, LPCWSTR) override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE PostNewItem(DWORD, IShellItem *, LPCWSTR, LPCWSTR, DWORD, HRESULT,
                                          IShellItem *) override
    {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE UpdateProgress(UINT, UINT) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ResetTimer() override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PauseTimer() override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ResumeTimer() override
    {
        return S_OK;
    }

    [[nodiscard]] std::optional<HRESULT> deleteResult() const
    {
        return m_deleteResult;
    }
    [[nodiscard]] std::optional<HRESULT> finishResult() const
    {
        return m_finishResult;
    }

  private:
    std::atomic<ULONG> m_references{1};
    std::optional<HRESULT> m_deleteResult;
    std::optional<HRESULT> m_finishResult;
};

template <typename T> void releaseCom(T *object)
{
    if (object)
    {
        object->Release();
    }
}

CleanupMoveResult moveToRecycleBin(const QString &absolutePath)
{
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized))
    {
        return {false, false, false, hresultDetail("COM initialization", initialized)};
    }

    IFileOperation *operation = nullptr;
    IShellItem *item = nullptr;
    CleanupMoveResult result;
    do
    {
        HRESULT hr =
            CoCreateInstance(CLSID_FileOperation, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&operation));
        if (FAILED(hr))
        {
            result.detail = hresultDetail("Recycle Bin operation setup", hr);
            break;
        }

        const QString nativePath = extendedNativePath(absolutePath);
        hr = SHCreateItemFromParsingName(reinterpret_cast<LPCWSTR>(nativePath.utf16()), nullptr,
                                         IID_PPV_ARGS(&item));
        if (FAILED(hr))
        {
            result.alreadyMissing = isMissingResult(hr);
            result.detail = result.alreadyMissing
                                ? "The live path disappeared before it could be moved."
                                : hresultDetail("Recycle Bin path lookup", hr);
            break;
        }

        constexpr FILEOP_FLAGS flags =
            static_cast<FILEOP_FLAGS>(FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI |
                                      FOF_SILENT | FOFX_RECYCLEONDELETE | FOFX_EARLYFAILURE);
        hr = operation->SetOperationFlags(flags);
        if (FAILED(hr))
        {
            result.detail = hresultDetail("Recycle Bin operation flags", hr);
            break;
        }
        const auto releaseProgress = [](FileOperationProgressSink *progress)
        {
            if (progress)
            {
                progress->Release();
            }
        };
        std::unique_ptr<FileOperationProgressSink, decltype(releaseProgress)> progress(
            new FileOperationProgressSink, releaseProgress);
        hr = operation->DeleteItem(item, progress.get());
        if (FAILED(hr))
        {
            result.detail = hresultDetail("Recycle Bin delete request", hr);
            break;
        }
        hr = operation->PerformOperations();
        if (FAILED(hr))
        {
            result.detail = hresultDetail("Recycle Bin move", hr);
            break;
        }

        BOOL aborted = FALSE;
        hr = operation->GetAnyOperationsAborted(&aborted);
        if (FAILED(hr))
        {
            result.detail = hresultDetail("Recycle Bin result", hr);
            break;
        }
        if (aborted != FALSE)
        {
            result.aborted = true;
            result.detail = "Windows Shell aborted the Recycle Bin move.";
            break;
        }

        const std::optional<HRESULT> deleteResult = progress->deleteResult();
        if (!deleteResult)
        {
            result.detail = "Windows Shell did not report the Recycle Bin item result.";
            break;
        }
        if (FAILED(*deleteResult))
        {
            result.alreadyMissing = isMissingResult(*deleteResult);
            result.detail = result.alreadyMissing
                                ? "The live path disappeared before it could be moved."
                                : hresultDetail("Recycle Bin item move", *deleteResult);
            break;
        }

        if (const std::optional<HRESULT> finishResult = progress->finishResult();
            finishResult && FAILED(*finishResult))
        {
            result.detail = hresultDetail("Recycle Bin operation", *finishResult);
            break;
        }

        const DWORD remainingAttributes =
            GetFileAttributesW(reinterpret_cast<LPCWSTR>(nativePath.utf16()));
        if (remainingAttributes != INVALID_FILE_ATTRIBUTES)
        {
            result.detail =
                "Windows Shell reported success, but the original path is still present.";
            break;
        }
        const DWORD remainingError = GetLastError();
        if (!isMissingWindowsError(remainingError))
        {
            result.detail = hresultDetail("Recycle Bin postcondition check",
                                          HRESULT_FROM_WIN32(remainingError));
            break;
        }
        result.moved = true;
    } while (false);

    releaseCom(item);
    releaseCom(operation);
    CoUninitialize();
    return result;
}
#endif

CleanupMoveCallback defaultMoveCallback()
{
#ifdef Q_OS_WIN
    return moveToRecycleBin;
#else
    return unsupportedMove;
#endif
}

void writeAudit(const CleanupExecutionRequest &request, const CleanupExecutionResult &result)
{
    const QString path = CleanupExecutor::auditPath(request.paths, request.rootId);
    QJsonArray items;
    for (const CleanupExecutionItem &item : result.items)
    {
        items.append(QJsonObject{{"path", item.path},
                                 {"status", cleanupStatusName(item.status)},
                                 {"detail", item.detail}});
    }
    const QJsonObject event{
        {"timestampUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"rootId", request.rootId},
        {"beforeId", request.beforeId},
        {"afterId", request.afterId},
        {"cancelled", result.cancelled},
        {"movedCount", result.summary.movedCount},
        {"blockedCount", result.summary.blockedCount},
        {"alreadyMissingCount", result.summary.alreadyMissingCount},
        {"failedCount", result.summary.failedCount},
        {"items", items}};
    QByteArray line = QJsonDocument(event).toJson(QJsonDocument::Compact);
    line.append('\n');

    const QFileInfo fileInfo(path);
    if (!QDir().mkpath(fileInfo.absolutePath()))
    {
        throw DomainError(ErrorCode::Io, "Could not create the cleanup audit directory.");
    }
    if (fileInfo.exists() && fileInfo.size() > kMaximumCleanupAuditBytes - line.size())
    {
        const QString rotatedPath =
            QDir(fileInfo.absolutePath())
                .filePath(QString("cleanup-log.%1.jsonl")
                              .arg(QUuid::createUuid().toString(QUuid::Id128)));
        if (!QFile::rename(path, rotatedPath))
        {
            throw DomainError(ErrorCode::Io, "Could not rotate the cleanup audit log.");
        }
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
    {
        throw DomainError(
            ErrorCode::Io,
            QString("Could not open the cleanup audit log: %1").arg(file.errorString()));
    }
    if (file.write(line) != line.size() || !file.flush())
    {
        throw DomainError(
            ErrorCode::Io,
            QString("Could not append the cleanup audit log: %1").arg(file.errorString()));
    }
}

void ensureAuditWritable(const CleanupExecutionRequest &request)
{
    const QString path = CleanupExecutor::auditPath(request.paths, request.rootId);
    const QFileInfo fileInfo(path);
    if (!QDir().mkpath(fileInfo.absolutePath()))
    {
        throw DomainError(ErrorCode::Io, "Could not create the cleanup audit directory.");
    }
    if (fileInfo.exists() && fileInfo.size() >= kMaximumCleanupAuditBytes)
    {
        const QString rotatedPath =
            QDir(fileInfo.absolutePath())
                .filePath(QString("cleanup-log.%1.jsonl")
                              .arg(QUuid::createUuid().toString(QUuid::Id128)));
        if (!QFile::rename(path, rotatedPath))
        {
            throw DomainError(ErrorCode::Io, "Could not rotate the cleanup audit log.");
        }
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append) || !file.flush())
    {
        throw DomainError(
            ErrorCode::Io,
            QString("Could not prepare the cleanup audit log: %1").arg(file.errorString()));
    }
}
} // namespace

QString CleanupExecutor::auditPath(const StoragePaths &paths, const QString &rootId)
{
    validateUuid(rootId);
    return QDir(paths.dataDirectory).filePath("roots/" + rootId + "/cleanup-log.jsonl");
}

CleanupExecutionResult CleanupExecutor::execute(const CleanupExecutionRequest &request,
                                                const CleanupMoveCallback &move,
                                                const CancellationCallback &cancelled)
{
    CleanupExecutionResult result;
    try
    {
        validateUuid(request.rootId);
        validateUuid(request.beforeId);
        validateUuid(request.afterId);
        checkCancelled(cancelled);

        result.preflight = CleanupPreflight::inspect(request.root, request.candidates,
                                                     request.selectedPaths, cancelled);
        if (result.preflight.cancelled)
        {
            result.cancelled = true;
            return result;
        }
        for (const CleanupPreflightItem &item : result.preflight.items)
        {
            result.items.append(itemFor(item));
        }
        if (result.items.isEmpty())
        {
            result.error = "No cleanup items were selected.";
            return result;
        }
        ensureAuditWritable(request);

        QHash<QString, CleanupCandidate> candidatesByPath;
        for (const CleanupCandidate &candidate : request.candidates)
        {
            try
            {
                const QString path = normalizeRelativePath(candidate.after.path);
                if (!candidatesByPath.contains(path))
                {
                    candidatesByPath.insert(path, candidate);
                }
            }
            catch (const DomainError &)
            {
            }
        }

        QHash<QString, EntryType> typesByPath;
        for (auto iterator = candidatesByPath.cbegin(); iterator != candidatesByPath.cend();
             ++iterator)
        {
            typesByPath.insert(iterator.key(), iterator.value().after.type);
        }

        QSet<QString> selectedRoots;
        for (const QString &selectedPath : request.selectedPaths)
        {
            try
            {
                const QString path = normalizeRelativePath(selectedPath);
                if (candidatesByPath.contains(path))
                {
                    selectedRoots.insert(path);
                }
            }
            catch (const DomainError &)
            {
            }
        }
        QSet<QString> selectedCandidatePaths;
        for (auto iterator = candidatesByPath.cbegin(); iterator != candidatesByPath.cend();
             ++iterator)
        {
            for (QString ancestor = iterator.key(); !ancestor.isEmpty();)
            {
                if (selectedRoots.contains(ancestor))
                {
                    selectedCandidatePaths.insert(iterator.key());
                    break;
                }
                const qsizetype separator = ancestor.lastIndexOf('/');
                if (separator < 0)
                {
                    break;
                }
                ancestor = ancestor.left(separator);
            }
        }
        const bool rootHasReparsePoint = hasReparsePointInPath(request.root.displayPath);

        QStringList readyPaths;
        for (const CleanupExecutionItem &item : std::as_const(result.items))
        {
            if (item.status == CleanupStatus::Ready)
            {
                readyPaths.append(item.path);
            }
        }
        std::sort(readyPaths.begin(), readyPaths.end(),
                  [&typesByPath](const QString &left, const QString &right)
                  {
                      if (pathDepth(left) != pathDepth(right))
                      {
                          return pathDepth(left) > pathDepth(right);
                      }
                      const bool leftFolder = typesByPath.value(left) == EntryType::Directory;
                      const bool rightFolder = typesByPath.value(right) == EntryType::Directory;
                      if (leftFolder != rightFolder)
                      {
                          return !leftFolder;
                      }
                      return left < right;
                  });

        QHash<QString, int> resultIndex;
        for (int index = 0; index < result.items.size(); ++index)
        {
            resultIndex.insert(result.items.at(index).path, index);
        }

        // Keep only failed descendants in an ancestor index. This avoids scanning every result
        // item for each parent while retaining the deepest-first safety ordering.
        QHash<QString, int> failedDescendantCounts;
        const auto markFailedPath = [&failedDescendantCounts](const QString &failedPath)
        {
            QString ancestor = failedPath;
            while (ancestor.contains('/'))
            {
                ancestor = ancestor.left(ancestor.lastIndexOf('/'));
                ++failedDescendantCounts[ancestor];
            }
        };
        for (const CleanupExecutionItem &item : std::as_const(result.items))
        {
            if (!isSuccessful(item) && item.status != CleanupStatus::Ready)
            {
                markFailedPath(item.path);
            }
        }

        const CleanupMoveCallback moveItem = move ? move : defaultMoveCallback();
        for (const QString &path : std::as_const(readyPaths))
        {
            checkCancelled(cancelled);
            const auto itemIndex = resultIndex.constFind(path);
            if (itemIndex == resultIndex.cend())
            {
                continue;
            }
            CleanupExecutionItem &item = result.items[*itemIndex];
            if (failedDescendantCounts.value(path) > 0)
            {
                item.status = CleanupStatus::Failed;
                item.detail = "A selected descendant was not moved, so the parent was kept.";
                markFailedPath(path);
                continue;
            }

            const auto candidate = candidatesByPath.constFind(path);
            if (candidate == candidatesByPath.cend())
            {
                item.status = CleanupStatus::Failed;
                item.detail = "The live path could not be revalidated.";
                markFailedPath(path);
                continue;
            }
            const CleanupPreflightResult currentCheck = CleanupPreflight::revalidate(
                request.root, candidate.value(), selectedCandidatePaths, rootHasReparsePoint,
                cancelled);
            if (currentCheck.cancelled)
            {
                result.cancelled = true;
                break;
            }
            const CleanupPreflightItem *current = nullptr;
            for (const CleanupPreflightItem &candidate : currentCheck.items)
            {
                if (candidate.path == path)
                {
                    current = &candidate;
                    break;
                }
            }
            if (!current)
            {
                item.status = CleanupStatus::Failed;
                item.detail = "The live path could not be revalidated.";
                markFailedPath(path);
                continue;
            }
            if (current->status != CleanupStatus::Ready)
            {
                item.status = current->status;
                item.detail = current->detail;
                if (!isSuccessful(item))
                {
                    markFailedPath(path);
                }
                continue;
            }

            QString absolutePath;
            try
            {
                absolutePath = joinUnderRoot(request.root, path);
            }
            catch (const DomainError &error)
            {
                item.status = CleanupStatus::OutsideRootOrInvalid;
                item.detail = error.message();
                markFailedPath(path);
                continue;
            }

            if (typesByPath.value(path) == EntryType::Directory)
            {
                const QFileInfo directoryInfo(absolutePath);
                if (!directoryInfo.exists())
                {
                    item.status = CleanupStatus::AlreadyMissing;
                    item.detail = "The directory disappeared before it could be moved.";
                    continue;
                }
                if (!directoryInfo.isDir())
                {
                    item.status = CleanupStatus::TypeChanged;
                    item.detail = "The live path is no longer a directory.";
                    markFailedPath(path);
                    continue;
                }
                if (!directoryInfo.isReadable())
                {
                    item.status = CleanupStatus::AccessDeniedOrUnreadable;
                    item.detail = "The directory could not be read before it was moved.";
                    markFailedPath(path);
                    continue;
                }

                bool hasRemainingEntries = false;
                const auto enumerationError =
                    enumerateDirectoryEntries(absolutePath, [&hasRemainingEntries](const QString &)
                                              { hasRemainingEntries = true; });
                if (enumerationError)
                {
                    item.status = *enumerationError == DirectoryEnumerationError::NotFound
                                      ? CleanupStatus::AlreadyMissing
                                      : CleanupStatus::AccessDeniedOrUnreadable;
                    item.detail =
                        *enumerationError == DirectoryEnumerationError::NotFound
                            ? "The directory disappeared before it could be moved."
                            : "The directory could not be enumerated before it was moved.";
                    if (!isSuccessful(item))
                    {
                        markFailedPath(path);
                    }
                    continue;
                }
                if (hasRemainingEntries)
                {
                    item.status = CleanupStatus::Failed;
                    item.detail = "The directory still contains content, so it was kept in place.";
                    markFailedPath(path);
                    continue;
                }
            }

            const CleanupMoveResult moveResult = moveItem(absolutePath);
            if (moveResult.moved)
            {
                item.status = CleanupStatus::MovedToRecycleBin;
                item.detail = "Moved to the Windows Recycle Bin.";
            }
            else if (moveResult.alreadyMissing)
            {
                item.status = CleanupStatus::AlreadyMissing;
                item.detail = moveResult.detail;
            }
            else
            {
                item.status = CleanupStatus::Failed;
                item.detail = moveResult.detail.isEmpty() ? "The Recycle Bin move failed."
                                                          : moveResult.detail;
                markFailedPath(path);
            }
        }
        summarize(result);
        writeAudit(request, result);
    }
    catch (const DomainError &error)
    {
        if (error.code() == ErrorCode::Cancelled)
        {
            result.cancelled = true;
            summarize(result);
            if (!result.items.isEmpty())
            {
                try
                {
                    writeAudit(request, result);
                }
                catch (const DomainError &auditError)
                {
                    result.error = auditError.message();
                }
            }
        }
        else
        {
            result.error = error.message();
        }
    }
    catch (const std::exception &error)
    {
        result.error = QString::fromUtf8(error.what());
    }
    catch (...)
    {
        result.error = "Unexpected cleanup execution failure.";
    }
    return result;
}
} // namespace foldersnap
