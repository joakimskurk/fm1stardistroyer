#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Star Distroyer's site (GitHub Pages, https://joakimskurk.github.io/fm1stardistroyer/): its page, its installer
in the browser, the firmware, the demos.

  index.html                       web/index.html with fm1pkg.js and fm1ota.js (SLOOP's, unchanged) and the metadata
                                   inlined: the installer (Web MIDI, Chrome or Edge), the demos, the controls, the links
  firmware/stardistroyer-VER.fwsc  the package the page installs (its SHA-256 in the page: a damaged download is never
                                   offered)
  audio/                           DERELICT, CINEMATIC and RANGE (tests/run_sd.sh's renders, the firmware's own engine), AAC,
                                   and each one's spectrum as a picture (WebP), low to high, left to right
  app/                             the player: the firmware as WebAssembly (web/app/build.sh: build/web/stardistroyer.wasm),
                                   its audio thread (worklet.js), its panel (player.js), the logo's font for the panel (and its licence)
  screens/                         the pictures (docs/readme: the logo's line, the screens, the signal flow; web/og.png
                                   and web/icon.png from web/make_og.py)
  .nojekyll                        (served as it is)

  tests/run_sd.sh                  (first: the renders)
  sh web/app/build.sh              (and the player)
  python3 web/make_site.py build/stardistroyer.fwsc VERSION OUT_DIR

The package must be an FM-1 one of Star Distroyer's (its identity FM-1_962XY) with its update loader in it (which the
installer talks to). The audio is encoded by ffmpeg, or afconvert on a Mac. After SLOOP's web/make_site.py (Copyright
(C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; isod89).
"""
import hashlib
import json
import re
import shutil
import subprocess
import sys
import wave
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F
ART = ROOT / "docs" / "readme"
PICTURES = {ART / "logo_line.png": "logo.png", ART / "slash.png": "slash.png", ART / "chain.svg": "chain.svg",
            HERE / "og.png": "og.png", HERE / "icon.png": "icon.png"}
PLAYER = {ROOT / "build" / "web" / "stardistroyer.wasm": "stardistroyer.wasm", HERE / "app" / "worklet.js": "worklet.js",
          HERE / "app" / "player.js": "player.js",
          ROOT / "assets" / "fonts" / "Orbitron-VariableFont_wght.ttf": "orbitron.ttf",
          ROOT / "assets" / "fonts" / "Orbitron-OFL.txt": "orbitron-OFL.txt"}
SCREENS = ("splash", "nav", "macros", "reactor", "distroy", "space", "env", "filter", "lfo", "mod", "seq", "turing", "orbit",
           "arp")
# the demos: each one FM-1 played by one person (tools/sd_play.h's rules); SINGULARITY a single take, DERELICT four
# such passes layered
DEMOS = [
    {"id": "singularity", "wav": "build/audio/singularity/star_distroyer_singularity.wav", "title": "SINGULARITY",
     "about": "One take on one FM-1, nothing layered: F minor at 72 BPM, POLY throughout. AURORA's glass looped, "
              "played backwards and frozen; VOWEL's voices over it, TURING moving their notes; then DISTROY's wall, "
              "ORBIT stepping the chords and the destroyed band, the loop an octave up; at the end the wall's plate "
              "frozen and the loop two octaves down."},
    {"id": "derelict", "wav": "build/audio/derelict/star_distroyer_derelict.wav", "title": "DERELICT",
     "about": "An overture, C minor at 84 BPM, four themes in four-bar turns: BRAAM's call; the cello far back in "
              "its own hall; PULSAR's staccato, its nav jumping across all four banks; BEACON's high line; the "
              "cello over the staccato; a pedal TURING rewrites, its top band crushed. Four passes on one FM-1."},
    {"id": "cine", "wav": "build/audio/cine/star_distroyer_cine.wav", "title": "CINEMATIC",
     "about": "A cue in D minor at 84 BPM, big, mighty and alien: NEBULA's chords and VOYAGE's alien line far "
              "back, PULSAR's 16ths, ION's bass and ORBITAL's taiko near, MONOLITH's and DISTROY's braams, a sine "
              "under it all, the macros stepping each bar. Eight parts layered as a multitrack."},
    {"id": "range", "wav": "build/audio/range/star_distroyer_range.wav", "title": "RANGE",
     "about": "The span in seven scenes, one patch each: NAV, FILTER, REACTOR, DISTROY, POLY, TURING and SPACE."},
]
# the spectrum's picture: 40 Hz .. 16 kHz on a log scale, 75 dB of range, in the screen's ice
SPEC_W, SPEC_H, SPEC_N = 1600, 240, 8192
SPEC_LO, SPEC_HI, SPEC_DB = 40.0, 16000.0, 75.0
ICE = [(0.00, (6, 8, 12)), (0.30, (13, 28, 40)), (0.55, (48, 84, 102)), (0.78, (143, 190, 210)),
       (0.92, (205, 230, 240)), (1.00, (255, 255, 255))]


def product_of(raw):
    """the package identity: one marker byte after each of the first 20 blocks (fm1pkg.js productOf)"""
    return "".join(chr((m - i - 1) & 0xFF) for i in range(BLOCKS) if (m := raw[i * BLK + KEEP]) != 0x7D)


def strip_module(src):
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def read_wav(path):
    with wave.open(str(path)) as f:
        if f.getsampwidth() != 2:
            raise SystemExit(f"{path}: not 16-bit")
        ch, fs = f.getnchannels(), f.getframerate()
        x = np.frombuffer(f.readframes(f.getnframes()), dtype="<i2").astype(np.float32)
    return x.reshape(-1, ch).mean(axis=1) / 32768.0, fs


def spectrum(x, fs, pic):
    """the piece's spectrum, left to right: a column a slice, the louder the whiter"""
    win = np.hanning(SPEC_N).astype(np.float32)
    x = np.concatenate([x, np.zeros(SPEC_N, np.float32)])
    hop = (len(x) - 2 * SPEC_N) / (SPEC_W - 1)
    freqs = np.fft.rfftfreq(SPEC_N, 1.0 / fs)
    edges = np.geomspace(SPEC_LO, SPEC_HI, SPEC_H + 1)
    lo = np.searchsorted(freqs, edges[:-1])
    cut = np.append(lo, np.searchsorted(freqs, edges[-1]))
    db = np.empty((SPEC_H, SPEC_W), np.float32)
    for c in range(SPEC_W):
        s = int(c * hop)
        m = np.abs(np.fft.rfft(x[s:s + SPEC_N] * win))
        db[:, c] = 20.0 * np.log10(np.maximum.reduceat(m, cut)[:-1] + 1e-9)   # each row: its bins' loudest
    ref = np.percentile(db, 99.8)
    t = np.clip((db - (ref - SPEC_DB)) / SPEC_DB, 0.0, 1.0)[::-1] ** 1.25   # high at the top; the floor darker
    rgb = np.zeros(t.shape + (3,), np.float32)
    for (t0, c0), (t1, c1) in zip(ICE, ICE[1:]):
        k = ((t >= t0) & (t <= t1))[..., None]
        f = ((t - t0) / (t1 - t0))[..., None]
        rgb = np.where(k, np.array(c0) + f * (np.array(c1) - np.array(c0)), rgb)
    Image.fromarray(rgb.astype(np.uint8), "RGB").save(pic, quality=84, method=6)   # (WebP: a fifth of a PNG)


