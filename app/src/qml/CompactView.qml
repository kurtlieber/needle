import QtQuick
import QtQuick.Layouts

// Concept A, compact (column A of compact.png): the same Review in one narrow column.
Item {
    id: v
    signal helpRequested()
    readonly property var card: review.current
    readonly property var readings: card.readings || ({})
    readonly property var baselines: review.baseline
    readonly property var summary: insight.summary(readings, baselines)

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 20; anchors.rightMargin: 20; anchors.topMargin: 18; anchors.bottomMargin: 18
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Wordmark { size: 15 }
            Item { Layout.fillWidth: true }
            PlaylistChip { visible: review.queue === "playlists" && playlists.hasSession }
            Text {
                text: review.queue === "likes" ? "Rate your likes, " + review.likesLeft + " left"
                      : review.queue === "playlists" ? "Playlists, " + review.playlistsPosition + " of " + review.playlistsTotal
                      : review.queue === "calibrate"
                      ? "Calibrate" + (review.calibrateTotal === 0 ? "" : review.queueDone ? ", done"
                                       : ", " + review.calibratePosition + " of " + review.calibrateTotal)
                      : review.queueDone ? "Discoveries, done"
                      : "Discoveries, " + review.discoveriesPosition + " of " + review.discoveriesTotal
                font.family: "Archivo"; font.pointSize: 9.375; font.features: { "tnum": 1 }
                color: theme.lightForeground
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: review.switchQueue() }
            }
        }

        RowLayout {
            Layout.topMargin: 16
            Layout.fillWidth: true
            spacing: 14
            Cover { Layout.preferredWidth: 76; Layout.preferredHeight: 76; radius: 10; shadow: false; source: v.card.coverSource || "" }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Text {
                    Layout.fillWidth: true
                    text: v.card.title || ""
                    elide: Text.ElideRight
                    font.family: "Archivo"; font.pixelSize: 24; font.weight: Font.Bold
                    font.variableAxes: { "wdth": 112 }
                    font.letterSpacing: -0.02 * 24
                    color: theme.brightForeground
                }
                Text {
                    Layout.fillWidth: true; Layout.topMargin: 4
                    text: (v.card.artists || []).join(", ")
                    elide: Text.ElideRight
                    font.family: "Archivo"; font.pointSize: 10.875; font.weight: Font.Medium
                    color: theme.foreground
                }
                Text {
                    text: insight.albumLine(v.card)
                    font.family: "Archivo"; font.pointSize: 9.375
                    color: theme.lightForeground
                }
            }
        }

        RowLayout {
            Layout.topMargin: 18
            spacing: 14
            Pulse { compact: true; bpm: v.readings.tempo || 0; playing: engine.playing; Layout.leftMargin: 6 }
            Column {
                Row {
                    spacing: 6
                    Text {
                        id: bpm
                        text: insight.format("tempo", v.readings.tempo)
                        font.family: "Archivo"; font.pixelSize: 52; font.weight: Font.Light
                        font.variableAxes: { "wdth": 125 }
                        font.letterSpacing: -0.03 * 52
                        font.features: { "tnum": 1 }
                        lineHeight: 0.9
                        color: theme.brightForeground
                    }
                    Text {
                        anchors.baseline: bpm.baseline
                        text: "BPM"
                        font.family: "Archivo"; font.pixelSize: 12; font.weight: Font.DemiBold
                        font.variableAxes: { "wdth": 125 }
                        color: theme.lightForeground
                    }
                }
                Text {
                    visible: v.baselines.tempo !== undefined
                    topPadding: 4
                    textFormat: Text.StyledText
                    text: "Yours <b><font color=\"" + theme.foreground + "\">"
                          + insight.format("tempo", v.baselines.tempo ? v.baselines.tempo.mean : undefined) + "</font></b>"
                    font.family: "Archivo"; font.pointSize: 9.375
                    color: theme.lightForeground
                }
            }
        }

        Text {
            Layout.topMargin: 14
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            textFormat: Text.StyledText
            text: v.card.measure === "pending" || v.card.measure === "unknown" ? "haystack is measuring this track…"
                  : v.card.measure === "no_preview" ? "No Readings: Deezer has no preview of this track."
                  : (v.summary.lead ? "<b><font color=\"" + theme.brightForeground + "\">"
                                       + v.summary.lead + "</font></b>" : "") + v.summary.tail
            font.family: "Archivo"; font.pointSize: 10.125
            lineHeight: 1.1
            color: theme.foreground
        }

        ColumnLayout {
            Layout.topMargin: 14
            Layout.fillWidth: true
            spacing: 4
            Repeater {
                model: insight.scaleTraits
                delegate: MiniTraitRow {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                    trait: modelData; readings: v.readings; baselines: v.baselines
                }
            }
        }

        Item { Layout.fillHeight: true }
        PlaybackNotice { Layout.fillWidth: true; Layout.topMargin: 12 }

        RowLayout {
            Layout.topMargin: 12
            Layout.fillWidth: true
            spacing: 8
            VerdictButton { compact: true; icon: "ban"; iconColor: theme.red; label: "Dislike"; key: "X"; onClicked: review.dislike() }
            VerdictButton { compact: true; icon: "pass"; label: "Pass"; key: "P"; onClicked: review.pass() }
            StarBar {
                Layout.fillWidth: true
                compact: true
                stars: v.card.verdict && v.card.verdict.kind === "rating" ? v.card.verdict.stars : 0
                onRate: (n) => n > 0 ? review.rate(n, false) : review.rate(-n, true)
            }
        }

        RowLayout {
            Layout.topMargin: 14
            Layout.fillWidth: true
            spacing: 8
            Transport { compact: true }
            Progress { Layout.fillWidth: true; knob: false }
            Text {
                text: insight.time(engine.positionMs) + " / " + insight.time(engine.durationMs || v.card.duration_ms || 0)
                font.family: "Archivo"; font.pixelSize: 13; font.features: { "tnum": 1 }
                color: theme.lightForeground
            }
        }
    }
}
