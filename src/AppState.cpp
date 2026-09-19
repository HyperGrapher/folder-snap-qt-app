#include "AppState.h"

#include <algorithm>
#include <exception>
#include <limits>
#include <optional>
#include <utility>

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QPromise>
#include <QSet>
#include <QTimeZone>
#include <QUrl>
#include <QtConcurrent>

#include "cleanup/CleanupPreflight.h"
#include "cleanup/CleanupExecutor.h"
#include "diff/ComparisonTree.h"
#include "diff/DiffEngine.h"
#include "domain/DomainError.h"
#include "schedule/ScheduleCalculator.h"
#include "storage/ConfigurationStore.h"
#include "storage/HistoryStore.h"
#include "storage/SnapshotStore.h"

namespace
{
using foldersnap::HistoryRecord;
using foldersnap::Snapshot;
using foldersnap::UtcTimestamp;

QString formatCount(qint64 value)
{
    return QLocale().toString(value);
}

QString formatBytes(qint64 bytes)
{
    if (bytes == 0)
    {
        return "0 B";
    }
    constexpr double unit = 1024.0;
    double value = static_cast<double>(bytes);
    const QStringList suffixes{"B", "KB", "MB", "GB", "TB"};
    int suffix = 0;
    while (value >= unit && suffix < suffixes.size() - 1)
    {
        value /= unit;
        ++suffix;
    }
    const int decimals = value >= 100.0 || suffix == 0 ? 0 : 1;
    return QString::number(value, 'f', decimals) + " " + suffixes[suffix];
}

QString formatSignedBytes(qint64 bytes)
{
    if (bytes == 0)
    {
        return "0 B";
    }
    if (bytes == std::numeric_limits<qint64>::min())
    {
        return "−8 EB";
    }
    return (bytes > 0 ? "+" : "−") + formatBytes(qAbs(bytes));
}

QString localDate(UtcTimestamp timestamp)
{
    return QDateTime::fromMSecsSinceEpoch(timestamp.nanoseconds / 1000000,
                                          QTimeZone::systemTimeZone())
        .toString("dd MMMM yyyy · HH:mm");
}

QString shortDate(UtcTimestamp timestamp)
{
    return QDateTime::fromMSecsSinceEpoch(timestamp.nanoseconds / 1000000,
                                          QTimeZone::systemTimeZone())
        .toString("dd MMM · HH:mm");
}

std::optional<foldersnap::ExportFormat> exportFormat(const QString &format)
{
    if (format.compare("html", Qt::CaseInsensitive) == 0)
    {
        return foldersnap::ExportFormat::Html;
    }
    if (format.compare("csv", Qt::CaseInsensitive) == 0)
    {
        return foldersnap::ExportFormat::Csv;
    }
    return std::nullopt;
}

QString exportPath(const QUrl &destination, foldersnap::ExportFormat format)
{
    QString path = destination.toLocalFile();
    if (path.isEmpty())
    {
        throw foldersnap::DomainError(foldersnap::ErrorCode::InvalidPath,
                                      "Choose a local export destination.");
    }
    if (QFileInfo(path).suffix().isEmpty())
    {
        path += format == foldersnap::ExportFormat::Html ? ".html" : ".csv";
    }
    return path;
}

QString triggerName(foldersnap::SnapshotTrigger trigger)
{
    return trigger == foldersnap::SnapshotTrigger::Scheduled ? "Scheduled" : "Manual";
}

QString scheduleName(const foldersnap::Schedule &schedule)
{
    switch (schedule.kind)
    {
    case foldersnap::ScheduleKind::Interval:
        return schedule.intervalHours == 1 ? "Every 1 hour"
                                           : QString("Every %1 hours").arg(schedule.intervalHours);
    case foldersnap::ScheduleKind::Daily:
        return QString("Daily at %1:%2")
            .arg(schedule.hour, 2, 10, QChar('0'))
            .arg(schedule.minute, 2, 10, QChar('0'));
    case foldersnap::ScheduleKind::Weekly:
        return QString("Weekly · %1 %2:%3")
            .arg(QStringList{"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
                             "Saturday"}
                     .value(schedule.weekday),
                 QString::number(schedule.hour).rightJustified(2, '0'))
            .arg(schedule.minute, 2, 10, QChar('0'));
    case foldersnap::ScheduleKind::Monthly:
        return QString("Monthly · day %1, %2:%3")
            .arg(schedule.dayOfMonth)
            .arg(schedule.hour, 2, 10, QChar('0'))
            .arg(schedule.minute, 2, 10, QChar('0'));
    case foldersnap::ScheduleKind::Manual:
        return "Manual only";
    }
    return "Manual only";
}

foldersnap::Schedule parseSchedule(const QString &text)
{
    foldersnap::Schedule schedule;
    if (text.startsWith("Every "))
    {
        schedule.kind = foldersnap::ScheduleKind::Interval;
        schedule.intervalHours = text.section(' ', 1, 1).toInt();
    }
    else if (text.startsWith("Daily"))
    {
        schedule.kind = foldersnap::ScheduleKind::Daily;
        schedule.hour = 9;
    }
    else if (text.startsWith("Weekly"))
    {
        schedule.kind = foldersnap::ScheduleKind::Weekly;
        schedule.weekday = 1;
        schedule.hour = 9;
    }
    else if (text.startsWith("Monthly"))
    {
        schedule.kind = foldersnap::ScheduleKind::Monthly;
        schedule.dayOfMonth = 1;
        schedule.hour = 9;
    }
    return schedule;
}

foldersnap::StoragePaths appStoragePaths()
{
    const QString overrideDirectory = qEnvironmentVariable("FOLDERSNAP_DATA_DIR");
    return overrideDirectory.isEmpty()
               ? foldersnap::StoragePaths::forCurrentUser()
               : foldersnap::StoragePaths::fromDataDirectory(overrideDirectory);
}

QVariantMap mapForRecord(const HistoryRecord &record)
{
    QVariantMap result;
    result["id"] = record.snapshotId;
    result["date"] = shortDate(record.completedAtUtc);
    result["fullDate"] = localDate(record.completedAtUtc);
    result["description"] = record.description.isEmpty() ? "No description" : record.description;
    result["trigger"] = triggerName(record.trigger);
    result["size"] = formatBytes(record.totalFileBytes);
    result["files"] = formatCount(record.fileCount);
    result["folders"] = formatCount(record.directoryCount);
    result["warningCount"] = record.warningCount;
    result["payloadAvailable"] = record.payloadAvailable;
    result["completedNs"] = record.completedAtUtc.nanoseconds;
    return result;
}

QVariantMap mapForRoot(const foldersnap::WatchedRoot &root, const QList<HistoryRecord> &history,
                       int colorIndex)
{
    QVariantMap result;
    result["name"] = root.displayName;
    result["path"] = root.path;
    result["archived"] = root.archived;
    result["schedule"] = scheduleName(root.schedule);
    result["retention"] = root.retention;
    result["snapshots"] = history.size();
    result["color"] = QStringList{"#94e6c6", "#b7a8e6", "#e9c387", "#8396a5"}[colorIndex % 4];
    result["note"] = root.archived       ? "History kept, watching paused"
                     : history.isEmpty() ? "Ready for its first snapshot"
                                         : "Metadata history for this folder";
    if (history.isEmpty())
    {
        result["size"] = "0 B";
        result["files"] = "0";
    }
    else
    {
        result["size"] = formatBytes(history.first().totalFileBytes);
        result["files"] = formatCount(history.first().fileCount);
    }
    return result;
}

QString changeStatus(foldersnap::ChangeKind kind)
{
    switch (kind)
    {
    case foldersnap::ChangeKind::Added:
        return "Added";
    case foldersnap::ChangeKind::Removed:
        return "Removed";
    case foldersnap::ChangeKind::Modified:
        return "Modified";
    case foldersnap::ChangeKind::Uncertain:
        return "Uncertain";
    case foldersnap::ChangeKind::ScopeDifference:
        return "Scope difference";
    case foldersnap::ChangeKind::Unchanged:
        return {};
    }
    return {};
}

ComparisonJobResult compareSnapshots(const foldersnap::StoragePaths &paths, const QString &beforeId,
                                     const QString &afterId,
                                     const foldersnap::DiffEngine::CancellationCallback &cancelled)
{
    ComparisonJobResult result;
    try
    {
        const foldersnap::SnapshotStore store(paths);
        const Snapshot before = store.loadSnapshot(beforeId);
        if (cancelled())
        {
            result.cancelled = true;
            return result;
        }
        const Snapshot after = store.loadSnapshot(afterId);
        if (cancelled())
        {
            result.cancelled = true;
            return result;
        }
        if (before.header.rootId != after.header.rootId)
        {
            result.error = "Snapshots from different folders cannot be compared.";
            return result;
        }

        const foldersnap::DiffResult diff =
            foldersnap::DiffEngine::compare(before, after, cancelled);
        if (diff.cancelled)
        {
            result.cancelled = true;
            return result;
        }
        result.addedCount = static_cast<int>(diff.summary.addedCount);
        result.removedCount = static_cast<int>(diff.summary.removedCount);
        result.modifiedCount = static_cast<int>(diff.summary.modifiedCount);
        result.unchangedCount = static_cast<int>(diff.summary.unchangedCount);
        result.warningCount =
            diff.summary.beforeWarningCount + diff.summary.afterWarningCount +
            static_cast<int>(diff.summary.uncertainCount + diff.summary.scopeDifferenceCount);
        result.comparedCount = static_cast<int>(diff.summary.comparedCount);
        result.netSize = diff.summary.netFileBytes;

        const QList<foldersnap::ComparisonTreeRow> rows =
            foldersnap::buildComparisonTree(before, after, diff);
        for (const foldersnap::ComparisonTreeRow &treeRow : rows)
        {
            QVariantMap row;
            row["path"] = treeRow.path;
            row["name"] = treeRow.name;
            row["depth"] = treeRow.depth;
            row["folder"] = treeRow.folder;
            row["status"] = treeRow.change ? changeStatus(*treeRow.change) : QString{};
            row["before"] = treeRow.hasBeforeSize ? formatBytes(treeRow.beforeBytes) : QString("—");
            row["after"] = treeRow.hasAfterSize ? formatBytes(treeRow.afterBytes) : QString("—");
            row["beforeBytes"] = treeRow.hasBeforeSize ? QVariant(treeRow.beforeBytes) : QVariant{};
            row["afterBytes"] = treeRow.hasAfterSize ? QVariant(treeRow.afterBytes) : QVariant{};
            result.changes.append(row);
        }
    }
    catch (const foldersnap::DomainError &error)
    {
        result.error = error.message();
    }
    return result;
}

CleanupPreflightJobResult runCleanupPreflight(
    const foldersnap::StoragePaths &paths, const foldersnap::RootPath &root, const QString &rootId,
    const QString &afterId, const QVariantList &candidateRows, const QStringList &selectedPaths,
    quint64 generation, const foldersnap::CleanupPreflight::CancellationCallback &cancelled)
{
    CleanupPreflightJobResult result;
    result.rootId = rootId;
    result.afterId = afterId;
    result.generation = generation;
    try
    {
        if (cancelled && cancelled())
        {
            result.cancelled = true;
            return result;
        }
        const foldersnap::Snapshot snapshot =
            foldersnap::SnapshotStore(paths).loadSnapshot(afterId);
        QHash<QString, foldersnap::SnapshotEntry> entriesByPath;
        for (const foldersnap::SnapshotEntry &entry : snapshot.entries)
        {
            entriesByPath.insert(entry.path, entry);
        }

        QList<foldersnap::CleanupCandidate> candidates;
        candidates.reserve(candidateRows.size());
        for (const QVariant &value : candidateRows)
        {
            if (cancelled && cancelled())
            {
                result.cancelled = true;
                return result;
            }
            const QString path = value.toMap().value("path").toString();
            const auto entry = entriesByPath.constFind(path);
            if (entry == entriesByPath.cend())
            {
                result.error =
                    QString("The cleanup candidate is missing from snapshot %1.").arg(afterId);
                return result;
            }
            candidates.append({*entry});
        }
        result.result =
            foldersnap::CleanupPreflight::inspect(root, candidates, selectedPaths, cancelled);
        result.cancelled = result.result.cancelled || (cancelled && cancelled());
    }
    catch (const foldersnap::DomainError &error)
    {
        result.error = error.message();
    }
    catch (const std::exception &error)
    {
        result.error = QString::fromUtf8(error.what());
    }
    catch (...)
    {
        result.error = "Unexpected cleanup preflight failure.";
    }
    return result;
}

CleanupExecutionJobResult
runCleanupExecution(const foldersnap::StoragePaths &paths, const foldersnap::RootPath &root,
                    const QString &rootId, const QString &beforeId, const QString &afterId,
                    const QVariantList &candidateRows, const QStringList &selectedPaths,
                    quint64 generation,
                    const foldersnap::CleanupExecutor::CancellationCallback &cancelled)
{
    CleanupExecutionJobResult result;
    result.rootId = rootId;
    result.beforeId = beforeId;
    result.afterId = afterId;
    result.generation = generation;
    try
    {
        if (cancelled && cancelled())
        {
            result.cancelled = true;
            return result;
        }
        const foldersnap::Snapshot snapshot =
            foldersnap::SnapshotStore(paths).loadSnapshot(afterId, cancelled);
        QHash<QString, foldersnap::SnapshotEntry> entriesByPath;
        for (const foldersnap::SnapshotEntry &entry : snapshot.entries)
        {
            entriesByPath.insert(entry.path, entry);
        }

        QList<foldersnap::CleanupCandidate> candidates;
        candidates.reserve(candidateRows.size());
        for (const QVariant &value : candidateRows)
        {
            if (cancelled && cancelled())
            {
                result.cancelled = true;
                return result;
            }
            const QString path = value.toMap().value("path").toString();
            const auto entry = entriesByPath.constFind(path);
            if (entry == entriesByPath.cend())
            {
                result.error =
                    QString("The cleanup candidate is missing from snapshot %1.").arg(afterId);
                return result;
            }
            candidates.append({*entry});
        }

        foldersnap::CleanupExecutionRequest request;
        request.paths = paths;
        request.root = root;
        request.rootId = rootId;
        request.beforeId = beforeId;
        request.afterId = afterId;
        request.candidates = std::move(candidates);
        request.selectedPaths = selectedPaths;
        result.result = foldersnap::CleanupExecutor::execute(request, {}, cancelled);
        result.cancelled = result.result.cancelled || (cancelled && cancelled());
    }
    catch (const foldersnap::DomainError &error)
    {
        result.error = error.message();
    }
    catch (const std::exception &error)
    {
        result.error = QString::fromUtf8(error.what());
    }
    catch (...)
    {
        result.error = "Unexpected cleanup execution failure.";
    }
    return result;
}

QString cleanupStatusLabel(foldersnap::CleanupStatus status)
{
    switch (status)
    {
    case foldersnap::CleanupStatus::Ready:
        return "Ready";
    case foldersnap::CleanupStatus::AlreadyMissing:
        return "Already missing";
    case foldersnap::CleanupStatus::ChangedSinceSnapshot:
        return "Changed";
    case foldersnap::CleanupStatus::TypeChanged:
        return "Type changed";
    case foldersnap::CleanupStatus::OutsideRootOrInvalid:
        return "Invalid or outside root";
    case foldersnap::CleanupStatus::AccessDeniedOrUnreadable:
        return "Unreadable";
    case foldersnap::CleanupStatus::ContainsUntrackedContent:
        return "Untracked content";
    case foldersnap::CleanupStatus::MovedToRecycleBin:
        return "Moved to Recycle Bin";
    case foldersnap::CleanupStatus::Failed:
        return "Failed";
    }
    return "Failed";
}

bool clearCleanupPreflightFields(QVariantList &rows)
{
    bool changed = false;
    for (QVariant &value : rows)
    {
        QVariantMap row = value.toMap();
        changed = row.remove("preflightStatus") > 0 || changed;
        changed = row.remove("preflightStatusLabel") > 0 || changed;
        changed = row.remove("preflightDetail") > 0 || changed;
        value = row;
    }
    return changed;
}
} // namespace

