import QtQuick
import QtQuick.Effects

// The beat pulse: an accent dot that beats at the track's measured tempo while it plays. Still with
// reduced motion.
Item {
    id: p
    property real bpm: 0
    property bool playing: false
    property bool compact: false
    implicitWidth: compact ? 26 : 92
    implicitHeight: implicitWidth

    readonly property bool beating: playing && bpm > 0 && !prefs.reducedMotion
    readonly property real dot: compact ? 14 : 32
    // pixels per unit on screen: the dot's glow is a texture, so it follows the window's zoom (Main.qml)
    readonly property real density: Screen.devicePixelRatio * (Window.window ? Window.window.zoom || 1 : 1)

    Rectangle {           // outer ring
        anchors.centerIn: parent
        width: p.compact ? p.dot + 26 : 88; height: width; radius: width / 2
        color: p.compact ? Qt.alpha(theme.accent, 0.06) : "transparent"
        border.width: p.compact ? 0 : 1.5
        border.color: Qt.alpha(theme.accent, 0.18)
        scale: beat.ring
    }
    Rectangle {           // middle ring
        anchors.centerIn: parent
        width: p.compact ? p.dot + 12 : 60; height: width; radius: width / 2
        color: p.compact ? Qt.alpha(theme.accent, 0.15) : "transparent"
        border.width: p.compact ? 0 : 1.5
        border.color: Qt.alpha(theme.accent, 0.45 + 0.35 * beat.flash)
    }
    Rectangle {           // the dot, glowing
        anchors.centerIn: parent
        width: p.dot; height: width; radius: width / 2
        color: theme.accent
        scale: 1 + 0.14 * beat.flash
        layer.enabled: true
        // the texture covers the padded rect MultiEffect sets for the glow, or the dot alone before that
        layer.textureSize: layer.sourceRect.width > 0
            ? Qt.size(layer.sourceRect.width * p.density, layer.sourceRect.height * p.density)
            : Qt.size(width * p.density, height * p.density)
        layer.effect: MultiEffect {
            shadowEnabled: true; shadowColor: Qt.alpha(theme.accent, 0.75); shadowBlur: 0.8
            shadowHorizontalOffset: 0; shadowVerticalOffset: 0; autoPaddingEnabled: true
        }
    }

    QtObject {
        id: beat
        property real flash: 0
        property real ring: 1
    }
    SequentialAnimation {
        id: kick
        ParallelAnimation {
            NumberAnimation { target: beat; property: "flash"; to: 1; duration: 60; easing.type: Easing.OutQuad }
            NumberAnimation { target: beat; property: "ring"; to: 1.04; duration: 60 }
        }
        ParallelAnimation {
            NumberAnimation { target: beat; property: "flash"; to: 0; duration: Math.max(120, 60000 / Math.max(p.bpm, 1) * 0.6); easing.type: Easing.OutCubic }
            NumberAnimation { target: beat; property: "ring"; to: 1; duration: 260; easing.type: Easing.OutCubic }
        }
    }
    Timer {
        interval: p.bpm > 0 ? 60000 / p.bpm : 1000
        repeat: true
        running: p.beating
        triggeredOnStart: true
        onTriggered: kick.restart()
    }
    onBeatingChanged: if (!beating) { kick.stop(); beat.flash = 0; beat.ring = 1 }
}
