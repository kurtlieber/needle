//! Reading a playlist's songs through librespot (ADR 0008). Spotify's dev mode won't let haystack list a playlist
//! you follow but don't own; Spotify's own clients can, so the engine reads it the same way and needle sends
//! the songs on to haystack. The track metadata comes along, so haystack needs no Web API call per song.

use std::time::Duration;

use librespot_core::error::ErrorKind;
use librespot_core::session::Session;
use librespot_core::SpotifyUri;
use librespot_metadata::image::ImageSize;
use librespot_metadata::{Album, Metadata, Playlist, Track};
use tokio::time::{interval, sleep, MissedTickBehavior};

use crate::protocol::{base64, bare_id, Event, PlaylistTrack};

/// One track's metadata every 200 ms. librespot allows 10 requests a second to Spotify and playback shares them:
/// a read that used them all up would make the next song fail to load, so it takes half. A 160-song playlist
/// takes about 30 s.
const PACE: Duration = Duration::from_millis(200);
const TRIES: u32 = 4;

/// Reads queue up: two at once would take all of librespot's requests again.
static ONE_AT_A_TIME: tokio::sync::Mutex<()> = tokio::sync::Mutex::const_new(());

/// The playlist's songs in order, or why not. Podcast episodes and local files are left out; a song whose
/// metadata won't load is left out too (it couldn't be played either).
pub async fn read(session: Session, uri: String) -> Event {
    let _turn = ONE_AT_A_TIME.lock().await;
    let id = bare_id(&uri);
    let pl = match SpotifyUri::from_uri(&uri) {
        Ok(u) => Playlist::get(&session, &u).await,
        Err(e) => Err(e),
    };
    let pl = match pl {
        Ok(pl) => pl,
        Err(e) => return Event::PlaylistError { id, message: format!("Spotify won't read this playlist: {e}") },
    };
    let mut tick = interval(PACE);
    tick.set_missed_tick_behavior(MissedTickBehavior::Delay);
    let mut songs = Vec::new();
    let mut seen = std::collections::HashSet::new();
    for u in pl.tracks().filter(|u| matches!(u, SpotifyUri::Track { .. })) {
        if !seen.insert(u.clone()) {
            continue; // listed twice: it plays once
        }
        for attempt in 1..=TRIES {
            tick.tick().await;
            match Track::get(&session, u).await {
                Ok(t) => {
                    songs.push(song(&t));
                    break;
                }
                // librespot's own limit (or Spotify's): wait for it to refill, then try again
                Err(e) if e.kind == ErrorKind::ResourceExhausted && attempt < TRIES => {
                    sleep(Duration::from_secs(2 * attempt as u64)).await
                }
                Err(e) => {
                    log::warn!("playlist song {}: {e}", u.to_uri().unwrap_or_default());
                    break;
                }
            }
        }
    }
    log::info!("read playlist {id}: {} songs", songs.len());
    Event::Playlist { id, name: pl.name().to_string(), snapshot_id: base64(&pl.revision), tracks: songs }
}

fn image(album: &Album, size: ImageSize) -> Option<String> {
    let c = album.covers.iter().find(|c| c.size == size).or_else(|| album.covers.first())?;
    Some(format!("https://i.scdn.co/image/{}", c.id.to_base16().ok()?))
}

fn song(t: &Track) -> PlaylistTrack {
    use librespot_metadata::album::AlbumType;
    let date = &t.album.date;
    PlaylistTrack {
        id: bare_id(&t.id.to_uri().unwrap_or_default()),
        title: t.name.clone(),
        artists: t.artists.iter().map(|a| a.name.clone()).filter(|n| !n.is_empty()).collect(),
        album: Some(t.album.name.clone()).filter(|n| !n.is_empty()),
        album_type: match t.album.album_type {
            AlbumType::ALBUM => Some("album"),
            AlbumType::SINGLE | AlbumType::EP => Some("single"),
            AlbumType::COMPILATION => Some("compilation"),
            _ => None,
        },
        // a missing date reads as the epoch
        release_date: (date.year() > 1900).then(|| format!("{:04}-{:02}-{:02}", date.year(), u8::from(date.month()), date.day())),
        duration_ms: u32::try_from(t.duration).ok().filter(|d| *d > 0),
        cover: image(&t.album, ImageSize::LARGE),
        cover_small: image(&t.album, ImageSize::SMALL),
        isrc: t.external_ids.iter().find(|e| e.external_type == "isrc").map(|e| e.id.to_uppercase()),
    }
}
