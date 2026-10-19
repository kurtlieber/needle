# Changelog

## [0.4.1](https://github.com/kurtlieber/needle/compare/v0.4.0...v0.4.1) (2026-10-19)


### Bug Fixes

* Up next keeps its track after a Rating ([1a2b25d](https://github.com/kurtlieber/needle/commit/1a2b25d3e9914a40cece058fdce242331680a0c0))
* Up next keeps its track after a Rating ([a56ec17](https://github.com/kurtlieber/needle/commit/a56ec170b8e443cad8559d8fb384e6a4e1066ff2))

## [0.4.0](https://github.com/kurtlieber/needle/compare/v0.3.0...v0.4.0) (2026-10-19)


### Features

* **release:** a signed pacman repository, best-effort .deb and .rpm, and docs for going public ([3879499](https://github.com/kurtlieber/needle/commit/3879499bfcfaa835e08bf9f4773d0a96135b3e8e))
* **release:** install from a signed pacman repository on GitHub Releases, plus best-effort .deb and .rpm ([263a0d9](https://github.com/kurtlieber/needle/commit/263a0d9f93f37017ec6544d9db293cb6bf670f13))

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
