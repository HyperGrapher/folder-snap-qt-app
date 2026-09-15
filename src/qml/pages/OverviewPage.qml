import QtQuick
import Aura
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
            title: "A calmer kind of space."
            subtitle: "Less noise. A little more room for what inspires you."
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 286
            radius: 22
            color: "#302c49"
            border.color: "#615372"
            clip: true
            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: 22
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop {
                        position: 0
                        color: "#403253"
                    }
                    GradientStop {
                        position: 0.6
                        color: "#3c365e"
                    }
                    GradientStop {
                        position: 1
                        color: "#4c527e"
                    }
                }
            }
            Item {
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width * 0.38
                height: 230
                opacity: 0.85
                Repeater {
                    model: 6
                    Rectangle {
                        required property int index
                        anchors.centerIn: parent
                        width: 70 + index * 23
                        height: 144 + index * 13
                        radius: width / 2
                        rotation: -34 + index * 13
                        color: "transparent"
                        border.width: 1.5
                        border.color: Qt.rgba(0.79, 0.73, 1, 0.7 - index * 0.07)
                    }
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: 70
                    height: 70
                    radius: 35
                    gradient: Gradient {
                        GradientStop {
                            position: 0
                            color: "#e2d8ff"
                        }
                        GradientStop {
                            position: 0.5
                            color: "#b2a1e3"
                        }
                        GradientStop {
                            position: 1
                            color: "#766eae"
                        }
                    }
                }
                Rectangle {
                    x: parent.width / 2 + 54
                    y: 41
                    width: 10
                    height: 10
                    radius: 5
                    color: "#efe1ff"
                }
            }
            ColumnLayout {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: Theme.large
                width: parent.width * 0.57
                spacing: 12
                Text {
                    text: "A SMALL COLLECTION OF POSSIBILITIES"
                    color: "#d8c9ed"
                    font.family: Theme.fontFamily
                    font.pixelSize: 9
                    font.letterSpacing: 1.2
                }
                Text {
                    Layout.fillWidth: true
                    text: "Find your\nnext little spark."
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: 34
                    font.weight: Font.Light
                    lineHeight: 1.05
                }
                BodyText {
                    Layout.fillWidth: true
                    text: "Explore a collection shaped by color,\ncuriosity, and a sense of flow."
                    color: "#d2cce3"
                    font.pixelSize: 12
                }
                Item {
                    Layout.fillHeight: true
                }
                ActionButton {
                    text: "Explore collection  →"
                    animationsEnabled: page.motion.transitionsEnabled
                    onClicked: page.appState.selectedSection = AppState.Collection
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "A moment at a glance"
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: 17
                font.weight: Font.DemiBold
            }
            Item {
                Layout.fillWidth: true
            }
            Text {
                text: "ALL LOCAL · ALL YOURS"
                color: Theme.muted
                font.family: Theme.fontFamily
                font.pixelSize: 9
                font.letterSpacing: 1
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap
            StatusCard {
                Layout.fillWidth: true
                Layout.preferredWidth: 3
                Layout.preferredHeight: 132
                status: page.appState.demoStatus
                animationsEnabled: page.motion.transitionsEnabled && page.visible
            }
            DemoCard {
                Layout.fillWidth: true
                Layout.preferredWidth: 2
                implicitHeight: 132
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 14
                    ProgressRing {
                        Layout.preferredWidth: 84
                        Layout.preferredHeight: 84
                        value: page.appState.demoProgress
                        animationsEnabled: page.motion.transitionsEnabled && page.visible
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        BodyText {
                            Layout.fillWidth: true
                            text: "Little steps,"
                            color: Theme.text
                            font.weight: Font.DemiBold
                        }
                        BodyText {
                            Layout.fillWidth: true
                            text: "lovely progress."
                            font.pixelSize: 12
                        }
                    }
                }
            }
        }
        BodyText {
            Layout.fillWidth: true
            text: "Thoughtfully made. Nothing to connect, nothing to configure."
            font.pixelSize: 11
            color: Theme.muted
            Layout.bottomMargin: 8
        }
    }
}
