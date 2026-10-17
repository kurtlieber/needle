import QtQuick

// The Playlist review in progress: its playlists' covers and how many, next to the queue tabs. Click it, or press
// L, to go back to the picker.
Rectangle {
    id: chip
    visible: playlists.hasSession
    implicitWidth: row.implicitWidth + 20
    implicitHeight: 40
    radius: 10
    color: Qt.alpha(theme.foreground, area.containsMouse ? 0.08 : 0.04)
    border.width: 1
    border.color: Qt.alpha(theme.foreground, 0.06)
    readonly property var lists: playlists.sessionPlaylists
    Row {
        id: row
        anchors.centerIn: parent
        spacing: 10
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: -8
            Repeater {
                model: chip.lists.slice(0, 3)
                delegate: Cover {
                    required property var modelData
                    width: 26; height: 26; radius: 5; shadow: false; placeholder: "playlist"
                    source: modelData.coverSource || ""
                }
            }
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: chip.lists.length === 1 ? chip.lists[0].name : chip.lists.length + " playlists"
            width: Math.min(implicitWidth, 220); elide: Text.ElideRight
            font.family: "Archivo"; font.pixelSize: 13; font.weight: Font.Medium
            color: theme.foreground
        }
        Kbd { text: "L"; anchors.verticalCenter: parent.verticalCenter }
    }
    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: { review.queue = "playlists"; playlists.openPicker() }
    }
    Accessible.role: Accessible.Button
    Accessible.name: "Choose playlists to review"
}
