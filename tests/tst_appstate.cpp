#include "AppState.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

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
            state.addFolder("Watched", watchedDirectory.path());
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
};
QTEST_GUILESS_MAIN(AppStateTest)
#include "tst_appstate.moc"
