import QtQuick
import QtQuick.Layouts
import FolderSnap

ColumnLayout {
    property string eyebrow: "YOUR WORKSPACE"
    property string title: ""
    property string subtitle: ""
    spacing: 7
    LabelText {
        text: parent.eyebrow
        color: Theme.accent
        font.pixelSize: 9
        font.letterSpacing: 1.8
        font.weight: Font.DemiBold
    }
    LabelText {
        text: parent.title
        font.pixelSize: 30
        font.weight: Font.DemiBold
        font.letterSpacing: -0.8
        Layout.fillWidth: true
    }
    LabelText {
        text: parent.subtitle
        color: Theme.secondary
        font.pixelSize: 12
        Layout.fillWidth: true
    }
}
