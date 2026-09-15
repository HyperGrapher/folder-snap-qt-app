import QtQuick
import FolderSnap

Item {
    id: bar
    required property Window appWindow
    implicitHeight: Theme.titleHeight
    Text {
        anchors.left: parent.left
        anchors.leftMargin: 26
        anchors.verticalCenter: parent.verticalCenter
        text: "AURA  /  A LITTLE SPACE TO EXPLORE"
        color: Theme.muted
        font.family: Theme.fontFamily
        font.pixelSize: 9
        font.letterSpacing: 1.8
    }
    Row {
        anchors.right: parent.right
        WindowButton {
            text: "Minimize"
            glyph: "minimize"
            onClicked: bar.appWindow.showMinimized()
        }
        WindowButton {
            text: bar.appWindow.visibility === Window.Maximized ? "Restore" : "Maximize"
            glyph: bar.appWindow.visibility === Window.Maximized ? "restore" : "maximize"
            onClicked: {
                if (bar.appWindow.visibility === Window.Maximized) {
                    bar.appWindow.showNormal();
                } else {
                    bar.appWindow.showMaximized();
                }
            }
        }
        WindowButton {
            text: "Close"
            glyph: "close"
            closeButton: true
            onClicked: bar.appWindow.close()
        }
    }
}
