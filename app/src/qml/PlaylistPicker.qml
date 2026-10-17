import QtQuick
import QtQuick.Layouts

// The Playlist review's picker, option A, "Cover wall" (mockups/playlists-a-wall.png): every playlist in your
// Spotify library as a cover with its progress, a details card on the one in focus, and a dock that adds up the
// pick and starts the review. Keyboard first: arrows move, Space picks, A picks every one shown, Enter starts.
Item {
    id: picker
    property bool compact: false
    signal helpRequested()
    signal keysBack()            // the name filter is done: keys go back to the window

    readonly property int columns: Math.max(2, Math.floor((grid.width + 20) / (compact ? 150 : 180)))
    property Item focusedTile: null          // the delegate in focus, for the details card

    function handleKey(e) {
        switch (e.key) {
        case Qt.Key_Left: playlists.move(-1); return true
        case Qt.Key_Right: playlists.move(1); return true
        case Qt.Key_Up: playlists.move(-picker.columns); return true
        case Qt.Key_Down: playlists.move(picker.columns); return true
        case Qt.Key_Home: playlists.currentIndex = 0; return true
        case Qt.Key_End: playlists.currentIndex = playlists.shown - 1; return true
        case Qt.Key_PageUp: playlists.move(-picker.columns * 3); return true
        case Qt.Key_PageDown: playlists.move(picker.columns * 3); return true
        case Qt.Key_Space: playlists.toggleCurrent(); return true
        case Qt.Key_A: playlists.pickAll(); return true
        case Qt.Key_Return: case Qt.Key_Enter: playlists.start(); return true
        case Qt.Key_Slash: search.focusInput(); return true
        case Qt.Key_F: playlists.cycleFilter(); return true
        case Qt.Key_O: playlists.cycleSort(); return true
        case Qt.Key_S: playlists.toggleShuffle(); return true
        case Qt.Key_V: playlists.toggleSkipVerdicts(); return true
        case Qt.Key_L: case Qt.Key_Escape:
            if (search.text !== "") { search.text = ""; return true }
            if (review.hasPlaylistReview) { playlists.closePicker(); return true }
            return e.key === Qt.Key_L
        }
        return false
    }

    // ---- top ----
    TopBar {
        id: topbar
        visible: !picker.compact
        x: 28; width: parent.width - 56
        onHelpRequested: picker.helpRequested()
    }
    RowLayout {
        id: compactBar
        visible: picker.compact
        x: 20; y: 18; width: parent.width - 40
        Wordmark { size: 15 }
        Item { Layout.fillWidth: true }
        PlaylistChip {}
        Text {
            text: "Playlists"
            font.family: "Archivo"; font.pointSize: 9.375
            color: theme.lightForeground
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: review.switchQueue() }
        }
    }

    GridLayout {
        id: head
        x: picker.compact ? 20 : 28
        y: picker.compact ? 58 : 78
        width: parent.width - 2 * x
        columns: picker.compact ? 1 : 2
        columnSpacing: 24
        rowSpacing: 12
        Column {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignBottom
            spacing: 6
            Text {
                width: parent.width; elide: Text.ElideRight
                text: "Choose playlists to review"
                font.family: "Archivo"; font.pixelSize: picker.compact ? 22 : 30; font.weight: Font.Bold
                font.variableAxes: { "wdth": 112 }
                font.letterSpacing: -0.02 * (picker.compact ? 22 : 30)
                color: theme.brightForeground
            }
            Text {
                width: parent.width; wrapMode: Text.WordWrap
                text: playlists.readingText !== "" ? playlists.readingText + "."
                      : playlists.total + " playlists from your Spotify account. Discoveries and To Rip have their own place, so they’re left out."
                font.family: "Archivo"; font.pointSize: 10.125
                color: theme.lightForeground
            }
        }
        Flow {
            Layout.alignment: picker.compact ? Qt.AlignLeft : (Qt.AlignRight | Qt.AlignBottom)
            Layout.fillWidth: picker.compact
            spacing: 12
            Segmented {
                value: playlists.filter
                options: [{ value: "all", label: "All", count: playlists.stageCounts.all },
                          { value: "fresh", label: "Not started", count: playlists.stageCounts.fresh },
                          { value: "progress", label: "In progress", count: playlists.stageCounts.progress },
                          { value: "done", label: "Done", count: playlists.stageCounts.done }]
                onPicked: (v) => playlists.filter = v
            }
            Item {
                width: sortRow.implicitWidth; height: 36
                Row {
                    id: sortRow
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        textFormat: Text.StyledText
                        text: "Sort: <font color=\"" + theme.foreground + "\">" + playlists.sortLabel + "</font>"
                        font.family: "Archivo"; font.pixelSize: 13
                        color: theme.lightForeground
                    }
                    Kbd { text: "O"; anchors.verticalCenter: parent.verticalCenter }
                }
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: playlists.cycleSort() }
                Accessible.role: Accessible.Button
                Accessible.name: "Sort: " + playlists.sortLabel
            }
            SearchField {
                id: search
                width: picker.compact ? head.width : 260
                onTextChanged: playlists.nameFilter = text
                onDone: picker.keysBack()
            }
        }
    }

    // ---- the wall ----
    GridView {
        id: grid
        x: picker.compact ? 10 : 18
        y: head.y + head.height + 22
        width: parent.width - 2 * x
        height: dock.y - y
        clip: true
        cellWidth: width / picker.columns
        cellHeight: cellWidth - 20 + 108
        model: playlists.model
        currentIndex: playlists.currentIndex
        boundsBehavior: Flickable.StopAtBounds
        topMargin: 8
        bottomMargin: 56
        cacheBuffer: 800
        onCurrentIndexChanged: positionViewAtIndex(currentIndex, GridView.Contain)
        delegate: PlaylistTile {
            id: cell
            width: grid.cellWidth; height: grid.cellHeight
            focused: index === playlists.currentIndex
            onFocusedChanged: if (focused) picker.focusedTile = cell
            Component.onCompleted: if (focused) picker.focusedTile = cell
            Component.onDestruction: if (picker.focusedTile === cell) picker.focusedTile = null
            onClicked: { playlists.currentIndex = index; playlists.toggle(index) }
            onHovered: playlists.currentIndex = index
        }
    }
    // the focused playlist's details, over the tiles below it
    PlaylistTip {
        id: tip
        readonly property Item item: picker.focusedTile
        // (contentY, x, y and the cell size only make the position follow a scroll or a new layout)
        readonly property point at: item ? (grid.contentY, item.x, item.y, grid.cellWidth, item.mapToItem(picker, 0, 0))
                                         : Qt.point(0, 0)
        readonly property real below: at.y + grid.cellWidth - 20 + 34       // over the tile's own meta line
        visible: item !== null && playlists.shown > 0 && !picker.compact && at.y + 40 > grid.y && at.y < dock.y
        info: playlists.current
        x: Math.max(18, Math.min(picker.width - width - 18, at.x + grid.cellWidth / 2 - width / 2))
        // no room below the tile (its last row): above it instead
        y: below + height < dock.y - 8 ? below : Math.max(grid.y, at.y - height - 8)
        z: 4
    }
    Rectangle {                       // the wall fades out under the dock
        x: 0; width: parent.width; height: 56
        y: dock.y - height
        z: 3
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.alpha(theme.background, 0) }
            GradientStop { position: 1.0; color: theme.background }
        }
    }
    Notice {
        anchors.centerIn: grid
        visible: playlists.total === 0
        title: "haystack hasn’t read your playlists yet"
        body: "It lists them a few minutes after it starts, and checks them every hour after that."
    }
    Text {
        anchors.horizontalCenter: grid.horizontalCenter
        y: grid.y + 80
        visible: playlists.total > 0 && playlists.shown === 0
        text: search.text !== "" ? "No playlist’s name has “" + search.text + "” in it." : "No playlists here yet."
        font.family: "Archivo"; font.pixelSize: 15
        color: theme.lightForeground
    }

    // ---- what the pick adds up to, and the one thing to do ----
    Rectangle {
        id: notice
        visible: playlists.notice !== ""
        z: 5
        anchors.horizontalCenter: parent.horizontalCenter
        y: dock.y - height - 16
        width: Math.min(parent.width - 40, noticeText.implicitWidth + 32)
        height: noticeText.implicitHeight + 20
        radius: 10
        color: theme.lighterBackground
        border.width: 1
        border.color: Qt.alpha(theme.foreground, 0.08)
        Text {
            id: noticeText
            anchors.centerIn: parent
            width: Math.min(implicitWidth, picker.width - 72)
            wrapMode: Text.WordWrap
            text: playlists.notice
            font.family: "Archivo"; font.pixelSize: 13
            color: theme.foreground
        }
    }

    Rectangle {
        id: dock
        z: 4
        width: parent.width
        height: picker.compact ? 176 : 96
        y: parent.height - height
        color: theme.darkBackground
        Rectangle { width: parent.width; height: 1; color: Qt.alpha(theme.foreground, 0.05) }
        readonly property var sel: playlists.selection

        Row {
            id: stack
            x: picker.compact ? 30 : 38
            y: picker.compact ? 16 : (dock.height - 48) / 2
            spacing: -10
            visible: dock.sel.count > 0
            Repeater {
                model: dock.sel.covers
                delegate: Rectangle {
                    required property var modelData
                    width: 54; height: 54; radius: 11
                    y: -3
                    color: theme.darkBackground
                    Cover {
                        x: 3; y: 3; width: 48; height: 48; radius: 8; shadow: false; placeholder: "playlist"
                        source: modelData || ""
                    }
                }
            }
        }
        Column {
            id: what
            // as wide as its text, up to the switches
            readonly property real room: picker.compact ? dock.width - x - 20 : opts.x - x - 24
            anchors.left: stack.visible ? stack.right : parent.left
            anchors.leftMargin: stack.visible ? 24 : (picker.compact ? 20 : 28)
            y: picker.compact ? 18 : (dock.height - height) / 2
            spacing: 2
            Text {
                width: Math.min(implicitWidth, what.room); elide: Text.ElideRight
                text: dock.sel.line1 || ""
                font.family: "Archivo"; font.pixelSize: 15; font.weight: Font.DemiBold; font.features: { "tnum": 1 }
                color: theme.brightForeground
            }
            Text {
                width: Math.min(implicitWidth, what.room); elide: Text.ElideRight
                text: dock.sel.line2 || ""
                font.family: "Archivo"; font.pixelSize: 13; font.features: { "tnum": 1 }
                color: theme.lightForeground
            }
        }
        Row {
            id: hints
            visible: !picker.compact && what.x + what.width + 40 + implicitWidth < opts.x
            anchors.left: what.right; anchors.leftMargin: 40
            anchors.verticalCenter: start.verticalCenter
            spacing: 18
            Row { spacing: 6; Kbd { text: "A" } Text { text: "all"; font.family: "Archivo"; font.pointSize: 9.375; color: theme.lightForeground; anchors.verticalCenter: parent.verticalCenter } }
            Row { spacing: 6; Kbd { text: "Space" } Text { text: "add or remove"; font.family: "Archivo"; font.pointSize: 9.375; color: theme.lightForeground; anchors.verticalCenter: parent.verticalCenter } }
        }
        Flow {
            id: opts
            // wide: left of the button; compact: a row of their own under the pick
            x: picker.compact ? 20 : start.x - width - 28
            y: picker.compact ? 70 : start.y + (start.height - height) / 2
            width: picker.compact ? dock.width - 40 : skip.implicitWidth + spacing + shuffle.implicitWidth
            spacing: 20
            Toggle { id: skip; label: "Skip songs with a verdict"; key: "V"; on: playlists.skipVerdicts; onToggled: playlists.toggleSkipVerdicts() }
            Toggle { id: shuffle; label: "Shuffle"; key: "S"; on: playlists.shuffle; onToggled: playlists.toggleShuffle() }
        }
        Rectangle {
            id: start
            readonly property bool ready: dock.sel.canStart === true && !playlists.starting
            x: parent.width - width - (picker.compact ? 20 : 28)
            y: picker.compact ? dock.height - height - 18 : (dock.height - height) / 2
            width: startRow.implicitWidth + 32; height: 44; radius: 12
            color: ready ? theme.accent : Qt.alpha(theme.foreground, 0.08)
            Row {
                id: startRow
                anchors.centerIn: parent
                spacing: 10
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: playlists.starting ? "Fetching the songs…" : "Start review"
                    font.family: "Archivo"; font.pixelSize: 14; font.weight: Font.DemiBold
                    color: start.ready ? theme.background : theme.lightForeground
                }
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: enterText.implicitWidth + 10; height: 20; radius: 5
                    color: start.ready ? Qt.alpha(theme.darkerBackground, 0.14) : Qt.alpha(theme.foreground, 0.05)
                    Text {
                        id: enterText
                        anchors.centerIn: parent
                        text: "Enter"
                        font.family: "Archivo"; font.pixelSize: 11; font.weight: Font.DemiBold
                        color: start.ready ? theme.background : theme.lightForeground
                    }
                }
            }
            MouseArea { anchors.fill: parent; cursorShape: start.ready ? Qt.PointingHandCursor : Qt.ArrowCursor; onClicked: playlists.start() }
            Accessible.role: Accessible.Button
            Accessible.name: "Start review"
        }
    }
}
