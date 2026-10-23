import QtQuick

// Every shortcut (?, or the keyboard button). Esc or ? closes it.
Rectangle {
    id: h
    property bool open: false
    anchors.fill: parent
    visible: open
    color: Qt.alpha(theme.darkerBackground, 0.82)
    MouseArea { anchors.fill: parent; onClicked: h.open = false }

    readonly property var keys: [
        ["1 – 5", "Rate (keeps playing)"], ["Shift + 1 – 5", "Rate and play the next track"],
        ["P", "Pass"], ["X", "Dislike"], ["S", "Swap a possible cover for its original (Rate your likes, Playlists)"],
        ["Space", "Play or pause"], ["← →", "Back or ahead 10 s"],
        ["J", "Jump ahead 30 s"], ["N", "Next track"], ["B", "Back to the previous track"],
        ["Tab", "Next queue: Discoveries, Calibrate, Rate your likes, Playlists"], ["L", "Choose playlists to review"],
        ["G", "Cover glow on or off"], ["M", "Motion: follow the desktop, or not"], ["?", "These shortcuts"], ["Q", "Quit"]]
    readonly property var pickerKeys: [
        ["← → ↑ ↓", "Move between playlists"], ["Space", "Add to the review, or take out"], ["A", "Pick every one shown"],
        ["Enter", "Start the review (with none picked: the one in focus)"], ["/", "Filter by name"],
        ["F", "Show all, Not started, In progress, Done"], ["O", "Sort: Spotify order, Most to review, Name"],
        ["V", "Skip songs with a verdict, or play them again"], ["S", "Shuffle on or off"],
        ["Esc", "Back to the review"]]
    property bool picking: false

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 520)
        height: col.implicitHeight + 48
        radius: 16
        color: theme.lighterBackground
        border.width: 1; border.color: Qt.alpha(theme.foreground, 0.06)
        Column {
            id: col
            x: 24; y: 24; width: parent.width - 48
            spacing: 10
            Text {
                text: h.picking ? "Keyboard, choosing playlists" : "Keyboard"
                font.family: "Archivo"; font.pixelSize: 17; font.weight: Font.Bold
                font.variableAxes: { "wdth": 112 }
                color: theme.brightForeground
                bottomPadding: 6
            }
            Repeater {
                model: h.picking ? h.pickerKeys : h.keys
                delegate: Row {
                    required property var modelData
                    spacing: 16
                    Item {
                        width: 130; height: 22
                        Row {
                            spacing: 4
                            Repeater {
                                model: modelData[0].split(" + ")
                                delegate: Kbd { required property string modelData; text: modelData }
                            }
                        }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData[1]
                        font.family: "Archivo"; font.pointSize: 10.125
                        color: theme.foreground
                    }
                }
            }
            Text {
                topPadding: 8
                width: parent.width; wrapMode: Text.WordWrap
                text: "Motion: " + (prefs.motion === "system" ? "following the desktop" : prefs.motion === "reduce" ? "reduced" : "full")
                      + (prefs.reducedMotion ? " (the beat pulse is still and the spectrum settles slowly)." : ".")
                font.family: "Archivo"; font.pointSize: 9.375
                color: theme.lightForeground
            }
        }
    }
}
