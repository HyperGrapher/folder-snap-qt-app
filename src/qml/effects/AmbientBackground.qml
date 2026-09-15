import QtQuick
import Aura

Rectangle {
    id: background
    required property MotionPolicy motion
    property int section: 0
    property real phase: 0
    property bool shaderFailed: false
    readonly property bool shaderSupported: GraphicsInfo.api !== GraphicsInfo.Software
    onShaderSupportedChanged: {
        if (!shaderSupported) {
            console.warn("Software renderer: using static ambient gradient.");
        }
    }
    property color first: Theme.palettes[section][0]
    property color second: Theme.palettes[section][1]
    property color third: Theme.palettes[section][2]
    property color fourth: Theme.palettes[section][3]
    gradient: Gradient {
        GradientStop {
            position: 0
            color: "#29283f"
        }
        GradientStop {
            position: 1
            color: Theme.background
        }
    }
    Connections {
        target: background.motion
        function onTransitionsEnabledChanged() {
            if (!background.motion.transitionsEnabled) {
                firstAnimation.complete();
                secondAnimation.complete();
                thirdAnimation.complete();
                fourthAnimation.complete();
            }
        }
    }
    Behavior on first {
        ColorAnimation {
            id: firstAnimation
            duration: background.motion.transitionsEnabled ? Theme.paletteDuration : 0
        }
    }
    Behavior on second {
        ColorAnimation {
            id: secondAnimation
            duration: background.motion.transitionsEnabled ? Theme.paletteDuration : 0
        }
    }
    Behavior on third {
        ColorAnimation {
            id: thirdAnimation
            duration: background.motion.transitionsEnabled ? Theme.paletteDuration : 0
        }
    }
    Behavior on fourth {
        ColorAnimation {
            id: fourthAnimation
            duration: background.motion.transitionsEnabled ? Theme.paletteDuration : 0
        }
    }
    NumberAnimation on phase {
        from: 0
        to: Math.PI * 2
        duration: Theme.ambientDuration
        loops: Animation.Infinite
        running: true
        paused: !background.motion.ambientEnabled || background.shaderFailed || !background.shaderSupported
    }
    ShaderEffect {
        id: effect
        objectName: "ambientShader"
        anchors.fill: parent
        visible: !background.shaderFailed && background.shaderSupported
        property real phase: background.phase
        property vector2d resolution: Qt.vector2d(width, height)
        property color first: background.first
        property color second: background.second
        property color third: background.third
        property color fourth: background.fourth
        fragmentShader: "qrc:/shaders/ambient.frag.qsb"
        onStatusChanged: {
            if (status === ShaderEffect.Error && !background.shaderFailed) {
                background.shaderFailed = true;
                console.warn("Ambient shader unavailable; using static gradient:", log);
            }
        }
    }
}
