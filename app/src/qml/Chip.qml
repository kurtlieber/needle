import QtQuick

Rectangle {
    property alias text: label.text
    implicitWidth: label.implicitWidth + 20
    implicitHeight: 26
    radius: 13
    color: Qt.alpha(theme.foreground, 0.06)
    Text {
        id: label
        anchors.centerIn: parent
        font.family: "Archivo"
        font.pointSize: 9.375
        font.weight: Font.Medium
        color: theme.foreground
    }
}
