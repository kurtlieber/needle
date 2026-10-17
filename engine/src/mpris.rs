//! MPRIS (`org.mpris.MediaPlayer2.needle`), so media keys and the Omarchy bar control needle.
//! Play, pause and seek act on the player directly; next and previous go to needle, which owns the queue.
//! mpris-server's player isn't Send, so it lives on its own thread with a single-threaded runtime.

use mpris_server::{Metadata, PlaybackStatus, Player, Time, TrackId};
use tokio::sync::mpsc::{UnboundedReceiver, UnboundedSender};

#[derive(Debug)]
pub enum Update {
    Status(PlaybackStatus),
    Track { id: String, title: String, artists: Vec<String>, album: String, art_url: Option<String>, length_ms: u32 },
    Position(u32),
    /// A jump (seek), which MPRIS clients are told about.
    Seeked(u32),
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Action {
    PlayPause,
    Play,
    Pause,
    Stop,
    Next,
    Previous,
    Raise,
    SeekBy(i64),
    SeekTo(u32),
}

pub fn spawn(mut updates: UnboundedReceiver<Update>, actions: UnboundedSender<Action>) {
    std::thread::Builder::new()
        .name("mpris".into())
        .spawn(move || {
            let rt = tokio::runtime::Builder::new_current_thread().enable_all().build().expect("mpris runtime");
            let local = tokio::task::LocalSet::new();
            local.block_on(&rt, async move {
                let player = match Player::builder("needle")
                    .identity("needle")
                    .desktop_entry("needle")
                    .can_play(true)
                    .can_pause(true)
                    .can_seek(true)
                    .can_go_next(true)
                    .can_go_previous(true)
                    .can_raise(true)
                    .can_control(true)
                    .build()
                    .await
                {
                    Ok(p) => p,
                    Err(e) => {
                        log::warn!("MPRIS unavailable (media keys won't reach needle): {e}");
                        return;
                    }
                };
                let send = |a: Action| {
                    let tx = actions.clone();
                    move |_: &Player| {
                        let _ = tx.send(a);
                    }
                };
                player.connect_play_pause(send(Action::PlayPause));
                player.connect_play(send(Action::Play));
                player.connect_pause(send(Action::Pause));
                player.connect_stop(send(Action::Stop));
                player.connect_next(send(Action::Next));
                player.connect_previous(send(Action::Previous));
                player.connect_raise(send(Action::Raise));
                let tx = actions.clone();
                player.connect_seek(move |_, t: Time| {
                    let _ = tx.send(Action::SeekBy(t.as_millis()));
                });
                let tx = actions.clone();
                player.connect_set_position(move |_, _id, t: Time| {
                    let _ = tx.send(Action::SeekTo(t.as_millis().max(0) as u32));
                });
                tokio::task::spawn_local(player.run());
                while let Some(u) = updates.recv().await {
                    let r = match u {
                        Update::Status(s) => player.set_playback_status(s).await,
                        Update::Track { id, title, artists, album, art_url, length_ms } => {
                            let mut m = Metadata::builder()
                                .title(title)
                                .artist(artists)
                                .album(album)
                                .length(Time::from_millis(length_ms as i64));
                            if let Ok(tid) = TrackId::try_from(format!("/org/needle/track/{id}")) {
                                m = m.trackid(tid);
                            }
                            if let Some(url) = art_url {
                                m = m.art_url(url);
                            }
                            player.set_metadata(m.build()).await
                        }
                        Update::Position(ms) => {
                            player.set_position(Time::from_millis(ms as i64));
                            Ok(())
                        }
                        Update::Seeked(ms) => {
                            player.set_position(Time::from_millis(ms as i64));
                            player.seeked(Time::from_millis(ms as i64)).await
                        }
                    };
                    if let Err(e) = r {
                        log::debug!("MPRIS update failed: {e}");
                    }
                }
            });
        })
        .expect("spawn the MPRIS thread");
}
