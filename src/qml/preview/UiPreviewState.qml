import QtQuick
import FolderSnap

// Deliberately memory-only: this prototype never calls filesystem or Shell services.
AppState {
    id: state
    property int rootIndex: 0
    property string scenario: "Sample library"
    property var roots: [
        {
            name: "Projects",
            path: "C:/Users/Burak/Projects",
            size: "12.8 GB",
            files: "8,426",
            snapshots: 24,
            color: "#94e6c6",
            schedule: "Every 6 hours",
            archived: false,
            note: "Your development workspace"
        },
        {
            name: "Design assets",
            path: "D:/Creative/Design assets",
            size: "6.4 GB",
            files: "2,184",
            snapshots: 18,
            color: "#b7a8e6",
            schedule: "Daily at 09:00",
            archived: false,
            note: "The things you create"
        },
        {
            name: "Documents",
            path: "C:/Users/Burak/Documents",
            size: "842 MB",
            files: "1,206",
            snapshots: 12,
            color: "#e9c387",
            schedule: "Manual only",
            archived: false,
            note: "A little order, over time"
        },
        {
            name: "Archive",
            path: "D:/Archive/2025",
            size: "3.1 GB",
            files: "904",
            snapshots: 8,
            color: "#8396a5",
            schedule: "Paused",
            archived: true,
            note: "History kept, watching paused"
        }
    ]
    readonly property var visibleRoots: scenario === "Empty library" ? [] : roots
    readonly property var currentRoot: roots[Math.min(rootIndex, roots.length - 1)]
    property var snapshots: [
        {
            id: 5,
            date: "Today, 14:32",
            day: "15 SEP",
            time: "14:32",
            description: "After the afternoon build",
            trigger: "Manual",
            size: "12.8 GB",
            files: "8,426"
        },
        {
            id: 4,
            date: "Today, 09:00",
            day: "15 SEP",
            time: "09:00",
            description: "Morning checkpoint",
            trigger: "Scheduled",
            size: "12.4 GB",
            files: "8,390"
        },
        {
            id: 3,
            date: "Yesterday, 18:00",
            day: "14 SEP",
            time: "18:00",
            description: "Before dependency updates",
            trigger: "Manual",
            size: "12.3 GB",
            files: "8,364"
        },
        {
            id: 2,
            date: "Yesterday, 12:00",
            day: "14 SEP",
            time: "12:00",
            description: "Midday snapshot",
            trigger: "Scheduled",
            size: "12.2 GB",
            files: "8,350"
        },
        {
            id: 1,
            date: "Sep 13, 18:00",
            day: "13 SEP",
            time: "18:00",
            description: "A fresh starting point",
            trigger: "Manual",
            size: "12.1 GB",
            files: "8,301"
        }
    ]
    property int beforeId: -1
    property int afterId: -1
    property bool comparisonReady: false
    property bool comparing: false
    property string search: ""
    property string filter: "All changes"
    property var expanded: ["src", "src/components", "assets"]
    property int scanProgress: 0
    property bool scanning: false
    property string scanError: ""
    property string sheet: ""
    property string toast: ""
    property int detailId: 5
    property var cleanupSelection: []
    property bool cleanupReviewed: false
    property string cleanupResult: ""
    property bool closeToTray: true
    property bool launchAtStartup: false
    property bool notifyScheduledSuccess: false
    property int retention: 50
    property string ignoreRules: "node_modules/\nbuild/\n.git/"
    readonly property bool hasWarnings: scenario === "Scan warning"
    readonly property bool payloadMissing: scenario === "Missing snapshot"
    readonly property bool hasPair: beforeId !== -1 && afterId !== -1
    readonly property var changes: [
        {
            path: "src",
            name: "src",
            depth: 0,
            folder: true,
            status: "",
            before: "124 KB",
            after: "168 KB"
        },
        {
            path: "src/components",
            name: "components",
            depth: 1,
            folder: true,
            status: "",
            before: "48 KB",
            after: "86 KB"
        },
        {
            path: "src/components/FolderCard.qml",
            name: "FolderCard.qml",
            depth: 2,
            folder: false,
            status: "Added",
            before: "—",
            after: "24 KB"
        },
        {
            path: "src/components/SnapshotList.qml",
            name: "SnapshotList.qml",
            depth: 2,
            folder: false,
            status: "Added",
            before: "—",
            after: "14 KB"
        },
        {
            path: "src/main.cpp",
            name: "main.cpp",
            depth: 1,
            folder: false,
            status: "Modified",
            before: "8 KB",
            after: "14 KB"
        },
        {
            path: "assets",
            name: "assets",
            depth: 0,
            folder: true,
            status: "",
            before: "286 MB",
            after: "714 MB"
        },
        {
            path: "assets/hero-render.png",
            name: "hero-render.png",
            depth: 1,
            folder: false,
            status: "Added",
            before: "—",
            after: "428 MB"
        },
        {
            path: "assets/old-preview.png",
            name: "old-preview.png",
            depth: 1,
            folder: false,
            status: "Removed",
            before: "18 MB",
            after: "—"
        },
        {
            path: "README.md",
            name: "README.md",
            depth: 0,
            folder: false,
            status: "Modified",
            before: "4 KB",
            after: "6 KB"
        },
        {
            path: "notes.txt",
            name: "notes.txt",
            depth: 0,
            folder: false,
            status: "Removed",
            before: "2 KB",
            after: "—"
        }
    ]
    readonly property var displayedChanges: {
        let all = changes.slice();
        if (scenario === "Large comparison") {
            for (let i = 0; i < 2000; ++i) {
                all.push({
                    path: "generated-" + i + ".json",
                    name: "generated-" + i + ".json",
                    depth: 0,
                    folder: false,
                    status: "Added",
                    before: "—",
                    after: "2 KB"
                });
            }
        }
        const query = search.toLowerCase();
        const matching = all.filter(row => (!query || row.path.toLowerCase().includes(query)) && (filter === "All changes" || row.status === filter));
        const searching = query.length > 0 || filter !== "All changes";
        return all.filter(row => {
            if (searching) {
                return matching.some(hit => hit.path === row.path || hit.path.startsWith(row.path + "/"));
            }
            const parts = row.path.split("/");
            for (let i = 1; i < parts.length; ++i) {
                if (!expanded.includes(parts.slice(0, i).join("/")))
                    return false;
            }
            return true;
        });
    }
    readonly property var cleanupCandidates: changes.filter(row => row.status === "Added")
    function snapshot(id) {
        return snapshots.find(row => row.id === id) || {
            date: "Choose a snapshot",
            description: "Select from the timeline below"
        };
    }
    function chooseRoot(index) {
        rootIndex = index;
        beforeId = -1;
        afterId = -1;
        comparisonReady = false;
        comparing = false;
        search = "";
        filter = "All changes";
        expanded = ["src", "src/components", "assets"];
    }
    function chooseSnapshot(id) {
        if (payloadMissing && id === 5) {
            toast = "This snapshot's payload is unavailable.";
            return;
        }
        comparing = false;
        comparisonReady = false;
        if (beforeId === id) {
            beforeId = -1;
            return;
        }
        if (afterId === id) {
            afterId = -1;
            return;
        }
        if (beforeId === -1)
            beforeId = id;
        else if (afterId === -1)
            afterId = id;
        else {
            beforeId = afterId;
            afterId = id;
        }
        if (hasPair && beforeId > afterId) {
            const older = afterId;
            afterId = beforeId;
            beforeId = older;
        }
    }
    function toggleExpanded(path) {
        expanded = expanded.includes(path) ? expanded.filter(p => p !== path) : expanded.concat([path]);
    }
    function takeSnapshot() {
        if (currentRoot.archived || visibleRoots.length === 0 || scanning)
            return;
        scanError = "";
        scanProgress = 0;
        scanning = true;
    }
    function openSheet(kind, id) {
        detailId = id === undefined ? 5 : id;
        cleanupSelection = [];
        cleanupReviewed = false;
        cleanupResult = "";
        sheet = kind;
    }
    function toggleCleanup(path) {
        cleanupReviewed = false;
        cleanupSelection = cleanupSelection.includes(path) ? cleanupSelection.filter(p => p !== path) : cleanupSelection.concat([path]);
    }
    function updateRoot(name, schedule, archived) {
        const updated = roots.slice();
        updated[rootIndex] = Object.assign({}, currentRoot, {
            name: name,
            schedule: schedule,
            archived: archived
        });
        roots = updated;
        toast = "Folder preferences updated in this preview.";
    }
    function addFolder(name, path) {
        roots = roots.concat([
            {
                name: name,
                path: path,
                size: "0 B",
                files: "0",
                snapshots: 0,
                color: "#94e6c6",
                schedule: "Manual only",
                archived: false,
                note: "Ready for its first snapshot"
            }
        ]);
        scenario = "Sample library";
        chooseRoot(roots.length - 1);
        selectedSection = AppState.Folders;
        toast = "Sample folder added. Take a snapshot to try the flow.";
    }
    onScenarioChanged: {
        chooseRoot(0);
        scanning = false;
        scanError = scenario === "Scan failure" ? "Projects is unavailable. Reconnect the drive or check folder permissions, then try again." : "";
    }
}
