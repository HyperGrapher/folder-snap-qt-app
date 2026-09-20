#include "cleanup/CleanupPreflight.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <optional>
#include <utility>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSet>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "domain/DomainError.h"
#include "platform/windows/NativeFileMetadata.h"

namespace foldersnap
{
namespace
{
enum class ProbeState
{
    Present,
    Missing,
    AccessDenied,
    Failed
};

struct LiveEntry
{
    ProbeState state{ProbeState::Failed};
    EntryType type{EntryType::Other};
    qint64 size{0};
    qint64 modifiedNs{0};
    qint64 createdNs{0};
    quint32 attributes{0};
    QString linkTarget;
};

struct DirectoryInspection
{
    CleanupStatus status{CleanupStatus::Ready};
    QString detail;
};

class PreflightCancelled final
{
};

void checkCancelled(const CleanupPreflight::CancellationCallback &cancelled)
{
    if (cancelled && cancelled())
    {
        throw PreflightCancelled{};
    }
}

qint64 timestampNanoseconds(const QDateTime &timestamp)
{
    if (!timestamp.isValid())
    {
        return 0;
    }
    const qint64 milliseconds = timestamp.toMSecsSinceEpoch();
    if (milliseconds > std::numeric_limits<qint64>::max() / 1000000 ||
        milliseconds < std::numeric_limits<qint64>::min() / 1000000)
    {
        return 0;
    }
    return milliseconds * 1000000;
}

#ifdef Q_OS_WIN
ProbeState probeStateFromWindowsError(DWORD error)
{
    switch (error)
    {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_NAME:
    case ERROR_BAD_NETPATH:
    case ERROR_BAD_NET_NAME:
        return ProbeState::Missing;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
        return ProbeState::AccessDenied;
    default:
        return ProbeState::Failed;
    }
}
#endif

LiveEntry probePath(const QString &path)
{
    LiveEntry result;
    std::optional<NativeFileMetadata> metadata;
#ifdef Q_OS_WIN
    const QString nativePath = extendedNativePath(path);
    const DWORD attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(nativePath.utf16()));
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        result.state = probeStateFromWindowsError(GetLastError());
        return result;
    }

    metadata = readNativeFileMetadata(path);
    if (!metadata)
    {
        result.state = probeStateFromWindowsError(GetLastError());
        return result;
    }

    result.attributes = metadata->attributes;
    result.type = (metadata->attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ? EntryType::Reparse
                  : (metadata->attributes & FILE_ATTRIBUTE_DIRECTORY) != 0   ? EntryType::Directory
                                                                             : EntryType::File;
#else
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink())
    {
        result.state = ProbeState::Missing;
        return result;
    }
    if (!info.isReadable())
    {
        result.state = ProbeState::AccessDenied;
        return result;
    }
    result.type = info.isSymLink() ? EntryType::Reparse
                  : info.isDir()   ? EntryType::Directory
                  : info.isFile()  ? EntryType::File
                                   : EntryType::Other;
#endif

    const QFileInfo info(path);
    if (!info.exists() && result.type != EntryType::Reparse)
    {
        result.state = ProbeState::Missing;
        return result;
    }
    if (result.type != EntryType::Directory && !info.isReadable())
    {
        result.state = ProbeState::AccessDenied;
        return result;
    }
    if (result.type == EntryType::File)
    {
        result.size = info.size();
        if (result.size < 0)
        {
            result.state = ProbeState::AccessDenied;
            return result;
        }
    }
    if (metadata)
    {
        result.modifiedNs = metadata->modifiedNs;
        result.createdNs = metadata->createdNs;
    }
    else
    {
        result.modifiedNs = timestampNanoseconds(info.lastModified());
        result.createdNs = timestampNanoseconds(info.birthTime());
    }
    if (result.type == EntryType::Reparse)
    {
        result.linkTarget = info.symLinkTarget();
    }
    result.state = ProbeState::Present;
    return result;
}

