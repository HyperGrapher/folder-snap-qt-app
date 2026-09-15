import QtQuick
import QtTest
import FolderSnap

TestCase {
    name: "PreviewWorkflows"
    UiPreviewState {
        id: state
    }
    function init() {
        state.scenario = "Sample library";
        state.chooseRoot(0);
        state.sheet = "";
    }
    function test_explicitPairAndRollover() {
        compare(state.hasPair, false);
        state.chooseSnapshot(5);
        compare(state.hasPair, false);
        state.chooseSnapshot(3);
        compare(state.beforeId, 3);
        compare(state.afterId, 5);
        state.chooseSnapshot(4);
        compare(state.beforeId, 4);
        compare(state.afterId, 5);
        state.chooseSnapshot(5);
        compare(state.afterId, -1);
        state.chooseRoot(1);
        compare(state.beforeId, -1);
        compare(state.hasPair, false);
    }
    function test_treeFilteringRetainsAncestors() {
        state.search = "FolderCard";
        compare(state.displayedChanges.length, 3);
        compare(state.displayedChanges[0].path, "src");
        compare(state.displayedChanges[2].name, "FolderCard.qml");
        state.search = "";
        state.filter = "Removed";
        compare(state.displayedChanges.length, 3);
        state.search = "no-such-file";
        compare(state.displayedChanges.length, 0);
    }
    function test_collapsingAndLargeList() {
        state.toggleExpanded("src");
        verify(!state.displayedChanges.some(row => row.path === "src/main.cpp"));
        state.scenario = "Large comparison";
        verify(state.displayedChanges.length > 2000);
    }
    function test_cleanupStartsEmpty() {
        state.openSheet("cleanup");
        compare(state.cleanupSelection.length, 0);
        state.toggleCleanup(state.cleanupCandidates[0].path);
        compare(state.cleanupSelection.length, 1);
        state.openSheet("cleanup");
        compare(state.cleanupSelection.length, 0);
        verify(state.cleanupCandidates.every(row => row.status === "Added"));
    }
    function test_missingAndArchived() {
        state.scenario = "Missing snapshot";
        state.chooseSnapshot(5);
        compare(state.beforeId, -1);
        state.chooseRoot(3);
        state.takeSnapshot();
        compare(state.scanning, false);
    }
}
