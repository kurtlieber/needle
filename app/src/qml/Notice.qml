import QtQuick

// A centred message card: what's wrong or what's next, and the one thing to do about it.
Rectangle {
    id: n
    property string title
    property string body
    property string action
    property string actionKey
    signal activated()
    width: Math.min(parent ? parent.width - 40 : 520, 520)
    height: col.implicitHeight + 48
    radius: 16
    color: theme.lighterBackground
    border.width: 1; border.color: Qt.alpha(theme.foreground, 0.06)
    Column {
        id: col
        x: 24; y: 24; width: parent.width - 48
        spacing: 10
        Text {
            width: parent.width; wrapMode: Text.WordWrap
            text: n.title
            font.family: "Archivo"; font.pixelSize: 20; font.weight: Font.Bold
            font.variableAxes: { "wdth": 112 }
            color: theme.brightForeground
        }
        Text {
            width: parent.width; wrapMode: Text.WordWrap
            visible: n.body !== ""
            text: n.body
            textFormat: Text.StyledText
            font.family: "Archivo"; font.pixelSize: 14
            lineHeight: 1.15
            color: theme.foreground
        }
        Rectangle {
            visible: n.action !== ""
            width: btn.implicitWidth + 32; height: 40; radius: 10
            color: theme.accent
            Row {
                id: btn
                anchors.centerIn: parent
                spacing: 10
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: n.action
                    font.family: "Archivo"; font.pixelSize: 14; font.weight: Font.DemiBold
                    color: theme.background
                }
                Kbd { visible: n.actionKey !== ""; text: n.actionKey; anchors.verticalCenter: parent.verticalCenter }
            }
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: n.activated() }
        }
    }
}
