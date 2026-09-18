#include "AppState.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "paths/WindowsPaths.h"
#include "storage/ConfigurationStore.h"
#include "storage/HistoryStore.h"
#include "storage/SnapshotStore.h"
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
            QSignalSpy startedSpy(&state, &AppState::scanStarted);
            QSignalSpy progressSpy(&state, &AppState::scanProgressed);
            QSignalSpy completedSpy(&state, &AppState::scanCompleted);
            QSignalSpy failedSpy(&state, &AppState::scanFailed);

            state.takeSnapshot();
            QTRY_VERIFY_WITH_TIMEOUT(!state.scanning(), 5000);
            QCOMPARE(state.snapshots().size(), 1);

            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
            QVERIFY(file.write("hello, FolderSnap") > 0);
            file.close();
            QVERIFY(QDir().mkpath(watchedDirectory.filePath("added/nested")));
            QFile addedFile(watchedDirectory.filePath("added/one.txt"));
            QVERIFY(addedFile.open(QIODevice::WriteOnly));
            QCOMPARE(addedFile.write("one"), qint64(3));
            addedFile.close();
            QFile nestedFile(watchedDirectory.filePath("added/nested/two.txt"));
            QVERIFY(nestedFile.open(QIODevice::WriteOnly));
            QCOMPARE(nestedFile.write("three"), qint64(5));
            nestedFile.close();

            state.takeSnapshot();
            QTRY_VERIFY_WITH_TIMEOUT(!state.scanning(), 5000);
            QCOMPARE(state.snapshots().size(), 2);
            QCOMPARE(startedSpy.count(), 2);
            QCOMPARE(completedSpy.count(), 2);
            QCOMPARE(failedSpy.count(), 0);
            QVERIFY(!progressSpy.isEmpty());
            for (const QList<QVariant> &arguments : progressSpy)
            {
                const int progress = arguments.at(1).toInt();
                QVERIFY(progress >= 0);
                QVERIFY(progress <= 100);
            }

            const QVariantList snapshots = state.snapshots();
            state.chooseSnapshot(snapshots.at(1).toMap().value("id").toString());
            state.chooseSnapshot(snapshots.at(0).toMap().value("id").toString());
            QVERIFY(state.hasPair());
            state.startComparison();
            QTRY_VERIFY_WITH_TIMEOUT(state.comparisonReady(), 5000);
            QCOMPARE(state.modifiedCount(), 1);
            QCOMPARE(state.addedCount(), 4);
            QCOMPARE(state.removedCount(), 0);

            const QVariantList cleanupCandidates = state.cleanupCandidates();
            QCOMPARE(cleanupCandidates.size(), 4);
            for (const QVariant &candidate : cleanupCandidates)
            {
                QCOMPARE(candidate.toMap().value("status").toString(), QString("Added"));
            }
            const auto addedFolder = std::find_if(
                cleanupCandidates.cbegin(), cleanupCandidates.cend(), [](const QVariant &candidate)
                { return candidate.toMap().value("path").toString() == "added"; });
            QVERIFY(addedFolder != cleanupCandidates.cend());
            QVERIFY(addedFolder->toMap().value("folder").toBool());
            QCOMPARE(addedFolder->toMap().value("after").toString(), QString("8 B"));
            state.openSheet("cleanup");
            QVERIFY(state.cleanupSelection().isEmpty());
            QCOMPARE(state.cleanupSelectedSize(), QString("0 B"));
            state.toggleCleanup("added");
            QCOMPARE(state.cleanupSelection().size(), 4);
            QCOMPARE(state.cleanupSelectedSize(), QString("8 B"));
            QCOMPARE(state.cleanupSelectionState("added"), QString("checked"));
            state.toggleCleanup("added/one.txt");
            QCOMPARE(state.cleanupSelection().size(), 2);
            QCOMPARE(state.cleanupSelectedSize(), QString("5 B"));
            QCOMPARE(state.cleanupSelectionState("added"), QString("partial"));
            QCOMPARE(state.cleanupSelectionState("added/nested"), QString("checked"));
            state.setCleanupSearch("two.txt");
            QCOMPARE(state.visibleCleanupCandidates().size(), 3);
            QCOMPARE(state.cleanupSelection().size(), 2);
            state.setCleanupSearch("not-present");
            QVERIFY(state.visibleCleanupCandidates().isEmpty());
            QCOMPARE(state.cleanupSelection().size(), 2);
            state.openSheet("cleanup");
            QVERIFY(state.cleanupSelection().isEmpty());
            QVERIFY(state.cleanupSearch().isEmpty());

            const QString snapshotExport = dataDirectory.filePath("snapshot-report.csv");
            state.setDetailId(snapshots.at(0).toMap().value("id").toString());
            state.exportSnapshot("csv", QUrl::fromLocalFile(snapshotExport));
            QTRY_VERIFY_WITH_TIMEOUT(!state.exporting(), 5000);
            QVERIFY(state.exportError().isEmpty());
            QFile snapshotReport(snapshotExport);
            QVERIFY(snapshotReport.open(QIODevice::ReadOnly));
            QCOMPARE(snapshotReport.read(3), QByteArray::fromHex("efbbbf"));

            const QString comparisonExport = dataDirectory.filePath("comparison-report.html");
            state.exportComparison("html", QUrl::fromLocalFile(comparisonExport));
            QTRY_VERIFY_WITH_TIMEOUT(!state.exporting(), 5000);
            QVERIFY(state.exportError().isEmpty());
            QFile comparisonReport(comparisonExport);
            QVERIFY(comparisonReport.open(QIODevice::ReadOnly));
            const QByteArray comparisonHtml = comparisonReport.readAll();
            QVERIFY(comparisonHtml.contains("\"reportType\":\"comparison\""));
            QVERIFY(!comparisonHtml.contains("/* FOLDERSNAP_REPORT_DATA */"));
        }
        qunsetenv("FOLDERSNAP_DATA_DIR");
    }

    void keepsUnchangedParentFoldersAsCleanupContext()
    {
        QTemporaryDir dataDirectory;
        QTemporaryDir watchedDirectory;
        QVERIFY(dataDirectory.isValid());
        QVERIFY(watchedDirectory.isValid());
        qputenv("FOLDERSNAP_DATA_DIR", dataDirectory.path().toUtf8());
        const auto restoreEnvironment = qScopeGuard([] { qunsetenv("FOLDERSNAP_DATA_DIR"); });

        const QString existingFolder = watchedDirectory.filePath("existing");
        QVERIFY(QDir().mkpath(existingFolder));
        QFile trackedFile(QDir(existingFolder).filePath("tracked.txt"));
        QVERIFY(trackedFile.open(QIODevice::WriteOnly));
        QCOMPARE(trackedFile.write("tracked"), qint64(7));
        trackedFile.close();

        AppState state;
        state.addFolder(QUrl::fromLocalFile(watchedDirectory.path()));
        state.takeSnapshot();
        QTRY_VERIFY_WITH_TIMEOUT(!state.scanning(), 5000);

        QFile addedFile(QDir(existingFolder).filePath("added.txt"));
        QVERIFY(addedFile.open(QIODevice::WriteOnly));
        QCOMPARE(addedFile.write("added"), qint64(5));
        addedFile.close();
        state.takeSnapshot();
        QTRY_VERIFY_WITH_TIMEOUT(!state.scanning(), 5000);

        const QVariantList snapshots = state.snapshots();
        state.chooseSnapshot(snapshots.at(1).toMap().value("id").toString());
        state.chooseSnapshot(snapshots.at(0).toMap().value("id").toString());
        state.startComparison();
        QTRY_VERIFY_WITH_TIMEOUT(state.comparisonReady(), 5000);

        QCOMPARE(state.cleanupCandidates().size(), 1);
        const QVariantList rows = state.visibleCleanupCandidates();
        QCOMPARE(rows.size(), 2);
        const QVariantMap context = rows.at(0).toMap();
        const QVariantMap file = rows.at(1).toMap();
        QCOMPARE(context.value("path").toString(), QString("existing"));
        QVERIFY(context.value("folder").toBool());
        QVERIFY(!context.value("cleanupSelectable").toBool());
        QCOMPARE(file.value("path").toString(), QString("existing/added.txt"));
        QVERIFY(file.value("cleanupSelectable").toBool());

        state.toggleCleanup("existing");
        QVERIFY(state.cleanupSelection().isEmpty());
        state.toggleCleanup("existing/added.txt");
        QCOMPARE(state.cleanupSelection(), QVariantList{QString("existing/added.txt")});
        QCOMPARE(state.cleanupSelectionState("existing"), QString("partial"));
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
        QSignalSpy configurationSpy(&state, &AppState::configurationChanged);
        state.addFolder(QUrl::fromLocalFile(watchedDirectory.path()));
        QCOMPARE(state.roots().size(), 1);
        QCOMPARE(state.selectedSection(), AppState::Section::Folders);
        QVERIFY(state.toast().contains("Take a snapshot"));
        QCOMPARE(configurationSpy.count(), 1);

        state.addFolder(QUrl::fromLocalFile(watchedDirectory.path()));
        QCOMPARE(state.roots().size(), 1);
        QCOMPARE(state.toast(), QString("That folder is already being watched."));
        QCOMPARE(configurationSpy.count(), 1);

        state.updateRoot("Renamed folder", "Every 3 hours", 25, "cache/\n*.tmp", true);
        QCOMPARE(state.currentRoot().value("name").toString(), QString("Renamed folder"));
        QCOMPARE(state.currentRoot().value("schedule").toString(), QString("Every 3 hours"));
        QCOMPARE(state.currentRoot().value("retention").toInt(), 25);
        QVERIFY(state.currentRoot().value("archived").toBool());
        QCOMPARE(state.ignoreRules(), QString("cache/\n*.tmp"));
        QCOMPARE(configurationSpy.count(), 2);

        state.updateRoot({}, "Manual only", 42, {}, false);
        QCOMPARE(state.currentRoot().value("name").toString(), QString("Renamed folder"));
        QCOMPARE(state.currentRoot().value("retention").toInt(), 25);
        QVERIFY(state.currentRoot().value("archived").toBool());
        QCOMPARE(configurationSpy.count(), 2);

        state.updateRoot("Renamed folder", "Manual only", 100, "cache/", false);
        QVERIFY(!state.currentRoot().value("archived").toBool());
        QCOMPARE(state.currentRoot().value("retention").toInt(), 100);
        QCOMPARE(configurationSpy.count(), 3);

        const foldersnap::StoragePaths paths =
            foldersnap::StoragePaths::fromDataDirectory(dataDirectory.path());
        const foldersnap::WatchedRoot persisted =
            foldersnap::ConfigurationStore(paths).loadConfiguration().value.roots.first();
        QCOMPARE(persisted.displayName, QString("Renamed folder"));
        QCOMPARE(persisted.retention, 100);
        QCOMPARE(persisted.ignoreRules, QStringList{"cache/"});
        QVERIFY(!persisted.archived);
    }

    void removesWatchedFolderWithoutTouchingItsFiles()
    {
        QTemporaryDir dataDirectory;
        QTemporaryDir watchedDirectory;
        QVERIFY(dataDirectory.isValid());
        QVERIFY(watchedDirectory.isValid());
        qputenv("FOLDERSNAP_DATA_DIR", dataDirectory.path().toUtf8());
        const auto restoreEnvironment = qScopeGuard([] { qunsetenv("FOLDERSNAP_DATA_DIR"); });
        const QString watchedFile = watchedDirectory.filePath("keep-me.txt");
        QFile file(watchedFile);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("untouched"), qint64(9));
        file.close();

        AppState state;
        state.addFolder(QUrl::fromLocalFile(watchedDirectory.path()));
        state.takeSnapshot();
        QTRY_VERIFY_WITH_TIMEOUT(!state.scanning(), 5000);
        QCOMPARE(state.snapshots().size(), 1);

        const foldersnap::StoragePaths paths =
            foldersnap::StoragePaths::fromDataDirectory(dataDirectory.path());
        const foldersnap::Configuration before =
            foldersnap::ConfigurationStore(paths).loadConfiguration().value;
        const QString rootId = before.roots.first().rootId;
        const QString snapshotId =
            foldersnap::HistoryStore(paths).loadHistoryForRoot(rootId).first().snapshotId;
        QVERIFY(foldersnap::SnapshotStore(paths).hasPayload(snapshotId));

        state.removeCurrentRoot();

        QVERIFY(state.roots().isEmpty());
        QVERIFY(state.currentRoot().isEmpty());
        QVERIFY(state.snapshots().isEmpty());
        QCOMPARE(state.toast(), QString("Watched folder and its snapshot history removed."));
        QVERIFY(foldersnap::ConfigurationStore(paths).loadConfiguration().value.roots.isEmpty());
        QVERIFY(foldersnap::HistoryStore(paths).loadHistoryForRoot(rootId).isEmpty());
        QVERIFY(!foldersnap::SnapshotStore(paths).hasPayload(snapshotId));
        QVERIFY(QFile::exists(watchedFile));
        QCOMPARE(QFileInfo(watchedFile).size(), qint64(9));
    }

    void reportsScanFailures()
    {
        QTemporaryDir dataDirectory;
        QTemporaryDir watchedDirectory;
        QVERIFY(dataDirectory.isValid());
        QVERIFY(watchedDirectory.isValid());
        qputenv("FOLDERSNAP_DATA_DIR", dataDirectory.path().toUtf8());
        const auto restoreEnvironment = qScopeGuard([] { qunsetenv("FOLDERSNAP_DATA_DIR"); });

        AppState state;
        state.addFolder(QUrl::fromLocalFile(watchedDirectory.path()));
        QVERIFY(watchedDirectory.remove());
        QSignalSpy failedSpy(&state, &AppState::scanFailed);
        QSignalSpy completedSpy(&state, &AppState::scanCompleted);

        state.takeSnapshot();

        QTRY_COMPARE_WITH_TIMEOUT(failedSpy.count(), 1, 5000);
        QCOMPARE(completedSpy.count(), 0);
        QVERIFY(!failedSpy.first().at(0).toString().isEmpty());
        QCOMPARE(failedSpy.first().at(1).toString(), state.scanError());
        QVERIFY(!state.scanning());
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
