#pragma once

#include <functional>
#include <optional>

#include <QString>
#include <QStringList>

#include "domain/Snapshot.h"
#include "paths/WindowsPaths.h"

namespace foldersnap
{
struct ScanRequest
{
    QString rootId;
    QString displayTitle;
    RootPath root;
    QStringList ignoreRules;
    std::optional<QString> protectedSubtree;
    SnapshotTrigger trigger{SnapshotTrigger::Manual};
    int directoryWorkerCount{4};
};

struct ScanResult
{
    Snapshot snapshot;
    QString error;
    bool cancelled{false};
};

class MetadataScanner final
{
  public:
    using ProgressCallback = std::function<void(int)>;
    using CancellationCallback = std::function<bool()>;

    [[nodiscard]] static ScanResult scan(const ScanRequest &request,
                                         const ProgressCallback &progress,
                                         const CancellationCallback &cancelled);
};
} // namespace foldersnap
