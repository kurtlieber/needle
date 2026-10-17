//! needle-engine: needle's playback engine (ADR 0003). librespot plays the audio; needle (the Qt app) drives
//! it over a Unix socket with one JSON object per line (protocol.rs). The engine also streams the live
//! spectrum of what is playing and publishes MPRIS for media keys and the Omarchy bar, and it reads the
//! playlists haystack can't (playlist.rs, ADR 0008).
//!
//!     needle-engine --socket $XDG_RUNTIME_DIR/needle/engine.sock [--exit-on-disconnect]

mod mpris;
mod playlist;
mod protocol;
mod sink;
mod spectrum;

use std::collections::VecDeque;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::{mpsc as std_mpsc, Arc, Mutex};
use std::time::{Duration, Instant};

use librespot_core::authentication::Credentials;
use librespot_core::cache::Cache;
use librespot_core::config::SessionConfig;
use librespot_core::session::Session;
use librespot_core::SpotifyUri;
use librespot_metadata::audio::UniqueFields;
use librespot_oauth::OAuthClientBuilder;
use librespot_playback::config::{Bitrate, PlayerConfig};
use librespot_playback::mixer::{self, Mixer, MixerConfig};
use librespot_playback::player::{Player, PlayerEvent, PlayerEventChannel};
use mpris_server::PlaybackStatus;
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::UnixListener;
use tokio::sync::mpsc::{unbounded_channel, UnboundedSender};

use protocol::{bare_id, playlist_uri, track_uri, AuthState, Command, Event, PlayState};

// needle's package version (../VERSION, set by build.rs), not the crate's
const VERSION: &str = env!("NEEDLE_VERSION");
const OAUTH_REDIRECT: &str = "http://127.0.0.1:5588/login";
const OAUTH_SCOPES: &[&str] = &["streaming", "user-read-private"];
const OAUTH_DONE: &str = "<!doctype html><title>needle</title><body style=\"font-family:sans-serif;background:#121212;\
color:#bebebe;display:grid;place-items:center;height:90vh\"><p>Signed in. You can close this tab and go back to needle.</p>";

/// The connected needle, if any: events go to it as JSON lines; nothing is queued while none is connected.
#[derive(Clone, Default)]
struct Outbox(Arc<Mutex<Option<UnboundedSender<String>>>>);

impl Outbox {
    fn emit(&self, e: &Event) {
        if let Some(tx) = self.0.lock().unwrap().as_ref() {
            let _ = tx.send(e.line());
        }
    }
    fn connected(&self) -> bool {
        self.0.lock().unwrap().is_some()
    }
}

/// Everything the engine reacts to, so one task owns all the state.
enum Input {
    Command(Command),
    Player(PlayerEvent),
    /// A needle connected: send it the whole state.
    Hello,
    Bye,
    LoginResult(Result<String, String>),
}

/// Where the track is, without asking librespot (seek-by needs it).
#[derive(Clone, Copy)]
struct Clock {
    position_ms: u32,
    at: Instant,
    playing: bool,
}

impl Clock {
    fn now_ms(&self) -> u32 {
        if self.playing {
            self.position_ms.saturating_add(self.at.elapsed().as_millis() as u32)
        } else {
            self.position_ms
        }
    }
}

struct Engine {
    out: Outbox,
    cache: Cache,
    session: Option<Session>,
    player: Option<Arc<Player>>,
    mixer: Arc<dyn Mixer>,
    tap: sink::Tap,
    /// Bumped on every pause, stop and seek: spectrum frames of audio that was flushed are dropped.
    generation: Arc<AtomicU64>,
    auth: (AuthState, Option<String>, Option<String>),
    state: PlayState,
    track: Option<String>,
    duration_ms: Option<u32>,
    clock: Clock,
    mpris: UnboundedSender<mpris::Update>,
    inputs: UnboundedSender<Input>,
    exit_on_disconnect: bool,
}

