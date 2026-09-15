import QtQuick
import FolderSnap

Item {
    id: host
    required property AppState appState
    required property MotionPolicy motion
    readonly property int currentIndex: appState.selectedSection
    property bool isTransitioning: false
    readonly property var pages: [overview, collection, activity, settings]
    property int previousIndex: 0
    property bool initialized: false
    clip: true

    function settle() {
        for (let i = 0; i < pages.length; ++i) {
            pages[i].stop();
            pages[i].opacity = i === currentIndex ? 1 : 0;
            pages[i].x = 0;
            pages[i].visible = i === currentIndex;
            pages[i].enabled = i === currentIndex;
        }
        isTransitioning = false;
    }
    function selectPage(index) {
        appState.selectedSection = index;
    }
    function transition() {
        if (!initialized) {
            return;
        }
        const direction = currentIndex >= previousIndex ? 1 : -1;
        previousIndex = currentIndex;
        if (!motion.transitionsEnabled) {
            settle();
            return;
        }
        isTransitioning = true;
        for (let i = 0; i < pages.length; ++i) {
            const page = pages[i];
            page.stop();
            page.enabled = false;
            page.focus = false;
            if (i === currentIndex && page.opacity === 0) {
                page.x = 12 * direction;
            }
            page.visible = page.opacity > 0 || i === currentIndex;
            page.animateTo(i === currentIndex ? 1 : 0, i === currentIndex ? 0 : -12 * direction, Theme.pageDuration);
        }
    }
    function finishIfReady() {
        for (let i = 0; i < pages.length; ++i) {
            if (pages[i].animating) {
                return;
            }
        }
        settle();
    }
    onCurrentIndexChanged: transition()
    Connections {
        target: host.motion
        function onTransitionsEnabledChanged() {
            if (!host.motion.transitionsEnabled) {
                host.settle();
            }
        }
    }
    Component.onCompleted: {
        initialized = true;
        previousIndex = currentIndex;
        settle();
    }
    PageFrame {
        id: overview
        width: host.width
        height: host.height
        onFinished: host.finishIfReady()
        OverviewPage {
            anchors.fill: parent
            appState: host.appState
            motion: host.motion
        }
    }
    PageFrame {
        id: collection
        width: host.width
        height: host.height
        onFinished: host.finishIfReady()
        CollectionPage {
            anchors.fill: parent
            appState: host.appState
            motion: host.motion
        }
    }
    PageFrame {
        id: activity
        width: host.width
        height: host.height
        onFinished: host.finishIfReady()
        ActivityPage {
            anchors.fill: parent
            appState: host.appState
            motion: host.motion
        }
    }
    PageFrame {
        id: settings
        width: host.width
        height: host.height
        onFinished: host.finishIfReady()
        SettingsPage {
            anchors.fill: parent
            appState: host.appState
            motion: host.motion
        }
    }
}
