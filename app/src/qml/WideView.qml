import QtQuick
import QtQuick.Layouts
import Needle

// Concept A, "Calibrated", wide: cover, tempo and live spectrum on the left; the track, why haystack picked it,
// its styles, the summary sentence and six calibrated Trait scales on the right; transport and verdicts below.
Item {
    id: v
    signal helpRequested()
    readonly property var card: review.current
    readonly property var readings: card.readings || ({})
    readonly property var baselines: review.baseline
    readonly property var tempoBase: baselines.tempo ? baselines.tempo.mean : undefined
    readonly property var summary: insight.summary(readings, baselines)

    // ---- top bar ----
    TopBar {
        id: topbar
        x: 28; width: parent.width - 56
        onHelpRequested: v.helpRequested()
    }

    // ---- left column ----
    Item {
        id: left
        x: 28; y: 80; width: 392; height: parent.height - y - 128
        // the cover gives way first when the window is shorter than the mockup's 1000 px
        Cover { id: cover; width: Math.max(260, Math.min(392, left.height - 330)); height: width; source: v.card.coverSource || "" }

        Item {
            id: tempo
            y: cover.height + 28; width: parent.width; height: 92
            Pulse { id: pulse; anchors.verticalCenter: parent.verticalCenter; bpm: v.readings.tempo || 0; playing: engine.playing }
            Column {
                anchors.left: pulse.right; anchors.leftMargin: 22; anchors.verticalCenter: parent.verticalCenter
                spacing: 8
                Row {
                    spacing: 10
                    Text {
                        id: bpm
                        text: insight.format("tempo", v.readings.tempo)
                        font.family: "Archivo"; font.pixelSize: 88; font.weight: Font.Light
                        font.variableAxes: { "wdth": 125 }
                        font.letterSpacing: -0.03 * 88
                        font.features: { "tnum": 1 }
                        lineHeight: 0.9
                        color: theme.brightForeground
                    }
                    Text {
                        anchors.baseline: bpm.baseline
                        text: "BPM"
                        font.family: "Archivo"; font.pixelSize: 15; font.weight: Font.DemiBold
                        font.variableAxes: { "wdth": 125 }
                        color: theme.lightForeground
                    }
                }
                Text {
                    visible: v.tempoBase !== undefined
                    textFormat: Text.StyledText
                    text: "Yours: <b><font color=\"" + theme.foreground + "\">"
                          + insight.format("tempo", v.tempoBase) + "</font></b>. "
                          + insight.tempoComparison(v.readings.tempo, v.tempoBase)
                    font.family: "Archivo"; font.pointSize: 10.125
                    color: theme.lightForeground
                }
            }
        }

        PlaybackNotice { y: tempo.y + tempo.height + 30; width: parent.width }
        Column {
            visible: engine.auth === "ok" || engine.auth === "starting"
            y: tempo.y + tempo.height + 30; width: parent.width
            Text {
                text: "Live spectrum"
                font.family: "Archivo"; font.pixelSize: 13; font.weight: Font.DemiBold
                color: theme.foreground
                bottomPadding: 12
            }
            Spectrum {
                // a painted texture, so it follows the window's zoom (Main.qml)
                readonly property real zoom: Window.window ? Window.window.zoom || 1 : 1
                width: parent.width; height: 96
                textureSize: Qt.size(width * zoom, height * zoom)
                bands: engine.bands; peaks: engine.peaks
                barColor: theme.foreground; peakColor: theme.accent
                reducedMotion: prefs.reducedMotion
            }
            Item {
                width: parent.width; height: 26
                Text { y: 8; text: "20 Hz"; font.family: "Archivo"; font.pixelSize: 12; color: theme.lightForeground }
                Text { y: 8; anchors.right: parent.right; text: "20 kHz"; font.family: "Archivo"; font.pixelSize: 12; color: theme.lightForeground }
            }
        }
    }

    // ---- right column ----
    Item {
        id: right
        x: left.x + left.width + 56; y: 80
        width: parent.width - x - 28; height: parent.height - y - 128

        Rectangle {           // up next: big enough to read the next cover at a glance
            id: upnext
            visible: review.upNext.id !== undefined
            anchors.right: parent.right
            width: 340; height: upCover.height + 24
            radius: 14
            color: Qt.alpha(theme.foreground, 0.04)
            Cover {
                id: upCover
                x: 12; y: 12; width: 120; height: 120; radius: 8; shadow: false
                source: review.upNext.coverSource || ""
            }
            Column {
                x: upCover.x + upCover.width + 16
                width: upnext.width - x - 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                Text {
                    width: parent.width; elide: Text.ElideRight
                    text: review.upNext.fromPlaylist ? "Up next, from " + review.upNext.fromPlaylist : "Up next"
                    font.family: "Archivo"; font.pixelSize: 13; color: theme.lightForeground
                }
                Text {
                    width: parent.width
                    text: review.upNext.title || ""
                    wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                    font.family: "Archivo"; font.pixelSize: 18; font.weight: Font.DemiBold
                    lineHeight: 1.1
                    color: theme.brightForeground
                }
                Text {
                    width: parent.width; elide: Text.ElideRight
                    text: (review.upNext.artists || []).join(", ")
                    font.family: "Archivo"; font.pixelSize: 15
                    color: theme.foreground
                }
            }
        }

        Column {
            id: head
            width: parent.width - (upnext.visible ? upnext.width + 24 : 0)
            Text {
                width: parent.width
                text: v.card.title || ""
                wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                fontSizeMode: Text.Fit; minimumPixelSize: 34; height: Math.min(implicitHeight, 120)
                font.family: "Archivo"; font.pixelSize: 54; font.weight: Font.Bold
                font.variableAxes: { "wdth": 112 }
                font.letterSpacing: -0.02 * 54
                lineHeight: 1.05
                color: theme.brightForeground
            }
            Text {
                topPadding: 10
                text: (v.card.artists || []).join(", ")
                font.family: "Archivo"; font.pixelSize: 20; font.weight: Font.Medium
                color: theme.foreground
            }
            Text {
                topPadding: 4
                text: insight.albumLine(v.card)
                font.family: "Archivo"; font.pointSize: 10.125
                color: theme.lightForeground
            }
        }

        Text {
            id: why
            y: head.y + head.height + 18
            width: Math.min(parent.width - (upnext.visible && y < upnext.height ? upnext.width + 24 : 0), 640)
            visible: text !== ""
            textFormat: Text.StyledText
            text: review.queue === "playlists" ? insight.playlistLine(v.card, theme.foreground)
                                               : insight.why(v.card.why || ({}), theme.foreground, review.features)
            wrapMode: Text.WordWrap
            lineHeight: 1.18
            font.family: "Archivo"; font.pointSize: 10.125
            color: theme.lightForeground
        }

        Flow {
            id: chips
            y: (why.visible ? why.y + why.height : head.y + head.height) + 14
            width: parent.width - (upnext.visible && y < upnext.height ? upnext.width + 24 : 0)
            spacing: 8
            Chip {
                visible: text !== ""
                outline: true
                text: review.queue === "playlists" ? "" : insight.confidence(v.card.why || ({}), review.features)
            }
            Repeater {
                model: v.card.styles || []
                delegate: Chip { required property var modelData; text: modelData[0] }
            }
        }

        Item {
            id: summaryBox
            y: chips.y + chips.height + 22
            width: parent.width; height: 52
            Rectangle { width: parent.width; height: 1; color: theme.muted }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - legend.width - 24
                textFormat: Text.StyledText
                wrapMode: Text.WordWrap
                text: v.card.measure === "pending" || v.card.measure === "unknown"
                      ? "haystack is measuring this track…"
                      : v.card.measure === "no_preview" ? "No Readings for this track: haystack found no preview of it to measure."
                      : (v.summary.lead ? "<b><font color=\"" + theme.brightForeground + "\">"
                                           + v.summary.lead + "</font></b>" : "") + v.summary.tail
                font.family: "Archivo"; font.pixelSize: 15
                color: theme.foreground
            }
            Row {
                id: legend
                anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                spacing: 20
                Row {
                    spacing: 8
                    NeedleMarker { anchors.verticalCenter: parent.verticalCenter }
                    Text { text: "This track"; font.family: "Archivo"; font.pointSize: 9.375; color: theme.lightForeground; anchors.verticalCenter: parent.verticalCenter }
                }
                Row {
                    spacing: 8
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 12; height: 12; radius: 6; color: theme.background
                        border.width: 2; border.color: theme.foreground
                    }
                    Text { text: "Your baseline"; font.family: "Archivo"; font.pointSize: 9.375; color: theme.lightForeground; anchors.verticalCenter: parent.verticalCenter }
                }
            }
        }

        Column {
            id: scales
            y: summaryBox.y + summaryBox.height + 4
            width: parent.width
            readonly property real rowHeight: Math.max(56, Math.min(80, (right.height - y) / 6))
            Repeater {
                model: insight.scaleTraits
                delegate: Item {
                    required property var modelData
                    required property int index
                    width: parent.width
                    height: scales.rowHeight
                    Rectangle { visible: index > 0; width: parent.width; height: 1; color: Qt.alpha(theme.foreground, 0.035) }
                    TraitRow { anchors.fill: parent; trait: modelData; readings: v.readings; baselines: v.baselines }
                }
            }
        }
    }

    // ---- dock ----
    Item {
        id: dock
        anchors.bottom: parent.bottom
        width: parent.width; height: 128
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.alpha(theme.darkBackground, 0) }
                GradientStop { position: 0.38; color: Qt.alpha(theme.darkBackground, 0.92) }
            }
        }
        Progress { x: 28; y: 10; width: parent.width - 56 }

        Row {
            x: 28; y: 18; height: parent.height - 18
            spacing: 20
            Transport { anchors.verticalCenter: parent.verticalCenter }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: insight.time(engine.positionMs) + " / " + insight.time(engine.durationMs || v.card.duration_ms || 0)
                font.family: "Archivo"; font.pixelSize: 13; font.features: { "tnum": 1 }
                color: theme.lightForeground
            }
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6
                Kbd { text: "J"; anchors.verticalCenter: parent.verticalCenter }
                Text { text: "jump ahead 30 s"; font.family: "Archivo"; font.pointSize: 9.375; color: theme.lightForeground; anchors.verticalCenter: parent.verticalCenter }
            }
        }

        Row {
            anchors.right: parent.right; anchors.rightMargin: 28
            y: 6; height: parent.height - 6
            spacing: 22
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 12
                VerdictButton { icon: "ban"; iconColor: theme.red; label: "Dislike"; key: "X"; onClicked: review.dislike() }
                VerdictButton { icon: "pass"; label: "Pass"; key: "P"; onClicked: review.pass() }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: 9
                spacing: 6
                StarBar {
                    id: stars
                    stars: v.card.verdict && v.card.verdict.kind === "rating" ? v.card.verdict.stars : 0
                    onRate: (n) => n > 0 ? review.rate(n, false) : review.rate(-n, true)
                }
                Row {
                    x: 4; spacing: 2
                    Repeater {
                        model: 5
                        delegate: Text {
                            required property int index
                            width: 52; horizontalAlignment: Text.AlignHCenter
                            text: index + 1
                            font.family: "Archivo"; font.pointSize: 8.625; font.weight: Font.DemiBold
                            color: theme.lightForeground
                        }
                    }
                }
            }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Row {
                    spacing: 6
                    Kbd { text: "Shift" }
                    Text { text: "+ number"; font.family: "Archivo"; font.pointSize: 9.375; color: theme.lightForeground; anchors.verticalCenter: parent.verticalCenter }
                }
                Text { text: "rates and plays the next track"; font.family: "Archivo"; font.pointSize: 9.375; color: theme.lightForeground }
            }
        }
    }
}