fn uri_id(u: &SpotifyUri) -> String {
    bare_id(&u.to_uri().unwrap_or_default())
}

impl Engine {
    fn emit(&self, e: Event) {
        self.out.emit(&e);
    }

    fn set_auth(&mut self, state: AuthState, user: Option<String>, message: Option<String>) {
        self.auth = (state, user.clone(), message.clone());
        self.emit(Event::Auth { state, user, message });
    }

    fn emit_state(&self) {
        self.emit(Event::State {
            state: self.state,
            id: self.track.clone(),
            position_ms: self.clock.now_ms(),
            duration_ms: self.duration_ms,
        });
    }

    async fn connect(&mut self, creds: Credentials) {
        let session = Session::new(SessionConfig::default(), Some(self.cache.clone()));
        match session.connect(creds, true).await {
            Ok(()) => {
                let user = session.username();
                log::info!("signed in to Spotify as {user}");
                let tap = self.tap.clone();
                let player = Player::new(
                    PlayerConfig {
                        bitrate: Bitrate::Bitrate320,
                        normalisation: true,
                        position_update_interval: Some(Duration::from_secs(1)),
                        ..PlayerConfig::default()
                    },
                    session.clone(),
                    self.mixer.get_soft_volume(),
                    move || Box::new(sink::PulseSink::new(tap)),
                );
                spawn_player_events(player.get_player_event_channel(), self.inputs.clone());
                self.session = Some(session);
                self.player = Some(player);
                self.set_auth(AuthState::Ok, Some(user), None);
            }
            Err(e) => {
                log::warn!("Spotify sign-in failed: {e}");
                self.set_auth(AuthState::Needed, None, Some(format!("Spotify sign-in failed: {e}")));
            }
        }
    }

    /// The session drops now and then (network change, Spotify restarts an access point): reconnect with the
    /// saved credentials before the next load.
    async fn ensure_session(&mut self) -> bool {
        if self.session.as_ref().is_some_and(|s| !s.is_invalid()) && self.player.as_ref().is_some_and(|p| !p.is_invalid()) {
            return true;
        }
        self.session = None;
        self.player = None;
        if let Some(creds) = self.cache.credentials() {
            self.connect(creds).await;
        }
        self.player.is_some()
    }

    fn login(&mut self) {
        if self.auth.0 == AuthState::Pending {
            return;
        }
        self.set_auth(AuthState::Pending, None, Some("Sign in to Spotify in the browser window".into()));
        let inputs = self.inputs.clone();
        tokio::spawn(async move {
            let client_id = SessionConfig::default().client_id;
            let r = match OAuthClientBuilder::new(&client_id, OAUTH_REDIRECT, OAUTH_SCOPES.to_vec())
                .open_in_browser()
                .with_custom_message(OAUTH_DONE)
                .build()
            {
                Ok(c) => c.get_access_token_async().await.map(|t| t.access_token).map_err(|e| e.to_string()),
                Err(e) => Err(e.to_string()),
            };
            let _ = inputs.send(Input::LoginResult(r));
        });
    }

    fn flushed(&self) {
        self.generation.fetch_add(1, Ordering::Relaxed);
    }

    fn seek(&mut self, position_ms: u32) {
        if let Some(p) = &self.player {
            let max = self.duration_ms.map(|d| d.saturating_sub(1000)).unwrap_or(u32::MAX);
            let to = position_ms.min(max);
            self.flushed();
            p.seek(to);
            self.clock = Clock { position_ms: to, at: Instant::now(), playing: self.clock.playing };
        }
    }

