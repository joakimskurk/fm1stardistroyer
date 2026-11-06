#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Writes firmware/src/sd_tables.h: the VOICE operator's tables (Sound Plan v0.1 §3).

    SD_SIN       1024-point sine (+1 guard), Q15: the operator's lookup, linear interpolation
    SD_TRACK     the aliasing tracking (Sound Plan §3.5), Q15 by MIDI note: 1 up to the knee, then -6 dB an octave
    SD_WARP      the warp law, 128 steps (+1): triangle blend t (Q15), skew breakpoint d (Q16 of a cycle), squash
                 drive G (Q8). Vector Wave's path (manual v1.24: "0 = Sine, 40 = Saw, 99 = Pulse"; the triangle on
                 the way, as RYK's product text has it): sine -> triangle (20) -> saw (40) -> pulse (99)
    SD_CENTS     2^(c / 1200) for c = -99..+99 cents, Q16: fine tune and detune spread
    SD_FOLD_G    the fold law, 128 steps (+1): gain before the triangle fold, Q12 (4096 = 1.0 = no fold)
    SD_ENV_DEC   envelope decay/release coefficient per control tick (32 frames), Q31, 128 times 1 ms .. 10 s
    SD_ENV_ATK   envelope attack coefficient (toward 1.25, reaching 1.0 in the time), Q31, same times
    SD_ENV_MS    the same times in ms (the screen's ENV and FILTER pages: 1 .. 10000)
    SD_RG        the Rider's filter warp g = tan(pi f / fs) by semitone, MIDI 16..130 (+1), Q24
    SD_RK        the Rider's damping k = 1/Q by RES, 128 steps (+1): 2.0 (no resonance) .. 0.01 (rings), Q24
    SD_FK        the filter's damping by RES (2026-11-06), 128 steps (+1), Q24: a factor on its flat k (LP12 1.41,
                 LP24's second stage 0.77), 1.0 .. 0.0707 exponentially: even in dB of the peak, to +23 dB (LP12)
                 and +25 dB (LP24) at full, never self-oscillating
    SD_RLEVEL    the Rider's level law, 128 steps (+1), Q12: clean 0 .. 1 over the first half (2x), then 1 .. 4
                 exponentially into the soft clip (Night Rider: "the last 5 brighter LEDs … saturation clipping")
    SD_DGAIN     DESTROY's band gain before its shaper, 128 steps (+1), Q8: 0 .. +36 dB (1 .. 63 x), even in dB
    SD_DLOG      DESTROY's dynamics: 20 log10(1 + i / 64) in dB x 256, 64 steps (+1): a level's dB from its top bits
    SD_DEXP      and back: 2^(i / 64), Q14, 64 steps (+1): a gain from its log2's fraction
    SD_RMIX      the Rider's MIX law (R4), 128 steps (+1), Q15 pairs {dry, wet}: the dry whole to the middle and the
                 wet in on top, past it the dry fading; both x 1 / sqrt(dry^2 + wet^2), so the loudness holds
    SD_HARM_ST   the harmonics 1..32 as semitones x 256 above the fundamental (12 log2 h): the Rider's HARM snap
    SD_SEMI      2^(k / 12) for k = 0..11, Q16: the matrix's PITCH (C1), with SD_CENTS for the rest
    SD_LFO_INC   the LFOs' free rate (C1), 128 steps (+1): 0.02 .. 40 Hz, exponential, as a phase step a control tick
                 (2^32 a cycle); SD_LFO_HZ100 the same rates in Hz x 100, for the screen
    SD_SP_G      SPACE's reverb: a line's gain by DECAY, 128 steps (+1), Q15, for a 3500-sample line: T60 0.3 .. 20 s
                 (exponential in time); a shorter SIZE rings proportionally shorter
    SD_RVOWEL    the Rider's 13 vowel presets (Night Rider's list: ee i ay e au u a aw oo ar er ir iir): four formant
                 frequencies as MIDI x 256. Textbook adult male formants (Peterson & Barney 1952 and the like for
                 F1..F3, F4 at 3.5 kHz), approximate; not RYK's values, which the manual does not give

