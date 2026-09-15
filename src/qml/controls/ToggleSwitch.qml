import QtQuick
import Aura
import QtQuick.Controls

Switch {
    id: control
    property bool animationsEnabled: true
    onAnimationsEnabledChanged: {
        if (!animationsEnabled) {
            thumbAnimation.complete();
            trackAnimation.complete();
        }
    }
    implicitWidth: 52
    implicitHeight: 32
    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: text
    indicator: Rectangle {
        implicitWidth: 52
        implicitHeight: 30
        x: 0
        y: (control.height - height) / 2
        radius: 15
        color: !control.enabled ? Theme.surface : control.checked ? Theme.accent : "#41465d"
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus ? Theme.text : control.hovered ? Theme.secondary : "transparent"
        Behavior on color {
            ColorAnimation {
                id: trackAnimation
                duration: control.animationsEnabled ? Theme.hoverDuration : 0
            }
        }
        Rectangle {
            x: control.checked ? 26 : 4
            y: 4
            width: 22
            height: 22
            radius: 11
            color: control.enabled ? Theme.text : Theme.muted
            Behavior on x {
                NumberAnimation {
                    id: thumbAnimation
                    duration: control.animationsEnabled ? Theme.hoverDuration : 0
                    easing.type: Theme.easing
                }
            }
        }
    }
    contentItem: Item {}
}
