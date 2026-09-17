#include "storage/HistoryStore.h"

#include <algorithm>
#include <utility>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSet>
#include <QThread>

#include "domain/DomainError.h"
#include "storage/ConfigurationStore.h"
#include "storage/SnapshotStore.h"

namespace foldersnap
{
namespace
{
class HistoryMutationLock final
{
  public:
    explicit HistoryMutationLock(const StoragePaths &paths)
        : m_lock(QDir(paths.dataDirectory).filePath("history.lock"))
    {
        if (!QDir().mkpath(paths.dataDirectory))
        {
            throw DomainError(ErrorCode::Io, "Could not create the FolderSnap data directory.");
        }
        m_lock.setStaleLockTime(0);
        QElapsedTimer timer;
        timer.start();
        while (!m_lock.tryLock(0))
        {
            if (timer.elapsed() >= 60000)
            {
                throw DomainError(ErrorCode::Io,
                                  "Could not acquire the FolderSnap history write lock.");
            }
            QThread::msleep(10);
        }
    }

    ~HistoryMutationLock()
    {
        m_lock.unlock();
    }

    HistoryMutationLock(const HistoryMutationLock &) = delete;
    HistoryMutationLock &operator=(const HistoryMutationLock &) = delete;

  private:
    QLockFile m_lock;
};

HistoryRecord recordForSnapshot(const Snapshot &snapshot, qint64 compressedBytes)
{
    const SnapshotHeader &header = snapshot.header;
    return {
        header.snapshotId,        header.rootId,
        header.rootPathAtCapture, header.displayTitle,
        header.completedAtUtc,    header.trigger,
        header.description,       header.fileCount,
        header.directoryCount,    header.otherCount,
        header.totalFileBytes,    header.scanWarnings.size(),
        compressedBytes,          true,
    };
}

void sortRecords(QList<HistoryRecord> &records)
{
    std::sort(records.begin(), records.end(),
              [](const HistoryRecord &left, const HistoryRecord &right)
              {
                  return left.completedAtUtc != right.completedAtUtc
                             ? left.completedAtUtc > right.completedAtUtc
                             : left.snapshotId < right.snapshotId;
              });
}

QString idFromPayloadFilename(const QString &filename, const QString &suffix)
{
    if (!filename.endsWith(suffix))
    {
        return {};
    }
    const QString id = filename.left(filename.size() - suffix.size());
    try
    {
        validateUuid(id);
    }
    catch (const DomainError &)
    {
        return {};
    }
    return id;
}
} // namespace

HistoryStore::HistoryStore(StoragePaths paths) : m_paths(std::move(paths)) {}

QList<HistoryRecord> HistoryStore::loadHistory() const
{
    return ConfigurationStore(m_paths).loadHistoryIndex().value;
}

QList<HistoryRecord> HistoryStore::loadHistoryForRoot(const QString &rootId) const
{
    validateUuid(rootId);
    QList<HistoryRecord> records;
    for (const HistoryRecord &record : loadHistory())
    {
        if (record.rootId == rootId)
        {
            records.append(record);
        }
    }
    return records;
}

SnapshotCommitResult HistoryStore::commitSnapshot(const Snapshot &snapshot, int retention) const
{
    validateSnapshot(snapshot);
    validateRetention(retention);
    HistoryMutationLock lock(m_paths);
    ConfigurationStore configurationStore(m_paths);
    SnapshotStore snapshotStore(m_paths);
    QList<HistoryRecord> records = configurationStore.loadHistoryIndex().value;
    const QString snapshotId = snapshot.header.snapshotId;
    if (std::any_of(records.cbegin(), records.cend(), [&snapshotId](const HistoryRecord &record)
                    { return record.snapshotId == snapshotId; }) ||
        snapshotStore.hasPayload(snapshotId))
    {
        throw DomainError(ErrorCode::InvalidData, "Snapshot ID already exists in history.");
    }

    const HistoryRecord record = recordForSnapshot(snapshot, snapshotStore.saveSnapshot(snapshot));
    records.append(record);
    sortRecords(records);

    QList<QString> prunedIds;
    if (retention != 0)
    {
        int retainedCount = 0;
        auto iterator = records.begin();
        while (iterator != records.end())
        {
            if (iterator->rootId != record.rootId || ++retainedCount <= retention)
            {
                ++iterator;
                continue;
            }
            prunedIds.append(iterator->snapshotId);
            iterator = records.erase(iterator);
        }
    }

    QList<QString> tombstonedIds;
    try
    {
        for (const QString &prunedId : prunedIds)
        {
            if (snapshotStore.movePayloadToTombstone(prunedId))
            {
                tombstonedIds.append(prunedId);
            }
        }
        configurationStore.saveHistoryIndex(records);
    }
    catch (...)
    {
        for (auto iterator = tombstonedIds.crbegin(); iterator != tombstonedIds.crend(); ++iterator)
        {
            snapshotStore.restoreTombstone(*iterator);
        }
        throw;
    }

    for (const QString &tombstonedId : tombstonedIds)
    {
        snapshotStore.removeTombstone(tombstonedId);
    }
    return {record, prunedIds};
}

void HistoryStore::updateDescription(const QString &snapshotId, const QString &description) const
{
    validateUuid(snapshotId);
    validateDescription(description);
    HistoryMutationLock lock(m_paths);
    ConfigurationStore configurationStore(m_paths);
    QList<HistoryRecord> records = configurationStore.loadHistoryIndex().value;
    const auto iterator =
        std::find_if(records.begin(), records.end(), [&snapshotId](const HistoryRecord &record)
                     { return record.snapshotId == snapshotId; });
    if (iterator == records.end())
    {
        throw DomainError(ErrorCode::InvalidData, "Snapshot ID does not exist in history.");
    }
    iterator->description = description;
    configurationStore.saveHistoryIndex(records);
}

void HistoryStore::deleteSnapshot(const QString &snapshotId) const
{
    validateUuid(snapshotId);
    HistoryMutationLock lock(m_paths);
    ConfigurationStore configurationStore(m_paths);
    SnapshotStore snapshotStore(m_paths);
    QList<HistoryRecord> records = configurationStore.loadHistoryIndex().value;
    const auto iterator =
        std::find_if(records.begin(), records.end(), [&snapshotId](const HistoryRecord &record)
                     { return record.snapshotId == snapshotId; });
    if (iterator == records.end())
    {
        throw DomainError(ErrorCode::InvalidData, "Snapshot ID does not exist in history.");
    }
    records.erase(iterator);

    const bool tombstoned = snapshotStore.movePayloadToTombstone(snapshotId);
    try
    {
        configurationStore.saveHistoryIndex(records);
    }
    catch (...)
    {
        if (tombstoned)
        {
            snapshotStore.restoreTombstone(snapshotId);
        }
        throw;
    }
    if (tombstoned)
    {
        snapshotStore.removeTombstone(snapshotId);
    }
}

void HistoryStore::clearRootHistory(const QString &rootId) const
{
    validateUuid(rootId);
    HistoryMutationLock lock(m_paths);
    ConfigurationStore configurationStore(m_paths);
    SnapshotStore snapshotStore(m_paths);
    QList<HistoryRecord> records = configurationStore.loadHistoryIndex().value;
    const QList<HistoryRecord> originalRecords = records;
    QList<QString> ids;
    for (const HistoryRecord &record : records)
    {
        if (record.rootId == rootId)
        {
            ids.append(record.snapshotId);
        }
    }
    if (ids.isEmpty())
    {
        return;
    }
    std::sort(ids.begin(), ids.end());
    records.erase(std::remove_if(records.begin(), records.end(),
                                 [&rootId](const HistoryRecord &record)
                                 { return record.rootId == rootId; }),
                  records.end());

    const auto configurationResult = configurationStore.loadConfiguration();
    const auto root = std::find_if(
        configurationResult.value.roots.cbegin(), configurationResult.value.roots.cend(),
        [&rootId](const WatchedRoot &watchedRoot) { return watchedRoot.rootId == rootId; });
    if (configurationResult.restoredDefaults())
    {
        throw DomainError(ErrorCode::InvalidData,
                          "Cannot clear history while configuration is corrupt.");
    }
    const bool hasConfigurationRoot = root != configurationResult.value.roots.cend();
    Configuration configuration = configurationResult.value;
    if (hasConfigurationRoot)
    {
        auto mutableRoot = std::find_if(configuration.roots.begin(), configuration.roots.end(),
                                        [&rootId](const WatchedRoot &watchedRoot)
                                        { return watchedRoot.rootId == rootId; });
        mutableRoot->lastSnapshotUtc.reset();
        mutableRoot->lastScanError.clear();
    }

    QList<QString> tombstonedIds;
    try
    {
        for (const QString &id : ids)
        {
            if (snapshotStore.movePayloadToTombstone(id))
            {
                tombstonedIds.append(id);
            }
        }
        configurationStore.saveHistoryIndex(records);
        if (hasConfigurationRoot)
        {
            configurationStore.saveConfiguration(configuration);
        }
    }
    catch (...)
    {
        try
        {
            configurationStore.saveHistoryIndex(originalRecords);
        }
        catch (...)
        {
        }
        for (auto iterator = tombstonedIds.crbegin(); iterator != tombstonedIds.crend(); ++iterator)
        {
            snapshotStore.restoreTombstone(*iterator);
        }
        throw;
    }
    for (const QString &id : tombstonedIds)
    {
        snapshotStore.removeTombstone(id);
    }
}

void HistoryStore::removeWatchedRoot(const QString &rootId) const
{
    validateUuid(rootId);
    HistoryMutationLock lock(m_paths);
    ConfigurationStore configurationStore(m_paths);
    SnapshotStore snapshotStore(m_paths);

    const auto configurationResult = configurationStore.loadConfiguration();
    if (configurationResult.restoredDefaults())
    {
        throw DomainError(ErrorCode::InvalidData,
                          "Cannot remove a watched folder while configuration is corrupt.");
    }
    const Configuration originalConfiguration = configurationResult.value;
    Configuration configuration = originalConfiguration;
    const auto root = std::find_if(configuration.roots.begin(), configuration.roots.end(),
                                   [&rootId](const WatchedRoot &watchedRoot)
                                   { return watchedRoot.rootId == rootId; });
    if (root == configuration.roots.end())
    {
        throw DomainError(ErrorCode::InvalidData, "Watched folder does not exist.");
    }
    configuration.roots.erase(root);

    const auto historyResult = configurationStore.loadHistoryIndex();
    if (historyResult.restoredDefaults())
    {
        throw DomainError(ErrorCode::InvalidData,
                          "Cannot remove a watched folder while history is corrupt.");
    }
    const QList<HistoryRecord> originalRecords = historyResult.value;
    QList<HistoryRecord> records = originalRecords;
    QList<QString> snapshotIds;
    for (const HistoryRecord &record : records)
    {
        if (record.rootId == rootId)
        {
            snapshotIds.append(record.snapshotId);
        }
    }
    std::sort(snapshotIds.begin(), snapshotIds.end());
    records.erase(std::remove_if(records.begin(), records.end(),
                                 [&rootId](const HistoryRecord &record)
                                 { return record.rootId == rootId; }),
                  records.end());

    QList<QString> tombstonedIds;
    try
    {
        for (const QString &snapshotId : snapshotIds)
        {
            if (snapshotStore.movePayloadToTombstone(snapshotId))
            {
                tombstonedIds.append(snapshotId);
            }
        }
        configurationStore.saveHistoryIndex(records);
        configurationStore.saveConfiguration(configuration);
    }
    catch (...)
    {
        try
        {
            configurationStore.saveConfiguration(originalConfiguration);
            configurationStore.saveHistoryIndex(originalRecords);
        }
        catch (...)
        {
        }
        for (auto iterator = tombstonedIds.crbegin(); iterator != tombstonedIds.crend(); ++iterator)
        {
            snapshotStore.restoreTombstone(*iterator);
        }
        throw;
    }
    for (const QString &snapshotId : tombstonedIds)
    {
        snapshotStore.removeTombstone(snapshotId);
    }
}

HistoryRepairResult HistoryStore::repair() const
{
    HistoryMutationLock lock(m_paths);
    ConfigurationStore configurationStore(m_paths);
    SnapshotStore snapshotStore(m_paths);
    const auto loaded = configurationStore.loadHistoryIndex();
    QList<HistoryRecord> records = loaded.value;
    QSet<QString> indexedIds;
    for (const HistoryRecord &record : records)
    {
        indexedIds.insert(record.snapshotId);
    }

    HistoryRepairResult result;
    const QDir snapshotDirectory(m_paths.snapshotsDirectory);
    const QStringList tombstones =
        snapshotDirectory.entryList({"*.snapshot.deleting"}, QDir::Files, QDir::Name);
    for (const QString &filename : tombstones)
    {
        const QString id = idFromPayloadFilename(filename, ".snapshot.deleting");
        const QString tombstone = snapshotDirectory.filePath(filename);
        if (id.isEmpty())
        {
            if (QFile::remove(tombstone))
            {
                ++result.removedTombstones;
            }
            continue;
        }
        if (loaded.restoredDefaults())
        {
            if (!snapshotStore.hasPayload(id))
            {
                snapshotStore.restoreTombstone(id);
                ++result.restoredTombstones;
            }
            else if (QFile::remove(tombstone))
            {
                ++result.removedTombstones;
            }
            continue;
        }
        if (!indexedIds.contains(id))
        {
            if (QFile::remove(tombstone))
            {
                ++result.removedTombstones;
            }
            continue;
        }
        if (!snapshotStore.hasPayload(id))
        {
            snapshotStore.restoreTombstone(id);
            ++result.restoredTombstones;
        }
        else if (QFile::remove(tombstone))
        {
            ++result.removedTombstones;
        }
    }

    const QStringList payloads =
        snapshotDirectory.entryList({"*.snapshot"}, QDir::Files, QDir::Name);
    for (const QString &filename : payloads)
    {
        const QString id = idFromPayloadFilename(filename, ".snapshot");
        if (id.isEmpty() || indexedIds.contains(id))
        {
            continue;
        }
        try
        {
            const Snapshot snapshot = snapshotStore.loadSnapshot(id);
            records.append(recordForSnapshot(
                snapshot, QFileInfo(snapshotDirectory.filePath(filename)).size()));
            indexedIds.insert(id);
            ++result.addedRecords;
        }
        catch (const DomainError &error)
        {
            if (error.code() == ErrorCode::Io)
            {
                throw;
            }
        }
    }

    if (loaded.restoredDefaults() || result.addedRecords > 0)
    {
        sortRecords(records);
        configurationStore.saveHistoryIndex(records);
    }
    return result;
}
} // namespace foldersnap
