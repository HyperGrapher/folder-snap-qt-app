#pragma once

#include <functional>
#include <optional>

#include <QList>
#include <QString>

#include "domain/Snapshot.h"

namespace foldersnap
{
enum class ChangeKind
{
    Added,
    Removed,
    Modified,
    Unchanged,
    Uncertain,
    ScopeDifference
};

enum class ModificationKind
{
    None,
    Metadata,
    TypeChanged
};

struct DiffEntry
{
    QString path;
    ChangeKind kind{ChangeKind::Unchanged};
    ModificationKind modification{ModificationKind::None};
    std::optional<SnapshotEntry> before;
    std::optional<SnapshotEntry> after;
};

struct DiffSummary
{
    qint64 comparedCount{0};
    qint64 addedCount{0};
    qint64 removedCount{0};
    qint64 modifiedCount{0};
    qint64 unchangedCount{0};
    qint64 uncertainCount{0};
    qint64 scopeDifferenceCount{0};
    qint64 addedFileBytes{0};
    qint64 removedFileBytes{0};
    qint64 modifiedBeforeFileBytes{0};
    qint64 modifiedAfterFileBytes{0};
    qint64 netFileBytes{0};
    int beforeWarningCount{0};
    int afterWarningCount{0};
    bool ignoreRulesDiffer{false};
};

struct DiffResult
{
    QList<DiffEntry> entries;
    DiffSummary summary;
    bool cancelled{false};
};

class DiffEngine final
{
  public:
    using CancellationCallback = std::function<bool()>;

    [[nodiscard]] static DiffResult compare(const Snapshot &first, const Snapshot &second,
                                            const CancellationCallback &cancelled = {});
};
} // namespace foldersnap
