import QtQuick
import QtTest
import FolderSnap

TestCase {
    id: testCase
    name: "PrimaryWorkflows"
    width: 1200
    height: 800
    visible: true
    when: windowShown

    UiPreviewState {
        id: state
    }
    MotionPolicy {
        id: motion
        reducedMotion: true
        windowVisible: true
        windowExposed: true
    }
    PageHost {
        id: host
        anchors.fill: parent
        appState: state
        motion: motion
    }
    PreviewDialog {
        id: dialog
        appState: state
        motion: motion
    }

    function test_snapshotAndComparisonWorkflow() {
        compare(state.visibleRoots.length, 1);
        compare(state.currentRoot.name, "QML test folder");

        const overviewSnapshot = findChild(host, "overviewSnapshotButton");
        verify(overviewSnapshot !== null);
        compare(overviewSnapshot.Accessible.name, "Take snapshot");
        overviewSnapshot.forceActiveFocus();
        verify(overviewSnapshot.activeFocus);
        keyClick(Qt.Key_Return);
        tryCompare(state, "scanning", false, 10000);
        tryVerify(() => state.snapshots.length === 1, 10000);

        host.selectPage(AppState.Folders);
        tryCompare(host, "isTransitioning", false, 1000);
        const folderSnapshot = findChild(host, "folderSnapshotButton");
        verify(folderSnapshot !== null);
        compare(folderSnapshot.Accessible.name, "Take snapshot");
        mouseClick(folderSnapshot);
        tryCompare(state, "scanning", false, 10000);
        tryVerify(() => state.snapshots.length === 2, 10000);

        host.selectPage(AppState.Compare);
        tryCompare(host, "isTransitioning", false, 1000);
        const snapshotList = findChild(host, "snapshotList");
        verify(snapshotList !== null);
        tryCompare(snapshotList, "count", 2, 1000);
        tryVerify(() => snapshotList.itemAtIndex(0) !== null && snapshotList.itemAtIndex(1) !== null, 1000);

        const firstSnapshot = snapshotList.itemAtIndex(0);
        const secondSnapshot = snapshotList.itemAtIndex(1);
        verify(firstSnapshot.Accessible.name.length > 0);
        verify(secondSnapshot.Accessible.name.length > 0);
        mouseClick(firstSnapshot);
        mouseClick(secondSnapshot);
        verify(state.hasPair);

        const compareButton = findChild(host, "compareButton");
        verify(compareButton !== null);
        compare(compareButton.Accessible.name, "Compare A and B");
        verify(compareButton.enabled);
        compareButton.forceActiveFocus();
        keyClick(Qt.Key_Return);
        tryCompare(state, "comparisonReady", true, 10000);
        verify(state.comparedCount > 0);

        state.openSheet("folder");
        tryCompare(dialog, "visible", true, 1000);
        const nameInput = findChild(dialog, "folderNameInput");
        const retentionInput = findChild(dialog, "folderRetentionInput");
        const ignoreInput = findChild(dialog, "folderIgnoreInput");
        const saveButton = findChild(dialog, "dialogPrimaryButton");
        verify(nameInput !== null);
        verify(retentionInput !== null);
        verify(ignoreInput !== null);
        verify(saveButton !== null);
        verify(nameInput.Accessible.name.length > 0);
        compare(ignoreInput.Accessible.name, "Exclusion rules");
        compare(saveButton.Accessible.name, "Save preferences");

        nameInput.text = "Renamed from UI";
        retentionInput.currentIndex = 1;
        ignoreInput.text = "cache/\n*.tmp";
        mouseClick(saveButton);
        tryCompare(dialog, "visible", false, 1000);
        compare(state.currentRoot.name, "Renamed from UI");
        compare(state.currentRoot.retention, 25);
        compare(state.ignoreRules, "cache/\n*.tmp");
    }
}
