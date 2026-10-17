import QtQuick
import QtQuick.Effects
import QtQuick.Shapes

// The cover's own pixels as a heavily blurred, dimmed glow behind everything (an image, not a palette),
// veiled into the theme background. A toggle (G).
Item {
    id: b
    property url source
    anchors.fill: parent

    Image {
        id: tiny
        // a tiny decode, scaled up: most of the blur for free
        source: b.source
        sourceSize.width: 64
        sourceSize.height: 64
        fillMode: Image.PreserveAspectCrop
        smooth: true
        visible: false
        anchors.fill: parent
        anchors.margins: -120
    }
    MultiEffect {
        anchors.fill: tiny
        source: tiny
        blurEnabled: true
        blur: 1.0
        blurMax: 64
        saturation: 0.15
        opacity: tiny.status === Image.Ready ? 0.30 : 0
        Behavior on opacity { enabled: !prefs.reducedMotion; NumberAnimation { duration: 400 } }
    }
    Shape {
        anchors.fill: parent
        ShapePath {
            strokeColor: "transparent"
            fillGradient: RadialGradient {
                centerX: b.width * 0.2; centerY: b.height * 0.3
                focalX: centerX; focalY: centerY
                centerRadius: Math.max(b.width * 1.2, b.height * 0.9) * 0.7
                GradientStop { position: 0.0; color: Qt.alpha(theme.background, 0) }
                GradientStop { position: 1.0; color: theme.background }
            }
            startX: 0; startY: 0
            PathLine { x: b.width; y: 0 }
            PathLine { x: b.width; y: b.height }
            PathLine { x: 0; y: b.height }
            PathLine { x: 0; y: 0 }
        }
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.alpha(theme.background, 0.35) }
            GradientStop { position: 1.0; color: Qt.alpha(theme.background, 0.85) }
        }
    }
}
