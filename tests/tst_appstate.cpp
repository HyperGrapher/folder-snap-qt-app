#include "AppState.h"

#include <QDir>
#include <QFile>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "paths/WindowsPaths.h"
#include "storage/ConfigurationStore.h"
#include "storage/StoragePaths.h"

class AppStateTest final : public QObject
{
    Q_OBJECT
  private slots:
    void defaults()
    {
        AppState state;
        QCOMPARE(state.selectedSection(), AppState::Section::Overview);
        QVERIFY(!state.reducedMotion());
        QVERIFY(state.backgroundMotionEnabled());
    }
    void navigationAndPreferences()
    {
        AppState state;
        QSignalSpy sectionSpy(&state, &AppState::selectedSectionChanged);
        state.setSelectedSection(AppState::Section::Compare);
        state.setSelectedSection(AppState::Section::Compare);
        state.setSelectedSection(static_cast<AppState::Section>(99));
        QCOMPARE(sectionSpy.count(), 1);
        QCOMPARE(state.selectedSection(), AppState::Section::Compare);
        state.setReducedMotion(true);
        state.setBackgroundMotionEnabled(false);
        state.setSelectedSection(AppState::Section::Settings);
        QVERIFY(state.reducedMotion());
        QVERIFY(!state.backgroundMotionEnabled());
    }

