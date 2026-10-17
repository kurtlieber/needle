import QtQuick
import QtQuick.Layouts

// The wide layout's top bar: the wordmark, the Review queues, the Playlist review's chip, the sync status and the
// shortcuts button.
RowLayout {
    id: bar
    signal helpRequested()
    height: 60
    spacing: 28
    Wordmark {}
    RowLayout {
        spacing: 12
        QueueTabs {}
        PlaylistChip {}
    }
    Item { Layout.fillWidth: true }
    StatusLine {}
    IconButton { icon: "keyboard"; label: "Keyboard shortcuts"; onClicked: bar.helpRequested() }
}
