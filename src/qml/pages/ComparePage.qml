pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FolderSnap

ColumnLayout {
    id: page
    required property UiPreviewState appState
    required property MotionPolicy motion
    readonly property bool showSizeColumns: width >= 980
    spacing: 18

    RowLayout {
        Layout.fillWidth: true
        PageHeading {
            Layout.fillWidth: true
            eyebrow: "BETWEEN TWO MOMENTS"
            title: "See what changed."
            subtitle: "Choose two snapshots from the history, then explore every difference."
        }
        SelectBox {
            model: page.appState.visibleRoots.map(root => root.name)
            currentIndex: page.appState.rootIndex
            onActivated: page.appState.chooseRoot(currentIndex)
            implicitWidth: 190
        }
    }

    EmptyState {
        visible: page.appState.visibleRoots.length === 0
        Layout.fillWidth: true
        Layout.fillHeight: true
        onTriggered: page.appState.openSheet("add")
    }

    RowLayout {
        visible: page.appState.visibleRoots.length > 0
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 16

        Panel {
            id: snapshotBrowser
            Layout.preferredWidth: page.width >= 900 ? 330 : 272
            Layout.fillHeight: true
            color: "#182125"
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 1
                spacing: 0

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.margins: 16
                    Layout.bottomMargin: 12
                    spacing: 6
                    RowLayout {
                        Layout.fillWidth: true
                        LabelText {
                            text: "Snapshot history"
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                            Layout.fillWidth: true
                        }
                        Badge {
                            text: page.appState.availableSnapshots.length + " saved"
                            tone: Theme.muted
                        }
                    }
                    BodyText {
                        Layout.fillWidth: true
                        text: "Select two snapshots. A is older and B is newer."
                        font.pixelSize: 10
                    }
                }

                SearchField {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    Layout.bottomMargin: 12
                    placeholderText: "Search snapshots…"
                    text: page.appState.snapshotSearch
                    onTextChanged: page.appState.snapshotSearch = text
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.border
                }

                ListView {
                    id: snapshotList
                    objectName: "snapshotList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    reuseItems: true
                    spacing: 7
                    topMargin: 10
                    bottomMargin: 10
                    leftMargin: 10
                    rightMargin: 10
                    model: page.appState.filteredSnapshots
                    ScrollBar.vertical: ScrollBar {}

                    delegate: Button {
                        id: snapshotRow
                        required property var modelData
                        readonly property bool isA: page.appState.beforeId === modelData.id
                        readonly property bool isB: page.appState.afterId === modelData.id
                        readonly property bool isAssigned: isA || isB
                        width: snapshotList.width - snapshotList.leftMargin - snapshotList.rightMargin
                        height: 116
                        hoverEnabled: true
                        focusPolicy: Qt.StrongFocus
                        Accessible.name: (isA ? "A, " : isB ? "B, " : "") + modelData.fullDate + ", " + modelData.files + " files, " + modelData.folders + " folders, " + modelData.size + ", " + modelData.trigger + ", " + modelData.description
                        onClicked: page.appState.chooseSnapshot(modelData.id)

                        background: Rectangle {
                            radius: 10
                            color: snapshotRow.isAssigned ? "#263934" : snapshotRow.hovered ? "#222e32" : "#1d282c"
                            border.width: snapshotRow.isAssigned || snapshotRow.visualFocus ? 1 : 0
                            border.color: snapshotRow.visualFocus ? Theme.text : snapshotRow.isA ? Theme.violet : snapshotRow.isB ? Theme.accent : "transparent"
                        }

                        contentItem: RowLayout {
                            spacing: 11
                            Rectangle {
                                Layout.preferredWidth: 30
                                Layout.preferredHeight: 30
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 2
                                radius: 8
                                color: snapshotRow.isA ? "#343247" : snapshotRow.isB ? "#24443b" : "#273338"
                                LabelText {
                                    anchors.centerIn: parent
                                    visible: snapshotRow.isAssigned
                                    text: snapshotRow.isA ? "A" : "B"
                                    color: snapshotRow.isA ? Theme.violet : Theme.accent
                                    font.weight: Font.DemiBold
                                }
                                Glyph {
                                    anchors.centerIn: parent
                                    visible: !snapshotRow.isAssigned
                                    name: "snapshot"
                                    color: Theme.muted
                                    font.pixelSize: 14
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                RowLayout {
                                    Layout.fillWidth: true
                                    LabelText {
                                        text: snapshotRow.modelData.fullDate
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                        Layout.fillWidth: true
                                    }
                                    Badge {
                                        text: snapshotRow.modelData.trigger
                                        tone: snapshotRow.modelData.trigger === "Manual" ? Theme.violet : Theme.muted
                                        implicitHeight: 21
                                    }
                                }
                                BodyText {
                                    Layout.fillWidth: true
                                    text: snapshotRow.modelData.description
                                    color: Theme.secondary
                                    font.pixelSize: 10
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    LabelText {
                                        text: snapshotRow.modelData.files + " files"
                                        color: Theme.muted
                                        font.pixelSize: 9
                                    }
                                    LabelText {
                                        text: "·"
                                        color: Theme.muted
                                        font.pixelSize: 9
                                    }
                                    LabelText {
                                        text: snapshotRow.modelData.folders + " folders"
                                        color: Theme.muted
                                        font.pixelSize: 9
                                        Layout.fillWidth: true
                                    }
                                    LabelText {
                                        text: snapshotRow.modelData.size
                                        font.pixelSize: 10
                                        font.weight: Font.DemiBold
                                    }
                                }
                            }
                        }
                    }

                    LabelText {
                        anchors.centerIn: parent
                        visible: snapshotList.count === 0
                        text: "No snapshots match your search."
                        color: Theme.muted
                        font.pixelSize: 11
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.border
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.margins: 12
                    spacing: 10
                    RowLayout {
                        Layout.fillWidth: true
                        LabelText {
                            text: page.appState.hasPair ? "A and B selected" : page.appState.beforeId !== -1 || page.appState.afterId !== -1 ? "1 of 2 selected" : "Nothing selected"
                            color: page.appState.hasPair ? Theme.accent : Theme.muted
                            font.pixelSize: 10
                            Layout.fillWidth: true
                        }
                        ActionButton {
                            animationsEnabled: page.motion.transitionsEnabled
                            visible: page.appState.beforeId !== -1 || page.appState.afterId !== -1
                            text: "Clear"
                            primary: false
                            quiet: true
                            implicitHeight: 26
                            onClicked: page.appState.clearSnapshotPair()
                        }
                    }
                    ActionButton {
                        animationsEnabled: page.motion.transitionsEnabled
                        objectName: "compareButton"
                        Layout.fillWidth: true
                        text: page.appState.comparing ? "Comparing sample metadata…" : "Compare A and B"
                        glyph: "compare"
                        enabled: page.appState.hasPair && !page.appState.comparing
                        onClicked: page.appState.comparing = true
                    }
                }
            }
        }

        ColumnLayout {
            id: comparisonWorkspace
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                LabelText {
                    text: "Comparison"
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                }
                Badge {
                    visible: page.appState.comparisonReady
                    text: "8,390 entries checked"
                    tone: Theme.muted
                }
                Item {
                    Layout.fillWidth: true
                }
                ActionButton {
                    animationsEnabled: page.motion.transitionsEnabled
                    visible: page.appState.comparisonReady
                    text: "Export"
                    glyph: "export"
                    primary: false
                    onClicked: page.appState.openSheet("exportComparison")
                }
            }

            EmptyState {
                visible: !page.appState.comparisonReady
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: page.appState.comparing ? "Putting the moments together…" : page.appState.hasPair ? "A and B are ready." : "Two moments tell the story."
                message: page.appState.comparing ? "Comparing sample metadata. Your files stay untouched." : page.appState.hasPair ? "Use Compare A and B to reveal what changed between the selected snapshots." : "Choose any two snapshots from the history.\nThe older one becomes A and the newer one becomes B."
                actionText: page.appState.hasPair ? "Compare A and B" : "Browse the history"
                glyph: "compare"
                onTriggered: {
                    if (page.appState.hasPair)
                        page.appState.comparing = true;
                    else
                        snapshotList.positionViewAtBeginning();
                }
            }

            RowLayout {
                visible: page.appState.comparisonReady
                Layout.fillWidth: true
                spacing: 8
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
                            label: "Net size",
                            value: "+410 MB",
                            tone: "#94e6c6"
                        }
                    ]
                    Panel {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        implicitHeight: 59
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 7
                            LabelText {
                                text: modelData.value
                                font.pixelSize: modelData.label === "Net size" ? 16 : 21
                                font.weight: Font.DemiBold
                                color: modelData.tone
                            }
                            LabelText {
                                text: modelData.label
                                font.pixelSize: 9
                                color: Theme.secondary
                                Layout.fillWidth: true
                            }
                        }
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
                        font.pixelSize: 10
                        Layout.fillWidth: true
                    }
                    ActionButton {
                        animationsEnabled: page.motion.transitionsEnabled
                        text: "Review"
                        quiet: true
                        primary: false
                        implicitHeight: 27
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
                        Layout.margins: 11
                        spacing: 8
                        SearchField {
                            Layout.fillWidth: true
                            text: page.appState.search
                            onTextChanged: page.appState.search = text
                        }
                        SelectBox {
                            model: ["All changes", "Added", "Removed", "Modified"]
                            implicitWidth: 128
                            currentIndex: model.indexOf(page.appState.filter)
                            onActivated: page.appState.filter = currentText
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        height: 31
                        color: "#182226"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 15
                            anchors.rightMargin: 15
                            spacing: 9
                            LabelText {
                                text: "NAME"
                                Layout.fillWidth: true
                                font.pixelSize: 8
                                font.letterSpacing: 1
                                color: Theme.muted
                            }
                            LabelText {
                                text: "CHANGE"
                                Layout.preferredWidth: 72
                                font.pixelSize: 8
                                color: Theme.muted
                            }
                            LabelText {
                                visible: page.showSizeColumns
                                text: "BEFORE"
                                Layout.preferredWidth: 60
                                horizontalAlignment: Text.AlignRight
                                font.pixelSize: 8
                                color: Theme.muted
                            }
                            LabelText {
                                visible: page.showSizeColumns
                                text: "AFTER"
                                Layout.preferredWidth: 60
                                horizontalAlignment: Text.AlignRight
                                font.pixelSize: 8
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
                                    width: parent.width
                                    height: 1
                                    color: "#263237"
                                }
                            }
                            contentItem: RowLayout {
                                spacing: 9
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 7
                                    Item {
                                        Layout.preferredWidth: treeRow.modelData.depth * 13 + 2
                                    }
                                    Glyph {
                                        name: page.appState.expanded.includes(treeRow.modelData.path) ? "down" : "chevron"
                                        visible: treeRow.modelData.folder
                                        font.pixelSize: 8
                                    }
                                    Glyph {
                                        name: treeRow.modelData.folder ? "folder" : "file"
                                        color: treeRow.modelData.folder ? Theme.warning : Theme.muted
                                        font.pixelSize: 13
                                    }
                                    LabelText {
                                        text: treeRow.modelData.name
                                        Layout.fillWidth: true
                                        font.pixelSize: 11
                                        font.weight: treeRow.modelData.folder ? Font.DemiBold : Font.Normal
                                    }
                                }
                                Item {
                                    Layout.preferredWidth: 72
                                    height: 22
                                    Badge {
                                        text: treeRow.modelData.status
                                        visible: text !== ""
                                        tone: text === "Added" ? Theme.accent : text === "Removed" ? Theme.danger : Theme.warning
                                        implicitHeight: 20
                                    }
                                }
                                LabelText {
                                    visible: page.showSizeColumns
                                    text: treeRow.modelData.before
                                    Layout.preferredWidth: 60
                                    horizontalAlignment: Text.AlignRight
                                    font.pixelSize: 10
                                    color: Theme.muted
                                }
                                LabelText {
                                    visible: page.showSizeColumns
                                    text: treeRow.modelData.after
                                    Layout.preferredWidth: 60
                                    horizontalAlignment: Text.AlignRight
                                    font.pixelSize: 10
                                    Layout.rightMargin: 10
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
                        Layout.margins: 10
                        Glyph {
                            name: "shield"
                            color: Theme.muted
                            font.pixelSize: 11
                        }
                        LabelText {
                            text: "8,383 unchanged · Originals untouched"
                            font.pixelSize: 9
                            color: Theme.muted
                            Layout.fillWidth: true
                        }
                        ActionButton {
                            animationsEnabled: page.motion.transitionsEnabled
                            text: "Review added items"
                            glyph: "trash"
                            primary: false
                            implicitHeight: 31
                            onClicked: page.appState.openSheet("cleanup")
                        }
                    }
                }
            }
        }
    }
}
