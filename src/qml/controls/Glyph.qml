import QtQuick
import QtQuick.Shapes
import FolderSnap

Item {
    id: glyph
    property string name: "folder"
    property color color: Theme.secondary
    property font font: Qt.font({
        pixelSize: 16
    })
    implicitWidth: font.pixelSize
    implicitHeight: font.pixelSize
    readonly property var paths: ({
            folder: "M3 6 Q3 4 5 4 L9 4 L12 7 L19 7 Q21 7 21 9 L21 18 Q21 20 19 20 L5 20 Q3 20 3 18 Z",
            overview: "M3 10 L12 3 L21 10 M5 9 L5 21 L10 21 L10 15 L14 15 L14 21 L19 21 L19 9",
            compare: "M3 7 L21 7 M17 3 L21 7 L17 11 M21 17 L3 17 M7 13 L3 17 L7 21",
            settings: "M9 3 L15 3 L16 6 L19 7 L22 10 L20 13 L20 17 L16 18 L14 21 L10 21 L8 18 L4 17 L4 13 L2 10 L5 7 L8 6 Z M16 12 A4 4 0 1 0 8 12 A4 4 0 1 0 16 12",
            snapshot: "M3 7 L7 7 L9 4 L15 4 L17 7 L21 7 L21 20 L3 20 Z M16 13 A4 4 0 1 0 8 13 A4 4 0 1 0 16 13",
            clock: "M21 12 A9 9 0 1 0 3 12 A9 9 0 1 0 21 12 M12 6 L12 12 L16 14",
            plus: "M12 4 L12 20 M4 12 L20 12",
            arrow: "M4 12 L20 12 M14 6 L20 12 L14 18",
            chevron: "M9 5 L16 12 L9 19",
            down: "M5 9 L12 16 L19 9",
            search: "M16 10 A6 6 0 1 0 4 10 A6 6 0 1 0 16 10 M15 15 L21 21",
            file: "M5 3 L14 3 L20 9 L20 21 L5 21 Z M14 3 L14 9 L20 9 M8 13 L16 13 M8 17 L14 17",
            check: "M4 12 L9 17 L20 6",
            close: "M5 5 L19 19 M19 5 L5 19",
            export: "M12 16 L12 3 M7 8 L12 3 L17 8 M4 14 L4 21 L20 21 L20 14",
            more: "M5 12 L5.1 12 M12 12 L12.1 12 M19 12 L19.1 12",
            shield: "M12 3 L21 6 L20 14 Q18 19 12 22 Q6 19 4 14 L3 6 Z M8 12 L11 15 L16 9",
            warning: "M12 3 L22 21 L2 21 Z M12 9 L12 14 M12 17 L12 17.2",
            trash: "M3 6 L21 6 M9 6 L9 3 L15 3 L15 6 M5 6 L6 21 L18 21 L19 6 M10 10 L10 17 M14 10 L14 17",
            link: "M14 3 L21 3 L21 10 M21 3 L11 13 M10 5 L4 5 L4 21 L20 21 L20 15",
            activity: "M2 12 L7 12 L10 4 L14 20 L17 12 L22 12",
            sun: "M16 12 A4 4 0 1 0 8 12 A4 4 0 1 0 16 12 M12 1 L12 4 M12 20 L12 23 M1 12 L4 12 M20 12 L23 12 M4 4 L6 6 M18 18 L20 20 M4 20 L6 18 M18 6 L20 4",
            archive: "M3 3 L21 3 L21 8 L3 8 Z M5 8 L5 21 L19 21 L19 8 M9 12 L15 12",
            pause: "M8 4 L8 20 M16 4 L16 20"
        })
    Shape {
        width: 24
        height: 24
        scale: glyph.font.pixelSize / 24
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeWidth: 1.6
            strokeColor: glyph.color
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg {
                path: glyph.paths[glyph.name] || glyph.paths.file
            }
        }
    }
}
