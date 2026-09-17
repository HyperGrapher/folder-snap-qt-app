#pragma once

#include <QList>

#include "domain/Configuration.h"
#include "domain/Snapshot.h"
#include "storage/StoragePaths.h"

namespace foldersnap
{
struct SnapshotCommitResult
{
    HistoryRecord record;
    QList<QString> prunedSnapshotIds;
};

struct HistoryRepairResult
{
    int restoredTombstones{0};
    int removedTombstones{0};
    int addedRecords{0};
};

class HistoryStore final
{
  public:
    explicit HistoryStore(StoragePaths paths);

    [[nodiscard]] QList<HistoryRecord> loadHistory() const;
    [[nodiscard]] QList<HistoryRecord> loadHistoryForRoot(const QString &rootId) const;
    [[nodiscard]] SnapshotCommitResult commitSnapshot(const Snapshot &snapshot,
                                                      int retention) const;
    void updateDescription(const QString &snapshotId, const QString &description) const;
    void deleteSnapshot(const QString &snapshotId) const;
    void clearRootHistory(const QString &rootId) const;
    void removeWatchedRoot(const QString &rootId) const;
    [[nodiscard]] HistoryRepairResult repair() const;

  private:
    StoragePaths m_paths;
};
} // namespace foldersnap
