#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The README's pictures in docs/readme/: the banner and the headings' "///" (tools/gen_logo.py, the logo's only
# source), and the screens, the host build's own (tests/scripts/readme.txt), 480 x 480 (2x the FM-1's 240 x 240, for
# sharp screens). docs/readme/chain.svg, the signal chain, is drawn by hand.
#   tools/make_readme_art.sh          (needs Pillow and NumPy)
set -e
cd "$(dirname "$0")/.."
python3 tools/gen_logo.py >/dev/null
rm -f build/host/stardistroyer-*.png
HOST_DEFS=-DSD_HELP_MS=0 sh tests/run_host.sh tests/scripts/readme.txt >/dev/null   # (the hints, not the pages' first lines)
python3 - <<'PY'
from pathlib import Path
from PIL import Image
NAMES = {700: "splash", 2600: "nav", 3600: "macros", 4400: "reactor", 5200: "distroy", 6000: "space", 6800: "env",
         7700: "filter", 8500: "lfo", 11000: "radar", 11800: "mod", 12600: "seq", 13700: "arp", 17400: "orbit",
         20200: "setup", 21200: "time", 22100: "sub", 23000: "lfo2", 24700: "looper", 25700: "seqb"}
out = Path("docs/readme")
out.mkdir(parents=True, exist_ok=True)
for t, name in NAMES.items():
    Image.open(f"build/host/stardistroyer-{t}.png").convert("RGB").save(out / f"screen_{name}.png", optimize=True)
print(f"readme art: docs/readme/banner.png and {len(NAMES)} screens")
PY
