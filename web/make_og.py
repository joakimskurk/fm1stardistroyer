#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Joakim (joakimskurk)
"""The page's pictures that are not screens, from the README's (tools/make_readme_art.sh, the logo from
tools/gen_logo.py only):

  web/og.png     the link preview (1200 x 630): the banner over three screens, at the screen's own pixels
  web/icon.png   the tab's icon (64 x 64): the red "///" on the screen's black

  python3 web/make_og.py
"""
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
ART = ROOT / "docs" / "readme"
BG = (6, 8, 12)                                 # C_BG
SCREENS = ("screen_splash.png", "screen_nav.png", "screen_turing.png")


def og():
    out = Image.new("RGB", (1200, 630), BG)
    banner = Image.open(ART / "banner.png").convert("RGB")
    w = 1120
    banner = banner.resize((w, banner.height * w // banner.width), Image.LANCZOS)
    out.paste(banner, ((1200 - w) // 2, 6))
    s, gap = 288, 28                            # 480 -> 288: the screens' pixels stay square, near enough
    x = (1200 - 3 * s - 2 * gap) // 2
    for i, name in enumerate(SCREENS):
        im = Image.open(ART / name).convert("RGB").resize((s, s), Image.LANCZOS)
        out.paste(im, (x + i * (s + gap), 310))
    out.save(ROOT / "web" / "og.png", optimize=True)


def icon():
    out = Image.new("RGBA", (64, 64), BG + (255,))
    mark = Image.open(ART / "slash.png").convert("RGBA")
    w = 54
    mark = mark.resize((w, mark.height * w // mark.width), Image.LANCZOS)
    out.alpha_composite(mark, ((64 - w) // 2, (64 - mark.height) // 2))
    out.save(ROOT / "web" / "icon.png", optimize=True)


if __name__ == "__main__":
    og()
    icon()
    print("og: web/og.png (1200 x 630), web/icon.png (64 x 64)")
