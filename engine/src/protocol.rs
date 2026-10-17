//! The socket protocol between needle (the Qt app) and this engine: one JSON object per line.
//! needle sends commands; the engine sends events. Both are tagged (`"cmd"` / `"event"`).

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Deserialize)]
#[serde(tag = "cmd", rename_all = "snake_case")]
pub enum Command {
    /// Start Spotify's own sign-in in the browser (only needed when no saved credentials work).
    Login,
    /// Load a track (a bare 22-character id or a `spotify:track:` URI).
    Load {
        id: String,
        #[serde(default = "yes")]
        play: bool,
        #[serde(default)]
        position_ms: u32,
    },
    Play,
    Pause,
    Toggle,
    Stop,
    Seek { position_ms: u32 },
    SeekBy { delta_ms: i64 },
    /// 0.0 ..= 1.0
    Volume { value: f64 },
    /// Stream spectrum frames (30 Hz) or not.
    Spectrum { enabled: bool },
    /// Read a playlist's songs and their metadata (a bare id or a `spotify:playlist:` URI). haystack can't list a
    /// playlist you follow (Spotify's dev mode refuses), so needle reads it here and sends it on (ADR 0008).
    Playlist { id: String },
    Quit,
}

fn yes() -> bool {
    true
}

#[derive(Debug, Clone, Copy, PartialEq, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum AuthState {
    /// No usable credentials: needle should offer a sign-in (the `login` command).
    Needed,
    /// The browser sign-in is open.
    Pending,
    Ok,
    Error,
}

#[derive(Debug, Clone, Copy, PartialEq, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum PlayState {
    Stopped,
    Loading,
    Playing,
    Paused,
}

#[derive(Debug, Clone, PartialEq, Serialize)]
#[serde(tag = "event", rename_all = "snake_case")]
pub enum Event {
    Ready {
        version: &'static str,
    },
    Auth {
        state: AuthState,
        #[serde(skip_serializing_if = "Option::is_none")]
        user: Option<String>,
        #[serde(skip_serializing_if = "Option::is_none")]
        message: Option<String>,
    },
    State {
        state: PlayState,
        #[serde(skip_serializing_if = "Option::is_none")]
        id: Option<String>,
        position_ms: u32,
        #[serde(skip_serializing_if = "Option::is_none")]
        duration_ms: Option<u32>,
    },
    Position {
        id: String,
        position_ms: u32,
    },
    /// The track played to its end.
    End {
        id: String,
    },
    /// Spotify can't play this track here (region, licence).
    Unavailable {
        id: String,
    },
    Spectrum {
        bands: Vec<u8>,
        peaks: Vec<u8>,
    },
    /// A playlist's songs, in order, as Spotify's own clients see them. `snapshot_id` is the Web API's name for the
    /// same version marker (the revision, base64).
    Playlist {
        id: String,
        name: String,
        snapshot_id: String,
        tracks: Vec<PlaylistTrack>,
    },
    PlaylistError {
        id: String,
        message: String,
    },
    /// A media key or the bar asked for something only needle can do (it owns the Review queue).
    Mpris {
        action: &'static str,
    },
    Error {
        message: String,
    },
}

/// One song of a playlist, with what haystack needs for a track card (the field names of haystack's API).
#[derive(Debug, Clone, PartialEq, Serialize)]
pub struct PlaylistTrack {
    pub id: String,
    pub title: String,
    pub artists: Vec<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub album: Option<String>,
    /// album | single | compilation, as the Web API says it (an EP is a single there)
    #[serde(skip_serializing_if = "Option::is_none")]
    pub album_type: Option<&'static str>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub release_date: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub duration_ms: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub cover: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub cover_small: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub isrc: Option<String>,
}

impl Event {
    pub fn line(&self) -> String {
        let mut s = serde_json::to_string(self).expect("events always serialise");
        s.push('\n');
        s
    }
}

/// `4MbtaySL1453JwADvGryCL` or `spotify:track:4MbtaySL1453JwADvGryCL` -> the URI; anything else -> None.
pub fn track_uri(id: &str) -> Option<String> {
    let bare = id.strip_prefix("spotify:track:").unwrap_or(id);
    (bare.len() == 22 && bare.bytes().all(|b| b.is_ascii_alphanumeric())).then(|| format!("spotify:track:{bare}"))
}

/// `5fRNWwLqDkFfGQdbMDZxU9` or `spotify:playlist:5fRNWwLqDkFfGQdbMDZxU9` -> the URI; anything else -> None.
pub fn playlist_uri(id: &str) -> Option<String> {
    let bare = id.strip_prefix("spotify:playlist:").unwrap_or(id);
    (bare.len() == 22 && bare.bytes().all(|b| b.is_ascii_alphanumeric())).then(|| format!("spotify:playlist:{bare}"))
}

/// Standard base64 with padding: a playlist revision as the Web API's snapshot_id.
pub fn base64(bytes: &[u8]) -> String {
    const A: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    let mut s = String::with_capacity(bytes.len().div_ceil(3) * 4);
    for c in bytes.chunks(3) {
        let n = (c[0] as u32) << 16 | (*c.get(1).unwrap_or(&0) as u32) << 8 | *c.get(2).unwrap_or(&0) as u32;
        for (i, shift) in [18, 12, 6, 0].into_iter().enumerate() {
            s.push(if i <= c.len() { A[(n >> shift & 63) as usize] as char } else { '=' });
        }
    }
    s
}

