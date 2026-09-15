import QtQuick
import QtTest
import Aura

TestCase {
    name: "Motion"
    visible: true
    when: windowShown
    width: 200
    height: 200
    MotionPolicy {
        id: policy
    }
    ProgressRing {
        id: ring
        width: 180
        height: 180
        animationsEnabled: policy.transitionsEnabled
    }
    function test_reduceActiveProgress() {
        ring.value = 100;
        wait(30);
        verify(ring.displayedValue < 100);
        policy.reducedMotion = true;
        compare(ring.displayedValue, 100);
        ring.value = 25;
        compare(ring.displayedValue, 25);
    }
}
