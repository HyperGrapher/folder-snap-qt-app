pragma Singleton
import QtQuick

QtObject {
    readonly property color background: "#10121c"
    readonly property color sidebar: "#151824"
    readonly property color surface: "#202536"
    readonly property color surfaceRaised: "#2b3045"
    readonly property color text: "#f5f6fc"
    readonly property color secondary: "#b3bdd1"
    readonly property color muted: "#8591ab"
    readonly property color accent: "#a99aff"
    readonly property color border: "#34394e"
    readonly property color success: "#83dbc2"
    readonly property string fontFamily: "Segoe UI"
    readonly property int bodySize: 14
    readonly property int titleSize: 30
    readonly property int small: 4
    readonly property int compact: 8
    readonly property int medium: 12
    readonly property int gap: 16
    readonly property int padding: 24
    readonly property int large: 32
    readonly property int controlRadius: 10
    readonly property int cardRadius: 18
    readonly property int titleHeight: 48
    readonly property int sidebarWidth: 216
    readonly property int windowButtonWidth: 46
    readonly property int hoverDuration: 120
    readonly property int pageDuration: 200
    readonly property int paletteDuration: 600
    readonly property int progressDuration: 250
    readonly property int ambientDuration: 40000
    readonly property int easing: Easing.OutCubic
    readonly property var palettes: [["#7760d8", "#4775bb", "#514179", "#273c69"], ["#427dcc", "#45b7ad", "#3d609c", "#25526c"], ["#36a799", "#7bb791", "#306f83", "#294e4f"], ["#9d7fca", "#bb7c9c", "#645391", "#5b385a"]]
}