AppState::AppState(QObject *parent) : QObject(parent), m_paths(appStoragePaths())
{
    try
    {
        m_configuration = foldersnap::ConfigurationStore(m_paths).loadConfiguration().value;
        const foldersnap::HistoryRepairResult repairResult =
            foldersnap::HistoryStore(m_paths).repair();
        Q_UNUSED(repairResult);
    }
    catch (const foldersnap::DomainError &error)
    {
        m_scanError = error.message();
    }
    m_closeToTray = m_configuration.closeToTray;
    m_launchAtStartup = m_configuration.launchAtStartup;
    m_notifyScheduledBefore = m_configuration.notifyScheduledBefore;
    m_retention = m_configuration.defaultRetention;
    m_scanCoordinator = std::make_unique<foldersnap::ScanCoordinator>(this);
    m_exportCoordinator = std::make_unique<foldersnap::ExportCoordinator>(this);
    m_comparisonWatcher = std::make_unique<QFutureWatcher<ComparisonJobResult>>(this);
    m_cleanupPreflightWatcher = std::make_unique<QFutureWatcher<CleanupPreflightJobResult>>(this);
    m_cleanupExecutionWatcher = std::make_unique<QFutureWatcher<CleanupExecutionJobResult>>(this);
    connect(m_scanCoordinator.get(), &foldersnap::ScanCoordinator::activeChanged, this,
            [this](const QString &rootId, bool active)
            {
                if (active)
                {
                    emit scanStarted(rootId);
                }
                updateCurrentScanState();
            });
    connect(m_scanCoordinator.get(), &foldersnap::ScanCoordinator::progressChanged, this,
            [this](const QString &rootId, int value)
            {
                emit scanProgressed(rootId, value);
                const auto *root = currentConfigurationRoot();
                if (!root || root->rootId != rootId)
                {
                    return;
                }
                if (m_scanProgress == value)
                {
                    return;
                }
                m_scanProgress = value;
                emit scanProgressChanged();
            });
    connect(m_scanCoordinator.get(), &foldersnap::ScanCoordinator::succeeded, this,
            &AppState::finishScan);
    connect(m_scanCoordinator.get(), &foldersnap::ScanCoordinator::failed, this,
            &AppState::failScan);
    connect(m_scanCoordinator.get(), &foldersnap::ScanCoordinator::cancelled, this,
            [this](const QString &rootId)
            {
                const auto *root = currentConfigurationRoot();
                if (root && root->rootId == rootId)
                {
                    setToast("Snapshot cancelled. Your folder was not changed.");
                }
            });
    connect(m_comparisonWatcher.get(), &QFutureWatcher<ComparisonJobResult>::finished, this,
            &AppState::finishComparison);
    connect(m_cleanupPreflightWatcher.get(), &QFutureWatcher<CleanupPreflightJobResult>::finished,
            this, &AppState::finishCleanupPreflight);
    connect(m_cleanupExecutionWatcher.get(), &QFutureWatcher<CleanupExecutionJobResult>::finished,
            this, &AppState::finishCleanupExecution);
    connect(m_exportCoordinator.get(), &foldersnap::ExportCoordinator::activeChanged, this,
            [this](bool active)
            {
                if (m_exporting == active)
                {
                    return;
                }
                m_exporting = active;
                emit exportChanged();
            });
    connect(
        m_exportCoordinator.get(), &foldersnap::ExportCoordinator::succeeded, this,
        [this](const foldersnap::ExportJobResult &result)
        {
            setExportError({});
            setSheet({});
            setToast(
                QString("Report exported · %1").arg(QFileInfo(result.destinationPath).fileName()));
        });
    connect(m_exportCoordinator.get(), &foldersnap::ExportCoordinator::failed, this,
            [this](const QString &error)
            { setExportError(error.isEmpty() ? "Could not export the report." : error); });
    connect(m_exportCoordinator.get(), &foldersnap::ExportCoordinator::cancelled, this,
            [this]()
            {
                setExportError({});
                setToast("Export cancelled. No report was changed.");
            });
    m_scheduleTimer.setInterval(15000);
    connect(&m_scheduleTimer, &QTimer::timeout, this, &AppState::evaluateSchedules);
    m_scheduleTimer.start();
    refreshModels();
    QTimer::singleShot(0, this, &AppState::evaluateSchedules);
}

