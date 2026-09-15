import QtQuick
import Aura
import QtQuick.Controls

Button {
    id: control
    property bool primary: true
    property bool animationsEnabled: true
    implicitHeight: 44
    implicitWidth: Math.max(100, contentItem.implicitWidth + 36)
    padding: Theme.medium
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: text
    scale: animationsEnabled && down ? 0.98 : 1
    onAnimationsEnabledChanged: {
        if (!animationsEnabled) {
            scaleAnimation.complete();
            colorAnimation.complete();
        }
    }
    Behavior on scale {
        NumberAnimation {
            id: scaleAnimation
            duration: control.animationsEnabled ? Theme.hoverDuration : 0
        }
    }
    contentItem: Text {
        text: control.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.bodySize
        font.weight: Font.DemiBold
        color: !control.enabled ? Theme.muted : control.primary ? "#19162d" : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: Theme.controlRadius
        color: !control.enabled ? "#2b2d3c" : control.primary ? (control.down ? "#9683ed" : control.hovered ? "#c0b4ff" : Theme.accent) : (control.down ? "#35384e" : control.hovered ? Theme.surfaceRaised : Theme.surface)
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus ? Theme.text : control.primary ? "transparent" : Theme.border
        Behavior on color {
            ColorAnimation {
                id: colorAnimation
                duration: control.animationsEnabled ? Theme.hoverDuration : 0
            }
        }
    }
    Keys.onReturnPressed: {
        if (enabled) {
            clicked();
        }
    }
}
