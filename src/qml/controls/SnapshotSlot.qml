import QtQuick
import QtQuick.Layouts
import FolderSnap

Panel {
    id: slot
    property string letter: "A"
    property string label: "BEFORE"
    property var snapshot
    property color tone: Theme.violet
    property bool assigned: false
    implicitHeight: 82
    color: assigned ? "#222c32" : "#172125"
    border.color: assigned ? Qt.rgba(tone.r, tone.g, tone.b, 0.45) : Theme.border
    RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12
        Rectangle {
            width: 31
            height: 31
            radius: 8
            color: Qt.rgba(slot.tone.r, slot.tone.g, slot.tone.b, 0.12)
            LabelText {
                anchors.centerIn: parent
                text: slot.letter
                color: slot.tone
                font.weight: Font.DemiBold
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 5
            LabelText {
                text: slot.label
                color: slot.tone
                font.pixelSize: 9
                font.letterSpacing: 1.2
            }
            LabelText {
                text: slot.snapshot.date
                font.pixelSize: 14
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
            LabelText {
                text: slot.snapshot.description
                font.pixelSize: 10
                color: Theme.muted
                Layout.fillWidth: true
            }
        }
    }
}
