import QtQuick
import QtQuick.Controls
import FolderSnap

TextField {
    id: control
    implicitHeight: 38
    leftPadding: 36
    rightPadding: 12
    color: Theme.text
    selectionColor: "#426d60"
    selectedTextColor: Theme.text
    placeholderTextColor: Theme.muted
    placeholderText: "Search by name or path…"
    font.family: Theme.fontFamily
    font.pixelSize: 12
    Accessible.name: placeholderText
    background: Rectangle {
        radius: 8
        color: "#161f23"
        border.color: control.activeFocus ? Theme.accent : Theme.border
    }
    Glyph {
        x: 12
        anchors.verticalCenter: parent.verticalCenter
        visible: control.leftPadding >= 32
        name: "search"
        font.pixelSize: 14
    }
}
