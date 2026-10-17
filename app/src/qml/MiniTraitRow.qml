import QtQuick

// A compact Trait row: name and values over a short scale.
Item {
    id: row
    property var trait
    property var readings: ({})
    property var baselines: ({})
    readonly property var song: readings ? readings[trait.key] : undefined
    readonly property var base: baselines && baselines[trait.key] ? baselines[trait.key].mean : undefined
    implicitHeight: 60

    Text {
        y: 10
        text: trait.name
        font.family: "Archivo"; font.pointSize: 9.375; font.weight: Font.DemiBold
        color: theme.foreground
    }
    Row {
        anchors.right: parent.right; y: 9
        spacing: 6
        Text {
            id: v
            text: insight.format(trait.key, row.song)
            font.family: "Archivo"; font.pixelSize: 14; font.weight: 650
            font.features: { "tnum": 1 }
            color: theme.brightForeground
        }
        Text {
            anchors.baseline: v.baseline
            text: "Yours " + insight.format(trait.key, row.base)
            font.family: "Archivo"; font.pointSize: 9.375
            font.features: { "tnum": 1 }
            color: theme.lightForeground
        }
    }
    ScaleTrack {
        x: 0; y: 34; width: row.width; height: 20
        trait: row.trait; song: row.song; base: row.base
        baselineY: 16; majorTick: 7; minorTick: 3; ringRadius: 5
        needleTop: 6; needleBelow: 6; arrow: 5; glow: false
    }
}