The laws are the starting point of V1; they are tuned by ear (python3 tools/gen_tables.py after a change)."""
import math, os

FS, SUB = 44100, 32
OUT = os.path.join(os.path.dirname(__file__), "..", "firmware", "src", "sd_tables.h")

# ---- the warp law: rows 0..26 sine -> triangle (t 0 -> 1), 26..52 triangle -> saw (d 0.5 -> D_MIN), 52..128 saw ->
#      pulse (G G_MIN -> G_MAX); even steps of timbre inside each. Row 26 reads 20 and row 52 reads 40 on a 0..99 dial.
ROW_TRI, ROW_SAW = 26, 52
D_MIN, G_MIN, G_MAX = 0.012, 0.05, 12.0
# ---- the fold law: the spec's regions (0-30 subtle, 30-65 obvious, 65-85 metallic, 85-100 extreme), gain at each edge
FOLD_EDGES = [(0.00, 1.00), (0.30, 1.18), (0.65, 2.40), (0.85, 4.50), (1.00, 10.0)]
# ---- aliasing tracking: gentle (Vector Wave is raw: its off-series partials sit only 15-33 dB down), from C6 up
TRACK_KNEE = 72                                  # C5 (C6 until the user heard the FM scream up high)
# ---- the Rider: its filters' range ("rumble" to "wispy", Night Rider's manual), its resonance law
R_LO, R_HI = 16, 130                    # MIDI 16 (20.6 Hz) .. 130 (14.9 kHz)
K_MAX, K_MIN = 2.0, 0.01
VOWELS = [                              # name, F1 F2 F3 F4 (Hz)
    ("EE", 270, 2290, 3010, 3500), ("I", 390, 1990, 2550, 3500), ("AY", 480, 2090, 2690, 3500),
    ("E", 530, 1840, 2480, 3500), ("AU", 660, 1720, 2410, 3500), ("U", 640, 1190, 2390, 3500),
    ("A", 730, 1090, 2440, 3500), ("AW", 570, 840, 2410, 3500), ("OO", 300, 870, 2240, 3500),
    ("AR", 700, 1220, 2600, 3500), ("ER", 490, 1350, 1690, 3500), ("IR", 440, 1020, 2240, 3500),
    ("IIR", 300, 2100, 2700, 3500)]
# ---- envelope times
T_MIN, T_MAX = 0.001, 10.0


NH = 64                                         # harmonics the evenness looks at


def cubic(u):
    """the squash's soft clip: u - u^3 / 3, flat at +-2/3 past |u| = 1 (smooth there: its slope is 0)"""
    u = max(-1.0, min(1.0, u))
    return u - u ** 3 / 3


def spectrum(t, d, g):
    """harmonic amplitudes 1..NH of one cycle (256 points) of the oscillator as the firmware computes it: the
    skew at d, the cosine mixed toward the triangle by t, the squash at drive g"""
    w = []
    for i in range(256):
        p = i / 256
        q = p * 0.5 / d if p < d else 0.5 + (p - d) * 0.5 / (1 - d)
        s = math.cos(2 * math.pi * q)
        s = s + t * ((4.0 * abs(q - 0.5) - 1.0) - s)       # toward the triangle: the same odd harmonics, at 1/n^2
        w.append(cubic(g * s) / cubic(g) if g > G_MIN else s)
    out = []
    for h in range(1, NH + 1):
        re = sum(x * COS[h * i % 256] for i, x in enumerate(w))
        im = sum(x * COS[(h * i + 192) % 256] for i, x in enumerate(w))
        out.append(math.hypot(re, im) / 128)
    return out


COS = [math.cos(2 * math.pi * i / 256) for i in range(256)]


def even(path, n):
    """n + 1 points along path (a list of parameter tuples) at equal steps of timbre change: the distance between
    harmonic spectra, each harmonic's difference weighted by its number (equal weight an octave, as the ear hears
    brightness), so the last bit of the skew -- a few strong high harmonics -- counts as much as the first bend"""
    specs = [spectrum(*pt) for pt in path]
    dist = [0.0]
    for a, b in zip(specs, specs[1:]):
        dist.append(dist[-1] + math.sqrt(sum(math.sqrt(h + 1) * (x - y) ** 2 for h, (x, y) in enumerate(zip(a, b)))))
    out, j = [], 0
    for k in range(n + 1):
        target = dist[-1] * k / n
        while j < len(path) - 2 and dist[j + 1] < target:
            j += 1
        t = 0.0 if dist[j + 1] == dist[j] else min(1.0, max(0.0, (target - dist[j]) / (dist[j + 1] - dist[j])))
        out.append(tuple(a + (b - a) * t for a, b in zip(path[j], path[j + 1])))
    return out


