# needle reference

The details the [README](../README.md) leaves out: how needle is built, every setting and key, how it behaves,
and how it is developed and released. Terms: [GLOSSARY.md](../GLOSSARY.md). Decisions: [docs/adr/](adr/).

## How it fits together

needle is the desktop app for Review: it plays the Review queues (Discoveries, Rate your likes, and the playlists
you pick from your Spotify library) through its own librespot engine, shows each track's Readings against your
Baseline, and sends your Verdicts to haystack, the server, which makes every Spotify write. needle knows haystack
only through its HTTP API ([haystack's `docs/api.md`](https://github.com/kurtlieber/haystack/blob/main/docs/api.md)).

Two programs, one package: `needle` (`app/`, C++ and Qt Quick) and `needle-engine` (`engine/`, Rust and the
librespot crates), started by needle and spoken to over a Unix socket. Nothing on the desktop needs Python.

needle's ADRs (0003, 0007, 0008) are in [docs/adr/](adr/). The other numbers (0001, 0002, 0004-0006) are
haystack's, in [its `docs/adr/`](https://github.com/kurtlieber/haystack/tree/main/docs/adr). The Review screen
was built from Concept A, "Calibrated", in [docs/mockups/](mockups/).

## The pacman repository

Each release on GitHub carries a one-package pacman repository: `needle.db` and `needle.files`, both signed, next
to the signed package (ADR 0007). The README's `install.sh` sets it up. By hand, as root:

```sh
curl -fsSLO https://github.com/kurtlieber/needle/releases/latest/download/needle-signing-key.asc
gpg --show-keys needle-signing-key.asc   # must show E527 8B21 48A2 833A B898  C69F C5A7 379D 20EC B792
pacman-key --add needle-signing-key.asc
pacman-key --lsign-key E5278B2148A2833AB898C69FC5A7379D20ECB792

printf '[needle]\nSigLevel = Required\nServer = https://github.com/kurtlieber/needle/releases/latest/download\n' \
  > /etc/pacman.d/needle.conf
printf '\nInclude = /etc/pacman.d/needle.conf\n' >> /etc/pacman.conf
pacman -Syu needle
```

`SigLevel = Required` makes pacman refuse the package and the database unless needle's key signed them. The local
signature (`--lsign-key`) is what tells pacman to trust that key, as for any third-party repository.

**Omarchy** rewrites `/etc/pacman.conf` when it refreshes its package configuration (`omarchy refresh pacman`, a
channel change), which drops the `Include` line. `install.sh` adds a hook,
`~/.config/omarchy/hooks/pre-refresh-pacman.d/needle-repo`, that puts it back each time.

**Uninstall:**

```sh
sudo pacman -Rns needle
sudo sed -i '\|^Include = /etc/pacman.d/needle.conf$|d' /etc/pacman.conf
sudo rm /etc/pacman.d/needle.conf
rm -f ~/.config/omarchy/hooks/pre-refresh-pacman.d/needle-repo
sudo pacman-key --delete E5278B2148A2833AB898C69FC5A7379D20ECB792
```

needle's own files stay: `~/.config/needle/`, `~/.cache/needle/` and `~/.local/state/needle/` (the last holds the
Spotify sign-in and any verdicts not yet sent).

**A single package** without the repository: download `needle-X.Y.Z-1-x86_64.pkg.tar.zst` and its `.sig` from
a release, trust the key as above, and `sudo pacman -U needle-*.pkg.tar.zst`. pacman checks the `.sig` against
its keyring. It doesn't update by itself.

## The .deb and .rpm

Built on each release, best effort (README, "Other distributions"): `app/dist/build-deb` in `debian:stable`
and `app/dist/build-rpm` in `fedora:latest`. CI installs each in a fresh container of its distribution and
starts needle once (`app/dist/smoke-test`); nothing is tested beyond that. They aren't signed: check them
against the release's `SHA256SUMS`, which lists every file the release carries.

## Settings

`~/.config/needle/config.toml`, which needle reads at start and never writes:

```toml
[haystack]
servers = ["http://192.168.1.10:8517", "http://10.20.0.10:8517"]   # in order: the next one when one doesn't answer
token_file = "~/.config/needle/token"                               # the default
```

Without the file needle tries `http://127.0.0.1:8517`. `NEEDLE_SERVER=url[,url]` and `NEEDLE_TOKEN_FILE=path`
override it. A file needle can't read is ignored with a warning on stderr.

The token file holds haystack's API bearer token on one line, mode 0600. needle picks it up without a restart.

The Spotify sign-in: librespot keeps reusable credentials in `~/.local/state/needle/librespot/` (0700), so it is
a one-time step. Verdicts work before it.

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
* **Why a Discovery.** Under the title, the wide layout says which of your tracks it sounds like and where
  haystack found it. When haystack's re-ranker picked it, the line adds the re-ranker's strongest reason
  ("Learned from your Verdicts: close to your 4-5★ tracks") and an outlined chip before the styles gives its
  Confidence ("Confidence 81%"). Each needs its feature in haystack's `server.features` (`why.reasons`, `rerank`,
  haystack 0.3.0 and later); against an older haystack the line is as before and there is no chip (ADR 0014).
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
app/bin/install   # makepkg -fsi from this checkout: builds both, runs the tests, installs
```

Development flags: `--grab FILE` renders the window to a file after `--delay` ms and quits, `--size WxH`, and
`--press 4,P,Shift+5` sends keys 3 s after start. `NEEDLE_TOKEN_FILE` points at another token file.

## Release

Merge the release PR that release-please keeps open ("chore(main): release needle X.Y.Z"). It collects the `feat:`
and `fix:` commits, so use conventional commit messages. Check the docs against them first
([AGENTS.md](../AGENTS.md#working-on-the-code) lists them) and merge any fix before the release PR. The merge tags
`vX.Y.Z` and opens the release as a draft. `needle-release` builds, signs and uploads the package and the pacman
repository, then publishes the release, so "latest" always has a complete repository. The .deb and .rpm follow; if
one of them fails, the release goes out without it. To rebuild a release, run `needle-release` by hand with its
tag. The version is `VERSION`, which release-please bumps; don't edit it by hand.
