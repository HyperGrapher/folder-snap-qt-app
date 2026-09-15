import QtQuick
import FolderSnap
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: page
    required property AppState appState
    required property MotionPolicy motion
    contentWidth: availableWidth
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ColumnLayout {
        width: page.availableWidth
        spacing: Theme.padding
        PageHeading {
            Layout.fillWidth: true
            title: "Enjoy the process."
            subtitle: "A few small steps. A satisfying sense of movement."
        }
        DemoCard {
            Layout.fillWidth: true
            implicitHeight: 286
            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.large
                spacing: Theme.large
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.gap
                    Text {
                        text: "ONE STEP AT A TIME"
                        color: Theme.success
                        font.family: Theme.fontFamily
                        font.pixelSize: 10
                        font.letterSpacing: 1.5
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "Small actions.\nVisible progress."
                        color: Theme.text
                        font.family: Theme.fontFamily
                        font.pixelSize: 30
                        font.weight: Font.Light
                    }
                    BodyText {
                        Layout.fillWidth: true
                        text: "Give the ring a gentle nudge. Each step adds a little more to the picture."
                    }
                    ActionButton {
                        objectName: "advanceButton"
                        text: page.appState.demoProgress === 100 ? "All steps complete" : "Advance  +25%"
                        enabled: page.appState.demoProgress < 100
                        animationsEnabled: page.motion.transitionsEnabled && page.visible
                        onClicked: page.appState.advanceProgress()
                    }
                }
                ProgressRing {
                    Layout.preferredWidth: page.availableWidth < 650 ? 150 : 200
                    Layout.preferredHeight: width
                    value: page.appState.demoProgress
                    accent: Theme.success
                    animationsEnabled: page.motion.transitionsEnabled && page.visible
                }
            }
        }
        StatusCard {
            Layout.fillWidth: true
            status: page.appState.demoStatus
            animationsEnabled: page.motion.transitionsEnabled && page.visible
        }
        RowLayout {
            spacing: Theme.medium
            ActionButton {
                text: "Change status"
                primary: false
                animationsEnabled: page.motion.transitionsEnabled
                onClicked: page.appState.cycleStatus()
            }
            ActionButton {
                text: "Start fresh"
                primary: false
                animationsEnabled: page.motion.transitionsEnabled
                onClicked: page.appState.resetDemo()
            }
            Item {
                Layout.fillWidth: true
            }
            BodyText {
                text: "Just a demo. Nothing runs in the background."
                Layout.fillWidth: true
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }
        }
    }
}
