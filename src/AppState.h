#pragma once

#include <memory>

#include <QFutureWatcher>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include "application/ExportCoordinator.h"
#include "application/ScanCoordinator.h"
#include "domain/Configuration.h"
#include "domain/Snapshot.h"
#include "storage/StoragePaths.h"

struct ComparisonJobResult
{
    QString beforeId;
    QString afterId;
    quint64 generation{0};
    QVariantList changes;
    int addedCount{0};
    int removedCount{0};
    int modifiedCount{0};
    int unchangedCount{0};
    int warningCount{0};
    int comparedCount{0};
    qint64 netSize{0};
    QString error;
    bool cancelled{false};
};

class AppState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Section selectedSection READ selectedSection WRITE setSelectedSection NOTIFY
                   selectedSectionChanged)
    Q_PROPERTY(
        bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged)
    Q_PROPERTY(bool backgroundMotionEnabled READ backgroundMotionEnabled WRITE
                   setBackgroundMotionEnabled NOTIFY backgroundMotionEnabledChanged)
    Q_PROPERTY(QVariantList roots READ roots NOTIFY rootsChanged)
    Q_PROPERTY(QVariantList visibleRoots READ roots NOTIFY rootsChanged)
    Q_PROPERTY(QVariantMap currentRoot READ currentRoot NOTIFY currentRootChanged)
    Q_PROPERTY(int rootIndex READ rootIndex WRITE chooseRoot NOTIFY rootIndexChanged)
    Q_PROPERTY(QVariantList snapshots READ snapshots NOTIFY snapshotsChanged)
    Q_PROPERTY(QVariantList availableSnapshots READ snapshots NOTIFY snapshotsChanged)
    Q_PROPERTY(QVariantList filteredSnapshots READ filteredSnapshots NOTIFY snapshotsChanged)
    Q_PROPERTY(QString snapshotSearch READ snapshotSearch WRITE setSnapshotSearch NOTIFY
                   snapshotSearchChanged)
    Q_PROPERTY(QString beforeId READ beforeId NOTIFY snapshotPairChanged)
    Q_PROPERTY(QString afterId READ afterId NOTIFY snapshotPairChanged)
    Q_PROPERTY(bool hasPair READ hasPair NOTIFY snapshotPairChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(int scanProgress READ scanProgress NOTIFY scanProgressChanged)
    Q_PROPERTY(QString scanError READ scanError NOTIFY scanErrorChanged)
    Q_PROPERTY(bool comparing READ comparing NOTIFY comparingChanged)
    Q_PROPERTY(bool exporting READ exporting NOTIFY exportChanged)
    Q_PROPERTY(QString exportError READ exportError NOTIFY exportChanged)
    Q_PROPERTY(bool comparisonReady READ comparisonReady NOTIFY comparisonChanged)
    Q_PROPERTY(QVariantList changes READ changes NOTIFY comparisonChanged)
    Q_PROPERTY(QVariantList displayedChanges READ displayedChanges NOTIFY comparisonChanged)
    Q_PROPERTY(QStringList expanded READ expanded WRITE setExpanded NOTIFY comparisonChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY comparisonChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY comparisonChanged)
    Q_PROPERTY(bool hasWarnings READ hasWarnings NOTIFY comparisonChanged)
    Q_PROPERTY(bool payloadMissing READ payloadMissing NOTIFY snapshotsChanged)
    Q_PROPERTY(int addedCount READ addedCount NOTIFY comparisonChanged)
    Q_PROPERTY(int removedCount READ removedCount NOTIFY comparisonChanged)
    Q_PROPERTY(int modifiedCount READ modifiedCount NOTIFY comparisonChanged)
    Q_PROPERTY(int unchangedCount READ unchangedCount NOTIFY comparisonChanged)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY comparisonChanged)
    Q_PROPERTY(int comparedCount READ comparedCount NOTIFY comparisonChanged)
    Q_PROPERTY(QString netSize READ netSize NOTIFY comparisonChanged)
    Q_PROPERTY(int activeRootCount READ activeRootCount NOTIFY rootsChanged)
    Q_PROPERTY(int totalSnapshotCount READ totalSnapshotCount NOTIFY rootsChanged)
    Q_PROPERTY(qint64 totalFileCount READ totalFileCount NOTIFY rootsChanged)
    Q_PROPERTY(QString sheet READ sheet WRITE setSheet NOTIFY sheetChanged)
    Q_PROPERTY(QString toast READ toast WRITE setToast NOTIFY toastChanged)
    Q_PROPERTY(QString detailId READ detailId WRITE setDetailId NOTIFY detailIdChanged)
    Q_PROPERTY(QString ignoreRules READ ignoreRules NOTIFY ignoreRulesChanged)
    Q_PROPERTY(QVariantList cleanupSelection READ cleanupSelection WRITE setCleanupSelection NOTIFY
                   cleanupChanged)
    Q_PROPERTY(QVariantList cleanupCandidates READ cleanupCandidates NOTIFY cleanupChanged)
    Q_PROPERTY(
        QVariantList visibleCleanupCandidates READ visibleCleanupCandidates NOTIFY cleanupChanged)
    Q_PROPERTY(
        QString cleanupSearch READ cleanupSearch WRITE setCleanupSearch NOTIFY cleanupChanged)
    Q_PROPERTY(QString cleanupSelectedSize READ cleanupSelectedSize NOTIFY cleanupChanged)
    Q_PROPERTY(
        bool cleanupReviewed READ cleanupReviewed WRITE setCleanupReviewed NOTIFY cleanupChanged)
    Q_PROPERTY(
        QString cleanupResult READ cleanupResult WRITE setCleanupResult NOTIFY cleanupChanged)
    Q_PROPERTY(bool closeToTray READ closeToTray WRITE setCloseToTray NOTIFY preferencesChanged)
    Q_PROPERTY(bool launchAtStartup READ launchAtStartup WRITE setLaunchAtStartup NOTIFY
                   preferencesChanged)
    Q_PROPERTY(bool notifyScheduledSuccess READ notifyScheduledSuccess WRITE
                   setNotifyScheduledSuccess NOTIFY preferencesChanged)
    Q_PROPERTY(int retention READ retention WRITE setRetention NOTIFY preferencesChanged)
    Q_PROPERTY(QString dataDirectory READ dataDirectory CONSTANT)

  public:
    enum class Section
    {
        Overview,
        Folders,
        Compare,
        Settings
    };
    Q_ENUM(Section)

    explicit AppState(QObject *parent = nullptr);
    ~AppState() override;

    [[nodiscard]] Section selectedSection() const
    {
        return m_selectedSection;
    }
    [[nodiscard]] bool reducedMotion() const
    {
        return m_reducedMotion;
    }
    [[nodiscard]] bool backgroundMotionEnabled() const
    {
        return m_backgroundMotionEnabled;
    }
    [[nodiscard]] QVariantList roots() const
    {
        return m_roots;
    }
    [[nodiscard]] QVariantMap currentRoot() const
    {
        return m_currentRoot;
    }
    [[nodiscard]] int rootIndex() const
    {
        return m_rootIndex;
    }
    [[nodiscard]] QVariantList snapshots() const
    {
        return m_snapshots;
    }
    [[nodiscard]] QVariantList filteredSnapshots() const;
    [[nodiscard]] QString snapshotSearch() const
    {
        return m_snapshotSearch;
    }
    [[nodiscard]] QString beforeId() const
    {
        return m_beforeId;
    }
    [[nodiscard]] QString afterId() const
    {
        return m_afterId;
    }
    [[nodiscard]] bool hasPair() const
    {
        return !m_beforeId.isEmpty() && !m_afterId.isEmpty();
    }
    [[nodiscard]] bool scanning() const
    {
        return m_scanning;
    }
    [[nodiscard]] int scanProgress() const
    {
        return m_scanProgress;
    }
    [[nodiscard]] QString scanError() const
    {
        return m_scanError;
    }
    [[nodiscard]] bool comparing() const
    {
        return m_comparing;
    }
    [[nodiscard]] bool exporting() const
    {
        return m_exporting;
    }
    [[nodiscard]] QString exportError() const
    {
        return m_exportError;
    }
    [[nodiscard]] bool comparisonReady() const
    {
        return m_comparisonReady;
    }
    [[nodiscard]] QVariantList changes() const
    {
        return m_changes;
    }
    [[nodiscard]] QVariantList displayedChanges() const;
    [[nodiscard]] QStringList expanded() const
    {
        return m_expanded;
    }
    [[nodiscard]] QString search() const
    {
        return m_search;
    }
    [[nodiscard]] QString filter() const
    {
        return m_filter;
    }
    [[nodiscard]] bool hasWarnings() const
    {
        return m_warningCount > 0;
    }
    [[nodiscard]] bool payloadMissing() const;
    [[nodiscard]] int addedCount() const
    {
        return m_addedCount;
    }
    [[nodiscard]] int removedCount() const
    {
        return m_removedCount;
    }
    [[nodiscard]] int modifiedCount() const
    {
        return m_modifiedCount;
    }
    [[nodiscard]] int unchangedCount() const
    {
        return m_unchangedCount;
    }
    [[nodiscard]] int warningCount() const
    {
        return m_warningCount;
    }
    [[nodiscard]] int comparedCount() const
    {
        return m_comparedCount;
    }
    [[nodiscard]] QString netSize() const;
    [[nodiscard]] int activeRootCount() const
    {
        return m_activeRootCount;
    }
    [[nodiscard]] int totalSnapshotCount() const
    {
        return m_totalSnapshotCount;
    }
    [[nodiscard]] qint64 totalFileCount() const
    {
        return m_totalFileCount;
    }
    [[nodiscard]] QString sheet() const
    {
        return m_sheet;
    }
    [[nodiscard]] QString toast() const
    {
        return m_toast;
    }
    [[nodiscard]] QString detailId() const
    {
        return m_detailId;
    }
    [[nodiscard]] QString ignoreRules() const
    {
        return m_ignoreRules;
    }
    [[nodiscard]] QVariantList cleanupSelection() const
    {
        return m_cleanupSelection;
    }
    [[nodiscard]] QVariantList cleanupCandidates() const;
    [[nodiscard]] QVariantList visibleCleanupCandidates() const;
    [[nodiscard]] QString cleanupSearch() const
    {
        return m_cleanupSearch;
    }
    [[nodiscard]] QString cleanupSelectedSize() const;
    [[nodiscard]] bool cleanupReviewed() const
    {
        return m_cleanupReviewed;
    }
    [[nodiscard]] QString cleanupResult() const
    {
        return m_cleanupResult;
    }
    [[nodiscard]] bool closeToTray() const
    {
        return m_closeToTray;
    }
    [[nodiscard]] bool launchAtStartup() const
    {
        return m_launchAtStartup;
    }
    [[nodiscard]] bool notifyScheduledSuccess() const
    {
        return m_notifyScheduledSuccess;
    }
    [[nodiscard]] int retention() const
    {
        return m_retention;
    }
    [[nodiscard]] QString dataDirectory() const
    {
        return m_paths.dataDirectory;
    }

    void setSelectedSection(Section section);
    void setReducedMotion(bool enabled);
    void setBackgroundMotionEnabled(bool enabled);
    void setSnapshotSearch(const QString &search);
    void setExpanded(const QStringList &expanded);
    void setSearch(const QString &search);
    void setFilter(const QString &filter);
    void setSheet(const QString &sheet);
    void setToast(const QString &toast);
    void setDetailId(const QString &detailId);
    void setCleanupSelection(const QVariantList &selection);
    void setCleanupSearch(const QString &search);
    void setCleanupReviewed(bool reviewed);
    void setCleanupResult(const QString &result);
    void setCloseToTray(bool enabled);
    void setLaunchAtStartup(bool enabled);
    void setNotifyScheduledSuccess(bool enabled);
    void setRetention(int retention);

    Q_INVOKABLE void chooseRoot(int index);
    Q_INVOKABLE void chooseSnapshot(const QString &snapshotId);
    Q_INVOKABLE void clearSnapshotPair();
    Q_INVOKABLE void toggleExpanded(const QString &path);
    Q_INVOKABLE void takeSnapshot();
    Q_INVOKABLE void cancelScan();
    Q_INVOKABLE void startComparison();
    Q_INVOKABLE void exportSnapshot(const QString &format, const QUrl &destination);
    Q_INVOKABLE void exportComparison(const QString &format, const QUrl &destination);
    Q_INVOKABLE void cancelExport();
    Q_INVOKABLE void openSheet(const QString &kind);
    Q_INVOKABLE void openCurrentFolder();
    Q_INVOKABLE void addFolder(const QUrl &folderUrl);
    Q_INVOKABLE void updateRoot(const QString &name, const QString &schedule, int retention,
                                const QString &ignoreRules, bool archived);
    Q_INVOKABLE void toggleCleanup(const QString &path);
    Q_INVOKABLE QString cleanupSelectionState(const QString &path) const;
    Q_INVOKABLE void saveDescription(const QString &description);
    Q_INVOKABLE void deleteSelectedSnapshot();
    Q_INVOKABLE void clearSelectedRootHistory();
    Q_INVOKABLE void removeCurrentRoot();
    Q_INVOKABLE QVariantMap snapshot(const QString &snapshotId) const;

  signals:
    void selectedSectionChanged();
    void reducedMotionChanged();
    void backgroundMotionEnabledChanged();
    void rootsChanged();
    void currentRootChanged();
    void rootIndexChanged();
    void snapshotsChanged();
    void snapshotSearchChanged();
    void snapshotPairChanged();
    void scanningChanged();
    void scanProgressChanged();
    void scanErrorChanged();
    void scanStarted(const QString &rootId);
    void scanProgressed(const QString &rootId, int progress);
    void scanCompleted(const QString &rootId, const QString &snapshotId, qint64 warningCount);
    void scanFailed(const QString &rootId, const QString &error);
    void configurationChanged();
    void comparingChanged();
    void comparisonChanged();
    void exportChanged();
    void sheetChanged();
    void toastChanged();
    void detailIdChanged();
    void ignoreRulesChanged();
    void cleanupChanged();
    void preferencesChanged();

  private:
    void refreshModels();
    void requestSnapshot(const foldersnap::WatchedRoot &root, foldersnap::SnapshotTrigger trigger);
    void finishScan(const foldersnap::ScanJobResult &result);
    void failScan(const QString &rootId, const QString &error);
    void updateCurrentScanState();
    void evaluateSchedules();
    void finishComparison();
    void invalidateComparison();
    void saveConfiguration();
    void setScanError(const QString &error);
    void setScanning(bool scanning);
    void setComparing(bool comparing);
    void startExport(const QString &firstSnapshotId, const QString &secondSnapshotId,
                     const QString &format, const QUrl &destination);
    void setExportError(const QString &error);
    [[nodiscard]] foldersnap::WatchedRoot *currentConfigurationRoot();
    [[nodiscard]] const foldersnap::WatchedRoot *currentConfigurationRoot() const;
    [[nodiscard]] foldersnap::WatchedRoot *configurationRoot(const QString &rootId);

    Section m_selectedSection{Section::Overview};
    bool m_reducedMotion{false};
    bool m_backgroundMotionEnabled{true};
    foldersnap::StoragePaths m_paths;
    foldersnap::Configuration m_configuration;
    QVariantList m_roots;
    QVariantMap m_currentRoot;
    QVariantList m_snapshots;
    int m_rootIndex{0};
    QString m_snapshotSearch;
    QString m_beforeId;
    QString m_afterId;
    bool m_scanning{false};
    int m_scanProgress{0};
    QString m_scanError;
    bool m_comparing{false};
    bool m_exporting{false};
    QString m_exportError;
    quint64 m_comparisonGeneration{0};
    bool m_comparisonReady{false};
    QVariantList m_changes;
    QStringList m_expanded;
    QString m_search;
    QString m_filter{"All changes"};
    int m_addedCount{0};
    int m_removedCount{0};
    int m_modifiedCount{0};
    int m_unchangedCount{0};
    int m_warningCount{0};
    int m_comparedCount{0};
    qint64 m_netSize{0};
    int m_activeRootCount{0};
    int m_totalSnapshotCount{0};
    qint64 m_totalFileCount{0};
    QString m_sheet;
    QString m_toast;
    QString m_detailId;
    QString m_ignoreRules;
    QVariantList m_cleanupSelection;
    QString m_cleanupSearch;
    bool m_cleanupReviewed{false};
    QString m_cleanupResult;
    bool m_closeToTray{true};
    bool m_launchAtStartup{false};
    bool m_notifyScheduledSuccess{false};
    int m_retention{50};
    std::unique_ptr<foldersnap::ScanCoordinator> m_scanCoordinator;
    std::unique_ptr<foldersnap::ExportCoordinator> m_exportCoordinator;
    std::unique_ptr<QFutureWatcher<ComparisonJobResult>> m_comparisonWatcher;
    QTimer m_scheduleTimer;
};
