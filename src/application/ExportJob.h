#pragma once

#include <functional>

#include <QByteArray>
#include <QMetaType>
#include <QString>

#include "storage/StoragePaths.h"

namespace foldersnap
{
enum class ExportFormat
{
    Html,
    Csv
};

struct ExportJobRequest
{
    StoragePaths paths;
    QString firstSnapshotId;
    QString secondSnapshotId;
    QString destinationPath;
    ExportFormat format{ExportFormat::Html};
    QByteArray htmlTemplate;
};

struct ExportJobResult
{
    QString destinationPath;
    qint64 bytesWritten{0};
    bool succeeded{false};
    bool cancelled{false};
    QString error;
};

class ExportJob final
{
  public:
    using CancellationCallback = std::function<bool()>;

    [[nodiscard]] static ExportJobResult run(const ExportJobRequest &request,
                                             const CancellationCallback &cancelled = {});
};
} // namespace foldersnap

Q_DECLARE_METATYPE(foldersnap::ExportJobResult)
