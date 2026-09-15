import QtQuick
import FolderSnap

Item {
    id: bar
    required property Window appWindow
    implicitHeight: Theme.titleHeight
    LabelText {
        anchors.left: parent.left
        anchors.leftMargin: 22
        anchors.verticalCenter: parent.verticalCenter
        text: "FOLDERSNAP"
        color: Theme.muted
        font.pixelSize: 9
        font.letterSpacing: 2
    }
    LabelText {
        anchors.centerIn: parent
        text: "A quieter way to keep track."
        color: "#74868b"
        font.pixelSize: 10
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
                if (bar.appWindow.visibility === Window.Maximized)
                    bar.appWindow.showNormal();
                else
                    bar.appWindow.showMaximized();
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
