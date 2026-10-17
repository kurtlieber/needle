import QtQuick

// A playlist's progress as one bar: a segment per verdict kind (5★ down to 1★, Pass, Dislike), then the songs
// still to review, each as wide as its share. The counts are always written next to it: colour never says them
// alone.
Item {
    id: bar
    property var stars: [0, 0, 0, 0, 0]
    property int passed: 0
    property int disliked: 0
    property int remaining: 0          // songs still to review
    implicitHeight: 4
    readonly property var parts: [
        [stars[4] || 0, theme.accent], [stars[3] || 0, Qt.alpha(theme.accent, 0.78)],
        [stars[2] || 0, Qt.alpha(theme.accent, 0.58)], [stars[1] || 0, Qt.alpha(theme.accent, 0.40)],
        [stars[0] || 0, Qt.alpha(theme.accent, 0.26)], [passed, theme.lightForeground], [disliked, theme.red],
        [remaining, Qt.alpha(theme.foreground, 0.07)]].filter(p => p[0] > 0)
    readonly property int total: parts.reduce((a, p) => a + p[0], 0)
    readonly property real usable: width - 2 * Math.max(0, parts.length - 1)
    Row {
        spacing: 2
        Repeater {
            model: bar.parts
            delegate: Rectangle {
                required property var modelData
                width: Math.max(2, bar.usable * modelData[0] / Math.max(1, bar.total))
                height: bar.height
                radius: Math.min(2, bar.height / 2)
                color: modelData[1]
            }
        }
    }
    Accessible.role: Accessible.ProgressBar
    Accessible.name: "Reviewed " + (bar.total - bar.remaining) + " of " + bar.total
}
