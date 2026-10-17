# needle mockups

**Playlist review picker, chosen (2026-10-16): A, Cover wall** (`playlists-a-wall.png`), with the shared Review screen `playlists-review.png`. B (Crate), C (Sound map) and D (Progress board) are kept for reference only. Built as drawn, with two changes: "Followed" sits on the cover instead of in the line under it (it squeezed the progress text), and the details card adds the average Rating. Names, covers, song counts, top artists, progress and the map positions are all made up.

**Review screen, chosen (2026-10-11): A, Calibrated, as-is** (`a-calibrated.png`, and the A column of `compact.png` for the narrow layout). B (Turntable) and C (Console) are kept for reference only.

Everything in the mockups is made up: the tracks, artists, covers, "sounds like" matches, Readings and Baseline.

`src/` holds the HTML/CSS that rendered them (`src/shot.sh page.html out.png [scale]`, headless Chromium at 1600×1000, the laptop's logical size, at scale 1.6). `src/base.css` maps the Omarchy `colors.toml` token names to CSS variables; `src/data.js` holds the mock data, `src/playlists.js` the playlists, and `src/picker.css` and `src/picker.js` what the four pickers share; the covers are abstract placeholders in `src/covers/`, written by `python3 src/covers.py` for every cover the mock data names. Every page uses the app's own Archivo (`app/src/fonts/`), so nothing is fetched to render them.
