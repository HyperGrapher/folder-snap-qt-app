import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FolderSnap

ColumnLayout {
    id: page
    required property AppState appState
    required property MotionPolicy motion
    spacing: 22
    RowLayout {
        Layout.fillWidth: true
        PageHeading {
            Layout.fillWidth: true
            eyebrow: "YOUR LIBRARY"
            title: "A place for every moment."
            subtitle: "Watch a folder. Build its history. Come back whenever you need."
        }
        ActionButton {
            animationsEnabled: page.motion.transitionsEnabled
            text: "Add folder"
            glyph: "plus"
            onClicked: page.appState.openSheet("add")
        }
    }
    EmptyState {
        visible: page.appState.visibleRoots.length === 0
        Layout.fillWidth: true
        onTriggered: page.appState.openSheet("add")
    }
    RowLayout {
        visible: page.appState.visibleRoots.length > 0
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 18
        Panel {
            Layout.preferredWidth: page.width > 850 ? 236 : 185
            Layout.fillHeight: true
            color: "#182125"
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                LabelText {
                    text: "FOLDERS  /  " + page.appState.visibleRoots.length
                    font.pixelSize: 9
                    font.letterSpacing: 1.3
                    color: Theme.muted
                    Layout.margins: 8
                }
                ListView {
                    id: folderList
                    objectName: "watchedFoldersList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 5
                    reuseItems: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: page.appState.visibleRoots
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Button {
                        id: folderItem
                        required property int index
                        required property var modelData
                        width: folderList.width
                        height: 54
                        hoverEnabled: true
                        Accessible.name: "Select " + modelData.name
                        onClicked: page.appState.chooseRoot(index)
                        background: Rectangle {
                            radius: 9
                            color: page.appState.rootIndex === folderItem.index ? "#2a3b35" : folderItem.hovered ? "#233034" : "transparent"
                            border.color: folderItem.visualFocus ? Theme.accent : page.appState.rootIndex === folderItem.index ? "#486053" : "transparent"
                        }
                        contentItem: ColumnLayout {
                            spacing: 2
                            RowLayout {
                                Glyph {
                                    name: folderItem.modelData.archived ? "archive" : "folder"
                                    color: folderItem.modelData.color
                                    Layout.leftMargin: 6
                                }
                                LabelText {
                                    text: folderItem.modelData.name
                                    font.weight: Font.DemiBold
                                    Layout.fillWidth: true
                                }
                            }
                            LabelText {
                                text: folderItem.modelData.snapshots + " snapshots   ·   " + folderItem.modelData.size
                                color: Theme.muted
                                font.pixelSize: 10
                                Layout.leftMargin: 6
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
                BodyText {
                    text: "Archiving pauses snapshots.\nYour history stays here."
                    Layout.fillWidth: true
                    font.pixelSize: 10
                    color: Theme.muted
                    Layout.margins: 8
                }
            }
        }
        ScrollView {
            id: folderScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            clip: true
            ColumnLayout {
                width: folderScroll.availableWidth
                spacing: 16
                Panel {
                    Layout.fillWidth: true
                    implicitHeight: 165
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 16
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            spacing: 11
                            LabelText {
                                text: page.appState.currentRoot.name || ""
                                font.pixelSize: 24
                                font.weight: Font.DemiBold
                                Layout.fillWidth: true
                            }
                            LabelText {
                                text: page.appState.currentRoot.path || ""
                                font.pixelSize: 11
                                color: Theme.muted
                                Layout.fillWidth: true
                                elide: Text.ElideMiddle
                            }
                            RowLayout {
                                Badge {
                                    text: page.appState.currentRoot.archived === true ? "Archived" : "Watching"
                                    tone: page.appState.currentRoot.archived === true ? Theme.muted : Theme.accent
                                }
                                LabelText {
                                    text: page.appState.currentRoot.schedule || "Manual only"
                                    color: Theme.secondary
                                    font.pixelSize: 11
                                    Layout.fillWidth: true
                                }
                            }
                        }
                        ColumnLayout {
                            Layout.alignment: Qt.AlignTop
                            Layout.preferredWidth: 145
                            spacing: 6
                            ActionButton {
                                animationsEnabled: page.motion.transitionsEnabled
                                text: "Preferences"
                                primary: false
                                glyph: "settings"
                                Layout.fillWidth: true
                                implicitHeight: 30
                                onClicked: page.appState.openSheet("folder")
                            }
                            ActionButton {
                                animationsEnabled: page.motion.transitionsEnabled
                                text: "Open folder"
                                primary: false
                                quiet: true
                                glyph: "link"
                                Layout.fillWidth: true
                                implicitHeight: 30
                                onClicked: page.appState.openCurrentFolder()
                            }
                            ActionButton {
                                animationsEnabled: page.motion.transitionsEnabled
                                objectName: "folderSnapshotButton"
                                text: page.appState.scanning ? "Capturing… " + page.appState.scanProgress + "%" : "Take snapshot"
                                glyph: "snapshot"
                                Layout.fillWidth: true
                                implicitHeight: 30
                                enabled: !page.appState.scanning && !page.appState.currentRoot.archived
                                onClicked: page.appState.takeSnapshot()
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    LabelText {
                        text: "Snapshot history"
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    ActionButton {
                        animationsEnabled: page.motion.transitionsEnabled
                        text: "Compare"
                        glyph: "compare"
                        primary: false
                        implicitHeight: 30
                        onClicked: page.appState.selectedSection = AppState.Compare
                    }
                }
                ListView {
                    id: historyList
                    objectName: "historyList"
                    Layout.fillWidth: true
                    implicitHeight: Math.min(contentHeight, 360)
                    height: implicitHeight
                    clip: true
                    spacing: 7
                    reuseItems: true
                    model: page.appState.currentRoot.snapshots === 0 ? [] : page.appState.snapshots
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Panel {
                        required property var modelData
                        width: historyList.width
                        height: 72
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10
                            Rectangle {
                                width: 34
                                height: 34
                                radius: 10
                                color: "#293837"
                                Glyph {
                                    anchors.centerIn: parent
                                    name: "snapshot"
                                    color: Theme.accent
                                    font.pixelSize: 15
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                RowLayout {
                                    LabelText {
                                        text: modelData.date
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                    }
                                    Badge {
                                        text: !modelData.payloadAvailable ? "Unavailable" : modelData.trigger
                                        tone: !modelData.payloadAvailable ? Theme.warning : Theme.muted
                                    }
                                }
                                LabelText {
                                    text: modelData.description
                                    color: Theme.secondary
                                    font.pixelSize: 11
                                    Layout.fillWidth: true
                                }
                            }
                            ColumnLayout {
                                visible: page.width > 820
                                LabelText {
                                    text: modelData.size
                                    font.weight: Font.DemiBold
                                    Layout.alignment: Qt.AlignRight
                                }
                                LabelText {
                                    text: modelData.files + " files"
                                    font.pixelSize: 10
                                    color: Theme.muted
                                    Layout.alignment: Qt.AlignRight
                                }
                            }
                            ActionButton {
                                animationsEnabled: page.motion.transitionsEnabled
                                text: "Details"
                                primary: false
                                quiet: true
                                implicitHeight: 30
                                onClicked: {
                                    page.appState.detailId = modelData.id;
                                    page.appState.openSheet("detail");
                                }
                            }
                        }
                    }
                }
                EmptyState {
                    visible: page.appState.currentRoot.snapshots === 0
                    Layout.fillWidth: true
                    title: "Ready for its first moment."
                    message: "A first snapshot gives this folder a starting point."
                    actionText: "Take snapshot"
                    onTriggered: page.appState.takeSnapshot()
                }
                RowLayout {
                    Layout.fillWidth: true
                    LabelText {
                        text: "Showing the latest 5 moments"
                        font.pixelSize: 10
                        color: Theme.muted
                        Layout.fillWidth: true
                    }
                    ActionButton {
                        animationsEnabled: page.motion.transitionsEnabled
                        text: "Remove folder"
                        primary: false
                        quiet: true
                        danger: true
                        enabled: !page.appState.scanning
                        implicitHeight: 30
                        onClicked: page.appState.openSheet("removeFolder")
                    }
                    ActionButton {
                        animationsEnabled: page.motion.transitionsEnabled
                        text: "Clear history"
                        primary: false
                        quiet: true
                        danger: true
                        enabled: !page.appState.scanning
                        implicitHeight: 30
                        onClicked: page.appState.openSheet("clear")
                    }
                }
            }
        }
    }
    Item {
        visible: page.appState.visibleRoots.length === 0
        Layout.fillHeight: true
    }
}
