import QtQuick
import QtQuick.Layouts
import FolderSnap

Item {
    id: artwork
    // Small code-native illustration: three saved moments become a visible history.
    Repeater {
        model: 3
        Rectangle {
            required property int index
            x: 30 + index * 20
            y: 40 - index * 14
            width: artwork.width - 90
            height: 130
            radius: 12
            rotation: -10 + index * 6
            color: index === 2 ? "#314642" : index === 1 ? "#2d3847" : "#343346"
            border.color: index === 2 ? "#628376" : "#5b6374"
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 17
                spacing: 9
                RowLayout {
                    Glyph {
                        name: "snapshot"
                        color: Theme.accent
                    }
                    LabelText {
                        text: "FOLDERSNAP"
                        font.pixelSize: 9
                        font.letterSpacing: 1.3
                        color: "#d4e3de"
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Rectangle {
                        width: 6
                        height: 6
                        radius: 3
                        color: Theme.accent
                    }
                }
                LabelText {
                    text: "A moment, saved."
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                }
                Rectangle {
                    height: 1
                    Layout.fillWidth: true
                    color: "#526961"
                }
                RowLayout {
                    LabelText {
                        text: "metadata only"
                        font.pixelSize: 10
                        color: "#b8cac4"
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    LabelText {
                        text: "local history"
                        font.pixelSize: 10
                        color: Theme.accent
                    }
                }
            }
        }
    }
    Badge {
        x: 0
        y: 137
        text: "✓   Everything in perspective"
        tone: Theme.accent
        rotation: -3
    }
}