QString normalizedLinkTarget(QString target)
{
    target = QDir::cleanPath(QDir::fromNativeSeparators(std::move(target)));
    return target.toCaseFolded();
}

QString statusDetail(CleanupStatus status, const QString &path)
{
    switch (status)
    {
    case CleanupStatus::Ready:
        return {};
    case CleanupStatus::AlreadyMissing:
        return "The live path is already missing.";
    case CleanupStatus::ChangedSinceSnapshot:
        return "Live metadata no longer matches the snapshot.";
    case CleanupStatus::TypeChanged:
        return "The live entry type no longer matches the snapshot.";
    case CleanupStatus::OutsideRootOrInvalid:
        return "The path is invalid, outside the watched root, or crosses a reparse point.";
    case CleanupStatus::AccessDeniedOrUnreadable:
        return "The live path could not be read safely.";
    case CleanupStatus::ContainsUntrackedContent:
        return "The directory contains content that is not selected for cleanup.";
    case CleanupStatus::MovedToRecycleBin:
        return {};
    case CleanupStatus::Failed:
        return "The live path could not be checked.";
    }
    Q_UNUSED(path);
    return {};
}

CleanupPreflightItem itemFor(const QString &path, CleanupStatus status, const QString &detail = {})
{
    return {path, status, detail.isEmpty() ? statusDetail(status, path) : detail};
}

std::optional<CleanupPreflightItem>
checkAncestors(const RootPath &root, const QString &relativePath, bool rootHasReparsePoint)
{
    QString current = root.displayPath;
    const LiveEntry rootEntry = probePath(current);
    if (rootEntry.state == ProbeState::Missing)
    {
        return itemFor(relativePath, CleanupStatus::AlreadyMissing,
                       "The watched folder is already missing.");
    }
    if (rootEntry.state == ProbeState::AccessDenied)
    {
        return itemFor(relativePath, CleanupStatus::AccessDeniedOrUnreadable,
                       "The watched folder cannot be read.");
    }
    if (rootEntry.state == ProbeState::Failed)
    {
        return itemFor(relativePath, CleanupStatus::Failed,
                       "The watched folder could not be checked.");
    }
    if (rootEntry.type == EntryType::Reparse)
    {
        return itemFor(relativePath, CleanupStatus::OutsideRootOrInvalid,
                       "The watched folder is a reparse point.");
    }
    if (rootEntry.type != EntryType::Directory)
    {
        return itemFor(relativePath, CleanupStatus::TypeChanged,
                       "The watched root is no longer a directory.");
    }
    if (rootHasReparsePoint)
    {
        return itemFor(relativePath, CleanupStatus::OutsideRootOrInvalid,
                       "The watched root or one of its parents is a reparse point.");
    }

    const QStringList components = relativePath.split('/');
    for (qsizetype index = 0; index + 1 < components.size(); ++index)
    {
        current = QDir::cleanPath(current + '/' + components.at(index));
        const LiveEntry ancestor = probePath(current);
        if (ancestor.state == ProbeState::Missing)
        {
            return itemFor(relativePath, CleanupStatus::AlreadyMissing,
                           "A parent folder is already missing.");
        }
        if (ancestor.state == ProbeState::AccessDenied)
        {
            return itemFor(relativePath, CleanupStatus::AccessDeniedOrUnreadable,
                           "A parent folder cannot be read.");
        }
        if (ancestor.state == ProbeState::Failed)
        {
            return itemFor(relativePath, CleanupStatus::Failed,
                           "A parent folder could not be checked.");
        }
        if (ancestor.type == EntryType::Reparse)
        {
            return itemFor(relativePath, CleanupStatus::OutsideRootOrInvalid,
                           "The path crosses a reparse-point parent.");
        }
        if (ancestor.type != EntryType::Directory)
        {
            return itemFor(relativePath, CleanupStatus::TypeChanged,
                           "A parent path is no longer a directory.");
        }
    }
    return std::nullopt;
}

