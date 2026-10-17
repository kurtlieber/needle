import QtQuick

// Dislike and Pass: icon, label (wide layout), keycap. Mouse works too.
Rectangle {
    id: b
    property string icon
    property color iconColor: theme.foreground
    property string label
    property string key
    property bool compact: false
    signal clicked()

    implicitHeight: compact ? 44 : 48
    implicitWidth: row.implicitWidth + (compact ? 22 : 28)
    radius: 12
    color: Qt.alpha(theme.foreground, area.containsMouse ? 0.09 : 0.05)
    border.width: 1
    border.color: Qt.alpha(theme.foreground, 0.06)

    Row {
        id: row
        anchors.centerIn: parent
        anchors.horizontalCenterOffset: compact ? 0 : 2
        spacing: compact ? 8 : 10
        Icon { name: b.icon; color: b.iconColor; size: 18; anchors.verticalCenter: parent.verticalCenter }
        Text {
            visible: !b.compact
            text: b.label
            anchors.verticalCenter: parent.verticalCenter
            font.family: "Archivo"; font.pixelSize: 14; font.weight: Font.DemiBold
            color: theme.foreground
        }
        Kbd { text: b.key; anchors.verticalCenter: parent.verticalCenter }
    }
    MouseArea { id: area; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: b.clicked() }
    Accessible.role: Accessible.Button
    Accessible.name: b.label
}
