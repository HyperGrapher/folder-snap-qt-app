#pragma once

#include <functional>

#include <QList>
#include <QString>
#include <QStringList>

#include "cleanup/CleanupPreflight.h"
#include "storage/StoragePaths.h"

namespace foldersnap
{
struct CleanupMoveResult
{
    bool moved{false};
    bool alreadyMissing{false};
    bool aborted{false};
    QString detail;
};

using CleanupMoveCallback = std::function<CleanupMoveResult(const QString &absolutePath)>;

struct CleanupExecutionRequest
{
    StoragePaths paths;
    RootPath root;
    QString rootId;
    QString beforeId;
    QString afterId;
    QList<CleanupCandidate> candidates;
    QStringList selectedPaths;
};

struct CleanupExecutionItem
{
    QString path;
    CleanupStatus status{CleanupStatus::Failed};
    QString detail;
};

struct CleanupExecutionSummary
{
    int movedCount{0};
    int blockedCount{0};
    int alreadyMissingCount{0};
    int failedCount{0};
};

struct CleanupExecutionResult
{
    CleanupPreflightResult preflight;
    QList<CleanupExecutionItem> items;
    CleanupExecutionSummary summary;
    QString error;
    bool cancelled{false};
};

class CleanupExecutor final
{
  public:
    using CancellationCallback = CleanupPreflight::CancellationCallback;

    [[nodiscard]] static CleanupExecutionResult execute(const CleanupExecutionRequest &request,
                                                        const CleanupMoveCallback &move = {},
                                                        const CancellationCallback &cancelled = {});

    [[nodiscard]] static QString auditPath(const StoragePaths &paths, const QString &rootId);
};
} // namespace foldersnap
