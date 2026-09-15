import QtQuick
import QtQuick.Layouts
import FolderSnap

Panel {
    id: control
    property string title: "Start with a folder."
    property string message: "Add a folder to give its changes a little history."
    property string actionText: "Add your first folder"
    property string glyph: "folder"
    signal triggered
    implicitHeight: 290
    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 390)
        spacing: 16
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 62
            height: 62
            radius: 20
            color: "#243d37"
            border.color: "#385c50"
            Glyph {
                anchors.centerIn: parent
                name: control.glyph
                color: Theme.accent
                font.pixelSize: 26
            }
        }
        LabelText {
            Layout.fillWidth: true
            text: control.title
            font.pixelSize: 22
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
        }
        BodyText {
            Layout.fillWidth: true
            text: control.message
            horizontalAlignment: Text.AlignHCenter
        }
        ActionButton {
            Layout.alignment: Qt.AlignHCenter
            text: control.actionText
            onClicked: control.triggered()
        }
    }
}
