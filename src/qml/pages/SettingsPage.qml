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
            title: "Make yourself comfortable."
            subtitle: "A little less movement, or a little more atmosphere. Your choice."
        }
        DemoCard {
            Layout.fillWidth: true
            implicitHeight: settingsContent.implicitHeight + 48
            ColumnLayout {
                id: settingsContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.padding
                spacing: Theme.padding
                Text {
                    text: "MOTION & ATMOSPHERE"
                    color: Theme.accent
                    font.family: Theme.fontFamily
                    font.pixelSize: 10
                    font.letterSpacing: 1.5
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.padding
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        BodyText {
                            text: "Reduced motion"
                            color: Theme.text
                            font.weight: Font.DemiBold
                        }
                        BodyText {
                            Layout.fillWidth: true
                            text: "Keep things still. Switch pages instantly and stop decorative movement."
                            font.pixelSize: 12
                        }
                    }
                    ToggleSwitch {
                        objectName: "reducedMotionToggle"
                        text: "Reduced motion"
                        checked: page.appState.reducedMotion
                        animationsEnabled: page.motion.transitionsEnabled
                        onToggled: page.appState.reducedMotion = checked
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: Theme.border
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.padding
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        BodyText {
                            text: "Background motion"
                            color: Theme.text
                            font.weight: Font.DemiBold
                        }
                        BodyText {
                            Layout.fillWidth: true
                            text: page.appState.reducedMotion ? "Paused while reduced motion is on. Your preference is kept." : "Let the ambient colors drift gently behind your space."
                            font.pixelSize: 12
                        }
                    }
                    ToggleSwitch {
                        text: "Background motion"
                        checked: page.appState.backgroundMotionEnabled
                        animationsEnabled: page.motion.transitionsEnabled
                        onToggled: page.appState.backgroundMotionEnabled = checked
                    }
                }
            }
        }
        DemoCard {
            Layout.fillWidth: true
            implicitHeight: aboutContent.implicitHeight + 48
            color: "#252338"
            ColumnLayout {
                id: aboutContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.padding
                spacing: Theme.medium
                BodyText {
                    text: "A small experiment in feeling good."
                    color: Theme.text
                    font.pixelSize: 20
                    font.weight: Font.Light
                }
                BodyText {
                    Layout.fillWidth: true
                    text: "FolderSnap is currently an interface preview. Everything shown is mock content, and every action stays inside this window."
                }
                BodyText {
                    Layout.fillWidth: true
                    text: "Preferences and demo progress reset when you close the app."
                    color: Theme.muted
                    font.pixelSize: 12
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            BodyText {
                text: "AURA"
                font.pixelSize: 10
                font.letterSpacing: 2
                color: Theme.muted
            }
            Item {
                Layout.fillWidth: true
            }
            BodyText {
                text: "INTERFACE EXPLORATION  /  01"
                font.pixelSize: 10
                color: Theme.muted
            }
        }
    }
}
