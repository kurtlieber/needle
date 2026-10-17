// Shared helpers for the playlist-picker mockups (data in playlists.js, icons in data.js).
const byName = n => PLAYLISTS.find(p => p.name.startsWith(n));
for (const p of PLAYLISTS) {
  p.rated = p.stars.reduce((a, b) => a + b, 0);
  p.reviewed = p.rated + p.passed + p.disliked;
  p.left = p.songs - p.reviewed;
  p.done = p.left === 0;
}
const nf = new Intl.NumberFormat("en-US");
const n = v => nf.format(v);

Object.assign(ICON, {
  search: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round"><circle cx="11" cy="11" r="6.5"/><path d="M16 16l4.5 4.5"/></svg>',
  playlist: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><path d="M4 6h11M4 11h11M4 16h7"/><circle cx="17.5" cy="16.5" r="2.5"/><path d="M20 16.5V7l2 1"/></svg>',
  shuffle: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M16 4h4v4M20 4l-6.5 6.5M4 20l6-6M16 20h4v-4M20 20L4 4"/></svg>',
  enter: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M19 5v6a3 3 0 0 1-3 3H6"/><path d="M10 10l-4 4 4 4"/></svg>',
  followed: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round"><path d="M5 12a7 7 0 0 1 14 0M8.5 12a3.5 3.5 0 0 1 7 0"/><circle cx="12" cy="12" r="1" fill="currentColor"/></svg>',
});

function topbar() {
  return `
  <div class="wordmark">${ICON.needle}needle</div>
  <nav class="tabs" aria-label="Review queues">
    <div class="tab">Discoveries <span class="count">7 of 20</span></div>
    <div class="tab">Rate your likes <span class="count">361</span></div>
    <div class="tab active playlists">Playlists</div>
  </nav>
  <div class="spacer"></div>
  <div class="status"><span class="dot"></span>Synced with haystack</div>
  <div class="iconbtn" aria-label="Keyboard shortcuts">${ICON.keyboard}</div>`;
}

function coverImg(p, cls = "") {
  return p.cover ? `<img class="${cls}" src="${p.cover}" alt="">`
                 : `<div class="${cls} cover-ph">${ICON.playlist}</div>`;
}

// the verdict colours, one per kind; every use sits beside a count or a legend in text
const VCOL = { 5: "rgb(var(--accent-rgb) / 1)", 4: "rgb(var(--accent-rgb) / 0.78)", 3: "rgb(var(--accent-rgb) / 0.58)",
               2: "rgb(var(--accent-rgb) / 0.40)", 1: "rgb(var(--accent-rgb) / 0.26)", pass: "var(--pass)", dislike: "var(--red)" };

// segments of a playlist's progress: 5★ … 1★, Pass, Dislike, then the songs still to review
function segments(p) {
  return [[p.stars[4], VCOL[5]], [p.stars[3], VCOL[4]], [p.stars[2], VCOL[3]], [p.stars[1], VCOL[2]], [p.stars[0], VCOL[1]],
          [p.passed, VCOL.pass], [p.disliked, VCOL.dislike]];
}

// a progress ring: reviewed share as an arc, split by verdict
function ring(p, size = 44, stroke = 4) {
  const r = (size - stroke) / 2, c = 2 * Math.PI * r, cx = size / 2;
  let off = 0, arcs = "";
  for (const [k, col] of segments(p)) {
    if (!k) continue;
    const len = (k / p.songs) * c;
    arcs += `<circle cx="${cx}" cy="${cx}" r="${r}" fill="none" stroke="${col}" stroke-width="${stroke}" stroke-dasharray="${Math.max(len - 0.6, 0.4)} ${c}" stroke-dashoffset="${-off}" transform="rotate(-90 ${cx} ${cx})"/>`;
    off += len;
  }
  return `<svg width="${size}" height="${size}" viewBox="0 0 ${size} ${size}" aria-hidden="true"><circle cx="${cx}" cy="${cx}" r="${r}" fill="none" stroke="rgb(255 255 255 / 0.10)" stroke-width="${stroke}"/>${arcs}</svg>`;
}

// a straight progress bar, same segments
function bar(p, h = 6) {
  let s = "";
  for (const [k, col] of segments(p)) if (k) s += `<i style="flex:${k};background:${col}"></i>`;
  if (p.left) s += `<i style="flex:${p.left};background:var(--none)"></i>`;
  return `<div class="vbar" style="height:${h}px">${s}</div>`;
}

function progressText(p) {
  if (p.done) return "All reviewed";
  if (!p.reviewed) return `${n(p.songs)} songs`;
  return `${n(p.left)} of ${n(p.songs)} to review`;
}

// the real union of the selected playlists' songs (owned ones), precomputed on the server
function sessionDock(sel, { songs, overlap, haveVerdict }) {
  const covers = sel.map(p => coverImg(p)).join("");
  return `
  <div class="stack">${covers}</div>
  <div class="what">
    <div class="l1">${sel.length} playlists · ${n(songs - haveVerdict)} songs to review</div>
    <div class="l2">${n(haveVerdict)} already have a verdict${overlap ? ` · ${overlap} are on more than one and play once` : ""}</div>
  </div>
  <div class="opts">
    <span class="switch on"><i></i>Skip songs with a verdict</span>
    <span class="switch on"><i></i>Shuffle</span>
  </div>
  <button class="btn primary">Start review <span class="kbd">Enter</span></button>`;
}
