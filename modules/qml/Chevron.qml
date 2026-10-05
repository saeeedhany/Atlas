import QtQuick
import QtQuick.Shapes

Shape {
    id: chevron

    property color color: Theme.onSurfaceMuted

    implicitWidth: 10
    implicitHeight: 6
    preferredRendererType: Shape.CurveRenderer

    ShapePath {
        fillColor: "transparent"
        strokeColor: chevron.color
        strokeWidth: 1.5
        capStyle: ShapePath.RoundCap
        joinStyle: ShapePath.RoundJoin
        startX: 1
        startY: 1
        PathLine { x: chevron.width / 2; y: chevron.height - 1 }
        PathLine { x: chevron.width - 1; y: 1 }
    }
}
