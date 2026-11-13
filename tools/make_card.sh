#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Joakim (joakimskurk)
# The Firmware Hub's card: the host build's screen (tests/scripts/card.txt) as docs/card.png, 240x240 as the FM-1
# shows it, then fmhub.json and the card checked against the hub's rules (thefirmwarehub.com/fmhub.md).
set -e
cd "$(dirname "$0")/.."
rm -f build/host/stardistroyer-*.png
sh tests/run_host.sh tests/scripts/card.txt >/dev/null
python3 - <<'PY'
import json, re
from pathlib import Path
from PIL import Image
shot = sorted(Path("build/host").glob("stardistroyer-*.png"), key=lambda p: int(p.stem.split("-")[-1]))[-1]
Image.open(shot).resize((240, 240), Image.NEAREST).save("docs/card.png", optimize=True)   # run_host.sh doubles it
j = json.loads(Path("fmhub.json").read_text())
assert set(j) <= {"what", "author", "kinds", "play", "editor", "cheat", "image", "color"}, "fmhub.json: an unknown field"
w = j["what"]
assert 20 <= len(w) <= 300 and not re.search(r"<|>|https?:|www\.|`", w), f"fmhub.json: what ({len(w)} characters)"
assert 2 <= len(j.get("author", "xx")) <= 60
assert j["kinds"][0] in ("synth", "groove", "drums", "chords", "tracker", "other")
assert re.fullmatch(r"#[0-9A-Fa-f]{6}", j["color"])
im, size = Image.open(j["image"]), Path(j["image"]).stat().st_size
assert im.size[0] == im.size[1] and 240 <= im.size[0] <= 1024 and im.format == "PNG" and size <= 300 * 1024
print(f"card: {j['image']} {im.size[0]}x{im.size[1]}, {size} B; what {len(w)} characters, kind {j['kinds'][0]}")
PY
