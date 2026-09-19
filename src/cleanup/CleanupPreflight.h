#pragma once

#include <functional>

#include <QList>
#include <QString>
#include <QStringList>

#include "domain/Snapshot.h"
#include "paths/WindowsPaths.h"

namespace foldersnap
{
enum class CleanupStatus
{
    Ready,
    AlreadyMissing,
    ChangedSinceSnapshot,
    TypeChanged,
    OutsideRootOrInvalid,
    AccessDeniedOrUnreadable,
    ContainsUntrackedContent,
    MovedToRecycleBin,
    Failed
};

[[nodiscard]] QString cleanupStatusName(CleanupStatus status);

struct CleanupCandidate
{
    SnapshotEntry after;
};

struct CleanupPreflightItem
{
    QString path;
    CleanupStatus status{CleanupStatus::Failed};
    QString detail;
};

struct CleanupPreflightSummary
{
    int readyCount{0};
    int blockedCount{0};
    int alreadyMissingCount{0};
};

struct CleanupPreflightResult
{
    QList<CleanupPreflightItem> items;
    CleanupPreflightSummary summary;
    bool cancelled{false};
};

class CleanupPreflight final
{
  public:
    using CancellationCallback = std::function<bool()>;

    [[nodiscard]] static CleanupPreflightResult inspect(const RootPath &root,
                                                        const QList<CleanupCandidate> &candidates,
                                                        const QStringList &selectedPaths,
                                                        const CancellationCallback &cancelled = {});
};
} // namespace foldersnap
