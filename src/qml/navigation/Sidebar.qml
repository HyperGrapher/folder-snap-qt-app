pragma ComponentBehavior: Bound
import QtQuick
import Aura
import QtQuick.Layouts

Rectangle {
    id: sidebar
    required property AppState appState
    required property MotionPolicy motion
    color: Theme.sidebar
    Connections {
        target: sidebar.motion
        function onTransitionsEnabledChanged() {
            if (!sidebar.motion.transitionsEnabled) {
                selectionAnimation.complete();
            }
        }
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 0
        RowLayout {
            Layout.topMargin: 24
            Layout.leftMargin: 10
            spacing: 12
            Rectangle {
                Layout.preferredWidth: 38
                Layout.preferredHeight: 38
                radius: 13
                gradient: Gradient {
                    GradientStop {
                        position: 0
                        color: "#c7b4ff"
                    }
                    GradientStop {
                        position: 1
                        color: "#7564c5"
                    }
                }
                Image {
                    anchors.centerIn: parent
                    width: 23
                    height: 23
                    source: "qrc:/resources/icons/spark.svg"
                }
            }
            Text {
                text: "aura"
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: 28
                font.weight: Font.DemiBold
                font.letterSpacing: -1
            }
        }
        Text {
            Layout.topMargin: 43
            Layout.leftMargin: 14
            Layout.bottomMargin: 14
            text: "YOUR SPACE"
            color: Theme.muted
            font.family: Theme.fontFamily
            font.pixelSize: 9
            font.letterSpacing: 1.5
        }
        Item {
            Layout.fillWidth: true
            implicitHeight: 4 * 56
            Rectangle {
                width: parent.width
                height: 48
                radius: Theme.controlRadius
                y: sidebar.appState.selectedSection * 56
                color: "#302b48"
                border.color: "#49415f"
                Behavior on y {
                    NumberAnimation {
                        id: selectionAnimation
                        duration: sidebar.motion.transitionsEnabled ? Theme.pageDuration : 0
                        easing.type: Theme.easing
                    }
                }
            }
            Column {
                anchors.fill: parent
                spacing: 8
                Repeater {
                    model: ["Overview", "Collection", "Activity", "Settings"]
                    NavigationButton {
                        required property int index
                        required property string modelData
                        width: parent.width
                        text: modelData
                        glyph: modelData.toLowerCase()
                        selected: sidebar.appState.selectedSection === index
                        onClicked: sidebar.appState.selectedSection = index
                    }
                }
            }
        }
        Item {
            Layout.fillHeight: true
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 98
            visible: sidebar.height >= 540
            radius: Theme.controlRadius
            color: "#1e2231"
            border.color: "#2c3143"
            Column {
                anchors.fill: parent
                anchors.margins: 15
                spacing: 8
                Text {
                    text: "Made for the moment"
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
                BodyText {
                    width: parent.width
                    text: "A small exploration of color, motion & interaction."
                    font.pixelSize: 11
                }
            }
        }
        RowLayout {
            Layout.topMargin: 22
            Layout.bottomMargin: 6
            Layout.leftMargin: 10
            Layout.rightMargin: 8
            Rectangle {
                Layout.preferredWidth: 6
                Layout.preferredHeight: 6
                radius: 3
                color: Theme.success
            }
            Text {
                text: "INTERFACE DEMO"
                color: Theme.muted
                font.family: Theme.fontFamily
                font.pixelSize: 9
                font.letterSpacing: 1
            }
            Item {
                Layout.fillWidth: true
            }
            Text {
                text: "01"
                color: Theme.muted
                font.family: Theme.fontFamily
                font.pixelSize: 10
            }
        }
    }
}
