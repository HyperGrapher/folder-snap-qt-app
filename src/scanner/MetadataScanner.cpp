#include "scanner/MetadataScanner.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <exception>
#include <limits>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QList>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "domain/DomainError.h"
#include "ignore/IgnoreMatcher.h"
#include "platform/windows/NativeFileMetadata.h"

namespace foldersnap
{
namespace
{
constexpr qsizetype kEntryBatchSize = 256;

struct DirectoryWork
{
    QString absolutePath;
    QString relativePath;
};

class ScanCancelled final
{
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

void addWarning(QList<ScanWarning> &warnings, const QString &path, WarningOperation operation,
                WarningCategory category, const QString &message)
{
    warnings.append({path, operation, category, message});
}

EntryType entryType(const QFileInfo &info, quint32 attributes)
{
#ifdef Q_OS_WIN
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
    {
        return EntryType::Reparse;
    }
#endif
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
    const QString nativePath = extendedNativePath(info.filePath());
    const DWORD attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(nativePath.utf16()));
    return attributes == INVALID_FILE_ATTRIBUTES ? 0U : static_cast<quint32>(attributes);
#else
    Q_UNUSED(info);
    return 0;
#endif
}

bool appendEntry(QList<SnapshotEntry> &entries, const QFileInfo &info, const QString &relativePath,
                 const QString &displayPath, EntryType type, quint32 attributes,
                 const std::optional<NativeFileMetadata> &metadata)
{
    SnapshotEntry entry;
    entry.path = relativePath;
    entry.displayPath = displayPath;
    entry.type = type;
    entry.attributes = attributes;
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
    if (metadata)
    {
        entry.modifiedNs = metadata->modifiedNs;
        entry.createdNs = metadata->createdNs;
    }
    else
    {
        entry.modifiedNs = timestampNanoseconds(info.lastModified());
        entry.createdNs = timestampNanoseconds(info.birthTime());
    }
    entries.append(std::move(entry));
    return true;
}

void checkCancelled(const MetadataScanner::CancellationCallback &cancelled)
{
    if (cancelled && cancelled())
    {
        throw ScanCancelled{};
    }
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
        if (hasReparsePointInPath(request.root.displayPath))
        {
            result.error = "The watched folder or one of its parents is a reparse point.";
            return result;
        }

        if (request.directoryWorkerCount < 1 || request.directoryWorkerCount > 32)
        {
            result.error = "The directory worker count must be between 1 and 32.";
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

        struct SharedWork
        {
            std::mutex mutex;
            std::condition_variable available;
            QList<DirectoryWork> pending;
            QList<SnapshotEntry> entries;
            QList<ScanWarning> warnings;
            QString failure;
            int activeDirectories{0};
        } shared;
        shared.pending.append({request.root.displayPath, {}});

        std::atomic_bool wasCancelled{false};
        std::atomic_bool failed{false};
        std::atomic<qint64> processedEntries{0};
        std::mutex progressMutex;
        int reportedProgress = 0;
        const auto reportProgress = [&]()
        {
            if (!progress)
            {
                return;
            }
            const qint64 processed = processedEntries.load();
            const int candidate = std::min(95, 5 + static_cast<int>(processed / kEntryBatchSize));
            std::lock_guard lock(progressMutex);
            if (candidate <= reportedProgress)
            {
                return;
            }
            reportedProgress = candidate;
            progress(candidate);
        };

        const auto worker = [&]()
        {
            while (!wasCancelled.load() && !failed.load())
            {
                DirectoryWork directory;
                {
                    std::unique_lock lock(shared.mutex);
                    shared.available.wait(lock,
                                          [&]()
                                          {
                                              return wasCancelled.load() || failed.load() ||
                                                     !shared.pending.isEmpty() ||
                                                     shared.activeDirectories == 0;
                                          });
                    if (wasCancelled.load() || failed.load() || shared.pending.isEmpty())
                    {
                        return;
                    }
                    directory = shared.pending.takeLast();
                    ++shared.activeDirectories;
                }

                QList<SnapshotEntry> directoryEntries;
                QList<ScanWarning> directoryWarnings;
                QList<DirectoryWork> discoveredDirectories;
                QString directoryFailure;
                try
                {
                    checkCancelled(cancelled);
                    const QDir currentDirectory(directory.absolutePath);
                    if (!currentDirectory.exists())
                    {
                        if (directory.relativePath.isEmpty())
                        {
                            directoryFailure = "The watched folder no longer exists.";
                        }
                        else
                        {
                            addWarning(directoryWarnings, directory.relativePath,
                                       WarningOperation::Enumerate, WarningCategory::NotFound,
                                       "The folder no longer exists.");
                        }
                    }
                    else if (!currentDirectory.isReadable())
                    {
                        if (directory.relativePath.isEmpty())
                        {
                            directoryFailure = "The watched folder cannot be read.";
                        }
                        else
                        {
                            addWarning(directoryWarnings, directory.relativePath,
                                       WarningOperation::Enumerate, WarningCategory::AccessDenied,
                                       "The folder could not be read.");
                        }
                    }
                    else
                    {
                        qsizetype batchCount = 0;
                        const auto enumerationError = enumerateDirectoryEntries(
                            directory.absolutePath,
                            [&](const QString &entryPath)
                            {
                                checkCancelled(cancelled);
                                const QFileInfo info(entryPath);
                                const QString displayPath = normalizeRelativeDisplayPath(
                                    QDir(request.root.displayPath)
                                        .relativeFilePath(info.filePath()));
                                const QString relativePath = displayPath.toLower();
                                const std::optional<NativeFileMetadata> metadata =
                                    readNativeFileMetadata(info.filePath());
                                const quint32 attributes =
                                    metadata ? metadata->attributes : fileAttributes(info);
                                const EntryType type = entryType(info, attributes);
                                const bool isDirectory = type == EntryType::Directory;
                                const IgnoreMatch match =
                                    matcher.testPath(relativePath, isDirectory);
                                if (!match.included)
                                {
                                    if (isDirectory && !matcher.canPruneDirectory(relativePath))
                                    {
                                        discoveredDirectories.append(
                                            {info.filePath(), relativePath});
                                    }
                                }
                                else if (!appendEntry(directoryEntries, info, relativePath,
                                                      displayPath, type, attributes, metadata))
                                {
                                    addWarning(directoryWarnings, relativePath,
                                               WarningOperation::Stat, WarningCategory::Io,
                                               "The file size could not be read.");
                                }
                                else
                                {
                                    if (isDirectory)
                                    {
                                        discoveredDirectories.append(
                                            {info.filePath(), relativePath});
                                    }
                                    ++processedEntries;
                                }
                                ++batchCount;
                                if (batchCount == kEntryBatchSize)
                                {
                                    reportProgress();
                                    batchCount = 0;
                                }
                            });
                        if (enumerationError)
                        {
                            const QString message =
                                *enumerationError == DirectoryEnumerationError::NotFound
                                    ? "The folder no longer exists."
                                : *enumerationError == DirectoryEnumerationError::AccessDenied
                                    ? "The folder could not be read."
                                    : "The folder could not be enumerated.";
                            if (directory.relativePath.isEmpty())
                            {
                                directoryFailure = message;
                            }
                            else
                            {
                                const WarningCategory category =
                                    *enumerationError == DirectoryEnumerationError::NotFound
                                        ? WarningCategory::NotFound
                                    : *enumerationError == DirectoryEnumerationError::AccessDenied
                                        ? WarningCategory::AccessDenied
                                        : WarningCategory::Io;
                                addWarning(directoryWarnings, directory.relativePath,
                                           WarningOperation::Enumerate, category, message);
                            }
                        }
                        if (batchCount != 0)
                        {
                            reportProgress();
                        }
                    }
                }
                catch (const ScanCancelled &)
                {
                    wasCancelled = true;
                }
                catch (const std::exception &exception)
                {
                    std::lock_guard lock(shared.mutex);
                    shared.failure = QString::fromUtf8(exception.what());
                    failed = true;
                }

                try
                {
                    checkCancelled(cancelled);
                }
                catch (const ScanCancelled &)
                {
                    wasCancelled = true;
                }

                {
                    std::lock_guard lock(shared.mutex);
                    --shared.activeDirectories;
                    if (!directoryFailure.isEmpty())
                    {
                        shared.failure = directoryFailure;
                        failed = true;
                    }
                    else if (!wasCancelled.load() && !failed.load())
                    {
                        shared.entries.append(directoryEntries);
                        shared.warnings.append(directoryWarnings);
                        shared.pending.append(discoveredDirectories);
                    }
                }
                shared.available.notify_all();
            }
        };

        std::vector<std::jthread> workers;
        workers.reserve(static_cast<size_t>(request.directoryWorkerCount));
        try
        {
            for (int workerIndex = 0; workerIndex < request.directoryWorkerCount; ++workerIndex)
            {
                workers.emplace_back(worker);
            }
        }
        catch (...)
        {
            wasCancelled = true;
            shared.available.notify_all();
            throw;
        }
        for (std::jthread &workerThread : workers)
        {
            workerThread.join();
        }
        if (failed.load())
        {
            result.error =
                shared.failure.isEmpty() ? "The folder could not be scanned." : shared.failure;
            return result;
        }
        if (wasCancelled.load() || (cancelled && cancelled()))
        {
            result.cancelled = true;
            return result;
        }

        result.snapshot.entries = std::move(shared.entries);
        result.snapshot.header.scanWarnings = std::move(shared.warnings);
        result.snapshot.header.completedAtUtc.nanoseconds =
            timestampNanoseconds(QDateTime::currentDateTimeUtc());
        std::sort(result.snapshot.entries.begin(), result.snapshot.entries.end(),
                  [&cancelled](const SnapshotEntry &left, const SnapshotEntry &right)
                  {
                      checkCancelled(cancelled);
                      return left.path < right.path;
                  });
        std::sort(result.snapshot.header.scanWarnings.begin(),
                  result.snapshot.header.scanWarnings.end(),
                  [&cancelled](const ScanWarning &left, const ScanWarning &right)
                  {
                      checkCancelled(cancelled);
                      if (left.path != right.path)
                      {
                          return left.path < right.path;
                      }
                      return left.operation < right.operation;
                  });
        qsizetype summaryIndex = 0;
        for (const SnapshotEntry &entry : result.snapshot.entries)
        {
            if ((summaryIndex++ % kEntryBatchSize) == 0)
            {
                checkCancelled(cancelled);
            }
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
        checkCancelled(cancelled);
        if (progress)
        {
            progress(100);
        }
    }
    catch (const ScanCancelled &)
    {
        result.cancelled = true;
    }
    catch (const DomainError &error)
    {
        result.error = error.message();
    }
    return result;
}
} // namespace foldersnap