    async fn command(&mut self, c: Command) {
        match c {
            Command::Login => self.login(),
            Command::Load { id, play, position_ms } => {
                let Some(uri) = track_uri(&id) else {
                    return self.emit(Event::Error { message: format!("not a Spotify track id: {id}") });
                };
                if !self.ensure_session().await {
                    return self.emit(Event::Error { message: "not signed in to Spotify".into() });
                }
                match SpotifyUri::from_uri(&uri) {
                    Ok(u) => {
                        self.flushed();
                        self.track = Some(bare_id(&uri));
                        self.duration_ms = None;
                        self.state = PlayState::Loading;
                        self.clock = Clock { position_ms, at: Instant::now(), playing: false };
                        if let Some(p) = &self.player {
                            p.load(u, play, position_ms);
                        }
                        self.emit_state();
                    }
                    Err(e) => self.emit(Event::Error { message: format!("{uri}: {e}") }),
                }
            }
            Command::Play => {
                if let Some(p) = &self.player {
                    p.play()
                }
            }
            Command::Pause => {
                if let Some(p) = &self.player {
                    p.pause()
                }
            }
            Command::Toggle => {
                if let Some(p) = &self.player {
                    if self.state == PlayState::Playing { p.pause() } else { p.play() }
                }
            }
            Command::Stop => {
                if let Some(p) = &self.player {
                    self.flushed();
                    p.stop()
                }
            }
            Command::Seek { position_ms } => self.seek(position_ms),
            Command::SeekBy { delta_ms } => {
                let to = (self.clock.now_ms() as i64 + delta_ms).max(0) as u32;
                self.seek(to);
            }
            Command::Volume { value } => {
                self.mixer.set_volume((value.clamp(0.0, 1.0) * u16::MAX as f64).round() as u16);
            }
            Command::Spectrum { enabled } => self.tap.enabled.store(enabled, Ordering::Relaxed),
            Command::Playlist { id } => {
                let Some(uri) = playlist_uri(&id) else {
                    return self.emit(Event::PlaylistError { id, message: "not a Spotify playlist id".into() });
                };
                if !self.ensure_session().await {
                    return self.emit(Event::PlaylistError { id: bare_id(&uri), message: "not signed in to Spotify".into() });
                }
                // off the command loop: playback keeps answering while hundreds of songs load
                let (session, out) = (self.session.clone().expect("signed in"), self.out.clone());
                tokio::spawn(async move { out.emit(&playlist::read(session, uri).await) });
            }
            Command::Quit => self.quit(),
        }
    }

    fn quit(&self) -> ! {
        if let Some(p) = &self.player {
            p.stop();
        }
        if let Some(s) = &self.session {
            s.shutdown();
        }
        std::process::exit(0);
    }

    fn mpris(&self, u: mpris::Update) {
        let _ = self.mpris.send(u);
    }

