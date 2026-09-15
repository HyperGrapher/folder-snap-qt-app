import QtQuick
import FolderSnap
import QtQuick.Controls

Button {
    id: control
    objectName: "window-" + glyph
    required property string glyph
    property bool closeButton: false
    implicitWidth: Theme.windowButtonWidth
    implicitHeight: Theme.titleHeight
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: text
    ToolTip.visible: hovered
    ToolTip.delay: 600
    ToolTip.text: text
    contentItem: Item {
        Image {
            anchors.centerIn: parent
            width: 14
            height: 14
            source: "qrc:/resources/icons/" + control.glyph + ".svg"
        }
    }
    background: Rectangle {
        color: control.closeButton && control.hovered ? "#c84e68" : control.down ? "#424359" : control.hovered ? "#2c2e41" : "transparent"
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
    }
    Keys.onReturnPressed: clicked()
}