AppState::~AppState() = default;

void AppState::setSelectedSection(Section section)
{
    if (section < Section::Overview || section > Section::Settings || section == m_selectedSection)
    {
        return;
    }
    m_selectedSection = section;
    emit selectedSectionChanged();
}

void AppState::setReducedMotion(bool enabled)
{
    if (m_reducedMotion == enabled)
    {
        return;
    }
    m_reducedMotion = enabled;
    emit reducedMotionChanged();
}

void AppState::setBackgroundMotionEnabled(bool enabled)
{
    if (m_backgroundMotionEnabled == enabled)
    {
        return;
    }
    m_backgroundMotionEnabled = enabled;
    emit backgroundMotionEnabledChanged();
}

void AppState::setSnapshotSearch(const QString &search)
{
    if (m_snapshotSearch == search)
    {
        return;
    }
    m_snapshotSearch = search;
    emit snapshotSearchChanged();
    emit snapshotsChanged();
}

void AppState::setExpanded(const QStringList &expanded)
{
    if (m_expanded == expanded)
    {
        return;
    }
    m_expanded = expanded;
    emit comparisonChanged();
}

void AppState::setSearch(const QString &search)
{
    if (m_search == search)
    {
        return;
    }
    m_search = search;
    emit comparisonChanged();
}

void AppState::setFilter(const QString &filter)
{
    if (m_filter == filter)
    {
        return;
    }
    m_filter = filter;
    emit comparisonChanged();
}

void AppState::setSheet(const QString &sheet)
{
    if (m_sheet == sheet)
    {
        return;
    }
    m_sheet = sheet;
    emit sheetChanged();
}

void AppState::setToast(const QString &toast)
{
    if (m_toast == toast)
    {
        return;
    }
    m_toast = toast;
    emit toastChanged();
}

void AppState::setDetailId(const QString &detailId)
{
    if (m_detailId == detailId)
    {
        return;
    }
    m_detailId = detailId;
    emit detailIdChanged();
}

void AppState::setCleanupSelection(const QVariantList &selection)
{
    if (m_cleanupExecuting)
    {
        return;
    }
    const QVariantList &candidates = m_cleanupCandidates;
    QSet<QString> candidatePaths;
    QSet<QString> folderPaths;
    for (const QVariant &candidate : candidates)
    {
        const QVariantMap row = candidate.toMap();
        const QString path = row.value("path").toString();
        candidatePaths.insert(path);
        if (row.value("folder").toBool())
        {
            folderPaths.insert(path);
        }
    }

    QSet<QString> selectedPaths;
    for (const QVariant &value : selection)
    {
        const QString path = value.toString();
        if (candidatePaths.contains(path))
        {
            selectedPaths.insert(path);
        }
    }

    QHash<QString, QStringList> children;
    for (const QString &candidatePath : std::as_const(candidatePaths))
    {
        QString parent = candidatePath;
        while (parent.contains('/'))
        {
            parent = parent.left(parent.lastIndexOf('/'));
            if (folderPaths.contains(parent))
            {
                children[parent].append(candidatePath);
                break;
            }
        }
    }
    for (auto iterator = candidates.crbegin(); iterator != candidates.crend(); ++iterator)
    {
        const QVariantMap candidate = iterator->toMap();
        if (!candidate.value("folder").toBool())
        {
            continue;
        }
        const QString folderPath = candidate.value("path").toString();
        const QStringList childPaths = children.value(folderPath);
        if (childPaths.isEmpty())
        {
            continue;
        }
        const bool allChildrenSelected = std::all_of(childPaths.cbegin(), childPaths.cend(),
                                                     [&selectedPaths](const QString &path)
                                                     { return selectedPaths.contains(path); });
        if (allChildrenSelected)
        {
            selectedPaths.insert(folderPath);
        }
        else
        {
            selectedPaths.remove(folderPath);
        }
    }

    QVariantList normalized;
    for (const QVariant &candidate : candidates)
    {
        const QString path = candidate.toMap().value("path").toString();
        if (selectedPaths.contains(path))
        {
            normalized.append(path);
        }
    }
    if (m_cleanupSelection == normalized)
    {
        return;
    }
    m_cleanupSelection = normalized;
    m_cleanupSelectedPaths = selectedPaths;
    cancelCleanupPreflight();
    const bool rowsChanged = clearCleanupPreflightFields(m_cleanupRows);
    const bool candidatesChanged = clearCleanupPreflightFields(m_cleanupCandidates);
    m_cleanupPartialPaths.clear();
    m_cleanupSelectedBytes = 0;
    for (const QString &selectedPath : std::as_const(selectedPaths))
    {
        QString parent = selectedPath;
        while (parent.contains('/'))
        {
            parent = parent.left(parent.lastIndexOf('/'));
            m_cleanupPartialPaths.insert(parent);
        }
    }
    for (const QVariant &candidate : candidates)
    {
        const QVariantMap row = candidate.toMap();
        if (!row.value("folder").toBool() && selectedPaths.contains(row.value("path").toString()))
        {
            m_cleanupSelectedBytes += row.value("afterBytes").toLongLong();
        }
    }
    m_cleanupReviewed = false;
    m_cleanupResult.clear();
    m_cleanupReadyCount = 0;
    m_cleanupBlockedCount = 0;
    m_cleanupAlreadyMissingCount = 0;
    m_cleanupCompleted = false;
    m_cleanupMovedCount = 0;
    m_cleanupFailedCount = 0;
    if (rowsChanged || candidatesChanged)
    {
        emit cleanupCandidatesChanged();
    }
    emit cleanupChanged();
    if (!m_cleanupSelection.isEmpty())
    {
        startCleanupPreflight();
    }
}

