# Changelog

## [0.7.1](https://github.com/kurtlieber/needle/compare/v0.7.0...v0.7.1) (2026-10-29)


### Bug Fixes

* **review:** songs Spotify can't play are passed over, and a queue of only them ends ([#26](https://github.com/kurtlieber/needle/issues/26)) ([76167da](https://github.com/kurtlieber/needle/commit/76167da754c19c0d16046aed0b42ab8b1c57569d))

## [0.7.0](https://github.com/kurtlieber/needle/compare/v0.6.3...v0.7.0) (2026-10-23)


### Features

* flag a possible cover and swap it for its original ([#24](https://github.com/kurtlieber/needle/issues/24)) ([e091ed1](https://github.com/kurtlieber/needle/commit/e091ed19cdd4ff3ef4ff45eae8f318b69e5e7a42))

## [0.6.3](https://github.com/kurtlieber/needle/compare/v0.6.2...v0.6.3) (2026-10-22)


### Bug Fixes

* **playlists:** a song on two releases is reviewed once (haystack's ADR 0021) ([#21](https://github.com/kurtlieber/needle/issues/21)) ([426e019](https://github.com/kurtlieber/needle/commit/426e019325ba36a15e83edcbda487d1f78e51f2e))

## [0.6.2](https://github.com/kurtlieber/needle/compare/v0.6.1...v0.6.2) (2026-10-21)


### Bug Fixes

* **app:** a click on the Playlists tab during a review opens the picker ([#19](https://github.com/kurtlieber/needle/issues/19)) ([ca4e7e8](https://github.com/kurtlieber/needle/commit/ca4e7e8c6bb12f7cdb7a54d6dd67cdfeb464137b))

## [0.6.1](https://github.com/kurtlieber/needle/compare/v0.6.0...v0.6.1) (2026-10-21)


### Bug Fixes

* **app:** a 4K window zooms the layout to fill it, and stays sharp ([6ea358c](https://github.com/kurtlieber/needle/commit/6ea358ccb96a18e77071f6780bee61129a96d4bd))
* **app:** a 4K window zooms the layout to fill it, and stays sharp ([0f0da64](https://github.com/kurtlieber/needle/commit/0f0da6439dad868f9a05871d521aca37a8ab996e))
* **review:** say haystack found no preview, not that Deezer has none ([c38b315](https://github.com/kurtlieber/needle/commit/c38b315314b87e5e86c2f6fb26b158093bced222))

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
