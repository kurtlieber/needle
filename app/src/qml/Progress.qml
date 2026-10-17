import QtQuick

// Playback position; click or drag to seek.
Item {
    id: p
    property bool knob: true
    implicitHeight: 12
    readonly property real fraction: engine.durationMs > 0 ? Math.min(1, engine.positionMs / engine.durationMs) : 0
    Rectangle {
        id: track
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width; height: 4; radius: 2
        color: Qt.alpha(theme.foreground, 0.08)
        Rectangle { width: parent.width * p.fraction; height: parent.height; radius: 2; color: theme.foreground }
    }
    Rectangle {
        visible: p.knob && engine.durationMs > 0
        x: track.width * p.fraction - 6
        anchors.verticalCenter: parent.verticalCenter
        width: 12; height: 12; radius: 6
        color: theme.brightForeground
        border.width: 3
        border.color: Qt.alpha(theme.darkerBackground, 0.5)
    }
    MouseArea {
        anchors.fill: parent
        anchors.topMargin: -8; anchors.bottomMargin: -8
        cursorShape: Qt.PointingHandCursor
        function seekTo(x) { if (engine.durationMs > 0) engine.seek(Math.max(0, Math.min(1, x / width)) * engine.durationMs) }
        onPressed: (m) => seekTo(m.x)
        onPositionChanged: (m) => { if (pressed) seekTo(m.x) }
    }
    Accessible.role: Accessible.Slider
    Accessible.name: "Playback position"
}