void AppState::setCleanupSearch(const QString &search)
{
    if (m_cleanupSearch == search)
    {
        return;
    }
    m_cleanupSearch = search;
    emit cleanupCandidatesChanged();
}

void AppState::setCleanupReviewed(bool reviewed)
{
    if (m_cleanupReviewed == reviewed)
    {
        return;
    }
    m_cleanupReviewed = reviewed;
    emit cleanupChanged();
}

void AppState::setCleanupResult(const QString &result)
{
    if (m_cleanupResult == result)
    {
        return;
    }
    m_cleanupResult = result;
    emit cleanupChanged();
}

void AppState::cancelCleanupPreflight()
{
    ++m_cleanupPreflightGeneration;
    if (m_cleanupPreflightWatcher && m_cleanupPreflightWatcher->isRunning())
    {
        m_cleanupPreflightWatcher->cancel();
    }
    m_cleanupPreflighting = false;
}

void AppState::startCleanupPreflight()
{
    if (m_cleanupSelection.isEmpty())
    {
        return;
    }
    const auto *root = currentConfigurationRoot();
    if (!root || m_afterId.isEmpty())
    {
        m_cleanupResult = "Choose a completed comparison before checking cleanup items.";
        emit cleanupChanged();
        return;
    }

    foldersnap::RootPath normalizedRoot;
    try
    {
        normalizedRoot = foldersnap::normalizeRootPath(root->path);
    }
    catch (const foldersnap::DomainError &error)
    {
        m_cleanupResult = "Preflight failed · " + error.message();
        emit cleanupChanged();
        return;
    }

    if (m_cleanupPreflightWatcher && m_cleanupPreflightWatcher->isRunning())
    {
        m_cleanupPreflightWatcher->cancel();
    }
    const quint64 generation = ++m_cleanupPreflightGeneration;
    const QString rootId = root->rootId;
    const QString afterId = m_afterId;
    const foldersnap::StoragePaths paths = m_paths;
    const QVariantList candidateRows = m_cleanupCandidates;
    QStringList selectedPaths;
    selectedPaths.reserve(m_cleanupSelection.size());
    for (const QVariant &value : m_cleanupSelection)
    {
        selectedPaths.append(value.toString());
    }

    m_cleanupPreflighting = true;
    m_cleanupReviewed = false;
    m_cleanupResult = "Checking live paths…";
    emit cleanupChanged();
    m_cleanupPreflightWatcher->setFuture(QtConcurrent::run(
        [paths, normalizedRoot, rootId, afterId, candidateRows, selectedPaths,
         generation](QPromise<CleanupPreflightJobResult> &promise)
        {
            const CleanupPreflightJobResult result = runCleanupPreflight(
                paths, normalizedRoot, rootId, afterId, candidateRows, selectedPaths, generation,
                [&promise]() { return promise.isCanceled(); });
            if (!result.cancelled && !promise.isCanceled())
            {
                promise.addResult(result);
            }
        }));
}

void AppState::finishCleanupPreflight()
{
    if (m_cleanupPreflightWatcher->future().isCanceled() ||
        m_cleanupPreflightWatcher->future().resultCount() == 0)
    {
        return;
    }
    const CleanupPreflightJobResult result = m_cleanupPreflightWatcher->result();
    const auto *root = currentConfigurationRoot();
    if (result.generation != m_cleanupPreflightGeneration || !root ||
        result.rootId != root->rootId || result.afterId != m_afterId)
    {
        return;
    }

    m_cleanupPreflighting = false;
    if (!result.error.isEmpty())
    {
        m_cleanupReviewed = false;
        m_cleanupResult = "Preflight failed · " + result.error;
        m_cleanupReadyCount = 0;
        m_cleanupBlockedCount = 0;
        m_cleanupAlreadyMissingCount = 0;
        clearCleanupPreflightFields(m_cleanupRows);
        clearCleanupPreflightFields(m_cleanupCandidates);
        emit cleanupCandidatesChanged();
        emit cleanupChanged();
        return;
    }

    QHash<QString, foldersnap::CleanupPreflightItem> itemsByPath;
    for (const foldersnap::CleanupPreflightItem &item : result.result.items)
    {
        itemsByPath.insert(item.path, item);
    }
    const auto apply = [&itemsByPath](QVariantList &rows)
    {
        for (QVariant &value : rows)
        {
            QVariantMap row = value.toMap();
            const auto item = itemsByPath.constFind(row.value("path").toString());
            if (item == itemsByPath.cend())
            {
                continue;
            }
            row["preflightStatus"] = foldersnap::cleanupStatusName(item->status);
            row["preflightStatusLabel"] = cleanupStatusLabel(item->status);
            row["preflightDetail"] = item->detail;
            value = row;
        }
    };
    apply(m_cleanupRows);
    apply(m_cleanupCandidates);
    m_cleanupReadyCount = result.result.summary.readyCount;
    m_cleanupBlockedCount = result.result.summary.blockedCount;
    m_cleanupAlreadyMissingCount = result.result.summary.alreadyMissingCount;
    m_cleanupReviewed = true;
    m_cleanupResult = QString("%1 ready · %2 blocked · %3 already missing")
                          .arg(m_cleanupReadyCount)
                          .arg(m_cleanupBlockedCount)
                          .arg(m_cleanupAlreadyMissingCount);
    emit cleanupCandidatesChanged();
    emit cleanupChanged();
}

void AppState::executeCleanup()
{
    if (m_cleanupExecuting || m_cleanupCompleted || m_cleanupPreflighting || !m_cleanupReviewed ||
        m_cleanupSelection.isEmpty() || m_cleanupReadyCount <= 0)
    {
        return;
    }
    startCleanupExecution();
}

void AppState::startCleanupExecution()
{
    const auto *root = currentConfigurationRoot();
    if (!root || m_beforeId.isEmpty() || m_afterId.isEmpty())
    {
        m_cleanupResult = "Choose a completed comparison before moving cleanup items.";
        emit cleanupChanged();
        return;
    }

    foldersnap::RootPath normalizedRoot;
    try
    {
        normalizedRoot = foldersnap::normalizeRootPath(root->path);
    }
    catch (const foldersnap::DomainError &error)
    {
        m_cleanupResult = "Cleanup failed · " + error.message();
        emit cleanupChanged();
        return;
    }

    const quint64 generation = ++m_cleanupExecutionGeneration;
    const QString rootId = root->rootId;
    const QString beforeId = m_beforeId;
    const QString afterId = m_afterId;
    const foldersnap::StoragePaths paths = m_paths;
    const QVariantList candidateRows = m_cleanupCandidates;
    QStringList selectedPaths;
    selectedPaths.reserve(m_cleanupSelection.size());
    for (const QVariant &value : m_cleanupSelection)
    {
        selectedPaths.append(value.toString());
    }

    m_cleanupExecuting = true;
    m_cleanupResult = "Rechecking live paths before moving…";
    emit cleanupChanged();
    m_cleanupExecutionWatcher->setFuture(QtConcurrent::run(
        [paths, normalizedRoot, rootId, beforeId, afterId, candidateRows, selectedPaths,
         generation](QPromise<CleanupExecutionJobResult> &promise)
        {
            const CleanupExecutionJobResult result = runCleanupExecution(
                paths, normalizedRoot, rootId, beforeId, afterId, candidateRows, selectedPaths,
                generation, [&promise]() { return promise.isCanceled(); });
            if (!result.cancelled && !promise.isCanceled())
            {
                promise.addResult(result);
            }
        }));
}

void AppState::finishCleanupExecution()
{
    if (m_cleanupExecutionWatcher->future().isCanceled() ||
        m_cleanupExecutionWatcher->future().resultCount() == 0)
    {
        return;
    }
    const CleanupExecutionJobResult result = m_cleanupExecutionWatcher->result();
    const auto *root = currentConfigurationRoot();
    if (result.generation != m_cleanupExecutionGeneration || !root ||
        result.rootId != root->rootId || result.beforeId != m_beforeId ||
        result.afterId != m_afterId)
    {
        return;
    }

    m_cleanupExecuting = false;
    if (!result.error.isEmpty() && result.result.items.isEmpty())
    {
        m_cleanupCompleted = false;
        m_cleanupResult = "Cleanup failed · " + result.error;
        emit cleanupChanged();
        return;
    }

    QHash<QString, foldersnap::CleanupExecutionItem> itemsByPath;
    for (const foldersnap::CleanupExecutionItem &item : result.result.items)
    {
        itemsByPath.insert(item.path, item);
    }
    const auto apply = [&itemsByPath](QVariantList &rows)
    {
        for (QVariant &value : rows)
        {
            QVariantMap row = value.toMap();
            const auto item = itemsByPath.constFind(row.value("path").toString());
            if (item == itemsByPath.cend())
            {
                continue;
            }
            row["preflightStatus"] = foldersnap::cleanupStatusName(item->status);
            row["preflightStatusLabel"] = cleanupStatusLabel(item->status);
            row["preflightDetail"] = item->detail;
            value = row;
        }
    };
    apply(m_cleanupRows);
    apply(m_cleanupCandidates);

    m_cleanupReadyCount = 0;
    m_cleanupBlockedCount = result.result.summary.blockedCount;
    m_cleanupAlreadyMissingCount = result.result.summary.alreadyMissingCount;
    m_cleanupMovedCount = result.result.summary.movedCount;
    m_cleanupFailedCount = result.result.summary.failedCount;
    m_cleanupReviewed = true;
    m_cleanupCompleted = true;
    m_cleanupResult = QString("%1 moved · %2 blocked · %3 already missing · %4 failed")
                          .arg(m_cleanupMovedCount)
                          .arg(m_cleanupBlockedCount)
                          .arg(m_cleanupAlreadyMissingCount)
                          .arg(m_cleanupFailedCount);
    if (!result.error.isEmpty())
    {
        m_cleanupResult += " · " + result.error;
    }
    emit cleanupCandidatesChanged();
    emit cleanupChanged();
}

