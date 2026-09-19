#include "cleanup/CleanupExecutor.h"

#include <algorithm>
#include <utility>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#ifdef Q_OS_WIN
#include <ShObjIdl.h>
#include <windows.h>
#endif

#include "domain/DomainError.h"
#include "storage/AtomicFile.h"

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

CleanupExecutionItem *findItem(QList<CleanupExecutionItem> &items, const QString &path)
{
    const auto iterator = std::find_if(items.begin(), items.end(),
                                       [&path](const auto &item) { return item.path == path; });
    return iterator == items.end() ? nullptr : &*iterator;
}

const CleanupExecutionItem *findItem(const QList<CleanupExecutionItem> &items, const QString &path)
{
    const auto iterator = std::find_if(items.cbegin(), items.cend(),
                                       [&path](const auto &item) { return item.path == path; });
    return iterator == items.cend() ? nullptr : &*iterator;
}

bool hasFailedDescendant(const QList<CleanupExecutionItem> &items, const QString &path)
{
    for (const CleanupExecutionItem &item : items)
    {
        if (item.path == path || !isAtOrBelow(item.path, path) || isSuccessful(item))
        {
            continue;
        }
        return true;
    }
    return false;
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

        const QString nativePath = QDir::toNativeSeparators(absolutePath);
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
        hr = operation->DeleteItem(item, nullptr);
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
    QByteArray contents;
    if (QFileInfo::exists(path))
    {
        contents = readFileLimited(path, kMaximumCleanupAuditBytes);
        if (!contents.isEmpty() && !contents.endsWith('\n'))
        {
            contents.append('\n');
        }
    }

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
    contents.append(QJsonDocument(event).toJson(QJsonDocument::Compact));
    contents.append('\n');
    replaceFileAtomically(path, contents);
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

        QHash<QString, EntryType> typesByPath;
        for (const CleanupCandidate &candidate : request.candidates)
        {
            try
            {
                typesByPath.insert(normalizeRelativePath(candidate.after.path),
                                   candidate.after.type);
            }
            catch (const DomainError &)
            {
            }
        }

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

        const CleanupMoveCallback moveItem = move ? move : defaultMoveCallback();
        for (const QString &path : std::as_const(readyPaths))
        {
            checkCancelled(cancelled);
            CleanupExecutionItem *item = findItem(result.items, path);
            if (!item)
            {
                continue;
            }
            if (hasFailedDescendant(result.items, path))
            {
                item->status = CleanupStatus::Failed;
                item->detail = "A selected descendant was not moved, so the parent was kept.";
                continue;
            }

            if (typesByPath.value(path) == EntryType::Directory)
            {
                const CleanupPreflightResult directoryCheck =
                    CleanupPreflight::inspect(request.root, request.candidates, {path}, cancelled);
                if (directoryCheck.cancelled)
                {
                    result.cancelled = true;
                    break;
                }
                const CleanupPreflightItem *current = nullptr;
                for (const CleanupPreflightItem &candidate : directoryCheck.items)
                {
                    if (candidate.path == path)
                    {
                        current = &candidate;
                        break;
                    }
                }
                if (!current || current->status == CleanupStatus::AlreadyMissing)
                {
                    item->status = CleanupStatus::AlreadyMissing;
                    item->detail = "The directory disappeared before it could be moved.";
                    continue;
                }
                if (current->status != CleanupStatus::Ready)
                {
                    item->status = current->status;
                    item->detail = current->detail;
                    continue;
                }
            }

            QString absolutePath;
            try
            {
                absolutePath = joinUnderRoot(request.root, path);
            }
            catch (const DomainError &error)
            {
                item->status = CleanupStatus::OutsideRootOrInvalid;
                item->detail = error.message();
                continue;
            }
            const CleanupMoveResult moveResult = moveItem(absolutePath);
            if (moveResult.moved)
            {
                item->status = CleanupStatus::MovedToRecycleBin;
                item->detail = "Moved to the Windows Recycle Bin.";
            }
            else if (moveResult.alreadyMissing)
            {
                item->status = CleanupStatus::AlreadyMissing;
                item->detail = moveResult.detail;
            }
            else
            {
                item->status = CleanupStatus::Failed;
                item->detail = moveResult.detail.isEmpty() ? "The Recycle Bin move failed."
                                                           : moveResult.detail;
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