DirectoryInspection inspectDirectoryContent(const RootPath &root, const QString &relativePath,
                                            const QString &absolutePath,
                                            const QSet<QString> &selectedPaths,
                                            const CleanupPreflight::CancellationCallback &cancelled,
                                            QHash<QString, DirectoryInspection> &cache)
{
    if (cache.contains(relativePath))
    {
        return cache.value(relativePath);
    }

    checkCancelled(cancelled);
    const QFileInfo directoryInfo(absolutePath);
    if (!directoryInfo.isReadable())
    {
        const DirectoryInspection result{CleanupStatus::AccessDeniedOrUnreadable,
                                         "The directory cannot be read."};
        cache.insert(relativePath, result);
        return result;
    }

    const QDir directory(absolutePath);
    const QFileInfoList children = directory.entryInfoList(
        QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System, QDir::NoSort);
    for (const QFileInfo &child : children)
    {
        checkCancelled(cancelled);
        const QString childRelative = QDir(root.displayPath).relativeFilePath(child.filePath());
        QString childKey;
        try
        {
            childKey = normalizeRelativePath(childRelative);
        }
        catch (const DomainError &)
        {
            const DirectoryInspection result{CleanupStatus::OutsideRootOrInvalid,
                                             "A live child path is outside the watched root."};
            cache.insert(relativePath, result);
            return result;
        }

        const LiveEntry live = probePath(child.filePath());
        if (live.state == ProbeState::Missing)
        {
            continue;
        }
        if (live.state == ProbeState::AccessDenied)
        {
            const DirectoryInspection result{CleanupStatus::AccessDeniedOrUnreadable,
                                             "A live child could not be read."};
            cache.insert(relativePath, result);
            return result;
        }
        if (live.state == ProbeState::Failed)
        {
            const DirectoryInspection result{CleanupStatus::Failed,
                                             "A live child could not be checked."};
            cache.insert(relativePath, result);
            return result;
        }
        if (!selectedPaths.contains(childKey))
        {
            const DirectoryInspection result{
                CleanupStatus::ContainsUntrackedContent,
                QString("The directory contains an unselected child: %1.").arg(childKey)};
            cache.insert(relativePath, result);
            return result;
        }
        if (live.type == EntryType::Directory)
        {
            const DirectoryInspection nested = inspectDirectoryContent(
                root, childKey, child.filePath(), selectedPaths, cancelled, cache);
            if (nested.status != CleanupStatus::Ready &&
                nested.status != CleanupStatus::AlreadyMissing)
            {
                cache.insert(relativePath, nested);
                return nested;
            }
        }
    }

    const DirectoryInspection result{};
    cache.insert(relativePath, result);
    return result;
}

