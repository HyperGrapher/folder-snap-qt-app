#pragma once

#include <optional>

#include <QList>
#include <QString>

#include "diff/DiffEngine.h"
#include "domain/Snapshot.h"

namespace foldersnap
{
struct ComparisonTreeRow
{
    QString path;
    QString name;
    int depth{0};
    bool folder{false};
    std::optional<ChangeKind> change;
    qint64 beforeBytes{0};
    qint64 afterBytes{0};
    bool hasBeforeSize{false};
    bool hasAfterSize{false};
};

[[nodiscard]] QList<ComparisonTreeRow>
buildComparisonTree(const Snapshot &before, const Snapshot &after, const DiffResult &diff);
} // namespace foldersnap