def warp_law():
    """129 rows (t, d, g): three stages, each at even steps of timbre so a sweep moves evenly, the hand-overs at the
    rows Vector Wave names"""
    m = 160
    tri = [(k / m, 0.5, G_MIN) for k in range(m + 1)]
    skew = [(1.0, 0.5 * (D_MIN / 0.5) ** (k / m), G_MIN) for k in range(m + 1)]
    squash = [(1.0, D_MIN, G_MIN * (G_MAX / G_MIN) ** (k / m)) for k in range(m + 1)]
    return even(tri, ROW_TRI) + even(skew, ROW_SAW - ROW_TRI)[1:] + even(squash, 128 - ROW_SAW)[1:]


def fold_gain(x):
    for (x0, g0), (x1, g1) in zip(FOLD_EDGES, FOLD_EDGES[1:]):
        if x <= x1:
            return g0 * (g1 / g0) ** ((x - x0) / (x1 - x0))
    return FOLD_EDGES[-1][1]


def env_time(i):
    return T_MIN * (T_MAX / T_MIN) ** (i / 127.0)


def rows(vals, per, fmt):
    out = []
    for i in range(0, len(vals), per):
        out.append("    " + ", ".join(fmt(v) for v in vals[i:i + per]) + ",")
    return "\n".join(out)


