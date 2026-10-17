import QtQuick

// Filter by name. / puts the cursor here; Enter or Down goes back to the covers, Esc clears it.
Rectangle {
    id: f
    property alias text: input.text
    property string placeholder: "Filter by name"
    signal done()
    implicitWidth: 260
    implicitHeight: 36
    radius: 10
    color: Qt.alpha(theme.foreground, 0.05)
    border.width: input.activeFocus ? 2 : 1
    border.color: input.activeFocus ? theme.brightForeground : Qt.alpha(theme.foreground, 0.07)
    function focusInput() { input.forceActiveFocus() }
    Icon { id: glass; x: 12; anchors.verticalCenter: parent.verticalCenter; name: "search"; size: 16; color: theme.lightForeground }
    TextInput {
        id: input
        anchors { left: glass.right; leftMargin: 10; right: key.left; rightMargin: 8; verticalCenter: parent.verticalCenter }
        font.family: "Archivo"; font.pixelSize: 13
        color: theme.brightForeground
        selectionColor: theme.selection
        clip: true
        Keys.onEscapePressed: { if (text !== "") text = ""; else f.done() }
        Keys.onReturnPressed: f.done()
        Keys.onEnterPressed: f.done()
        Keys.onDownPressed: f.done()
        Keys.onTabPressed: f.done()
        Text {
            visible: input.text === ""
            anchors.verticalCenter: parent.verticalCenter
            text: f.placeholder
            font: input.font
            color: theme.lightForeground
        }
    }
    Kbd { id: key; text: "/"; anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter }
    MouseArea { anchors.fill: parent; cursorShape: Qt.IBeamCursor; onClicked: input.forceActiveFocus() }
    Accessible.role: Accessible.EditableText
    Accessible.name: f.placeholder
}
