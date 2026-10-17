# needle plays through its own librespot engine

needle, the desktop app for Review, owns playback instead of steering the official Spotify client. It is a Qt Quick/C++ UI in the style of Omarchy's own apps (live theme, one package, launcher entry) plus a small Rust engine built on the `librespot` crates, joined by a Unix socket. The engine plays the audio and streams position and live audio levels; the UI does layout and interaction. Transport is local and instant and never spends Web API quota, the visuals can use the real audio, and when Spotify breaks librespot the fix is a crate bump in the engine alone. Both parts live in this repo (`app/`, `engine/`) and ship as one `pacman` package; nothing on the desktop needs Python.

librespot is an unofficial, reverse-engineered client: it breaks from time to time and its use is a Spotify terms risk you accept by using needle.

**Considered Options**: a companion to the official Spotify client over MPRIS (no engine to maintain, but needle would not own playback); the stock `librespot` binary as a hidden Spotify Connect device (least code, but every play, pause and seek goes through the Web API and its shared development-mode quota); one Rust binary driving QML through `cxx-qt` (a macro-heavy bridge, and unlike Omarchy's apps); the Web Playback SDK in QtWebEngine (needs Widevine, which Arch's build lacks).
