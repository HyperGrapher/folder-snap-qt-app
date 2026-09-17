#pragma once

#include <functional>

#include <QByteArray>
#include <QJsonObject>

#include "diff/DiffEngine.h"
#include "domain/Snapshot.h"

namespace foldersnap
{
class ExportBuilder final
{
  public:
    using CancellationCallback = std::function<bool()>;

    [[nodiscard]] static QJsonObject snapshotDto(const Snapshot &snapshot,
                                                 const CancellationCallback &cancelled = {});
    [[nodiscard]] static QJsonObject comparisonDto(const Snapshot &before, const Snapshot &after,
                                                   const DiffResult &diff,
                                                   const CancellationCallback &cancelled = {});
    [[nodiscard]] static QByteArray snapshotCsv(const Snapshot &snapshot,
                                                const CancellationCallback &cancelled = {});
    [[nodiscard]] static QByteArray comparisonCsv(const Snapshot &before, const Snapshot &after,
                                                  const DiffResult &diff,
                                                  const CancellationCallback &cancelled = {});
    [[nodiscard]] static QByteArray htmlReport(const QJsonObject &dto,
                                               const QByteArray &templateHtml);
};
} // namespace foldersnap
