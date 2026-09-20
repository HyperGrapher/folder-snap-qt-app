import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FolderSnap

ScrollView {
    id: page
    required property AppState appState
    required property MotionPolicy motion
    contentWidth: availableWidth
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ColumnLayout {
        width: page.availableWidth
        spacing: 20
        PageHeading {
            Layout.fillWidth: true
            eyebrow: "MAKE IT YOURS"
            title: "A little more you."
            subtitle: "Thoughtful defaults. Room for your preferences."
        }
        Panel {
            Layout.fillWidth: true
            implicitHeight: 232
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 19
                RowLayout {
                    Glyph {
                        name: "sun"
                        color: Theme.violet
                    }
                    LabelText {
                        text: "Appearance & motion"
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Badge {
                        text: "Midnight"
                        tone: Theme.violet
                    }
                }
                SettingRow {
                    Layout.fillWidth: true
                    title: "Reduced motion"
                    description: "Keep the atmosphere still and switch pages instantly."
                    toggleObjectName: "reducedMotionToggle"
                    checked: page.appState.reducedMotion
                    animationsEnabled: page.motion.transitionsEnabled
                    onToggled: value => page.appState.reducedMotion = value
                }
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.border
                }
                SettingRow {
                    Layout.fillWidth: true
                    title: "Animated background"
                    description: page.appState.reducedMotion ? "Paused by reduced motion. Your preference is remembered." : "Let the ambient colors drift gently behind your workspace."
                    checked: page.appState.backgroundMotionEnabled
                    animationsEnabled: page.motion.transitionsEnabled
                    onToggled: value => page.appState.backgroundMotionEnabled = value
                }
            }
        }
        Panel {
            Layout.fillWidth: true
            implicitHeight: 293
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 18
                RowLayout {
                    Glyph {
                        name: "settings"
                        color: Theme.accent
                    }
                    LabelText {
                        text: "Fits into your day"
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                    }
                }
                SettingRow {
                    Layout.fillWidth: true
                    title: "Close to the system tray"
                    description: "Keep scheduled snapshots running after the window closes."
                    checked: page.appState.closeToTray
                    animationsEnabled: page.motion.transitionsEnabled
                    onToggled: value => page.appState.closeToTray = value
                }
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.border
                }
                SettingRow {
                    Layout.fillWidth: true
                    title: "Start with Windows"
                    description: "Start quietly in the tray when you sign in."
                    checked: page.appState.launchAtStartup
                    animationsEnabled: page.motion.transitionsEnabled
                    onToggled: value => page.appState.launchAtStartup = value
                }
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Theme.border
                }
                SettingRow {
                    Layout.fillWidth: true
                    title: "Notify before scheduled snapshots"
                    description: "Ask before starting each scheduled metadata snapshot."
                    checked: page.appState.notifyScheduledBefore
                    animationsEnabled: page.motion.transitionsEnabled
                    onToggled: value => page.appState.notifyScheduledBefore = value
                }
            }
        }
        Panel {
            Layout.fillWidth: true
            implicitHeight: 139
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 15
                RowLayout {
                    Glyph {
                        name: "snapshot"
                        color: Theme.warning
                    }
                    LabelText {
                        text: "A good starting point"
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        Layout.fillWidth: true
                        LabelText {
                            text: "Default history retention"
                            font.weight: Font.DemiBold
                        }
                        LabelText {
                            text: "Applied to folders you add in the future."
                            color: Theme.secondary
                            font.pixelSize: 11
                        }
                    }
                    SelectBox {
                        model: ["10 snapshots", "25 snapshots", "50 snapshots", "100 snapshots", "Unlimited"]
                        currentIndex: [10, 25, 50, 100, 0].indexOf(page.appState.retention)
                        onActivated: page.appState.retention = [10, 25, 50, 100, 0][currentIndex]
                    }
                }
            }
        }
        Panel {
            Layout.fillWidth: true
            implicitHeight: 164
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 22
                spacing: 12
                RowLayout {
                    Glyph {
                        name: "shield"
                        color: Theme.accent
                    }
                    LabelText {
                        text: "Your data stays with you"
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                    }
                }
                BodyText {
                    Layout.fillWidth: true
                    text: "FolderSnap saves paths and file details locally. It never copies file contents or sends them to a server."
                    font.pixelSize: 12
                }
                RowLayout {
                    ActionButton {
                        animationsEnabled: page.motion.transitionsEnabled
                        text: "Data folder"
                        primary: false
                        glyph: "folder"
                        onClicked: page.appState.openDataFolder()
                    }
                    ActionButton {
                        animationsEnabled: page.motion.transitionsEnabled
                        text: "View logs (coming soon)"
                        primary: false
                        quiet: true
                        glyph: "file"
                        enabled: false
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    ActionButton {
                        animationsEnabled: page.motion.transitionsEnabled
                        text: "Quit FolderSnap"
                        primary: false
                        quiet: true
                        onClicked: Qt.quit()
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Image {
                source: "qrc:/resources/icons/foldersnap-icon.png"
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
            }
            LabelText {
                text: "FolderSnap"
                font.weight: Font.DemiBold
            }
            LabelText {
                text: "0.1 · Local metadata"
                color: Theme.muted
                font.pixelSize: 10
            }
            Item {
                Layout.fillWidth: true
            }
            LabelText {
                text: "Made for a little peace of mind."
                color: Theme.muted
                font.pixelSize: 10
            }
        }
    }
}
