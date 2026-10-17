import QtQuick
import QtQuick.Effects

// Album art with rounded corners, a hairline edge and (optionally) a soft drop shadow.
Item {
    id: c
    property url source
    property real radius: 16
    property bool shadow: true
    property string placeholder: "needle"     // the icon while there's no picture

    Rectangle {           // the shadow, cast by a rounded plate behind the art (masking and shadow don't mix)
        visible: c.shadow
        anchors.fill: parent
        radius: c.radius
        color: theme.darkerBackground
        layer.enabled: c.shadow
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Qt.alpha(theme.darkerBackground, 0.75)
            shadowVerticalOffset: 18
            shadowBlur: 1.0
            blurMax: 48
            autoPaddingEnabled: true
        }
    }
    Item {
        id: art
        anchors.fill: parent
        layer.enabled: true
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: mask
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }
        Rectangle { anchors.fill: parent; color: theme.lighterBackground }
        Icon {
            anchors.centerIn: parent
            name: c.placeholder; color: theme.muted; size: Math.min(c.width, c.height) / 3
            visible: img.status !== Image.Ready
        }
        Image {
            id: img
            anchors.fill: parent
            source: c.source
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            sourceSize.width: c.width * Screen.devicePixelRatio
            sourceSize.height: c.height * Screen.devicePixelRatio
        }
    }
    Item {
        id: mask
        anchors.fill: parent
        layer.enabled: true
        layer.smooth: true
        visible: false
        Rectangle { anchors.fill: parent; radius: c.radius; color: "black"; antialiasing: true }
    }
    Rectangle {
        anchors.fill: parent
        radius: c.radius
        color: "transparent"
        border.width: 1
        border.color: Qt.alpha(theme.foreground, 0.1)
    }
}
