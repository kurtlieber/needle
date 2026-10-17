# needle phase 1: build plan

Phase 1 is Review: needle plays the two Review queues through its own librespot engine, shows the Concept A
screen (`mockups/a-calibrated.png`, compact column A of `mockups/compact.png`), and sends Verdicts and plays to
haystack, which makes every Spotify write. Phase 2 (library, playlists, search, bar widget, auto-skipping
Dislikes everywhere, a possible move of the API off the server) is out of scope; the seams it needs are noted.

Order of work: **haystack data and API → engine → app → packaging and host plumbing**. Each slice ships with
tests (`pytest` on haystack's side, `cargo test` for the engine, Qt Test for the app).

## 1. haystack: data

### Verdict storage (ADR 0005)

One append-only table keyed by Spotify track id, for any track, not just candidates:

```sql
verdicts(
  id TEXT PRIMARY KEY,          -- client-made UUID: an outbox replay is a no-op
  spotify_id TEXT NOT NULL,
  kind TEXT NOT NULL,           -- rating | pass | dislike
  stars INTEGER,                -- 1..5 for a rating, NULL otherwise
  given_at TEXT NOT NULL,       -- when it was given (client clock): orders the history
  received_at TEXT NOT NULL,
  source TEXT NOT NULL,         -- needle | cli | feedback (migrated)
  queue TEXT,                   -- discoveries | likes | NULL
  effects TEXT NOT NULL DEFAULT 'pending',  -- pending | done | skipped
  effects_at TEXT, effects_error TEXT)
```

The **current Verdict** of a track is its row with the latest `given_at` (ties: latest `received_at`). History
is never rewritten. The old `feedback` table (7 rows) is migrated once: `yes` → 3★, `no` mild/normal → Pass,
`no` strong → Dislike, `effects='skipped'` (those were applied by hand, To Rip included); the mild "stupid
song" becomes a normal negative (0.5 → 1.0). `feedback yes|no` keeps working as a thin alias of the new
`verdict` command.

### Verdict effects (ADR 0002), applied by haystack

Applied by a worker in the API service right after a verdict arrives, and retried by the hourly sync. Only the
*current* verdict of a track is applied, so a burst of re-ratings ends in one consistent state:

| Current verdict | Liked Songs | Discoveries | To Rip |
|---|---|---|---|
| 3–5★ | add the heart if missing | remove (bot-added only), close the offer `rated` | add if not in the library (bot add) |
| 1–2★ | remove the heart if present | remove, close `rated` | remove if the bot added it |
| Pass, Dislike | remove the heart if present | remove, close `passed` / `disliked` | remove if the bot added it |

Hearts are only ever written when a verdict is given (never to fight another client). Hearts use
`PUT`/`DELETE /me/library?uris=` (2026 dev mode), which need the scope **`user-library-modify`**: a refresh
token issued before it was added needs one more `haystack auth`. Until then heart writes stay
`pending` with `effects_error` naming the missing scope, and everything else (playlists, To Rip) is applied.
The candidate's state follows the verdict (`liked` for a Rating, `negative` for Pass/Dislike), so it is never
offered again.

### Weights (configurable, `[scoring.verdicts]`)

Discovery scores a candidate by its similarity to positives plus each positive's additive bonus. With
`bonus_discovery_like = 0.005` (a liked discovery, which is a 3★-equivalent heart under ADR 0002) as the
anchor, a Rating's bonus is linear in stars and the 3★ point equals today's discovery like:

| Verdict | Role | Default |
|---|---|---|
| 1★ | positive | bonus 0.000 |
| 2★ | positive | bonus 0.0025 |
| 3★ | positive | bonus 0.005 (= `bonus_discovery_like`) |
| 4★ | positive | bonus 0.0075 |
| 5★ | positive | bonus 0.010 |
| Pass | negative | strength 1.0 (today's `normal`) |
| Dislike | negative | strength 2.0 (today's `strong`) |

An Unrated like counts as 3★ (ADR 0002), so it gets the 3★ bonus (`unrated_like_stars = 3`); that replaces
`bonus_like` (0) and equals what a liked discovery already got. Library tracks the listener hasn't liked keep
`bonus_library` (0). Every variant adds the bonus before it takes the nearest / top-k (the genre variants now
do too). A track whose current verdict is Pass or Dislike is never a positive, even while its heart is still
waiting to be removed.

### Traits and Readings (ADR 0001)

`readings(deezer_id PK, tempo, tempo_confidence, valence, energy, danceability, acousticness,
instrumentalness, loudness, models, measured_at)`, measured from the 30 s Deezer preview in the same pass
that makes the embeddings:

| Trait | Model / algorithm | Reading |
|---|---|---|
| danceability | `danceability-discogs-effnet-1` on the `discogs-effnet-bs64-1` embeddings (the pass the styles already run) | P(danceable), 0–1 |
| acousticness | `mood_acoustic-discogs-effnet-1` | P(acoustic), 0–1 |
| instrumentalness | `voice_instrumental-discogs-effnet-1` | P(instrumental), 0–1 |
| valence, energy | `deam-msd-musicnn-2` on `msd-musicnn-1` embeddings (no effnet head exists) | DEAM 1–9 scale mapped to 0–1; energy = arousal |
| tempo | Essentia `RhythmExtractor2013` (multifeature) at 44.1 kHz | BPM + confidence |
| loudness | Essentia `LoudnessEBUR128`, stereo 44.1 kHz | integrated LUFS |

Model files are checked against essentia.upf.edu/models (2026-10-11): the three classifier heads take
`model/Placeholder` [1280] → `model/Softmax`; DEAM takes `model/Placeholder` [200] → `model/Identity`
(valence, arousal); `msd-musicnn-1` → `model/dense/BiasAdd` [200]. `haystack models fetch`
downloads the missing ones into `models/`.

Coverage: new candidates are measured in the harvest; `haystack traits backfill` (and a capped
daily step in the harvest) measures stored previews, active offers and liked songs first, then the pool by
score. A track haystack has never seen is measured **on demand**: Spotify `GET /tracks/{id}` → ISRC → Deezer
(ISRC, then search with the duration/version check) → preview → Readings; the mapping is kept in
`spotify_deezer`. No preview means no Readings (about 8% of liked songs).

### Baseline

Per Trait, over the tracks the listener likes (active Liked Songs plus 1–2★ Ratings; Pass and Dislike excluded):
the weighted mean (weight = stars, an Unrated like = 3) and the weighted 25th–75th percentile, the **usual
range** that drives the summary sentence. Concept A shows the mean only.

### Track metadata

`spotify_tracks` gains `album_type`, `release_date`, `image_url`, `image_url_small` (from the track objects
the sync already fetches; a full Liked Songs resync fills them for old likes; `GET /tracks/{id}` for the rest).
needle loads covers straight from Spotify's image CDN and caches them.

## 2. haystack: API (ADR 0006)

A stdlib `ThreadingHTTPServer` (no new Python dependencies) run as the user unit `haystack-api.service`
on `0.0.0.0:8517`. The endpoints, payloads and versioning rules are the server's contract, in
[`docs/api.md`](https://github.com/kurtlieber/haystack/blob/main/docs/api.md); needle builds against that document only. Sources are limited to loopback and `api.allow_networks` (in code,
and by the host firewall); the bearer token is the `api-token` secret (`haystack token new`).

## 3. engine (`engine/`, Rust)

`needle-engine --socket $XDG_RUNTIME_DIR/needle/engine.sock`, built on the librespot 0.8 crates, started and
owned by the app. Newline-delimited JSON over the Unix socket, one client.

Commands: `login`, `load {id, play, position_ms}`, `play`, `pause`, `toggle`, `seek {position_ms}`,
`seek_by {delta_ms}`, `stop`, `volume {value}`, `spectrum {enabled}`, `metadata {title, artist, album,
cover}` (for MPRIS), `quit`.
Events: `ready`, `auth {state: needed|pending|ok|error, user?, message?}`, `state {state, id, position_ms,
duration_ms}`, `position {position_ms}` (1 Hz plus on every change), `end {id}`, `unavailable {id}`,
`spectrum {bands: [56 × 0..255], peaks: [...]}` at 30 Hz while enabled, `mpris {action: next|previous|
raise}`, `error {message}`.

- Login: librespot's own OAuth flow (`librespot-oauth`, the browser on loopback), then reusable credentials
  in `$XDG_STATE_HOME/needle/librespot/` (0700). Premium is required.
- Audio: librespot's PulseAudio sink (PipeWire on Omarchy) wrapped in a tap sink that copies PCM to the
  analyser: 2048-point real FFT, 56 log-spaced bands 20 Hz–20 kHz, dB-scaled, attack/decay smoothing, peak
  hold.
- MPRIS: `org.mpris.MediaPlayer2.needle` (play/pause/seek directly; next/previous are forwarded to the app,
  which owns the queue), so media keys and the Omarchy bar work.

## 4. app (`app/`, Qt Quick / C++)

Per the `omarchy-app` pattern (qmake6, one binary, live theme, Material style, `Shortcut`s), with the
whole Omarchy palette exposed (not only `accent`), followed live. Archivo (variable, OFL) is bundled in the
qrc; `wdth` 125 for the wordmark and BPM, 112 for the title.

- `HaystackClient`: the servers in `~/.config/needle/config.toml`, in order (the next one when one doesn't
  answer), and the token from its `token_file` (0600); `NEEDLE_SERVER` and `NEEDLE_TOKEN_FILE` override.
  Caches `/v1/state` and covers under `~/.cache/needle/`; the outbox (`~/.local/state/needle/outbox.json`,
  atomic writes) holds verdicts and plays until haystack accepts them, and verdicts apply to the cached
  queues at once, so Review works offline.
- `EngineClient`: starts `needle-engine`, speaks the socket protocol, restarts it if it dies.
- `Review`: the two queues, current track, verdict keys (Pass/Dislike advance; a plain Rating keeps playing;
  Shift+digit rates and advances), plays reporting (a play is reported when a track is left after ≥ 5 s or
  ends), the summary sentence and tempo comparison.
- Layout: the wide Concept A screen at ≥ 1100 px, the compact column below. Backdrop glow (the blurred
  cover) is a toggle (`G`). Reduced motion (system setting or `M`) stops the beat pulse and slows the
  spectrum to a still average.

Keys: `1`–`5` rate, `Shift`+digit rate and next, `P` Pass, `X` Dislike, `Space` play/pause, `←`/`→` seek
10 s, `J` jump 30 s, `N` next, `B` back, `Tab` switch queue, `G` glow, `M` motion, `?` shortcuts, `Q` quit.

## 5. Packaging and host plumbing

- `app/pkgbuild/PKGBUILD` builds both parts (and runs both test suites in `check()`): `/usr/bin/needle`,
  `/usr/lib/needle/needle-engine`, `needle.desktop`, the icon, the MIT licence and Archivo's OFL.
  `app/bin/install` runs `makepkg -fsi`.
- The server enables `haystack-api.service` and its firewall allows 8517 from the client networks only.
- The desktop's `token_file` (0600) holds the server's `api-token` (`haystack token show`).

## Sign-ins

1. A refresh token issued before `user-library-modify` was added needs `haystack auth` once more (hearts):
   `haystack auth` on the server, through the SSH tunnel it prints, then approve in the browser.
2. First needle start: log in to Spotify in the browser window librespot opens (Premium account).

## As built (2026-10-11)

- Tempo: Deezer's own bpm when it is the same tempo as the measurement at some metrical level (ADR 0001 note).
- The engine has its own PulseAudio sink (about 120 ms of buffer, flushed on pause): librespot's asks for the
  server default (about 2 s), which would delay every pause and put the spectrum 2 s ahead of the music. Spectrum
  frames are released after the sink's measured latency.
- The API worker puts Spotify writes ahead of on-demand measuring, and `/v1/state` asks it to measure only the
  next 20 tracks of each queue that have no Readings; the harvest's daily backfill does the rest.
- `api.spotify_writes = false` (dev only) records verdicts without touching Spotify.

## Playlist review (2026-10-16)

The listener reviews any of their Spotify playlists, not only Discoveries and their likes. The picker is the cover wall
(`mockups/playlists-a-wall.png`, option A of four): every playlist as a cover with its progress, a details card on
the one in focus, and a dock that adds up the pick and starts the review.

- **haystack, data.** `playlists` (one row per playlist in the library, the listener's own and Followed playlists,
  minus Discoveries and To Rip: name, owner, cover, Spotify's song count, `snapshot_id`, its place in the library) and
  `playlist_tracks` (its songs in order). The hourly sync and the API worker (at start, then on a snapshot read at
  most every 10 minutes) list `/me/playlists` and re-read the songs of a playlist only when its `snapshot_id`
  changed: one call when nothing did. The Web API lists the songs of the listener's own playlists, with full track objects,
  so their cards need no more calls. For a Followed playlist it answers 403 (dev mode): haystack notes the refused
  version and leaves the playlist to needle's engine (ADR 0008).
- **haystack, API.** `/v1/state` gains `playlists` (every playlist with its songs' ids and the current verdicts of
  those songs, about 130 KB for 31 playlists, so needle counts progress for any pick offline); `GET
  /v1/playlists/{id}` returns a playlist with a full card per song; `PUT /v1/playlists/{id}/tracks` takes the songs
  needle's engine read. Shapes: [`docs/api.md`](https://github.com/kurtlieber/haystack/blob/main/docs/api.md). Verdicts carry `queue: "playlists"`; their Spotify
  writes are the usual ones (ADR 0002). Playlists themselves are never written.
- **engine.** `playlist {id}` reads the songs through librespot with their metadata (`playlist {id, name,
  snapshot_id, tracks}` or `playlist_error {id, message}`), one playlist at a time, five songs a second.
- **app.** `Playlists` (the picker: the model, the pick, filter, sort, name filter, the engine reads, the saved
  review in `~/.local/state/needle/playlist-review.json`), `PlaylistLibrary` and `PlaylistSession` (pure: progress,
  what a pick adds up to, the frozen play order), and a third `ReviewQueue` in `Review`. Start fetches each picked
  playlist's cards (saved under `~/.cache/needle/playlists/`, so a started review works offline), puts their songs
  together, each once, drops the ones with a verdict unless "Skip songs with a verdict" is off, and shuffles by a
  hash of each song and a seed, so the order survives a restart and a fresh snapshot. With the skip off the review
  plays every song and counts one as reviewed once it gets a verdict in this review. The cards of a Playlist review
  aren't in the snapshot, so needle asks haystack for the song on screen and the next three (`GET /v1/tracks/{id}`),
  which starts measuring them, and asks again while one is still being measured.
- **Keys.** `L` opens the picker (and closes it over a running review). In the picker: arrows move, `Space` picks
  or unpicks, `A` picks every playlist shown (or none, when all are), `Enter` starts (with none picked: the one in
  focus alone), `/` filters by name, `F` cycles All, Not started, In progress and Done, `O` the sort (Spotify order,
  Most to review, Name), `V` and `S` the two switches, `Esc` goes back to the review. `Tab` goes round all three
  queues.

As built: a song Spotify can't play here moves on without starting playback when nothing was playing (it used to
start the next song). An engine that restarts mid-read has the read started again.
