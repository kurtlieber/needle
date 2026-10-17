import QtQuick

// An on/off switch with its label and key; when on, the knob sits right, in the accent colour.
Item {
    id: t
    property bool on
    property string label
    property string key
    signal toggled()
    implicitWidth: row.implicitWidth
    implicitHeight: 28
    Row {
        id: row
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 34; height: 20; radius: 10
            color: t.on ? Qt.alpha(theme.accent, 0.35) : Qt.alpha(theme.foreground, 0.12)
            Rectangle {
                x: t.on ? 17 : 3; y: 3
                width: 14; height: 14; radius: 7
                color: t.on ? theme.accent : theme.lightForeground
                Behavior on x { enabled: !prefs.reducedMotion; NumberAnimation { duration: 120 } }
            }
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: t.label
            font.family: "Archivo"; font.pixelSize: 13
            color: theme.foreground
        }
        Kbd { visible: t.key !== ""; text: t.key; anchors.verticalCenter: parent.verticalCenter }
    }
    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: t.toggled() }
    Accessible.role: Accessible.CheckBox
    Accessible.name: t.label
    Accessible.checkable: true
    Accessible.checked: t.on
}