void AppState::setCloseToTray(bool enabled)
{
    if (m_closeToTray == enabled)
    {
        return;
    }
    m_closeToTray = enabled;
    m_configuration.closeToTray = enabled;
    saveConfiguration();
    emit preferencesChanged();
}

void AppState::setLaunchAtStartup(bool enabled)
{
    if (m_launchAtStartup == enabled)
    {
        return;
    }
    m_launchAtStartup = enabled;
    m_configuration.launchAtStartup = enabled;
    saveConfiguration();
    emit preferencesChanged();
}

void AppState::setNotifyScheduledBefore(bool enabled)
{
    if (m_notifyScheduledBefore == enabled)
    {
        return;
    }
    m_notifyScheduledBefore = enabled;
    m_configuration.notifyScheduledBefore = enabled;
    saveConfiguration();

    if (!enabled)
    {
        const QStringList pendingRootIds = m_pendingScheduledRoots.values();
        for (const QString &rootId : pendingRootIds)
        {
            startScheduledSnapshot(rootId);
        }
    }

    emit preferencesChanged();
}

void AppState::setRetention(int retention)
{
    if (m_retention == retention)
    {
        return;
    }
    try
    {
        foldersnap::validateRetention(retention);
        m_retention = retention;
        m_configuration.defaultRetention = retention;
        saveConfiguration();
        emit preferencesChanged();
    }
    catch (const foldersnap::DomainError &error)
    {
        setToast(error.message());
    }
}

QVariantList AppState::filteredSnapshots() const
{
    const QString query = m_snapshotSearch.trimmed().toLower();
    if (query.isEmpty())
    {
        return m_snapshots;
    }
    QVariantList filtered;
    for (const QVariant &value : m_snapshots)
    {
        const QVariantMap row = value.toMap();
        const QString searchable =
            row.value("fullDate").toString() + ' ' + row.value("description").toString() + ' ' +
            row.value("trigger").toString() + ' ' + row.value("size").toString() + ' ' +
            row.value("files").toString();
        if (searchable.toLower().contains(query))
        {
            filtered.append(row);
        }
    }
    return filtered;
}

QVariantList AppState::displayedChanges() const
{
    const QString query = m_search.trimmed().toLower();
    const bool searching = !query.isEmpty() || m_filter != "All changes";
    QSet<QString> matches;
    for (const QVariant &value : m_changes)
    {
        const QVariantMap row = value.toMap();
        const bool statusMatches = m_filter == "All changes" || row.value("status") == m_filter;
        const bool queryMatches =
            query.isEmpty() || row.value("path").toString().toLower().contains(query);
        if (statusMatches && queryMatches)
        {
            matches.insert(row.value("path").toString());
        }
    }
    QVariantList displayed;
    for (const QVariant &value : m_changes)
    {
        const QVariantMap row = value.toMap();
        const QString path = row.value("path").toString();
        bool visible = matches.contains(path);
        if (searching && !visible)
        {
            for (const QString &match : std::as_const(matches))
            {
                if (match.startsWith(path + '/'))
                {
                    visible = true;
                    break;
                }
            }
        }
        if (!searching)
        {
            visible = true;
            QString parent = path;
            while (parent.contains('/'))
            {
                parent = parent.left(parent.lastIndexOf('/'));
                if (!m_expanded.contains(parent))
                {
                    visible = false;
                    break;
                }
            }
        }
        if (visible)
        {
            displayed.append(row);
        }
    }
    return displayed;
}

bool AppState::payloadMissing() const
{
    if (m_detailId.isEmpty())
    {
        return false;
    }
    const QVariantMap row = snapshot(m_detailId);
    return row.contains("payloadAvailable") && !row.value("payloadAvailable").toBool();
}

QString AppState::netSize() const
{
    return formatSignedBytes(m_netSize);
}

QVariantList AppState::visibleCleanupCandidates() const
{
    const QString query = m_cleanupSearch.trimmed().toLower();
    if (query.isEmpty())
    {
        return m_cleanupRows;
    }

    QSet<QString> visiblePaths;
    for (const QVariant &value : m_cleanupRows)
    {
        const QString path = value.toMap().value("path").toString();
        if (!path.toLower().contains(query))
        {
            continue;
        }
        visiblePaths.insert(path);
        QString parent = path;
        while (parent.contains('/'))
        {
            parent = parent.left(parent.lastIndexOf('/'));
            visiblePaths.insert(parent);
        }
    }

    QVariantList visible;
    for (const QVariant &value : m_cleanupRows)
    {
        if (visiblePaths.contains(value.toMap().value("path").toString()))
        {
            visible.append(value);
        }
    }
    return visible;
}

QString AppState::cleanupSelectedSize() const
{
    return formatBytes(m_cleanupSelectedBytes);
}

QString AppState::cleanupSelectionState(const QString &path) const
{
    if (m_cleanupSelectedPaths.contains(path))
    {
        return "checked";
    }
    if (m_cleanupPartialPaths.contains(path))
    {
        return "partial";
    }
    return "unchecked";
}

void AppState::chooseRoot(int index)
{
    if (index < 0 || index >= m_configuration.roots.size() || index == m_rootIndex)
    {
        return;
    }
    m_rootIndex = index;
    clearSnapshotPair();
    m_snapshotSearch.clear();
    emit rootIndexChanged();
    emit snapshotSearchChanged();
    refreshModels();
    updateCurrentScanState();
}

void AppState::chooseSnapshot(const QString &snapshotId)
{
    const QVariantMap selected = snapshot(snapshotId);
    if (selected.isEmpty())
    {
        return;
    }
    if (!selected.value("payloadAvailable").toBool())
    {
        setToast("This snapshot's payload is unavailable.");
        return;
    }
    if (m_beforeId == snapshotId)
    {
        m_beforeId.clear();
    }
    else if (m_afterId == snapshotId)
    {
        m_afterId.clear();
    }
    else if (m_beforeId.isEmpty())
    {
        m_beforeId = snapshotId;
    }
    else if (m_afterId.isEmpty())
    {
        m_afterId = snapshotId;
    }
    else
    {
        m_beforeId = m_afterId;
        m_afterId = snapshotId;
    }
    if (hasPair())
    {
        if (snapshot(m_beforeId).value("completedNs").toLongLong() >
            snapshot(m_afterId).value("completedNs").toLongLong())
        {
            std::swap(m_beforeId, m_afterId);
        }
    }
    invalidateComparison();
    m_comparisonReady = false;
    m_changes.clear();
    rebuildCleanupCandidates();
    m_expanded.clear();
    emit snapshotPairChanged();
    emit comparisonChanged();
}

void AppState::clearSnapshotPair()
{
    if (!m_beforeId.isEmpty() || !m_afterId.isEmpty())
    {
        m_beforeId.clear();
        m_afterId.clear();
        emit snapshotPairChanged();
    }
    invalidateComparison();
    m_comparisonReady = false;
    m_changes.clear();
    rebuildCleanupCandidates();
    emit comparisonChanged();
}

void AppState::toggleExpanded(const QString &path)
{
    if (m_expanded.contains(path))
    {
        m_expanded.removeAll(path);
    }
    else
    {
        m_expanded.append(path);
    }
    emit comparisonChanged();
}

void AppState::takeSnapshot()
{
    const auto *root = currentConfigurationRoot();
    if (!root || root->archived)
    {
        return;
    }
    requestSnapshot(*root, foldersnap::SnapshotTrigger::Manual);
}

