# Setting up needle and haystack: instructions for an AI agent

You are helping a person install needle, a desktop app for rating Spotify songs, and connect it to haystack, the
self-hosted server it works with. Read the [README](README.md) first for what needle does; this file is the
procedure. It has only been tested with Claude Code, but any agent that can run shell commands should be able to
follow it.

needle runs on the person's desktop. haystack runs on an always-on machine (a home server, a NAS, a VPS), which may
be the same computer. Setting up haystack has its own procedure, which this file sends you to; don't repeat it here.

If you are here to work on needle's code instead, skip to [Working on the code](#working-on-the-code).

## Ground rules

- **Secrets stay out of the conversation.** Never ask the person to paste haystack's API token, a Spotify client
  secret or a password into the chat, and never print one. Move the API token straight into needle's token file,
  as in step 4.
- **Sign-ins are the person's.** You can't open their browser or type their password. The Spotify sign-in in
  needle, `sudo` prompts and haystack's own steps are theirs.
- **Batch the person's steps.** Give every step only they can do as one numbered list, at the point where you
  need it, with the exact commands. Don't drip them out one at a time.
- **Confirm before `sudo`** and before changing anything outside `~/.config/needle/` on the desktop.
- **Supported means Arch Linux or Omarchy on Wayland.** Elsewhere, say plainly that it's best effort.

## 1. Check the desktop

```sh
uname -m                                         # must be x86_64
cat /etc/os-release                              # Arch, Omarchy, Debian, Ubuntu, Fedora...
echo "$XDG_SESSION_TYPE"                         # wayland; x11 is unsupported
test -d ~/.config/omarchy && echo omarchy
command -v needle && pacman -Qi needle 2>/dev/null | grep Version
```

If `uname -m` isn't `x86_64`, stop: needle isn't built for anything else. On X11, or a desktop other than
Omarchy, tell the person it is unsupported and may not work, and go on only if they want to.

## 2. Ask the person

Ask these together, with the defaults filled in:

1. **Do they have haystack running already?** If so: its address (host or IP, port 8517 by default) and how you
   reach it over SSH. If not: which machine it will run on (default: this one).
2. **Spotify Premium?** needle needs it to play. Without Premium, stop and say so.
3. **The network the desktop reaches haystack from.** The home LAN range (`ip -4 route` shows it), or a VPN such
   as Tailscale's `100.64.0.0/10`. haystack only answers networks it allows.

## 3. Install needle

**Arch Linux or Omarchy.** Run the installer in the person's terminal (it uses `sudo`, and pacman asks before
upgrading):

```sh
curl -fsSL https://raw.githubusercontent.com/kurtlieber/needle/main/install.sh | bash
```

