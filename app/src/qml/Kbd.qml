import QtQuick

// A keycap: legends in foreground (contrast-checked on the mockups).
Rectangle {
    property alias text: label.text
    implicitWidth: Math.max(20, label.implicitWidth + 10)
    implicitHeight: 20
    radius: 5
    color: Qt.alpha(theme.foreground, 0.05)
    border.width: 1
    border.color: Qt.alpha(theme.foreground, 0.07)
    Rectangle {       // the key's lower edge
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 1; rightMargin: 1 }
        height: 1
        color: Qt.alpha(theme.darkerBackground, 0.6)
    }
    Text {
        id: label
        anchors.centerIn: parent
        font.family: "Archivo"
        font.pixelSize: 11
        font.weight: Font.DemiBold
        font.features: { "tnum": 1 }
        color: theme.foreground
    }
}
