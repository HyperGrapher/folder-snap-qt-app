#include "application/ExportJob.h"

#include <utility>

#include "diff/DiffEngine.h"
#include "domain/DomainError.h"
#include "export/ExportBuilder.h"
#include "storage/AtomicFile.h"
#include "storage/SnapshotStore.h"

namespace foldersnap
{
namespace
{
void checkCancelled(const ExportJob::CancellationCallback &cancelled)
{
    if (cancelled && cancelled())
    {
        throw DomainError(ErrorCode::Cancelled, "Export was cancelled.");
    }
}

QByteArray buildSnapshotReport(const Snapshot &snapshot, const ExportJobRequest &request,
                               const ExportJob::CancellationCallback &cancelled)
{
    if (request.format == ExportFormat::Csv)
    {
        return ExportBuilder::snapshotCsv(snapshot, cancelled);
    }
    return ExportBuilder::htmlReport(ExportBuilder::snapshotDto(snapshot, cancelled),
                                     request.htmlTemplate);
}

QByteArray buildComparisonReport(Snapshot first, Snapshot second, const ExportJobRequest &request,
                                 const ExportJob::CancellationCallback &cancelled)
{
    if (second.header.completedAtUtc < first.header.completedAtUtc)
    {
        std::swap(first, second);
    }
    const DiffResult diff = DiffEngine::compare(first, second, cancelled);
    if (diff.cancelled)
    {
        throw DomainError(ErrorCode::Cancelled, "Export was cancelled.");
    }
    if (request.format == ExportFormat::Csv)
    {
        return ExportBuilder::comparisonCsv(first, second, diff, cancelled);
    }
    return ExportBuilder::htmlReport(ExportBuilder::comparisonDto(first, second, diff, cancelled),
                                     request.htmlTemplate);
}
} // namespace

ExportJobResult ExportJob::run(const ExportJobRequest &request,
                               const CancellationCallback &cancelled)
{
    ExportJobResult result;
    result.destinationPath = request.destinationPath;
    try
    {
        if (request.destinationPath.isEmpty())
        {
            throw DomainError(ErrorCode::InvalidPath, "Export destination cannot be empty.");
        }
        if (request.format == ExportFormat::Html && request.htmlTemplate.isEmpty())
        {
            throw DomainError(ErrorCode::InvalidData, "The HTML export template is missing.");
        }

        checkCancelled(cancelled);
        const SnapshotStore store(request.paths);
        Snapshot first = store.loadSnapshot(request.firstSnapshotId, cancelled);
        checkCancelled(cancelled);

        QByteArray report;
        if (request.secondSnapshotId.isEmpty())
        {
            report = buildSnapshotReport(first, request, cancelled);
        }
        else
        {
            Snapshot second = store.loadSnapshot(request.secondSnapshotId, cancelled);
            checkCancelled(cancelled);
            report = buildComparisonReport(std::move(first), std::move(second), request, cancelled);
        }
        checkCancelled(cancelled);
        if (!replaceFileAtomically(request.destinationPath, report, cancelled))
        {
            result.cancelled = true;
            return result;
        }
        result.bytesWritten = report.size();
        result.succeeded = true;
    }
    catch (const DomainError &error)
    {
        if (error.code() == ErrorCode::Cancelled)
        {
            result.cancelled = true;
        }
        else
        {
            result.error = error.message();
        }
    }
    catch (const std::exception &error)
    {
        result.error = QString::fromUtf8(error.what());
    }
    return result;
}
} // namespace foldersnap
