import QtQuick
import QtQuick.Shapes

Item {
    id: ring

    property real recall: -1
    property real thickness: 3
    readonly property bool learned: recall >= 0
    readonly property real sweep: learned ? Math.min(recall, 1) * 360 : 360
    property real shownSweep: sweep
    readonly property real radius: (Math.min(width, height) - thickness) / 2

    implicitWidth: 32
    implicitHeight: 32

    Behavior on shownSweep {
        enabled: !Motion.reduced
        NumberAnimation { duration: Motion.decay; easing.type: Easing.BezierSpline; easing.bezierCurve: Motion.curve }
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: "transparent"
            strokeColor: ring.learned ? Theme.ringTrack : "transparent"
            strokeWidth: ring.thickness
            PathAngleArc {
                centerX: ring.width / 2
                centerY: ring.height / 2
                radiusX: ring.radius
                radiusY: ring.radius
                startAngle: -90
                sweepAngle: 360
            }
        }

        ShapePath {
            fillColor: "transparent"
            strokeColor: Theme.ringColor(ring.recall)
            strokeWidth: ring.thickness
            capStyle: ring.learned ? ShapePath.RoundCap : ShapePath.FlatCap
            strokeStyle: ring.learned ? ShapePath.SolidLine : ShapePath.DashLine
            dashPattern: [1.5, 1.5]
            PathAngleArc {
                centerX: ring.width / 2
                centerY: ring.height / 2
                radiusX: ring.radius
                radiusY: ring.radius
                startAngle: -90
                sweepAngle: ring.shownSweep
            }
        }
    }
}
