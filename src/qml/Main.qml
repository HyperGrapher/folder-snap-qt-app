import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FolderSnap

Window {
    id: window
    property alias appState: state
    property var windowController: null
    width: Math.min(1280, Screen.desktopAvailableWidth)
    height: Math.min(840, Screen.desktopAvailableHeight)
    minimumWidth: Math.min(960, Screen.desktopAvailableWidth)
    minimumHeight: Math.min(680, Screen.desktopAvailableHeight)
    visible: false
    title: "FolderSnap"
    color: Theme.background
    flags: Qt.Window | Qt.FramelessWindowHint
    AppState {
        id: state
    }
    MotionPolicy {
        id: motion
        windowVisible: window.visible
        windowExposed: window.windowController ? window.windowController.exposed : false
        windowMinimized: window.visibility === Window.Minimized
        reducedMotion: state.reducedMotion
        backgroundMotionEnabled: state.backgroundMotionEnabled
    }
    AmbientBackground {
        anchors.fill: parent
        motion: motion
        section: state.selectedSection
        opacity: 0.55
    }
    Rectangle {
        anchors.fill: parent
        color: "#66101416"
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
        appState: state
        motion: motion
    }
    PageHost {
        id: pageHost
        objectName: "pageHost"
        x: sidebar.width + (window.width - sidebar.width - width) / 2
        width: Math.min(1200, window.width - sidebar.width - 56)
        anchors.top: titleBar.bottom
        anchors.bottom: footer.top
        anchors.topMargin: 27
        anchors.bottomMargin: 16
        appState: state
        motion: motion
    }
    Rectangle {
        id: footer
        anchors.left: sidebar.right
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 31
        color: "#9913191b"
        Rectangle {
            width: parent.width
            height: 1
            color: "#243237"
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 28
            anchors.rightMargin: 28
            Glyph {
                name: state.scanning ? "snapshot" : "shield"
                font.pixelSize: 11
                color: Theme.accent
            }
            LabelText {
                text: state.scanning ? "Scanning folder… " + state.scanProgress + "%" : "Local by design"
                font.pixelSize: 10
                color: Theme.secondary
            }
            ActionButton {
                visible: state.scanning
                text: "Cancel"
                primary: false
                quiet: true
                implicitHeight: 24
                onClicked: {
                    state.cancelScan();
                }
            }
            Item {
                Layout.fillWidth: true
            }
            LabelText {
                text: "Local metadata · Originals untouched"
                font.pixelSize: 10
                color: Theme.muted
            }
        }
    }
    PreviewDialog {
        id: previewDialog
        objectName: "previewDialog"
        appState: state
        motion: motion
        parent: Overlay.overlay
    }
    Rectangle {
        anchors.bottom: footer.top
        anchors.bottomMargin: 16
        anchors.horizontalCenter: pageHost.horizontalCenter
        width: Math.min(pageHost.width, toastText.implicitWidth + 64)
        height: 46
        radius: 10
        color: "#30443d"
        border.color: "#587667"
        visible: state.toast !== ""
        z: 100
        LabelText {
            id: toastText
            anchors.fill: parent
            anchors.margins: 16
            text: state.toast
            font.pixelSize: 12
        }
        TapHandler {
            onTapped: state.toast = ""
        }
    }
    Timer {
        interval: 5000
        running: state.toast !== ""
        onTriggered: state.toast = ""
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
