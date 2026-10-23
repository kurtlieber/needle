import QtQuick

// A Possible cover (haystack's ADR 0022, feature card.original): the recording haystack thinks this one stands in for,
// and, in Rate your likes and a Playlist review, a button that swaps it for its Original (S). Hidden on a card
// without an Original.
Rectangle {
    id: b
    property var card: ({})
    property bool compact: false
    readonly property string line: insight.original(card, theme.brightForeground, review.features)
    signal swap()

    visible: line !== ""
    implicitHeight: Math.max(label.implicitHeight, button.visible ? button.height : 0) + 20
    radius: 12
    color: Qt.alpha(theme.accent, 0.08)
    border.width: 1
    border.color: Qt.alpha(theme.accent, 0.35)

    Text {
        id: label
        anchors {
            left: parent.left; right: button.visible ? button.left : parent.right; verticalCenter: parent.verticalCenter
            leftMargin: 14; rightMargin: 12
        }
        textFormat: Text.StyledText
        wrapMode: Text.WordWrap
        text: b.line
        lineHeight: 1.18
        font.family: "Archivo"; font.pointSize: 10.125
        color: theme.foreground
    }

    Rectangle {
        id: button
        visible: review.canSwap
        anchors { right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
        width: row.implicitWidth + 20; height: 34
        radius: 9
        color: Qt.alpha(theme.accent, area.containsMouse ? 0.24 : 0.15)
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 8
            Text {
                text: b.compact ? "Swap" : "Swap for the original"
                anchors.verticalCenter: parent.verticalCenter
                font.family: "Archivo"; font.pixelSize: 13; font.weight: Font.DemiBold
                color: theme.brightForeground
            }
            Kbd { text: "S"; anchors.verticalCenter: parent.verticalCenter }
        }
        MouseArea { id: area; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: b.swap() }
        Accessible.role: Accessible.Button
        Accessible.name: "Swap for the original"
    }
}