    fn player_event(&mut self, ev: PlayerEvent) {
        match ev {
            PlayerEvent::Loading { track_id, position_ms, .. } => {
                self.state = PlayState::Loading;
                self.track = Some(uri_id(&track_id));
                self.clock = Clock { position_ms, at: Instant::now(), playing: false };
                self.emit_state();
            }
            PlayerEvent::Playing { position_ms, .. } => {
                self.state = PlayState::Playing;
                self.clock = Clock { position_ms, at: Instant::now(), playing: true };
                self.emit_state();
                self.mpris(mpris::Update::Status(PlaybackStatus::Playing));
                self.mpris(mpris::Update::Position(position_ms));
            }
            PlayerEvent::Paused { position_ms, .. } => {
                self.flushed();
                self.state = PlayState::Paused;
                self.clock = Clock { position_ms, at: Instant::now(), playing: false };
                self.emit_state();
                self.mpris(mpris::Update::Status(PlaybackStatus::Paused));
                self.mpris(mpris::Update::Position(position_ms));
            }
            PlayerEvent::Stopped { .. } => {
                self.flushed();
                self.state = PlayState::Stopped;
                self.clock.playing = false;
                self.emit_state();
                self.mpris(mpris::Update::Status(PlaybackStatus::Stopped));
            }
            PlayerEvent::PositionChanged { track_id, position_ms, .. }
            | PlayerEvent::PositionCorrection { track_id, position_ms, .. } => {
                self.clock = Clock { position_ms, at: Instant::now(), playing: self.clock.playing };
                self.emit(Event::Position { id: uri_id(&track_id), position_ms });
                self.mpris(mpris::Update::Position(position_ms));
            }
            PlayerEvent::Seeked { track_id, position_ms, .. } => {
                self.clock = Clock { position_ms, at: Instant::now(), playing: self.clock.playing };
                self.emit(Event::Position { id: uri_id(&track_id), position_ms });
                self.mpris(mpris::Update::Seeked(position_ms));
            }
            PlayerEvent::EndOfTrack { track_id, .. } => {
                self.state = PlayState::Stopped;
                self.clock.playing = false;
                self.emit(Event::End { id: uri_id(&track_id) });
            }
            PlayerEvent::Unavailable { track_id, .. } => {
                self.state = PlayState::Stopped;
                self.emit(Event::Unavailable { id: uri_id(&track_id) });
            }
            PlayerEvent::TrackChanged { audio_item } => {
                self.duration_ms = Some(audio_item.duration_ms);
                self.emit_state();
                let (artists, album) = match &audio_item.unique_fields {
                    UniqueFields::Track { artists, album, .. } => (artists.iter().map(|a| a.name.clone()).collect(), album.clone()),
                    _ => (Vec::new(), String::new()),
                };
                let art_url = audio_item.covers.iter().max_by_key(|c| c.width).map(|c| c.url.clone());
                self.mpris(mpris::Update::Track {
                    id: bare_id(&audio_item.uri),
                    title: audio_item.name.clone(),
                    artists,
                    album,
                    art_url,
                    length_ms: audio_item.duration_ms,
                });
            }
            PlayerEvent::SessionDisconnected { .. } => {
                log::warn!("Spotify session disconnected; reconnecting on the next load");
            }
            _ => {}
        }
    }

    /// Media keys and the bar: transport acts here; next, previous and raise belong to needle.
    fn mpris_action(&mut self, a: mpris::Action) -> Option<Command> {
        use mpris::Action::*;
        let forward = |me: &Self, action: &'static str| {
            me.emit(Event::Mpris { action });
            None
        };
        match a {
            PlayPause => Some(Command::Toggle),
            Play => Some(Command::Play),
            Pause | Stop => Some(Command::Pause),
            SeekBy(ms) => Some(Command::SeekBy { delta_ms: ms }),
            SeekTo(ms) => Some(Command::Seek { position_ms: ms }),
            Next => forward(self, "next"),
            Previous => forward(self, "previous"),
            Raise => forward(self, "raise"),
        }
    }
}

fn spawn_player_events(mut ch: PlayerEventChannel, inputs: UnboundedSender<Input>) {
    tokio::spawn(async move {
        while let Some(ev) = ch.recv().await {
            if inputs.send(Input::Player(ev)).is_err() {
                break;
            }
        }
    });
}

fn main() {
    // librespot is chatty at info; needle shows the engine's own log
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("needle_engine=info,librespot=warn")).init();
    let args: Vec<String> = std::env::args().collect();
    let socket = match args.iter().position(|a| a == "--socket").and_then(|i| args.get(i + 1)) {
        Some(s) => PathBuf::from(s),
        None => {
            eprintln!("usage: needle-engine --socket PATH [--exit-on-disconnect]");
            std::process::exit(2);
        }
    };
    let exit_on_disconnect = args.iter().any(|a| a == "--exit-on-disconnect");
    let rt = tokio::runtime::Builder::new_multi_thread().enable_all().build().expect("tokio runtime");
    rt.block_on(run(socket, exit_on_disconnect));
}

fn state_dir() -> PathBuf {
    std::env::var_os("XDG_STATE_HOME")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(std::env::var_os("HOME").unwrap_or_default()).join(".local/state"))
        .join("needle")
}

fn private_dir(p: &Path) -> std::io::Result<()> {
    use std::os::unix::fs::PermissionsExt;
    std::fs::create_dir_all(p)?;
    std::fs::set_permissions(p, std::fs::Permissions::from_mode(0o700))
}

