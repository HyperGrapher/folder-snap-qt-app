#pragma once

#include <QByteArray>
#include <QJsonObject>

#include "diff/DiffEngine.h"
#include "domain/Snapshot.h"

namespace foldersnap
{
class ExportBuilder final
{
  public:
    [[nodiscard]] static QJsonObject snapshotDto(const Snapshot &snapshot);
    [[nodiscard]] static QJsonObject comparisonDto(const Snapshot &before, const Snapshot &after,
                                                   const DiffResult &diff);
    [[nodiscard]] static QByteArray snapshotCsv(const Snapshot &snapshot);
    [[nodiscard]] static QByteArray comparisonCsv(const Snapshot &before, const Snapshot &after,
                                                  const DiffResult &diff);
};
} // namespace foldersnap
