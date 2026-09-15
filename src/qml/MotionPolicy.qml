import QtQuick

QtObject {
    property bool windowVisible: true
    property bool windowExposed: true
    property bool windowMinimized: false
    property bool reducedMotion: false
    property bool backgroundMotionEnabled: true
    readonly property bool renderable: windowVisible && windowExposed && !windowMinimized
    readonly property bool transitionsEnabled: renderable && !reducedMotion
    readonly property bool ambientEnabled: transitionsEnabled && backgroundMotionEnabled
}
