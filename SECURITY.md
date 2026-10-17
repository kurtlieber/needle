# Security

## Reporting a vulnerability

Report it privately: on GitHub, open this repository's **Security** tab and choose **Report a vulnerability**.
Please don't open a public issue or pull request for it. needle has one maintainer, so replies are best
effort; you'll hear back before anything about it is made public. A vulnerability in haystack, the server,
goes to [haystack's repository](https://github.com/kurtlieber/haystack/security) instead.

Only the latest release gets fixes.

## What needle promises

A report that breaks one of these is a vulnerability:

* **The haystack token stays private.** needle reads it from the token file and puts it only on its requests
  to the haystack servers in `config.toml`, never on cover downloads. Its notices name the file, never the
  token. needle never writes the token file or `config.toml`.
* **Spotify's credentials stay private.** needle-engine keeps librespot's reusable credentials in a 0700
  directory of their own, `~/.local/state/needle/librespot/`.
* **The outbox is private.** The Verdicts and plays waiting for haystack are kept in a 0600 file.
* **Releases are signed** (ADR 0007). Check the `.sig` against `needle-signing-key.asc` before you install.

## Known limits

These are by design, not vulnerabilities:

* needle reaches haystack over plain HTTP. haystack's
  [SECURITY.md](https://github.com/kurtlieber/haystack/blob/main/SECURITY.md) says what that exposes and how
  to limit it.
* needle plays through librespot, which isn't Spotify's own client (NOTICE).
