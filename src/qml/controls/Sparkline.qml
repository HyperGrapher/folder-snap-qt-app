import QtQuick
import FolderSnap

Canvas {
    id: chart
    property color tone: Theme.accent
    property var values: [24, 25, 22, 30, 28, 37, 35, 39, 36, 44, 43, 52]
    onToneChanged: requestPaint()
    onValuesChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        ctx.beginPath();
        for (let i = 0; i < values.length; ++i) {
            const x = i * width / (values.length - 1);
            const y = height - (values[i] / 60) * (height - 4);
            if (i === 0)
                ctx.moveTo(x, y);
            else
                ctx.lineTo(x, y);
        }
        ctx.strokeStyle = tone;
        ctx.lineWidth = 1.7;
        ctx.stroke();
        ctx.lineTo(width, height);
        ctx.lineTo(0, height);
        ctx.closePath();
        const fill = ctx.createLinearGradient(0, 0, 0, height);
        fill.addColorStop(0, Qt.rgba(tone.r, tone.g, tone.b, 0.17));
        fill.addColorStop(1, "transparent");
        ctx.fillStyle = fill;
        ctx.fill();
    }
}
