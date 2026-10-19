import QtQuick

Rectangle {
    property alias text: label.text
    property bool outline: false      // a figure about the track (Confidence) rather than one of its styles
    implicitWidth: label.implicitWidth + 20
    implicitHeight: 26
    radius: 13
    color: outline ? "transparent" : Qt.alpha(theme.foreground, 0.06)
    border.width: outline ? 1 : 0
    border.color: Qt.alpha(theme.foreground, 0.3)
    Text {
        id: label
        anchors.centerIn: parent
        font.family: "Archivo"
        font.pointSize: 9.375
        font.weight: Font.Medium
        color: theme.foreground
    }
}
