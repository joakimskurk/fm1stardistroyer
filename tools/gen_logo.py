#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Writes firmware/src/sd_logo.h: the Star Distroyer logo, traced from the user's mockup (docs/sd/ui/
mockup_workstation.webp, the "/// STAR DISTROYER" in its top left), as 4-bit masks for the 240 x 240 screen.

The shapes are the mockup's own. What a plain resize of it lost (2026-11-06, the user: it looked like a resized
original, its colours off) is put back:
  - The letters are a clean mask, not the picture's brightness. Each pixel is traced at 8 x 8 samples, the small
    scratches of the picture closed, so a letter is solid C_ICE and a slash solid C_RED, the screen's own colours,
    with no metallic shading left in them as grey.
  - The bars land on whole pixels. The mockup's six horizontal edges (the top, the three bars' edges, the
    bottom, found from its rows) are mapped to whole rows of each size, and the rows between are stretched to fit.
    Only the slanted edges are smoothed, as at the mockup's own size.

A pixel is 4 bits: 0 nothing, 1..7 the logo's ice white at that coverage (of 7), 9..15 its red (the slashes) at
coverage 1..7. Two pixels a byte, the left one high; a row starts on a byte.
    SD_LOGO_STAR       "/// STAR"     the splash's first line
    SD_LOGO_DISTROYER  "DISTROYER"    its second, at the same scale
    SD_LOGO_HEADER     the whole line, small: the main screen's top left
