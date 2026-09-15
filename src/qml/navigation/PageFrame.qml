import QtQuick
import FolderSnap

FocusScope {
    id: frame
    property real targetOpacity: 0
    property real targetOffset: 0
    property int duration: Theme.pageDuration
    readonly property bool animating: animation.running
    signal finished
    function stop() {
        animation.stop();
    }
    function animateTo(opacityValue, offsetValue, durationValue) {
        animation.stop();
        targetOpacity = opacityValue;
        targetOffset = offsetValue;
        duration = durationValue;
        animation.start();
    }
    ParallelAnimation {
        id: animation
        NumberAnimation {
            target: frame
            property: "opacity"
            to: frame.targetOpacity
            duration: frame.duration
            easing.type: Theme.easing
        }
        NumberAnimation {
            target: frame
            property: "x"
            to: frame.targetOffset
            duration: frame.duration
            easing.type: Theme.easing
        }
        onFinished: frame.finished()
    }
}
