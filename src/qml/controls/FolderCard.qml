import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FolderSnap

Panel {
    id: card
    required property var root
    property int order: 0
    signal opened
    signal snapshotRequested
    implicitHeight: 182
    color: openArea.hovered ? "#24312f" : "#1c282a"
    border.color: openArea.hovered ? "#587365" : Theme.border
    Button {
        id: openArea
        anchors.fill: parent
        hoverEnabled: true
        Accessible.name: "Open " + card.root.name
        onClicked: card.opened()
        background: Rectangle {
            color: "transparent"
            radius: Theme.cardRadius
            border.color: openArea.visualFocus ? Theme.accent : "transparent"
        }
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 9
        RowLayout {
            Rectangle {
                width: 34
                height: 34
                radius: 10
                color: "#2c3c39"
                Glyph {
                    anchors.centerIn: parent
                    name: "folder"
                    color: card.root.color
                    font.pixelSize: 18
                }
            }
            Item {
                Layout.fillWidth: true
            }
            Badge {
                text: card.root.archived ? "Archived" : card.root.snapshots === 0 ? "Not scanned" : "Ready"
                tone: card.root.archived || card.root.snapshots === 0 ? Theme.muted : Theme.accent
            }
        }
        LabelText {
            Layout.fillWidth: true
            text: card.root.name
            font.pixelSize: 17
            font.weight: Font.DemiBold
        }
        LabelText {
            Layout.fillWidth: true
            text: card.root.path
            color: Theme.muted
            font.pixelSize: 10
            elide: Text.ElideMiddle
        }
        RowLayout {
            Layout.fillWidth: true
            LabelText {
                text: card.root.size
                font.pixelSize: 21
                font.weight: Font.DemiBold
                font.letterSpacing: -0.7
            }
            LabelText {
                text: card.root.snapshots + " snapshots"
                font.pixelSize: 10
                color: Theme.muted
                Layout.alignment: Qt.AlignBottom
                Layout.bottomMargin: 4
            }
            Item {
                Layout.fillWidth: true
            }
            Glyph {
                name: "snapshot"
                color: card.root.color
                font.pixelSize: 16
            }
        }
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }
        RowLayout {
            Glyph {
                name: "clock"
                font.pixelSize: 11
                color: Theme.muted
            }
            LabelText {
                text: card.root.schedule
                color: Theme.secondary
                font.pixelSize: 10
                Layout.fillWidth: true
            }
            Glyph {
                name: "arrow"
                font.pixelSize: 11
                color: card.root.color
            }
        }
    }
}
