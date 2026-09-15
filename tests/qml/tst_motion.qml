import QtQuick
import QtTest
import FolderSnap

TestCase {
    name: "Motion"
    visible: true
    when: windowShown
    width: 200
    height: 200
    MotionPolicy {
        id: policy
    }
    function init() {
        policy.windowVisible = true;
        policy.windowExposed = true;
        policy.windowMinimized = false;
        policy.reducedMotion = false;
        policy.backgroundMotionEnabled = true;
    }
    function test_motionPreferences() {
        verify(policy.transitionsEnabled);
        verify(policy.ambientEnabled);
        policy.reducedMotion = true;
        verify(!policy.transitionsEnabled);
        verify(!policy.ambientEnabled);
        policy.reducedMotion = false;
        policy.backgroundMotionEnabled = false;
        verify(policy.transitionsEnabled);
        verify(!policy.ambientEnabled);
        policy.windowMinimized = true;
        verify(!policy.transitionsEnabled);
    }
}
