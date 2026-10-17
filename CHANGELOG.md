# Changelog

## 0.3.0 (2026-10-17)


### ⚠ BREAKING CHANGES

* **needle:** needle no longer has built-in server addresses or the variable that picked one. Without a config.toml it tries http://127.0.0.1:8517 with the token in ~/.config/needle/token.

### Features

* **needle:** talk to haystack through config.toml, not built-in server addresses

## 0.2.0 (2026-10-16)


### Features

* **needle:** review any of your Spotify playlists (ADR 0008)

## 0.1.0 (2026-10-15)


### Features

* **needle:** build, sign and publish releases with GitHub Actions (ADR 0007)

---

Releases up to 0.3.0 were made in needle's earlier repository, so their entries have no links.
