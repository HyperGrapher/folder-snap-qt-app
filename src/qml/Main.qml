import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FolderSnap

Window {
    id: window
    property alias appState: preview
    property var windowController: null
    width: Math.min(1280, Screen.desktopAvailableWidth)
    height: Math.min(840, Screen.desktopAvailableHeight)
    minimumWidth: Math.min(960, Screen.desktopAvailableWidth)
    minimumHeight: Math.min(680, Screen.desktopAvailableHeight)
    visible: false
    title: "FolderSnap"
    color: Theme.background
    flags: Qt.Window | Qt.FramelessWindowHint
    UiPreviewState {
        id: preview
    }
    MotionPolicy {
        id: motion
        windowVisible: window.visible
        windowExposed: window.windowController ? window.windowController.exposed : false
        windowMinimized: window.visibility === Window.Minimized
        reducedMotion: preview.reducedMotion
        backgroundMotionEnabled: preview.backgroundMotionEnabled
    }
    AmbientBackground {
        anchors.fill: parent
        motion: motion
        section: preview.selectedSection
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
        appState: preview
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
        appState: preview
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
                name: preview.scanning ? "snapshot" : "shield"
                font.pixelSize: 11
                color: Theme.accent
            }
            LabelText {
                text: preview.scanning ? "Taking a sample snapshot… " + preview.scanProgress + "%" : "Local by design"
                font.pixelSize: 10
                color: Theme.secondary
            }
            ActionButton {
                visible: preview.scanning
                text: "Cancel"
                primary: false
                quiet: true
                implicitHeight: 24
                onClicked: {
                    preview.scanning = false;
                    preview.toast = "Sample scan cancelled.";
                }
            }
            Item {
                Layout.fillWidth: true
            }
            LabelText {
                text: "Sample data · No files are changed"
                font.pixelSize: 10
                color: Theme.muted
            }
        }
    }
    PreviewDialog {
        id: previewDialog
        objectName: "previewDialog"
        appState: preview
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
        visible: preview.toast !== ""
        z: 100
        LabelText {
            id: toastText
            anchors.fill: parent
            anchors.margins: 16
            text: preview.toast
            font.pixelSize: 12
        }
        TapHandler {
            onTapped: preview.toast = ""
        }
    }
    Timer {
        interval: 220
        repeat: true
        running: preview.scanning
        onTriggered: {
            preview.scanProgress = Math.min(100, preview.scanProgress + 8);
            if (preview.scanProgress === 100) {
                if (preview.scenario === "Scan failure") {
                    preview.scanning = false;
                    preview.scanError = "Projects is unavailable. Reconnect the drive or check folder permissions, then try again.";
                    return;
                }
                preview.scanning = false;
                preview.toast = "Sample snapshot complete. Your files have not been scanned.";
            }
        }
    }
    Timer {
        interval: 700
        running: preview.comparing
        onTriggered: {
            preview.comparing = false;
            preview.comparisonReady = true;
        }
    }
    Timer {
        interval: 5000
        running: preview.toast !== ""
        onTriggered: preview.toast = ""
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