Needs Pillow (python3 -m pip install pillow); writes build/logo_preview.png to look at."""
import os
from PIL import Image, ImageFilter

HERE = os.path.dirname(__file__)
SRC = os.path.join(HERE, "..", "docs", "sd", "ui", "mockup_workstation.webp")
OUT = os.path.join(HERE, "..", "firmware", "src", "sd_logo.h")
PREVIEW = os.path.join(HERE, "..", "build", "logo_preview.png")
TOP, BOTTOM = 10, 46                    # the logo's rows in the mockup, with a margin (glyphs at 14..41)
SLASH_STAR = (38, 258)                  # "///" 42..107 and "STAR" 125..254
DISTROYER = (280, 596)                  # 284..592
LINE = (38, 596)                        # the whole line
U = 8                                   # samples a mockup pixel, each way
# Each size: the cap height in pixels, the rows the six edges go to, and its width against the mockup's aspect.
SPLASH = (20, (0, 4, 8, 12, 16, 20), 1.0)        # 4 px bars, 4 px between: DISTROYER ~226 px wide
HEADER = (10, (0, 2, 4, 6, 8, 10), 0.92)         # 2 px bars and gaps; 8 % narrower, so it clears P07


def masks(img):
    """the white letters' and the red slashes' masks, U x U samples a mockup pixel (L images, 0 / 255)"""
    crop = img.crop((0, TOP, img.width, BOTTOM))
    big = crop.resize((crop.width * U, crop.height * U), Image.BICUBIC)
    r, g, b = big.split()
    redness = Image.merge("RGB", (r, g, b)).point(lambda v: v)   # (a copy)
    pr, pg, pb = r.load(), g.load(), b.load()
    red = Image.new("L", big.size, 0)
    out = red.load()
    for y in range(big.height):
        for x in range(big.width):
            rv = pr[x, y]
            if rv > 95 and rv > 2.2 * max(pg[x, y], pb[x, y]):
                out[x, y] = 255
    del redness
    white = big.convert("L").point(lambda v: 255 if v > 112 else 0)
    white = Image.composite(Image.new("L", big.size, 0), white, red)           # the slashes are not white
    close = U + 1                               # close the picture's scratches, under a mockup pixel wide
    white = white.filter(ImageFilter.MaxFilter(close)).filter(ImageFilter.MinFilter(close))
    return white, red


def edges(white, x0, x1):
    """the six horizontal edges of the letters, in mockup pixels from TOP: the cap top, the top bar's bottom, the
    middle bar's top and bottom, the bottom bar's top, the cap bottom; from the rows' ink between x0 and x1"""
    px = white.load()
    ink = [sum(1 for x in range(x0 * U, x1 * U, 2) if px[x, y]) for y in range(white.height)]
    rows = [y for y, v in enumerate(ink) if v]
    top, bot = rows[0], rows[-1] + 1
    hi = sorted(ink[top:bot])
    cut = (hi[len(hi) // 8] + hi[-len(hi) // 8]) / 2   # between the gaps' ink and the bars'
    runs, y = [], top
    while y < bot:
        if ink[y] > cut:
            s = y
            while y < bot and ink[y] > cut:
                y += 1
            if y - s >= U:                      # (a bar is at least a mockup pixel tall)
                runs.append((s, y))
        else:
            y += 1
    assert len(runs) == 3, f"expected three bars, found {runs}"
    return [v / U for v in (top, runs[0][1], runs[1][0], runs[1][1], runs[2][0], bot)]


def trace(white, red, x0, x1, size, e):
    """the masks between mockup columns x0..x1 at a size: rows of 0..15"""
    H, keys, narrow = size
    sx = H / (e[-1] - e[0]) * narrow            # target pixels a mockup pixel, across
    w = int(round((x1 - x0) * sx))
    pw, pr = white.load(), red.load()

    def src_y(t):                               # a target row edge -> the mockup's, piecewise linear between keys
        for i in range(5):
            if t <= keys[i + 1]:
                return e[i] + (t - keys[i]) / (keys[i + 1] - keys[i]) * (e[i + 1] - e[i])
        return e[5]

    rows = []
    for r in range(H):
        ya, yb = int(round(src_y(r) * U)), int(round(src_y(r + 1) * U))
        row = []
        for c in range(w):
            xa, xb = int(round((x0 + c / sx) * U)), int(round((x0 + (c + 1) / sx) * U))
            n = (xb - xa) * (yb - ya)
            sw = sum(1 for yy in range(ya, yb) for xx in range(xa, xb) if pw[xx, yy])
            sr = sum(1 for yy in range(ya, yb) for xx in range(xa, xb) if pr[xx, yy])
            a, b = round(7 * sr / n), round(7 * sw / n)
            row.append(8 + a if a and a >= b else b)
        rows.append(row)
    while all(not rr[0] for rr in rows):        # trim the empty columns at either end
        rows = [rr[1:] for rr in rows]
    while all(not rr[-1] for rr in rows):
        rows = [rr[:-1] for rr in rows]
    return len(rows[0]), H, rows


def emit(f, name, m):
    w, h, rows = m
    data = []
    for row in rows:
        row = row + [0] * (len(row) % 2)
        data += [row[i] << 4 | row[i + 1] for i in range(0, len(row), 2)]
    f.write("static const uint8_t %s_PX[%d] = {\n" % (name, len(data)))
    for i in range(0, len(data), 20):
        f.write("    " + ", ".join("%d" % v for v in data[i:i + 20]) + ",\n")
    f.write("};\nstatic const SdLogo %s = {%d, %d, %s_PX};\n\n" % (name, w, h, name))


def preview(logos):
    """the three on the screen's background in its colours, 3x: build/logo_preview.png"""
    bg, iceC, redC = (6, 8, 12), (205, 230, 240), (235, 28, 28)
    wmax = max(m[0] for m in logos) + 8
    img = Image.new("RGB", (wmax, sum(m[1] + 8 for m in logos) + 4), bg)
    px, y0 = img.load(), 4
    for w, h, rows in logos:
        for y in range(h):
            for x in range(w):
                v = rows[y][x]
                if v:
                    c, a = (redC if v & 8 else iceC), (v & 7) / 7
                    px[x + 4, y0 + y] = tuple(round(bg[i] + (c[i] - bg[i]) * a) for i in range(3))
        y0 += h + 8
    os.makedirs(os.path.dirname(PREVIEW), exist_ok=True)
    img.resize((img.size[0] * 3, img.size[1] * 3), Image.NEAREST).save(PREVIEW)


def main():
    img = Image.open(SRC).convert("RGB")
    white, red = masks(img)
    e = edges(white, 120, 594)                  # the letters (not the slashes)
    star = trace(white, red, *SLASH_STAR, SPLASH, e)
    distroyer = trace(white, red, *DISTROYER, SPLASH, e)
    header = trace(white, red, *LINE, HEADER, e)
    with open(OUT, "w") as f:
        f.write("/* SPDX-License-Identifier: GPL-3.0-only */\n")
        f.write("/* Generated by tools/gen_logo.py from docs/sd/ui/mockup_workstation.webp. Do not edit. */\n")
        f.write("#ifndef SD_LOGO_H\n#define SD_LOGO_H\n#include <stdint.h>\n\n")
        f.write("typedef struct { uint16_t w, h; const uint8_t *px; } SdLogo;   /* 4 bits a pixel, see the script */\n\n")
        emit(f, "SD_LOGO_STAR", star)
        emit(f, "SD_LOGO_DISTROYER", distroyer)
        emit(f, "SD_LOGO_HEADER", header)
        f.write("#endif\n")
    preview([header, star, distroyer])
    print("gen_logo -> %s\n  the mockup's edges (rows) %s; header %dx%d, splash %dx%d and %dx%d; preview %s" % (
        os.path.normpath(OUT), " ".join("%.2f" % (v + TOP) for v in e), header[0], header[1], star[0], star[1],
        distroyer[0], distroyer[1], os.path.normpath(PREVIEW)))


if __name__ == "__main__":
    main()
