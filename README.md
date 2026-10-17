# needle

A desktop app for Review: it plays the Review queues (Discoveries, Rate your likes, and the playlists you pick
from your Spotify library) through its own librespot engine, shows each track's Readings against your Baseline
(Concept A, "Calibrated", `docs/mockups/`), and sends your Verdicts to haystack, the server, which
makes every Spotify write. needle knows haystack only through its HTTP API
([haystack's `docs/api.md`](https://github.com/kurtlieber/haystack/blob/main/docs/api.md)). Terms: `GLOSSARY.md`. Design:
`docs/plan.md`; needle's ADRs (0003, 0007, 0008) are in `docs/adr/`. The other numbers (0001, 0002,
0004-0006) are haystack's, in [its `docs/adr/`](https://github.com/kurtlieber/haystack/tree/main/docs/adr).

Two programs, one package: `needle` (`app/`, C++ and Qt Quick, the `omarchy-app` pattern) and
`needle-engine` (`engine/`, Rust and the librespot crates), started by needle and spoken to over a Unix
socket. Nothing on the desktop needs Python.

## Install

A released package (ADR 0007): from the GitHub Release, or from the repo's Packages panel
(`ghcr.io/kurtlieber/needle`). The repo is private, so both need a GitHub login.

```sh
gh release download -R kurtlieber/needle --pattern 'needle-*.pkg.tar.zst*' --pattern needle-signing-key.asc
# or: oras pull ghcr.io/kurtlieber/needle:latest   (after `gh auth token | oras login ghcr.io -u kurtlieber --password-stdin`)
gpg --import needle-signing-key.asc && gpg --verify needle-*.pkg.tar.zst.sig
rm needle-*.pkg.tar.zst.sig
sudo pacman -U needle-*.pkg.tar.zst
```

`gpg --verify` must print the fingerprint `E527 8B21 48A2 833A B898  C69F C5A7 379D 20EC B792`. Remove the
`.sig` before installing: pacman checks a `.sig` it finds next to the package against its own keyring, which doesn't
have this key, and stops with "required key missing from keyring".

From this checkout instead:

```sh
app/bin/install   # makepkg -fsi: builds both, runs the tests, installs; needle shows up in the launcher
```

## Release

Merge the release PR that release-please keeps open ("chore(main): release needle X.Y.Z"). It collects the
`feat:` and `fix:` commits, so use conventional commit messages. The merge tags `vX.Y.Z`; `needle-release` builds, signs and publishes the package to the Release and to GHCR. To
rebuild a release, run `needle-release` by hand with its tag. The version is `VERSION`, which
release-please bumps; don't edit it by hand.

First run, once per computer:

1. **Where haystack is.** `~/.config/needle/config.toml`, which needle reads at start and never writes:
   ```toml
   [haystack]
   servers = ["http://192.168.1.10:8517", "http://10.20.0.10:8517"]   # in order: the next one when one doesn't answer
   token_file = "~/.config/needle/token"                               # the default
   ```
   Without the file needle tries `http://127.0.0.1:8517`. `NEEDLE_SERVER=url[,url]` and `NEEDLE_TOKEN_FILE=path`
   override it. A file needle can't read is ignored with a warning on stderr.
2. **haystack's token.** The API's bearer token, one line, in the token file (0600):
   ```sh
   umask 077; mkdir -p ~/.config/needle
   cat > ~/.config/needle/token   # paste it, then Ctrl-D
   ```
   needle picks the file up without a restart.
3. **Spotify.** needle shows "Sign in to Spotify to play": press Enter, sign in in the browser window
   (Premium account). librespot keeps reusable credentials in `~/.local/state/needle/librespot/` (0700), so this
   is a one-time step. Verdicts work before it.

## Keys

| Key | Does |
|---|---|
| `1`–`5` | Rate (the track keeps playing) |
| `Shift` + `1`–`5` | Rate and play the next track |
| `P` | Pass (moves on) |
| `X` | Dislike (moves on) |
| `Space` | Play or pause |
| `←` / `→` | Back or ahead 10 s |
| `J` | Jump ahead 30 s |
| `N` | Next track |
| `B` | Back to the previous track (to change its verdict) |
| `Tab` | Next queue: Discoveries, Rate your likes, Playlists |
| `L` | Choose playlists to review (again: back to the review) |
| `G` | Cover glow on or off |
| `M` | Motion: follow the desktop's animation setting, or override it |
| `?` | All shortcuts |
| `Q` | Quit |
| `Enter` | Sign in to Spotify (when asked) |

Choosing playlists (the Playlists tab, or `L`):

| Key | Does |
|---|---|
| Arrows | Move between playlists |
| `Space` | Add the playlist in focus to the review, or take it out |
| `A` | Pick every playlist shown (again: none) |
| `Enter` | Start the review (with none picked: the playlist in focus alone) |
| `/` | Filter by name (`Enter` or `Esc` goes back to the covers) |
| `F` | Show all, Not started, In progress or Done |
| `O` | Sort: Spotify order, Most to review, Name |
| `V` | Skip songs with a verdict, or play them again |
| `S` | Shuffle on or off |
| `Esc` | Back to the review |

Media keys and the Omarchy bar work through MPRIS (`org.mpris.MediaPlayer2.needle`).

## How it behaves

* **Offline.** needle caches haystack's last snapshot (`~/.cache/needle/state.json`) and the covers of the next
  tracks (`~/.cache/needle/covers/`), and keeps an outbox of verdicts and plays
  (`~/.local/state/needle/outbox.json`) until haystack accepts them. The status line says how many wait.
* **Servers.** The ones in `config.toml`, in order: when one doesn't answer needle tries the next, once round,
  then shows Offline and tries again every 30 s.
* **Plays.** A track left after 5 s or more, or played to the end, is reported to haystack (it marks the
  discovery as heard).
* **Playlists.** The Playlists tab shows every playlist in your Spotify library but haystack's own Discoveries and
  To Rip, with how far each is reviewed. A review of the ones you pick plays their songs, each once, and survives a
  restart (`~/.local/state/needle/playlist-review.json`; the cards are saved in `~/.cache/needle/playlists/`).
  Spotify won't list a Followed playlist for haystack, so needle's engine reads its songs (one playlist at a time,
  while needle runs and is signed in to Spotify) and sends them to haystack (ADR 0008).
* **Theme.** Every colour is an Omarchy theme token, followed live (`~/.local/state/omarchy/current/theme`).
  The cover glow is the cover's own pixels, blurred.
* **Reduced motion.** The desktop's `enable-animations` (settings portal), or `M`: the beat pulse stands still
  and the spectrum shows a slowly settling average.
* **Layout.** The wide layout from about 1100 x 860; the compact column below that.

## Develop

```sh
app/bin/build     # cargo build --release (engine) + qmake6/make (app) into app/build/
app/bin/test      # Qt tests offscreen, then the engine's cargo tests
app/build/needle  # finds app/build/needle-engine next to it (or NEEDLE_ENGINE=path)
```

Development flags: `--grab FILE` renders the window to a file after `--delay` ms and quits, `--size WxH`, and
`--press 4,P,Shift+5` sends keys 3 s after start. `NEEDLE_TOKEN_FILE` points at another token file.
