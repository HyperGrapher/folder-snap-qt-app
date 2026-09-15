import QtQuick
import FolderSnap
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: control
    required property string glyph
    property bool selected: false
    implicitHeight: 48
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: text
    Accessible.role: Accessible.PageTab
    Accessible.selected: selected
    contentItem: RowLayout {
        spacing: Theme.medium
        Image {
            Layout.leftMargin: Theme.medium
            source: "qrc:/resources/icons/" + control.glyph + ".svg"
            sourceSize: Qt.size(20, 20)
            opacity: control.selected ? 1 : 0.65
        }
        Text {
            Layout.fillWidth: true
            text: control.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.bodySize
            font.weight: control.selected ? Font.DemiBold : Font.Normal
            color: control.selected ? Theme.text : Theme.secondary
        }
        Rectangle {
            Layout.rightMargin: 12
            Layout.preferredWidth: 5
            Layout.preferredHeight: 5
            radius: 3
            color: Theme.accent
            visible: control.selected
        }
    }
    background: Rectangle {
        radius: Theme.controlRadius
        color: control.down ? "#304051" : control.hovered && !control.selected ? "#202536" : "transparent"
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
    }
    Keys.onReturnPressed: clicked()
}
