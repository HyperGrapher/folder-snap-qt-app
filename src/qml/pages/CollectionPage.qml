pragma ComponentBehavior: Bound
import QtQuick
import FolderSnap
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: page
    objectName: "collectionPage"
    required property AppState appState
    required property MotionPolicy motion
    property int selectedCard: 0
    readonly property var collection: [
        {
            name: "Quiet horizons",
            kind: "A study in stillness",
            color: "#778bd0",
            second: "#343f6b",
            detail: "A soft horizon, a little breathing room. An invitation to slow down."
        },
        {
            name: "Tidal rhythm",
            kind: "Follow the gentle current",
            color: "#5aacac",
            second: "#284e69",
            detail: "Small waves and easy movement. Find a pace that feels like your own."
        },
        {
            name: "After the rain",
            kind: "Something fresh is here",
            color: "#8aac9c",
            second: "#35556b",
            detail: "Fresh color and a clear beginning. A small space for a new perspective."
        },
        {
            name: "Velvet hour",
            kind: "Between day and dream",
            color: "#a184bd",
            second: "#50426d",
            detail: "Warm lavender meets evening blue. The last light has a rhythm of its own."
        },
        {
            name: "Soft geometry",
            kind: "Order with a little wonder",
            color: "#b29a86",
            second: "#625675",
            detail: "Simple forms, unexpected balance. Familiar shapes seen a little differently."
        },
        {
            name: "Open skies",
            kind: "Make room for possibility",
            color: "#8eadd7",
            second: "#465e91",
            detail: "A wide blue canvas. Plenty of room for whatever comes next."
        }
    ]
    contentWidth: availableWidth
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ColumnLayout {
        width: page.availableWidth
        spacing: Theme.padding
        PageHeading {
            Layout.fillWidth: true
            title: "A collection of small wonders."
            subtitle: "Six little worlds. Pick one that feels like you."
        }
        GridLayout {
            Layout.fillWidth: true
            columns: page.availableWidth >= 760 ? 3 : 2
            rowSpacing: Theme.gap
            columnSpacing: Theme.gap
            Repeater {
                model: page.collection
                Button {
                    id: tile
                    required property int index
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    implicitHeight: 185
                    padding: 12
                    hoverEnabled: true
                    focusPolicy: Qt.StrongFocus
                    Accessible.name: modelData.name
                    Accessible.description: modelData.kind
                    Accessible.role: Accessible.Button
                    onClicked: page.selectedCard = index
                    Keys.onReturnPressed: clicked()
                    background: DemoCard {
                        hoverResponse: true
                        animationsEnabled: page.motion.transitionsEnabled && page.visible
                        border.color: tile.visualFocus ? Theme.text : page.selectedCard === tile.index ? Theme.accent : Theme.border
                        border.width: tile.visualFocus || page.selectedCard === tile.index ? 2 : 1
                    }
                    contentItem: ColumnLayout {
                        spacing: 10
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 12
                            gradient: Gradient {
                                GradientStop {
                                    position: 0
                                    color: tile.modelData.color
                                }
                                GradientStop {
                                    position: 1
                                    color: tile.modelData.second
                                }
                            }
                            Item {
                                anchors.centerIn: parent
                                width: 100
                                height: 90
                                rotation: tile.index * 22 - 20
                                Repeater {
                                    model: 4
                                    Rectangle {
                                        required property int index
                                        anchors.centerIn: parent
                                        width: 35 + index * 18
                                        height: 60 + index * 12
                                        radius: tile.index % 2 === 0 ? width / 2 : 14
                                        rotation: index * 14
                                        color: "transparent"
                                        border.width: 1
                                        border.color: "#aee8e7f9"
                                    }
                                }
                            }
                            Rectangle {
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 9
                                width: 20
                                height: 20
                                radius: 10
                                color: "#e8dffb"
                                visible: page.selectedCard === tile.index
                                Text {
                                    anchors.centerIn: parent
                                    text: "✓"
                                    color: "#332c47"
                                    font.pixelSize: 12
                                }
                            }
                        }
                        BodyText {
                            Layout.fillWidth: true
                            text: tile.modelData.name
                            color: Theme.text
                            font.weight: Font.DemiBold
                        }
                        BodyText {
                            Layout.fillWidth: true
                            text: tile.modelData.kind
                            font.pixelSize: 11
                            Layout.bottomMargin: 2
                        }
                    }
                }
            }
        }
        DemoCard {
            Layout.fillWidth: true
            implicitHeight: detail.implicitHeight + 40
            ColumnLayout {
                id: detail
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 20
                spacing: 8
                BodyText {
                    text: "YOUR PICK  /  " + page.collection[page.selectedCard].name
                    color: Theme.accent
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
                BodyText {
                    Layout.fillWidth: true
                    text: page.collection[page.selectedCard].detail
                }
            }
        }
    }
}