CleanupPreflightItem inspectCandidate(const RootPath &root, const CleanupCandidate &candidate,
                                      const QSet<QString> &selectedPaths,
                                      const CleanupPreflight::CancellationCallback &cancelled,
                                      bool rootHasReparsePoint,
                                      QHash<QString, DirectoryInspection> &directoryCache)
{
    checkCancelled(cancelled);
    const QString relativePath = candidate.after.path;
    QString normalizedPath;
    QString absolutePath;
    try
    {
        normalizedPath = normalizeRelativePath(relativePath);
        absolutePath = joinUnderRoot(root, normalizedPath);
    }
    catch (const DomainError &error)
    {
        return itemFor(relativePath, CleanupStatus::OutsideRootOrInvalid, error.message());
    }

    if (const auto ancestorFailure = checkAncestors(root, normalizedPath, rootHasReparsePoint))
    {
        return *ancestorFailure;
    }

    const LiveEntry live = probePath(absolutePath);
    if (live.state == ProbeState::Missing)
    {
        return itemFor(normalizedPath, CleanupStatus::AlreadyMissing);
    }
    if (live.state == ProbeState::AccessDenied)
    {
        return itemFor(normalizedPath, CleanupStatus::AccessDeniedOrUnreadable);
    }
    if (live.state == ProbeState::Failed)
    {
        return itemFor(normalizedPath, CleanupStatus::Failed);
    }
    if (live.type != candidate.after.type)
    {
        return itemFor(normalizedPath, CleanupStatus::TypeChanged);
    }
    if (candidate.after.type == EntryType::File)
    {
        if (live.size != candidate.after.size || live.modifiedNs != candidate.after.modifiedNs ||
            (candidate.after.createdNs != 0 && live.createdNs != candidate.after.createdNs))
        {
            return itemFor(normalizedPath, CleanupStatus::ChangedSinceSnapshot);
        }
    }
    else if (candidate.after.type == EntryType::Reparse && !candidate.after.linkTarget.isEmpty())
    {
        if (live.linkTarget.isEmpty())
        {
            return itemFor(normalizedPath, CleanupStatus::AccessDeniedOrUnreadable,
                           "The reparse target could not be read.");
        }
        if (normalizedLinkTarget(live.linkTarget) !=
            normalizedLinkTarget(candidate.after.linkTarget))
        {
            return itemFor(normalizedPath, CleanupStatus::ChangedSinceSnapshot,
                           "The reparse target no longer matches the snapshot.");
        }
    }
    else if (candidate.after.type == EntryType::Other &&
             (live.modifiedNs != candidate.after.modifiedNs ||
              live.attributes != candidate.after.attributes))
    {
        return itemFor(normalizedPath, CleanupStatus::ChangedSinceSnapshot);
    }

    if (candidate.after.type == EntryType::Directory)
    {
        const DirectoryInspection directory = inspectDirectoryContent(
            root, normalizedPath, absolutePath, selectedPaths, cancelled, directoryCache);
        if (directory.status != CleanupStatus::Ready &&
            directory.status != CleanupStatus::AlreadyMissing)
        {
            return itemFor(normalizedPath, directory.status, directory.detail);
        }
    }
    return itemFor(normalizedPath, CleanupStatus::Ready);
}

void summarize(CleanupPreflightResult &result)
{
    result.summary = {};
    for (const CleanupPreflightItem &item : result.items)
    {
        switch (item.status)
        {
        case CleanupStatus::Ready:
            ++result.summary.readyCount;
            break;
        case CleanupStatus::AlreadyMissing:
            ++result.summary.alreadyMissingCount;
            break;
        default:
            ++result.summary.blockedCount;
            break;
        }
    }
}
} // namespace

QString cleanupStatusName(CleanupStatus status)
{
    switch (status)
    {
    case CleanupStatus::Ready:
        return "ready";
    case CleanupStatus::AlreadyMissing:
        return "already_missing";
    case CleanupStatus::ChangedSinceSnapshot:
        return "changed_since_snapshot";
    case CleanupStatus::TypeChanged:
        return "type_changed";
    case CleanupStatus::OutsideRootOrInvalid:
        return "outside_root_or_invalid";
    case CleanupStatus::AccessDeniedOrUnreadable:
        return "access_denied_or_unreadable";
    case CleanupStatus::ContainsUntrackedContent:
        return "contains_untracked_content";
    case CleanupStatus::MovedToRecycleBin:
        return "moved_to_recycle_bin";
    case CleanupStatus::Failed:
        return "failed";
    }
    return "failed";
}

