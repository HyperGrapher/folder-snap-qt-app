import QtQuick
import FolderSnap
import QtQuick.Layouts

DemoCard {
    id: card
    property int status: 0
    readonly property var labels: ["Ready when you are", "Finding its rhythm", "Beautifully complete"]
    readonly property var descriptions: ["A little space for your next idea.", "Your demo is moving along.", "One small moment, nicely finished."]
    readonly property color statusColor: status === 2 ? Theme.success : status === 1 ? Theme.accent : "#91b7f3"
    implicitHeight: 112
    onAnimationsEnabledChanged: {
        if (!animationsEnabled) {
            statusAnimation.complete();
        }
    }
    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.padding
        spacing: Theme.gap
        Rectangle {
            Layout.preferredWidth: 40
            Layout.preferredHeight: 40
            radius: 13
            color: Qt.rgba(card.statusColor.r, card.statusColor.g, card.statusColor.b, 0.12)
            Rectangle {
                anchors.centerIn: parent
                width: 12
                height: 12
                radius: 6
                color: card.statusColor
                Behavior on color {
                    ColorAnimation {
                        id: statusAnimation
                        duration: card.animationsEnabled ? Theme.hoverDuration : 0
                    }
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 5
            BodyText {
                Layout.fillWidth: true
                text: card.labels[card.status]
                color: Theme.text
                font.weight: Font.DemiBold
            }
            BodyText {
                Layout.fillWidth: true
                text: card.descriptions[card.status]
                font.pixelSize: 12
            }
        }
    }
}
