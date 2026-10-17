import QtQuick
import QtQuick.Shapes

// The mockup's icons (24-unit viewBox), drawn as shapes so they take any theme colour.
Item {
    id: icon
    property string name
    property color color: theme.foreground
    property real size: 24
    implicitWidth: size
    implicitHeight: size

    readonly property var specs: ({
        needle: [{ d: "M21 12a9 9 0 1 1-18 0a9 9 0 1 1 18 0", stroke: 2, opacity: 0.45 },
                 { d: "M14.2 12a2.2 2.2 0 1 1-4.4 0a2.2 2.2 0 1 1 4.4 0", fill: true },
                 { d: "M12 12 L19 4.5", stroke: 2 }],
        star: [{ d: "M12 2.6l2.85 6.05 6.6.78-4.88 4.53 1.3 6.52L12 17.2l-5.87 3.28 1.3-6.52-4.88-4.53 6.6-.78z", fill: true }],
        starOutline: [{ d: "M12 2.6l2.85 6.05 6.6.78-4.88 4.53 1.3 6.52L12 17.2l-5.87 3.28 1.3-6.52-4.88-4.53 6.6-.78z", stroke: 1.6 }],
        ban: [{ d: "M20.5 12a8.5 8.5 0 1 1-17 0a8.5 8.5 0 1 1 17 0", stroke: 2 }, { d: "M6 6l12 12", stroke: 2 }],
        pass: [{ d: "M4 12h12", stroke: 2 }, { d: "M12 7l5 5-5 5", stroke: 2 }, { d: "M20 5v14", stroke: 2 }],
        play: [{ d: "M8 5.2v13.6a.8.8 0 0 0 1.2.7l10.6-6.8a.8.8 0 0 0 0-1.4L9.2 4.5A.8.8 0 0 0 8 5.2z", fill: true }],
        pause: [{ d: "M7.2 5h1.6a1.2 1.2 0 0 1 1.2 1.2v11.6a1.2 1.2 0 0 1-1.2 1.2H7.2A1.2 1.2 0 0 1 6 17.8V6.2A1.2 1.2 0 0 1 7.2 5z", fill: true },
                { d: "M15.2 5h1.6a1.2 1.2 0 0 1 1.2 1.2v11.6a1.2 1.2 0 0 1-1.2 1.2h-1.6a1.2 1.2 0 0 1-1.2-1.2V6.2A1.2 1.2 0 0 1 15.2 5z", fill: true }],
        next: [{ d: "M5 6.2v11.6a.8.8 0 0 0 1.25.66L14.5 12.7a.8.8 0 0 0 0-1.4L6.25 5.54A.8.8 0 0 0 5 6.2z", fill: true },
               { d: "M17.7 5h.2a1.2 1.2 0 0 1 1.2 1.2v11.6a1.2 1.2 0 0 1-1.2 1.2h-.2a1.2 1.2 0 0 1-1.2-1.2V6.2A1.2 1.2 0 0 1 17.7 5z", fill: true }],
        prev: [{ d: "M19 6.2v11.6a.8.8 0 0 1-1.25.66L9.5 12.7a.8.8 0 0 1 0-1.4l8.25-5.76A.8.8 0 0 1 19 6.2z", fill: true },
               { d: "M6.1 5h.2a1.2 1.2 0 0 1 1.2 1.2v11.6a1.2 1.2 0 0 1-1.2 1.2h-.2a1.2 1.2 0 0 1-1.2-1.2V6.2A1.2 1.2 0 0 1 6.1 5z", fill: true }],
        keyboard: [{ d: "M5 6h14a2.5 2.5 0 0 1 2.5 2.5v7a2.5 2.5 0 0 1-2.5 2.5H5a2.5 2.5 0 0 1-2.5-2.5v-7A2.5 2.5 0 0 1 5 6z", stroke: 1.6 },
                   { d: "M6.5 10h.01M10 10h.01M13.5 10h.01M17 10h.01M8 14h8", stroke: 1.6 }],
        heart: [{ d: "M12 20.3s-7.6-4.5-9.1-9.3C1.8 7.4 4.2 4.4 7.4 4.4c2 0 3.4 1.1 4.6 2.6 1.2-1.5 2.6-2.6 4.6-2.6 3.2 0 5.6 3 4.5 6.6-1.5 4.8-9.1 9.3-9.1 9.3z", fill: true }],
        check: [{ d: "M5.5 12.5l4.2 4.2L18.5 7.8", stroke: 2.6 }],
        search: [{ d: "M17.5 11a6.5 6.5 0 1 1-13 0a6.5 6.5 0 1 1 13 0", stroke: 1.8 }, { d: "M16 16l4.5 4.5", stroke: 1.8 }],
        playlist: [{ d: "M4 6h11M4 11h11M4 16h7", stroke: 2 }, { d: "M20 16.5a2.5 2.5 0 1 1-5 0a2.5 2.5 0 1 1 5 0", stroke: 2 },
                   { d: "M20 16.5V7l2 1", stroke: 2 }]
    })

    readonly property var parts: specs[name] || []

    // At most three parts per icon: a fixed set of paths, each bound to its part (or empty).
    component Part: ShapePath {
        property var spec
        fillColor: spec && spec.fill ? Qt.alpha(icon.color, spec.opacity || 1) : "transparent"
        strokeColor: spec && spec.stroke ? Qt.alpha(icon.color, spec.opacity || 1) : "transparent"
        strokeWidth: spec && spec.stroke ? spec.stroke : 0
        capStyle: ShapePath.RoundCap
        joinStyle: ShapePath.RoundJoin
        PathSvg { path: spec ? spec.d : "" }
    }

    Shape {
        anchors.centerIn: parent
        width: 24
        height: 24
        scale: icon.size / 24
        preferredRendererType: Shape.CurveRenderer
        Part { spec: icon.parts[0] }
        Part { spec: icon.parts[1] }
        Part { spec: icon.parts[2] }
    }
}