def encode(wav, m4a):
    if shutil.which("ffmpeg"):
        cmd = ["ffmpeg", "-loglevel", "error", "-y", "-i", str(wav), "-c:a", "aac", "-b:a", "160k", "-movflags",
               "+faststart", str(m4a)]
    elif shutil.which("afconvert"):
        cmd = ["afconvert", "-f", "m4af", "-d", "aac", "-b", "160000", str(wav), str(m4a)]
    else:
        raise SystemExit("make_site: no ffmpeg or afconvert for the audio")
    subprocess.run(cmd, check=True)


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_962\d\d", product):
        raise SystemExit(f"{pkg}: identity {product!r} is not Star Distroyer's FM-1 one (FM-1_962XY)")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: no update loader in it: the installer could not write it")
    for d in DEMOS:
        if not (ROOT / d["wav"]).exists():
            raise SystemExit(f"{d['wav']}: missing (tests/run_sd.sh renders it)")
    for src in PLAYER:
        if not src.exists():
            raise SystemExit(f"{src.relative_to(ROOT)}: missing (sh web/app/build.sh makes the player)")
    name = f"stardistroyer-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    if out.exists():
        shutil.rmtree(out)
    for sub in ("firmware", "screens", "audio", "app"):
        (out / sub).mkdir(parents=True)
    shutil.copy(pkg, out / "firmware" / name)
    for src, dst in PICTURES.items():
        shutil.copy(src, out / "screens" / dst)
    for src, dst in PLAYER.items():
        shutil.copy(src, out / "app" / dst)
    for s in SCREENS:
        shutil.copy(ART / f"screen_{s}.png", out / "screens" / f"{s}.png")
    demos = []
    for d in DEMOS:
        x, fs = read_wav(ROOT / d["wav"])
        spectrum(x, fs, out / "audio" / f"{d['id']}.webp")
        encode(ROOT / d["wav"], out / "audio" / f"{d['id']}.m4a")
        demos.append({"id": d["id"], "title": d["title"], "about": d["about"], "src": f"audio/{d['id']}.m4a",
                      "spectrum": f"audio/{d['id']}.webp", "secs": round(len(x) / fs, 1)})
    meta = json.dumps({"version": version, "product": product, "pkg": "firmware/" + name,
                       "sha256": hashlib.sha256(raw).hexdigest(), "size": len(raw)})
    lib = strip_module((HERE / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1ota.js").read_text(encoding="utf-8"))
    html = (HERE / "index.html").read_text(encoding="utf-8")
    for mark in ("/*LIB*/", "/*META*/", "/*DEMOS*/"):
        if html.count(mark) != 1:
            raise SystemExit(f"web/index.html must hold {mark} once")
    html = html.replace("/*LIB*/", lib).replace("/*META*/", meta).replace("/*DEMOS*/", json.dumps(demos))
    (out / "index.html").write_text(html, encoding="utf-8")
    (out / ".nojekyll").write_text("")
    size = sum(p.stat().st_size for p in out.rglob("*") if p.is_file())
    print(f"site: {out}: index.html ({len(html)} B), firmware/{name} ({len(raw)} B, {product}), "
          f"{len(demos)} demos, {len(PICTURES) + len(SCREENS)} pictures, the player "
          f"({(out / 'app' / 'stardistroyer.wasm').stat().st_size} B), {size // 1024} KB in all")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:])
