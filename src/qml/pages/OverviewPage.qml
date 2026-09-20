import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FolderSnap

ScrollView {
    id: page
    required property AppState appState
    required property MotionPolicy motion
    contentWidth: availableWidth
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ColumnLayout {
        width: page.availableWidth
        spacing: 18
        RowLayout {
            Layout.fillWidth: true
            PageHeading {
                Layout.fillWidth: true
                eyebrow: "THE BIG PICTURE"
                title: "Everything, in perspective."
                subtitle: "A clear view of your folders and how they change."
            }
            ActionButton {
                animationsEnabled: page.motion.transitionsEnabled
                text: "Add folder"
                glyph: "plus"
                primary: false
                onClicked: page.appState.openSheet("add")
            }
        }
        Panel {
            visible: page.appState.scanError !== ""
            Layout.fillWidth: true
            implicitHeight: 58
            color: "#382c2a"
            border.color: "#664943"
            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                Glyph {
                    name: "warning"
                    color: Theme.danger
                }
                BodyText {
                    Layout.fillWidth: true
                    text: page.appState.scanError
                    font.pixelSize: 11
                    color: Theme.danger
                }
                ActionButton {
                    text: "Retry"
                    primary: false
                    animationsEnabled: page.motion.transitionsEnabled
                    onClicked: page.appState.takeSnapshot()
                }
            }
        }
        EmptyState {
            visible: page.appState.visibleRoots.length === 0
            Layout.fillWidth: true
            onTriggered: page.appState.openSheet("add")
        }
        Panel {
            visible: page.appState.visibleRoots.length > 0
            Layout.fillWidth: true
            implicitHeight: 184
            color: "#253c37"
            border.color: "#435c51"
            clip: true
            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: 14
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop {
                        position: 0
                        color: "#2b443b"
                    }
                    GradientStop {
                        position: 0.62
                        color: "#293d3e"
                    }
                    GradientStop {
                        position: 1
                        color: "#343a50"
                    }
                }
            }
            SnapshotArtwork {
                anchors.right: parent.right
                anchors.rightMargin: 22
                anchors.verticalCenter: parent.verticalCenter
                width: 300
                height: 180
                visible: parent.width > 800
            }
            ColumnLayout {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 22
                width: parent.width > 800 ? parent.width - 375 : parent.width - 52
                spacing: 9
                LabelText {
                    text: "KNOW WHAT CHANGED"
                    font.pixelSize: 9
                    font.letterSpacing: 1.7
                    color: "#afcbbc"
                }
                LabelText {
                    text: "Your files move forward.\nKeep a little history."
                    font.pixelSize: 25
                    font.weight: Font.DemiBold
                    font.letterSpacing: -0.6
                    Layout.fillWidth: true
                }
                LabelText {
                    text: "Capture a moment. Compare changes. Stay in control."
                    font.pixelSize: 11
                    color: "#bfd0c9"
                    Layout.fillWidth: true
                }
            }
        }
        RowLayout {
            visible: page.appState.visibleRoots.length > 0
            Layout.fillWidth: true
            spacing: 0
            Repeater {
                model: [
                    {
                        label: "WATCHED FOLDERS",
                        value: page.appState.visibleRoots.length.toString().padStart(2, "0"),
                        note: page.appState.activeRootCount + " active · " + (page.appState.visibleRoots.length - page.appState.activeRootCount) + " archived",
                        glyph: "folder",
                        tone: "#94e6c6"
                    },
                    {
                        label: "SAVED SNAPSHOTS",
                        value: page.appState.totalSnapshotCount.toLocaleString(),
                        note: "Across all folders",
                        glyph: "snapshot",
                        tone: "#b7a8e6"
                    },
                    {
                        label: "FILES IN VIEW",
                        value: page.appState.totalFileCount.toLocaleString(),
                        note: "Metadata, not copies",
                        glyph: "file",
                        tone: "#b6c5dd"
                    },
                    {
                        label: "NEXT SNAPSHOT",
                        value: "—",
                        note: "Manual snapshots only",
                        glyph: "clock",
                        tone: "#e9c387"
                    }
                ]
                Item {
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    implicitHeight: 78
                    Rectangle {
                        width: 1
                        height: 61
                        y: 8
                        color: Theme.border
                        visible: parent.index > 0
                    }
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.leftMargin: parent.index === 0 ? 0 : 22
                        spacing: 6
                        LabelText {
                            text: modelData.label
                            font.pixelSize: 9
                            color: Theme.muted
                            font.letterSpacing: 0.8
                            Layout.fillWidth: true
                        }
                        RowLayout {
                            LabelText {
                                text: modelData.value
                                font.pixelSize: 27
                                font.weight: Font.DemiBold
                                font.letterSpacing: -1
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                        }
                        LabelText {
                            text: modelData.note
                            font.pixelSize: 10
                            color: Theme.secondary
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }
        RowLayout {
            visible: page.appState.visibleRoots.length > 0
            Layout.fillWidth: true
            LabelText {
                text: "Your folders"
                font.pixelSize: 17
                font.weight: Font.DemiBold
            }
            Badge {
                text: page.appState.activeRootCount + " active"
                tone: Theme.muted
            }
            Item {
                Layout.fillWidth: true
            }
            ActionButton {
                animationsEnabled: page.motion.transitionsEnabled
                text: "View all"
                glyph: "arrow"
                primary: false
                quiet: true
                implicitHeight: 28
                onClicked: page.appState.selectedSection = AppState.Folders
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 14
            Repeater {
                model: page.appState.visibleRoots.slice(0, 3)
                FolderCard {
                    required property int index
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    root: modelData
                    order: index
                    onOpened: {
                        page.appState.chooseRoot(index);
                        page.appState.selectedSection = AppState.Folders;
                    }
                }
            }
        }
        Panel {
            visible: page.appState.visibleRoots.length > 0
            Layout.fillWidth: true
            implicitHeight: 101
            RowLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 17
                Rectangle {
                    width: 38
                    height: 38
                    radius: 12
                    color: "#2e3443"
                    Glyph {
                        anchors.centerIn: parent
                        name: "compare"
                        color: Theme.violet
                        font.pixelSize: 18
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    LabelText {
                        text: "Every change has a story."
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                    }
                    LabelText {
                        text: "Choose two snapshots to see what arrived, changed, or left."
                        font.pixelSize: 11
                        color: Theme.secondary
                        Layout.fillWidth: true
                    }
                }
                ActionButton {
                    animationsEnabled: page.motion.transitionsEnabled
                    text: "Compare snapshots"
                    primary: false
                    glyph: "arrow"
                    onClicked: page.appState.selectedSection = AppState.Compare
                }
            }
        }
        LabelText {
            Layout.fillWidth: true
            text: "Snapshots remember file details, not file contents. Your originals stay right where they are."
            font.pixelSize: 10
            color: Theme.muted
            Layout.bottomMargin: 4
        }
    }
}
