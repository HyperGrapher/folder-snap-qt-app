import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FolderSnap

Button {
    id: control
    property bool primary: true
    property bool quiet: false
    property bool danger: false
    property string glyph: ""
    property bool animationsEnabled: false
    implicitHeight: 38
    implicitWidth: contentItem.implicitWidth + 28
    horizontalPadding: 14
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: text
    scale: animationsEnabled && down ? 0.98 : 1
    Behavior on scale {
        NumberAnimation {
            duration: control.animationsEnabled ? Theme.hoverDuration : 0
        }
    }
    contentItem: RowLayout {
        spacing: 8
        Glyph {
            visible: control.glyph !== ""
            name: control.glyph
            font.pixelSize: 14
            color: control.primary ? "#133b2e" : control.danger ? Theme.danger : Theme.secondary
        }
        LabelText {
            text: control.text
            font.pixelSize: 12
            font.weight: Font.DemiBold
            color: !control.enabled ? Theme.muted : control.primary ? "#123b2f" : control.danger ? Theme.danger : Theme.text
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }
    }
    background: Rectangle {
        radius: Theme.controlRadius
        color: !control.enabled ? "#273235" : control.primary ? (control.hovered ? "#b2f3db" : Theme.accent) : control.hovered ? Theme.surfaceRaised : control.quiet ? "transparent" : Theme.surface
        border.color: control.visualFocus ? Theme.accent : control.primary || control.quiet ? "transparent" : Theme.border
        border.width: control.visualFocus ? 2 : 1
    }
    Keys.onReturnPressed: {
        if (enabled)
            clicked();
    }
}
