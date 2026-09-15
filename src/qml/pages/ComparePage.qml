import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FolderSnap

ColumnLayout {
    id: page
    required property UiPreviewState appState
    required property MotionPolicy motion
    spacing: 16
    RowLayout {
        Layout.fillWidth: true
        PageHeading {
            Layout.fillWidth: true
            eyebrow: "BETWEEN TWO MOMENTS"
            title: "See what changed."
            subtitle: "Choose a before and after. Every difference, in one place."
        }
        SelectBox {
            model: page.appState.visibleRoots.map(root => root.name)
            currentIndex: page.appState.rootIndex
            onActivated: page.appState.chooseRoot(currentIndex)
            implicitWidth: 180
        }
    }
    EmptyState {
        visible: page.appState.visibleRoots.length === 0
        Layout.fillWidth: true
        onTriggered: page.appState.openSheet("add")
    }
    ColumnLayout {
        visible: page.appState.visibleRoots.length > 0
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 14
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            SnapshotSlot {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                snapshot: page.appState.snapshot(page.appState.beforeId)
                assigned: page.appState.beforeId !== -1
            }
            Glyph {
                name: "arrow"
                color: Theme.muted
            }
            SnapshotSlot {
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                letter: "B"
                label: "AFTER"
                tone: Theme.accent
                snapshot: page.appState.snapshot(page.appState.afterId)
                assigned: page.appState.afterId !== -1
            }
            ActionButton {
                animationsEnabled: page.motion.transitionsEnabled
                objectName: "compareButton"
                text: page.appState.comparing ? "Comparing…" : "Compare"
                glyph: "compare"
                enabled: page.appState.hasPair && !page.appState.comparing
                onClicked: page.appState.comparing = true
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Repeater {
                model: page.appState.snapshots
                Button {
                    id: moment
                    required property var modelData
                    readonly property bool assigned: page.appState.beforeId === modelData.id || page.appState.afterId === modelData.id
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    implicitHeight: 52
                    hoverEnabled: true
                    Accessible.name: "Select snapshot " + modelData.date
                    onClicked: page.appState.chooseSnapshot(modelData.id)
                    background: Rectangle {
                        radius: 8
                        color: moment.assigned ? "#2c3e38" : moment.hovered ? Theme.surfaceRaised : "#1a2428"
                        border.color: moment.visualFocus ? Theme.text : moment.assigned ? "#648c76" : Theme.border
                    }
                    contentItem: ColumnLayout {
                        spacing: 4
                        LabelText {
                            Layout.alignment: Qt.AlignHCenter
                            text: (page.appState.beforeId === moment.modelData.id ? "A · " : page.appState.afterId === moment.modelData.id ? "B · " : "") + moment.modelData.day
                            font.pixelSize: 9
                            color: moment.assigned ? Theme.accent : Theme.muted
                            font.letterSpacing: 0.6
                        }
                        LabelText {
                            Layout.alignment: Qt.AlignHCenter
                            text: moment.modelData.time
                            font.pixelSize: 12
                        }
                    }
                }
            }
        }
        LabelText {
            visible: !page.appState.comparisonReady
            text: "Select any two moments above. A is always the older snapshot."
            font.pixelSize: 10
            color: Theme.muted
            Layout.fillWidth: true
        }
        EmptyState {
            visible: !page.appState.comparisonReady
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: page.appState.comparing ? "Putting the moments together…" : "A small distance. A clear difference."
            message: page.appState.comparing ? "Comparing sample metadata. Your files stay untouched." : "Pick two snapshots to discover what was added,\nremoved, or modified between them."
            actionText: page.appState.hasPair ? "Compare selected snapshots" : "Browse snapshot history"
            glyph: "compare"
            onTriggered: {
                if (page.appState.hasPair)
                    page.appState.comparing = true;
                else
                    page.appState.selectedSection = AppState.Folders;
            }
        }
        RowLayout {
            visible: page.appState.comparisonReady
            Layout.fillWidth: true
            spacing: 9
            Repeater {
                model: [
                    {
                        label: "Added",
                        value: page.appState.scenario === "Large comparison" ? "2,003" : "3",
                        tone: "#94e6c6"
                    },
                    {
                        label: "Removed",
                        value: "2",
                        tone: "#eda6a6"
                    },
                    {
                        label: "Modified",
                        value: "2",
                        tone: "#e9c387"
                    },
                    {
                        label: "Unchanged",
                        value: "8,383",
                        tone: "#9db2bd"
                    }
                ]
                Panel {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    implicitHeight: 66
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        LabelText {
                            text: modelData.value
                            font.pixelSize: 24
                            font.weight: Font.DemiBold
                            color: modelData.tone
                        }
                        LabelText {
                            text: modelData.label
                            font.pixelSize: 10
                            color: Theme.secondary
                            Layout.fillWidth: true
                        }
                    }
                }
            }
            ColumnLayout {
                Layout.leftMargin: 8
                spacing: 3
                LabelText {
                    text: "+410 MB"
                    color: Theme.accent
                    font.pixelSize: 21
                    font.weight: Font.DemiBold
                }
                LabelText {
                    text: "NET SIZE CHANGE"
                    font.pixelSize: 8
                    font.letterSpacing: 0.8
                    color: Theme.muted
                }
            }
        }
        Panel {
            visible: page.appState.comparisonReady && page.appState.hasWarnings
            Layout.fillWidth: true
            implicitHeight: 43
            color: "#383327"
            border.color: "#66583e"
            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                Glyph {
                    name: "warning"
                    color: Theme.warning
                }
                LabelText {
                    text: "2 unreadable paths · Some changes may be uncertain."
                    color: Theme.warning
                    font.pixelSize: 11
                    Layout.fillWidth: true
                }
                ActionButton {
                    animationsEnabled: page.motion.transitionsEnabled
                    text: "Review"
                    quiet: true
                    primary: false
                    implicitHeight: 28
                    onClicked: page.appState.openSheet("warnings")
                }
            }
        }
        Panel {
            visible: page.appState.comparisonReady
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 1
                spacing: 0
                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: 12
                    spacing: 8
                    SearchField {
                        Layout.fillWidth: true
                        text: page.appState.search
                        onTextChanged: page.appState.search = text
                    }
                    SelectBox {
                        model: ["All changes", "Added", "Removed", "Modified"]
                        implicitWidth: 132
                        currentIndex: model.indexOf(page.appState.filter)
                        onActivated: page.appState.filter = currentText
                    }
                    ActionButton {
                        animationsEnabled: page.motion.transitionsEnabled
                        text: "Export"
                        glyph: "export"
                        primary: false
                        onClicked: page.appState.openSheet("exportComparison")
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    height: 32
                    color: "#182226"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 18
                        anchors.rightMargin: 18
                        spacing: 12
                        LabelText {
                            text: "NAME"
                            Layout.fillWidth: true
                            font.pixelSize: 9
                            font.letterSpacing: 1
                            color: Theme.muted
                        }
                        LabelText {
                            text: "CHANGE"
                            Layout.preferredWidth: 90
                            font.pixelSize: 9
                            color: Theme.muted
                        }
                        LabelText {
                            text: "BEFORE"
                            Layout.preferredWidth: 73
                            horizontalAlignment: Text.AlignRight
                            font.pixelSize: 9
                            color: Theme.muted
                        }
                        LabelText {
                            text: "AFTER"
                            Layout.preferredWidth: 73
                            horizontalAlignment: Text.AlignRight
                            font.pixelSize: 9
                            color: Theme.muted
                        }
                    }
                }
                ListView {
                    id: tree
                    objectName: "comparisonTree"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: page.appState.displayedChanges
                    reuseItems: true
                    ScrollBar.vertical: ScrollBar {}
                    delegate: ItemDelegate {
                        id: treeRow
                        required property var modelData
                        width: tree.width
                        height: 40
                        hoverEnabled: true
                        Accessible.name: modelData.path + " " + modelData.status
                        onClicked: {
                            if (modelData.folder)
                                page.appState.toggleExpanded(modelData.path);
                            else
                                page.appState.toast = modelData.path + " · " + modelData.status;
                        }
                        background: Rectangle {
                            color: treeRow.hovered ? "#2a3739" : treeRow.modelData.status === "Added" ? "#182c27" : "transparent"
                            border.color: treeRow.visualFocus ? Theme.accent : "transparent"
                            Rectangle {
                                anchors.bottom: parent.bottom
                                height: 1
                                width: parent.width
                                color: "#263237"
                            }
                        }
                        contentItem: RowLayout {
                            spacing: 12
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 9
                                Item {
                                    Layout.preferredWidth: treeRow.modelData.depth * 16 + 3
                                }
                                Glyph {
                                    name: page.appState.expanded.includes(treeRow.modelData.path) ? "down" : "chevron"
                                    visible: treeRow.modelData.folder
                                    font.pixelSize: 9
                                }
                                Glyph {
                                    name: treeRow.modelData.folder ? "folder" : "file"
                                    color: treeRow.modelData.folder ? Theme.warning : Theme.muted
                                    font.pixelSize: 14
                                }
                                LabelText {
                                    text: treeRow.modelData.name
                                    font.pixelSize: 12
                                    Layout.fillWidth: true
                                    font.weight: treeRow.modelData.folder ? Font.DemiBold : Font.Normal
                                }
                            }
                            Item {
                                Layout.preferredWidth: 90
                                height: 24
                                Badge {
                                    text: treeRow.modelData.status
                                    visible: text !== ""
                                    tone: text === "Added" ? Theme.accent : text === "Removed" ? Theme.danger : Theme.warning
                                    implicitHeight: 21
                                }
                            }
                            LabelText {
                                text: treeRow.modelData.before
                                Layout.preferredWidth: 73
                                horizontalAlignment: Text.AlignRight
                                font.pixelSize: 11
                                color: Theme.muted
                            }
                            LabelText {
                                text: treeRow.modelData.after
                                Layout.preferredWidth: 73
                                horizontalAlignment: Text.AlignRight
                                font.pixelSize: 11
                                Layout.rightMargin: 12
                            }
                        }
                    }
                    LabelText {
                        anchors.centerIn: parent
                        visible: tree.count === 0
                        text: "No changes match your search."
                        color: Theme.muted
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: 12
                    Glyph {
                        name: "shield"
                        color: Theme.muted
                        font.pixelSize: 12
                    }
                    LabelText {
                        text: "Historical comparison · Originals untouched"
                        font.pixelSize: 10
                        color: Theme.muted
                        Layout.fillWidth: true
                    }
                    ActionButton {
                        animationsEnabled: page.motion.transitionsEnabled
                        text: "Review added items"
                        glyph: "trash"
                        primary: false
                        implicitHeight: 32
                        onClicked: page.appState.openSheet("cleanup")
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
