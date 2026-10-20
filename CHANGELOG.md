# Changelog

## [0.6.0](https://github.com/kurtlieber/needle/compare/v0.5.0...v0.6.0) (2026-10-20)


### Features

* **review:** a Calibrate tab for the songs haystack is least sure about ([a9a1e01](https://github.com/kurtlieber/needle/commit/a9a1e0190dc9fb58bea9aef8b42153df288a59ad))
* **review:** a Calibrate tab for the songs haystack is least sure about ([cfb98e9](https://github.com/kurtlieber/needle/commit/cfb98e9aeffeb955f96948f5dbd5df90af9eb617))


### Bug Fixes

* **review:** a finished Rate your likes offers Calibrate while it has songs left ([b8e7bec](https://github.com/kurtlieber/needle/commit/b8e7bec529ed84c0003a8bd9624b883890e1c9ed))

## [0.5.0](https://github.com/kurtlieber/needle/compare/v0.4.1...v0.5.0) (2026-10-19)


### Features

* show the re-ranker's first reason and its Confidence on a Discoveries pick ([f0eeb15](https://github.com/kurtlieber/needle/commit/f0eeb15451107706783a2293c3ad32c7440a3f41))
* show the re-ranker's first reason and its Confidence on a Discoveries pick ([96a1063](https://github.com/kurtlieber/needle/commit/96a10635bc9ca8700285afcad2c7d11628e3f48f))

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
