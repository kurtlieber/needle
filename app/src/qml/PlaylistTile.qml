import QtQuick

// One playlist on the cover wall: its cover, name and progress. Picked: an accent outline and a check (never
// the colour alone). The keyboard focus is a separate, light ring; the focused tile also shows its details card.
Item {
    id: tile
    required property var model
    required property int index
    property real gap: 20
    property bool focused: false
    property bool showRing: true
    signal clicked()
    signal hovered()

    readonly property bool picked: model.selected
    readonly property bool done: model.readable && model.left === 0
    z: focused ? 2 : 0

    Item {
        id: art
        x: tile.gap / 2
        width: tile.width - tile.gap; height: width
        Rectangle {               // picked: the accent outline, with a soft glow
            visible: tile.picked
            anchors.fill: parent; anchors.margins: -7
            radius: 19
            color: "transparent"
            border.width: 4
            border.color: Qt.alpha(theme.accent, 0.22)
        }
        Rectangle {
            visible: tile.picked
            anchors.fill: parent; anchors.margins: -3
            radius: 15
            color: "transparent"
            border.width: 3
            border.color: theme.accent
        }
        Cover {
            anchors.fill: parent
            radius: 12
            shadow: true
            placeholder: "playlist"
            source: tile.model.coverSource || ""
            opacity: tile.model.readable ? 1 : 0.55
        }
        Rectangle {               // reviewed: the cover steps back
            visible: tile.done
            anchors.fill: parent
            radius: 12
            color: Qt.alpha(theme.darkerBackground, 0.55)
        }
        Rectangle {               // the keyboard focus
            visible: tile.focused && tile.showRing
            anchors.fill: parent; anchors.margins: tile.picked ? -11 : -6
            radius: tile.picked ? 23 : 18
            color: "transparent"
            border.width: 2
            border.color: theme.brightForeground
        }
        Rectangle {               // the check: filled when picked, an empty ring on the focused tile
            visible: tile.picked || (tile.focused && tile.model.readable)
            anchors { top: parent.top; right: parent.right; margins: 10 }
            width: 26; height: 26; radius: 13
            color: tile.picked ? theme.accent : Qt.alpha(theme.darkerBackground, 0.7)
            border.width: tile.picked ? 3 : 1.5
            border.color: tile.picked ? theme.background : Qt.alpha(theme.brightForeground, 0.55)
            Icon { visible: tile.picked; anchors.centerIn: parent; name: "check"; size: 15; color: theme.background }
        }
        Rectangle {               // someone else's playlist
            visible: !tile.model.mine
            anchors { left: parent.left; top: parent.top; margins: 10 }
            width: fl.implicitWidth + 14; height: 22; radius: 7
            color: Qt.alpha(theme.darkBackground, 0.82)
            Text {
                id: fl
                anchors.centerIn: parent
                text: "Followed"
                font.family: "Archivo"; font.pointSize: 8.625; font.weight: Font.DemiBold
                color: theme.foreground
            }
        }
        Rectangle {               // All reviewed / Reading…
            visible: tile.done || tile.model.reading
            anchors { left: parent.left; bottom: parent.bottom; margins: 10 }
            width: badge.implicitWidth + 18; height: 26; radius: 13
            color: Qt.alpha(theme.darkBackground, 0.82)
            Row {
                id: badge
                anchors.centerIn: parent
                spacing: 6
                Icon { visible: tile.done; name: "check"; size: 13; color: theme.accent; anchors.verticalCenter: parent.verticalCenter }
                Text {
                    text: tile.done ? "All reviewed" : "Reading…"
                    font.family: "Archivo"; font.pixelSize: 12; font.weight: Font.DemiBold
                    color: theme.brightForeground
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }

    Text {
        id: name
        x: art.x; y: art.y + art.height + 12
        width: art.width
        text: tile.model.name || ""
        wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight
        lineHeight: 1.12
        font.family: "Archivo"; font.pixelSize: 14; font.weight: Font.DemiBold
        color: theme.brightForeground
    }
    Text {
        id: meta
        x: art.x; y: name.y + name.height + 3
        width: art.width
        elide: Text.ElideRight
        text: tile.model.meta || ""
        font.family: "Archivo"; font.pointSize: 9.375; font.features: { "tnum": 1 }
        color: theme.lightForeground
    }
    VerdictBar {
        visible: tile.model.readable && !tile.done
        x: art.x; y: meta.y + meta.height + 9
        width: art.width; height: 4
        stars: tile.model.stars; passed: tile.model.passed; disliked: tile.model.disliked; remaining: tile.model.left
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onEntered: tile.hovered()
        onClicked: tile.clicked()
    }
    Accessible.role: Accessible.CheckBox
    Accessible.name: (tile.model.name || "") + ", " + (tile.model.meta || "")
    Accessible.checkable: true
    Accessible.checked: tile.picked
}
