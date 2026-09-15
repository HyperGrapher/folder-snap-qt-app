import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FolderSnap

Dialog {
    id: dialog
    required property UiPreviewState appState
    required property MotionPolicy motion
    readonly property string kind: appState.sheet
    readonly property bool isExport: kind === "export" || kind === "exportComparison"
    readonly property bool isDestructive: kind === "delete" || kind === "clear"
    modal: true
    anchors.centerIn: parent
    width: Math.min(570, parent ? parent.width - 48 : 570)
    padding: 24
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    visible: kind !== ""
    onClosed: appState.sheet = ""
    background: Panel {
        color: "#1c272c"
        border.color: "#53616a"
        radius: 17
    }
    Overlay.modal: Rectangle {
        color: "#b8081014"
    }
    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0
            to: 1
            duration: dialog.motion.transitionsEnabled ? 160 : 0
        }
    }
    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1
            to: 0
            duration: dialog.motion.transitionsEnabled ? 110 : 0
        }
    }
    header: Item {
        implicitHeight: 92
        ColumnLayout {
            anchors.left: parent.left
            anchors.right: closeButton.left
            anchors.top: parent.top
            anchors.margins: 25
            spacing: 7
            LabelText {
                text: "FOLDERSNAP  /  " + (dialog.kind === "cleanup" ? "REVIEW" : "YOUR WORKSPACE")
                font.pixelSize: 9
                font.letterSpacing: 1.4
                color: Theme.accent
            }
            LabelText {
                Layout.fillWidth: true
                text: ({
                        add: "Give a folder some history.",
                        folder: "Folder preferences",
                        detail: "A moment, in detail.",
                        warnings: "A few things to know.",
                        export: "Take this moment with you.",
                        exportComparison: "Share the difference.",
                        cleanup: "A considered cleanup.",
                        delete: "Delete this snapshot?",
                        clear: "Clear this folder's history?"
                    })[dialog.kind] || ""
                font.pixelSize: 24
                font.weight: Font.DemiBold
                font.letterSpacing: -0.6
            }
        }
        ActionButton {
            id: closeButton
            animationsEnabled: dialog.motion.transitionsEnabled
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 17
            text: "Close"
            glyph: "close"
            primary: false
            quiet: true
            onClicked: dialog.close()
        }
    }
    contentItem: ColumnLayout {
        spacing: 16
        BodyText {
            Layout.fillWidth: true
            text: dialog.kind === "add" ? "Start with one folder. FolderSnap remembers its file details so you can see what changes over time." : dialog.kind === "folder" ? "Small preferences that make this folder work for you." : dialog.isExport ? "A complete, portable report of this " + (dialog.kind === "export" ? "snapshot" : "comparison") + ". Ready to open offline." : dialog.kind === "cleanup" ? "Only items added between these snapshots are eligible. Nothing is selected automatically." : dialog.isDestructive ? "This removes saved metadata from FolderSnap. Your watched files are unaffected." : dialog.kind === "warnings" ? "The snapshot is saved, but these paths could not be read. Changes beneath them may be uncertain." : dialog.appState.snapshot(dialog.appState.detailId).date + " · " + dialog.appState.currentRoot.name
            font.pixelSize: 12
        }
        ColumnLayout {
            visible: dialog.kind === "add" || dialog.kind === "folder"
            Layout.fillWidth: true
            spacing: 10
            LabelText {
                text: "DISPLAY NAME"
                font.pixelSize: 9
                font.letterSpacing: 1
                color: Theme.muted
            }
            SearchField {
                id: nameInput
                Layout.fillWidth: true
                leftPadding: 12
                placeholderText: "e.g. Weekend projects"
                text: dialog.kind === "folder" ? dialog.appState.currentRoot.name : ""
            }
            LabelText {
                text: dialog.kind === "add" ? "FOLDER PATH" : "REGISTERED FOLDER"
                font.pixelSize: 9
                font.letterSpacing: 1
                color: Theme.muted
            }
            SearchField {
                id: pathInput
                Layout.fillWidth: true
                leftPadding: 12
                readOnly: dialog.kind === "folder"
                text: dialog.kind === "folder" ? dialog.appState.currentRoot.path : ""
                placeholderText: "C:/Users/Burak/My folder"
            }
            RowLayout {
                visible: dialog.kind === "folder"
                Layout.fillWidth: true
                LabelText {
                    text: "Snapshot schedule"
                    Layout.fillWidth: true
                }
                SelectBox {
                    id: scheduleInput
                    implicitWidth: 200
                    model: ["Manual only", "Every 1 hour", "Every 3 hours", "Every 6 hours", "Every 12 hours", "Daily at 09:00", "Weekly · Monday 09:00", "Monthly · day 1, 09:00"]
                    currentIndex: Math.max(0, model.indexOf(dialog.appState.currentRoot.schedule))
                }
            }
            LabelText {
                visible: dialog.kind === "folder"
                text: "EXCLUSIONS · ONE RULE PER LINE"
                font.pixelSize: 9
                font.letterSpacing: 1
                color: Theme.muted
            }
            TextArea {
                id: ignoreInput
                visible: dialog.kind === "folder"
                Layout.fillWidth: true
                implicitHeight: 82
                text: dialog.appState.ignoreRules
                color: Theme.secondary
                font.family: "Consolas"
                font.pixelSize: 12
                padding: 12
                background: Rectangle {
                    radius: 8
                    color: "#151f23"
                    border.color: ignoreInput.activeFocus ? Theme.accent : Theme.border
                }
            }
            SettingRow {
                id: archiveInput
                visible: dialog.kind === "folder"
                Layout.fillWidth: true
                title: "Archive this folder"
                description: "Pause future snapshots and keep its history."
                checked: dialog.appState.currentRoot.archived
                animationsEnabled: dialog.motion.transitionsEnabled
            }
        }
        ColumnLayout {
            visible: dialog.kind === "detail"
            Layout.fillWidth: true
            spacing: 13
            Panel {
                Layout.fillWidth: true
                implicitHeight: 76
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    ColumnLayout {
                        LabelText {
                            text: dialog.appState.snapshot(dialog.appState.detailId).size || "12.8 GB"
                            font.pixelSize: 24
                            font.weight: Font.DemiBold
                        }
                        LabelText {
                            text: "TRACKED SIZE"
                            color: Theme.muted
                            font.pixelSize: 9
                        }
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    ColumnLayout {
                        LabelText {
                            text: dialog.appState.snapshot(dialog.appState.detailId).files || "8,426"
                            font.pixelSize: 24
                            font.weight: Font.DemiBold
                        }
                        LabelText {
                            text: "FILES"
                            color: Theme.muted
                            font.pixelSize: 9
                        }
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Badge {
                        text: dialog.appState.payloadMissing && dialog.appState.detailId === 5 ? "Unavailable" : "Saved"
                        tone: dialog.appState.payloadMissing && dialog.appState.detailId === 5 ? Theme.warning : Theme.accent
                    }
                }
            }
            LabelText {
                text: "DESCRIPTION"
                color: Theme.muted
                font.pixelSize: 9
                font.letterSpacing: 1
            }
            SearchField {
                id: descriptionInput
                Layout.fillWidth: true
                leftPadding: 12
                text: dialog.appState.snapshot(dialog.appState.detailId).description || ""
                placeholderText: "Give this moment a little context"
                maximumLength: 500
            }
            RowLayout {
                ActionButton {
                    animationsEnabled: dialog.motion.transitionsEnabled
                    text: "Save description"
                    primary: false
                    onClicked: {
                        const rows = dialog.appState.snapshots.slice();
                        const i = rows.findIndex(row => row.id === dialog.appState.detailId);
                        if (i >= 0)
                            rows[i] = Object.assign({}, rows[i], {
                                description: descriptionInput.text
                            });
                        dialog.appState.snapshots = rows;
                        dialog.appState.toast = "Sample description saved.";
                    }
                }
                ActionButton {
                    animationsEnabled: dialog.motion.transitionsEnabled
                    text: "Scan warnings"
                    primary: false
                    quiet: true
                    glyph: "warning"
                    onClicked: dialog.appState.sheet = "warnings"
                }
            }
            RowLayout {
                ActionButton {
                    animationsEnabled: dialog.motion.transitionsEnabled
                    text: "Export snapshot"
                    glyph: "export"
                    enabled: !(dialog.appState.payloadMissing && dialog.appState.detailId === 5)
                    onClicked: dialog.appState.sheet = "export"
                }
                Item {
                    Layout.fillWidth: true
                }
                ActionButton {
                    animationsEnabled: dialog.motion.transitionsEnabled
                    text: "Delete"
                    primary: false
                    quiet: true
                    danger: true
                    glyph: "trash"
                    onClicked: dialog.appState.sheet = "delete"
                }
            }
        }
        ColumnLayout {
            visible: dialog.kind === "warnings"
            Layout.fillWidth: true
            spacing: 10
            Repeater {
                model: ["cache/private — Access denied", "assets/staging — Directory became unavailable"]
                Panel {
                    required property string modelData
                    Layout.fillWidth: true
                    implicitHeight: 58
                    color: "#342f26"
                    border.color: "#59503c"
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        Glyph {
                            name: "warning"
                            color: Theme.warning
                        }
                        LabelText {
                            text: modelData
                            font.pixelSize: 11
                            color: Theme.warning
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }
        ColumnLayout {
            visible: dialog.isExport
            Layout.fillWidth: true
            spacing: 12
            Repeater {
                model: [
                    {
                        name: "Interactive HTML",
                        detail: "Search, sort, and explore an offline folder tree.",
                        icon: "overview"
                    },
                    {
                        name: "CSV spreadsheet",
                        detail: "Every entry, ready for your own analysis.",
                        icon: "file"
                    }
                ]
                Panel {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 88
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 17
                        spacing: 15
                        Glyph {
                            name: modelData.icon
                            color: Theme.accent
                            font.pixelSize: 23
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            LabelText {
                                text: modelData.name
                                font.weight: Font.DemiBold
                            }
                            BodyText {
                                text: modelData.detail
                                font.pixelSize: 11
                                Layout.fillWidth: true
                            }
                        }
                        ActionButton {
                            animationsEnabled: dialog.motion.transitionsEnabled
                            text: "Export"
                            primary: false
                            onClicked: {
                                dialog.appState.toast = "Preview: " + modelData.name + " export complete. No file was written.";
                                dialog.close();
                            }
                        }
                    }
                }
            }
        }
        ColumnLayout {
            visible: dialog.kind === "cleanup"
            Layout.fillWidth: true
            spacing: 10
            RowLayout {
                LabelText {
                    text: dialog.appState.cleanupSelection.length + " items selected"
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                }
                ActionButton {
                    animationsEnabled: dialog.motion.transitionsEnabled
                    text: dialog.appState.cleanupSelection.length === dialog.appState.cleanupCandidates.length ? "Clear selection" : "Select all"
                    primary: false
                    quiet: true
                    implicitHeight: 29
                    onClicked: {
                        dialog.appState.cleanupReviewed = false;
                        dialog.appState.cleanupSelection = dialog.appState.cleanupSelection.length === dialog.appState.cleanupCandidates.length ? [] : dialog.appState.cleanupCandidates.map(row => row.path);
                    }
                }
            }
            CheckBox {
                id: componentGroup
                Layout.fillWidth: true
                implicitHeight: 34
                readonly property var childrenPaths: dialog.appState.cleanupCandidates.filter(row => row.path.startsWith("src/components/")).map(row => row.path)
                readonly property int selectedCount: childrenPaths.filter(path => dialog.appState.cleanupSelection.includes(path)).length
                checkState: selectedCount === 0 ? Qt.Unchecked : selectedCount === childrenPaths.length ? Qt.Checked : Qt.PartiallyChecked
                tristate: true
                Accessible.name: "Select added items in src/components"
                nextCheckState: function () {
                    return checkState;
                }
                onClicked: {
                    dialog.appState.cleanupReviewed = false;
                    const rest = dialog.appState.cleanupSelection.filter(path => !childrenPaths.includes(path));
                    dialog.appState.cleanupSelection = selectedCount === childrenPaths.length ? rest : rest.concat(childrenPaths);
                }
                indicator: Rectangle {
                    x: 10
                    y: 8
                    width: 18
                    height: 18
                    radius: 4
                    color: componentGroup.checkState !== Qt.Unchecked ? Theme.accent : "transparent"
                    border.color: componentGroup.visualFocus ? Theme.text : "#648273"
                    LabelText {
                        anchors.centerIn: parent
                        text: componentGroup.checkState === Qt.PartiallyChecked ? "−" : "✓"
                        visible: componentGroup.checkState !== Qt.Unchecked
                        color: "#183b2f"
                    }
                }
                contentItem: LabelText {
                    text: "src / components"
                    leftPadding: 40
                    font.weight: Font.DemiBold
                }
            }
            Repeater {
                model: dialog.appState.cleanupCandidates
                CheckBox {
                    id: candidate
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 44
                    checked: dialog.appState.cleanupSelection.includes(modelData.path)
                    text: modelData.name
                    Accessible.name: text
                    onToggled: dialog.appState.toggleCleanup(modelData.path)
                    indicator: Rectangle {
                        x: 10
                        y: 13
                        width: 18
                        height: 18
                        radius: 4
                        color: candidate.checked ? Theme.accent : "transparent"
                        border.color: candidate.visualFocus ? Theme.text : "#648273"
                        Glyph {
                            anchors.centerIn: parent
                            name: "check"
                            visible: candidate.checked
                            color: "#183b2f"
                            font.pixelSize: 12
                        }
                    }
                    contentItem: LabelText {
                        text: candidate.text + "   ·   " + candidate.modelData.after
                        leftPadding: 40
                        font.pixelSize: 12
                    }
                    background: Rectangle {
                        radius: 7
                        color: candidate.hovered ? "#2d3b3a" : "#202e30"
                    }
                }
            }
            Panel {
                Layout.fillWidth: true
                implicitHeight: 66
                color: "#2c332b"
                border.color: "#485b43"
                BodyText {
                    anchors.fill: parent
                    anchors.margins: 14
                    font.pixelSize: 11
                    color: Theme.warning
                    text: dialog.appState.cleanupResult !== "" ? dialog.appState.cleanupResult : dialog.appState.cleanupReviewed ? dialog.appState.cleanupSelection.length + " ready · 0 blocked · 0 already missing. Sample preflight complete." : "Before anything moves, FolderSnap checks that selected files are unchanged and still inside the watched folder."
                }
            }
        }
        Badge {
            visible: dialog.isDestructive
            text: "Watched files are unaffected"
            tone: Theme.warning
        }
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
            visible: dialog.kind !== "detail" && !dialog.isExport
        }
        RowLayout {
            visible: dialog.kind !== "detail" && !dialog.isExport
            Layout.fillWidth: true
            LabelText {
                text: "PREVIEW · NO FILE CHANGES"
                font.pixelSize: 8
                font.letterSpacing: 0.8
                color: Theme.muted
                Layout.fillWidth: true
            }
            ActionButton {
                animationsEnabled: dialog.motion.transitionsEnabled
                text: "Cancel"
                primary: false
                quiet: true
                onClicked: dialog.close()
            }
            ActionButton {
                animationsEnabled: dialog.motion.transitionsEnabled
                text: dialog.kind === "add" ? "Add folder" : dialog.kind === "folder" ? "Save preferences" : dialog.kind === "cleanup" ? (dialog.appState.cleanupReviewed ? "Move to Recycle Bin" : "Check selected items") : dialog.isDestructive ? (dialog.kind === "clear" ? "Clear history" : "Delete snapshot") : "Done"
                glyph: dialog.kind === "add" ? "plus" : ""
                enabled: dialog.kind === "add" ? nameInput.text.trim() !== "" && pathInput.text.trim() !== "" : dialog.kind === "cleanup" ? dialog.appState.cleanupSelection.length > 0 && dialog.appState.cleanupResult === "" : true
                primary: !dialog.isDestructive
                danger: dialog.isDestructive
                onClicked: {
                    if (dialog.kind === "cleanup") {
                        if (!dialog.appState.cleanupReviewed)
                            dialog.appState.cleanupReviewed = true;
                        else
                            dialog.appState.cleanupResult = "Sample cleanup complete. No real files were moved. A new snapshot would capture the updated folder.";
                        return;
                    }
                    if (dialog.kind === "add")
                        dialog.appState.addFolder(nameInput.text.trim(), pathInput.text.trim());
                    if (dialog.kind === "folder") {
                        dialog.appState.updateRoot(nameInput.text, scheduleInput.currentText, archiveInput.checked);
                        dialog.appState.ignoreRules = ignoreInput.text;
                    }
                    if (dialog.isDestructive)
                        dialog.appState.toast = "Preview: " + (dialog.kind === "clear" ? "history cleared" : "snapshot deleted") + ". No stored data was changed.";
                    dialog.close();
                }
            }
        }
    }
}
