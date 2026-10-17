import QtQuick

// A wide Trait row: name, calibrated scale with pole labels, this track's Reading and the listener's Baseline.
Item {
    id: row
    property var trait
    property var readings: ({})
    property var baselines: ({})
    readonly property var song: readings ? readings[trait.key] : undefined
    readonly property var base: baselines && baselines[trait.key] ? baselines[trait.key].mean : undefined
    implicitHeight: 80

    Text {
        x: 0; width: 168; anchors.verticalCenter: parent.verticalCenter
        text: trait.name
        font.family: "Archivo"; font.pixelSize: 15; font.weight: Font.DemiBold
        color: theme.foreground
    }
    Item {
        id: track
        x: 168 + 28; width: row.width - 2 * (168 + 28); height: 56
        anchors.verticalCenter: parent.verticalCenter
        ScaleTrack { anchors.fill: parent; trait: row.trait; song: row.song; base: row.base }
        Text {
            x: 0; y: 42
            text: trait.lowPole
            font.family: "Archivo"; font.pointSize: 8.625
            color: theme.lightForeground
        }
        Text {
            anchors.right: parent.right; y: 42
            text: trait.highPole
            font.family: "Archivo"; font.pointSize: 8.625
            color: theme.lightForeground
        }
    }
    // this track's Reading, then "Yours 0.41" in a column at least 74 px wide, baselines aligned (as the mockup)
    Item {
        anchors.right: parent.right
        width: 168; height: parent.height
        readonly property real yoursWidth: Math.max(74, yoursLabel.implicitWidth + 4 + yoursValue.implicitWidth)
        Text {
            id: yoursValue
            anchors.right: parent.right; anchors.baseline: value.baseline
            text: insight.format(trait.key, row.base)
            font.family: "Archivo"; font.pixelSize: 13; font.weight: Font.DemiBold
            font.features: { "tnum": 1 }
            color: theme.foreground
        }
        Text {
            id: yoursLabel
            anchors.right: yoursValue.left; anchors.rightMargin: 4; anchors.baseline: value.baseline
            text: "Yours"
            font.family: "Archivo"; font.pixelSize: 13
            color: theme.lightForeground
        }
        Text {
            id: unit
            visible: trait.unit !== "" && row.song !== undefined
            anchors.right: parent.right; anchors.rightMargin: parent.yoursWidth + 14; anchors.baseline: value.baseline
            text: trait.unit
            font.family: "Archivo"; font.pixelSize: 12; font.weight: Font.Medium
            color: theme.lightForeground
        }
        Text {
            id: value
            anchors.right: unit.visible ? unit.left : parent.right
            anchors.rightMargin: unit.visible ? 3 : parent.yoursWidth + 14
            anchors.verticalCenter: parent.verticalCenter
            text: insight.format(trait.key, row.song)
            font.family: "Archivo"; font.pixelSize: 24; font.weight: Font.DemiBold
            font.variableAxes: { "wdth": 105 }
            font.features: { "tnum": 1 }
            color: theme.brightForeground
        }
    }
}
