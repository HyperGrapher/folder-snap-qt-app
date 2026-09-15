import QtQuick
import FolderSnap

Window {
    id: window
    required property AppState appState
    property var windowController: null
    width: Math.min(1100, Screen.desktopAvailableWidth)
    height: Math.min(720, Screen.desktopAvailableHeight)
    minimumWidth: Math.min(860, Screen.desktopAvailableWidth)
    minimumHeight: Math.min(600, Screen.desktopAvailableHeight)
    visible: false
    title: "FolderSnap"
    color: Theme.background
    flags: Qt.Window | Qt.FramelessWindowHint
    MotionPolicy {
        id: motion
        windowVisible: window.visible
        windowExposed: window.windowController ? window.windowController.exposed : false
        windowMinimized: window.visibility === Window.Minimized
        reducedMotion: window.appState.reducedMotion
        backgroundMotionEnabled: window.appState.backgroundMotionEnabled
    }
    AmbientBackground {
        anchors.fill: parent
        motion: motion
        section: window.appState.selectedSection
    }
    Rectangle {
        anchors.fill: parent
        color: "#8810121c"
    }
    TitleBar {
        id: titleBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Theme.titleHeight
        appWindow: window
    }
    Sidebar {
        id: sidebar
        anchors.left: parent.left
        anchors.top: titleBar.bottom
        anchors.bottom: parent.bottom
        width: Theme.sidebarWidth
        appState: window.appState
        motion: motion
    }
    Rectangle {
        anchors.left: sidebar.right
        anchors.top: sidebar.top
        anchors.bottom: sidebar.bottom
        width: 1
        color: "#403c4056"
    }
    PageHost {
        id: pageHost
        objectName: "pageHost"
        x: sidebar.width + (window.width - sidebar.width - width) / 2
        width: Math.min(1040, window.width - sidebar.width - 2 * Theme.large)
        anchors.top: titleBar.bottom
        anchors.bottom: parent.bottom
        anchors.topMargin: Theme.large
        anchors.bottomMargin: Theme.large
        appState: window.appState
        motion: motion
    }
    Binding {
        target: window.windowController
        property: "titleHeight"
        value: Theme.titleHeight
        when: window.windowController !== null
    }
    Binding {
        target: window.windowController
        property: "titleButtonsWidth"
        value: 3 * Theme.windowButtonWidth
        when: window.windowController !== null
    }
}