    void scansAndComparesARealFolder()
    {
        QTemporaryDir dataDirectory;
        QTemporaryDir watchedDirectory;
        QVERIFY(dataDirectory.isValid());
        QVERIFY(watchedDirectory.isValid());
        qputenv("FOLDERSNAP_DATA_DIR", dataDirectory.path().toUtf8());

        QFile file(watchedDirectory.filePath("notes.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("hello") > 0);
        file.close();

        {
            AppState state;
            state.addFolder(QUrl::fromLocalFile(watchedDirectory.path()));
            QCOMPARE(state.roots().size(), 1);

            state.takeSnapshot();
            QTRY_VERIFY_WITH_TIMEOUT(!state.scanning(), 5000);
            QCOMPARE(state.snapshots().size(), 1);

            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
            QVERIFY(file.write("hello, FolderSnap") > 0);
            file.close();

            state.takeSnapshot();
            QTRY_VERIFY_WITH_TIMEOUT(!state.scanning(), 5000);
            QCOMPARE(state.snapshots().size(), 2);

            const QVariantList snapshots = state.snapshots();
            state.chooseSnapshot(snapshots.at(1).toMap().value("id").toString());
            state.chooseSnapshot(snapshots.at(0).toMap().value("id").toString());
            QVERIFY(state.hasPair());
            state.startComparison();
            QTRY_VERIFY_WITH_TIMEOUT(state.comparisonReady(), 5000);
            QCOMPARE(state.modifiedCount(), 1);
            QCOMPARE(state.addedCount(), 0);
            QCOMPARE(state.removedCount(), 0);
        }
        qunsetenv("FOLDERSNAP_DATA_DIR");
    }

    void managesWatchedFolderPreferencesAtomically()
    {
        QTemporaryDir dataDirectory;
        QTemporaryDir watchedDirectory;
        QVERIFY(dataDirectory.isValid());
        QVERIFY(watchedDirectory.isValid());
        qputenv("FOLDERSNAP_DATA_DIR", dataDirectory.path().toUtf8());
        const auto restoreEnvironment = qScopeGuard([] { qunsetenv("FOLDERSNAP_DATA_DIR"); });

        AppState state;
        state.addFolder(QUrl::fromLocalFile(watchedDirectory.path()));
        QCOMPARE(state.roots().size(), 1);
        QCOMPARE(state.selectedSection(), AppState::Section::Folders);
        QVERIFY(state.toast().contains("Take a snapshot"));

        state.addFolder(QUrl::fromLocalFile(watchedDirectory.path()));
        QCOMPARE(state.roots().size(), 1);
        QCOMPARE(state.toast(), QString("That folder is already being watched."));

        state.updateRoot("Renamed folder", "Every 3 hours", 25, "cache/\n*.tmp", true);
        QCOMPARE(state.currentRoot().value("name").toString(), QString("Renamed folder"));
        QCOMPARE(state.currentRoot().value("schedule").toString(), QString("Every 3 hours"));
        QCOMPARE(state.currentRoot().value("retention").toInt(), 25);
        QVERIFY(state.currentRoot().value("archived").toBool());
        QCOMPARE(state.ignoreRules(), QString("cache/\n*.tmp"));

        state.updateRoot({}, "Manual only", 42, {}, false);
        QCOMPARE(state.currentRoot().value("name").toString(), QString("Renamed folder"));
        QCOMPARE(state.currentRoot().value("retention").toInt(), 25);
        QVERIFY(state.currentRoot().value("archived").toBool());

        state.updateRoot("Renamed folder", "Manual only", 100, "cache/", false);
        QVERIFY(!state.currentRoot().value("archived").toBool());
        QCOMPARE(state.currentRoot().value("retention").toInt(), 100);

        const foldersnap::StoragePaths paths =
            foldersnap::StoragePaths::fromDataDirectory(dataDirectory.path());
        const foldersnap::WatchedRoot persisted =
            foldersnap::ConfigurationStore(paths).loadConfiguration().value.roots.first();
        QCOMPARE(persisted.displayName, QString("Renamed folder"));
        QCOMPARE(persisted.retention, 100);
        QCOMPARE(persisted.ignoreRules, QStringList{"cache/"});
        QVERIFY(!persisted.archived);
    }

    void runsOneCatchUpSnapshotForAnOverdueSchedule()
    {
        QTemporaryDir dataDirectory;
        QTemporaryDir watchedDirectory;
        QVERIFY(dataDirectory.isValid());
        QVERIFY(watchedDirectory.isValid());
        qputenv("FOLDERSNAP_DATA_DIR", dataDirectory.path().toUtf8());
        const auto restoreEnvironment = qScopeGuard([] { qunsetenv("FOLDERSNAP_DATA_DIR"); });

        QFile file(watchedDirectory.filePath("scheduled.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("scheduled content") > 0);
        file.close();

        const foldersnap::RootPath normalized =
            foldersnap::normalizeRootPath(watchedDirectory.path());
        foldersnap::WatchedRoot root;
        root.rootId = foldersnap::createId();
        root.displayName = "Scheduled folder";
        root.path = normalized.displayPath;
        root.normalizedPath = normalized.identityPath;
        root.schedule.kind = foldersnap::ScheduleKind::Interval;
        root.schedule.intervalHours = 1;
        constexpr qint64 kTenHoursInNanoseconds = 10LL * 60LL * 60LL * 1000000000LL;
        root.schedule.nextDueAtUtc = foldersnap::UtcTimestamp{
            QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() * 1000000 - kTenHoursInNanoseconds};

        foldersnap::Configuration configuration;
        configuration.roots.append(root);
        const foldersnap::StoragePaths paths =
            foldersnap::StoragePaths::fromDataDirectory(dataDirectory.path());
        foldersnap::ConfigurationStore(paths).saveConfiguration(configuration);

        AppState state;
        QTRY_COMPARE_WITH_TIMEOUT(state.snapshots().size(), 1, 5000);
        QCOMPARE(state.snapshots().first().toMap().value("trigger").toString(),
                 QString("Scheduled"));
        QTest::qWait(50);
        QCOMPARE(state.snapshots().size(), 1);

        const QStringList scheduleOptions{
            "Every 1 hour",   "Every 3 hours",         "Every 6 hours",         "Every 12 hours",
            "Daily at 09:00", "Weekly · Monday 09:00", "Monthly · day 1, 09:00"};
        for (const QString &schedule : scheduleOptions)
        {
            state.updateRoot("Scheduled folder", schedule, 50, {}, false);
            QCOMPARE(state.currentRoot().value("schedule").toString(), schedule);
        }

        const foldersnap::Configuration persisted =
            foldersnap::ConfigurationStore(paths).loadConfiguration().value;
        QVERIFY(persisted.roots.first().schedule.nextDueAtUtc.has_value());
        QVERIFY(*persisted.roots.first().schedule.nextDueAtUtc >
                foldersnap::UtcTimestamp{QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() *
                                         1000000});
    }

    void doesNotScheduleArchivedRoots()
    {
        QTemporaryDir dataDirectory;
        QTemporaryDir watchedDirectory;
        QVERIFY(dataDirectory.isValid());
        QVERIFY(watchedDirectory.isValid());
        qputenv("FOLDERSNAP_DATA_DIR", dataDirectory.path().toUtf8());
        const auto restoreEnvironment = qScopeGuard([] { qunsetenv("FOLDERSNAP_DATA_DIR"); });

        const foldersnap::RootPath normalized =
            foldersnap::normalizeRootPath(watchedDirectory.path());
        foldersnap::WatchedRoot root;
        root.rootId = foldersnap::createId();
        root.displayName = "Archived folder";
        root.path = normalized.displayPath;
        root.normalizedPath = normalized.identityPath;
        root.archived = true;
        root.schedule.kind = foldersnap::ScheduleKind::Interval;
        root.schedule.intervalHours = 1;
        const foldersnap::UtcTimestamp overdue{
            QDateTime::currentDateTimeUtc().addDays(-1).toMSecsSinceEpoch() * 1000000};
        root.schedule.nextDueAtUtc = overdue;

        foldersnap::Configuration configuration;
        configuration.roots.append(root);
        const foldersnap::StoragePaths paths =
            foldersnap::StoragePaths::fromDataDirectory(dataDirectory.path());
        foldersnap::ConfigurationStore(paths).saveConfiguration(configuration);

        AppState state;
        QTest::qWait(50);
        QVERIFY(state.snapshots().isEmpty());

        const foldersnap::Configuration persisted =
            foldersnap::ConfigurationStore(paths).loadConfiguration().value;
        QCOMPARE(persisted.roots.first().schedule.nextDueAtUtc, overdue);
    }
};
QTEST_GUILESS_MAIN(AppStateTest)
#include "tst_appstate.moc"
