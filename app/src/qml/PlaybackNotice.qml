import QtQuick

// Why nothing plays, and the one-time Spotify sign-in. Takes the spectrum's place (nothing to show there
// until needle can play); verdicts keep working meanwhile.
Rectangle {
    id: n
    readonly property bool needed: engine.auth !== "ok" && engine.auth !== "starting"
    readonly property bool canSignIn: engine.auth === "needed" || engine.auth === "error"
    visible: needed
    implicitHeight: col.implicitHeight + 32
    radius: 12
    color: Qt.alpha(theme.foreground, 0.04)
    border.width: 1
    border.color: Qt.alpha(theme.foreground, 0.06)
    Column {
        id: col
        x: 16; y: 16; width: parent.width - 32
        spacing: 8
        Text {
            width: parent.width; wrapMode: Text.WordWrap
            text: engine.auth === "pending" ? "Finish signing in to Spotify in your browser"
                  : engine.auth === "missing" ? "needle can't play" : "Sign in to Spotify to play"
            font.family: "Archivo"; font.pixelSize: 15; font.weight: Font.Bold
            color: theme.brightForeground
        }
        Text {
            width: parent.width; wrapMode: Text.WordWrap
            text: engine.auth === "missing" ? engine.authMessage
                  : engine.auth === "pending" ? "needle remembers the sign-in, so this is a one-time step."
                  : "needle plays through your Spotify Premium account. Verdicts work without it."
                    + (engine.authMessage ? " " + engine.authMessage : "")
            font.family: "Archivo"; font.pointSize: 9.75
            lineHeight: 1.15
            color: theme.lightForeground
        }
        Row {
            visible: n.canSignIn
            spacing: 10
            topPadding: 4
            Rectangle {
                width: label.implicitWidth + 28; height: 34; radius: 9
                color: theme.accent
                Text {
                    id: label
                    anchors.centerIn: parent
                    text: "Sign in to Spotify"
                    font.family: "Archivo"; font.pixelSize: 13; font.weight: Font.DemiBold
                    color: theme.background
                }
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: engine.login() }
                Accessible.role: Accessible.Button
                Accessible.name: label.text
            }
            Kbd { text: "Enter"; anchors.verticalCenter: parent.verticalCenter }
        }
    }
}
