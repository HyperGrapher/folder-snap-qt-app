#include "AppState.h"
#include <QSignalSpy>
#include <QtTest>

class AppStateTest final : public QObject
{
    Q_OBJECT
  private slots:
    void defaults()
    {
        AppState state;
        QCOMPARE(state.selectedSection(), AppState::Section::Overview);
        QCOMPARE(state.demoProgress(), 25);
        QCOMPARE(state.demoStatus(), AppState::Status::Ready);
        QVERIFY(!state.reducedMotion());
        QVERIFY(state.backgroundMotionEnabled());
    }
    void actionsAndNotifications()
    {
        AppState state;
        QSignalSpy progressSpy(&state, &AppState::demoProgressChanged);
        for (int step = 0; step < 8; ++step)
        {
            state.advanceProgress();
        }
        QCOMPARE(state.demoProgress(), 100);
        QCOMPARE(progressSpy.count(), 3);
        state.cycleStatus();
        QCOMPARE(state.demoStatus(), AppState::Status::Active);
        state.cycleStatus();
        QCOMPARE(state.demoStatus(), AppState::Status::Complete);
        state.cycleStatus();
        QCOMPARE(state.demoStatus(), AppState::Status::Ready);
        QSignalSpy sectionSpy(&state, &AppState::selectedSectionChanged);
        state.setSelectedSection(AppState::Section::Settings);
        state.setSelectedSection(AppState::Section::Settings);
        state.setSelectedSection(static_cast<AppState::Section>(99));
        QCOMPARE(sectionSpy.count(), 1);
        state.setReducedMotion(true);
        state.setBackgroundMotionEnabled(false);
        state.cycleStatus();
        state.resetDemo();
        QCOMPARE(state.demoProgress(), 25);
        QCOMPARE(state.demoStatus(), AppState::Status::Ready);
        QCOMPARE(state.selectedSection(), AppState::Section::Settings);
        QVERIFY(state.reducedMotion());
        QVERIFY(!state.backgroundMotionEnabled());
    }
};
QTEST_GUILESS_MAIN(AppStateTest)
#include "tst_appstate.moc"
