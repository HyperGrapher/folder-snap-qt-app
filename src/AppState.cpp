#include "AppState.h"

#include <algorithm>
#include <limits>
#include <utility>

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QPromise>
#include <QSet>
#include <QTimeZone>
#include <QUrl>
#include <QtConcurrent>

#include "domain/DomainError.h"
#include "storage/ConfigurationStore.h"
#include "storage/HistoryStore.h"
#include "storage/SnapshotStore.h"

namespace
{
using foldersnap::EntryType;
using foldersnap::HistoryRecord;
using foldersnap::Snapshot;
using foldersnap::SnapshotEntry;
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

QString triggerName(foldersnap::SnapshotTrigger trigger)
{
    return trigger == foldersnap::SnapshotTrigger::Scheduled ? "Scheduled" : "Manual";
}

QString scheduleName(const foldersnap::Schedule &schedule)
{
    switch (schedule.kind)
    {
    case foldersnap::ScheduleKind::Interval:
        return QString("Every %1 hours").arg(schedule.intervalHours);
    case foldersnap::ScheduleKind::Daily:
        return QString("Daily at %1:%2")
            .arg(schedule.hour, 2, 10, QChar('0'))
            .arg(schedule.minute, 2, 10, QChar('0'));
    case foldersnap::ScheduleKind::Weekly:
        return QString("Weekly at %1:%2")
            .arg(schedule.hour, 2, 10, QChar('0'))
            .arg(schedule.minute, 2, 10, QChar('0'));
    case foldersnap::ScheduleKind::Monthly:
        return QString("Monthly · day %1").arg(schedule.dayOfMonth);
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
        schedule.weekday = 0;
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

bool sameMetadata(const SnapshotEntry &left, const SnapshotEntry &right)
{
    return left.type == right.type && left.size == right.size &&
           left.modifiedNs == right.modifiedNs && left.createdNs == right.createdNs &&
           left.attributes == right.attributes && left.linkTarget == right.linkTarget;
}

QHash<QString, qint64> recursiveSizes(const Snapshot &snapshot)
{
    QHash<QString, qint64> sizes;
    for (const SnapshotEntry &entry : snapshot.entries)
    {
        if (entry.type != EntryType::File)
        {
            continue;
        }
        sizes[entry.path] += entry.size;
        QString parent = entry.path;
        while (parent.contains('/'))
        {
            parent = parent.left(parent.lastIndexOf('/'));
            sizes[parent] += entry.size;
        }
    }
    return sizes;
}

QString entrySize(const SnapshotEntry *entry, const QHash<QString, qint64> &sizes,
                  const QString &path)
{
    if (!entry)
    {
        return "—";
    }
    if (entry->type == EntryType::File)
    {
        return formatBytes(entry->size);
    }
    if (entry->type == EntryType::Directory)
    {
        return formatBytes(sizes.value(path));
    }
    return "—";
}

ComparisonJobResult compareSnapshots(const foldersnap::StoragePaths &paths, const QString &beforeId,
                                     const QString &afterId)
{
    ComparisonJobResult result;
    try
    {
        const foldersnap::SnapshotStore store(paths);
        const Snapshot before = store.loadSnapshot(beforeId);
        const Snapshot after = store.loadSnapshot(afterId);
        if (before.header.rootId != after.header.rootId)
        {
            result.error = "Snapshots from different folders cannot be compared.";
            return result;
        }

        QHash<QString, SnapshotEntry> beforeEntries;
        QHash<QString, SnapshotEntry> afterEntries;
        for (const SnapshotEntry &entry : before.entries)
        {
            beforeEntries.insert(entry.path, entry);
        }
        for (const SnapshotEntry &entry : after.entries)
        {
            afterEntries.insert(entry.path, entry);
        }
        QStringList allPaths = beforeEntries.keys();
        allPaths.append(afterEntries.keys());
        allPaths.removeDuplicates();
        std::sort(allPaths.begin(), allPaths.end());
        result.comparedCount = allPaths.size();
        const auto beforeSizes = recursiveSizes(before);
        const auto afterSizes = recursiveSizes(after);
        QSet<QString> changedPaths;
        QSet<QString> folderPaths;

        for (const QString &path : allPaths)
        {
            const SnapshotEntry *left =
                beforeEntries.contains(path) ? &beforeEntries[path] : nullptr;
            const SnapshotEntry *right =
                afterEntries.contains(path) ? &afterEntries[path] : nullptr;
            if (left && right && sameMetadata(*left, *right))
            {
                ++result.unchangedCount;
                continue;
            }
            changedPaths.insert(path);
            if (!left)
            {
                ++result.addedCount;
            }
            else if (!right)
            {
                ++result.removedCount;
            }
            else
            {
                ++result.modifiedCount;
            }
            if ((left && left->type == EntryType::Directory) ||
                (right && right->type == EntryType::Directory))
            {
                folderPaths.insert(path);
            }
        }

        for (const QString &path : changedPaths)
        {
            QString parent = path;
            while (parent.contains('/'))
            {
                parent = parent.left(parent.lastIndexOf('/'));
                if (beforeEntries.value(parent).type == EntryType::Directory ||
                    afterEntries.value(parent).type == EntryType::Directory)
                {
                    folderPaths.insert(parent);
                }
            }
        }
        for (const QString &path : folderPaths)
        {
            changedPaths.insert(path);
        }

        QStringList rows = changedPaths.values();
        std::sort(rows.begin(), rows.end());
        for (const QString &path : rows)
        {
            const SnapshotEntry *left =
                beforeEntries.contains(path) ? &beforeEntries[path] : nullptr;
            const SnapshotEntry *right =
                afterEntries.contains(path) ? &afterEntries[path] : nullptr;
            const bool folder = folderPaths.contains(path);
            QVariantMap row;
            row["path"] = path;
            row["name"] = path.section('/', -1);
            row["depth"] = path.count('/');
            row["folder"] = folder;
            row["status"] = folder ? "" : !left ? "Added" : !right ? "Removed" : "Modified";
            row["before"] = entrySize(left, beforeSizes, path);
            row["after"] = entrySize(right, afterSizes, path);
            result.changes.append(row);
        }
        result.warningCount = before.header.scanWarnings.size() + after.header.scanWarnings.size();
        result.netSize = after.header.totalFileBytes - before.header.totalFileBytes;
    }
    catch (const foldersnap::DomainError &error)
    {
        result.error = error.message();
    }
    return result;
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
    m_notifyScheduledSuccess = m_configuration.notifyScheduledSuccess;
    m_retention = m_configuration.defaultRetention;
    m_scanWatcher = std::make_unique<QFutureWatcher<ScanJobResult>>(this);
    m_comparisonWatcher = std::make_unique<QFutureWatcher<ComparisonJobResult>>(this);
    connect(m_scanWatcher.get(), &QFutureWatcher<ScanJobResult>::progressValueChanged, this,
            [this](int value)
            {
                if (m_scanProgress == value)
                {
                    return;
                }
                m_scanProgress = value;
                emit scanProgressChanged();
            });
    connect(m_scanWatcher.get(), &QFutureWatcher<ScanJobResult>::finished, this,
            &AppState::finishScan);
    connect(m_comparisonWatcher.get(), &QFutureWatcher<ComparisonJobResult>::finished, this,
            &AppState::finishComparison);
    refreshModels();
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

void AppState::setIgnoreRules(const QString &rules)
{
    if (m_ignoreRules == rules)
    {
        return;
    }
    m_ignoreRules = rules;
    emit ignoreRulesChanged();
    auto *root = currentConfigurationRoot();
    if (!root)
    {
        return;
    }
    root->ignoreRules = rules.split('\n', Qt::SkipEmptyParts);
    saveConfiguration();
}

void AppState::setCleanupSelection(const QVariantList &selection)
{
    m_cleanupSelection = selection;
    emit cleanupChanged();
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

void AppState::setNotifyScheduledSuccess(bool enabled)
{
    if (m_notifyScheduledSuccess == enabled)
    {
        return;
    }
    m_notifyScheduledSuccess = enabled;
    m_configuration.notifyScheduledSuccess = enabled;
    saveConfiguration();
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

QVariantList AppState::cleanupCandidates() const
{
    QVariantList result;
    for (const QVariant &value : m_changes)
    {
        if (value.toMap().value("status") == "Added")
        {
            result.append(value);
        }
    }
    return result;
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
    m_comparisonReady = false;
    m_changes.clear();
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
    m_comparisonReady = false;
    m_changes.clear();
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
    if (!root || root->archived || m_scanning)
    {
        return;
    }
    foldersnap::ScanRequest request;
    request.rootId = root->rootId;
    request.displayTitle = root->displayName;
    request.root = foldersnap::normalizeRootPath(root->path);
    request.ignoreRules = root->ignoreRules;
    request.trigger = foldersnap::SnapshotTrigger::Manual;
    try
    {
        request.protectedSubtree = foldersnap::protectedDataSubtree(
            request.root, foldersnap::normalizeRootPath(m_paths.dataDirectory));
    }
    catch (const foldersnap::DomainError &)
    {
        request.protectedSubtree.reset();
    }
    setScanError({});
    m_scanProgress = 0;
    emit scanProgressChanged();
    setScanning(true);
    const foldersnap::StoragePaths paths = m_paths;
    const int retention = root->retention;
    m_scanWatcher->setFuture(QtConcurrent::run(
        [request, paths, retention](QPromise<ScanJobResult> &promise)
        {
            ScanJobResult result;
            promise.setProgressRange(0, 100);
            const foldersnap::ScanResult scan = foldersnap::MetadataScanner::scan(
                request, [&promise](int value) { promise.setProgressValue(value); },
                [&promise]() { return promise.isCanceled(); });
            if (scan.cancelled || promise.isCanceled())
            {
                result.cancelled = true;
                promise.addResult(result);
                return;
            }
            if (!scan.error.isEmpty())
            {
                result.error = scan.error;
                promise.addResult(result);
                return;
            }
            try
            {
                result.commit =
                    foldersnap::HistoryStore(paths).commitSnapshot(scan.snapshot, retention);
                promise.setProgressValue(100);
            }
            catch (const foldersnap::DomainError &error)
            {
                result.error = error.message();
            }
            promise.addResult(result);
        }));
}

void AppState::cancelScan()
{
    if (m_scanning && m_scanWatcher)
    {
        m_scanWatcher->cancel();
    }
}

void AppState::startComparison()
{
    if (!hasPair() || m_comparing)
    {
        return;
    }
    setComparing(true);
    m_comparisonReady = false;
    emit comparisonChanged();
    const QString beforeId = m_beforeId;
    const QString afterId = m_afterId;
    const foldersnap::StoragePaths paths = m_paths;
    m_comparisonWatcher->setFuture(QtConcurrent::run(
        [paths, beforeId, afterId]() { return compareSnapshots(paths, beforeId, afterId); }));
}

void AppState::openSheet(const QString &kind)
{
    if ((kind == "detail" || kind == "delete") && m_detailId.isEmpty() && !m_snapshots.isEmpty())
    {
        m_detailId = m_snapshots.first().toMap().value("id").toString();
        emit detailIdChanged();
    }
    m_cleanupSelection.clear();
    m_cleanupReviewed = false;
    m_cleanupResult.clear();
    emit cleanupChanged();
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

void AppState::updateRoot(const QString &name, const QString &schedule, bool archived)
{
    auto *root = currentConfigurationRoot();
    if (!root)
    {
        return;
    }
    try
    {
        root->displayName = name.trimmed();
        root->schedule = parseSchedule(schedule);
        root->archived = archived;
        foldersnap::validateConfiguration(m_configuration);
        saveConfiguration();
        refreshModels();
        setToast("Folder preferences saved.");
    }
    catch (const foldersnap::DomainError &error)
    {
        setToast(error.message());
    }
}

void AppState::toggleCleanup(const QString &path)
{
    QVariantList selection = m_cleanupSelection;
    for (int index = 0; index < selection.size(); ++index)
    {
        if (selection[index].toString() == path)
        {
            selection.removeAt(index);
            setCleanupSelection(selection);
            return;
        }
    }
    selection.append(path);
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

void AppState::finishScan()
{
    const ScanJobResult result = m_scanWatcher->result();
    setScanning(false);
    if (result.cancelled)
    {
        setToast("Snapshot cancelled. Your folder was not changed.");
        return;
    }
    if (!result.error.isEmpty())
    {
        if (auto *root = currentConfigurationRoot())
        {
            root->lastScanError = result.error;
            saveConfiguration();
        }
        setScanError(result.error);
        return;
    }
    m_scanProgress = 100;
    emit scanProgressChanged();
    if (auto *root = currentConfigurationRoot())
    {
        root->lastSnapshotUtc = result.commit.record.completedAtUtc;
        root->lastScanError.clear();
        saveConfiguration();
    }
    refreshModels();
    setToast(QString("Snapshot saved · %1 files · %2")
                 .arg(formatCount(result.commit.record.fileCount),
                      formatBytes(result.commit.record.totalFileBytes)));
}

void AppState::finishComparison()
{
    const ComparisonJobResult result = m_comparisonWatcher->result();
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

void AppState::saveConfiguration()
{
    try
    {
        foldersnap::validateConfiguration(m_configuration);
        foldersnap::ConfigurationStore(m_paths).saveConfiguration(m_configuration);
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
