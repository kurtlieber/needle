import QtQuick

// "Synced with haystack", or why not.
Row {
    spacing: 8
    readonly property color dotColor: haystack.status === "synced" ? theme.foreground
                                      : haystack.status === "syncing" ? theme.accent
                                      : haystack.status === "offline" ? theme.lightForeground : theme.red
    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: 8; height: 8; radius: 4
        color: parent.dotColor
        Rectangle {
            anchors.centerIn: parent; width: 14; height: 14; radius: 7; z: -1
            color: Qt.alpha(parent.color, 0.12)
        }
    }
    Text {
        anchors.verticalCenter: parent.verticalCenter
        text: haystack.statusText
        font.family: "Archivo"; font.pixelSize: 13
        color: theme.lightForeground
    }
}