CleanupPreflightResult CleanupPreflight::inspect(const RootPath &root,
                                                 const QList<CleanupCandidate> &candidates,
                                                 const QStringList &selectedPaths,
                                                 const CancellationCallback &cancelled)
{
    CleanupPreflightResult result;
    try
    {
        checkCancelled(cancelled);
        QHash<QString, SnapshotEntry> candidatesByPath;
        for (const CleanupCandidate &candidate : candidates)
        {
            try
            {
                const QString path = normalizeRelativePath(candidate.after.path);
                if (!candidatesByPath.contains(path))
                {
                    candidatesByPath.insert(path, candidate.after);
                }
            }
            catch (const DomainError &)
            {
                // The invalid candidate is reported if the caller selects it below.
            }
        }

        QSet<QString> requestedPaths;
        QList<CleanupPreflightItem> invalidSelections;
        for (const QString &selectedPath : selectedPaths)
        {
            checkCancelled(cancelled);
            try
            {
                requestedPaths.insert(normalizeRelativePath(selectedPath));
            }
            catch (const DomainError &error)
            {
                invalidSelections.append(
                    itemFor(selectedPath, CleanupStatus::OutsideRootOrInvalid, error.message()));
            }
        }

        QSet<QString> selectedRoots;
        for (const QString &requestedPath : std::as_const(requestedPaths))
        {
            if (!candidatesByPath.contains(requestedPath))
            {
                invalidSelections.append(itemFor(requestedPath, CleanupStatus::OutsideRootOrInvalid,
                                                 "The selected path is not a cleanup candidate."));
                continue;
            }
            selectedRoots.insert(requestedPath);
        }

        QSet<QString> selectedCandidatePaths;
        for (auto iterator = candidatesByPath.cbegin(); iterator != candidatesByPath.cend();
             ++iterator)
        {
            QString ancestor = iterator.key();
            while (true)
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

        QHash<QString, DirectoryInspection> directoryCache;
        const bool rootHasReparsePoint = hasReparsePointInPath(root.displayPath);
        QHash<QString, int> resultIndex;
        for (const CleanupCandidate &candidate : candidates)
        {
            checkCancelled(cancelled);
            QString path;
            try
            {
                path = normalizeRelativePath(candidate.after.path);
            }
            catch (const DomainError &)
            {
                continue;
            }
            if (!selectedCandidatePaths.contains(path) || resultIndex.contains(path))
            {
                continue;
            }
            resultIndex.insert(path, result.items.size());
            result.items.append(inspectCandidate(root, candidate, selectedCandidatePaths, cancelled,
                                                 rootHasReparsePoint, directoryCache));
        }
        result.items.append(invalidSelections);

        QList<int> deepestFirst;
        deepestFirst.reserve(result.items.size());
        for (int index = 0; index < result.items.size(); ++index)
        {
            deepestFirst.append(index);
        }
        std::sort(deepestFirst.begin(), deepestFirst.end(),
                  [&result](int left, int right)
                  {
                      const QString leftPath = result.items.at(left).path;
                      const QString rightPath = result.items.at(right).path;
                      if (leftPath.count('/') != rightPath.count('/'))
                      {
                          return leftPath.count('/') > rightPath.count('/');
                      }
                      return leftPath < rightPath;
                  });
        for (const int childIndex : deepestFirst)
        {
            checkCancelled(cancelled);
            const CleanupPreflightItem child = result.items.at(childIndex);
            if (child.status == CleanupStatus::Ready ||
                child.status == CleanupStatus::AlreadyMissing)
            {
                continue;
            }
            QString parent = child.path;
            while (parent.contains('/'))
            {
                parent = parent.left(parent.lastIndexOf('/'));
                const auto parentIndex = resultIndex.constFind(parent);
                if (parentIndex == resultIndex.cend())
                {
                    continue;
                }
                CleanupPreflightItem &parentItem = result.items[*parentIndex];
                if (parentItem.status == CleanupStatus::Ready)
                {
                    parentItem.status = child.status;
                    parentItem.detail = QString("Blocked by selected descendant %1: %2")
                                            .arg(child.path, child.detail);
                }
            }
        }
        summarize(result);
    }
    catch (const PreflightCancelled &)
    {
        result.items.clear();
        result.summary = {};
        result.cancelled = true;
    }
    return result;
}
} // namespace foldersnap
