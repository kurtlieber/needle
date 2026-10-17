import QtQuick

Row {
    property real size: 19
    spacing: size * 0.53
    Icon { name: "needle"; color: theme.accent; size: parent.size * 0.95; anchors.verticalCenter: parent.verticalCenter }
    Text {
        text: "needle"
        anchors.verticalCenter: parent.verticalCenter
        font.family: "Archivo"; font.pixelSize: parent.size; font.weight: Font.Bold
        font.variableAxes: { "wdth": 125 }
        font.letterSpacing: -0.01 * parent.size
        color: theme.brightForeground
    }
}
