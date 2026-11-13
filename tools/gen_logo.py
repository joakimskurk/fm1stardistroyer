#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Joakim (joakimskurk)
"""Writes firmware/src/sd_logo.h: the Star Distroyer logo as 4-bit masks for the 240 x 240 screen; and
docs/readme/banner.png, the README's banner, docs/readme/slash.png, its "///" for the headings, and
docs/readme/logo_line.png, the banner's line alone on nothing (the page's, web/: its stars move).

The logo is "/// STAR DISTROYER": the letters set in Orbitron Black (assets/fonts, SIL OFL 1.1: the user's choice,
2026-11-09, over a tracing of the first mockup, whose letters kept the picture's pixel errors), leaning 10 degrees
(0.175 across a row, the mockup's slant), after three red slashes with the mockup's own proportions. Each size is
drawn at 8 x 8 samples a pixel and boxed down, its cap height on whole pixels, so a letter is solid C_ICE and a slash
solid C_RED, the screen's own colours, their slanted edges smoothed.

A pixel is 4 bits: 0 nothing, 1..7 the logo's ice white at that coverage (of 7), 9..15 its red (the slashes) at
coverage 1..7. Two pixels a byte, the left one high; a row starts on a byte.
    SD_LOGO_STAR       "/// STAR"     the splash's first line
    SD_LOGO_DISTROYER  "DISTROYER"    its second, at the same size
    SD_LOGO_HEADER     the whole line, small: the main screen's top left
The banner is the whole line on the screen's background with the splash's streaming stars and its shooting star,
at 2x for sharp screens (shown at half its width). Needs Pillow (with FreeType) and NumPy (python3 -m pip install
pillow numpy); writes build/logo_preview.png to look at."""
import os
import random
import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(__file__)
FONT = os.path.join(HERE, "..", "assets", "fonts", "Orbitron-VariableFont_wght.ttf")
OUT = os.path.join(HERE, "..", "firmware", "src", "sd_logo.h")
BANNER = os.path.join(HERE, "..", "docs", "readme", "banner.png")
SLASH = os.path.join(HERE, "..", "docs", "readme", "slash.png")
LOGO_LINE = os.path.join(HERE, "..", "docs", "readme", "logo_line.png")
PREVIEW = os.path.join(HERE, "..", "build", "logo_preview.png")
WEIGHT = 900                            # Orbitron's axis: Black
SLANT = 0.175                           # across a row, the letters leaning right (10 degrees)
SS = 8                                  # samples a pixel, each way
# the slashes, in caps: a slash's width, its lean (top over bottom), the step between them; the letters start this
# far from the first slash's foot (as the mockup has them)
SL_W, SL_LEAN, SL_STEP, SL_TEXT = 0.40, 0.75, 0.66, 2.42
# each size: its cap height in pixels, and its width against the font's
SPLASH = (20, 1.0)                      # the splash's two lines
HEADER = (10, 0.92)                     # the main screen's top left: 8 % narrower, so it clears P03
BANNER_CAP = (64, 1.0)                  # 2x: shown 880 x 220
SLASH_CAP = (28, 1.0)                   # 2x: shown 14 px tall, a heading's mark
BANNER_SIZE = (1760, 440)
BG, ICE, ICE_D, RED = (6, 8, 12), (205, 230, 240), (84, 104, 116), (235, 28, 28)   # C_BG C_ICE C_ICE_D C_RED


def font(px):
    f = ImageFont.truetype(FONT, px)
    f.set_variation_by_axes([WEIGHT])
    return f