async fn run(socket: PathBuf, exit_on_disconnect: bool) {
    // Spotify's reusable credentials (a login secret): kept in a 0700 directory of their own
    let creds_dir = state_dir().join("librespot");
    if let Err(e) = private_dir(&creds_dir) {
        log::error!("{}: {e}", creds_dir.display());
        std::process::exit(1);
    }
    let cache = Cache::new(Some(&creds_dir), None, None, None).expect("credentials cache");
    let mixer = (mixer::find(None).expect("the soft mixer"))(MixerConfig::default()).expect("soft mixer");

    let out = Outbox::default();
    let (inputs, mut input_rx) = unbounded_channel::<Input>();
    let (tap_tx, tap_rx) = std_mpsc::sync_channel::<Vec<f32>>(64);
    let tap = sink::Tap { tx: tap_tx, enabled: Arc::new(AtomicBool::new(false)), latency_us: Arc::new(AtomicU64::new(0)) };
    let generation = Arc::new(AtomicU64::new(0));
    spawn_analyzer(tap_rx, tap.clone(), generation.clone(), out.clone());

    let (mpris_tx, mpris_rx) = unbounded_channel();
    let (mpris_act_tx, mut mpris_act_rx) = unbounded_channel();
    mpris::spawn(mpris_rx, mpris_act_tx);

    if let Some(dir) = socket.parent()
        && let Err(e) = private_dir(dir)
    {
        log::error!("{}: {e}", dir.display());
        std::process::exit(1);
    }
    let _ = std::fs::remove_file(&socket);
    let listener = match UnixListener::bind(&socket) {
        Ok(l) => l,
        Err(e) => {
            log::error!("cannot listen on {}: {e}", socket.display());
            std::process::exit(1);
        }
    };
    {
        use std::os::unix::fs::PermissionsExt;
        let _ = std::fs::set_permissions(&socket, std::fs::Permissions::from_mode(0o600));
    }
    log::info!("needle-engine {VERSION} listening on {}", socket.display());
    spawn_listener(listener, out.clone(), inputs.clone());

    let mut engine = Engine {
        out,
        cache,
        session: None,
        player: None,
        mixer,
        tap,
        generation,
        auth: (AuthState::Needed, None, None),
        state: PlayState::Stopped,
        track: None,
        duration_ms: None,
        clock: Clock { position_ms: 0, at: Instant::now(), playing: false },
        mpris: mpris_tx,
        inputs,
        exit_on_disconnect,
    };
    match engine.cache.credentials() {
        Some(creds) => engine.connect(creds).await,
        None => engine.set_auth(AuthState::Needed, None, None),
    }

    loop {
        tokio::select! {
            Some(i) = input_rx.recv() => match i {
                Input::Command(c) => engine.command(c).await,
                Input::Player(ev) => engine.player_event(ev),
                Input::Hello => {
                    engine.emit(Event::Ready { version: VERSION });
                    let (s, u, m) = engine.auth.clone();
                    engine.emit(Event::Auth { state: s, user: u, message: m });
                    engine.emit_state();
                }
                Input::Bye => {
                    if engine.exit_on_disconnect && !engine.out.connected() {
                        log::info!("needle disconnected; exiting");
                        engine.quit();
                    }
                }
                Input::LoginResult(Ok(token)) => engine.connect(Credentials::with_access_token(token)).await,
                Input::LoginResult(Err(e)) => {
                    engine.set_auth(AuthState::Error, None, Some(format!("Spotify sign-in failed: {e}")))
                }
            },
            Some(a) = mpris_act_rx.recv() => {
                if let Some(c) = engine.mpris_action(a) {
                    engine.command(c).await;
                }
            }
        }
    }
}

