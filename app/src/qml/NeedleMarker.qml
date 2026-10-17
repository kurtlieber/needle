import QtQuick
import QtQuick.Shapes

// The "This track" marker of the legend: the needle's head and stem.
Shape {
    width: 14
    height: 18
    preferredRendererType: Shape.CurveRenderer
    ShapePath {
        fillColor: theme.accent; strokeColor: "transparent"
        PathSvg { path: "M3 1h8l-4 6z M6 5h2v12h-2z" }
    }
}
