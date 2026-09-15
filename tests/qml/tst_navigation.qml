import QtQuick
import QtTest
import Aura

TestCase {
    id: testCase
    name: "Navigation"
    width: 820
    height: 650
    visible: true
    when: windowShown
    AppState {
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
        state.resetDemo();
        motion.reducedMotion = false;
    }
    function assertSettled(index) {
        tryCompare(host, "isTransitioning", false, 1000);
        compare(host.currentIndex, index);
        compare(host.pages.length, 4);
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
        host.selectPage(3);
        compare(host.isTransitioning, false);
    }
    function test_interruptedNavigation() {
        const originalPages = host.pages.slice();
        host.selectPage(1);
        wait(35);
        host.selectPage(2);
        for (let i = 0; i < 4; ++i) {
            compare(host.pages[i].enabled, false);
        }
        wait(35);
        let totalOpacity = 0;
        for (let i = 0; i < 4; ++i) {
            totalOpacity += host.pages[i].opacity;
        }
        fuzzyCompare(totalOpacity, 1, 0.02);
        host.selectPage(3);
        host.selectPage(0);
        assertSettled(0);
        for (let i = 0; i < 4; ++i) {
            compare(host.pages[i], originalPages[i]);
        }
    }
    function test_statePreserved() {
        host.selectPage(1);
        assertSettled(1);
        const collection = findChild(host, "collectionPage");
        collection.selectedCard = 4;
        host.selectPage(2);
        assertSettled(2);
        state.advanceProgress();
        host.selectPage(1);
        assertSettled(1);
        compare(collection.selectedCard, 4);
        compare(state.demoProgress, 50);
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
        motion.windowExposed = true;
        motion.backgroundMotionEnabled = false;
        compare(motion.ambientEnabled, false);
        compare(motion.transitionsEnabled, true);
        motion.backgroundMotionEnabled = true;
    }
    function test_outgoingPageCannotAct() {
        host.selectPage(2);
        assertSettled(2);
        const advance = findChild(host, "advanceButton");
        advance.forceActiveFocus();
        host.selectPage(1);
        keyClick(Qt.Key_Space);
        compare(state.demoProgress, 25);
        assertSettled(1);
        keyClick(Qt.Key_Return);
        compare(state.demoProgress, 25);
    }
}
