import QtQuick

Rectangle {
    id: b
    property string icon
    property real iconSize: 18
    property color iconColor: theme.lightForeground
    property bool filled: false        // the play button: a foreground disc with a background icon
    property string label
    signal clicked()
    implicitWidth: 36
    implicitHeight: 36
    radius: filled ? width / 2 : 8
    color: filled ? theme.foreground : Qt.alpha(theme.foreground, area.containsMouse ? 0.07 : (flat ? 0 : 0.03))
    property bool flat: false
    Icon { anchors.centerIn: parent; name: b.icon; size: b.iconSize; color: b.filled ? theme.background : b.iconColor }
    MouseArea { id: area; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: b.clicked() }
    Accessible.role: Accessible.Button
    Accessible.name: b.label
}
