import QtQuick
import QtQuick.Layouts
import FolderSnap

RowLayout {
    id: row
    property string title
    property string description
    property alias checked: toggle.checked
    property bool animationsEnabled: true
    property alias toggleObjectName: toggle.objectName
    signal toggled(bool value)
    spacing: 24
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 6
        LabelText {
            text: row.title
            font.pixelSize: 13
            font.weight: Font.DemiBold
            Layout.fillWidth: true
        }
        BodyText {
            text: row.description
            font.pixelSize: 11
            color: Theme.secondary
            Layout.fillWidth: true
        }
    }
    ToggleSwitch {
        id: toggle
        text: row.title
        animationsEnabled: row.animationsEnabled
        onToggled: row.toggled(checked)
    }
}
