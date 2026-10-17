// Mockup data: the tracks, artists and covers are made up, and the Readings and Baseline are invented
// placeholders.
const TRACK = {
  title: "Glass Lantern", artist: "Moth Signal", album: "Glass Lantern", kind: "Single", year: 2018,
  cover: "covers/tr-glass-lantern.svg", duration: 179, position: 72,
  soundsLike: { title: "Paper Tide", artist: "Velvet Static", similarity: 0.96 },
  source: "Deezer artist radio", exploration: true,
  styles: [["Dubstep", 0.25], ["Bassline", 0.11], ["Downtempo", 0.10]],
};

const TRAITS = [
  { key: "tempo", name: "Tempo", unit: "BPM", min: 60, max: 180, song: 102, base: 126, lo: 118, hi: 140,
    poles: ["slow", "fast"], fmt: v => String(Math.round(v)), major: 30, minor: 5 },
  { key: "valence", name: "Valence", song: 0.14, base: 0.41, lo: 0.27, hi: 0.55, poles: ["sad", "happy"] },
  { key: "energy", name: "Energy", song: 0.83, base: 0.79, lo: 0.70, hi: 0.88, poles: ["calm", "intense"] },
  { key: "dance", name: "Danceability", song: 0.61, base: 0.67, lo: 0.58, hi: 0.76, poles: ["loose", "danceable"] },
  { key: "acoustic", name: "Acousticness", song: 0.02, base: 0.05, lo: 0.01, hi: 0.08, poles: ["electronic", "acoustic"] },
  { key: "instr", name: "Instrumentalness", song: 0.77, base: 0.34, lo: 0.10, hi: 0.61, poles: ["vocal", "instrumental"] },
  { key: "loud", name: "Loudness", unit: "LUFS", min: -20, max: 0, song: -7.1, base: -6.6, lo: -7.8, hi: -5.2,
    poles: ["quiet", "loud"], fmt: v => v.toFixed(1).replace("-", "−"), major: 5, minor: 1 },
];
for (const t of TRAITS) {
  t.min ??= 0; t.max ??= 1; t.major ??= 0.25; t.minor ??= 0.05;
  t.fmt ??= v => v.toFixed(2);
  t.norm = v => (v - t.min) / (t.max - t.min);
  t.outside = t.song < t.lo ? "below" : t.song > t.hi ? "above" : null;
}

const QUEUE = [
  { title: "Blackout (Kilo Tide Remix)", artist: "Neon Basin", cover: "covers/tr-blackout.svg", verdict: 5 },
  { title: "Undertow", artist: "Copperline", verdict: 4 },
  { title: "Low Hum", artist: "Static Bloom", verdict: "pass" },
  { title: "Iron Bloom", artist: "Ostra", verdict: 3 },
  { title: "starfall* (VIP)", artist: "Ninefold", verdict: 4 },
  { title: "Fall Apart", artist: "Halcyon Ward", verdict: "dislike" },
  { title: "Tape Loop", artist: "Arlo Brine", cover: "covers/tr-tape-loop.svg", verdict: 2 },
  { title: "Glass Lantern", artist: "Moth Signal", cover: "covers/tr-glass-lantern.svg", current: true },
  { title: "Close Behind", artist: "Grey Orchard", cover: "covers/tr-close-behind.svg" },
  { title: "Lemon Soda", artist: "Saffron Hill", cover: "covers/tr-lemon-soda.svg" },
];
const QUEUE_TOTAL = 20, QUEUE_DONE = 7;

// deterministic noise
function rng(seed) { return () => (seed = (seed * 16807) % 2147483647) / 2147483647; }

// a dubstep-ish spectrum frame: heavy sub, a mid hump, rolling off; 0..1 per band
function spectrum(n, seed = 7) {
  const r = rng(seed); const out = [];
  for (let i = 0; i < n; i++) {
    const f = i / (n - 1);
    const sub = Math.exp(-((f - 0.06) ** 2) / 0.006) * 0.95;
    const mid = Math.exp(-((f - 0.38) ** 2) / 0.03) * 0.62;
    const air = Math.exp(-((f - 0.72) ** 2) / 0.02) * 0.30;
    const v = Math.max(sub, mid, air, 0.12 * (1 - f)) * (0.72 + 0.4 * r());
    out.push(Math.min(1, Math.max(0.04, v)));
  }
  return out;
}