void AppState::startScheduledSnapshot(const QString &rootId)
{
    if (!m_pendingScheduledRoots.remove(rootId))
    {
        return;
    }
    auto *root = configurationRoot(rootId);
    if (!root || root->archived)
    {
        m_pendingScheduledNextDue.remove(rootId);
        return;
    }
    const auto nextDue = m_pendingScheduledNextDue.find(rootId);
    if (nextDue != m_pendingScheduledNextDue.end())
    {
        root->schedule.nextDueAtUtc = nextDue.value();
        m_pendingScheduledNextDue.erase(nextDue);
        saveConfiguration();
    }
    requestSnapshot(*root, foldersnap::SnapshotTrigger::Scheduled);
}

void AppState::snoozeScheduledSnapshot(const QString &rootId, int minutes)
{
    if ((minutes != 5 && minutes != 15 && minutes != 30) ||
        !m_pendingScheduledRoots.contains(rootId))
    {
        return;
    }
    const auto *root = configurationRoot(rootId);
    if (!root || root->archived)
    {
        m_pendingScheduledRoots.remove(rootId);
        m_pendingScheduledNextDue.remove(rootId);
        return;
    }
    constexpr qint64 kNanosecondsPerSecond = 1000000000;
    const qint64 nowNanoseconds = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() * 1000000;
    const qint64 delay = static_cast<qint64>(minutes) * 60 * kNanosecondsPerSecond;
    if (nowNanoseconds > std::numeric_limits<qint64>::max() - delay)
    {
        return;
    }
    m_pendingScheduledRoots.remove(rootId);
    m_snoozedScheduledRoots.insert(rootId, foldersnap::UtcTimestamp{nowNanoseconds + delay});
}

void AppState::requestSnapshot(const foldersnap::WatchedRoot &root,
                               foldersnap::SnapshotTrigger trigger)
{
    foldersnap::ScanRequest request;
    request.rootId = root.rootId;
    request.displayTitle = root.displayName;
    request.root = foldersnap::normalizeRootPath(root.path);
    request.ignoreRules = root.ignoreRules;
    request.trigger = trigger;
    try
    {
        request.protectedSubtree = foldersnap::protectedDataSubtree(
            request.root, foldersnap::normalizeRootPath(m_paths.dataDirectory));
    }
    catch (const foldersnap::DomainError &)
    {
        request.protectedSubtree.reset();
    }
    if (trigger == foldersnap::SnapshotTrigger::Scheduled && !m_notifyScheduledBefore)
    {
        emit scheduledSnapshotStarted(root.rootId);
    }
    if (const auto *current = currentConfigurationRoot(); current && current->rootId == root.rootId)
    {
        setScanError({});
        m_scanProgress = 0;
        emit scanProgressChanged();
    }
    foldersnap::ScanJobRequest job;
    job.scan = std::move(request);
    job.paths = m_paths;
    job.retention = root.retention;
    m_scanCoordinator->request(std::move(job));
}

void AppState::cancelScan()
{
    const auto *root = currentConfigurationRoot();
    if (root && m_scanCoordinator)
    {
        m_scanCoordinator->cancelRoot(root->rootId);
    }
}

void AppState::startComparison()
{
    if (!hasPair() || m_comparing)
    {
        return;
    }
    const quint64 generation = ++m_comparisonGeneration;
    setComparing(true);
    m_comparisonReady = false;
    emit comparisonChanged();
    const QString beforeId = m_beforeId;
    const QString afterId = m_afterId;
    const foldersnap::StoragePaths paths = m_paths;
    m_comparisonWatcher->setFuture(QtConcurrent::run(
        [paths, beforeId, afterId, generation](QPromise<ComparisonJobResult> &promise)
        {
            ComparisonJobResult result = compareSnapshots(paths, beforeId, afterId, [&promise]()
                                                          { return promise.isCanceled(); });
            result.beforeId = beforeId;
            result.afterId = afterId;
            result.generation = generation;
            if (!result.cancelled && !promise.isCanceled())
            {
                promise.addResult(result);
            }
        }));
}

void AppState::exportSnapshot(const QString &format, const QUrl &destination)
{
    if (m_detailId.isEmpty())
    {
        setExportError("Choose a snapshot to export.");
        return;
    }
    startExport(m_detailId, {}, format, destination);
}

void AppState::exportComparison(const QString &format, const QUrl &destination)
{
    if (!m_comparisonReady || !hasPair())
    {
        setExportError("Complete a comparison before exporting it.");
        return;
    }
    startExport(m_beforeId, m_afterId, format, destination);
}

void AppState::cancelExport()
{
    if (m_exportCoordinator)
    {
        m_exportCoordinator->cancel();
    }
}

void AppState::startExport(const QString &firstSnapshotId, const QString &secondSnapshotId,
                           const QString &format, const QUrl &destination)
{
    if (m_exporting)
    {
        return;
    }
    try
    {
        const std::optional<foldersnap::ExportFormat> parsedFormat = exportFormat(format);
        if (!parsedFormat)
        {
            throw foldersnap::DomainError(foldersnap::ErrorCode::InvalidData,
                                          "Choose HTML or CSV export format.");
        }

        foldersnap::ExportJobRequest request;
        request.paths = m_paths;
        request.firstSnapshotId = firstSnapshotId;
        request.secondSnapshotId = secondSnapshotId;
        request.destinationPath = exportPath(destination, *parsedFormat);
        request.format = *parsedFormat;
        if (*parsedFormat == foldersnap::ExportFormat::Html)
        {
            QFile templateFile(":/resources/snapshot-export-template.html");
            if (!templateFile.open(QIODevice::ReadOnly))
            {
                throw foldersnap::DomainError(foldersnap::ErrorCode::Io,
                                              "The packaged HTML report template is unavailable.");
            }
            request.htmlTemplate = templateFile.readAll();
        }
        setExportError({});
        m_exportCoordinator->start(std::move(request));
    }
    catch (const foldersnap::DomainError &error)
    {
        setExportError(error.message());
    }
}

void AppState::openSheet(const QString &kind)
{
    if (m_cleanupExecuting)
    {
        return;
    }
    if ((kind == "detail" || kind == "delete") && m_detailId.isEmpty() && !m_snapshots.isEmpty())
    {
        m_detailId = m_snapshots.first().toMap().value("id").toString();
        emit detailIdChanged();
    }
    const bool cleanupFilterChanged = !m_cleanupSearch.isEmpty();
    cancelCleanupPreflight();
    const bool cleanupRowsChanged = clearCleanupPreflightFields(m_cleanupRows);
    const bool cleanupCandidateRowsChanged = clearCleanupPreflightFields(m_cleanupCandidates);
    m_cleanupSelection.clear();
    m_cleanupSelectedPaths.clear();
    m_cleanupPartialPaths.clear();
    m_cleanupSelectedBytes = 0;
    m_cleanupSearch.clear();
    m_cleanupReviewed = false;
    m_cleanupResult.clear();
    m_cleanupReadyCount = 0;
    m_cleanupBlockedCount = 0;
    m_cleanupAlreadyMissingCount = 0;
    m_cleanupCompleted = false;
    m_cleanupMovedCount = 0;
    m_cleanupFailedCount = 0;
    if (cleanupFilterChanged || cleanupRowsChanged || cleanupCandidateRowsChanged)
    {
        emit cleanupCandidatesChanged();
    }
    emit cleanupChanged();
    if (kind == "export" || kind == "exportComparison")
    {
        setExportError({});
    }
    setSheet(kind);
}

void AppState::openCurrentFolder()
{
    const auto *root = currentConfigurationRoot();
    if (!root)
    {
        return;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(root->path)))
    {
        setToast("Could not open this folder.");
    }
}

void AppState::addFolder(const QUrl &folderUrl)
{
    try
    {
        const QString path = folderUrl.toLocalFile();
        const foldersnap::RootPath normalized = foldersnap::normalizeRootPath(path.trimmed());
        const QFileInfo info(normalized.displayPath);
        if (!info.exists() || !info.isDir())
        {
            throw foldersnap::DomainError(foldersnap::ErrorCode::InvalidPath,
                                          "Choose an existing folder.");
        }
        const auto dataDirectory = foldersnap::normalizeRootPath(m_paths.dataDirectory);
        const auto protectedSubtree = foldersnap::protectedDataSubtree(normalized, dataDirectory);
        Q_UNUSED(protectedSubtree);
        QString displayName = info.fileName();
        if (displayName.isEmpty())
        {
            displayName = QDir(normalized.displayPath).dirName();
        }
        if (displayName.isEmpty())
        {
            displayName = normalized.displayPath;
        }
        for (const auto &root : m_configuration.roots)
        {
            if (root.normalizedPath == normalized.identityPath)
            {
                throw foldersnap::DomainError(foldersnap::ErrorCode::InvalidData,
                                              "That folder is already being watched.");
            }
        }
        foldersnap::WatchedRoot root;
        root.rootId = foldersnap::createId();
        root.displayName = displayName;
        root.path = normalized.displayPath;
        root.normalizedPath = normalized.identityPath;
        root.ignoreRules = m_configuration.defaultIgnoreRules;
        root.retention = m_configuration.defaultRetention;
        m_configuration.roots.append(root);
        foldersnap::validateConfiguration(m_configuration);
        saveConfiguration();
        m_rootIndex = m_configuration.roots.size() - 1;
        emit rootIndexChanged();
        refreshModels();
        setSheet({});
        setSelectedSection(Section::Folders);
        setToast("Folder added. Take a snapshot to build its first history.");
    }
    catch (const foldersnap::DomainError &error)
    {
        setToast(error.message());
    }
}

