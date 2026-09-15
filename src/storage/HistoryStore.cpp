#include "storage/HistoryStore.h"

#include <algorithm>
#include <utility>

#include <QDir>
#include <QElapsedTimer>
#include <QLockFile>
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
} // namespace foldersnap
