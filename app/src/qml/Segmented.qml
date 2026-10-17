import QtQuick

// One of a few: the picker's filter. Each option shows how many it holds.
Rectangle {
    id: seg
    property var options: []          // [{ value, label, count }]
    property string value
    signal picked(string value)
    implicitWidth: row.implicitWidth + 6
    implicitHeight: 36
    radius: 10
    color: Qt.alpha(theme.darkerBackground, 0.55)
    Row {
        id: row
        x: 3; y: 3
        spacing: 2
        Repeater {
            model: seg.options
            delegate: Rectangle {
                id: opt
                required property var modelData
                readonly property bool on: modelData.value === seg.value
                width: label.implicitWidth + 24
                height: 30
                radius: 7
                color: on ? theme.lighterBackground : "transparent"
                border.width: on ? 1 : 0
                border.color: Qt.alpha(theme.foreground, 0.04)
                Row {
                    id: label
                    anchors.centerIn: parent
                    spacing: 6
                    Text {
                        text: opt.modelData.label
                        font.family: "Archivo"; font.pixelSize: 13; font.weight: Font.Medium
                        color: opt.on ? theme.brightForeground : theme.lightForeground
                    }
                    Text {
                        text: opt.modelData.count
                        font.family: "Archivo"; font.pixelSize: 13; font.features: { "tnum": 1 }
                        color: opt.on ? theme.foreground : theme.lightForeground
                    }
                }
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: seg.picked(opt.modelData.value) }
                Accessible.role: Accessible.RadioButton
                Accessible.name: opt.modelData.label + " " + opt.modelData.count
                Accessible.checked: opt.on
            }
        }
    }
}