void AppState::updateRoot(const QString &name, const QString &schedule, int retention,
                          const QString &ignoreRules, bool archived)
{
    const auto *root = currentConfigurationRoot();
    if (!root)
    {
        return;
    }
    try
    {
        const QString rootId = root->rootId;
        m_pendingScheduledRoots.remove(rootId);
        m_pendingScheduledNextDue.remove(rootId);
        m_snoozedScheduledRoots.remove(rootId);
        foldersnap::Configuration updatedConfiguration = m_configuration;
        auto updatedRoot = std::find_if(
            updatedConfiguration.roots.begin(), updatedConfiguration.roots.end(),
            [&rootId](const foldersnap::WatchedRoot &item) { return item.rootId == rootId; });
        if (updatedRoot == updatedConfiguration.roots.end())
        {
            return;
        }
        foldersnap::Schedule updatedSchedule = parseSchedule(schedule);
        foldersnap::Schedule comparableSchedule = updatedRoot->schedule;
        comparableSchedule.nextDueAtUtc.reset();
        if (updatedSchedule == comparableSchedule)
        {
            updatedSchedule.nextDueAtUtc = updatedRoot->schedule.nextDueAtUtc;
        }
        const bool becameArchived = !updatedRoot->archived && archived;
        updatedRoot->displayName = name.trimmed();
        updatedRoot->schedule = updatedSchedule;
        updatedRoot->retention = retention;
        updatedRoot->ignoreRules = ignoreRules.split('\n', Qt::SkipEmptyParts);
        updatedRoot->archived = archived;
        foldersnap::validateConfiguration(updatedConfiguration);
        foldersnap::ConfigurationStore(m_paths).saveConfiguration(updatedConfiguration);
        m_configuration = std::move(updatedConfiguration);
        emit configurationChanged();
        if (becameArchived)
        {
            m_scanCoordinator->cancelRoot(rootId);
        }
        refreshModels();
        evaluateSchedules();
        setToast("Folder preferences saved.");
    }
    catch (const foldersnap::DomainError &error)
    {
        setToast(error.message());
    }
}

void AppState::toggleCleanup(const QString &path)
{
    const QVariantList &candidates = m_cleanupCandidates;
    bool isCandidate = false;
    for (const QVariant &candidate : candidates)
    {
        if (candidate.toMap().value("path").toString() == path)
        {
            isCandidate = true;
            break;
        }
    }
    if (!isCandidate)
    {
        return;
    }

    QSet<QString> selectedPaths = m_cleanupSelectedPaths;
    const bool shouldRemove = selectedPaths.contains(path);
    for (const QVariant &candidate : candidates)
    {
        const QString candidatePath = candidate.toMap().value("path").toString();
        if (candidatePath == path || candidatePath.startsWith(path + '/'))
        {
            if (shouldRemove)
            {
                selectedPaths.remove(candidatePath);
            }
            else
            {
                selectedPaths.insert(candidatePath);
            }
        }
    }
    QVariantList selection;
    for (const QString &selectedPath : std::as_const(selectedPaths))
    {
        selection.append(selectedPath);
    }
    setCleanupSelection(selection);
}

void AppState::saveDescription(const QString &description)
{
    if (m_detailId.isEmpty())
    {
        return;
    }
    try
    {
        foldersnap::HistoryStore(m_paths).updateDescription(m_detailId, description);
        refreshModels();
        setToast("Description saved.");
    }
    catch (const foldersnap::DomainError &error)
    {
        setToast(error.message());
    }
}

void AppState::deleteSelectedSnapshot()
{
    if (m_detailId.isEmpty())
    {
        return;
    }
    try
    {
        foldersnap::HistoryStore(m_paths).deleteSnapshot(m_detailId);
        clearSnapshotPair();
        refreshModels();
        setSheet({});
        setToast("Snapshot deleted.");
    }
    catch (const foldersnap::DomainError &error)
    {
        setToast(error.message());
    }
}

void AppState::clearSelectedRootHistory()
{
    const auto *root = currentConfigurationRoot();
    if (!root)
    {
        return;
    }
    try
    {
        foldersnap::HistoryStore(m_paths).clearRootHistory(root->rootId);
        clearSnapshotPair();
        refreshModels();
        setSheet({});
        setToast("Folder history cleared.");
    }
    catch (const foldersnap::DomainError &error)
    {
        setToast(error.message());
    }
}

void AppState::removeCurrentRoot()
{
    const auto *root = currentConfigurationRoot();
    if (!root)
    {
        return;
    }
    const QString rootId = root->rootId;
    if (m_scanCoordinator->isActive(rootId))
    {
        setToast("Cancel the active snapshot before removing this folder.");
        return;
    }
    try
    {
        m_scanCoordinator->cancelRoot(rootId);
        m_pendingScheduledRoots.remove(rootId);
        m_pendingScheduledNextDue.remove(rootId);
        m_snoozedScheduledRoots.remove(rootId);
        foldersnap::HistoryStore(m_paths).removeWatchedRoot(rootId);
        m_configuration.roots.erase(std::remove_if(m_configuration.roots.begin(),
                                                   m_configuration.roots.end(),
                                                   [&rootId](const foldersnap::WatchedRoot &item)
                                                   { return item.rootId == rootId; }),
                                    m_configuration.roots.end());
        m_rootIndex = std::min(m_rootIndex, static_cast<int>(m_configuration.roots.size()) - 1);
        if (m_rootIndex < 0)
        {
            m_rootIndex = 0;
        }
        clearSnapshotPair();
        m_detailId.clear();
        emit detailIdChanged();
        emit rootIndexChanged();
        emit configurationChanged();
        refreshModels();
        updateCurrentScanState();
        setSheet({});
        setToast("Watched folder and its snapshot history removed.");
    }
    catch (const foldersnap::DomainError &error)
    {
        setToast(error.message());
    }
}

QVariantMap AppState::snapshot(const QString &snapshotId) const
{
    for (const QVariant &value : m_snapshots)
    {
        const QVariantMap row = value.toMap();
        if (row.value("id").toString() == snapshotId)
        {
            return row;
        }
    }
    return {};
}

void AppState::refreshModels()
{
    QList<HistoryRecord> history;
    try
    {
        history = foldersnap::HistoryStore(m_paths).loadHistory();
    }
    catch (const foldersnap::DomainError &error)
    {
        setScanError(error.message());
    }
    m_roots.clear();
    m_activeRootCount = 0;
    m_totalSnapshotCount = history.size();
    m_totalFileCount = 0;
    for (int index = 0; index < m_configuration.roots.size(); ++index)
    {
        const auto &root = m_configuration.roots[index];
        QList<HistoryRecord> rootHistory;
        for (const auto &record : history)
        {
            if (record.rootId == root.rootId)
            {
                rootHistory.append(record);
            }
        }
        if (!root.archived)
        {
            ++m_activeRootCount;
        }
        if (!rootHistory.isEmpty())
        {
            m_totalFileCount += rootHistory.first().fileCount;
        }
        m_roots.append(mapForRoot(root, rootHistory, index));
    }
    if (m_rootIndex >= m_configuration.roots.size())
    {
        m_rootIndex = std::max(0, static_cast<int>(m_configuration.roots.size()) - 1);
        emit rootIndexChanged();
    }
    m_snapshots.clear();
    m_currentRoot.clear();
    m_ignoreRules.clear();
    if (const auto *root = currentConfigurationRoot())
    {
        m_currentRoot = m_roots.value(m_rootIndex).toMap();
        m_ignoreRules = root->ignoreRules.join('\n');
        for (const auto &record : history)
        {
            if (record.rootId == root->rootId)
            {
                m_snapshots.append(mapForRecord(record));
            }
        }
    }
    emit rootsChanged();
    emit currentRootChanged();
    emit snapshotsChanged();
    emit ignoreRulesChanged();
}