const ICON = {
  needle: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><circle cx="12" cy="12" r="9" opacity=".45"/><circle cx="12" cy="12" r="2.2" fill="currentColor" stroke="none"/><path d="M12 12 L19 4.5"/></svg>',
  star: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M12 2.6l2.85 6.05 6.6.78-4.88 4.53 1.3 6.52L12 17.2l-5.87 3.28 1.3-6.52-4.88-4.53 6.6-.78z"/></svg>',
  starOutline: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linejoin="round"><path d="M12 2.6l2.85 6.05 6.6.78-4.88 4.53 1.3 6.52L12 17.2l-5.87 3.28 1.3-6.52-4.88-4.53 6.6-.78z"/></svg>',
  ban: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><circle cx="12" cy="12" r="8.5"/><path d="M6 6l12 12"/></svg>',
  pass: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 12h12"/><path d="M12 7l5 5-5 5"/><path d="M20 5v14"/></svg>',
  play: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M8 5.2v13.6a.8.8 0 0 0 1.2.7l10.6-6.8a.8.8 0 0 0 0-1.4L9.2 4.5A.8.8 0 0 0 8 5.2z"/></svg>',
  pause: '<svg viewBox="0 0 24 24" fill="currentColor"><rect x="6" y="5" width="4" height="14" rx="1.2"/><rect x="14" y="5" width="4" height="14" rx="1.2"/></svg>',
  next: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M5 6.2v11.6a.8.8 0 0 0 1.25.66L14.5 12.7a.8.8 0 0 0 0-1.4L6.25 5.54A.8.8 0 0 0 5 6.2z"/><rect x="16.5" y="5" width="2.6" height="14" rx="1.2"/></svg>',
  prev: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M19 6.2v11.6a.8.8 0 0 1-1.25.66L9.5 12.7a.8.8 0 0 1 0-1.4l8.25-5.76A.8.8 0 0 1 19 6.2z"/><rect x="4.9" y="5" width="2.6" height="14" rx="1.2"/></svg>',
  keyboard: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"><rect x="2.5" y="6" width="19" height="12" rx="2.5"/><path d="M6.5 10h.01M10 10h.01M13.5 10h.01M17 10h.01M8 14h8"/></svg>',
  heart: '<svg viewBox="0 0 24 24" fill="currentColor"><path d="M12 20.3s-7.6-4.5-9.1-9.3C1.8 7.4 4.2 4.4 7.4 4.4c2 0 3.4 1.1 4.6 2.6 1.2-1.5 2.6-2.6 4.6-2.6 3.2 0 5.6 3 4.5 6.6-1.5 4.8-9.1 9.3-9.1 9.3z"/></svg>',
  check: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"><path d="M5 12.5l4.5 4.5L19 7.5"/></svg>',
};

function fmtTime(s) { return `${Math.floor(s / 60)}:${String(Math.round(s % 60)).padStart(2, "0")}`; }

// "Slower, darker and more instrumental than you usually like."
function summarySentence() {
  const words = {
    tempo: { below: "slower", above: "faster" }, valence: { below: "darker", above: "happier" },
    energy: { below: "calmer", above: "more intense" }, dance: { below: "looser", above: "more danceable" },
    acoustic: { below: "more electronic", above: "more acoustic" }, instr: { below: "more vocal", above: "more instrumental" },
    loud: { below: "quieter", above: "louder" },
  };
  const parts = TRAITS.filter(t => t.outside).map(t => words[t.key][t.outside]);
  if (!parts.length) return "Right in your usual range on every trait.";
  const list = parts.length === 1 ? parts[0] : parts.slice(0, -1).join(", ") + " and " + parts.at(-1);
  return `<em>${list[0].toUpperCase() + list.slice(1)}</em> than what you usually like.`;
}
