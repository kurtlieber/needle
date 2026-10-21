import QtQuick

// The Review queues: Discoveries, Calibrate (when haystack offers it), Rate your likes, and Playlists (the picker,
// or the Playlist review once one is started; a click on it there goes back to the picker, as L does). Tab goes
// round them.
Rectangle {
    implicitWidth: row.implicitWidth + 8
    implicitHeight: 40
    radius: 10
    color: Qt.alpha(theme.darkerBackground, 0.55)

    component Tab: Rectangle {
        id: tab
        property string name
        property string label
        property string count
        readonly property bool active: review.queue === name
        width: content.implicitWidth + 28
        height: 32
        radius: 6
        color: active ? theme.lighterBackground : "transparent"
        border.width: active ? 1 : 0
        border.color: Qt.alpha(theme.foreground, 0.04)
        Row {
            id: content
            anchors.centerIn: parent
            spacing: 8
            Text {
                text: tab.label
                font.family: "Archivo"; font.pixelSize: 13; font.weight: Font.Medium
                color: tab.active ? theme.brightForeground : theme.lightForeground
            }
            Text {
                text: tab.count
                font.family: "Archivo"; font.pixelSize: 13; font.weight: Font.Medium
                font.features: { "tnum": 1 }
                color: tab.active ? theme.foreground : theme.lightForeground
            }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (tab.active && tab.name === "playlists" && review.hasPlaylistReview && !playlists.pickerOpen)
                    playlists.openPicker()
                else
                    review.queue = tab.name
            }
        }
        Accessible.role: Accessible.PageTab
        Accessible.name: tab.label + " " + tab.count
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 4
        Tab {
            name: "discoveries"; label: "Discoveries"
            count: review.discoveriesTotal === 0 ? "" : review.queueDone && review.queue === "discoveries"
                   ? "done" : review.discoveriesPosition + " of " + review.discoveriesTotal
        }
        Tab {
            name: "calibrate"; label: "Calibrate"
            visible: review.hasCalibrate
            count: review.calibrateTotal === 0 ? "" : review.queueDone && review.queue === "calibrate"
                   ? "done" : review.calibratePosition + " of " + review.calibrateTotal
        }
        Tab { name: "likes"; label: "Rate your likes"; count: String(review.likesLeft) }
        Tab {
            name: "playlists"; label: "Playlists"
            count: !review.hasPlaylistReview ? ""
                   : review.queue === "playlists" && review.queueDone ? "done"
                   : review.playlistsPosition + " of " + review.playlistsTotal
        }
    }
}
