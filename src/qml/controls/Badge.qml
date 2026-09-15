import QtQuick
import FolderSnap

Rectangle {
    id: badge
    property string text: ""
    property color tone: Theme.accent
    implicitWidth: label.implicitWidth + 16
    implicitHeight: 24
    radius: 6
    color: Qt.rgba(tone.r, tone.g, tone.b, 0.10)
    border.color: Qt.rgba(tone.r, tone.g, tone.b, 0.15)
    LabelText {
        id: label
        anchors.centerIn: parent
        text: badge.text
        font.pixelSize: 10
        font.weight: Font.DemiBold
        color: badge.tone
    }
}