def main():
    sin = [round(32767 * math.sin(2 * math.pi * i / 1024)) for i in range(1025)]
    track = [32767 if n <= TRACK_KNEE else round(32767 * 2 ** (-(n - TRACK_KNEE) / 12)) for n in range(128)]
    wp = warp_law()
    wrows = ",\n".join("    {%du, %du, %du}" % (round(t * 32767), round(d * 65536), round(g * 256)) for t, d, g in wp)
    cents = [round(65536 * 2 ** ((c - 99) / 1200)) for c in range(199)]
    fg = [round(4096 * fold_gain(i / 128)) for i in range(129)]
    rg = [round((1 << 24) * math.tan(math.pi * 440 * 2 ** ((n - 69) / 12) / FS)) for n in range(R_LO, R_HI + 2)]
    rk = [round((1 << 24) * K_MAX * (K_MIN / K_MAX) ** (i / 128)) for i in range(129)]
    fk = [round((1 << 24) * 0.0707 ** (i / 128)) for i in range(129)]
    vowel = [[round(256 * (69 + 12 * math.log2(hz / 440))) for hz in v[1:]] for v in VOWELS]
    rlevel = [round(4096 * (2 * (i / 128) if i <= 64 else 4 ** ((i / 128 - 0.5) * 2))) for i in range(129)]
    dgain = [round(256 * 10 ** (36 * i / 128 / 20)) for i in range(129)]
    dlog = [round(256 * 20 * math.log10(1 + i / 64)) for i in range(65)]
    dexp = [round(16384 * 2 ** (i / 64)) for i in range(65)]
    rmix = []
    for i in range(129):
        m = i / 128
        dg, wg = min(1, 2 * (1 - m)), min(1, 2 * m)
        nrm = 1 / math.sqrt(dg * dg + wg * wg)
        rmix.append((round(32767 * dg * nrm), round(32767 * wg * nrm)))
    harm = [round(256 * 12 * math.log2(h)) for h in range(1, 33)]
    semi = [round(65536 * 2 ** (k / 12)) for k in range(12)]
    lfo_hz = [0.02 * 2000 ** (i / 128) for i in range(129)]
    lfo_inc = [round(f * SUB / FS * 2 ** 32) for f in lfo_hz]
    lfo_hz100 = [round(100 * f) for f in lfo_hz]
    spg = [round(32767 * 10 ** (-3 * 3500 / (FS * 0.3 * (20 / 0.3) ** (i / 128)))) for i in range(129)]
    tick = SUB / FS
    dec = [min(0x7FFFFFFF, round(2 ** 31 * math.exp(-tick * math.log(100) / env_time(i)))) for i in range(128)]
    atk = [min(0x7FFFFFFF, round(2 ** 31 * math.exp(-tick * math.log(5) / env_time(i)))) for i in range(128)]
    with open(OUT, "w") as f:
        f.write("/* SPDX-License-Identifier: GPL-3.0-only */\n")
        f.write("/* Generated by tools/gen_tables.py. Do not edit: change the laws there and run it. */\n")
        f.write("#ifndef SD_TABLES_H\n#define SD_TABLES_H\n#include <stdint.h>\n\n")
        f.write("static const int16_t SD_SIN[1025] = {\n%s\n};\n\n" % rows(sin, 12, str))
        f.write("static const uint16_t SD_TRACK[128] = {\n%s\n};\n\n" % rows(track, 12, lambda v: "%du" % v))
        f.write("typedef struct { uint32_t t_q15, d16, g_q8; } SdWarpRow;\n")
        f.write("static const SdWarpRow SD_WARP[129] = {\n%s\n};\n\n" % wrows)
        f.write("#define SD_WARP_ROW_TRI %du\n#define SD_WARP_ROW_SAW %du\n" % (ROW_TRI, ROW_SAW))
        f.write("static const uint32_t SD_CENTS[199] = {\n%s\n};\n\n" % rows(cents, 10, lambda v: "%du" % v))
        f.write("static const uint16_t SD_FOLD_G[129] = {\n%s\n};\n\n" % rows(fg, 12, lambda v: "%du" % v))
        f.write("static const uint32_t SD_ENV_DEC[128] = {\n%s\n};\n\n" % rows(dec, 6, lambda v: "%du" % v))
        f.write("static const uint32_t SD_ENV_ATK[128] = {\n%s\n};\n\n" % rows(atk, 6, lambda v: "%du" % v))
        f.write("#define SD_R_LO %d\n#define SD_R_HI %d\n" % (R_LO, R_HI))
        f.write("static const uint32_t SD_RG[%d] = {\n%s\n};\n\n" % (len(rg), rows(rg, 8, lambda v: "%du" % v)))
        f.write("static const uint16_t SD_ENV_MS[128] = {\n%s\n};\n\n" % rows([max(1, round(1000 * env_time(i))) for i in range(128)], 12, lambda v: "%du" % v))
        f.write("static const uint32_t SD_RK[129] = {\n%s\n};\n\n" % rows(rk, 8, lambda v: "%du" % v))
        f.write("static const uint32_t SD_FK[129] = {\n%s\n};\n\n" % rows(fk, 8, lambda v: "%du" % v))
        f.write("static const uint16_t SD_RLEVEL[129] = {\n%s\n};\n\n" % rows(rlevel, 12, lambda v: "%du" % v))
        f.write("static const uint16_t SD_DGAIN[129] = {\n%s\n};\n\n" % rows(dgain, 12, lambda v: "%du" % v))
        f.write("static const uint16_t SD_DLOG[65] = {\n%s\n};\n\n" % rows(dlog, 12, lambda v: "%du" % v))
        f.write("static const uint16_t SD_DEXP[65] = {\n%s\n};\n\n" % rows(dexp, 12, lambda v: "%du" % v))
        f.write("static const uint16_t SD_RMIX[129][2] = {\n%s\n};\n\n" % ",\n".join("    {%du, %du}" % r for r in rmix))
        f.write("static const uint16_t SD_HARM_ST[32] = {\n%s\n};\n\n" % rows(harm, 12, lambda v: "%du" % v))
        f.write("static const uint32_t SD_SEMI[12] = {\n%s\n};\n\n" % rows(semi, 12, lambda v: "%du" % v))
        f.write("static const uint32_t SD_LFO_INC[129] = {\n%s\n};\n\n" % rows(lfo_inc, 8, lambda v: "%du" % v))
        f.write("static const uint16_t SD_LFO_HZ100[129] = {\n%s\n};\n\n" % rows(lfo_hz100, 12, lambda v: "%du" % v))
        f.write("static const uint16_t SD_SP_G[129] = {\n%s\n};\n\n" % rows(spg, 12, lambda v: "%du" % v))
        f.write("#define SD_RVOWELS %d\n" % len(VOWELS))
        f.write("static const char *const SD_RVOWEL_NAME[SD_RVOWELS] = {%s};\n" % ", ".join('"%s"' % v[0] for v in VOWELS))
        for v, row in zip(VOWELS, vowel):       # the same as macros: patches' initialisers can use them
            f.write("#define SD_RV_%s {%s}\n" % (v[0], ", ".join("%du" % x for x in row)))
        f.write("static const uint16_t SD_RVOWEL[SD_RVOWELS][4] = {\n%s\n};\n\n" % ",\n".join(
            "    SD_RV_%s" % v[0] for v in VOWELS))
        f.write("#endif\n")
    print("gen_tables -> %s" % os.path.normpath(OUT))


if __name__ == "__main__":
    main()
