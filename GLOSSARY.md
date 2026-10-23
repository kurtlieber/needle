# needle

The desktop app for Review, a client of haystack (the server). It speaks haystack's language ([haystack's glossary](https://github.com/kurtlieber/haystack/blob/main/GLOSSARY.md): Verdict, Rating, Pass, Dislike, Liked Songs, Unrated like, Song, Trait, Reading, Baseline, Usual range, Review, Review queue, Followed playlist, Discoveries, Calibrate, Soft no, To Rip, Confidence, Possible cover, Original, Swap) and adds the terms below.

## Language

**needle**:
The listener's desktop app for Review: it plays the Review queues, including the playlists they pick, and records their Verdicts.
_Avoid_: the player, the client, the Spotify app (that is Spotify's own)

**Playlist review**:
A Review queue of the songs on the playlists the listener picks from their Spotify library, each song once, however many of them it is on and however many of its releases they hold (a Song, from haystack 0.8.0). It can skip the songs that already have a verdict on any release, and survives a restart.
_Avoid_: playlist mode, session
