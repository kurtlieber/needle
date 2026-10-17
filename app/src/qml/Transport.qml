import QtQuick

Row {
    property bool compact: false
    spacing: compact ? 8 : 6
    IconButton {
        icon: "prev"; label: "Back"; flat: true; iconColor: theme.foreground
        width: compact ? 34 : 40; height: width; radius: 10; iconSize: compact ? 17 : 20
        anchors.verticalCenter: parent.verticalCenter
        onClicked: review.back()
    }
    IconButton {
        icon: engine.playing ? "pause" : "play"; label: engine.playing ? "Pause" : "Play"; filled: true
        width: compact ? 40 : 48; height: width; iconSize: compact ? 18 : 22
        anchors.verticalCenter: parent.verticalCenter
        onClicked: engine.toggle()
    }
    IconButton {
        icon: "next"; label: "Next"; flat: true; iconColor: theme.foreground
        width: compact ? 34 : 40; height: width; radius: 10; iconSize: compact ? 17 : 20
        anchors.verticalCenter: parent.verticalCenter
        onClicked: review.next()
    }
}
