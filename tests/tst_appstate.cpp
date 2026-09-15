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
};
QTEST_GUILESS_MAIN(AppStateTest)
#include "tst_appstate.moc"