It trusts needle's signing key (`E5278B2148A2833AB898C69FC5A7379D20ECB792`), writes `/etc/pacman.d/needle.conf`,
adds an `Include` line to `/etc/pacman.conf`, on Omarchy adds the hook
`~/.config/omarchy/hooks/pre-refresh-pacman.d/needle-repo` (Omarchy rewrites pacman.conf, and the hook puts the
line back), then runs `pacman -Syu needle`. If you can't give it a terminal, do the same steps one at a time from
[docs/reference.md, "The pacman repository"](docs/reference.md#the-pacman-repository).

**Debian 13+, Ubuntu 25.04+ or Fedora (best effort).** Download the `.deb` or `.rpm` and `SHA256SUMS` from the
latest release, check the sum, and install it:

```sh
cd "$(mktemp -d)"
url=https://github.com/kurtlieber/needle/releases/latest/download
curl -fsSLO "$url/SHA256SUMS"
pkg="$(awk '/\.deb$/ {print $2}' SHA256SUMS)"   # or /\.rpm$/ on Fedora
curl -fsSLO "$url/$pkg"
sha256sum --ignore-missing -c SHA256SUMS
sudo apt install "./$pkg"                       # or: sudo dnf install "./$pkg"
```

Check: `command -v needle` and `test -x /usr/lib/needle/needle-engine`.

## 4. Set up haystack

**If haystack isn't running yet,** follow
[haystack's AGENTS.md](https://github.com/kurtlieber/haystack/blob/main/AGENTS.md) from the start, on the machine it
will run on. When it asks about needle (its step 2, question 6), the answer is yes, with the network from step 2
above. Then come back here.

**If it is running,** check from the desktop that it answers:

```sh
curl -s -o /dev/null -w '%{http_code}\n' http://<server>:8517/v1/health
```

`401` (no token yet) means it answers and allows this network: go on. `403` means this network isn't in haystack's
`api.allow_networks`. No answer: see [Troubleshooting](#troubleshooting).

## 5. Connect needle

On the desktop:

```sh
mkdir -p ~/.config/needle
(umask 077; ssh <server> 'cd ~/haystack && docker compose exec -T haystack haystack token show' > ~/.config/needle/token)
test -s ~/.config/needle/token && echo token saved   # don't print the token
```

If haystack runs on this same machine, drop the `ssh <server>` and run the `docker compose` command in its folder.
If it runs without Docker, the command is `haystack token show`.

Then write `~/.config/needle/config.toml` (if it exists, change only `servers`):

```toml
[haystack]
servers = ["http://<server address>:8517"]
```

A second address (say, the server's Tailscale IP) can follow the first; needle tries them in order.

## 6. The person's steps

Give them these together:

1. Start needle from the app launcher (or `needle` in a terminal).
2. When it shows "Sign in to Spotify to play", press `Enter` and sign in in the browser window, with the Premium
   account. This happens once.
3. Rate a song (`1` to `5`) to see it work. `?` shows every key.

## 7. Check it works

- The top right of needle's window says "Synced with haystack", not "Offline".
- Discoveries shows songs. It is empty until haystack's first weekly batch; Rate your likes should already have
  songs once haystack has read the person's Liked Songs.
- With the token, haystack answers from the desktop (`printf` keeps the token off the command line):

  ```sh
  printf 'Authorization: Bearer %s\n' "$(cat ~/.config/needle/token)" |
    curl -s -H @- http://<server>:8517/v1/health   # {"ok": true, ...}
  ```

## Troubleshooting

| Symptom | Cause and fix |
|---|---|
| needle says Offline | Wrong address in `servers`, or the server's firewall blocks 8517. `curl http://<server>:8517/v1/health` from the desktop should give 401 without a token. |
| `403` from haystack | The desktop's network isn't in haystack's `api.allow_networks`: add it in haystack's `config.toml`, then `docker compose restart`. |
| `401` with the token | The token file is wrong or empty: copy it again (step 5). |
| "needle's API is off" in haystack's logs | haystack has no API token: `docker compose run --rm haystack token new`, `docker compose restart`, and copy it again. |
| `pacman` says "required key missing from keyring" | The key isn't trusted: run `install.sh` again, or the key steps in docs/reference.md. |
| `pacman -Syu` can't find `needle` after an Omarchy update | The `Include` line was dropped: run `install.sh` again, which also adds the hook. |
| No sound, or Spotify sign-in fails | Spotify Premium is required. The sign-in lives in `~/.local/state/needle/librespot/`; deleting that folder asks again. |
| needle doesn't start on X11 | Unsupported: needle is Wayland only. |

Run `needle` from a terminal to see its log. Every setting, file and key: [docs/reference.md](docs/reference.md).

## Working on the code

- Build, test and run: `app/bin/build`, `app/bin/test`, `app/build/needle`. Install from the checkout with
  `app/bin/install`. Details in [docs/reference.md, "Develop"](docs/reference.md#develop).
- Two programs: `app/` (C++ and Qt Quick, the window) and `engine/` (Rust and librespot, the player), talking over
  a Unix socket. needle knows haystack only through its HTTP API
  ([haystack's `docs/api.md`](https://github.com/kurtlieber/haystack/blob/main/docs/api.md)); haystack makes every
  Spotify write.
- Terms: [GLOSSARY.md](GLOSSARY.md). Decisions: [docs/adr/](docs/adr/).
- Use conventional commit messages (`feat:`, `fix:`): release-please builds the changelog and the version from
  them.
- Never commit a token, a `config.toml`, or anything from `~/.local/state/needle/`.
- **The docs change with the code, in the same PR.** A change to what needle shows or does, a key, a setting or how
  it installs updates every doc that describes it: [README.md](README.md), [docs/reference.md](docs/reference.md),
  this file's setup steps, [GLOSSARY.md](GLOSSARY.md) for a new term, and an ADR for a decision. ADR numbers are
  shared with haystack: take the next number that is free in both repositories.
- **Every release checks the docs.** Before the release PR is merged, read the docs above against everything it
  collects (`git log --oneline v<last>..main`). Fix any line the release makes untrue, including older text that
  the new behaviour contradicts, in a `docs:` PR, and merge that first so the tag carries docs that match.
