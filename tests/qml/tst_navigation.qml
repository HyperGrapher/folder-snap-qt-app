import QtQuick
import QtTest
import FolderSnap

TestCase {
    id: testCase
    name: "Navigation"
    width: 900
    height: 700
    visible: true
    when: windowShown
    UiPreviewState {
        id: state
    }
    MotionPolicy {
        id: motion
    }
    PageHost {
        id: host
        anchors.fill: parent
        appState: state
        motion: motion
    }
    function init() {
        motion.reducedMotion = true;
        motion.windowVisible = true;
        motion.windowExposed = true;
        motion.windowMinimized = false;
        state.selectedSection = AppState.Overview;
        state.chooseRoot(0);
        motion.reducedMotion = false;
    }
    function assertSettled(index) {
        tryCompare(host, "isTransitioning", false, 1000);
        compare(host.currentIndex, index);
        for (let i = 0; i < 4; ++i) {
            compare(host.pages[i].opacity, i === index ? 1 : 0);
            compare(host.pages[i].visible, i === index);
            compare(host.pages[i].enabled, i === index);
        }
    }
    function test_allPages() {
        for (let i = 0; i < 4; ++i) {
            host.selectPage(i);
            assertSettled(i);
        }
    }
    function test_interruptedNavigation() {
        const originalPages = host.pages.slice();
        host.selectPage(1);
        wait(35);
        host.selectPage(2);
        for (let i = 0; i < 4; ++i)
            compare(host.pages[i].enabled, false);
        wait(35);
        let total = 0;
        for (let i = 0; i < 4; ++i)
            total += host.pages[i].opacity;
        fuzzyCompare(total, 1, 0.02);
        host.selectPage(3);
        host.selectPage(0);
        assertSettled(0);
        for (let i = 0; i < 4; ++i)
            compare(host.pages[i], originalPages[i]);
    }
    function test_statePreserved() {
        host.selectPage(2);
        assertSettled(2);
        state.chooseSnapshot("not-a-snapshot");
        state.search = "main";
        host.selectPage(1);
        assertSettled(1);
        host.selectPage(2);
        assertSettled(2);
        compare(state.beforeId, "");
        compare(state.afterId, "");
        compare(state.search, "main");
    }
    function test_reduceDuringTransition() {
        host.selectPage(2);
        motion.reducedMotion = true;
        assertSettled(2);
        host.selectPage(3);
        compare(host.isTransitioning, false);
        assertSettled(3);
    }
    function test_hiddenSettles() {
        host.selectPage(2);
        motion.windowVisible = false;
        compare(host.isTransitioning, false);
        compare(motion.ambientEnabled, false);
        motion.windowVisible = true;
        motion.windowMinimized = true;
        compare(motion.ambientEnabled, false);
        motion.windowMinimized = false;
        compare(motion.ambientEnabled, true);
        motion.windowExposed = false;
        compare(motion.ambientEnabled, false);
    }
    function test_outgoingPageCannotAct() {
        host.selectPage(0);
        assertSettled(0);
        const take = findChild(host, "overviewSnapshotButton");
        take.forceActiveFocus();
        host.selectPage(1);
        keyClick(Qt.Key_Space);
        compare(state.scanning, false);
        assertSettled(1);
        keyClick(Qt.Key_Return);
        compare(state.scanning, false);
    }
}
