import QtQuick
import FolderSnap

Rectangle {
    id: card
    property bool hoverResponse: false
    property bool animationsEnabled: true
    onAnimationsEnabledChanged: {
        if (!animationsEnabled) {
            scaleAnimation.complete();
            colorAnimation.complete();
        }
    }
    radius: Theme.cardRadius
    color: hoverResponse && hover.hovered ? Theme.surfaceRaised : Theme.surface
    border.color: hoverResponse && hover.hovered ? "#696184" : Theme.border
    border.width: 1
    scale: hoverResponse && hover.hovered && animationsEnabled ? 1.008 : 1
    Behavior on scale {
        NumberAnimation {
            id: scaleAnimation
            duration: card.animationsEnabled ? Theme.hoverDuration : 0
            easing.type: Theme.easing
        }
    }
    Behavior on color {
        ColorAnimation {
            id: colorAnimation
            duration: card.animationsEnabled ? Theme.hoverDuration : 0
        }
    }
    HoverHandler {
        id: hover
        enabled: card.hoverResponse
    }
}