/// The bare id of a `spotify:track:` URI.
pub fn bare_id(uri: &str) -> String {
    uri.rsplit(':').next().unwrap_or(uri).to_string()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn commands_parse() {
        let c: Command = serde_json::from_str(r#"{"cmd":"load","id":"4MbtaySL1453JwADvGryCL"}"#).unwrap();
        assert_eq!(c, Command::Load { id: "4MbtaySL1453JwADvGryCL".into(), play: true, position_ms: 0 });
        let c: Command = serde_json::from_str(r#"{"cmd":"seek_by","delta_ms":-10000}"#).unwrap();
        assert_eq!(c, Command::SeekBy { delta_ms: -10000 });
        let c: Command = serde_json::from_str(r#"{"cmd":"spectrum","enabled":false}"#).unwrap();
        assert_eq!(c, Command::Spectrum { enabled: false });
        assert!(serde_json::from_str::<Command>(r#"{"cmd":"explode"}"#).is_err());
        let c: Command = serde_json::from_str(r#"{"cmd":"playlist","id":"07sXM6LKembbBjAetqKwxN"}"#).unwrap();
        assert_eq!(c, Command::Playlist { id: "07sXM6LKembbBjAetqKwxN".into() });
    }

    #[test]
    fn events_serialise_one_per_line() {
        let e = Event::State { state: PlayState::Playing, id: Some("x".into()), position_ms: 72000, duration_ms: None };
        assert_eq!(e.line(), "{\"event\":\"state\",\"state\":\"playing\",\"id\":\"x\",\"position_ms\":72000}\n");
        let e = Event::Auth { state: AuthState::Needed, user: None, message: None };
        assert_eq!(e.line(), "{\"event\":\"auth\",\"state\":\"needed\"}\n");
        assert_eq!(Event::Mpris { action: "next" }.line(), "{\"event\":\"mpris\",\"action\":\"next\"}\n");
        let t = PlaylistTrack {
            id: "5ubH30Ad3HJNu2W7EsKd9O".into(),
            title: "RESONATE".into(),
            artists: vec!["SXULBXRN".into()],
            album: None,
            album_type: Some("single"),
            release_date: None,
            duration_ms: Some(137042),
            cover: None,
            cover_small: None,
            isrc: Some("BC6M22604272".into()),
        };
        let e = Event::Playlist { id: "p".into(), name: "OG Drift Phonk".into(), snapshot_id: "s".into(), tracks: vec![t] };
        assert_eq!(
            e.line(),
            "{\"event\":\"playlist\",\"id\":\"p\",\"name\":\"OG Drift Phonk\",\"snapshot_id\":\"s\",\"tracks\":[{\"id\":\"5ubH30Ad3HJNu2W7EsKd9O\",\
             \"title\":\"RESONATE\",\"artists\":[\"SXULBXRN\"],\"album_type\":\"single\",\"duration_ms\":137042,\"isrc\":\"BC6M22604272\"}]}\n"
        );
        let e = Event::PlaylistError { id: "p".into(), message: "no".into() };
        assert_eq!(e.line(), "{\"event\":\"playlist_error\",\"id\":\"p\",\"message\":\"no\"}\n");
    }

    #[test]
    fn track_ids() {
        assert_eq!(track_uri("4MbtaySL1453JwADvGryCL").as_deref(), Some("spotify:track:4MbtaySL1453JwADvGryCL"));
        assert_eq!(track_uri("spotify:track:4MbtaySL1453JwADvGryCL").as_deref(), Some("spotify:track:4MbtaySL1453JwADvGryCL"));
        assert_eq!(track_uri("spotify:album:4MbtaySL1453JwADvGryCL").as_deref(), None);
        assert_eq!(track_uri("short"), None);
        assert_eq!(bare_id("spotify:track:abc"), "abc");
        assert_eq!(playlist_uri("07sXM6LKembbBjAetqKwxN").as_deref(), Some("spotify:playlist:07sXM6LKembbBjAetqKwxN"));
        assert_eq!(playlist_uri("spotify:track:07sXM6LKembbBjAetqKwxN"), None);
    }

    #[test]
    fn revisions_read_as_snapshot_ids() {
        // Wide Mouth Bass, read through librespot and through the Web API on 2026-10-16
        let rev = [0, 0, 0, 7, 0x50, 0xd2, 0xbf, 0xfa, 0x96, 0xc2, 0x94, 0x42, 0x4b, 0x40, 0x61, 0x98, 0xed, 0x56, 0x0f, 0x4b,
                   0xb1, 0xef, 0x13, 0x76];
        assert_eq!(base64(&rev), "AAAAB1DSv/qWwpRCS0BhmO1WD0ux7xN2");
        assert_eq!(base64(b"f"), "Zg==");
        assert_eq!(base64(b"fo"), "Zm8=");
        assert_eq!(base64(b"foo"), "Zm9v");
        assert_eq!(base64(b""), "");
    }
}
