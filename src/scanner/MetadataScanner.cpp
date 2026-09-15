#include "scanner/MetadataScanner.h"

#include <algorithm>
#include <limits>
#include <utility>

#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QList>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "domain/DomainError.h"
#include "ignore/IgnoreMatcher.h"

namespace foldersnap
{
namespace
{
struct DirectoryWork
{
    QString absolutePath;
    QString relativePath;
};

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

void addWarning(Snapshot &snapshot, const QString &path, WarningOperation operation,
                WarningCategory category, const QString &message)
{
    snapshot.header.scanWarnings.append({path, operation, category, message});
}

EntryType entryType(const QFileInfo &info)
{
    if (info.isSymLink())
    {
        return EntryType::Reparse;
    }
    if (info.isDir())
    {
        return EntryType::Directory;
    }
    if (info.isFile())
    {
        return EntryType::File;
    }
    return EntryType::Other;
}

quint32 fileAttributes(const QFileInfo &info)
{
#ifdef Q_OS_WIN
    const DWORD attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(info.filePath().utf16()));
    return attributes == INVALID_FILE_ATTRIBUTES ? 0U : static_cast<quint32>(attributes);
#else
    Q_UNUSED(info);
    return 0;
#endif
}

bool appendEntry(Snapshot &snapshot, const QFileInfo &info, const QString &relativePath,
                 EntryType type)
{
    SnapshotEntry entry;
    entry.path = relativePath;
    entry.displayPath = relativePath;
    entry.type = type;
    entry.attributes = fileAttributes(info);
    if (type == EntryType::File)
    {
        entry.size = info.size();
        if (entry.size < 0)
        {
            return false;
        }
    }
    if (type == EntryType::Reparse)
    {
        entry.linkTarget = info.symLinkTarget();
    }
    entry.modifiedNs = timestampNanoseconds(info.lastModified());
    entry.createdNs = timestampNanoseconds(info.birthTime());
    snapshot.entries.append(std::move(entry));
    return true;
}
} // namespace

ScanResult MetadataScanner::scan(const ScanRequest &request, const ProgressCallback &progress,
                                 const CancellationCallback &cancelled)
{
    ScanResult result;
    const auto startedAt = QDateTime::currentDateTimeUtc();
    try
    {
        validateUuid(request.rootId);
        const QFileInfo rootInfo(request.root.displayPath);
        if (!rootInfo.exists() || !rootInfo.isDir())
        {
            result.error = "The watched folder does not exist or is not a directory.";
            return result;
        }
        if (!rootInfo.isReadable())
        {
            result.error = "The watched folder cannot be read.";
            return result;
        }

        const IgnoreMatcher matcher(request.ignoreRules, request.protectedSubtree);
        result.snapshot.header.snapshotId = createId();
        result.snapshot.header.rootId = request.rootId;
        result.snapshot.header.rootPathAtCapture = request.root.displayPath;
        result.snapshot.header.displayTitle = request.displayTitle;
        result.snapshot.header.startedAtUtc.nanoseconds = timestampNanoseconds(startedAt);
        result.snapshot.header.trigger = request.trigger;
        result.snapshot.header.ignoreConfig = {request.ignoreRules,
                                               IgnoreMatcher::rulesHash(request.ignoreRules)};

        QList<DirectoryWork> pending{{request.root.displayPath, {}}};
        qint64 processedEntries = 0;
        while (!pending.isEmpty())
        {
            if (cancelled())
            {
                result.cancelled = true;
                return result;
            }
            const DirectoryWork directory = pending.takeLast();
            const QDir currentDirectory(directory.absolutePath);
            if (!currentDirectory.isReadable())
            {
                addWarning(result.snapshot, directory.relativePath, WarningOperation::Enumerate,
                           WarningCategory::AccessDenied, "The folder could not be read.");
                continue;
            }
            const QFileInfoList children = currentDirectory.entryInfoList(
                QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System, QDir::Name);
            for (const QFileInfo &info : children)
            {
                if (cancelled())
                {
                    result.cancelled = true;
                    return result;
                }
                const QString relativePath = normalizeRelativePath(
                    QDir(request.root.displayPath).relativeFilePath(info.filePath()));
                const EntryType type = entryType(info);
                const bool isDirectory = type == EntryType::Directory;
                const IgnoreMatch match = matcher.testPath(relativePath, isDirectory);
                if (!match.included)
                {
                    if (isDirectory && !matcher.canPruneDirectory(relativePath))
                    {
                        pending.append({info.filePath(), relativePath});
                    }
                    continue;
                }
                if (!appendEntry(result.snapshot, info, relativePath, type))
                {
                    addWarning(result.snapshot, relativePath, WarningOperation::Stat,
                               WarningCategory::Io, "The file size could not be read.");
                    continue;
                }
                if (isDirectory)
                {
                    pending.append({info.filePath(), relativePath});
                }
                ++processedEntries;
                if (progress && (processedEntries % 32 == 0))
                {
                    progress(std::min(95, 5 + static_cast<int>(processedEntries / 32)));
                }
            }
        }
        result.snapshot.header.completedAtUtc.nanoseconds =
            timestampNanoseconds(QDateTime::currentDateTimeUtc());
        std::sort(result.snapshot.entries.begin(), result.snapshot.entries.end(),
                  [](const SnapshotEntry &left, const SnapshotEntry &right)
                  { return left.path < right.path; });
        std::sort(result.snapshot.header.scanWarnings.begin(),
                  result.snapshot.header.scanWarnings.end(),
                  [](const ScanWarning &left, const ScanWarning &right)
                  { return left.path < right.path; });
        for (const SnapshotEntry &entry : result.snapshot.entries)
        {
            if (entry.type == EntryType::File)
            {
                ++result.snapshot.header.fileCount;
                result.snapshot.header.totalFileBytes += entry.size;
            }
            else if (entry.type == EntryType::Directory)
            {
                ++result.snapshot.header.directoryCount;
            }
            else
            {
                ++result.snapshot.header.otherCount;
            }
        }
        if (progress)
        {
            progress(100);
        }
    }
    catch (const DomainError &error)
    {
        result.error = error.message();
    }
    return result;
}
} // namespace foldersnap
