import QtQuick
import FolderSnap
import QtQuick.Shapes

Item {
    id: ring
    property int value: 25
    property bool animationsEnabled: true
    property real displayedValue: 25
    property color accent: Theme.accent
    implicitWidth: 180
    implicitHeight: 180
    Accessible.role: Accessible.ProgressBar
    Accessible.name: "Demo progress, " + value + " percent"
    onAnimationsEnabledChanged: {
        if (!animationsEnabled) {
            progressAnimation.stop();
            displayedValue = value;
        }
    }
    onValueChanged: {
        progressAnimation.stop();
        if (animationsEnabled) {
            progressAnimation.start();
        } else {
            displayedValue = value;
        }
    }
    Component.onCompleted: displayedValue = value
    NumberAnimation {
        id: progressAnimation
        target: ring
        property: "displayedValue"
        to: ring.value
        duration: Theme.progressDuration
        easing.type: Theme.easing
    }
    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: "#373d53"
            strokeWidth: 8
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: ring.width / 2
                centerY: ring.height / 2
                radiusX: ring.width / 2 - 8
                radiusY: ring.height / 2 - 8
                startAngle: -90
                sweepAngle: 360
            }
        }
        ShapePath {
            strokeColor: ring.accent
            strokeWidth: 8
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: ring.width / 2
                centerY: ring.height / 2
                radiusX: ring.width / 2 - 8
                radiusY: ring.height / 2 - 8
                startAngle: -90
                sweepAngle: Math.max(0.01, ring.displayedValue * 3.6)
            }
        }
    }
    Column {
        anchors.centerIn: parent
        spacing: 3
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Math.round(ring.displayedValue) + "%"
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: ring.width > 150 ? 40 : 26
            font.weight: Font.Light
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "COMPLETE"
            color: Theme.secondary
            font.family: Theme.fontFamily
            font.pixelSize: 9
            font.letterSpacing: 1.4
        }
    }
}
