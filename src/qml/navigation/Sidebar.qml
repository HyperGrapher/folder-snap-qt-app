import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FolderSnap

Rectangle {
    id: sidebar
    required property AppState appState
    required property MotionPolicy motion
    color: Theme.sidebar
    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: "#293338"
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 0
        RowLayout {
            Layout.topMargin: 12
            Layout.bottomMargin: 34
            spacing: 10
            Image {
                source: "qrc:/resources/icons/foldersnap-icon.png"
                sourceSize: Qt.size(80, 80)
                Layout.preferredWidth: 38
                Layout.preferredHeight: 38
            }
            ColumnLayout {
                spacing: 1
                LabelText {
                    text: "FolderSnap"
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                    font.letterSpacing: -0.7
                }
                LabelText {
                    text: "A little history. More clarity."
                    font.pixelSize: 9
                    color: Theme.muted
                }
            }
        }
        LabelText {
            text: "WORKSPACE"
            font.pixelSize: 9
            font.letterSpacing: 1.6
            color: Theme.muted
            Layout.leftMargin: 12
            Layout.bottomMargin: 12
        }
        Repeater {
            model: [
                {
                    label: "Overview",
                    glyph: "overview"
                },
                {
                    label: "Folders",
                    glyph: "folder"
                },
                {
                    label: "Compare",
                    glyph: "compare"
                },
                {
                    label: "Settings",
                    glyph: "settings"
                }
            ]
            Button {
                id: nav
                required property int index
                required property var modelData
                readonly property bool selected: sidebar.appState.selectedSection === index
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                Layout.bottomMargin: 5
                hoverEnabled: true
                focusPolicy: Qt.StrongFocus
                Accessible.name: modelData.label
                onClicked: sidebar.appState.selectedSection = index
                background: Rectangle {
                    radius: 8
                    color: nav.selected ? "#293c36" : nav.hovered ? "#1e2a2c" : "transparent"
                    border.color: nav.visualFocus ? Theme.accent : nav.selected ? "#3b564a" : "transparent"
                    Rectangle {
                        width: 3
                        height: 16
                        radius: 1.5
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.accent
                        visible: nav.selected
                    }
                    Behavior on color {
                        ColorAnimation {
                            duration: sidebar.motion.transitionsEnabled ? 120 : 0
                        }
                    }
                }
                contentItem: RowLayout {
                    spacing: 12
                    Glyph {
                        Layout.leftMargin: 8
                        name: nav.modelData.glyph
                        color: nav.selected ? Theme.accent : Theme.muted
                        font.pixelSize: 17
                    }
                    LabelText {
                        text: nav.modelData.label
                        color: nav.selected ? Theme.text : Theme.secondary
                        font.weight: nav.selected ? Font.DemiBold : Font.Normal
                        Layout.fillWidth: true
                    }
                    LabelText {
                        visible: nav.index === 1
                        text: sidebar.appState.visibleRoots.length
                        font.pixelSize: 10
                        color: Theme.muted
                        Layout.rightMargin: 8
                    }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#293338"
            Layout.topMargin: 19
            Layout.bottomMargin: 23
        }
        LabelText {
            text: "YOUR FOLDERS"
            font.pixelSize: 9
            font.letterSpacing: 1.6
            color: Theme.muted
            Layout.leftMargin: 12
            Layout.bottomMargin: 12
        }
        Repeater {
            model: sidebar.appState.visibleRoots.slice(0, 4)
            Button {
                id: rootButton
                required property int index
                required property var modelData
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                hoverEnabled: true
                Accessible.name: "Open " + modelData.name
                onClicked: {
                    sidebar.appState.chooseRoot(index);
                    sidebar.appState.selectedSection = AppState.Folders;
                }
                background: Rectangle {
                    radius: 6
                    color: rootButton.hovered ? "#202c2e" : "transparent"
                    border.color: rootButton.visualFocus ? Theme.accent : "transparent"
                }
                contentItem: RowLayout {
                    spacing: 10
                    Rectangle {
                        Layout.leftMargin: 13
                        width: 5
                        height: 5
                        radius: 3
                        color: rootButton.modelData.color
                    }
                    LabelText {
                        text: rootButton.modelData.name
                        color: Theme.secondary
                        font.pixelSize: 11
                        Layout.fillWidth: true
                    }
                    Glyph {
                        visible: rootButton.modelData.archived
                        name: "archive"
                        font.pixelSize: 11
                        Layout.rightMargin: 8
                    }
                }
            }
        }
        ActionButton {
            animationsEnabled: sidebar.motion.transitionsEnabled
            Layout.fillWidth: true
            Layout.topMargin: 6
            primary: false
            quiet: true
            glyph: "plus"
            text: "Add folder"
            onClicked: sidebar.appState.openSheet("add")
        }
        Item {
            Layout.fillHeight: true
            Layout.minimumHeight: 18
        }
        Panel {
            Layout.fillWidth: true
            implicitHeight: 87
            color: "#1b2928"
            border.color: "#2d413b"
            visible: sidebar.height > 640
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 13
                spacing: 6
                RowLayout {
                    Glyph {
                        name: "shield"
                        color: Theme.accent
                        font.pixelSize: 13
                    }
                    LabelText {
                        text: "Only on your device"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }
                }
                BodyText {
                    Layout.fillWidth: true
                    text: "Your folders. Your history.\nNo cloud, no accounts."
                    font.pixelSize: 10
                    color: Theme.muted
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 20
            Layout.leftMargin: 10
            Layout.bottomMargin: 5
            Rectangle {
                width: 5
                height: 5
                radius: 3
                color: Theme.accent
            }
            LabelText {
                text: "UI PREVIEW"
                color: Theme.muted
                font.pixelSize: 9
                font.letterSpacing: 1
            }
            Item {
                Layout.fillWidth: true
            }
            LabelText {
                text: "0.1"
                font.pixelSize: 10
                color: Theme.muted
            }
        }
    }
}
