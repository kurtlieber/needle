#!/usr/bin/env python3
"""Placeholder covers for the mockups: abstract SVG art, the same every run (`python3 covers.py`).

Writes covers/<name>.svg for every cover the mock data names (data.js, playlists.js and the pages)."""
import hashlib
import math
import random
import re
from pathlib import Path

HERE = Path(__file__).parent
SIZE = 300


def _hsl(h: float, s: float, l: float) -> str:
    return f"hsl({h % 360:.0f} {s:.0f}% {l:.0f}%)"


def cover(name: str) -> str:
    r = random.Random(hashlib.sha256(name.encode()).digest())
    h = r.uniform(0, 360)
    h2 = h + r.choice((30, 150, 180, 210))
    bg = (f'<defs><linearGradient id="g" x1="0" y1="0" x2="1" y2="1">'
          f'<stop offset="0" stop-color="{_hsl(h, r.uniform(45, 80), r.uniform(18, 34))}"/>'
          f'<stop offset="1" stop-color="{_hsl(h2, r.uniform(45, 80), r.uniform(8, 20))}"/>'
          f'</linearGradient></defs><rect width="{SIZE}" height="{SIZE}" fill="url(#g)"/>')
    shapes = []
    style = r.randrange(4)
    accent = _hsl(h2 + r.uniform(-20, 20), r.uniform(60, 90), r.uniform(55, 72))
    c = SIZE / 2
    if style == 0:      # rings
        cx, cy = r.uniform(80, 220), r.uniform(80, 220)
        for i in range(r.randint(4, 8)):
            shapes.append(f'<circle cx="{cx:.0f}" cy="{cy:.0f}" r="{18 + i * r.uniform(14, 22):.0f}" fill="none" '
                          f'stroke="{accent}" stroke-width="{r.uniform(2, 7):.1f}" opacity="{0.85 - i * 0.09:.2f}"/>')
    elif style == 1:    # stripes
        a = r.choice((0, 30, 45, 60, 90, 120, 135))
        w = r.uniform(10, 26)
        for i in range(-10, 20):
            if r.random() < 0.55:
                shapes.append(f'<rect x="{i * w * 1.8:.0f}" y="-150" width="{w:.0f}" height="600" fill="{accent}" '
                              f'opacity="{r.uniform(0.25, 0.85):.2f}" transform="rotate({a} {c} {c})"/>')
    elif style == 2:    # a sun over a horizon
        y = r.uniform(170, 230)
        shapes.append(f'<circle cx="{r.uniform(90, 210):.0f}" cy="{y - 10:.0f}" r="{r.uniform(50, 80):.0f}" fill="{accent}"/>')
        for i in range(5):
            shapes.append(f'<rect x="0" y="{y + i * 14:.0f}" width="{SIZE}" height="{7 - i:.0f}" fill="{accent}" '
                          f'opacity="{0.8 - i * 0.12:.2f}"/>')
        shapes.append(f'<rect x="0" y="{y + 70:.0f}" width="{SIZE}" height="{SIZE}" fill="{_hsl(h2, 50, 8)}"/>')
    else:               # a wave field
        for i in range(r.randint(7, 12)):
            y0, amp, k = 30 + i * 26, r.uniform(8, 30), r.uniform(1.5, 3.5)
            pts = " ".join(f"{x:.0f},{y0 + amp * math.sin(k * x / SIZE * 2 * math.pi + i):.1f}"
                           for x in range(0, SIZE + 1, 10))
            shapes.append(f'<polyline points="{pts}" fill="none" stroke="{accent}" '
                          f'stroke-width="{r.uniform(1.5, 5):.1f}" opacity="{r.uniform(0.35, 0.95):.2f}"/>')
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {SIZE} {SIZE}" width="{SIZE}" height="{SIZE}">'
            + bg + "".join(shapes) + "</svg>\n")


def main() -> None:
    names = set()
    for f in [*HERE.glob("*.js"), *HERE.glob("*.html")]:
        names |= set(re.findall(r"covers/([a-z0-9-]+)\.svg", f.read_text()))
    out = HERE / "covers"
    out.mkdir(exist_ok=True)
    for n in sorted(names):
        (out / f"{n}.svg").write_text(cover(n))
    print(f"{len(names)} covers in {out}")


if __name__ == "__main__":
    main()
