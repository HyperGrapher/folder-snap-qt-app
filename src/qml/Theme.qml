pragma Singleton
import QtQuick

QtObject {
    readonly property color background: "#101416"
    readonly property color sidebar: "#13191b"
    readonly property color surface: "#1b2327"
    readonly property color surfaceRaised: "#253136"
    readonly property color text: "#edf3f2"
    readonly property color secondary: "#a4b4b9"
    readonly property color muted: "#7e949b"
    readonly property color accent: "#94e6c6"
    readonly property color border: "#304047"
    readonly property color success: "#94e6c6"
    readonly property color violet: "#b7a8e6"
    readonly property color warning: "#e9c387"
    readonly property color danger: "#eda6a6"
    readonly property string fontFamily: "Segoe UI"
    readonly property int bodySize: 13
    readonly property int titleSize: 30
    readonly property int small: 4
    readonly property int compact: 8
    readonly property int medium: 12
    readonly property int gap: 16
    readonly property int padding: 24
    readonly property int large: 32
    readonly property int controlRadius: 8
    readonly property int cardRadius: 14
    readonly property int titleHeight: 42
    readonly property int sidebarWidth: 210
    readonly property int windowButtonWidth: 46
    readonly property int hoverDuration: 120
    readonly property int pageDuration: 220
    readonly property int paletteDuration: 700
    readonly property int progressDuration: 250
    readonly property int ambientDuration: 48000
    readonly property int easing: Easing.OutCubic
    readonly property var palettes: [["#25584e", "#353857", "#243a4a", "#30453e"], ["#294948", "#283b56", "#36384d", "#3d4a3a"], ["#293e57", "#3e355b", "#294e50", "#244644"], ["#3b3551", "#294e4b", "#353c54", "#3a493e"]]
}
