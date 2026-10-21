import QtQuick
import QtQuick.Effects
import QtQuick.Shapes

// One calibrated scale: ticks, the gap between this track and the Baseline, the Baseline ring (foreground)
// and this track's needle (accent). `baselineY` is where the scale line runs.
Item {
    id: s
    property var trait                  // {key, min, max, major, minor}
    property var song                   // this track's Reading, or undefined
    property var base                   // the Baseline mean, or undefined
    property real baselineY: 28
    property real majorTick: 10
    property real minorTick: 5
    property real ringRadius: 6
    property real needleTop: 8          // the needle runs from here down to baselineY + needleBelow
    property real needleBelow: 14
    property real arrow: 6              // half-width of the needle's head
    property bool glow: true
    // pixels per unit on screen: the needle's glow is a texture, so it follows the window's zoom (Main.qml)
    readonly property real density: Screen.devicePixelRatio * (Window.window ? Window.window.zoom || 1 : 1)

    readonly property bool hasSong: song !== undefined && song !== null
    readonly property bool hasBase: base !== undefined && base !== null
    readonly property real sx: hasSong ? insight.position(trait.key, song) * width : 0
    readonly property real bx: hasBase ? insight.position(trait.key, base) * width : 0
    readonly property int ticks: Math.round((trait.max - trait.min) / trait.minor)
    readonly property int every: Math.round(trait.major / trait.minor)

    Rectangle { x: 0; width: s.width; y: s.baselineY; height: 1; color: theme.muted }
    Repeater {
        model: s.ticks + 1
        delegate: Rectangle {
            required property int index
            readonly property bool major: index % s.every === 0
            x: Math.round(index / s.ticks * (s.width - 1))
            width: 1
            height: major ? s.majorTick : s.minorTick
            y: s.baselineY - height
            color: major ? theme.lightForeground : theme.darkForeground
            opacity: major ? 0.7 : 0.8
        }
    }
    Rectangle {           // the gap between this track and the Baseline
        visible: s.hasSong && s.hasBase
        x: Math.min(s.sx, s.bx); width: Math.abs(s.sx - s.bx)
        y: s.baselineY - 1.5; height: 4
        color: Qt.alpha(theme.accent, 0.38)
    }
    Rectangle {           // the Baseline ring
        visible: s.hasBase
        width: s.ringRadius * 2; height: width; radius: s.ringRadius
        x: s.bx - s.ringRadius; y: s.baselineY + 0.5 - s.ringRadius
        color: theme.background
        border.width: 2; border.color: theme.foreground
    }
    Item {                // this track's needle
        id: needle
        visible: s.hasSong
        x: s.sx - s.arrow; y: s.needleTop - s.arrow * (4 / 3)
        width: s.arrow * 2; height: s.baselineY + s.needleBelow - y
        layer.enabled: s.glow
        // the texture covers the padded rect MultiEffect sets for the glow, or the needle alone before that
        layer.textureSize: layer.sourceRect.width > 0
            ? Qt.size(layer.sourceRect.width * s.density, layer.sourceRect.height * s.density)
            : Qt.size(width * s.density, height * s.density)
        layer.effect: MultiEffect {
            shadowEnabled: true; shadowColor: Qt.alpha(theme.accent, 0.6); shadowBlur: 0.35
            shadowHorizontalOffset: 0; shadowVerticalOffset: 0; autoPaddingEnabled: true
        }
        Rectangle { x: s.arrow - 1; y: s.arrow * (4 / 3); width: 2; height: needle.height - y; color: theme.accent }
        Shape {
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                fillColor: theme.accent; strokeColor: "transparent"
                startX: 0; startY: 0
                PathLine { x: s.arrow * 2; y: 0 }
                PathLine { x: s.arrow; y: s.arrow * (4 / 3) }
                PathLine { x: 0; y: 0 }
            }
        }
    }
}