/// One needle at a time: a new connection replaces the old one.
fn spawn_listener(listener: UnixListener, out: Outbox, inputs: UnboundedSender<Input>) {
    tokio::spawn(async move {
        loop {
            let (stream, _) = match listener.accept().await {
                Ok(s) => s,
                Err(e) => {
                    log::warn!("accept: {e}");
                    continue;
                }
            };
            let (rd, mut wr) = stream.into_split();
            let (tx, mut rx) = unbounded_channel::<String>();
            *out.0.lock().unwrap() = Some(tx.clone());
            let _ = inputs.send(Input::Hello);
            tokio::spawn(async move {
                while let Some(line) = rx.recv().await {
                    if wr.write_all(line.as_bytes()).await.is_err() {
                        break;
                    }
                }
            });
            let inputs = inputs.clone();
            let out = out.clone();
            tokio::spawn(async move {
                let mut lines = BufReader::new(rd).lines();
                while let Ok(Some(line)) = lines.next_line().await {
                    if line.trim().is_empty() {
                        continue;
                    }
                    match serde_json::from_str::<Command>(&line) {
                        Ok(c) => {
                            let _ = inputs.send(Input::Command(c));
                        }
                        Err(e) => out.emit(&Event::Error { message: format!("bad command {line:?}: {e}") }),
                    }
                }
                // forget the sender only if it is still ours (a newer needle may have replaced it)
                {
                    let mut cur = out.0.lock().unwrap();
                    if cur.as_ref().is_some_and(|c| c.same_channel(&tx)) {
                        *cur = None;
                    }
                }
                let _ = inputs.send(Input::Bye);
            });
        }
    });
}

/// The spectrum thread: frames are held back by the sink's latency so the bars move with what you hear, and
/// frames of audio that was flushed (pause, seek, a new track) are dropped.
fn spawn_analyzer(rx: std_mpsc::Receiver<Vec<f32>>, tap: sink::Tap, generation: Arc<AtomicU64>, out: Outbox) {
    std::thread::Builder::new()
        .name("spectrum".into())
        .spawn(move || {
            let mut an = spectrum::Analyzer::new();
            let mut due: VecDeque<(Instant, u64, spectrum::Frame)> = VecDeque::new();
            let mut last_audio = Instant::now();
            let mut quiet = true;
            let tick = Duration::from_millis(1000 / spectrum::FPS as u64);
            let mut next_idle = Instant::now();
            loop {
                match rx.recv_timeout(tick / 2) {
                    Ok(chunk) => {
                        let now = Instant::now();
                        let lat = Duration::from_micros(tap.latency_us.load(Ordering::Relaxed));
                        let epoch = generation.load(Ordering::Relaxed);
                        for (after, f) in an.push_timed(&chunk) {
                            let ahead = Duration::from_secs_f32(after as f32 / spectrum::SAMPLE_RATE);
                            due.push_back(((now + lat).checked_sub(ahead).unwrap_or(now), epoch, f));
                        }
                        last_audio = now;
                        quiet = false;
                    }
                    Err(std_mpsc::RecvTimeoutError::Timeout) => {}
                    Err(std_mpsc::RecvTimeoutError::Disconnected) => break,
                }
                let now = Instant::now();
                let epoch = generation.load(Ordering::Relaxed);
                due.retain(|(_, g, _)| *g == epoch);
                while due.front().is_some_and(|(t, _, _)| *t <= now) {
                    let (_, _, f) = due.pop_front().unwrap();
                    if tap.enabled.load(Ordering::Relaxed) {
                        out.emit(&Event::Spectrum { bands: f.bands, peaks: f.peaks });
                    }
                }
                // no audio for a moment (paused, stopped): let the bars fall to rest, then go quiet
                if !quiet && due.is_empty() && now.duration_since(last_audio) > Duration::from_millis(250) && now >= next_idle {
                    next_idle = now + tick;
                    let f = an.idle();
                    quiet = f.bands.iter().chain(f.peaks.iter()).all(|&v| v == 0);
                    if tap.enabled.load(Ordering::Relaxed) {
                        out.emit(&Event::Spectrum { bands: f.bands, peaks: f.peaks });
                    }
                }
            }
        })
        .expect("spawn the spectrum thread");
}
