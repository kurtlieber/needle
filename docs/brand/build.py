#!/usr/bin/env python3
"""needle's brand kit: the SVG masters, PNG exports and site icons in docs/brand/ (`python3 docs/brand/build.py`).

The marks are the app's own geometry (Icon.qml, Wordmark.qml, ScaleTrack.qml, Pulse.qml, spectrumitem.cpp) in
the matte-black theme's colours. hb-view (HarfBuzz) outlines the type, so the SVGs need no font, and Chrome or
Chromium renders the PNGs (CHROME picks one). haystack's docs/brand/build.py imports this file for its helpers
and both marks."""
import base64
import functools
import json
import math
import os
import random
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).parent
FONT = Path(os.environ.get("ARCHIVO") or HERE.parents[1] / "app/src/fonts/Archivo[wdth,wght].ttf")

# The matte-black theme (theme.cpp), the accent needle falls back to amber for, haystack's straw, the light ground.
BG, TILE, SURFACE, HAIR, DIM = "#121212", "#161616", "#1e1e1e", "#333333", "#555555"
MUTED, FG, BRIGHT, AMBER = "#8a8a8d", "#bebebe", "#e6e6e6", "#e68e0d"
STRAW, STRAW_DEEP, PAPER = "#d8c079", "#a3842c", "#f4f3ef"

# Archivo styles: (wdth, wght, tracking in em, OpenType features).
WORD = (125, 700, -0.01, "")       # the wordmark (Wordmark.qml)
TITLE = (112, 700, -0.02, "")
BODY = (100, 400, 0, "")
NUM = (125, 300, -0.03, "tnum")    # the big readings and the BPM
LABEL = (125, 600, 0, "")          # "BPM"
TRAIT = (100, 600, 0, "")          # a scale's name (TraitRow.qml)

NEEDLE_INK = (2, 2, 22, 22)        # the marks' ink on their 24-unit grid
HAYSTACK_INK = (2, 3, 22, 21)


def n(v: float, places: int = 2) -> str:
    """A number, short: 12.0 -> "12", 3.14159 -> "3.14"."""
    s = f"{v:.{places}f}".rstrip("0").rstrip(".")
    return "0" if s == "-0" else s


def svg(w: float, h: float, body: str, title: str, defs: str = "", px: float | None = None) -> str:
    """A standalone SVG, `w` x `h` units, drawn `px` wide (one unit a pixel by default)."""
    s = (px or w) / w
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {n(w)} {n(h)}" width="{n(w * s)}" '
            f'height="{n(h * s)}"><title>{title}</title>' + (f"<defs>{defs}</defs>" if defs else "") + body + "</svg>\n")


def glow(uid: str, blur: float, alpha: float) -> str:
    """The app's MultiEffect glow: the shape over a blurred copy of itself at `alpha`."""
    return (f'<filter id="{uid}" x="-1" y="-1" width="3" height="3" color-interpolation-filters="sRGB">'
            f'<feGaussianBlur stdDeviation="{n(blur)}"/><feComponentTransfer><feFuncA type="linear" '
            f'slope="{n(alpha)}"/></feComponentTransfer><feMerge><feMergeNode/><feMergeNode in="SourceGraphic"/>'
            '</feMerge></filter>')


def lit(uid: str) -> str:
    return f' filter="url(#{uid})"'


def fade(uid: str, c: str, alpha: float) -> str:
    """A radial gradient from `c` at `alpha` to nothing."""
    return (f'<radialGradient id="{uid}"><stop stop-color="{c}" stop-opacity="{n(alpha)}"/>'
            f'<stop offset="1" stop-color="{c}" stop-opacity="0"/></radialGradient>')


def place(body: str, x: float, y: float, size: float, grid: float = 24) -> str:
    """`body`, drawn on a `grid`-unit square, scaled to `size` with its top left at (x, y)."""
    return f'<g transform="translate({n(x)} {n(y)}) scale({n(size / grid, 4)})">{body}</g>'


def image(doc: str, x: float, y: float, w: float, h: float) -> str:
    """A whole SVG document drawn into another, its ids kept apart."""
    uri = "data:image/svg+xml;base64," + base64.b64encode(doc.encode()).decode()
    return f'<image href="{uri}" x="{n(x)}" y="{n(y)}" width="{n(w)}" height="{n(h)}"/>'


def scoped(body: str, prefix: str) -> str:
    """`body` with its ids and the references to them prefixed, so it can appear twice in one document."""
    return re.sub(r'(id="|url\(#)', rf"\1{prefix}", body)


# The marks

def needle_mark(c: str = AMBER, small: bool = False, attrs: str = "") -> str:
    """Icon.qml's needle on its 24-unit grid: a record's edge at 45 %, the spindle and the tonearm.
    `small` is the heavier cut for 16 to 32 px; `attrs` go on the spindle and arm (a glow)."""
    r, w, op, tip, dot = (8.7, 2.6, .5, "18.8 4.7", 2.8) if small else (9, 2, .45, "19 4.5", 2.2)
    return (f'<circle cx="12" cy="12" r="{r}" fill="none" stroke="{c}" stroke-width="{w}" opacity="{op}"/>'
            f'<g{attrs}><path d="M12 12 L{tip}" stroke="{c}" stroke-width="{w}" stroke-linecap="round"/>'
            f'<circle cx="12" cy="12" r="{dot}" fill="{c}"/></g>')


