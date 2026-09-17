import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import FolderSnap

Dialog {
    id: dialog
    required property UiPreviewState appState
    required property MotionPolicy motion
    readonly property string kind: appState.sheet
    readonly property bool isExport: kind === "export" || kind === "exportComparison"
    readonly property bool isDestructive: kind === "delete" || kind === "clear" || kind === "removeFolder"
    property string pendingExportFormat: "html"
    property bool pendingComparisonExport: false
    modal: true
    anchors.centerIn: parent
    width: Math.min(570, parent ? parent.width - 48 : 570)
    padding: 24
    closePolicy: appState.exporting ? Popup.NoAutoClose : Popup.CloseOnEscape | Popup.CloseOnPressOutside
    visible: kind !== "" && kind !== "add"
    onClosed: appState.sheet = ""
    onKindChanged: {
        if (kind === "add")
            folderChooser.open();
    }
    FolderDialog {
        id: folderChooser
        title: "Choose a folder to watch"
        onAccepted: {
            dialog.appState.sheet = "";
            dialog.appState.addFolder(selectedFolder);
        }
        onRejected: dialog.appState.sheet = ""
    }
    FileDialog {
        id: exportChooser
        title: dialog.pendingComparisonExport ? "Export comparison report" : "Export snapshot report"
        fileMode: FileDialog.SaveFile
        defaultSuffix: dialog.pendingExportFormat
        nameFilters: dialog.pendingExportFormat === "html" ? ["HTML report (*.html)"] : ["CSV spreadsheet (*.csv)"]
        onAccepted: {
            if (dialog.pendingComparisonExport)
                dialog.appState.exportComparison(dialog.pendingExportFormat, selectedFile);
            else
                dialog.appState.exportSnapshot(dialog.pendingExportFormat, selectedFile);
        }
    }
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
                        folder: "Folder preferences",
                        detail: "A moment, in detail.",
                        warnings: "A few things to know.",
                        export: "Take this moment with you.",
                        exportComparison: "Share the difference.",
                        cleanup: "A considered cleanup.",
                        delete: "Delete this snapshot?",
                        clear: "Clear this folder's history?",
                        removeFolder: "Remove this watched folder?"
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
            enabled: !dialog.appState.exporting
            onClicked: dialog.close()
        }
    }
    contentItem: ColumnLayout {
        spacing: 16
        BodyText {
            Layout.fillWidth: true
            text: dialog.kind === "folder" ? "Small preferences that make this folder work for you." : dialog.isExport ? "Save a private, offline report. Snapshot data stays on this computer." : dialog.kind === "cleanup" ? "Added entries can be reviewed here. Moving live files is disabled until the safety workflow is implemented." : dialog.kind === "removeFolder" ? "This removes the watched-folder registration and all of its saved snapshot history. The real folder and its files are untouched." : dialog.isDestructive ? "This removes saved metadata from FolderSnap. Your watched files are unaffected." : dialog.kind === "warnings" ? "The snapshot is saved, but these paths could not be read. Changes beneath them may be uncertain." : dialog.appState.snapshot(dialog.appState.detailId).date + " · " + dialog.appState.currentRoot.name
            font.pixelSize: 12
        }
        ColumnLayout {
            visible: dialog.kind === "folder"
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
                objectName: "folderNameInput"
                Layout.fillWidth: true
                leftPadding: 12
                placeholderText: "e.g. Weekend projects"
                text: dialog.kind === "folder" ? dialog.appState.currentRoot.name : ""
            }
            LabelText {
                text: "REGISTERED FOLDER"
                font.pixelSize: 9
                font.letterSpacing: 1
                color: Theme.muted
            }
            RowLayout {
                Layout.fillWidth: true
                SearchField {
                    Layout.fillWidth: true
                    leftPadding: 12
                    readOnly: true
                    text: dialog.appState.currentRoot.path
                }
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
                    objectName: "folderScheduleInput"
                    implicitWidth: 200
                    model: ["Manual only", "Every 1 hour", "Every 3 hours", "Every 6 hours", "Every 12 hours", "Daily at 09:00", "Weekly · Monday 09:00", "Monthly · day 1, 09:00"]
                    currentIndex: Math.max(0, model.indexOf(dialog.appState.currentRoot.schedule))
                }
            }
            RowLayout {
                visible: dialog.kind === "folder"
                Layout.fillWidth: true
                LabelText {
                    text: "History retention"
                    Layout.fillWidth: true
                }
                SelectBox {
                    id: retentionInput
                    objectName: "folderRetentionInput"
                    implicitWidth: 200
                    model: ["10 snapshots", "25 snapshots", "50 snapshots", "100 snapshots", "Unlimited"]
                    currentIndex: Math.max(0, [10, 25, 50, 100, 0].indexOf(dialog.appState.currentRoot.retention))
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
                objectName: "folderIgnoreInput"
                visible: dialog.kind === "folder"
                Layout.fillWidth: true
                implicitHeight: 82
                text: dialog.appState.ignoreRules
                color: Theme.secondary
                font.family: "Consolas"
                font.pixelSize: 12
                padding: 12
                Accessible.name: "Exclusion rules"
                background: Rectangle {
                    radius: 8
                    color: "#151f23"
                    border.color: ignoreInput.activeFocus ? Theme.accent : Theme.border
                }
            }
            SettingRow {
                id: archiveInput
                objectName: "folderArchiveInput"
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
                            text: dialog.appState.snapshot(dialog.appState.detailId).size || "—"
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
                            text: dialog.appState.snapshot(dialog.appState.detailId).files || "—"
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
                        text: !dialog.appState.snapshot(dialog.appState.detailId).payloadAvailable ? "Unavailable" : "Saved"
                        tone: !dialog.appState.snapshot(dialog.appState.detailId).payloadAvailable ? Theme.warning : Theme.accent
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
                        dialog.appState.saveDescription(descriptionInput.text);
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
                    enabled: dialog.appState.snapshot(dialog.appState.detailId).payloadAvailable === true
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
                objectName: "exportOptions"
                model: [
                    {
                        name: "Interactive HTML",
                        detail: "Search, sort, and explore an offline folder tree.",
                        icon: "overview",
                        format: "html"
                    },
                    {
                        name: "CSV spreadsheet",
                        detail: "Every entry, ready for your own analysis.",
                        icon: "file",
                        format: "csv"
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
                            objectName: modelData.format === "html" ? "exportHtmlButton" : "exportCsvButton"
                            animationsEnabled: dialog.motion.transitionsEnabled
                            text: dialog.appState.exporting ? "Exporting…" : "Choose location"
                            primary: false
                            enabled: !dialog.appState.exporting
                            onClicked: {
                                dialog.pendingExportFormat = modelData.format;
                                dialog.pendingComparisonExport = dialog.kind === "exportComparison";
                                exportChooser.open();
                            }
                        }
                    }
                }
            }
            ColumnLayout {
                visible: dialog.appState.exporting
                Layout.fillWidth: true
                spacing: 8
                LabelText {
                    text: "Building report…"
                    color: Theme.accent
                    font.weight: Font.DemiBold
                }
                ProgressBar {
                    Layout.fillWidth: true
                    indeterminate: true
                    Accessible.name: "Export in progress"
                }
                ActionButton {
                    objectName: "cancelExportButton"
                    text: "Cancel export"
                    primary: false
                    quiet: true
                    onClicked: dialog.appState.cancelExport()
                }
            }
            Panel {
                visible: dialog.appState.exportError !== ""
                Layout.fillWidth: true
                implicitHeight: 58
                color: "#342f26"
                border.color: "#75594a"
                BodyText {
                    anchors.fill: parent
                    anchors.margins: 14
                    text: dialog.appState.exportError
                    color: Theme.warning
                    font.pixelSize: 11
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
                    text: "Cleanup is intentionally disabled until a safe, transactional file operation is implemented."
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
                text: "LOCAL METADATA · ORIGINALS UNTOUCHED"
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
                objectName: "dialogPrimaryButton"
                animationsEnabled: dialog.motion.transitionsEnabled
                text: dialog.kind === "folder" ? "Save preferences" : dialog.kind === "cleanup" ? "Cleanup unavailable" : dialog.kind === "removeFolder" ? "Remove folder and history" : dialog.isDestructive ? (dialog.kind === "clear" ? "Clear history" : "Delete snapshot") : "Done"
                enabled: dialog.kind !== "cleanup"
                primary: !dialog.isDestructive
                danger: dialog.isDestructive
                onClicked: {
                    if (dialog.kind === "cleanup") {
                        return;
                    }
                    if (dialog.kind === "folder") {
                        dialog.appState.updateRoot(nameInput.text, scheduleInput.currentText, [10, 25, 50, 100, 0][retentionInput.currentIndex], ignoreInput.text, archiveInput.checked);
                    }
                    if (dialog.kind === "clear")
                        dialog.appState.clearSelectedRootHistory();
                    if (dialog.kind === "removeFolder")
                        dialog.appState.removeCurrentRoot();
                    if (dialog.kind === "delete")
                        dialog.appState.deleteSelectedSnapshot();
                    dialog.close();
                }
            }
        }
    }
}
