import QtQuick
import QtTest
import FolderSnap

TestCase {
    name: "LiveWorkflows"

    UiPreviewState {
        id: state
    }

    function init() {
        state.clearSnapshotPair();
        state.sheet = "";
        state.snapshotSearch = "";
        state.search = "";
        state.filter = "All changes";
    }

    function test_startsWithStoredDataOnly() {
        compare(state.hasPair, false);
        verify(!state.scanning);
        verify(!state.comparing);
        compare(state.changes.length, 0);
    }

    function test_invalidSelectionsDoNotCreatePair() {
        state.chooseRoot(999);
        state.chooseSnapshot("not-a-snapshot");
        compare(state.beforeId, "");
        compare(state.afterId, "");
        compare(state.hasPair, false);
    }

    function test_emptyComparisonFiltersRemainEmpty() {
        state.search = "folder";
        compare(state.displayedChanges.length, 0);
        state.filter = "Added";
        compare(state.displayedChanges.length, 0);
        compare(state.cleanupCandidates.length, 0);
    }

    function test_dialogStateResetsTransientCleanup() {
        state.cleanupSelection = ["file.txt"];
        state.cleanupReviewed = true;
        state.cleanupResult = "old";
        state.openSheet("cleanup");
        compare(state.cleanupSelection.length, 0);
        compare(state.cleanupReviewed, false);
        compare(state.cleanupResult, "");
    }
}
