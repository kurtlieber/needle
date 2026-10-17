import QtQuick

// The focused playlist's details card: its progress in numbers, who it is mostly, and what Space and Enter do.
Rectangle {
    id: tip
    property var info: ({})
    width: 300
    height: col.implicitHeight + 24
    radius: 12
    color: theme.lighterBackground
    border.width: 1
    border.color: Qt.alpha(theme.foreground, 0.06)

    component Line: Item {
        property alias leftText: l.text
        property alias rightText: r.text
        property bool strong: false
        width: parent.width
        height: Math.max(l.implicitHeight, r.implicitHeight)
        Text {
            id: l
            width: parent.width - r.implicitWidth - 10; elide: Text.ElideRight
            font.family: "Archivo"; font.pointSize: strong ? 10.125 : 9.375; font.weight: strong ? Font.DemiBold : Font.Normal
            font.features: { "tnum": 1 }
            color: strong ? theme.foreground : theme.lightForeground
        }
        Text {
            id: r
            anchors.right: parent.right
            font.family: "Archivo"; font.pointSize: 9.375; font.features: { "tnum": 1 }
            color: theme.lightForeground
        }
    }

    Column {
        id: col
        x: 14; y: 12; width: parent.width - 28
        spacing: 6
        Line {
            strong: true
            leftText: tip.info.name || ""
            rightText: tip.info.readable ? Number(tip.info.songs).toLocaleString(Qt.locale("en_US"), "f", 0) + (tip.info.songs === 1 ? " song" : " songs") : ""
        }
        VerdictBar {
            visible: tip.info.readable === true
            width: parent.width; height: 8
            stars: tip.info.stars || [0, 0, 0, 0, 0]; passed: tip.info.passed || 0; disliked: tip.info.disliked || 0
            remaining: tip.info.left || 0
        }
        Line {
            visible: tip.info.readable === true
            leftText: !tip.info.reviewed ? "Not started"
                  : tip.info.reviewed + " reviewed" + (tip.info.rated && tip.info.rated < tip.info.reviewed ? ", " + tip.info.rated + " rated" : "")
            rightText: tip.info.left ? Number(tip.info.left).toLocaleString(Qt.locale("en_US"), "f", 0) + " to review" : "All reviewed"
        }
        Text {
            visible: tip.info.rated > 0
            width: parent.width; elide: Text.ElideRight
            text: "Average rating " + Number(tip.info.average).toFixed(1) + "★"
            font.family: "Archivo"; font.pointSize: 9.375
            color: theme.lightForeground
        }
        Text {
            visible: !tip.info.readable
            width: parent.width; wrapMode: Text.WordWrap
            text: tip.info.meta || ""
            font.family: "Archivo"; font.pointSize: 9.375
            color: theme.lightForeground
        }
        Text {
            visible: (tip.info.artists || []).length > 0
            width: parent.width; elide: Text.ElideRight
            text: "Mostly " + (tip.info.artists || []).join(", ")
            font.family: "Archivo"; font.pointSize: 9.375
            color: theme.lightForeground
        }
        Text {
            visible: tip.info.mine === false
            width: parent.width; elide: Text.ElideRight
            text: "Followed" + (tip.info.owner ? ", made by " + tip.info.owner : "")
            font.family: "Archivo"; font.pointSize: 9.375
            color: theme.lightForeground
        }
        Rectangle { width: parent.width; height: 1; color: Qt.alpha(theme.foreground, 0.06); visible: keyHints.visible }
        Grid {
            id: keyHints
            visible: tip.info.readable === true
            columns: 2
            columnSpacing: 10
            rowSpacing: 8
            topPadding: 4
            verticalItemAlignment: Grid.AlignVCenter
            Kbd { text: "Space" }
            Text { text: tip.info.spaceText || ""; font.family: "Archivo"; font.pointSize: 9.375; color: theme.lightForeground }
            Kbd { text: "Enter" }
            Text { text: tip.info.enterText || ""; font.family: "Archivo"; font.pointSize: 9.375; color: theme.lightForeground }
        }
    }
}