def line(text, size, slashes):
    """coverage arrays [row, column] 0..1, the letters' (white) and the slashes' (red), for a line at a size; the
    empty columns at either end trimmed"""
    cap, narrow = size
    cs = cap * SS                               # the cap in samples
    probe = font(400)
    b = probe.getbbox("H")
    f = font(max(1, round(400 * cs / (b[3] - b[1]))))
    hb = f.getbbox("H")
    x0 = round(cs * SL_TEXT) if slashes else 0
    tb = f.getbbox(text) if text else (0, 0, 0, 0)
    w = x0 + (tb[2] - tb[0]) + round(SLANT * cs) + 4 * SS
    white, red = Image.new("L", (w, cs), 0), Image.new("L", (w, cs), 0)
    if text:                                    # the cap's top on row 0, its foot on the last
        ImageDraw.Draw(white).text((x0 - tb[0], -hb[1]), text, font=f, fill=255)
        white = white.transform(white.size, Image.AFFINE, (1, SLANT, -SLANT * cs, 0, 1, 0), resample=Image.BICUBIC)
    if slashes:                                 # three parallelograms on the cap's height
        d = ImageDraw.Draw(red)
        for k in range(3):
            fx = k * SL_STEP * cs
            d.polygon([(fx, cs), (fx + SL_W * cs, cs), (fx + (SL_W + SL_LEAN) * cs, 0), (fx + SL_LEAN * cs, 0)], fill=255)
    if narrow != 1.0:
        white, red = (m.resize((round(w * narrow), cs), Image.BOX) for m in (white, red))
    W = -(-white.width // SS) * SS
    cw, cr = (np.asarray(m.crop((0, 0, W, cs)), np.float64) / 255.0 for m in (white, red))
    cw, cr = (m.reshape(cap, SS, W // SS, SS).mean(axis=(1, 3)) for m in (cw, cr))
    cr = np.minimum(cr, 1.0)
    cw = np.minimum(cw, 1.0 - cr)               # (a pixel's coverage never over 1)
    ink = (np.rint(7 * cw) > 0) | (np.rint(7 * cr) > 0)
    cols = np.nonzero(ink.any(axis=0))[0]
    return cw[:, cols[0]:cols[-1] + 1], cr[:, cols[0]:cols[-1] + 1]


def codes(cw, cr):
    """the screen's 4-bit codes: rows of 0..15"""
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
    hx, hy = x0 + w - 110, y0 - 56             # the shooting star over the line's end: the head lower left, the trail
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
    star = codes(*line("STAR", SPLASH, True))
    distroyer = codes(*line("DISTROYER", SPLASH, False))
    header = codes(*line("STAR DISTROYER", HEADER, True))
    with open(OUT, "w") as f:
        f.write("/* SPDX-License-Identifier: GPL-3.0-only */\n/* Copyright (C) 2026 Joakim (joakimskurk) */\n")
        f.write("/* Generated by tools/gen_logo.py (Orbitron Black, assets/fonts). Do not edit. */\n")
        f.write("#ifndef SD_LOGO_H\n#define SD_LOGO_H\n#include <stdint.h>\n\n")
        f.write("typedef struct { uint16_t w, h; const uint8_t *px; } SdLogo;   /* 4 bits a pixel, see the script */\n\n")
        emit(f, "SD_LOGO_STAR", star)
        emit(f, "SD_LOGO_DISTROYER", distroyer)
        emit(f, "SD_LOGO_HEADER", header)
        f.write("#endif\n")
    preview([header, star, distroyer])
    cw, cr = line("STAR DISTROYER", BANNER_CAP, True)
    banner(cw, cr)
    a = np.clip(cw + cr, 0, 1)                  # the line alone: ice and red at their coverage, on nothing
    rgb = (np.array(ICE, np.float64) * cw[..., None] + np.array(RED, np.float64) * cr[..., None]) / \
        np.maximum(cw + cr, 1e-9)[..., None]
    out = np.dstack([np.clip(np.rint(rgb), 0, 255), np.rint(255 * a)]).astype(np.uint8)
    Image.fromarray(out, "RGBA").save(LOGO_LINE, optimize=True)
    sw, sr = line("", SLASH_CAP, True)          # the slashes alone, red on nothing (GitHub's light or dark page)
    rgba = np.zeros(sr.shape + (4,), np.uint8)
    rgba[..., :3] = RED
    rgba[..., 3] = np.clip(np.rint(255 * sr), 0, 255).astype(np.uint8)
    Image.fromarray(rgba, "RGBA").save(SLASH, optimize=True)
    print("gen_logo -> %s\n  header %dx%d, splash %dx%d and %dx%d; preview %s\n  banner %s: %dx%d, the line %dx%d" % (
        os.path.normpath(OUT), header[0], header[1], star[0], star[1], distroyer[0], distroyer[1],
        os.path.normpath(PREVIEW), os.path.normpath(BANNER), *BANNER_SIZE, cw.shape[1], cw.shape[0]))


if __name__ == "__main__":
    main()
