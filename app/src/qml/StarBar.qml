import QtQuick

// Five stars: the current Rating lit in accent, unlit ones in light_foreground; hovering previews a Rating.
Rectangle {
    id: bar
    property int stars: 0               // the track's current Rating (0: none)
    property bool compact: false
    property real starWidth: compact ? 38 : 52
    property real starHeight: compact ? 36 : 44
    property bool showKeys: !compact
    property int hovered: 0
    signal rate(int stars)

    implicitWidth: row.implicitWidth + 8
    implicitHeight: starHeight + 8
    radius: 14
    color: Qt.alpha(theme.foreground, 0.05)
    border.width: 1
    border.color: Qt.alpha(theme.foreground, 0.06)

    Row {
        id: row
        anchors.centerIn: parent
        spacing: bar.compact ? 0 : 2
        Repeater {
            model: 5
            delegate: Item {
                required property int index
                readonly property int value: index + 1
                readonly property bool lit: value <= bar.stars
                width: bar.compact ? (bar.width - 8) / 5 : bar.starWidth
                height: bar.starHeight
                Rectangle {
                    anchors.fill: parent
                    radius: 10
                    color: Qt.alpha(theme.accent, 0.10)
                    visible: bar.hovered >= parent.value && !parent.lit
                }
                Icon {
                    anchors.centerIn: parent
                    name: parent.lit ? "star" : "starOutline"
                    color: parent.lit ? theme.accent : theme.lightForeground
                    size: bar.compact ? 20 : 24
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onEntered: bar.hovered = parent.value
                    onExited: if (bar.hovered === parent.value) bar.hovered = 0
                    onClicked: (mouse) => bar.rate(mouse.modifiers & Qt.ShiftModifier ? -parent.value : parent.value)
                }
                Accessible.role: Accessible.RadioButton
                Accessible.name: value + (value > 1 ? " stars" : " star")
                Accessible.checked: lit
            }
        }
    }
}
