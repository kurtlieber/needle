# needle

The desktop app for Review, a client of haystack (the server). It speaks haystack's language ([haystack's glossary](https://github.com/kurtlieber/haystack/blob/main/GLOSSARY.md): Verdict, Rating, Pass, Dislike, Liked Songs, Unrated like, Trait, Reading, Baseline, Usual range, Review, Review queue, Followed playlist, Discoveries, Calibrate, Soft no, To Rip, Confidence) and adds the terms below.

## Language

**needle**:
The listener's desktop app for Review: it plays the Review queues, including the playlists they pick, and records their Verdicts.
_Avoid_: the player, the client, the Spotify app (that is Spotify's own)

**Playlist review**:
A Review queue of the songs on the playlists the listener picks from their Spotify library, each song once, however many of them it is on. It can skip the songs that already have a verdict, and survives a restart.
_Avoid_: playlist mode, session
