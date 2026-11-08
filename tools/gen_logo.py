#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Writes firmware/src/sd_logo.h: the Star Distroyer logo, traced from the user's drawing of it (docs/sd/
logo_source.png, the "/// STAR DISTROYER" cut from the user's first mockup), as 4-bit masks for the 240 x 240
screen; and
docs/readme/banner.png, the README's banner, and docs/readme/slash.png, its "///" for the headings, traced the
same way at their own sizes.

The shapes are the drawing's own. What a plain resize of it lost (2026-11-06, the user: it looked like a resized
original, its colours off) is put back:
  - The letters are a clean mask, not the picture's brightness. Each pixel is traced at 8 x 8 samples (the banner at
    24 x 24), the small scratches of the picture closed, so a letter is solid C_ICE and a slash solid C_RED, the
    screen's own colours, with no metallic shading left in them as grey.
  - The bars land on whole pixels. The drawing's six horizontal edges (the top, the three bars' edges, the
    bottom, found from its rows) are mapped to whole rows of each size, and the rows between are stretched to fit.
    Only the slanted edges are smoothed, as at the drawing's own size.

A pixel is 4 bits: 0 nothing, 1..7 the logo's ice white at that coverage (of 7), 9..15 its red (the slashes) at
coverage 1..7. Two pixels a byte, the left one high; a row starts on a byte.
    SD_LOGO_STAR       "/// STAR"     the splash's first line
    SD_LOGO_DISTROYER  "DISTROYER"    its second, at the same scale
    SD_LOGO_HEADER     the whole line, small: the main screen's top left
