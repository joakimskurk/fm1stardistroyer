# Star Distroyer — Sound Plan

**Status:** Plan v0.1, 2026-11-02. Companion to *FM1 Design & Feasibility v0.1*, sections 2–4, 13–15, and to *Star Distroyer — Sequencer Plan v0.2*.
**Priority:** sound is the current focus. This plan covers VOICE, REACTOR and DISTROY: how each is built on Dinghy,
in what order, and how we know it sounds right.
**Platform:** Dinghy (decided 2026-11-02).

---

## 0. What Dinghy changes in the feasibility spec

The feasibility spec was written against the FM-1 in general (stock firmware map, fm1_open). Dinghy sets
harder and more concrete limits. These override the spec where they disagree.

| Spec assumed | Dinghy reality (docs/API.md, docs/RULES.md, app.ld) | Consequence |
|---|---|---|
| "floating-point support" | **No `float`, no `double`**: no FPU, no soft-float library; the link fails | All DSP in fixed point: Q15 audio, Q31 phase and coefficients |
| — | **No 64-bit division** (`__udivdi3` missing); 64-bit *multiply* is one instruction | Divides only at control rate and only 32-bit; per sample: multiply and shift |
| 64-sample blocks, ~1.45 ms | **256 frames per interrupt, 5.8 ms**, 44.1 kHz | Render in 32-frame sub-blocks (§2.2); budget per interrupt is "under half": **2.9 ms** |
| Two cores: Core A voice, Core B post | One audio interrupt (`app_audio`); Dinghy exposes no second core to the app | Plan for **one core**. A second core is a bonus to explore in Phase 0, not a dependency (S3: SPACE's plate runs there, with the room on one core as its fallback) |
| Audio kernels in RAM | Code runs from XIP flash; `.ram_text` (24 KB) is the flash driver, 8 KB free for RAM-run code | Hot kernels into the 8 KB RAM window if the cache misses show up in measurements |
| "hundreds of KB SRAM" | **96 KB** `.data/.bss` + **336 KB pool** for big buffers + 24 KB stack | Delay lines and tables in the pool; plenty for Reactor and feedback buffers |
| Fold: `repeat while outside [-1,+1]` | **No unbounded loops** in the audio callback | Fold in closed form (§3.4) |
| — | Flash erase blocks interrupts for tens of ms | Save patches only when silent, as Dinghy does |

**What the platform teaches:** a dense synth on Dinghy, everything on, measures 174 M instructions/s =
72.6 % of the chip in fm1-emu (which counts instructions, not cycles), and a per-sample path gets fast only once all
divides are removed (some 70 a sample) and modulation moves to a control rate. Star Distroyer starts with those rules
instead of discovering them.

---

## 1. Working method: hear it on the host first

There is no browser model to keep in step with the C, so the C is the reference: **one fixed-point DSP codebase**, built for two targets.

```text
dsp/*.c  (fixed point, no float, no 64-bit div)
   │
   ├─► host build ──► render tool ──► WAV files      ← sound design happens here, seconds per iteration
   │                    └─► cpu_bench (host)          ← relative cost of each block
   │
   └─► Dinghy build ──► fm1-emu (instruction count)   ← budget gate
                    └─► FM-1 (CPU nn % on screen)      ← final cycles, when hardware is used
```

- **Render tool:** a patch file + a note list or a fixed 16-step loop → stereo WAV. Same code as the firmware.
- **Identity patch set:** eight patches (§7) rendered on every change, kept as `build/audio/*.wav`, so a change is
  heard A/B against the last good version. Golden renders catch accidental changes.
- **Laws are data:** warp, fold, resonance, drive and PHAT/SHARP curves are generated tables (`tools/gen_*.py`),
  so tuning a curve is editing a script and re-rendering.
- **Sound decisions are written down** in `docs/DECISIONS.md` with the date and what was heard.

---

## 2. Numeric and timing conventions

### 2.1 Formats

| Signal | Format | Note |
|---|---|---|
| Audio between blocks | Q15 in `int32_t` | Headroom above ±1.0 inside a block; clipped only at the end |
| Oscillator phase | 32-bit unsigned, wraps | `inc = f · 2^32 / 44100`, from a table (Dinghy's octave table) |
| Filter states, coefficients | Q31 or Q28 | 64-bit multiply, shift back |
| Parameters from UI/mod | 0..65535 (Q16 unipolar) or Q15 bipolar | Mapped to coefficients at control rate |
| Pitch for interpolation | log pitch, 1/256 semitone | Reactor states and nav motion interpolate in log pitch, not Hz |

`q15mul` of two full-scale values overflows: every multiply site documents its range, and the
host tests run at extreme parameter values.

### 2.2 Rates

- **Audio rate:** 44.1 kHz. Operators, filters, shapers, delay lines.
- **Control rate:** every **32 frames** (1378 Hz). Envelopes, LFOs, mod matrix, nav weights, Reactor state
  interpolation and slew, filter coefficient targets, dynamics gains. Coefficients are ramped linearly across the
  32 frames where zipper noise is audible (cutoff, resonance, levels).
- **Sub-blocks:** each 256-frame interrupt renders as 8 sub-blocks of 32. Note events from the sequencer or MIDI
  start at a sub-block boundary, so timing jitter is ≤ 0.73 ms instead of 5.8 ms. One loop gives both.

### 2.3 Per-sample rules

No divides, no branches on parameters inside the inner loop (switch once per sub-block), no table larger than it needs
to be (XIP cache), no loops whose length depends on a parameter.

---

## 3. VOICE — nav / harmonic FM

### 3.0 The voice's design (decided 2026-11-03)

The user's rule: when a choice is open, follow this design. What it settles:

| What | Here |
|---|---|
| 16 sine oscillators in banks A–D of four | the same |
| An oscillator is a harmonic 1–32 of the note, with a fine tune | harmonic 1–32, fine ±99 cents |
| Each oscillator is a carrier, a series modulator or a parallel modulator, read left to right | S feeds the next to its right, P the nearest carrier to its right |
| The FM amount is the modulator's level, ENV2 and XM together | index = level × ENV2 × XM; 4π at all three full |
| ENV1 the amplitude, ENV2 the FM amount | the same |
| WP, a continuous wave shape: sine → triangle → saw → pulse | sine 0 → triangle 20 → saw 40 → pulse 99 |
| FD, a fold of the oscillators' waveforms | in each oscillator, before it modulates the next |
| The Q page (XM, WP, FD, DT) acts on all banks; WP and FD per bank through a modulation slot's offset | WP and FD per bank, the same for its four oscillators |
| DT spreads the four oscillators' tuning | the four at −21/−7/+7/+21 cents at 99 |
| No feedback operator | none: V1's feedback is gone |
| Generators: ALL, ODD, RND, CLUST, CHRD (14 chords), CPY TO | the generator list of §3.7 |
| Modes: one voice with all banks on the nav, POLY (bank A on four voices), a bank a voice | §3.6; DUO is the user's addition |
| Per bank: AM, FM, XM, WP, HM (harmonic shift of modulators), DT, PN (pan), FD, BF/LF (harmonic band/low filter) | modulation destinations for C1; BF/LF are the harmonic mask |

Decided here as well:
- The order inside the oscillator: warp, then fold.
- Fold acts on the modulators as well as the carriers.
- A P modulator feeds only the nearest carrier. PPCC (1 and 2 in parallel on 3) and SCSC (two chains) are the
  routings to check it against.

The target character, in numbers:

| What | Target | Star Distroyer V2 | So |
|---|---|---|---|
| Spectral centroid, median of 0.5 s windows | 400–620 Hz | GROWL's loop ~1400, BELL's ~860, GLASS's ~150 | the target sits lower and darker; it is the patches and the register, not the engine (next row) |
| Long-term spectrum above 1 kHz | about −4.5 dB/octave: −8 at 1k … −21 at 8k … −29 at 16k | about −4 dB/octave | the same slope: no output filter |
| Harmonics in clear notes | out to 10–15 kHz at −60 dB | the same | not low-passed |
| Strongest partial off the harmonic series, 2–15 kHz, in the brightest clear notes | −15 to −33 dB | BELL −45 dB | the target is rawer: aliasing, detune or chords. V3's aliasing tracking (§3.5) stays gentle |
| Stereo, side / mid | 0.2–0.4 (to 0.77 in organ stops) | 0 (mono) | V3: pan per bank (PN) |
| An organ stop | harmonics 1 and 3 only, h3/h1 0.67–0.84 steady | ORGAN, h3/h1 0.70 | added as a patch |
| Clean saw / square / triangle | rare; harmonics falling faster than 1/n | the warp's exact anchors | the anchors stand |

These numbers cannot settle the warp law or the fold order; the list above does.

### 3.1 Oscillator kernel

```text
phase += inc                                   32-bit wrap; inc = note x harmonic x 2^(cents/1200)
p  = phase + PM in                             from the oscillators to the left (true PM)
s  = shape(p, WP)                              cosine (table) -> triangle -> skewed triangle (saw) -> squash (pulse)
s  = fold(s, FD)                               closed-form triangle fold, §3.4
-> s x level, to the output (C) or to a modulator's target (S, P)
```

A bank renders oscillator 1 → 4. Each oscillator does its 32 frames in one loop, and the PM passes through a
32-frame bus per oscillator. WP and FD are one per bank, so the kernel's variant is picked once per bank per
sub-block. There are eight variants:
- sine
- sine → triangle
- triangle/saw
- squash
- each of the four with or without fold

### 3.2 Warp law

Three stages, each at even steps of timbre: the harmonic spectra, weighted by √h. The hand-overs are at the
anchors of §3.0.
- **0 → 20: sine → triangle.** The cosine mixed toward the triangle 4|q − ½| − 1. Both have only odd harmonics, and the
  triangle's fall as 1/n². So this mix is a bend, not the sine/saw/pulse crossfade the spec forbids. An earlier
  version bent the phase instead and left a seam at 20: the table could not follow the triangle's sharp peaks.
- **20 → 40: triangle → saw.** The phase is skewed at a breakpoint d from 0.5 down to 0.012. The triangle tilts into
  a saw, computed in closed form with no table.
- **40 → 99: saw → pulse.** tanh(G s) / tanh(G), with G rising from 0.05 (a no-op) to 12. 1/tanh(G) is computed
  for each sub-block.

### 3.3 Routing and roles

Roles left to right, as in §3.0. A modulator with no oscillator to its right, or with no carrier to its right for P,
is silent. ALGORITHM steps through SSSC PSSC PPPC SCSC PPCC CSSC SCCC CCCC.

### 3.4 Fold

Closed form, bounded time: a triangle wrap of the gained signal.

```text
x = s · gain(FD)                gain from a table: the spec's 0–30 / 30–65 / 65–85 / 85–100 law (1.0 .. 10)
fold(x) = triangle(x)           period 4: reflect via a mask and one subtraction, no loop
```

### 3.5 Aliasing without oversampling

At 44.1 kHz with no oversampling, FM, warp and fold all alias. The countermeasures are cheap and decided per note at
control rate:

- **Ratio ceiling:** an operator whose `ratio × f0` passes ~16 kHz fades out over an octave.
- **Index tracking:** PM index is scaled down with pitch above a knee (the patch sets the knee).
- **Fold and warp tracking:** fold gain and squash drive scale down with pitch the same way.

High notes get slightly gentler; low and mid notes keep their full grit. This is the single biggest difference
between "digital and harsh" and "digital and good" on this hardware, and it is tuned with the identity set in V3.

### 3.6 Banks, nav, stereo

- **Voice modes (decided 2026-11-03):** always 16 operator kernels a sample; the mode divides them among voices, so
  every mode costs the same CPU.

  | Mode | Voices × operators | Banks per voice | Nav |
  |---|---|---|---|
  | **MONO** | 1 × 16 | A B C D | full XY field |
  | **DUO** | 2 × 8 | A B | X only (A ↔ B), Y free for modulation |
  | **POLY** | 4 × 4 | A | none: one bank, morph by mod only |
  | **POLY8** (candidate) | 8 × 2 | A, operators 1–2 | none: a 2-op pair; kept only if it sounds like more than a toy |

  Each voice has its own note, ENV1 and ENV2. Patch, Reactor and Distroy are shared, since they sit after the voices.
  The allocator takes, in order:
  - the same note again
  - the oldest silent voice
  - the oldest released one
  - the oldest held one

  A stolen voice restarts its envelopes from where they are, so it does not click. A mode change releases every
  voice and switches once all are quiet. The sequencer's notes go through the same allocator. Built in V3; POLY8 is
  not built.
- **Nav mix:** bilinear weights from X/Y, normalized to equal power (`w_i / sqrt(Σ w²)`, the root from a table) at
  control rate. Gain law tuned in V3 (the spec's "energy hole").
- **Stereo:** banks A/D left of centre, B/C right, by a WIDTH parameter, spread round the middle of what sounds: a
  patch may lean, never hard left or right. The nav mix's own lean (its weights' energy) is the middle: a corner's
  bank alone plays there, banks mixed spread round it, the far side down to a half at the most (6 dB); in POLY, where
  each voice is a bank, half that (2.5 dB: each note near the middle, a chord a little wide). WIDTH 0 is mono.

### 3.7 Harmonic generators and mask

Generators, the list of §3.0 first: ALL, ODD, RND, CLUST, CHRD (14 chords), CPY TO. They run in the
main loop when pressed and write harmonics, levels and roles, at no audio cost. The spec's EVEN, SPREAD, METAL and
BELL come after them, if they are wanted. The harmonic mask is a per-operator weight from its harmonic number (tilt and
odd/even emphasis), applied at control rate on operator levels — it shapes which building blocks contribute, never the
audio after the bank (spec §2.6).

### 3.8 Envelopes and LFOs

Two envelopes: ENV1 on the level, ENV2 on the FM amount (XM). They are exponential segments from a generated Q31 table.
Two LFOs. All at control rate; AMP envelope ramped per sample across the sub-block.

---

## 4. REACTOR — quad morphing resonator

### 4.0 The Reactor's design (decided 2026-11-04)

| What | Here |
|---|---|
| Four filters in parallel (the allpass modes in series), one type for all: 6 dB BPF, 12 dB BPF, variable-width BPF, KP comb, AP1 (delay), AP2 (IIR) | the same; R1 has the three BPFs, R2 the rest |
| Six stages; each holds every filter's frequency and level; pan is a filter's own | the same |
| POS moves through the stages; with no slew it jumps, with slew it morphs | SLEW 0 jumps stage to stage, above it POS interpolates in log pitch and the filters glide |
| RES, SLEW, a global frequency offset, key tracking | RES, SLEW, FREQ ± 48 semitones, key tracking of the last note |
| A stage clock: division, range, FWD / BWD / ping-pong / random | the same, clocked by the note driver's steps |
| A level that runs into saturation at its top | level to 2×, past 1 a soft clip on each filter |
| Presets into a stage: 13 vowel formants (ee i ay e au u a aw oo ar er ir iir), 13 chords | the vowels as textbook formants; chords as patches use them |
| A dry control | MIX dry ↔ wet, so 0 is no Reactor at all (the patches before R1 are bit for bit unchanged) |

### 4.1 Signal path

```text
VOICE L/R ──► sum to mono ──► DRIVE in ──► 4 resonators (parallel) ──► pan each ──► + DRY L/R ──► out L/R
```

Resonators run **mono, panned after**, as the spec recommends: half the cost and memory, and four panned resonators
make a wide image of their own. Whether the dry path should stay stereo or also collapse is a listening decision (R3).

### 4.2 States, position, slew

- 6 states × 4 resonators × (log pitch, level, pan, model value).
- `POS` 0.000–5.999 picks two neighbouring states and a fraction; values interpolate in log pitch.
- **Slew** is a one-pole per resonator on the *target* (log pitch and level), at control rate, so a sequenced POS
  jump can be a snap or a glide.
- Coefficients from slewed targets at control rate, ramped across the sub-block.

### 4.3 Models

| Model | Implementation | Cost / sample / resonator |
|---|---|---|
| BPF (resonant) | TPT state-variable filter in Q31; `g = tan(πf/fs)` from a table; `1/(1+g(g+k))` once per control tick (32-bit divide, allowed there) | ~15 cycles |
| COMB / KP | int16 delay line in the pool, fractional read (linear first; allpass interpolation if tuning drifts at high notes), one-pole damping, feedback | ~25 |
| DELAY AP | the same delay line as an allpass (flanger-like, the pitch "skating" kept) | ~25 |
| IIR AP (phaser) | two first-order allpass stages per resonator, notch at its frequency | ~15 |

**Memory:** lowest comb pitch 30 Hz → 1470 samples × int16 × 4 = **11.8 KB** in the pool (the spec's 23 KB figure was
32-bit). One buffer per resonator shared by COMB and DELAY AP.

### 4.4 Resonance, safety, drive

- A soft saturator (table) inside each resonator's feedback path: high resonance rings and pings but cannot run away,
  and drive pushes the rings into saturation — the Reactor's own nonlinear layer before DISTROY.
- Level law: clean → drive → saturation in the upper range (spec §3.8), one generated table.
- Gain compensation per model so switching BPF ↔ COMB does not jump in level.

---

## 5. DISTROY — multiband destruction

### 5.0 DISTROY's design (decided 2026-11-04)

- four frequency bands
- each band in a fixed order: an M/S matrix, a gate, dynamics, distortion, make-up gain and pan, and a feedback stage
- dynamics with SHAPE (a low-level compressor one way, an expander that emphasises transients the other) and BODY (a
  threshold with a limiter)
- BIAS, a DC offset before the distortion; per band a GAIN, a type, an ALT and a mode
- feedback per band

Here: the four bands and that order. Dynamics (PHAT↔SHARP, BODY) come in D2 and feedback in D3; M/S is deferred
(spec §4.8). NONE is a type with no distortion. The crossover type is decided by measurement
below.

### 5.1 Crossover: complementary by construction

```text
LP1 = lowpass(x, f1)   LP2 = lowpass(x, f2)   LP3 = lowpass(x, f3)

band1 = LP1
band2 = LP2 − LP1
band3 = LP3 − LP2
band4 = x   − LP3

Σ bands = x   exactly, with any filters
```

Three 2-pole lowpasses per channel, stereo: 6 SVFs. The sum is exact by construction (the spec's `sum(bands) ≈ input`
at no extra cost). The slopes overlap more than Linkwitz–Riley, so bands bleed into each other a little — for
destruction arguably more musical. If the bands sound smeared in D1, LR4 with allpass compensation is the fallback, at
roughly twice the filter cost.

### 5.2 Per band

```text
band ─► DYNAMICS: PHAT ↔ SHARP (SHAPE) and its threshold (BODY), a gain from the band's own follower in dB
        (built in D2: a gain folded into GAIN's, ramped through the sub-block)
     ─► + BIAS
     ─► SHAPER (SOFT · HARD · FOLD · CRUSH in V1, each with its ALT)
     ─► DC block ─► LEVEL / PAN
```

- The follower runs per sample (one-pole on |x|, cheap); the gain curves are read at control rate and ramped.
- **V1 shapers and their ALT:** SOFT (asymmetric tanh table, ALT = hardness), HARD (clip, ALT = knee), FOLD (closed form
  as §3.4, ALT = spacing), CRUSH (mask bits, ALT = depth). One `switch` per band per sub-block.
- No oversampling (spec Tier 2 cut). Each band is already band-limited by its crossover, which bounds aliasing in the
  low bands; the top band aliases most, and TAME is the answer there.

### 5.3 Feedback

```text
Σ bands (wet) ─► short delay (≤ 20 ms, int16, pool, L/R lengths differ by SPREAD) ─► one-pole ─► soft clip ─► × FB ─► crossover input
```

FB is bounded below 1.0 in the table; a DC blocker and the soft clip sit inside the loop; a safety limiter after
TAME stays inaudible until the system is already unstable (spec §4.7). Memory: 2 × 882 × 2 B ≈ 3.5 KB.

### 5.4 TAME

Bypass at zero, then a tilt toward a 2-pole lowpass with a little resonance at the top of the range. One SVF per
channel.

### 5.5 SPACE: a delay into a reverb (decided by the user)

The spec keeps a large-reverb SPACE optional: "if Space compromises any of [the core] … Space is removed". The user's
decision: a delay inside the reverb, one block, called SPACE, last in the chain.

```text
DISTROY out (L/R) ──┬─► dry ─────────────────────────────────────────────┐
                    ├─► DELAY (stereo or ping-pong; TIME free or synced; │
                    │   FDBK with TONE and a soft ceiling in the loop) ──┼─► x D-LEVEL ─┐
                    └─► + the echoes ─► REVERB (4 allpass diffusers →    │              ├─► out
                                        4-line FDN, Hadamard; SIZE,      │              │
                                        DECAY, TONE) ───────────────────► x R-LEVEL ───┘
```

The echoes go into the reverb, so they reverberate too. The delay's line is up to 1 s stereo; the reverb runs at
half rate, its four lines up to 2048 half-rate samples (92 ms): the room, now the plate's fallback (§5.5.1, §5.5.2).
The memory is in the pool: the app lends it to the engine. It rests when nothing comes in and its tails are under the
floor. Both levels at 0: no SPACE.

#### 5.5.1 The plate (decided by the user)

A raw, long reverb, chosen by ear against the room. It takes the room's place and its four fields (SIZE, DECAY, TONE,
R-LEVEL keep their meaning, so a saved patch loads as it was); the room plays where the second core cannot (§5.5.2).
A prototype stuttered and clicked: its second LFO jumped its phase, and its half rate went in and out by a straight
line.

```text
in (the dry + the echoes, mono) ─► half-band ↓2 ─► lowpass ─► 4 diffusers ─┐
                                                                            ▼
   ┌─► allpass (its length moved by an LFO) ─► delay ─► damping ─► x decay ─► allpass ─► delay ─► x decay ─┐  left
   │                                                                                                       │
   └── the right branch's end ◄─────────── (SHIMMER: a share an octave up) ◄───── the left's end ──────────┘  right
   out: seven taps a side across both branches ─► half-band ↑2 ─► x R-LEVEL
```

- *Half rate* (22.05 kHz), a 23-tap half-band in and out: flat to 8 kHz, -57 dB past 15 kHz (a straight line
  between the outputs let images through, a fizz). The demos' SPACE alone (no dry): fewer sharp
  spikes than the room's (RANGE: 113 against 123), the same top over 12.5 kHz.
- *SIZE* 0.35 .. 1.75 x of the lines, gliding (a 256th of x 1 a tick at most); read between samples only while it
  moves (a straight line), at whole samples while it is still.
- *DECAY* is a T60 (`SD_PL_T60`, gen_tables.py: 0.3 .. 20 s as the room's, then up to 60 s at the top), whatever the
  SIZE: each pass of a branch is down by 10^(-1.5 k span / T60), k 1.07. Measured at 700 Hz: the law within a fifth
  up to ~9 s; the longest ~27..31 s of the nominal 53..60 (a tail's last LSBs go to the rounding, as to a noise floor).
- *Modulation*: two LFOs (~0.7 and ~0.87 Hz, each its own phase) move each branch's first allpass up to 12 samples x
  SIZE. It is what keeps a long tail from ringing (the room's tails peaked 61..65 dB over their neighbours, the
  plate's 30..40).
- *FREEZE* (ALGORITHM on SPACE, the player's): the input fades out over ~10 ms, the decay and the damping go to 1
  exactly, SHIMMER off, the LFOs glide out over ~0.2 s to a whole sample (and back after): the tail holds (-0.6 dB
  in 7 s) and nothing new gets in.
- *SHIMMER* (s_mode's bits 2..7, SCL + ALGORITHM): the branches' ends, an octave up (two heads crossfading in a
  93 ms window), take up to 0.2 of the way round, inside the loop: (1 - s) and s never pass more than the decay, so
  it cannot run away (an input shimmer, and an equal-power mix, both did at the longest DECAY); the decay is raised
  by the power the mix loses, to 1 at most.
- *Level*: x 1.11 at the taps, so R-LEVEL is as loud as the room's was (the factory patches' wets, a chord and its
  tail: the median 0 dB, -4.8 .. +2.3).
- *Memory*: the lines 61 KB (room for 1.75 x and the modulation) next to the delay's 173 KB: the pool 10 KB from full.
- *CPU* (fm1-emu): the first build, a sample at a time with a cubic between samples in every line and 64-bit
  products, took the gate from 41.2 % to 61.7 %. Now: whole samples while SIZE is still, a straight line only while
  it moves and in the modulated allpasses (FREEZE glides their LFOs out to a whole sample, so the held tail keeps:
  -0.6 dB in 7 s), 32-bit products with Q14 coefficients, the LFOs once a tick, a 23-tap half-band, the block of a
  call stage by stage (bit for bit the sample-at-a-time plate): the gate 48.7 %, everything at once 56.3 % (49.1 with
  the room). Of the plate's 17 M instructions/s, the tank ~10, the diffusers ~4.5, the taps ~3: fewer taps or
  diffusers are what is left to take, and they would change its sound.
- *The second core* (S3): the plate runs as jobs on the FM-1's second core, the room (S1's, bit for bit) its
  fallback. See §5.5.2.
- Tests: `test_plate` (T60 at 700 Hz within 25 % at DECAY 36000 and 48000, SIZE 0 and 65535; SIZE swept the whole
  way and back under a chord: no click; FREEZE held, a tone kept out, then it dies; SHIMMER at its most on the
  longest DECAY: there at 5 s, under its top at 30 s); `tests/scripts/p1.txt` (the hint, FREEZE's tag, SHIMMER).

#### 5.5.2 The plate on the second core, the room as its fallback (S3)

- *Jobs:* the plate's input is queued at half rate, with each tick's settings in a chunk (`SdPlateP`: its lengths,
  taps, gains and SIZE's glide as core 0 left them). At the end of each block of 128 samples (256 frames, one audio
  interrupt) the block is handed to the second core (`pl_post`), which runs it between interrupts; the next
  interrupt waits for it (`pl_join`) before it reads its outputs, a block late. Run later or at once (a computer: the
  host, the page's player): the same sound bit for bit (`test_plate_jobs`). On the FM-1 the plate never runs on core
  0 (core 0 keeps the original sound): a block not handed over or not back is dropped, its tail fading out over the block (`sd_plate_drop`), and the room plays from the next tick.
- *The platform:* `firmware/hal/fm1_cpu1.h`, `fm1_cpu1.S`, `fm1_cpu1_stop.h`, taken from felucca-bsp (MPL-2.0) and
  adapted to Dinghy: the second core starts at boot before the audio; its waiting loop runs from RAM; each job carries
  a check word (the function, the argument and the job's number), so a job read wrong is never run; the flash goes
  off (a save) only while it idles (`irq_save` waits).
- *Units that misread:* on some FM-1s the second core reads RAM wrong now and then (bit 21). Its waiting loop reads
  the mailbox millions of times a second, so a unit like that shows it there first, and at once. Then, or at a job
  not back in 8 ms, or a fault: the core is held until the power is off (a bar in `.noinit`; a new build lifts it),
  the block is dropped, and from the next tick the room plays in the plate's place (`sp_room`): ROOM on SPACE's
  screen, *2ND CORE OFF: ROOM* once. A crash of core 0 while the second core runs bars it too, so the FM-1 comes back
  on one core.
- *A word read wrong in a job:* what the job reads to place a write is kept inside its line, its memory is a
  constant from the caller (never a pointer from RAM), and its gains and state are kept in range (a gain read 2^21
  too big blew the tank up for its whole T60: in `test_plate_jobs`, 86 words read wrong in 0.5 s left the tail
  +22..30 dB over the clean one's, for good; kept in range, -0.1 dB). A line's sample read wrong is off by 32 at most.

#### 5.5.3 TIME: the delay on its own page, and a looper (S4, decided by the user)

The delay stays §5.5's (every subdivision, its 1 s, on core 0, bit for bit as before), on a page of its own, TIME,
beside SPACE under FX. The looper is separate: it loops and pitches what SPACE hears, into the reverb as the synth
goes (the fallback too), while the delay goes on, so a loop layer builds in the background. (A tape in the delay's
place, with a LOOP of its own, was tried first and dropped: the delay sounded better.)

```text
core 0:  dry ─► DC out ─► DELAY ─► echoes ─┬─► x D-LEVEL ─────────────────────────────────────────────┐
                                           ├─► (dry + echoes x D-LEVEL) ─► half-band ↓2, each side ─┐ │
                                           └─► (dry + echoes) mono ──────► half-band ↓2 ───────────┐│ │
core 1:  LOOPER (records, or plays its LENGTH at its SPEED) ─► + plate's way in (as the synth's) ◄─┘│ │
         PLATE ─► x R-LEVEL ─┬─ + LOOPER x L-LEVEL ─► half-band ↑2, each side ─────────────────────┘ ├─► out
core 0:  dry ───────────────────────────────────────────────────────────────────────────────────────┘
```

- *The pages:* FX goes REACTOR, DISTROY, SPACE, TIME. SPACE is the reverb: SIZE DECAY TONE R-LEVEL, SCL: SHIMMER
  (as SCL + ALGORITHM), ALGORITHM: FREEZE. TIME is the delay: TIME FDBK TONE D-LEVEL; SCL: the looper's GLIDE,
  LENGTH, SPEED and L-LEVEL, SCL + EDIT the delay's MODE (FREE SYNC, PP: it was SCL + K1 until GLIDE came); ALGORITHM:
  LOOP. TONE is now two (the delay's, the reverb's: s_dtone and
  s_rtone, which the one knob kept equal). EDIT: SPACE's is the reverb's, TIME's the delay's and the looper's (a
  patch saved with SPACE off before plays its delay now).
- *The looper* records, always, what SPACE plays before the reverb: the dry and the echoes at D-LEVEL, each side at
  half rate (as the plate, through the same half-band), into its own memory (int16, two channels). LOOP: it stops
  recording and its last LENGTH goes round from its start, faded in over 12 ms, crossfaded where it comes round (the
  10 ms before its start fade in: they run on into it), at its SPEED (-12 .. +12 semitones, an eighth a detent: the
  pitch and the length together, a tape's, gliding), held; off, it fades out over 46 ms and records again. LENGTH
  as TIME (free 1 ms .. 1 s, or synced, 1/32 .. 1/2 and 1, 2 and 4 bars), halved until it fits its memory (red, and
  the hint says what plays). L-LEVEL is its level out; it goes into the reverb's way in as the synth goes (mono,
  the same share), so R-LEVEL is its send too. REVERSE (SCL + ALGORITHM on TIME) turns its head
  round: it plays the loop backwards, before it loops or while it does, round
  its start through the same crossfade (one curve either way). The player's, not saved (as FREEZE).
- *Its grid* (an octave finds it again, over a glide or at once: SCL + OCT the octave, GLIDE on SCL + K1, MODE on
  SCL + EDIT; where a glide cannot get there, a crossfade into its place). Core 0 plans the head a tick (16 of its samples) at a time, the job reads where it says (a tick's start
  and its way: what it reads stays inside the loop). The loop's grid is its own: where it would read had it played
  at x 0.5, 1 or 2 since LOOP, from its start (backwards: from where REVERSE turned it). **GLIDE** is how long a new
  SPEED takes (OFF: at once; synced LENGTH's divisions, 1/32 .. 4 bars; free 0 .. 4 s), a smoothstep in the rate.
  **SCL + OCT- / OCT+** sets SPEED to the next octave (-12, 0, +12). At an octave the glide is bent on the way, at most
  a sixth of its own change (it stays a glide), to land where the grid has it, else on the grid's beats (a 32nd of
  the loop); what it misses after, it crossfades over (two heads, both on the beats, GLIDE long and at least a 16th
  of the loop: x(2 - x) in, 1 - x^2 out), or, a few samples, nudges (0.4 % at most). An octave at once waits for the
  grid's next 16th of the loop (both heads on its beats then), REVERSE's turn for its next 64th (on them too).
  Between the octaves it runs free. The octaves are exact (the glide reaches x 1 and x 2: nothing drifts off the
  beat).
- *Where:* on the second core in the plate's job (its memory's pointers constants from the caller); on one core
  with the room, here. The delay is always on core 0, so it never needs a fallback.
- *Its memory* (borrowed from the delay: armed while L-LEVEL is above 0, ~5 s, mono mu-law at half rate): the delay
  line's second half, lent while the looper is armed, and what the RAM has left, 24 KB: two pieces of one ring of
  bytes, 5.11 s (MAX in TIME's picture with SCL held; the SMK-37's build, with less RAM left, 23 KB: 5.07 s). A bar at
  48 BPM and up, two at 96. Mono, the two sides' mean, a byte a sample (mu-law: a sign, 3 bits of octave, 4 of step:
  its noise 31 dB under a sine, a light tape). The delay line is kept its frames
  left and right in turn (as fast to read; the same sound bit for bit), so its second half is one run of bytes.
  Armed, the delay has the first half: TIME to 0.5 s, a longer division played at half as over 1 s before (TIME red,
  the hint *OVER 0.5 S: PLAYS*), and free ms halved as well (they were held at the line's end; under 1 s no change).
  The change, armed or let go, fades the echoes out over ~23 ms, clears the line and the looper's memory over ~80 ms
  (no old sound in either; 1024 a control tick, the line taking silence meanwhile: at once, the worst interrupt went
  from 52 % to 79 %), and fades what goes into the line in again (its first echo as smooth: fading the echoes in,
  the first one came in on a step); letting go waits for the looper to be quiet. L-LEVEL is 0 as the app starts (*LOOPER
  ON: DELAY 0.5 S* when it leaves 0, *LOOPER OFF: DELAY 1 S* back at 0). The test build
  `DINGHY_DEFS=-DSD_RAM2=1` looks for a second bank after the splash (`hal/fm1_ram2.h`: a value at each 4 KiB of 8
  MiB, all written before any is read back, then three patterns; the bus traps masked; a reset in the middle seen at
  the next boot, which does not try again), says *2ND RAM: n KB OK*, *NONE* or *THE TEST HUNG*, and, found, gives the
  looper the largest of 512 KiB .. 4 MiB there (24 .. 190 s), the delay keeping its line. Whether there is one: the
  FM-1's AC7911B comes with 2 MB (BA) or 8 MB (BB) inside, but no firmware for it is known to use it, M-VAVE's
  included (its start-up copies an overlay to 0x04000120 of length 0), and its loader sets the clocks up without it:
  set up or not, only an FM-1 can tell, and the test sets nothing up.
- Tests: `test_looper`, on two cores and on one: held to 0.00 dB from 2 to 9 s at 440 Hz; +12 plays 880 Hz, -12 220;
  no step round or let go bigger than the sine's own; while it loops an impulse echoes on TIME (13230 frames); its
  wet in the plate and in the room; REVERSE: a sine rising through its 500 ms plays rising forward (184 of 195 10-ms
  pieces up) and falling backward (188 of 199 down), round its start no step bigger than its own, at the turn 1.27 x
  (a kink, no click). Its grid: an octave down at an odd time and back (gliding 300 ms; at once; +12 gliding 900 ms)
  and REVERSE there and back: settled there the head is where the grid has it, and from 6.5 s the loop plays bit for
  bit as it would have, never moved; no step bigger than the sine's own (x 2 at +12). `test_space`: the delay's
  echoes as before, on both. `test_stereo`: a corner's bank alone at WIDTH's most in the middle (right / left 1.000);
  no factory patch's note leans more than 5.2 dB (PHASER: its Reactor's own), POLY's at WIDTH's most 2.5 dB.
---

## 6. Build order with listening gates

Each step ends with two gates: **hear it** (the identity patch set, A/B against the last step) and **fit it** (host
bench + fm1-emu instruction count). A step is not done until both pass.

| Step | Content | Hear it | Fit it |
|---|---|---|---|
| **P0** | Dinghy app skeleton, sub-block renderer, render tool, host bench, emu count, CPU nn % readout | A sine at the right pitch, no clicks at sub-block edges | Empty render < 2 % |
| **V1** | One operator: warp, fold, envelopes | Warp sweep is continuous sine → saw → pulse, no "crossfade" sound; fold sweep has the spec's four regions | Kernel cost per operator measured |
| **V2** | One bank: roles, PM, feedback op, FM/warp/fold order A/B | Bells to growls from one bank; the order is decided by ear and written down | 4 ops |
| **V3** | Four banks, nav mix, stereo, aliasing tracking, harmonic generators | **Gate D VOICE:** clean additive → complex FM → folded digital across the XY field, not a generic FM synth | 16 ops ≤ 35–40 % of the 2.9 ms budget |
| **R1** | Four BPFs, six states, POS, slew | Six states turn a static voice into vowels/chords; POS scans smoothly | |
| **R2** | COMB/KP, DELAY AP, IIR AP, shared pool | Pinged combs at musical pitches; AP skating kept | |
| **R3** | Drive, internal saturation, gain compensation, dry-path decision | **Gate D REACTOR:** phoneme / chord / resonator motion from one patch | Reactor ≤ 15–20 % |
| **R4** | The Reactor fills: MIX law, low keep, HARM | The Reactor colours the voice instead of replacing it; no inharmonic ringing | Scenes A/B; ~0.2 % |
| **D1** | Crossover, four shapers, bias | Bands audibly separate; slope decision (complementary vs LR4) | |
| **D2** | Dynamics, PHAT/SHARP, BODY | PHAT thickens, SHARP spits transients — not a compressor knob | (measured 1.4 %) |
| **D3** | Feedback, spread, TAME, safety limiter | **Gate D DISTROY:** bands behave as distinct dynamic/nonlinear systems; feedback sings, never blows up | Distroy ≤ 15–20 % |
| **S1** | SPACE: the delay into the reverb (§5.5) | Echoes that sit in the room; synced echoes on the beat; long tails that never blow up | ~3 % (measured 5.6 %); the worst case settled with C1 |
| **C1** | Whole chain, mod matrix, MUTATE / MOVE / BREAK / BODY | The identity set across all macros; worst-case patch | **Gate A/B:** total < 50 % (2.9 ms) with UI and MIDI (measured 46.6 %) |
| **S2** | SPACE's plate in place of the room (§5.5.1), FREEZE, SHIMMER | Long, dense tails with no ringing, no click when SIZE moves; FREEZE holds | measured +7.5 points: the gate 48.7 % (41.2 with the room), everything at once 56.3 % (49.1) |
| **S3** | SPACE's plate on the second core, the room its fallback (§5.5.2) | The plate as in S2, bit for bit; a second core that misreads: the room, with no dropout | measured: the gate 42.7 % of the first core (50.1 with the plate there, 42.2 with the room), everything at once 50.7 % (58.2, 50.2); the plate 7.4 % of the second (7.6 at most) |
| **S4** | TIME: the delay on its own page, and a looper on the second core (and one, with the room) (§5.5.3) | The delay as before; the looper holds, its speed an octave each way, no click round or let go; the delay goes on while it loops | measured: the gate 43.0 % of the first core (S3: 42.7), everything at once 50.8 % (50.7); the plate and the looper 11.1 % of the second (11.8 at most); one core, the room and the looper: 48.2 % |
| **HW** | First install on an FM-1 | Same renders through the real output | CPU nn % on the chip within the emu's estimate |
| **W1** | CONSOLE: a page of the site that connects to an FM-1 and shows everything at once, a ship's console (below) | Nothing in the sound changes; what it shows follows the FM-1 within ~50 ms | Its stream < 1 % of the chip, in the main loop, never in the audio's way |

Every step is done but HW (it needs an FM-1) and W1 (planned, below). What follows is what each left that later work
needs: what it built and where, what was decided and why, and what bit. The day-by-day log, its measurements and the
listening rounds are in the git history.

### 6.1 The voice (P0–V3)

- *Where:* `sd_engine.h` (the kernel, the banks, the nav, the voices), the laws in `tools/gen_tables.py`, the
  generators in `sd_gen.h`. A sub-block is 32 frames, the control tick before it.
- *Decided:* each oscillator folds before it modulates (§3.0); no self-feedback; the warp table spaced at even steps of
  timbre (spaced by waveform shape, the brightness jumped at the end of the skew); the squash a cubic soft clip; the
  nav's bilinear weights normalised by (Σw²)^¼. POLY8 is not built (a candidate, §3.6).
- *Aliasing* (gentle, §3.0): above C5 XM, FD and the squash fall 6 dB an octave, a saw's edge is never shorter than a
  sample of the note, an oscillator fades out from 16 to 20 kHz, a modulator loses its index from 4 kHz, gone by
  12 kHz (its sidebands were the screech of FM up high).
- *The kernel* (instructions a frame and oscillator, plain / folded): sine 19 / 33, sine → triangle 29 / 44,
  triangle/saw 26 / 39, squash 46 / 60. Every stage keeps the wave within −32767..32768, so no clamps; the warp's
  divisions and the nav's roots are cached while their inputs stand still.

### 6.2 The Reactor (R1–R4)

- *Built* (§4): four TPT SVFs in integers (BP6, BP12, VBP), KP combs, AP1 (Schroeder, skating kept), AP2 (IIR), six
  stages, POS, SLEW, the stage clock, FREQ and its tracking; a soft knee inside each loop (linear to 2 x full scale, a
  ceiling at 6 x; KP 1.5 / 3 x), the level law SD_RLEVEL (clean to 1 x, then into the soft clip), a make-up per model.
- *Decided:* linear interpolation in the comb (allpass interpolation would smear the skating); the dry path stereo.
- *R4, the Reactor fills instead of replacing* (it sounded like a ring modulator): MIX keeps the dry whole to its
  middle and adds the wet (`SD_RMIX`, the loudness held); the lows under `r_low` skip it; HARM snaps each filter to the
  nearest harmonic of the note (its level x 0.59). A POLY voice is −6 dB.
- *Bit:* a shift's floor held a loop at −1 for ever (round toward zero in every loop, CLAUDE.md); MIX's product
  overflowed 32 bits (now 64; the tests run under the sanitizers since); the Reactor rests when silent.

### 6.3 DISTROY (D1–D3)

- *Built* (§5): per band GAIN (0 .. +36 dB) → BIAS → the shaper (NONE SOFT HARD FOLD CRUSH, each its ALT) → LEVEL and
  PAN; one DC blocker a channel on the sum; the dynamics per band (PHAT, an upward compressor under BODY, +24 dB at
  most and none under −72 dB; SHARP, transients up by the fast follower against the slow); the loop (1 .. 20 ms, SPREAD
  for the right, a one-pole, a soft clip, FB up to 0.9), TAME (a 2-pole lowpass inside the loop) and a safety limiter.
- *The crossover:* LR4 is the engine's default (neighbours −25 .. −34 dB, CMP's −4 .. −15) and the edge's; CMP is the
  boot patch DISTROY's, for the CPU (with SPACE, LR4 in a patch that heavy reads ~52 %). XOVER is a knob.
- *Bit:* the loop's lowpass in Q15 overflowed and latched a DC (Q13 now); at FB ~0.27 a patch's gain in the loop
  holds a drone after the release; restructured into blocks (the crossover into four band buffers, then each band
  through its shaper picked once a call) to fit.

### 6.4 SPACE and TIME (S1–S4)

- *The delay* (§5.5): linear reads, a tape's glide to a new TIME, TONE, FDBK to 0.95 (toward zero), a shoulder in
  every store. Ping-pong in pairs (the left's echo into the right whole, the right's back x FDBK²: both sides as loud).
  Synced divisions exact, a division longer than the line halved until it fits, on the beat (`sd_space_sync_q8`). Its
  input loses its DC (a warp toward pulse built a rumble in the feedback).
- *The reverb:* the plate (§5.5.1) on the second core as jobs (§5.5.2); the 4-line room its fallback on core 0, and
  the build `DINGHY_DEFS=-DSD_ONE_CORE`. FREEZE (the player's) and SHIMMER.
- *TIME* (§5.5.3): the delay on its own page, the looper on the second core (with the room on one core), its memory
  the delay line's second half and 24 KB (5.11 s; the SMK-37's build 23 KB, 5.07 s) while L-LEVEL is above 0. The
  change clears 1024 a tick (~80 ms): at once it took the worst interrupt from 52 % to 79 %.
- *Kept as the user heard them:* the demos made before the divisions were exact (DERELICT, CINEMATIC, the second cue)
  have SPACE on FREE at 1000 ms, as they were saved.

### 6.5 The chain and the modulation (C1)

- *The matrix:* `e->base` is the patch, `e->patch` the base with the modulation written on each tick (`sd_mod_tick`),
  `sd_mod_restore` putting back only what it wrote (no slot and the macros at 0: the patch, sample for sample). One
  copy of the patch only when the main loop hands one over (`-fno-builtin` copies a byte at a time).
- *Sources:* ENV1..3, LFO1..2, NAV X/Y, VEL, KEY, WHEEL, AFTER, RAND, and the engines as modulators: STEP, SEG, D LOW,
  D TOP, R STAGE, ECHO, TURING. *Destinations:* 54, in the chain's order (`SD_MD_ORDER`), each new one after the
  others in the enum so a saved slot keeps its meaning: DISTROY's GAIN band by band and each crossover (sorted each
  tick: one moved past another goes through it), the looper's SPEED (free; TURING's in semitones).
- *LFOs:* six shapes, free or synced, KEY, and ONE (one cycle a note, unipolar: a SAW a ramp down inside the note).
- *The macros:* MUTATE (XM WP FD DT, gentle: past it the saw edges alias), MOVE (the nav and POS stepping on the beat,
  glided ~10 ms), BREAK (DISTROY weighted toward the top band, never its feedback: on a patch without one it opened a
  1 ms howl), MACRO 4 (BODY, AIR or DROWN).
- *The output:* a limiter 64 frames ahead to −1.4 dBFS, then a shoulder (linear to 0.85, a cubic to full scale).
- *The CPU:* no divide a sample (`sd_tz`); one loop variant per model and per crossover; the user chose a lighter
  DISTROY (its middle banks under the squash, unfolded) over -O2, to keep room (-O2 also broke USB-MIDI on the device).

### 6.6 The sound, as decided by ear after C1

- *The essence:* an evolving spectrum; modulation stepped and rhythmic (S&H glided ~10 ms, MOVE on the beat), not
  slow sines; the destruction on the top band (D HIGH, D SPLIT); no HARD or FOLD over 5 kHz in a new patch (without
  oversampling they fold back inharmonic); no aliasing noise.
- *The edge* (`SD_EDGE`, `sd_patches.h`): a plain FM patch sounds as it should with an edge: under the top split
  DISTROY passes it as it was (NONE through LR4: CMP's slopes let the body into the top's shaper), the top band alone
  destroyed (driven until its shaper's own content is −4 .. −8 dB of it), something stepping on the beat; BODY gives a
  hot patch its headroom back (LR4 raises peaks). GROWL and AURORA have it; GLASS, BELL, EPIANO and ORGAN went (still
  classic FM). 29 patches, GROWL first, DISTROY (P03) the boot patch; the code names them by `SD_P_*`.
- *STRINGS* is the cello found by ear (V4, `tools/cello_v4.txt`; `tests/run_sd.sh` checks the patch renders as the
  file does).
- *The stereo* (§3.6): a patch may lean, never hard left or right: the banks round the nav mix's middle, the far side
  6 dB down at most (POLY 2.5).
- *A MIDI clock:* each input a source of its own; the first leads until it has been quiet for 500 ms (the same clock
  on two ports counted twice).

### 6.7 The demos

Each is one FM-1 played by one person (`tools/sd_play.h`'s rules: one engine, the app's controls on their pages at a
hand's speed, the voice rules; a gesture the FM-1 could not make stops the tool). DERELICT and HELIOPAUSE layer four
such passes at fixed faders (`tools/sd_layer.h`), SINGULARITY is a single one; CINEMATIC is the first cue's
multitrack, kept as it was (remade as passes it lost what it had). The page plays DERELICT, CINEMATIC and RANGE.
`sd_play.h`'s transposes are chromatic (as RANGE is written); a player keeping the app's IN KEY sets `e.xkey`. Its
TURING DEST turns in `SD_MD_ORDER`, as the app does. A render waits for quiet: a loop left ringing (BREAK up) never
ends, so a performance ends with it off.

### 6.8 The numbers now (fm1-emu, 2026-11-11)

| Scenario | The first core | The second core |
|---|---|---|
| The gate: DISTROY, SEQ A, MONO | 44.0 % | 9.9 % (10.1 at most) |
| The looper armed, LOOP, GLIDE, SCL + OCT- | 45.3 % | 10.6 % (12.2 at most) |
| Everything at once | 51.8 % | 9.9 % |
| One core (a second core that misreads): the room | 44.3 % | – |
| One core, the room and the looper's glide | 47.4 % | – |

The app's CPU (the worst interrupt of a second) reads 52–54 % in the gate. RAM 96,796 B of 98,304 (the SMK-37's
97,420), the image 173 KB of 582, the pool 334 KB of 344. The loader is byte for byte Dinghy's.
- *Open:* the worst interrupt is a little over half in the emulator, and DISTROY in POLY with a held chord read 50.9 %
  on average (2026-11-08); the emulator counts instructions, not cycles, so HW measures both on the chip.

**HW, the checklist (on hold: the user has no FM-1 yet, so the work goes on without it).** The package:
`build/stardistroyer.fwsc` (`./build.sh`, every check ok; identity FM-1_96200, a dev build; a release X.Y is
FM-1_962XY, the first beta 0.1-beta FM-1_96201), installed with `python3 tools/fm1_install.py
build/stardistroyer.fwsc` (needs `pip3 install mido python-rtmidi`; it asks before it writes). The way back: the FM-1
Firmware Hub, or OCT- held at power-on for the USB rescue.
0. *Before anything:* a full flash backup (M-Flasher or MvaveFM1Unbricker, both chips), kept off the unit. Note
   the flash part: 0x856014 (the first units) or 0xB36014 (the UC25HQ80, units sold since autumn 2026); both must
   save and update (`firmware/hal/fm1_flash_map.h`).
1. *It starts:* the splash, then DISTROY on the NAV page; no crash screen; the second core runs (no *2ND CORE OFF*).
2. *Fit it:* PLAY, DISTROY in the loop: the CPU readout's worst against the emulator's (the gate's 44.0 % on
   average, 52–54 % its worst interrupt; the emulator counts instructions, the chip cycles: the gap is what this step
   measures). Then the other patches with PRESETS, POLY with a chord, and the looper armed.
3. *Hear it:* the loop of DISTROY, VOWEL, PLUCKS, CHORDS and VOYAGE through the line out against
   `build/audio/loop_*.wav`; no clicks at the knobs, none when SPACE's TIME moves or the looper is armed.
4. *The pages:* every page button through its pages (HOME NAV MACROS; FX REACTOR DISTROY SPACE TIME; ENV ENV FILTER;
   LFO LFO 1 LFO 2 TURING MOD; ARP; SEQ SEQ A SEQ B ORBIT); EDIT switches ON/OFF, SCL held is the second layer; the key
   LEDs follow the screen; the macros move the sound live.
5. *The voices:* `tests/scripts/v4.txt` by hand: ARP refused while SEQ plays in MONO, B locked in MONO, the
   messages; DUO and POLY share the voices without a note cut.
6. *MIDI:* notes over USB and the TRS jack, and REC writing them (`tests/scripts/m1.txt`); CC 1 (WHEEL) feeds
   DISTROY's loop; channel pressure (AFTER) on VOWEL drives it; a clock leads the tempo.
7. *Flash:* the octave, tempo, patch and mode kept over a restart (saved when silent); a user patch and a pattern
   SAVEd and back after a power-off.
8. *The update path (the hub's author checklist):* install from the official V15 and from another firmware of the
   hub, both ending with Star Distroyer running; from Star Distroyer, install another firmware (it must start that
   package's loader); unplug once during Writing, plug in, install again: it must go on. `fm1_install.py --info`
   (and the hub) must see the FM-1 as MIDI with its identity under a port name the hub knows.

**W1, planned: a page of the site that connects to the FM-1 and works as a big screen with an overview of
everything, a spaceship's console in the page's design and its player's.** The next step while HW waits for the
user's FM-1.
- *Where:* a page of its own on the site (`web/`, built by `make_site.py`, published by CI from `main`), e.g.
  `console/`, in the page's look (near black, ice-white lines, red accents, the "///" marks, Orbitron for the titles)
  and its player's console frame, wide: a ship's bridge rather than a phone screen.
- *What it shows, all at once:* the patch and its four banks on the nav's X/Y; REACTOR's stage and POS; DISTROY's four
  bands, their splits and levels; SPACE's echoes and the plate's tail (FREEZE, SHIMMER); the envelopes, the LFOs,
  TURING's register and where every matrix slot goes, moving as they play; the macros; SEQ A and B with their
  playheads, the master and its one, ORBIT's step, ARP's run; the tempo and its clock, the voices in use, CPU.
- *How it knows:* the FM-1 sends its state as SysEx over USB MIDI, the way the installer already talks to it: a
  compact frame (what moved since the last one) some 20-30 times a second, from the main loop between screen draws,
  never from the audio interrupt; the page asks for it and can stop it. Without an FM-1 the same console runs on the
  page's player (the firmware as WebAssembly), which hands over the same frames, so it can be built and tested
  before HW.
- *Later, not in W1:* editing from the console (the knobs on the big screen), patches to and from the computer.
- *Gates:* the sound unchanged (the renders bit for bit with the stream off); the stream's cost measured in fm1-emu
  (< 1 % of the chip) and on USB (no MIDI note late because of it); the console within ~50 ms of the FM-1's own
  screen.

The sequencer plan's minimal note driver (a fixed 16-step loop) is built in P0 so every step above is auditioned in
time as well as on held notes.

---

## 7. Identity patch set

Eight patches, rendered as a held note, a 16-step line and a POS/XY sweep each:

1. **GLASS** — clean additive, harmonic generator ALL, no fold: the "is it clean?" reference.
2. **BELL CELL** — METAL generator, series PM, nav between two bell banks.
3. **GROWL** — feedback operator, warp toward pulse, fold mid range, low register.
4. **VOYAGE** — four contrasting banks, slow TRACE around the field.
5. **VOWEL** — Reactor BPF, six vowel states, POS scanned by LFO.
6. **PLUCK CHOIR** — Reactor COMB at chord pitches, pinged by a short voice.
7. **SPLIT DAMAGE** — Distroy: clean lows, crushed mids, folded highs, PHAT on the low band.
8. **STAR DISTROYER** — everything at once, feedback on, BREAK high: the worst-case CPU patch and the "does it hold
   together" test.

Patch 8 is also the CPU worst case for Gate A/B.

---

## 8. Budget estimate (to be replaced by measurement)

240 MHz / 44.1 kHz ≈ 5440 cycles per sample; the Dinghy rule (under half the interrupt) leaves **~2700**.

| Block | Estimate, cycles / sample | Share of 2700 |
|---|---|---|
| VOICE: 16 operators × ~40 + nav mix + envelope ramps | ~700 | 26 % |
| REACTOR: 4 resonators × ~25 + drive + pan | ~200 | 7 % |
| DISTROY: 6 SVFs + 4 bands × 2 ch × ~35 + feedback + TAME | ~550 | 20 % |
| Control rate (mod, coefficients, slew) amortized | ~100 | 4 % |
| Sequencer, UI interrupts, key scan | ~150 | 6 % |
| **Total** | **~1700** | **~63 % of the half-budget, ~31 % of the chip** |

These are estimates of instruction counts on a RISC core, not measurements; XIP cache misses could cost more. For
scale: a dense synth on the same platform measured ~3900 instructions a sample. The estimate leaves room, but P0 and V1 exist to
replace it with numbers before anything else is built on it.

---

## 9. Open sound questions (decided by listening, in the step named)

| Question | Step |
|---|---|
| FM → warp → fold order | V2: decided by §3.0 |
| Self-feedback on operator 1: keep? | V2: no (§3.0) |
| Nav gain law (bilinear vs equal power vs in between) | V3 |
| Aliasing knees: how much to tame high notes | V3 |
| Reactor dry path: stereo or mono | R3: stereo (the voice's image kept); to be heard on the FM-1 |
| Comb interpolation: linear or allpass | R2: linear (KP within 4 cents to 1.76 kHz) |
| Crossover: complementary or LR4 | D1: LR4 by default (neighbours −25 … −34 dB against −4 … −15); CMP a patch's choice; to be heard |
| Which 4 further shapers come next (SLEW, HOLD, CHAOS, FRACTAL candidates) | after D3 |