void AppState::finishScan(const foldersnap::ScanJobResult &result)
{
    if (auto *root = configurationRoot(result.rootId))
    {
        root->lastSnapshotUtc = result.commit.record.completedAtUtc;
        root->lastScanError.clear();
        saveConfiguration();
    }
    const auto *current = currentConfigurationRoot();
    const bool isCurrent = current && current->rootId == result.rootId;
    if (isCurrent)
    {
        m_scanProgress = 100;
        emit scanProgressChanged();
        setScanError({});
    }
    refreshModels();
    if (isCurrent && result.trigger == foldersnap::SnapshotTrigger::Manual)
    {
        setToast(QString("Snapshot saved · %1 files · %2")
                     .arg(formatCount(result.commit.record.fileCount),
                          formatBytes(result.commit.record.totalFileBytes)));
    }
    emit scanCompleted(result.rootId, result.commit.record.snapshotId,
                       result.commit.record.warningCount);
}

void AppState::failScan(const QString &rootId, const QString &error)
{
    if (auto *root = configurationRoot(rootId))
    {
        root->lastScanError = error;
        saveConfiguration();
    }
    const auto *current = currentConfigurationRoot();
    if (current && current->rootId == rootId)
    {
        setScanError(error);
    }
    emit scanFailed(rootId, error);
}

void AppState::updateCurrentScanState()
{
    const auto *root = currentConfigurationRoot();
    setScanning(root && m_scanCoordinator && m_scanCoordinator->isActive(root->rootId));
}

void AppState::evaluateSchedules()
{
    const foldersnap::UtcTimestamp now{QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() *
                                       1000000};
    const QTimeZone timeZone = QTimeZone::systemTimeZone();
    QStringList notificationRootIds;
    bool configurationChanged = false;
    for (foldersnap::WatchedRoot &root : m_configuration.roots)
    {
        if (root.archived || root.schedule.kind == foldersnap::ScheduleKind::Manual)
        {
            m_pendingScheduledRoots.remove(root.rootId);
            m_pendingScheduledNextDue.remove(root.rootId);
            m_snoozedScheduledRoots.remove(root.rootId);
            continue;
        }
        auto snoozed = m_snoozedScheduledRoots.find(root.rootId);
        if (snoozed != m_snoozedScheduledRoots.end())
        {
            if (snoozed.value().nanoseconds > now.nanoseconds)
            {
                continue;
            }
            m_snoozedScheduledRoots.erase(snoozed);
            if (!m_pendingScheduledRoots.contains(root.rootId))
            {
                m_pendingScheduledRoots.insert(root.rootId);
                notificationRootIds.append(root.rootId);
            }
            continue;
        }
        try
        {
            const foldersnap::ScheduleDecision decision =
                foldersnap::ScheduleCalculator::evaluate(root.schedule, now, timeZone);
            if (decision.shouldRun)
            {
                if (!m_notifyScheduledBefore)
                {
                    if (decision.nextDueAtUtc)
                    {
                        root.schedule.nextDueAtUtc = decision.nextDueAtUtc;
                        configurationChanged = true;
                    }
                    requestSnapshot(root, foldersnap::SnapshotTrigger::Scheduled);
                }
                else if (!m_pendingScheduledRoots.contains(root.rootId))
                {
                    m_pendingScheduledRoots.insert(root.rootId);
                    if (decision.nextDueAtUtc)
                    {
                        m_pendingScheduledNextDue.insert(root.rootId, *decision.nextDueAtUtc);
                    }
                    notificationRootIds.append(root.rootId);
                }
                continue;
            }
            if (root.schedule.nextDueAtUtc != decision.nextDueAtUtc)
            {
                root.schedule.nextDueAtUtc = decision.nextDueAtUtc;
                configurationChanged = true;
            }
        }
        catch (const foldersnap::DomainError &error)
        {
            if (root.lastScanError != error.message())
            {
                root.lastScanError = error.message();
                configurationChanged = true;
            }
        }
    }
    if (configurationChanged)
    {
        saveConfiguration();
    }
    for (const QString &rootId : notificationRootIds)
    {
        const auto *root = configurationRoot(rootId);
        if (root && !root->archived)
        {
            emit scheduledSnapshotDue(root->rootId, root->displayName);
        }
    }
}

void AppState::finishComparison()
{
    if (m_comparisonWatcher->future().isCanceled())
    {
        return;
    }
    const ComparisonJobResult result = m_comparisonWatcher->result();
    if (result.generation != m_comparisonGeneration || result.beforeId != m_beforeId ||
        result.afterId != m_afterId)
    {
        return;
    }
    setComparing(false);
    if (!result.error.isEmpty())
    {
        setScanError(result.error);
        m_comparisonReady = false;
        emit comparisonChanged();
        return;
    }
    m_changes = result.changes;
    m_addedCount = result.addedCount;
    m_removedCount = result.removedCount;
    m_modifiedCount = result.modifiedCount;
    m_unchangedCount = result.unchangedCount;
    m_warningCount = result.warningCount;
    m_comparedCount = result.comparedCount;
    m_netSize = result.netSize;
    rebuildCleanupCandidates();
    m_expanded.clear();
    for (const QVariant &value : m_changes)
    {
        const QVariantMap row = value.toMap();
        if (row.value("folder").toBool())
        {
            m_expanded.append(row.value("path").toString());
        }
    }
    m_comparisonReady = true;
    emit comparisonChanged();
}

void AppState::invalidateComparison()
{
    ++m_comparisonGeneration;
    if (m_comparisonWatcher && m_comparisonWatcher->isRunning())
    {
        m_comparisonWatcher->cancel();
    }
    setComparing(false);
}

void AppState::rebuildCleanupCandidates()
{
    cancelCleanupPreflight();
    m_cleanupCandidates.clear();
    m_cleanupRows.clear();
    m_cleanupCandidates.reserve(m_addedCount);
    m_cleanupRows.reserve(m_changes.size());

    QSet<QString> contextPaths;
    for (const QVariant &value : m_changes)
    {
        const QVariantMap row = value.toMap();
        if (row.value("status").toString() != "Added")
        {
            continue;
        }
        QString parent = row.value("path").toString();
        while (parent.contains('/'))
        {
            parent = parent.left(parent.lastIndexOf('/'));
            contextPaths.insert(parent);
        }
    }

    for (const QVariant &value : m_changes)
    {
        QVariantMap row = value.toMap();
        const QString path = row.value("path").toString();
        const bool selectable = row.value("status").toString() == "Added";
        if (!selectable && !contextPaths.contains(path))
        {
            continue;
        }
        row["cleanupSelectable"] = selectable;
        m_cleanupRows.append(row);
        if (selectable)
        {
            m_cleanupCandidates.append(row);
        }
    }
    m_cleanupSelection.clear();
    m_cleanupSelectedPaths.clear();
    m_cleanupPartialPaths.clear();
    m_cleanupSelectedBytes = 0;
    m_cleanupSearch.clear();
    m_cleanupReviewed = false;
    m_cleanupResult.clear();
    m_cleanupReadyCount = 0;
    m_cleanupBlockedCount = 0;
    m_cleanupAlreadyMissingCount = 0;
    m_cleanupCompleted = false;
    m_cleanupMovedCount = 0;
    m_cleanupFailedCount = 0;
    emit cleanupCandidatesChanged();
    emit cleanupChanged();
}

void AppState::saveConfiguration()
{
    try
    {
        foldersnap::validateConfiguration(m_configuration);
        foldersnap::ConfigurationStore(m_paths).saveConfiguration(m_configuration);
        emit configurationChanged();
    }
    catch (const foldersnap::DomainError &error)
    {
        setToast(error.message());
    }
}

void AppState::setScanError(const QString &error)
{
    if (m_scanError == error)
    {
        return;
    }
    m_scanError = error;
    emit scanErrorChanged();
}

void AppState::setScanning(bool scanning)
{
    if (m_scanning == scanning)
    {
        return;
    }
    m_scanning = scanning;
    emit scanningChanged();
}

void AppState::setComparing(bool comparing)
{
    if (m_comparing == comparing)
    {
        return;
    }
    m_comparing = comparing;
    emit comparingChanged();
}

void AppState::setExportError(const QString &error)
{
    if (m_exportError == error)
    {
        return;
    }
    m_exportError = error;
    emit exportChanged();
}

foldersnap::WatchedRoot *AppState::currentConfigurationRoot()
{
    if (m_rootIndex < 0 || m_rootIndex >= m_configuration.roots.size())
    {
        return nullptr;
    }
    return &m_configuration.roots[m_rootIndex];
}

const foldersnap::WatchedRoot *AppState::currentConfigurationRoot() const
{
    if (m_rootIndex < 0 || m_rootIndex >= m_configuration.roots.size())
    {
        return nullptr;
    }
    return &m_configuration.roots[m_rootIndex];
}

foldersnap::WatchedRoot *AppState::configurationRoot(const QString &rootId)
{
    for (foldersnap::WatchedRoot &root : m_configuration.roots)
    {
        if (root.rootId == rootId)
        {
            return &root;
        }
    }
    return nullptr;
}
