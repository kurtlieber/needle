import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

ApplicationWindow {
    id: win
    width: 1440
    height: 900
    minimumWidth: 380
    minimumHeight: 640
    visible: true
    title: review.current.title ? review.current.title + " – needle" : "needle"
    color: theme.background

    Material.theme: theme.dark ? Material.Dark : Material.Light
    Material.accent: theme.accent
    font.family: "Archivo"

    // The wide Concept A layout needs about 1100 x 860; below that, the compact column.
    readonly property bool compact: width < 1100 || height < 860
    // The Playlists queue shows the picker until a Playlist review starts, and again on L.
    readonly property bool picking: review.hasState && review.queue === "playlists"
                                    && (playlists.pickerOpen || !review.hasPlaylistReview)

    Backdrop {
        visible: prefs.glow
        source: review.current.coverSource || ""
    }

    FocusScope {
        id: keys
        objectName: "keys"
        anchors.fill: parent
        focus: true

        Loader {
            id: view
            anchors.fill: parent
            active: review.hasState && (win.picking || !review.queueDone)
            sourceComponent: win.picking ? pickerView : win.compact ? compactView : wideView
        }
        Component { id: wideView; WideView { onHelpRequested: help.open = !help.open } }
        Component { id: compactView; CompactView { onHelpRequested: help.open = !help.open } }
        Component {
            id: pickerView
            PlaylistPicker {
                compact: win.compact
                onHelpRequested: help.open = !help.open
                onKeysBack: keys.forceActiveFocus()
            }
        }

        // ---- states with no track to show ----
        Notice {
            anchors.centerIn: parent
            visible: !review.hasState
            title: haystack.status === "no_token" ? "needle needs haystack's token"
                   : haystack.status === "unauthorised" ? "haystack refused needle's token"
                   : haystack.status === "offline" ? "Can't reach haystack" : "Fetching your Review queues…"
            body: haystack.status === "no_token" || haystack.status === "unauthorised"
                  ? "Put haystack's API token in <tt>" + haystack.tokenFile + "</tt> (one line, readable only by you), then needle picks it up on its own."
                  : haystack.status === "offline" ? "needle tries " + haystack.servers.join(", then ") + " (<tt>~/.config/needle/config.toml</tt>), and keeps trying. Nothing has been fetched on this computer yet, so there is nothing to Review offline."
                  : ""
            action: haystack.status === "offline" ? "Try again" : ""
            onActivated: haystack.refresh()
        }
        Notice {
            anchors.centerIn: parent
            visible: review.hasState && review.queueDone && !win.picking
            title: review.queue === "discoveries" ? "This week's Discoveries are reviewed"
                   : review.queue === "calibrate" ? "Calibrate is done for now"
                   : review.queue === "playlists" ? "These playlists are reviewed" : "Every like has a Rating"
            body: review.queue === "discoveries"
                  ? (review.nextBatchText ? "The next batch lands " + review.nextBatchText + ". " : "")
                    + (review.doneNext === "calibrate"
                       ? (review.calibrateLeft === 1 ? "1 song in Calibrate still needs a verdict."
                          : review.calibrateLeft + " songs in Calibrate still need a verdict.")
                       : review.likesLeft > 0 ? review.likesLeft + " liked songs still have no Rating." : "")
                  : review.queue === "calibrate" ? "haystack asks about more songs each week."
                  : review.queue === "playlists" ? "Every song in this review has a verdict now."
                  : "New likes show up here after the next sync."
            action: review.queue === "playlists" ? "Choose more playlists"
                    : review.doneNext === "calibrate" ? "Calibrate"
                    : review.doneNext === "likes" ? "Rate your likes"
                    : review.doneNext === "discoveries" ? "Back to Discoveries" : ""
            actionKey: review.queue === "playlists" ? "L" : review.doneNext === review.tabNext ? "Tab" : ""
            onActivated: {
                if (review.queue === "playlists") playlists.openPicker()
                else review.queue = review.doneNext
            }
        }

        HelpOverlay { id: help; picking: win.picking }

        Keys.onPressed: (e) => {
            const shifted = { [Qt.Key_Exclam]: 1, [Qt.Key_At]: 2, [Qt.Key_NumberSign]: 3, [Qt.Key_Dollar]: 4, [Qt.Key_Percent]: 5 };
            let digit = e.key >= Qt.Key_1 && e.key <= Qt.Key_5 ? e.key - Qt.Key_0 : (shifted[e.key] || 0);
            const shift = (e.modifiers & Qt.ShiftModifier) || shifted[e.key] !== undefined;
            e.accepted = true;
            if (help.open && (e.key === Qt.Key_Escape || e.key === Qt.Key_Question)) { help.open = false; return }
            if (win.picking && view.item && view.item.handleKey && view.item.handleKey(e)) return
            if (win.picking) {      // no verdicts or playback keys while picking: only the window's own
                switch (e.key) {
                case Qt.Key_Tab: case Qt.Key_Backtab: review.switchQueue(); break
                case Qt.Key_G: prefs.toggleGlow(); break
                case Qt.Key_M: prefs.toggleMotion(); break
                case Qt.Key_Question: help.open = !help.open; break
                case Qt.Key_Q: Qt.quit(); break
                default: e.accepted = false
                }
                return
            }
            if (digit > 0) { review.rate(digit, shift); return }
            switch (e.key) {
            case Qt.Key_P: review.pass(); break
            case Qt.Key_X: review.dislike(); break
            case Qt.Key_Space: engine.toggle(); break
            case Qt.Key_Left: engine.seekBy(-10000); break
            case Qt.Key_Right: engine.seekBy(10000); break
            case Qt.Key_J: engine.seekBy(30000); break
            case Qt.Key_N: review.next(); break
            case Qt.Key_B: review.back(); break
            case Qt.Key_Tab: case Qt.Key_Backtab: review.switchQueue(); break
            case Qt.Key_L: review.queue = "playlists"; playlists.openPicker(); break
            case Qt.Key_G: prefs.toggleGlow(); break
            case Qt.Key_M: prefs.toggleMotion(); break
            case Qt.Key_Question: help.open = !help.open; break
            case Qt.Key_Escape: help.open = false; break
            case Qt.Key_Q: Qt.quit(); break
            case Qt.Key_Return: case Qt.Key_Enter:
                if (engine.auth === "needed" || engine.auth === "error") engine.login()
                else if (review.queueDone && review.queue === "playlists") playlists.openPicker()
                else if (review.queueDone) review.switchQueue()
                break
            default: e.accepted = false
            }
        }
    }

    // the spectrum streams only while it is on screen
    Component.onCompleted: engine.setSpectrum(!compact)
    onCompactChanged: engine.setSpectrum(!compact && !picking)
    onPickingChanged: engine.setSpectrum(!compact && !picking)
    Connections {
        target: engine
        function onMpris(action) { if (action === "raise") { win.raise(); win.requestActivate() } }
    }
}
