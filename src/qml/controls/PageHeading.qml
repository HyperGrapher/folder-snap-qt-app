import QtQuick
import FolderSnap
import QtQuick.Layouts

ColumnLayout {
    property alias title: heading.text
    property alias subtitle: description.text
    spacing: Theme.compact
    Text {
        id: heading
        Layout.fillWidth: true
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.titleSize
        font.weight: Font.DemiBold
        wrapMode: Text.WordWrap
    }
    BodyText {
        id: description
        Layout.fillWidth: true
    }
}