def haystack_mark(uid: str, straw: str = STRAW, needle: str = AMBER, small: bool = False,
                  straw_opacity: float = 1, attrs: str = "") -> str:
    """haystack's mark on the same grid: five straws stacked into a stook, and needle's own spindle and tonearm
    (the same dot, arm and angle) knocked through them. `uid` names the knock-out mask, once per document.
    `small` is the three-straw cut for 16 to 32 px."""
    if small:
        g, cut, straws, arm, (cx, cy, r) = 16, (4, 2.7), "M6 4h4M3.5 8h9M2 12h12", "M7.5 10.5 12.2 5.5", (7.5, 10.5, 1.7)
    else:
        g, cut, straws, arm, (cx, cy, r) = (24, (4.6, 3.5), "M9.5 4h5M6.5 8h11M4.8 12h14.4M3.8 16h16.4M3 20h18",
                                            "M12 14 19 6.5", (12, 14, 2.2))
    body = (f'<mask id="{uid}" maskUnits="userSpaceOnUse" x="0" y="0" width="{g}" height="{g}">'
            f'<rect width="{g}" height="{g}" fill="#fff"/><path d="{arm}" stroke="#000" stroke-width="{cut[0]}" '
            f'stroke-linecap="round"/><circle cx="{cx}" cy="{cy}" r="{cut[1]}"/></mask>'
            f'<path d="{straws}" fill="none" stroke="{straw}" stroke-width="2" stroke-linecap="round" '
            f'opacity="{n(straw_opacity)}" mask="url(#{uid})"/>'
            f'<g{attrs}><path d="{arm}" stroke="{needle}" stroke-width="2" stroke-linecap="round"/>'
            f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{needle}"/></g>')
    return body if g == 24 else f'<g transform="scale(1.5)">{body}</g>'


# Type, outlined

@functools.cache
def shaped(s: str, wdth: float, wght: float, features: str) -> tuple[list[tuple[float, str]], float]:
    """`s` shaped by HarfBuzz at 1000 units an em: each glyph's (pen x, outline) and the advance. Outlines are
    absolute from the glyph's origin, y negative up from the baseline; a space's is empty."""
    out = subprocess.run(
        ["hb-view", f"--font-file={FONT}", f"--variations=wdth={wdth},wght={wght}", f"--features={features}",
         "--font-size=1000", "--margin=0", "--output-format=svg", "--unicodes=" + ",".join(f"{ord(c):x}" for c in s)],
        capture_output=True, text=True, check=True).stdout
    paths = dict(re.findall(r'<g id="(glyph-[\d-]+)">\s*<path[^>]*?\sd="([^"]*)"', out))
    pens = re.findall(r'<use xlink:href="#(glyph-[\d-]+)" x="([-\d.]+)"', out)
    return [(float(x), paths.get(g, "")) for g, x in pens], float(re.search(r'<svg[^>]*\swidth="([\d.]+)', out)[1])


PAIR = re.compile(r"(-?[\d.]+) (-?[\d.]+)")


def words(s: str, size: float, x: float, y: float, style=WORD, anchor: str = "pen") -> tuple[str, tuple]:
    """`s` as path data at `size` px with its baseline at y. x is the pen's start, or with `anchor` where the
    ink's left, middle or right goes. Returns the data and the ink box (x0, y0, x1, y1)."""
    wdth, wght, track, features = style
    glyphs = [(pen + i * track * 1000, d) for i, (pen, d) in enumerate(shaped(s, wdth, wght, features)[0]) if d]
    k = size / 1000

    def draw(x0: float) -> tuple[str, list]:
        box = [math.inf, math.inf, -math.inf, -math.inf]

        def pt(m: re.Match, pen: float) -> str:
            px, py = x0 + (pen + float(m[1])) * k, y + float(m[2]) * k
            box[:] = min(box[0], px), min(box[1], py), max(box[2], px), max(box[3], py)
            return f"{n(px)} {n(py)}"
        return "".join(PAIR.sub(lambda m: pt(m, pen), d) for pen, d in glyphs), box

    d, box = draw(x)
    if anchor != "pen":
        d, box = draw(x + x - {"left": box[0], "middle": (box[0] + box[2]) / 2, "right": box[2]}[anchor])
    return d, tuple(box)


def text(s: str, size: float, x: float, y: float, fill: str, style=WORD, anchor: str = "left", attrs: str = "") -> str:
    return f'<path d="{words(s, size, x, y, style, anchor)[0]}" fill="{fill}"{attrs}/>'


def ring_text(s: str, cx: float, cy: float, r: float, size: float, fill: str, style=WORD) -> str:
    """`s` once round a circle at `size` px, clockwise with its baseline at radius r, spaced out evenly to close
    the ring and turned so its first word sits centred at the top."""
    wdth, wght, track, features = style
    glyphs, adv = shaped(s, wdth, wght, features)
    k = size / 1000
    pens = [pen + i * track * 1000 for i, (pen, _) in enumerate(glyphs)] + [adv + len(glyphs) * track * 1000]
    extra = (2 * math.pi * r / k - pens[-1]) / len(glyphs)
    assert extra >= 0, f"{s!r} at {size} px is longer than its ring"
    mids = [(pens[i] + pens[i + 1]) / 2 for i in range(len(glyphs))]
    at = [(mid + (i + .5) * extra) * k / r for i, mid in enumerate(mids)]
    turn = (at[0] + at[s.index(" ") - 1]) / 2
    d = []
    for i, (_, g) in enumerate(glyphs):
        ca, sa = math.cos(at[i] - turn), math.sin(at[i] - turn)

        def pt(m: re.Match) -> str:
            lx, ly = (float(m[1]) + pens[i] - mids[i]) * k, float(m[2]) * k - r
            return f"{n(cx + lx * ca - ly * sa)} {n(cy + lx * sa + ly * ca)}"
        d.append(PAIR.sub(pt, g))
    return f'<path d="{"".join(d)}" fill="{fill}"/>'


def x_height(size: float) -> float:
    return -words("x", size, 0, 0)[1][1]


def lockup(mark: str, ink: tuple, word: str, size: float, fill: str) -> tuple[str, tuple]:
    """Wordmark.qml's row, baseline at y = 0: the mark at .95 em centred .334 em above the baseline and the word
    from 1.48 em. Returns the body and its ink box."""
    m, s = .95 * size, .95 * size / 24
    top = -.334 * size - m / 2
    d, (x0, y0, x1, y1) = words(word, size, 1.48 * size, 0)
    box = (ink[0] * s, min(top + ink[1] * s, y0), x1, max(top + ink[3] * s, y1))
    return place(mark, 0, top, m) + f'<path d="{d}" fill="{fill}"/>', box


def stacked(mark: str, ink: tuple, word: str, size: float, fill: str) -> tuple[str, tuple]:
    """The stacked lockup, baseline at y = 0: the mark at 2 em centred over the word, its ink .42 em above."""
    d, (x0, y0, x1, y1) = words(word, size, 0, 0)
    s = 2 * size / 24
    left, top = (x0 + x1) / 2 - 12 * s, y0 - .42 * size - ink[3] * s
    box = (min(x0, left + ink[0] * s), top + ink[1] * s, max(x1, left + ink[2] * s), y1)
    return place(mark, left, top, 2 * size) + f'<path d="{d}" fill="{fill}"/>', box


def fitted(body: str, box: tuple, pad: float, title: str, defs: str = "", bg: str | None = None,
           px: float | None = None) -> str:
    """An SVG of `body` cropped to its ink `box` with `pad` all round, on `bg` if given."""
    x0, y0, x1, y1 = box
    w, h = x1 - x0 + 2 * pad, y1 - y0 + 2 * pad
    back = f'<rect width="{n(w)}" height="{n(h)}" fill="{bg}"/>' if bg else ""
    return svg(w, h, back + f'<g transform="translate({n(pad - x0)} {n(pad - y0)})">{body}</g>', title, defs, px)


def family(light: bool = False, size: float = 100) -> str:
    """needle and haystack side by side, one row of Wordmark.qml with a hairline between."""
    word, straw = (BG, STRAW_DEEP) if light else (BRIGHT, STRAW)
    a, (ax0, ay0, ax1, ay1) = lockup(needle_mark(), NEEDLE_INK, "needle", size, word)
    rule = ax1 + .73 * size
    shift = rule + .025 * size + .73 * size
    b, (bx0, by0, bx1, by1) = lockup(haystack_mark("family", straw), HAYSTACK_INK, "haystack", size, word)
    body = (a + f'<rect x="{n(rule)}" y="{n(-.884 * size)}" width="{n(.025 * size)}" height="{n(1.1 * size)}" '
            f'fill="{word}" fill-opacity=".25"/><g transform="translate({n(shift)} 0)">{b}</g>')
    box = (ax0, min(ay0, by0, -.884 * size), shift + bx1, max(ay1, by1, .216 * size))
    return fitted(body, box, x_height(size), "needle and haystack")


# needle's motifs, from the app

def scale(x: float, y: float, width: float, k: float = 1, ticks: int = 20, every: int = 5, base: float | None = None,
          song: float | None = None, uid: str | None = None, ground: str = BG) -> str:
    """ScaleTrack.qml at k x: the line at y and its ticks, the gap from the Baseline to this track, the Baseline
    ring at `base` and the needle at `song` (0 to 1 along the scale, or None), lit by the glow `uid`."""
    out = [f'<rect x="{n(x)}" y="{n(y)}" width="{n(width)}" height="{n(k)}" fill="{HAIR}"/>']
    for major in (True, False):
        h = (10 if major else 5) * k
        d = "".join(f"M{n(x + i / ticks * (width - k))} {n(y - h)}h{n(k)}v{n(h)}h{n(-k)}z"
                    for i in range(ticks + 1) if (i % every == 0) == major)
        out.append(f'<path d="{d}" fill="{MUTED if major else DIM}" opacity="{.7 if major else .8}"/>')
    bx, sx = (None if v is None else x + v * width for v in (base, song))
    if bx is not None and sx is not None:
        out.append(f'<rect x="{n(min(bx, sx))}" y="{n(y - 1.5 * k)}" width="{n(abs(sx - bx))}" height="{n(4 * k)}" '
                   f'fill="{AMBER}" fill-opacity=".38"/>')
    if bx is not None:
        out.append(f'<circle cx="{n(bx)}" cy="{n(y + .5 * k)}" r="{n(5 * k)}" fill="{ground}" stroke="{FG}" '
                   f'stroke-width="{n(2 * k)}"/>')
    if sx is not None:
        out.append(marker(sx, y, k, uid))
    return "".join(out)


def marker(x: float, y: float, k: float = 1, uid: str | None = None) -> str:
    """ScaleTrack's needle at x: its head above the line at y, its stem through it."""
    return (f'<path d="M{n(x - 6 * k)} {n(y - 28 * k)}h{n(12 * k)}L{n(x)} {n(y - 20 * k)}zM{n(x - k)} {n(y - 20 * k)}'
            f'h{n(2 * k)}v{n(34 * k)}h{n(-2 * k)}z" fill="{AMBER}"{lit(uid) if uid else ""}/>')


def legend(x: float, y: float, k: float = 1, ground: str = BG) -> str:
    """WideView's legend, centred on y from x: NeedleMarker and "This track", the ring and "Your baseline"."""
    size = 12.5 * k
    a, (_, _, ax1, _) = words("This track", size, x + 20 * k, y + .36 * size, BODY)
    rx = ax1 + 20 * k + 6 * k
    return (f'<path d="M3 1h8l-4 6zM6 5h2v12h-2z" fill="{AMBER}" transform="translate({n(x)} {n(y - 9 * k)}) '
            f'scale({n(k)})"/><path d="{a}" fill="{MUTED}"/>'
            f'<circle cx="{n(rx)}" cy="{n(y)}" r="{n(5 * k)}" fill="{ground}" stroke="{FG}" stroke-width="{n(2 * k)}"/>'
            + text("Your baseline", size, rx + 14 * k, y + .36 * size, MUTED, BODY))


def pulse(cx: float, cy: float, k: float = 1, uid: str | None = None) -> str:
    """Pulse.qml at rest: the glowing dot and its two rings."""
    return (f'<circle cx="{n(cx)}" cy="{n(cy)}" r="{n(43.25 * k)}" fill="none" stroke="{AMBER}" stroke-opacity=".18" '
            f'stroke-width="{n(1.5 * k)}"/><circle cx="{n(cx)}" cy="{n(cy)}" r="{n(29.25 * k)}" fill="none" '
            f'stroke="{AMBER}" stroke-opacity=".45" stroke-width="{n(1.5 * k)}"/>'
            f'<circle cx="{n(cx)}" cy="{n(cy)}" r="{n(16 * k)}" fill="{AMBER}"{lit(uid) if uid else ""}/>')


def spectrum(x: float, y: float, width: float, height: float, bars: int, seed: str, k: float = 1) -> str:
    """SpectrumItem's bars from (x, y) down to the floor: seeded levels in FG at .35 + .5 v, amber peaks."""
    rnd, gap = random.Random(seed), 2 * k
    bw, v, out = (width - gap * (bars - 1)) / bars, .75, []
    for i in range(bars):
        t = i / (bars - 1)
        v = min(1, max(.04, .5 * v + .5 * (.8 - .55 * t ** 1.5 + .08 * math.sin(11 * t)) * rnd.uniform(.7, 1.25)))
        bh = max(3 * k, v * (height - 8 * k))
        top = min(height - 2 * k, max(bh + 4 * k, min(1, v + rnd.uniform(.03, .22)) * (height - 4 * k)))
        bx = x + i * (bw + gap)
        out.append(f'<rect x="{n(bx)}" y="{n(y + height - bh)}" width="{n(bw)}" height="{n(bh)}" rx="{n(1.5 * k)}" '
                   f'fill="{FG}" fill-opacity="{n(.35 + .5 * v)}"/><rect x="{n(bx)}" y="{n(y + height - top)}" '
                   f'width="{n(bw)}" height="{n(2 * k)}" rx="{n(k)}" fill="{AMBER}"/>')
    return "".join(out)


def grooves(cx: float, cy: float, r0: float, r1: float, step: float, seed: str = "grooves", opacity: float = .04,
            width: float = 1) -> str:
    """A record's grooves: rings every `step` from r0 out to r1, in tracks with a wider gap between them."""
    rnd, r, d = random.Random(seed), r0, []
    while r <= r1:
        for _ in range(rnd.randint(5, 12)):
            if r <= r1:
                d.append(f"M{n(cx - r)} {n(cy)}a{n(r)} {n(r)} 0 1 0 {n(2 * r)} 0a{n(r)} {n(r)} 0 1 0 {n(-2 * r)} 0")
            r += step
        r += step * 1.6
    return (f'<path d="{"".join(d)}" fill="none" stroke="#fff" stroke-opacity="{n(opacity, 3)}" '
            f'stroke-width="{n(width)}"/>')


def sheen(uid: str, cx: float, cy: float, r: float, angle: float = -20, spread: float = 14) -> str:
    """A mask of two opposite wedges through (cx, cy), feathered: the light across a record's grooves."""
    d = ""
    for a in (angle, angle + 180):
        (x0, y0), (x1, y1) = ((cx + r * math.cos(math.radians(a + t)), cy + r * math.sin(math.radians(a + t)))
                              for t in (-spread, spread))
        d += f"M{n(cx)} {n(cy)}L{n(x0)} {n(y0)}L{n(x1)} {n(y1)}z"
    return (f'<filter id="{uid}-soft"><feGaussianBlur stdDeviation="{n(r * .05)}"/></filter>'
            f'<mask id="{uid}"><path d="{d}" fill="#fff" filter="url(#{uid}-soft)"/></mask>')


def dial(cx: float, cy: float, r: float, ticks: int = 60, every: int = 5, k: float = 1) -> str:
    """ScaleTrack's ticks wrapped round a circle, standing out from radius r."""
    d = {True: "", False: ""}
    for i in range(ticks):
        a, h = 2 * math.pi * i / ticks, (10 if i % every == 0 else 5) * k
        sa, ca = math.sin(a), -math.cos(a)
        d[i % every == 0] += f"M{n(cx + r * sa)} {n(cy + r * ca)}L{n(cx + (r + h) * sa)} {n(cy + (r + h) * ca)}"
    return "".join(f'<path d="{p}" stroke="{MUTED if major else DIM}" stroke-opacity="{.7 if major else .8}" '
                   f'stroke-width="{n(k)}"/>' for major, p in d.items())


# The one-page sheet, shared with haystack's kit

def sheet(word: str, marks: dict, icon_doc: str, palette: list, specimens: tuple, motifs: str, defs: str = "") -> str:
    """The brand sheet, 1600 x 1200: marks, lockups, colours, type and motifs on matte black. `marks` holds the
    colour, small, white, black and light marks and their ink; `specimens` a title, a line, a number and its unit."""
    w, h, m = 1600, 1200, 80
    ink = marks["ink"]
    head, _ = lockup(scoped(marks["colour"], "head"), ink, word, 40, BRIGHT)
    body = [f'<rect width="{w}" height="{h}" fill="{BG}"/>', f'<g transform="translate({m} 104)">{head}</g>',
            text("Brand kit", 15, w - m, 104, MUTED, LABEL, "right"),
            f'<rect x="{m}" y="140" width="{w - 2 * m}" height="1" fill="{HAIR}"/>']

    def label(s: str, x: float, y: float) -> str:
        return text(s.upper(), 12, x, y, MUTED, LABEL)

    # marks
    body += [label("Mark", m, 190), place(scoped(marks["colour"], "big"), m - 20, 210, 240),
             label("Small cut, 32 / 24 / 16 px", 340, 190)]
    for i, s in enumerate((32, 24, 16)):
        body.append(place(scoped(marks["small"], f"s{s}"), 340 + i * 52, 230 + 32 - s, s))
    body += [label("App icon", 340, 330), image(icon_doc, 340, 350, 96, 96),
             label("One colour", 560, 190), f'<rect x="560" y="210" width="200" height="236" rx="14" fill="{SURFACE}"/>',
             place(marks["white"], 590, 258, 140), f'<rect x="780" y="210" width="200" height="236" rx="14" fill="{PAPER}"/>',
             place(marks["black"], 810, 258, 140)]
    # lockups
    lx = 1020
    a, abox = lockup(scoped(marks["colour"], "dark"), ink, word, 44, BRIGHT)
    b, bbox = lockup(marks["light"], ink, word, 44, BG)
    body += [label("Lockup", lx, 190), f'<rect x="{lx}" y="210" width="{w - m - lx}" height="110" rx="14" fill="{SURFACE}"/>',
             f'<g transform="translate({n(lx + 40 - abox[0])} {n(265 - (abox[1] + abox[3]) / 2)})">{a}</g>',
             f'<rect x="{lx}" y="336" width="{w - m - lx}" height="110" rx="14" fill="{PAPER}"/>',
             f'<g transform="translate({n(lx + 40 - bbox[0])} {n(391 - (bbox[1] + bbox[3]) / 2)})">{b}</g>']
    # colours
    body.append(label("Colour", m, 510))
    sw = (w - 2 * m - 12 * (len(palette) - 1)) / len(palette)
    for i, (name, c) in enumerate(palette):
        x = m + i * (sw + 12)
        on = BG if c in (MUTED, FG, BRIGHT, AMBER, STRAW, STRAW_DEEP, PAPER) else FG
        body += [f'<rect x="{n(x)}" y="530" width="{n(sw)}" height="120" rx="10" fill="{c}" stroke="#fff" '
                 f'stroke-opacity=".06"/>', text(name, 14, x + 14, 614, on, TRAIT), text(c, 13, x + 14, 636, on, BODY)]
    # type
    big, line, sample, unit = specimens
    body += [label("Type: Archivo, one variable family", m, 710),
             text(word, 88, m, 816, BRIGHT), text("wdth 125, wght 700, -1 % tracking", 13, m, 846, MUTED, BODY),
             text(big, 34, 640, 772, BRIGHT, TITLE), text("Titles: wdth 112, wght 700", 13, 640, 798, MUTED, BODY),
             text(line, 17, 640, 838, FG, BODY), text("Text: wdth 100, wght 400", 13, 640, 862, MUTED, BODY)]
    d, nbox = words(sample, 88, 1180, 816, NUM)
    body += [f'<path d="{d}" fill="{BRIGHT}"/>', text(unit, 15, nbox[2] + 10, 816, MUTED, LABEL),
             text("Numbers: wdth 125, wght 300, tabular", 13, 1180, 846, MUTED, BODY)]
    # motifs
    body += [label("Motifs", m, 920), f'<rect x="{m}" y="940" width="{w - 2 * m}" height="180" rx="14" fill="{TILE}"/>',
             motifs]
    return svg(w, h, "".join(body), f"{word} brand sheet", defs)


# Layouts both kits share

def tile(body: str, warm: str, title: str, rx: float = 14, defs: str = "") -> str:
    """An app icon, 64 units drawn at 512 px: `body` on a matte tile with a lit edge, over a bloom of `warm`."""
    defs += ('<linearGradient id="face" x2="0" y2="1"><stop stop-color="#1f1f1f"/><stop offset="1" stop-color="#101010"/>'
             '</linearGradient><linearGradient id="edge" x2="0" y2="1"><stop stop-color="#fff" stop-opacity=".14"/>'
             '<stop offset=".5" stop-color="#fff" stop-opacity=".03"/><stop offset="1" stop-color="#fff" '
             'stop-opacity=".06"/></linearGradient>' + fade("warm", warm, .16))
    edge = (f'<rect x=".5" y=".5" width="63" height="63" rx="{n(rx - .5)}" fill="none" stroke="url(#edge)"/>'
            if rx else "")
    return svg(64, 64, f'<rect width="64" height="64" rx="{n(rx)}" fill="url(#face)"/>{edge}'
                       f'<circle cx="32" cy="32" r="24" fill="url(#warm)"/>{body}', title, defs, px=512)


def small_tile(mark: str, title: str) -> str:
    """The icon for 16 to 32 px: a mark's small cut, nearly filling a flat tile."""
    return svg(32, 32, f'<rect width="32" height="32" rx="7" fill="{TILE}"/>' + place(mark, 2, 2, 28), title, px=32)


def card(word: str, line: str, about: tuple, hero: str, strip: str, defs: str = "") -> str:
    """The link preview (Open Graph, GitHub), 1280 x 640: the name, its line and `about` on the left over the
    `hero`, and a `strip` along the bottom."""
    w, h = 1280, 640
    return svg(w, h, f'<rect width="{w}" height="{h}" fill="{BG}"/>{hero}' + text(word, 128, 96, 250, BRIGHT)
               + text(line, 40, 98, 326, FG, TITLE)
               + "".join(text(s, 24, 98, 384 + 34 * i, MUTED, BODY) for i, s in enumerate(about)) + strip, word, defs)


def header(word: str, mark: str, ink: tuple, line: str, art: str, defs: str = "") -> str:
    """A header for sites and READMEs, 1600 x 400: the lockup and its line on the left, `art` on the right."""
    w, h = 1600, 400
    body, box = lockup(mark, ink, word, 84, BRIGHT)
    return svg(w, h, f'<rect width="{w}" height="{h}" fill="{BG}"/><g transform="translate({n(120 - box[0])} 196)">'
               f'{body}</g>' + text(line, 28, 120, 262, MUTED, BODY) + art, word, defs)


def disc(line: str, mark: str, ticks: int, every: int, title: str, size: float = 46, extra: str = "") -> str:
    """A round sticker, 600 units across: `mark` in the middle (lit by "lit"), ticks round it, `line` round the
    edge and anything `extra` over the ticks."""
    c = 300
    body = (f'<circle cx="{c}" cy="{c}" r="297" fill="{TILE}" stroke="{HAIR}" stroke-width="3"/>'
            + ring_text(line, c, c, 232, size, BRIGHT) + dial(c, c, 176, ticks, every, 1.8) + extra
            + place(mark, c - 156, c - 156, 312))
    return svg(600, 600, body, title, glow("lit", .7, .6))


def kit(word: str, ink: tuple, dark: str, light: str) -> dict[str, str]:
    """The type on dark and light grounds, `dark` and `light` being the mark for each: the wordmark, the lockups
    and the two products together."""
    out, pad = {}, x_height(100)
    for theme, fill, mark in (("on-dark", BRIGHT, dark), ("on-light", BG, light)):
        d, box = words(word, 100, 0, 0)
        out[f"{word}-wordmark-{theme}"] = fitted(f'<path d="{d}" fill="{fill}"/>', box, pad, word)
        out[f"{word}-lockup-{theme}"] = fitted(*lockup(mark, ink, word, 100, fill), pad, word)
        out[f"{word}-lockup-stacked-{theme}"] = fitted(*stacked(mark, ink, word, 100, fill), pad, word)
        out[f"needle-and-haystack-{theme}"] = family(theme == "on-light")
    return out


def exports(word: str, out: dict[str, str], big: tuple) -> list:
    """The PNG jobs: the icon from 1024 to 16 px, the mark at 1024, the lockups at 2x, and `big`'s (name, scale)."""
    pngs = [(out[f"{word}-icon"], f"{word}-icon-{s}", s, s) for s in (1024, 512, 256, 128, 64)]
    pngs += [(out[f"{word}-icon-small"], f"{word}-icon-{s}", s, s) for s in (32, 16)]
    pngs.append((out[f"{word}-mark"], f"{word}-mark-1024", 1024, 1024))
    pngs += [at(doc, name, 2) for name, doc in out.items() if "lockup" in name or name.startswith("needle-and-")]
    return pngs + [at(out[name], name, s) for name, s in big]


# needle's assets

def icon(rx: float = 14) -> str:
    """The app icon, pkgbuild/needle.svg finished: the mark lit on a matte tile over a record's grooves."""
    return tile(grooves(32, 32, 6.4, 15, .8, "icon", .06, .35) + place(needle_mark(attrs=lit("lit")), 8, 8, 48),
                AMBER, "needle", rx, glow("lit", 1, .6))


def social() -> str:
    """The link preview: the mark lit at a record's spindle, a reading along the bottom."""
    cx, cy, s = 990, 262, 400
    hero = (f'<circle cx="{cx}" cy="{cy}" r="330" fill="url(#warm)"/>' + grooves(cx, cy, 42, 140, 5.5, "social", .05)
            + place(needle_mark(attrs=lit("lit")), cx - s / 2, cy - s / 2, s))
    return card("needle", "Play it. Read it. Judge it.",
                ("A desktop player for what haystack finds,", "each track read against your baseline."), hero,
                scale(96, 560, 1088, 2, 44, 4, .34, .58, "needle"),
                fade("warm", AMBER, .14) + glow("lit", .7, .6) + glow("needle", 6, .6))


def banner() -> str:
    """The header: the app's spectrum on the right."""
    return header("needle", needle_mark(), NEEDLE_INK, "Play it. Read it. Judge it.",
                  spectrum(880, 104, 600, 158, 64, "banner") + text("20 Hz", 15, 880, 294, MUTED, BODY)
                  + text("20 kHz", 15, 1480, 294, MUTED, BODY, "right"))


def wallpaper(w: int, h: int, cx: float, cy: float, s: float) -> str:
    """A wallpaper: the mark lit at a record's spindle, its grooves running out past the corners."""
    far = max(math.hypot(cx - x, cy - y) for x in (0, w) for y in (0, h))
    defs = (fade("bloom", AMBER, .1) + glow("lit", .7, .6) + sheen("sheen", cx, cy, far)
            + f'<radialGradient id="shade" cx="{n(cx)}" cy="{n(cy)}" r="{n(far)}" gradientUnits="userSpaceOnUse">'
              '<stop stop-color="#161616"/><stop offset="1" stop-color="#0b0b0b"/></radialGradient>')
    body = (f'<rect width="{w}" height="{h}" fill="url(#shade)"/>'
            f'<circle cx="{n(cx)}" cy="{n(cy)}" r="{n(s * 1.5)}" fill="url(#bloom)"/>'
            + grooves(cx, cy, s * .58, far, s * .024, "wallpaper", .035, s * .004)
            + f'<g mask="url(#sheen)">{grooves(cx, cy, s * .58, far, s * .024, "wallpaper", .09, s * .004)}</g>'
            + place(needle_mark(attrs=lit("lit")), cx - s / 2, cy - s / 2, s))
    return svg(w, h, body, "needle", defs)


ANIMATION = """
.ring,.arm,.dot,.halo{transform-box:view-box;transform-origin:12px 12px}
.ring{animation:edge .9s ease-out both,breathe .5s 1.5s ease-out infinite}
.arm{animation:read 1.5s cubic-bezier(.3,.7,.3,1) both}
.dot{animation:beat .5s 1.5s ease-out infinite}
.halo{animation:halo .5s 1.5s ease-out infinite}
@keyframes edge{from{opacity:0}}
@keyframes read{from{transform:rotate(80deg)}60%{transform:rotate(-6deg)}80%{transform:rotate(2deg)}to{transform:none}}
@keyframes beat{12%{transform:scale(1.14)}70%{transform:none}}
@keyframes breathe{12%{transform:scale(1.04)}52%{transform:none}}
@keyframes halo{from{opacity:.5;transform:none}to{opacity:0;transform:scale(3.6)}}
@media (prefers-reduced-motion:reduce){*{animation:none!important}}
"""


def animated() -> str:
    """The mark for sites, animated: the tonearm swings in and settles, then the spindle beats at 120 BPM
    (Pulse.qml's kick). Still with reduced motion."""
    body = (f'<style>{ANIMATION}</style>'
            f'<circle class="ring" cx="12" cy="12" r="9" fill="none" stroke="{AMBER}" stroke-width="2" opacity=".45"/>'
            f'<circle class="halo" cx="12" cy="12" r="2.2" fill="{AMBER}" opacity="0"/>'
            f'<path class="arm" d="M12 12 L19 4.5" stroke="{AMBER}" stroke-width="2" stroke-linecap="round"/>'
            f'<circle class="dot" cx="12" cy="12" r="2.2" fill="{AMBER}"/>')
    return svg(24, 24, body, "needle", px=256)


def pattern() -> str:
    """A tile for backgrounds, 480 x 240, seamless: rows of scales a tick every 12, each read against a Baseline."""
    rnd, body = random.Random("pattern"), [f'<rect width="480" height="240" fill="{BG}"/>']
    for y in (40, 100, 160, 220):
        b = rnd.uniform(.2, .75)
        body.append(scale(0, y, 481, 1, 40, 5, b, b + rnd.choice((-1, 1)) * rnd.uniform(.06, .15)))
    return svg(480, 240, "".join(body), "needle pattern")


def main() -> None:
    out = {"needle-mark": svg(24, 24, needle_mark(), "needle", px=256),
           "needle-mark-small": svg(24, 24, needle_mark(small=True), "needle", px=32),
           "needle-mark-white": svg(24, 24, needle_mark("#ffffff"), "needle", px=256),
           "needle-mark-black": svg(24, 24, needle_mark(BG), "needle", px=256),
           "needle-mark-animated": animated(),
           "needle-icon": icon(),
           "needle-icon-small": small_tile(needle_mark(small=True), "needle")}
    out |= kit("needle", NEEDLE_INK, needle_mark(), needle_mark())
    out["needle-social"] = social()
    out["needle-banner"] = banner()
    out["needle-wallpaper"] = wallpaper(3840, 2160, 2470, 1240, 520)
    out["needle-wallpaper-phone"] = wallpaper(1179, 2556, 590, 1460, 420)
    out["needle-sticker"] = disc("needle · play it · read it · judge it · ", needle_mark(attrs=lit("lit")), 60, 5,
                                 "needle sticker")
    out["needle-pattern"] = pattern()
    marks = {"colour": needle_mark(), "small": needle_mark(small=True), "white": needle_mark("#ffffff"),
             "black": needle_mark(BG), "light": needle_mark(), "ink": NEEDLE_INK}
    palette = [("Ground", BG), ("Tile", TILE), ("Surface", SURFACE), ("Hairline", HAIR), ("Dim", DIM),
               ("Muted", MUTED), ("Text", FG), ("Bright", BRIGHT), ("Amber", AMBER), ("Paper", PAPER)]
    motifs = (scale(130, 1062, 560, 1.6, 30, 5, .3, .62, "smark", TILE) + legend(130, 1094, 1.2, TILE)
              + pulse(890, 1030, 1.2, "spulse") + spectrum(1040, 966, 440, 110, 56, "sheet")
              + text("20 Hz", 12, 1040, 1100, MUTED, BODY) + text("20 kHz", 12, 1480, 1100, MUTED, BODY, "right"))
    out["needle-brand-sheet"] = sheet("needle", marks, icon(), palette,
                                      ("Play it. Read it. Judge it.", "A desktop player for what haystack finds.", "128",
                                       "BPM"), motifs, glow("smark", 4, .6) + glow("spulse", 8, .75))
    write(out)
    pngs = exports("needle", out, (("needle-social", 1), ("needle-banner", 2), ("needle-wallpaper", 1),
                                   ("needle-wallpaper-phone", 1), ("needle-sticker", 2), ("needle-brand-sheet", 1)))
    small = svg(24, 24, needle_mark(small=True), "needle")
    web("needle", out["needle-icon"], icon(0), small, small, pngs)


def at(doc: str, name: str, scale_by: float) -> tuple[str, str, int, int]:
    """A PNG job for `doc` at `scale_by` times its own size."""
    w, h = (float(v) for v in re.search(r'viewBox="0 0 ([\d.]+) ([\d.]+)"', doc).groups())
    return doc, name, round(w * scale_by), round(h * scale_by)


def write(docs: dict[str, str], folder: Path = HERE) -> None:
    (folder / "svg").mkdir(parents=True, exist_ok=True)
    for name, doc in docs.items():
        (folder / "svg" / f"{name}.svg").write_text(doc)


def web(name: str, icon_doc: str, bleed_doc: str, small_doc: str, favicon: str, pngs: list, folder: Path = HERE) -> None:
    """Render the PNGs, then the site icons: favicon.svg and .ico, the touch icon, and the manifest and its two."""
    site = folder / "web"
    site.mkdir(parents=True, exist_ok=True)
    (site / "favicon.svg").write_text(favicon)
    icons = [{"src": f"/icon-{s}.png", "type": "image/png", "sizes": f"{s}x{s}"} for s in (192, 512)]
    (site / "site.webmanifest").write_text(json.dumps({"name": name, "icons": icons, "theme_color": BG,
                                                       "background_color": BG}, indent=2) + "\n")
    jobs = [(d, folder / "png" / f"{name}.png", w, h) for d, name, w, h in pngs]
    jobs += [(bleed_doc, site / "apple-touch-icon.png", 180, 180), (icon_doc, site / "icon-192.png", 192, 192),
             (icon_doc, site / "icon-512.png", 512, 512)]
    jobs += [(small_doc, site / f"favicon-{s}.png", s, s) for s in (16, 32, 48)]
    (folder / "png").mkdir(exist_ok=True)
    shoot(jobs)
    sizes = [site / f"favicon-{s}.png" for s in (16, 32, 48)]
    (site / "favicon.ico").write_bytes(ico([p.read_bytes() for p in sizes]))
    for p in sizes:
        p.unlink()
    print(f"{len(list((folder / 'svg').glob('*.svg')))} SVGs, {len(jobs) - 3} PNGs, favicon.ico in {folder}")


# Export

def chrome() -> str:
    for c in (os.environ.get("CHROME"), "chromium", "google-chrome-stable", "google-chrome",
              "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"):
        if c and shutil.which(c):
            return shutil.which(c)
    sys.exit("no Chrome or Chromium found: set CHROME to one")


def shoot(jobs: list[tuple[str, Path, int, int]]) -> None:
    """Render each SVG to a PNG of exactly w x h pixels with headless Chrome."""
    exe = chrome()

    def one(job: tuple[str, Path, int, int]) -> None:
        doc, out, w, h = job
        with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as tmp:
            page = Path(tmp) / "page.html"
            uri = "data:image/svg+xml;base64," + base64.b64encode(doc.encode()).decode()
            page.write_text(f'<!doctype html><body style="margin:0"><img src="{uri}" width="{w}" height="{h}" '
                            'style="display:block">')
            out.unlink(missing_ok=True)
            proc = subprocess.Popen([exe, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--no-first-run",
                                     "--no-default-browser-check", f"--user-data-dir={tmp}/profile",
                                     f"--window-size={w},{h}", "--force-device-scale-factor=1",
                                     "--default-background-color=00000000", f"--screenshot={out}", page.as_uri()],
                                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            # ponytail: headless Chrome lingers after the screenshot, so wait for the file to settle, then stop it
            deadline, size = time.time() + 60, -1
            while time.time() < deadline and not (out.exists() and out.stat().st_size == size > 0):
                size = out.stat().st_size if out.exists() else -1
                time.sleep(.4)
            proc.kill()
            proc.wait()
        got = struct.unpack(">II", out.read_bytes()[16:24])
        assert got == (w, h), f"{out.name} is {got[0]} x {got[1]}, not {w} x {h}"

    with ThreadPoolExecutor(4) as pool:
        list(pool.map(one, jobs))


def ico(pngs: list[bytes]) -> bytes:
    """A .ico of PNG entries, as browsers and Windows read them."""
    head, offset = struct.pack("<HHH", 0, 1, len(pngs)), 6 + 16 * len(pngs)
    entries = b""
    for p in pngs:
        w, h = struct.unpack(">II", p[16:24])
        entries += struct.pack("<BBBBHHII", w % 256, h % 256, 0, 0, 1, 32, len(p), offset)
        offset += len(p)
    return head + entries + b"".join(pngs)


if __name__ == "__main__":
    main()