The banner is the whole line on the screen's background with the splash's streaming stars and its shooting star,
at 2x for sharp screens (shown at half its width). Needs Pillow and NumPy (python3 -m pip install pillow numpy);
writes build/logo_preview.png to look at."""
import os
import random
import numpy as np
from PIL import Image

HERE = os.path.dirname(__file__)
SRC = os.path.join(HERE, "..", "docs", "sd", "logo_source.png")
OUT = os.path.join(HERE, "..", "firmware", "src", "sd_logo.h")
BANNER = os.path.join(HERE, "..", "docs", "readme", "banner.png")
SLASH = os.path.join(HERE, "..", "docs", "readme", "slash.png")
PREVIEW = os.path.join(HERE, "..", "build", "logo_preview.png")
TOP, BOTTOM = 0, 36                     # the logo's rows in the drawing, with a margin (glyphs at 4..31)
SLASH_STAR = (38, 258)                  # "///" 42..107 and "STAR" 125..254
SLASHES = (38, 114)                     # the "///" alone
DISTROYER = (280, 596)                  # 284..592
LINE = (38, 596)                        # the whole line
U = 8                                   # samples a source pixel, each way (the screen's sizes)
U_BANNER = 24                           # (the banner's: its pixels are a third of a source pixel)
# Each size: the cap height in pixels, the rows the six edges go to, and its width against the drawing's aspect.
SPLASH = (20, (0, 4, 8, 12, 16, 20), 1.0)        # 4 px bars, 4 px between: DISTROYER ~226 px wide
HEADER = (10, (0, 2, 4, 6, 8, 10), 0.92)         # 2 px bars and gaps; 8 % narrower, so it clears P07
BANNER_CAP = (64, (0, 14, 27, 39, 50, 64), 1.0)  # the drawing's proportions, on whole pixels
BANNER_SIZE = (1760, 440)                        # 2x: shown 880 x 220
SLASH_CAP = (28, (0, 6, 12, 17, 22, 28), 1.0)    # 2x: shown 14 px tall, a heading's mark
BG, ICE, ICE_D, RED = (6, 8, 12), (205, 230, 240), (84, 104, 116), (235, 28, 28)   # C_BG C_ICE C_ICE_D C_RED


def masks(img, u):
    """the white letters' and the red slashes' masks, u x u samples a source pixel (boolean arrays [y, x])"""
    crop = img.crop((0, TOP, img.width, BOTTOM))
    big = crop.resize((crop.width * u, crop.height * u), Image.BICUBIC)
    rgb = np.asarray(big).astype(np.int32)
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    red = (r > 95) & (r > 2.2 * np.maximum(g, b))
    white = (np.asarray(big.convert("L")) > 112) & ~red     # the slashes are not white
    close = u + 1                               # close the picture's scratches, under a source pixel wide
    return erode(dilate(white, close), close), red


def dilate(m, k):
    """a k x k maximum of a boolean mask, as two passes of k shifts (the mask's border is far from the letters)"""
    for axis in (0, 1):
        p = np.pad(m, [(k // 2, k // 2) if a == axis else (0, 0) for a in (0, 1)])
        m = np.logical_or.reduce([np.take(p, range(i, i + m.shape[axis]), axis=axis) for i in range(k)])
    return m


def erode(m, k):
    return ~dilate(~m, k)


def edges(white, u, x0, x1):
    """the six horizontal edges of the letters, in source pixels from TOP: the cap top, the top bar's bottom, the
    middle bar's top and bottom, the bottom bar's top, the cap bottom; from the rows' ink between x0 and x1"""
    ink = [int(v) for v in white[:, x0 * u:x1 * u:2].sum(axis=1)]
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
            if y - s >= u:                      # (a bar is at least a source pixel tall)
                runs.append((s, y))
        else:
            y += 1
    assert len(runs) == 3, f"expected three bars, found {runs}"
    return [v / u for v in (top, runs[0][1], runs[1][0], runs[1][1], runs[2][0], bot)]


def coverage(white, red, u, x0, x1, size, e):
    """the masks between source columns x0..x1 at a size: two float arrays [row, column], white's and red's
    coverage 0..1 of each target pixel, the empty columns at either end trimmed"""
    H, keys, narrow = size
    sx = H / (e[-1] - e[0]) * narrow            # target pixels a source pixel, across
    w = int(round((x1 - x0) * sx))

    def src_y(t):                               # a target row edge -> the source's, piecewise linear between keys
        for i in range(5):
            if t <= keys[i + 1]:
                return e[i] + (t - keys[i]) / (keys[i + 1] - keys[i]) * (e[i + 1] - e[i])
        return e[5]

    ys = [int(round(src_y(r) * u)) for r in range(H + 1)]
    xs = [int(round((x0 + c / sx) * u)) for c in range(w + 1)]
    iw, ir = (np.pad(m.astype(np.int64).cumsum(0).cumsum(1), ((1, 0), (1, 0))) for m in (white, red))

    def area(ii):                               # sums over each [ys[r], ys[r+1]) x [xs[c], xs[c+1]) box
        Y, X = np.array(ys), np.array(xs)
        return ii[Y[1:, None], X[None, 1:]] - ii[Y[:-1, None], X[None, 1:]] - ii[Y[1:, None], X[None, :-1]] \
            + ii[Y[:-1, None], X[None, :-1]]

    n = np.diff(np.array(ys))[:, None] * np.diff(np.array(xs))[None, :]
    cw, cr = area(iw) / n, area(ir) / n
    ink = (np.rint(7 * cw) > 0) | (np.rint(7 * cr) > 0)
    cols = np.nonzero(ink.any(axis=0))[0]
    return cw[:, cols[0]:cols[-1] + 1], cr[:, cols[0]:cols[-1] + 1]


def trace(white, red, x0, x1, size, e):
    """the screen's 4-bit codes: rows of 0..15"""
    cw, cr = coverage(white, red, U, x0, x1, size, e)
    a, b = np.rint(7 * cr).astype(int), np.rint(7 * cw).astype(int)
    code = np.where((a > 0) & (a >= b), 8 + a, b)
    return code.shape[1], code.shape[0], code.tolist()


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


def banner(cw, cr):
    """the README's banner: the stars as the splash streams them, the shooting star, the line in the middle"""
    W, H = BANNER_SIZE
    img = np.zeros((H, W, 3), np.float64) + BG
    bg, ice, red = (np.array(c, np.float64) for c in (BG, ICE, RED))

    def blend(x, y, c, a):
        if 0 <= x < W and 0 <= y < H and a > 0:
            img[y, x] += (c - img[y, x]) * min(a, 1.0)

    h, w = cw.shape
    x0, y0 = (W - w) // 2, (H - h) // 2
    rnd = random.Random(0x5D15)
    n = 0
    while n < 150:                              # streaming right to left: the fast ones long and bright
        x, y, v = rnd.randrange(W), rnd.randrange(H), rnd.randrange(6, 76)
        a, length = min(1 + v / 12, 7) / 7 * 0.85, v // 3
        if x0 - 40 - length < x < x0 + w + 40 and y0 - 30 < y < y0 + h + 30:
            continue                            # (none across the line)
        n += 1
        for t in (0, 1):                        # (2 px tall at 2x)
            blend(x, y + t, ice, a)
            for k in range(1, length + 1):
                blend(x + k, y + t, ice, a * (length - k + 1) / (length + 2))
    hx, hy = 1250, 132                          # the shooting star over the line's end: the head lower left, the trail
    for k in range(150, -1, -1):                # up to the right, ice near the head, red behind, fading
        a = (7 - k * 6 / 150) / 7
        c = ice if k < 40 else red
        for dx, dy, f in ((0, 0, 1), (1, 0, 1), (0, 1, 0.45), (1, 1, 0.45), (0, -1, 0.2), (1, -1, 0.2)):
            blend(hx + 2 * k + dx, hy - k + dy, c, a * f)
    for k in range(-7, 8):                      # its head, a small glow
        blend(hx + k, hy, ice, 1 - abs(k) / 8)
        blend(hx, hy + k // 2, ice, 1 - abs(k) / 5)
    area = img[y0:y0 + h, x0:x0 + w]
    wa, ra = cw[..., None], cr[..., None]
    area[:] = area * np.clip(1 - wa - ra, 0, 1) + ice * wa + red * ra
    os.makedirs(os.path.dirname(BANNER), exist_ok=True)
    Image.fromarray(np.clip(np.rint(img), 0, 255).astype(np.uint8)).save(BANNER, optimize=True)


def preview(logos):
    """the three on the screen's background in its colours, 3x: build/logo_preview.png"""
    wmax = max(m[0] for m in logos) + 8
    img = Image.new("RGB", (wmax, sum(m[1] + 8 for m in logos) + 4), BG)
    px, y0 = img.load(), 4
    for w, h, rows in logos:
        for y in range(h):
            for x in range(w):
                v = rows[y][x]
                if v:
                    c, a = (RED if v & 8 else ICE), (v & 7) / 7
                    px[x + 4, y0 + y] = tuple(round(BG[i] + (c[i] - BG[i]) * a) for i in range(3))
        y0 += h + 8
    os.makedirs(os.path.dirname(PREVIEW), exist_ok=True)
    img.resize((img.size[0] * 3, img.size[1] * 3), Image.NEAREST).save(PREVIEW)


def main():
    img = Image.open(SRC).convert("RGB")
    white, red = masks(img, U)
    e = edges(white, U, 120, 594)               # the letters (not the slashes)
    star = trace(white, red, *SLASH_STAR, SPLASH, e)
    distroyer = trace(white, red, *DISTROYER, SPLASH, e)
    header = trace(white, red, *LINE, HEADER, e)
    with open(OUT, "w") as f:
        f.write("/* SPDX-License-Identifier: GPL-3.0-only */\n")
        f.write("/* Generated by tools/gen_logo.py from docs/sd/logo_source.png. Do not edit. */\n")
        f.write("#ifndef SD_LOGO_H\n#define SD_LOGO_H\n#include <stdint.h>\n\n")
        f.write("typedef struct { uint16_t w, h; const uint8_t *px; } SdLogo;   /* 4 bits a pixel, see the script */\n\n")
        emit(f, "SD_LOGO_STAR", star)
        emit(f, "SD_LOGO_DISTROYER", distroyer)
        emit(f, "SD_LOGO_HEADER", header)
        f.write("#endif\n")
    preview([header, star, distroyer])
    wb, rb = masks(img, U_BANNER)
    cw, cr = coverage(wb, rb, U_BANNER, *LINE, BANNER_CAP, edges(wb, U_BANNER, 120, 594))
    banner(cw, cr)
    sw, sr = coverage(wb, rb, U_BANNER, *SLASHES, SLASH_CAP, edges(wb, U_BANNER, 120, 594))
    rgba = np.zeros(sw.shape + (4,), np.uint8)   # red on nothing: it sits on GitHub's light or dark page
    rgba[..., :3] = RED
    rgba[..., 3] = np.clip(np.rint(255 * (sr + sw)), 0, 255).astype(np.uint8)
    Image.fromarray(rgba, "RGBA").save(SLASH, optimize=True)
    print("gen_logo -> %s\n  the drawing's edges (rows) %s; header %dx%d, splash %dx%d and %dx%d; preview %s\n"
          "  banner %s: %dx%d, the line %dx%d" % (
              os.path.normpath(OUT), " ".join("%.2f" % (v + TOP) for v in e), header[0], header[1], star[0], star[1],
              distroyer[0], distroyer[1], os.path.normpath(PREVIEW), os.path.normpath(BANNER), *BANNER_SIZE,
              cw.shape[1], cw.shape[0]))


if __name__ == "__main__":
    main()
