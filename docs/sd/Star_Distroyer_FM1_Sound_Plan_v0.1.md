# Star Distroyer — Sound Plan

**Status:** Plan v0.1, 2026-11-02. Companion to *FM1 Design & Feasibility v0.1*, sections 2–4, 13–15, and to *Star Distroyer — Sequencer Plan v0.2*.
**Priority:** sound is the current focus. This plan covers VOICE, REACTOR and DESTROY: how each is built on Dinghy,
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
| Two cores: Core A voice, Core B post | One audio interrupt (`app_audio`); Dinghy exposes no second core to the app | Plan for **one core**. A second core is a bonus to explore in Phase 0, not a dependency |
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

  Each voice has its own note, ENV1 and ENV2. Patch, Reactor and Destroy are shared, since they sit after the voices.
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
- **Stereo:** banks A/D left of centre, B/C right, by a WIDTH parameter, so moving X moves the image as well as the
  timbre.

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
  and drive pushes the rings into saturation — the Reactor's own nonlinear layer before DESTROY.
- Level law: clean → drive → saturation in the upper range (spec §3.8), one generated table.
- Gain compensation per model so switching BPF ↔ COMB does not jump in level.

---

## 5. DESTROY — multiband destruction

### 5.0 DESTROY's design (decided 2026-11-04)

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

### 5.5 SPACE: a delay into a reverb (decided by the user, 2026-11-04)

The spec keeps a BigSky-like SPACE optional: "if Space compromises any of [the core] … Space is removed". The user's
decision: a delay inside the reverb, one block, called SPACE, last in the chain.

```text
DESTROY out (L/R) ──┬─► dry ─────────────────────────────────────────────┐
                    ├─► DELAY (stereo or ping-pong; TIME free or synced; │
                    │   FDBK with TONE and a soft ceiling in the loop) ──┼─► x D-LEVEL ─┐
                    └─► + the echoes ─► REVERB (4 allpass diffusers →    │              ├─► out
                                        4-line FDN, Hadamard; SIZE,      │              │
                                        DECAY, TONE) ───────────────────► x R-LEVEL ───┘
```

The echoes go into the reverb, so they reverberate too. The delay's line is up to 1 s stereo; the reverb runs at
half rate, its four lines up to 2048 half-rate samples (92 ms). ~195 KB, in the pool: the app lends it to the engine.
CPU 5.6 % (planned ~3 %; see S1's notes). It rests when nothing
comes in and its tails are under the floor. Both levels at 0: no SPACE.

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
| **R4** | The Reactor fills (the user, 2026-11-05: "too extreme … ring modulator … tin can"): MIX law, low keep, HARM | The Reactor colours the voice instead of replacing it; no inharmonic ringing | Scenes A/B; ~0.2 % |
| **D1** | Crossover, four shapers, bias | Bands audibly separate; slope decision (complementary vs LR4) | |
| **D2** | Dynamics, PHAT/SHARP, BODY | PHAT thickens, SHARP spits transients — not a compressor knob | (measured 1.4 %) |
| **D3** | Feedback, spread, TAME, safety limiter | **Gate D DESTROY:** bands behave as distinct dynamic/nonlinear systems; feedback sings, never blows up | Destroy ≤ 15–20 % |
| **S1** | SPACE: the delay into the reverb (§5.5) | Echoes that sit in the room; synced echoes on the beat; long tails that never blow up | ~3 % (measured 5.6 %); the worst case settled with C1 |
| **C1** | Whole chain, mod matrix, MUTATE / MOVE / BREAK / BODY | The identity set across all macros; worst-case patch | **Gate A/B:** total < 50 % (2.9 ms) with UI and MIDI (measured 46.6 %) |
| **HW** | First install on an FM-1 | Same renders through the real output | CPU nn % on the chip within the emu's estimate |

**P0 done 2026-11-03.** fm1-emu (instructions, not cycles): the audio interrupt with the loop playing 7.9 M/s =
3.3 % of the chip, in silence 5.7 M/s (Dinghy's key scan and timers). The engine's own share is
~2.2 M/s, ~50 instructions a frame for one sine. The CPU readout (the render alone, worst interrupt a second) reads
0.6 % in the emulator. Not yet measured on an FM-1.

**V1 done 2026-11-03.** One operator in `sd_engine.h`; the laws in `tools/gen_tables.py`.
- *Hear it:* `build/audio/v1_sweep_warp.wav` goes sine → saw (5 s) → pulse (10 s) with no seam. The warp table is
  spaced at even steps of timbre: harmonic spectra, each harmonic weighted by its number. Spacing by waveform shape
  instead left the brightness jumping at the end of the skew. The squash's 1/tanh(G) is computed for each sub-block,
  not interpolated, so the hand-over at 0.5 is smooth. The test proves it: a 1/256 step at 0.5 moves the wave
  0.0127 rms, against a median of 0.012. Zero crossings per cycle in the fold sweep, at 0 / .30 / .65 / .85 / 1:
  2 / 2 / 6 / 10 / 18. That is the spec's four regions.
- *Fit it:* instructions per frame in the compiled kernel loop (pi32v2):
  - clean 37
  - fold 51
  - squash 62 (78 before the move to 32-bit math)
  - all on 79

  fm1-emu, GROWL in the loop: 8.7 M/s = 3.6 % of the chip, and the CPU readout shows 1.5 %.
- *For V3:* 16 operators all on, at ~75 each, is ~1200 a frame. That is about 22 % of the chip, just over the V3
  gate of 35–40 % of the half-budget. V2 sums each bank before the stereo store, which takes the per-operator output
  out of the loop. If that is not enough, squash and fold are the places to save.
- *Aliasing* is not measured yet. Energy above 15 kHz cannot tell a saw's own harmonics from aliases. V3 needs a
  real measure: render against a band-limited reference.

**V2 done 2026-11-03, to the design of §3.0.** One bank in `sd_engine.h`; the patches (GLASS, BELL,
EPIANO, GROWL, ORGAN) in `sd_patches.h`.

- *The fold-order A/B* was settled by §3.0: each
  oscillator folds before it modulates. The A/B before that heard folding in the oscillator as harder and brighter:
  +6 dB above 6 kHz against folding on the bank's sum.
- *Hear it:* the Q page swept on a held A2.
  - `q_sweep_wp.wav`: the test measures the anchors, h3/h1 at each:
    - sine at 0
    - triangle at 20: 0.111, the triangle's 1/9
    - saw at 40: h2 0.500, h3 0.333
    - pulse at 99: h3 0.302, h2 0.026

    No step crosses a hand-over at more than twice its neighbours.
  - `q_sweep_fd.wav` crosses 2 / 2 / 6 / 10 / 18 zero crossings at 0 / 30 / 65 / 85 / 99.
  - `q_sweep_xm.wav` is a 1:1 pair from 0 to 4π, and `q_sweep_dt.wav` four unison carriers spreading.
  - The bell's spectral centroid falls from 1070 to 299 Hz as ENV2 lets go.
  - PM matches cos(φ + β cos φ) to within 0.5 %.
- *Fit it:* instructions per frame in the compiled loops (pi32v2), per oscillator:

  | Shape | Without fold | With fold |
  |---|---|---|
  | sine | 21 | 36 |
  | sine → triangle | 36 | 51 |
  | triangle/saw | 33 | 48 |
  | squash | 59 | 74 |

  The worst case is down from 82 to 74. Dropping feedback, the shared shape and the closed-form triangle saved that.
  fm1-emu, with GROWL in the loop and all four oscillators in the worst variant: 15.0 M/s = 6.2 % of the chip
  (silent 6.0). The CPU readout shows 5.9 %, which is ~320 instructions a frame.
- *For V3:* every oscillator of four banks in squash with fold comes to ~4 × 4 × 74 + mixing ≈ 1250 a frame, ~23 % of
  the chip. A bank of plain FM comes to ~4 × 21–36. The gate is 35–40 % of the half-budget, 950–1080, so a patch at
  the very worst is over it and most patches are well under. Squash is the place to save.

**V3 done 2026-11-03.** Four banks in `sd_engine.h`, the generators
in `sd_gen.h`, the patches (GLASS BELL EPIANO GROWL ORGAN VOYAGE DISTROY) in `sd_patches.h`.
- *Built:*
  - Four banks with per-bank Q offsets (§3.0).
  - Voices with ENV1 and ENV2 each.
  - The modes: MONO, all banks by X/Y; DUO, two voices, A ↔ B by X; POLY, four voices on bank A. An allocator
    takes the same note, then the oldest silent, then the oldest released, then the oldest held.
  - The nav, with bilinear weights normalised by (Σw²)^¼.
  - COURSE from the gate, playing four cells.
  - A stereo balance per bank, with A and D left and B and C right, set by WIDTH.
  - Aliasing tracking, described below.
  - The generators: ALL, ODD, RND, CLUST, CHRD (14 chords in just intonation) and CPY.
  - The squash is now a cubic soft clip instead of tanh. The warp table was re-spaced for it, and the anchors still
    measure triangle 0.111, saw 0.500/0.333 and pulse h3 0.325.
  - POLY8 is not built. It remains a candidate (§3.6).
- *Gate D VOICE, hear it:* `v3_field_tour.wav`. VOYAGE is held while X/Y walks A → B → C → D → A, then the diagonal.
  - The spectral centroid goes 156 Hz (A, clean additive) → 690–1120 Hz (B FM bell, C folded growl) → 145–190 Hz
    (D organ).
  - The level holds within ±2 dB the whole way, with no energy hole at the centre.
  - Stereo side/mid is 0.3–0.44, against the target's 0.2–0.4.
  - Also `v3_poly_*.wav`, chords on four voices with a peak of 27k and no clipping, and `v3_duo_tour.wav`.
- *Aliasing:* a saw at C7 drops from −14.2 dB of its energy off the harmonic series to −21.7 dB with the tracking on.
  A2 is unchanged.
  - Above C6, XM, FD and the squash fall 6 dB an octave.
  - The saw's edge is never shorter than one sample of the note.
  - An oscillator fades out between 16 and 20 kHz.

  This is gentle, as the target asks: off-series partials only 15–33 dB down (§3.0).
- *Fit it:* instructions per frame and oscillator in the compiled loops:

  | Shape | Without fold | With fold |
  |---|---|---|
  | sine | 21 | 37 |
  | sine → triangle | 36 | 51 |
  | triangle/saw | 33 | 49 |
  | squash | 57 | 73 |

  The output stage is 13 a bank. The worst case is DISTROY, every oscillator of four banks in squash with fold at
  the field's centre: 16 × 73 + 4 × 13 ≈ 1240 a frame, ~23 % of the chip. That is over the V3 gate of 35–40 % of
  the half-budget (950–1080).

  On the host the other patches cost 15–55 % of DISTROY at the centre or in POLY: GLASS, BELL, EPIANO and ORGAN at
  25–45 %, VOYAGE 50 % and GROWL 95 %. At a corner it is 13–30 %.

  fm1-emu, DISTROY in the loop: 41.1 M/s = 17.1 % of the chip on average, with the loop's rests (silent 7.4). The
  CPU readout shows the worst interrupt at 23.8 %, which is ~1290 instructions a frame, as the count predicts. Not
  yet measured on an FM-1.
- *Optimised 2026-11-03, fit gate met.*
  - Every stage keeps the waveform within −32767..32768 and writes each next product for 32768, so no clamps are
    needed. The kernel walks pointers instead of indexes.
  - The control tick caches the warp's three divisions and the nav's two square roots while their inputs stand
    still.
  - The loops, plain / with fold:

    | Shape | V3 | Optimised |
    |---|---|---|
    | sine | 21 / 37 | 19 / 33 |
    | sine → triangle | 36 / 51 | 29 / 44 |
    | triangle/saw | 33 / 49 | 26 / 39 |
    | squash | 57 / 73 | 46 / 60 |

  - DISTROY in fm1-emu: 35.4 M/s = 14.7 % of the chip on average, and the CPU readout's worst interrupt at
    **19.9 %**. That was 23.8 % and is now inside the gate.
  - The sound is unchanged: the anchors, PM, chunking, aliasing and identity tests pass as before. The triangle is
    1 LSB higher.
  - Still left for C1, if Reactor + Destroy need room:
    - the cubic's ×⅓, which can go into its normalisation
    - the fold ramp's load
    - the skew's three loads from memory a sample (register pressure)

**R1 done 2026-11-04.** The Reactor in `sd_engine.h`; patches VOWEL and CHORDS, and DISTROY's Reactor.
- *Built:*
  - Four TPT state-variable filters in integers: states Q19, coefficients Q30, 64-bit products.
    1/(1 + g(g+k)) uses a 32-bit division to 15 bits and one Newton step, since 64-bit division is not allowed.
  - g = tan(πf/fs) comes from a table by semitone, and k (RES) runs exponentially from 2 to 0.01.
  - BP6 is one SVF's band-pass, which grows louder as it rings. BP12 is a unit-peak SVF into a second one. VBP is
    unit-peak, so RES sets its width.
  - The six stages, POS, SLEW, the stage clock and FREQ with key tracking.
  - The page is on FX: K1 POS, K2 RES, K3 SLEW, K4 DRIVE; with SEL held, FREQ, MODEL, DIV and MIX. The bottom field
    shows the six stages, the one in play red and the one it glides toward dim red.
- *Hear it:*
  - `r1_sweep_pos.wav` runs VOWEL's POS from stage 1 to 6. Each stage's strongest harmonics land on its formants
    within one harmonic of 110 Hz: A 660/1100 (F1/F2 730/1090), E 440/1760, EE 220/2200, AW 550/880, ER 440/1320.
  - Also `loop_vowel.wav`, `loop_chords.wav` and `hold_chords.wav`. CHORDS follows the key, and a narrow BP12 on a
    harmonic rings ~+25 dB, so its levels sit low.
- *Tests:*
  - VBP peaks within 0.15 % of its frequency at 0.00 ± 0.02 dB, from 100 Hz to 13 kHz.
  - At 1 kHz BP6 rings to +15 dB, BP12 is −38 dB two octaves off against BP6's −12, and VBP is at 0 dB.
  - SLEW 0 jumps, SLEW glides to the log-pitch midpoint.
  - The clock's modes, every maximum without overflow, and MIX 0 bit-exact.
- *Fit it:* the loop is 95 instructions a filter (every model's branches) and ~57 around it. BP12, the costliest,
  comes to ~420 a frame, ~8 % of the chip, under the plan's 15–20 %. fm1-emu, DISTROY with its Reactor in BP12, in the loop: 53.4 M/s = 22.2 % of the chip on average. The CPU readout
  shows the worst interrupt at **28.3 %**: the voice's 19.9 plus the Reactor's ~8.4. That leaves ~20 % for DESTROY
  under C1's 50 %.
  - The Reactor rests when the voice is silent and its integrators have rung out under ⅛ LSB: no work, no output.
    Without that, silence cost 25.3 M/s; with it, 9.0.
  - A first measurement was of a stale package: `./build.sh` had failed its ROM-address check on `-(1 << 22)` (now
    clamped before the shift) and written no new `.fwsc`. CLAUDE.md now says how to see that.

**R2 done 2026-11-04.** The Reactor's other three models in `sd_engine.h`: KP, AP1, AP2. Patches PLUCKS, FLANGE and
PHASER.
- *Built, to §4.0:*
  - **KP:** four combs in parallel. Each is a delay line fs/f long, read linearly between samples, with
    Karplus-Strong's two-point average in the loop (its half sample taken off). Feedback 1 − k/2 = 0 … 0.995 by RES.
  - **AP1:** four Schroeder allpasses (g 0.6) in series on delay lines. A filter's delay slides as its pitch moves,
    so the pitch skates (kept on purpose).
  - **AP2:** eight first-order IIR allpasses in series, two per filter, c = (g−1)/(g+1).
  - **The AP modes' outputs**, a subtracted and an added dry: left = wet − RES·dry,
    right = wet + RES·dry, at the four levels' mean, with no pan. They are very wide (side ≈ mid); in mono the
    notches cancel.
  - The lines are int16 (Q13: 4 × full scale), 4 × 2048 samples = 16 KB of RAM in the engine (21.5 Hz the lowest
    comb). The delays are ramped across each sub-block.
  - The delay models rest once their output has stayed under 4 LSB for 73 ms with the voice silent.
- *Decided:* **linear interpolation** in the comb (§9's open question). KP rings within 0.1 cents up to 440 Hz,
  +0.9 at 880 Hz and +4.1 at 1.76 kHz. In AP1's loops, linear reads cost a few dB of colour above ~4 kHz when all
  four delays are the same (−4.6 dB at 6 kHz). Allpass interpolation would fix that, but it would smear the skating
  the Reactor keeps. AP1 is flat within 1.2 dB to 3 kHz.
- *Found and fixed by the tests:*
  - Two limit cycles. A shift's rounding toward −∞ held KP's loop at −1 and an AP2 stage at a small DC for ever: no
    tail ended, and the Reactor never rested. Both loops now round toward zero.
  - AP2's coefficient had the wrong sign, so its phase turned only near Nyquist.
- *Hear it:*
  - `loop_plucks.wav` / `hold_plucks.wav`: a pluck pings four combs on chords.
  - `loop_flange.wav`: AP1 skating.
  - `loop_phaser.wav`: AP2 gliding.

  AP2 notches to −31 dB left and −32 dB right at RES max, and is flat within 0.14 dB at RES 0.
- *Fit it:* on the host, with DISTROY's voice in front, the Reactor costs BP6 +24 %, VBP +27 %, AP1 +27 %, AP2 +32 %,
  BP12 +35 % and KP +35 % of the voice. BP12, DISTROY's, stays the worst. fm1-emu, DISTROY in the loop: 54.5 M/s = 22.7 % of the
  chip on average (silent 9.1). The CPU readout shows the worst interrupt at **29.0 %** (R1: 28.3; the models'
  dispatch). RAM: 38 of 96 KB with the delay lines.

**R3 done 2026-11-04: the Reactor is finished (Gate D REACTOR).** Its nonlinear layer, its levels, and patch FUSION.
- *Saturation in the loops:* each SVF's band-pass passes a knee before it updates its integrator: exactly linear to
  2 × full scale, then a cubic shoulder to a ceiling at 6×. KP's line works the same way: linear to 1.5×, a ceiling
  at 3×. High RES and DRIVE ring into saturation and never away. Both shoulders work on the magnitude, with the
  sign put back.
  - A first version used the cubic all the way: two downward roundings a sample left a bias. Recirculating in a
    ringing filter, it took 18 dB off VBP at 100 Hz and held KP at a DC again.
  - The tests: a quiet sine through BP6 at RES 79 has h3/h1 0.027 %, and driven 4× on a loud one 10.8 %, with every
    integrator under 16 × full scale.
- *The level law (SD_RLEVEL):* the first half is clean, 0 … 1×; the second rises 1 … 4× into the output's soft
  clip. 32768 is 1×, as before; VOWEL's levels were moved to keep their gains.
- *Make-up per model,* measured against the dry voice (a saw through VOWEL's stage A, RES and level one half):

  | Model | Before | With make-up |
  |---|---|---|
  | BP6 | +3.4 dB | +0.2 dB |
  | BP12 | −0.3 dB | −0.0 dB |
  | VBP | −13.3 dB | −0.1 dB |
  | KP | +15.1 dB | +2.0 dB (its own saturation) |
  | AP1 | +0.8 dB | −0.0 dB |
  | AP2 | +0.7 dB | +0.1 dB |

  A model change no longer jumps. BP6's and BP12's loudness growing with RES stays,
  by design. VOWEL's and PLUCKS' levels were converted to keep their gains: their renders are unchanged.
- *The dry path (§9): stereo.* MIX blends the voice's own stereo (banks panned by WIDTH) with the Reactor's panned
  wet. Mono would throw that image away. To be heard on the FM-1.
- *Gate D REACTOR, hear it:* `loop_ride.wav`, `hold_ride.wav`. One patch moves through a phoneme (A), a chord, a
  phoneme (OO), a single high ring, a phoneme (EE) and a chord. BP12 at RES 70, ping-pong a beat, with a glide. The
  spectral centroid walks 300 … 1540 Hz beat by beat, without clipping. FUSION needed DRIVE low: narrow BP12 on a
  held vowel rings ~+25 dB.
- *Found by the sanitizers:* MIX's (wet − dry) × MIX overflowed 32 bits once wet and dry stood more than 2 × full
  scale apart (KP at +15 dB): a crackle on the FM-1. It is now 64-bit. `tests/run_sd.sh` now also runs the tests
  under ASan/UBSan (SD_ASAN=0 skips it).
- *Fit it:* fm1-emu, DISTROY (BP12) in the loop: 56.8 M/s = 23.6 % of the chip on average (silent
  9.3). The CPU readout shows the worst interrupt at **30.1 %** (R2: 29.0; the knees' tests). That is the voice's
  ~19.9 plus the Reactor's ~10.2, inside the plan's 15–20 % for the Reactor. ~20 % is left for DESTROY under C1's 50 %.

**D1 done 2026-11-04.** DESTROY's crossover, bias and shapers in `sd_engine.h`; patch SPLIT, and DESTROY in DISTROY.
- *Built:* per band, GAIN (0 … +36 dB) → BIAS (±½ full scale) → shaper → DC blocker (~14 Hz, its output × 256: a
  first version had a dead zone of 511 LSB and left a DC of 37) → LEVEL (the Reactor's law) → PAN. Then MIX dry ↔ wet;
  0 is no DESTROY, bit for bit. The shapers:
  - **NONE:** guarded at 4 × full scale.
  - **SOFT:** asymmetric, a lower ceiling on the negative side; h2 −20 dB. ALT = hardness.
  - **HARD:** symmetric; h2 −57 dB. ALT = the knee.
  - **FOLD:** ALT = the spacing.
  - **CRUSH:** toward zero, 15 … 2 bits by ALT.

  BIAS before HARD makes h2 −11 dB, and no DC comes out.
- *The slope (§5.1, decided): LR4 by default, CMP kept as a patch's choice.* Measured with each band alone and a sine
  in each band's middle (splits 150/800/4000 Hz):
  - The complementary split's neighbours pass −3.9 … −14.6 dB: "crushed mids" would crush the bass too.
  - LR4's pass −24.8 … −34.3 dB.
  - Both sum flat, within 0.013 dB.

  LR4 is a tree: split 2 first, then split 1 on the low side and split 3 on the high side. Each split is two
  Butterworth stages, and each side gets an allpass at the other side's split. That is 11 SVFs a channel against
  CMP's 3. The A/B is `d1_split_lr4.wav` / `d1_split_cmp.wav`, to be heard.
- *Hear it:* `loop_split.wav`, `hold_split.wav`: clean lows, crushed mids, biased SOFT upper mids, folded highs, the
  bands panned apart.
- *The page:* FX steps NAV → REACTOR → DESTROY. EDIT picks the band and ALGORITHM its shaper. K1–K4 are GAIN, ALT,
  BIAS and LEVEL; with SEL held, SPLIT, PAN, XOVER and MIX. The bands are drawn as rectangles on a log-frequency
  axis.
- *Fit it:* fm1-emu, DISTROY (voice, Reactor BP12, DESTROY LR4) in the loop: the first build's worst
  interrupt was **52.0 %**, over C1's 50 %, with DESTROY alone ~22 %.
  - Restructured into blocks: the crossover over a call's frames into four band buffers, then each band through
    its shaper with the type picked once, not a sample.
  - Level and pan in one gain a band and channel.
  - One DC blocker a channel on the sum, not one a band (linear: the same).
  - The gain in 32 bits.

  After that: **46.5 %** worst, 94.2 M/s = 39.2 % on average. DESTROY ~16 %, LR4's crossover about half of it. On the
  host DESTROY LR4 costs 43 % of the voice and CMP 26 %.
  - *For D2/D3:* the dynamics and the feedback must fit in the ~3.5 % left under 50 % for this worst case, or the
    worst case gives way. CMP saves ~7 %, which makes the A/B by ear a CPU decision too.

**S1 done 2026-11-04.** SPACE in `sd_engine.h` (§5.5); its memory a `.pool` static in the app.
- *Built:* the delay reads with linear interpolation and glides to a new TIME like a tape (~12 ms). It has a one-pole
  TONE, and FDBK up to 0.95 rounds toward zero. Every store has a shoulder past 1.5 × full scale, so a long feedback
  saturates instead of growing.
  - Ping-pong takes the mono sum into the left and crosses the feedback.
  - Synced, TIME steps through twelve divisions of the note driver's sixteenth.
  - The reverb takes the dry and the echoes (mono) through four Schroeder allpasses (g 0.625) into four lines with a
    Hadamard matrix.
  - SIZE scales the lines (a sample a sub-block, no clicks); DECAY sets their gain from a T60 table (`SD_SP_G`); TONE
    is a lowpass in each line.
  - Two decorrelated taps make the stereo.
  - It rests 2756 ticks (2 s) after its tails fall under 4 LSB with no input.
- *Tests:* echo timing exact (250 ms at frame 11025; a dotted 1/4 at 100 BPM at 39690), ping-pong L then R, FDBK at
  max bounded (peak 57795 in the line), DECAY lengthens the tail, it rests 4.3 s after a note, levels 0 bit-exact.
  The tails' floor is ~−80 dB: the lines are Q14 in 16 bits.
- *Hear it:* `loop_plucks.wav`, `loop_tour.wav`, `loop_chords.wav`, `loop_distroy.wav` (everything at max, 1/16
  synced, ping-pong), and the demo. No render clips (PLUCKS peaks at 31469).
- *The page:* FX steps NAV → REACTOR → DESTROY → SPACE. The panel shows the echoes as lines falling by FDBK (the
  right channel's ice-white in ping-pong) over the reverb's tail, along 2 s; the bottom shows the delay's time and
  mode and the room.
- *Fit it:* fm1-emu, DISTROY in the loop.
  - Without SPACE (the same build, no memory lent): 39.3 % on average.
  - The first SPACE: 47.7 %, 8.4 % for SPACE.
  - The reverb at half rate (two frames averaged in, a straight line between its outputs; the lengths halved, the
    tone's coefficient squared to keep its corner): 44.9 %, SPACE 5.6 %.
  - The worst interrupt reads **52.2 %**, over C1's 50 % (46.5 % before SPACE).
  - *Decided (the user, 2026-11-04):* DISTROY's crossover is CMP. With it: **45.4 %** worst, 92.3 M/s = 38.5 % on
    average, ~4.6 % left under 50 % for D2/D3. LR4 stays the engine's default and SPLIT's; with SPACE on, LR4 in a
    patch as heavy as DISTROY would read ~52 % again, so C1 must keep that in mind (XOVER is a knob).

**D2 done 2026-11-04.** DESTROY's dynamics in `sd_engine.h`, per band: SHAPE (PHAT ← off → SHARP) and BODY.
- *Built:* each band's own follower, not the full-band one §5.2 first planned: the dynamics sit in each band (§5.0).
  - The band's peak is gathered in the shaper loop (two instructions a sample).
  - At each tick (0.73 ms) it becomes dB (`SD_DLOG`: five fixed steps for the top bit, a table for the rest).
  - It feeds two envelopes: fast holds the peak and falls 60 dB in 80 ms; slow is a one-pole, 15 ms up and 150 ms
    down.
  - The gain in dB goes back to linear (`SD_DEXP`) and multiplies GAIN's. It ramps through the sub-block, Q12, so
    it has no steps.
  - SHAPE 0 is D1's gain, bit for bit.
  - BODY is the threshold T, −54 … −6 dBFS.
- *PHAT,* a low-level compressor: under T, up to 4:1 upward; above T, nothing. Quiet parts and tails come
  up.
  - A first version also pressed down above T. It made SPLIT's held bass quieter and spikier (RMS −2.6 dB, crest
    +0.7 dB), the opposite of "thickens".
  - Its level is the slow envelope or the tick's peak − 3 dB, whichever is higher. The slow one alone let a note's
    first 15 ms through lifted, up to +24 dB.
  - Below −72 dB it lifts no more, so noise is not pumped up; +24 dB at most.
- *SHARP,* an expander that emphasises transients: + 2 × (fast − slow), so the attack goes up and the tail
  down, plus a gentle expander under T. −30 … +12 dB. At +18, with the first PHAT, DISTROY's onset peaks rose 3.2 dB
  and clipped.
- *Tests:*
  - A 30 dB step from −40 to −10 dBFS with T −25 comes out 19.1 dB (4:1 upward: 18.75). The loud part moves 0.0 dB.
  - SHARP raises plucks' crest from 12.8 to 15.6 dB.
  - −90 dBFS with PHAT full and T −6 gets +0.0 dB.
  - SHAPE 0 leaves every band's gain at GAIN's.
- *Hear it:* `d2_split_phat.wav` / `d2_split_nodyn.wav` (SPLIT, a held bass, PHAT on the low band at T −12 dBFS:
  RMS +0.5 dB, crest −0.8 dB), and `d2_distroy_dyn.wav` / `d2_distroy_nodyn.wav` (PHAT low, SHARP on CRUSH and
  HARD: crest +0.5 dB, the attacks spit). DISTROY's band levels came down ~1.5 dB, and with them the clipping in
  `hold_distroy` at note 57: a Reactor stage rings there.
  - That clipping was older than D2. S1's scan of the renders had missed it, because the sanitizer run had stopped
    `run_sd.sh` before the renders: the files measured were stale. The cause was a buffer in D2's new test.
- *The page:* DESTROY with SEL held: K3 SHAPE (PH / OFF / SH), K4 BODY (dBFS). MIX moved to ALGORITHM and XOVER to
  GLO, both with SEL held; the panel's title row shows both. The bottom shows P / S on a band and a meter of the
  dynamics' gain now, red up, ice down.
- *Fit it:* fm1-emu, DISTROY in the loop: 95.8 M/s = 39.9 % on average (38.5 % at S1): D2 costs 1.4 %. The worst
  interrupt reads **47.1 %** (45.4 % at S1): ~2.9 % left under 50 % for D3's feedback, TAME and limiter.

**D3 done 2026-11-05.** DESTROY's feedback (§5.3), TAME (§5.4) and a safety limiter, in `sd_engine.h`.
- *Built:* the wet out (after the DC blocker, TAME and the limiter) goes into a line of 1024 int16 a channel (4 KB,
  in the engine).
  - It is read 1 … 20 ms back (TIME). The right line is up to 30 % shorter (SPREAD).
  - In the loop: a one-pole (~7 kHz), a soft clip (linear to 0.75 × full scale, a ceiling ~1.05), × FB (0 … 0.9,
    toward zero), then into the crossover's input.
  - The shortest delay is 1 ms (44 frames), longer than a call. A call's feedback is read before the call writes any,
    so the blocks stay blocks.
  - TIME and SPREAD glide a sample a tick: turning them bends the loop's pitch.
  - The loop's lowpass first multiplied by 22938 in Q15. That overflowed at full scale and latched the loop on a DC,
    which the DC blocker then hid: max feedback went silent. It is Q13 now.
- *TAME:* off at 0, then a 2-pole lowpass from 14.9 kHz to 1.2 kHz, its damping from 1.41 to 0.81 (a little
  resonance at the top). One SVF a channel. It is inside the loop, so it tames the feedback too.
- *The limiter:* the peak before it, a tick at a time. Past 1.5 × full scale its gain drops at once and comes back
  over ~0.75 s. It is a tick late, so a ceiling at 2 × full scale follows it. It stays out of the way until something
  is already wrong.
- *Tests:*
  - At TIME 10 ms the repeat comes at frame 441, −6.1 dB at FB 0.5, with no second repeat above the first.
  - At FB 0.8 a burst dies to ≤ 2 LSB within 1.5 s: the crossover's last bit through the DC blocker's floor. DESTROY
    rests there.
  - Everything at max (+36 dB in every band, FB 0.9, noise in): it screams, bounded at ≤ 2 × full scale, the
    limiter at 0.09.
  - +36 dB on a sine of 0.25 holds at 1.51 × full scale.
  - TAME at max: 100 Hz +0.0 dB, 10 kHz −40.2 dB. At its least, 10 kHz −0.2 dB.
- *DISTROY:* FB 0.19, 6 ms, SPREAD 34 %, TAME 34 %. From FB ~0.27 this patch's loop holds itself up after the note's
  release and drones on, at −37 dBFS: the bands' gain (+7 … +10 dB) is in the loop. At 0.19 it rings and dies like
  the patch without feedback.
- *Hear it:* `d3_distroy_fb.wav` / `d3_distroy_nofb.wav`, and `d3_distroy_hold_fb.wav`. No render clips.
- *The page:* EDIT on DESTROY's page steps bands 1–4, then LOOP: K1 FDBK, K2 TIME, K3 SPREAD, K4 TAME.
- *Fit it:* fm1-emu, DISTROY in the loop: 43.2 % on average (39.9 % at D2). Feedback ~1.4 %, TAME ~0.6 %, and the
  rest (the limiter, the ceiling, the peak) ~1 %. The worst interrupt reads **50.5 %**, over C1's 50 %.
  - Tried: the output loop's state in locals (no change). A 32-bit limiter: slower, 44.0 %, because the chip's
    64-bit multiply is cheap. The feedback read inside the crossover loop: 43.2 % against 42.9 %, kept for being
    simpler.
  - *Open:* ~0.5 % has to come back, now or in C1, which adds the mod matrix as well.

**R4 done 2026-11-05.** The user heard the Reactor as "too extreme, ring-modulator-like, a tin can".
- *Why:*
  - MIX crossfaded dry to wet, and most patches sat at full wet: the voice's fundamental and body were gone,
    leaving narrow bandpasses alone.
  - In DISTROY, VOWEL and FUSION the filters did not follow the note. A resonance between the harmonics rings at a
    pitch the note does not have, which is what a ring modulator does.
  - DRIVE and the loops' saturation intermodulated the filters.
- *MIX fills (`SD_RMIX`):*
  - Up to the middle the dry stays whole and the wet comes in on top; past it the dry fades.
  - Both are scaled by 1 / sqrt(dry² + wet²), so the loudness holds across MIX: −3 dB each at the middle.
  - The ends are the old law's.
- *The low keep (`r_low`):* the lows under 50 … 400 Hz skip the Reactor. Whatever MIX takes of the dry, a one-pole
  gives back below the corner. At full MIX, 60 Hz comes through at −0.3 dB with it, −41 dB without.
- *HARM (`r_track` bit 2; bit 1 is KEY):* each filter goes onto the nearest harmonic of the note (1 … 32, none under
  the fundamental). The resonances then strengthen what the voice has instead of adding tones.
  - KEY+H first transposes with the key, then snaps; HARM alone keeps the stages' own colour (a vowel's formants)
    and snaps it.
  - On the harmonics a filter passes 1.4 … 7.4 dB more (measured on five patches), so HARM takes the Reactor's level
    × 0.59.
- *Patches:* MIX 24 000 … 40 000, RES and DRIVE lower, HARM or KEY+H, LOW 160 … 180 Hz.
  - DISTROY, VOWEL, CHORDS, PLUCKS and FUSION.
  - FLANGE and PHASER are unchanged (full wet, r_low 0: the old law exactly).
- *POLY:* a voice is now −6 dB (was −3). Once the Reactor kept the dry, four voices in a chord clipped CHORDS and FUSION.
  POLY patches are 3 dB quieter.
- *Hear it:* `build/audio/r4/before_*.wav` and `after_*.wav` for DISTROY, VOWEL, CHORDS, PLUCKS and FUSION. Each is
  a scene (`sd_render scene`) at 100 BPM, 29 s: a bass riff low, a slow melody, four POLY chords, a fast arpeggio
  over two octaves. No render clips; the loudness is within ~1 dB of before, FUSION +4 dB.
- *The page:* REACTOR, ALGORITHM TRACK (OFF KEY HARM KEY+H), SEL + ALGORITHM LOW; both on the panel's title row.
- *Fit it:* fm1-emu, DISTROY in the loop: 44.2 % on average (43.2 % at D3). The worst interrupt reads **52.7 %**
  (50.5 % at D3).
  - The first R4 read 53.4 / 53.9 %. HARM's scan of all 32 harmonics, four filters a tick, eight ticks an interrupt,
    cost ~1.2 % at the worst interrupt; a search by halving (five steps) replaced it.
  - MIX up to the middle takes a 32-bit fast path (the dry whole, no lows to add). It measured no change in DISTROY.
  - The rest (~1 % on average) is likely the level: DISTROY's DESTROY now gets the dry and the wet. More of the
    signal goes over the shapers', the feedback's and the limiter's knees, whose slow path is the cubic.
  - *Open, for C1:* the worst case is 2.7 % over 50 %, with C1's mod matrix still to add.

**C1 (2026-11-05): the CPU first.** fm1-emu, DISTROY in the loop, the worst interrupt (the readout):
- *Where it goes* (one block off at a time): the voice 20.7 %, DESTROY 14.3 %, the Reactor 10.9 %, SPACE 6.4 %; the
  four add up to the 52.7 % R4 left.
- *Done, all bit-exact (the demo renders sample for sample the same):*
  - The Reactor's loop: one variant per model and MIX side (`sd_reactor_loop`, MODEL and FILL constants), its scalars in
    locals. DESTROY's crossover: one variant per XOVER and feedback (`sd_dx_loop`), its coefficients in locals.
    52.7 → 50.7 %, 44.2 → 42.5 % on average.
  - No divide per sample: the signed `/ 2`, `/ 256`, `/ 512` in the Reactor's KP, the knee, DESTROY's DC blocker and
    line, SPACE's store and its Hadamard compiled to the chip's divide. They are shifts toward zero (`sd_tz`) now.
    77 → 50 divides in the image, those left at control rate. Neutral in the emulator, which counts a divide as one
    instruction; on the chip a divide costs many cycles.
- *Tried, no gain, reverted or kept for clarity:* SPACE's state in locals (kept: neutral); the oscillator's shape
  constants in locals (+0.3 %, reverted: the registers spill).
- *Measured, not taken:* the whole build at -O2 reads 50.3 % (−0.6 %), but -O2 broke USB-MIDI on the device
  (tools/build.py); the engine alone at -O2 would need its own translation unit.
- *Left:* ~1 % to 50, plus the mod matrix (the user, 2026-11-05: the matrix first, the last percent settled after).

**C1, the modulation (2026-11-05).** One matrix for all four engines (the spec's §7), two LFOs (§2.9) and the four
macros (§8), in `sd_engine.h`, all at the control rate:
- *How it plays:* `e->base` is the patch as programmed (taken once each time the main loop hands one over);
  `e->patch`, what the engines read, is the base with the modulation written on top each tick (`sd_mod_tick`).
  `sd_mod_restore` puts back only the fields the modulation wrote. No slot and the macros at 0: the patch exactly,
  sample for sample (the A/B: loop renders with a full copy a tick and with the restore are identical).
- *Sources (Q15):* ENV1 and ENV2 of the last note's voice, LFO1, LFO2, NAV X and NAV Y (where the nav is,
  around the centre), VEL, KEY (around C4), WHEEL (MIDI CC 1), AFTER (channel pressure), RAND (drawn at each note).
- *Destinations:* PITCH (+-24 semitones at 100 %), XM WP FD DT, X Y (an offset on top of the nav or COURSE), WIDTH;
  the Reactor's POS FREQ RES DRIVE MIX; DESTROY's GAIN and BIAS (all bands), FDBK TAME MIX; SPACE's FDBK D-LVL
  R-LVL DECAY. An amount of 100 % moves a destination by its whole range at a full source; slots add up.
- *LFOs:* SINE TRI SAW RAMP SQUARE S&H; free 0.02 .. 40 Hz (`SD_LFO_INC`, exponential) or 1/16 .. 4 bars of the
  note driver's tempo; KEY restarts one on each note. Global, not a voice's (POLY's four voices share them).
- *Macros:* 0 is the patch as programmed; each moves a curated set by its own depth (to be tuned by ear):
  - MUTATE: XM, WP, FD, DT up.
  - MOVE: an orbit of the nav (two slow sines, ~0.23 and ~0.17 Hz), a triangle sweep of POS (+-3 stages), the
    Reactor's slew, WIDTH.
  - BREAK: the Reactor's DRIVE, DESTROY's GAIN, ALT, the bias's range (up to 2.5 x), FDBK and MIX; the bands' level
    back by up to 8.5 dB, so BREAK destroys instead of getting louder (without it DISTROY and SPLIT clipped).
  - MACRO 4, the patch's kind: BODY (the Reactor's RES and LOW, PHAT on every band, TAME), AIR (the Reactor's FREQ
    up to +12, WIDTH, less TAME, SPACE's tones brighter) or DROWN (SPACE's levels, decay, feedback, size).
- *Patches:* eight have slots (GLASS AFTER -> XM, WHEEL -> WP; GROWL a synced LFO on FD, VEL -> XM; VOYAGE two slow
  LFOs on X and Y; DISTROY S&H a sixteenth on POS, a slow LFO on WP, ENV2 -> DESTROY's GAIN, WHEEL -> its FDBK;
  VOWEL VEL -> RES, AFTER -> DRIVE; PLUCKS VEL -> RES, RAND -> POS; FUSION a two-bar LFO on FREQ; SPLIT ENV1 -> BIAS).
  DISTROY's LFO on WP keeps the warp's cache from ever holding: the worst case.
- *The pages:* FX steps on to MOD (EDIT: slots 1..8, LFO 1, LFO 2; a slot's K1 SRC K2 DST K3 AMT; an LFO's K1
  SHAPE K2 RATE K3 MODE) and HOME (K1..K4 the macros, ALGORITHM MACRO 4's kind). `tests/scripts/c1.txt`.
- *Hear it:* `build/audio/c1_macros_*.wav` (`sd_render macros`): the loop at 110 BPM, each macro in turn up to 99,
  held, back, for GLASS GROWL VOYAGE DISTROY VOWEL PLUCKS SPLIT. No clipping but DISTROY's MUTATE at full (16 samples).
- *The copy:* a whole patch is ~400 bytes, and `-fno-builtin` makes the copy a byte at a time (~1200 instructions).
  Before C1 the audio took one every tick, ~0.7 % of the chip; the first matrix took two (52.3 % with no slot).
  Now one only when the main loop hands a patch over.
- *Fit it:* fm1-emu, DISTROY in the loop with its four slots: the worst interrupt **51.2 %** (53.2 % with the two
  copies), 42.5 % on average. The matrix with its slots costs ~0.5 % over the same patch without them.
  - The last 1.2 %: the user chose a lighter DISTROY over -O2 (2026-11-05: "we will add more functionality
    later", among it a Turing machine / randomizer). Its middle banks, BELL and EPIANO,
    sit under the squash (WP −17000 a bank: the saw at most with the LFO) and unfolded (FD −15000); the outer two
    GROWL chains keep both. Its level +0.4 dB, and MUTATE at full no longer clips.
  - **46.6 %** worst, 39.4 % on average: 3.4 % under 50 % with the whole chain and the matrix.
- *Hear it (the user, 2026-11-05):* "it sounds very good … a character I do not find in other firmwares, the
  macros on DISTROY especially".

**C1 done 2026-11-05.**

**After C1: the essence (2026-11-05).** The user on the sequencer demo: "very fast and choppy … can we get
something that evolves more? I want to hear the sonic spectrum. That should be the essence of this synth."
- `sd_render evolve OUT PATCH [SECONDS]`: a slow line of tied and slid quarters at 76 BPM in D minor, while the four
  macros drift on slow curves of their own (37, 23, 29 and 53 s), out of step, MUTATE and BREAK under an arc that
  rises to two thirds and settles. `build/audio/evolve/evolve_*.wav` (VOYAGE, DISTROY, VOWEL, GROWL, FUSION, 90 s): the
  spectrum's centroid wanders 350 .. 1400 Hz.
- What it found:
  - *DC in SPACE.* A warp toward pulse has a DC; SPACE's feedback and its room's lines multiplied it (up to 20 ..
    100 x), DROWN on VOYAGE building a rumble. SPACE's input now loses its DC (~7 Hz) before the echoes and the room;
    the dry keeps it. The mix's DC: −56 dB (VOYAGE).
  - *The macros together clipped* (FUSION, VOYAGE: long tied notes, MOVE, BODY / DROWN up at once). The output now has
    a shoulder: linear to 0.85 of full scale, then `sd_knee`'s cubic to it, so an overload saturates instead of
    clipping; under 0.85 it is the same sample for sample (of the loops only PLUCKS and FUSION reach it). DROWN's
    feedback +3000 (was +8000), BODY's RES +4000 (8000).
- *Fit it:* the shoulder and the DC cost 0.8 %: DISTROY in the loop reads 47.9 % worst, 40.6 % on average.

**The essence, round two (2026-11-05).** The user on the evolve pieces: VOYAGE "definitely the best", but it
"sounds like a random voxel / formant … often with a random sine modulation"; "much better with stepwise modulation
from the randomizer"; GROWL "so noisy it is nearly unusable"; "I always get the best results with [the distortion] on the top
frequencies, and/or modulated, but all modulation must add something rhythmic".
- *Rhythmic, stepped:* MOVE jumps the nav and the Reactor's POS to new random places on each beat (glided ~10 ms)
  instead of its slow sines; an S&H LFO glides ~10 ms into each step (no click, still a step); `sd_render evolve`
  steps the macros on bars (each its own count, a random walk, glided ~80 ms). VOYAGE's LFOs are synced S&H on X (a
  quarter) and Y (a half); DISTROY's warp and POS step on 16ths.
- *DESTROY on the top:* BREAK weights the bands' gain and ALT toward the top (gain +4000 / 12000 / 20000 / 26000 at
  full, low to high). On a patch without DESTROY BREAK sets one up (lows clean, SOFT and HARD on the top two, at
  unity): before, it only faded the voice into an empty DESTROY (−5 dB). Two new destinations, D HIGH (the top band's
  gain) and D SPLIT (the upper crossover, +−24 semitones), so what is destroyed can move in steps. DISTROY's lows
  are nearly clean now, its top heavy, D HIGH stepping each quarter; SPLIT steps D HIGH and D SPLIT.
- *GROWL usable, MUTATE gentler:* measured as the energy off the note's harmonics on a held D2 (aliasing heard as
  noise): GROWL −13.8 dB, at MUTATE full −4.4 (half of it noise). The chain's saw edges under FM alias; GROWL sits
  between triangle and saw now (WP 20000, FD 8000, XM 24000): −24.4 dB. MUTATE adds XM 12000, WP 6000, FD 7000,
  DT 6000 (was 20000, 13000, 16000, 8000): GROWL at MUTATE full −8.2, VOYAGE −12.2 (−8.0). BREAK's Reactor drive +6000
  (FUSION was +3 dB louder).
- *Left:* FUSION's evolve piece is hot (−10.5 dB RMS against −17 for the others: low tied notes into its resonances),
  its peaks on the output's shoulder.
- *Fit it:* DISTROY in the loop reads **48.5 %** worst, 40.8 % on average (47.9 % before: its top bands driven harder
  take the knees' cubic more often). 1.5 % under 50: the next feature that costs audio time pays for itself.

**Round three (2026-11-05): movement inside the note, more patches.** The user: "better, but I miss more
variation … ramp-down curves that modulate within a note … I'd like to hear more patches, it is the same again and
again … not that exciting".
- *LFO ONE* (`SD_LM_ONE`, on MOD: FREE ONE, SYNC ONE): one cycle from each note, then held at its end. A SAW in ONE
  is a ramp down inside each note: the FM dying away, the grit falling, a pitch drop, a split sinking.
- *Six patches* (P15–P20), each built on that and on stepped S&H, the destruction on top, warps under the saw:
  COMET (an FM pluck; FM and grit fall each note, the warp steps on eighths), NEBULA (a pad through AP2; its place
  steps a bar, the fold grows each note), PULSAR (the nav between bell and glass stepping on 16ths; the upper
  split sinks each note), ION (a round bass under a destroyed, panned top; the split jumps on 16ths, the grit falls
  each note), ORBITAL (percussive FM: pitch and FM fall in ~150 ms; RAND places each hit in the field), MONOLITH
  (long notes; the fold steps on quarters, the grit pulses on eighths). Off the harmonics on a held D2: −19 … −32 dB.
- *`sd_render tour DIR`:* ten patches, a phrase each (tempo 64 … 124 BPM, its own register, rhythm, DIV, ratchets
  and chances), the macros stepping in each part's own ranges on its own bar counts, 22 s a part:
  `build/audio/tour/tour.wav` (3.7 min) and `tour_NN_NAME.wav`. Levels −16 … −20 dB RMS (ORBITAL, percussion,
  −26.5); ION's CRUSH was hash (a 6.5 kHz centroid): HARD and TAME now.

**Round four (2026-11-05).** The user on the tour: COMET "full internal feedback", ION the same; NEBULA "pretty,
but nothing special beyond an ordinary FM patch", PULSAR and ORBITAL the same; MONOLITH "extreme ring modulator
sound: no go".
- *BREAK no longer touches DESTROY's feedback.* On a patch without its own, BREAK's push opened a 1 ms loop: a
  comb's howl. DISTROY keeps the feedback it has.
- *ONE is unipolar:* it starts or ends at the patch's own value (a SAW from full to 0, a RAMP from 0). ORBITAL's
  pitch drop had held a semitone flat.
- *No HARD or FOLD over 5 kHz in the new patches:* without oversampling their harmonics fold back as inharmonic
  tones (MONOLITH −26 → −18 dB off the harmonics with its DESTROY, −10 at BREAK 70). MONOLITH: SOFT only, no loop.
- *A bank's WP offset counts twice* (the Q page's scale): the chain banks' +17000 was +34000, past the saw. Now
  +8000.
- *NEBULA, PULSAR and ORBITAL rebuilt on what makes VOYAGE and DISTROY themselves:* NEBULA's character is the Reactor,
  four KP combs on the chord's tones kept on the harmonics (KEY+H), bowed by a soft stack, the chord stepping a bar;
  PULSAR is a spectral sequencer, four banks as far apart as they go (glass, the odd cluster 5 7 9 11, a chain, the
  organ) with X and Y stepping on 16ths and eighths; ORBITAL is a short FM hit landing anywhere in such a field,
  its pitch falling to the note, four BP6 filters on the harmonics ringing after it.

**Round five (2026-11-05): the engines talking.** The user: "all the new sounded much better, but MONOLITH is
completely unusable … drowned in reverb / fast delay / ring modulator. Let's get everything talking together even
better, preferably in unorthodox ways"; and before it: "systems so the outcome is always thought through,
calculated and musical are important, even if it is to be sonically exciting".
- *Six new sources*, the engines as modulators, each from the last tick: SEQ STEP (where the pattern is, −1 … +1
  across it), SEGMENT (which segment), D LOW and D TOP (DESTROY's low and top bands' fast followers, −48 … 0 dBFS
  as 0 … 1), R STAGE (the Reactor's stage, 1 … 6 as −1 … +1), ECHO (SPACE's wet level, followed ~60 ms). They are
  rhythmic by nature: steps, segments, stages and the hits' energy.
- *Cross-wired patches:* DISTROY (the bass pushes the Reactor's stages, the split sweeps the loop), ION (the bass
  ducks the top's grit), COMET (the grit feeds the echoes), VOYAGE (Y walks the loop, the echoes bend the warp),
  ORBITAL (the Reactor's stage picks where each hit lands), NEBULA (the echoes fold it), PULSAR (each segment its own
  fold). The tour's patterns are four segments now, so SEGMENT has something to say.
- *MONOLITH dry:* no delay, a small room, no gating; its FM grows inside each note, its fold rises across the
  loop, the warp steps on quarters, BODY for MACRO 4. Off the harmonics on a held G2 at 3 s: −20.8 dB, GLASS's
  −17.7 there (the measure's floor at that note).
- *Still to build:* the musical guardrails as a system (pitch, time, spectrum, level, a musicality gate in the
  tests), proposed to the user.

**The cinematic demo (2026-11-05).** The user asked for "a short demo layering some sounds … a soundtrack …
big, mighty and alien": `tools/sd_cine.c`, original
material (D minor, 84 BPM, Dm Bb Gm A), eight engines layered as a multitrack (NEBULA's chords, VOYAGE's alien line,
PULSAR's 16th ostinato, ION's bass, ORBITAL's taiko, MONOLITH's drone and braams with DISTROY's, a sine sub), the
macros stepping a bar at a time; −27 dB RMS in the intro to −14 in the climax. The user: "fantastic, but missing a
little depth, and somewhat fierce, screaming high overtones in the lead and in the FM in general".
- *FM up high, in the engine:* a modulator past 4 kHz now loses its index, gone by 12 kHz (its sidebands were the
  screech), and the tracking's knee is C5 (was C6). The energy over 6 kHz on a held note: VOYAGE at D6 −8 → −17 dB,
  at D5 −24 → −26; EPIANO at D5 −19 → −26, at D6 −21 → −56; under C5 nothing changes.
- *Depth in the demo:* the pad and the alien far back (MACRO 4 DROWN at 65 … 90 %), the bass and the drums dry and
  near, a sine sub an octave under the root from the build on, the alien line an octave lower.

**The second cue (2026-11-05).** The user: "use the whole instrument, much more extreme, another sci-fi film
soundtrack: immensely big, laser sounds hidden in the modulation of the overtones, beautiful and hard and distorted at
the same time … but keep it melancholy and beautiful". `tools/sd_epic.c`, original material.
- *First render:* E minor, 70 BPM, ten engines, ~99 s. The cello V4 sang the melody. The lasers were COMET on the
  sequencer with a SAW once a note on PITCH, XM and RFREQ, falling 3.8 → 1.7 kHz in 120 ms. Two DISTROY braams
  dropped their destroyed band through the spectrum (a ONE SAW on D SPLIT).
- *The user, on it:* "at the start the melody lacks a proper filter, and plenty of space", then "drop the pew-pew
  laser, have more parts and higher intensity instead: we are after goosebumps and a wall of sound".
- *The intro's filter and space:* the cello starts closed, with TAME at 1.2 kHz (resonant) and DESTROY's upper
  bands at −14 and −25 dB, then opens on an S-curve over eight bars. Its energy over 2 kHz goes −40 → −16 dB of
  its whole, and its centroid goes 280 → 720 Hz. In A it sits in a long dark hall with a dotted-quarter ping-pong
  delay. That narrows through B and opens again for the ending.
- *More parts:* the laser is gone. There are thirteen engines, ~127 s:
  - three cellos (the melody; a harmony on the nearest chord tone a third or more below; the roots, a long note a
    chord);
  - GLASS's choir an octave over the chord tones, and DISTROY's POLY horns in the low middle;
  - the rhythm parts, the braams, VOYAGE's lead and the sub as before.
- *Intensity:* the climax's phrase plays twice. The second time is a tone up, F# minor, reached from D (its VI).
  Before each climax the rhythm and the drones drop out for a beat. The drums are denser in each climax.
- *The bus (the tool's mix, not the engine):* the master fader rides the form, then a peak compressor (−6 dBFS,
  4:1, 8 / 250 ms, at most −4.2 dB) with +3 dB and a tanh. Bars by RMS: −20 dB in A, −17 … −13 in B, −11 in the
  first climax, −10 in the second, then −14 → −26. The peak is 0.96. Over 6 kHz in the climaxes: −28 / −26 dB of
  the whole (the first cue's −23).
- *The user, on that:* "the last ~40 s glitch and are out of tune with the rest, bars jump … much bigger reverb
  and delay on the elements without much bottom, fast arps high up in the last part, and the modulation should be
  smoother".
  - *The glitch:* the disk was 99 % full, and the render's writes failed without a word, so bars dropped out of
    the file. The tuning measured within 9 cents. `sd_epic` now checks its writes and fails loudly.
    `tests/run_sd.sh` no longer writes its 13 stems (~290 MB); pass a folder as the second argument for them.
  - *The key change:* the second climax is reached through a pivot with no break. Bar 23 is C#7, so D slides a
    semitone down in every voice (D F# A → C# E# G#) into V of F# minor. The melody's F# hangs over the C#7 and
    resolves to E#. The chord-driven parts (pad, choir, horns, sub, low strings, the patterns) now follow each chord
    change rather than every two bars.
  - *Space:* pad, choir, lead, ostinato and the arps sit in a large dark hall with ping-pong delays (dotted
    quarters; dotted eighths for the ostinato and the arps). The cellos never go dry: half their intro wet through
    the climaxes.
  - *The arps:* GLASS on the sequencer in 32nds, up and down the chord tones over two octaves (C5 … B6), short,
    wide and tamed, from the second climax into the ending.
- *The user, on that:* "the full, very staccato synth/bass doesn't follow the key, and the key change didn't work.
  Drop the modulation: go up a third, to a major chord, in a new progression that releases and is at its most
  intense".
  - *Off key:* the 16th ostinato. PULSAR's bank B (harmonics 5 7 9 11) adds a G#, a D 31 cents flat and a
    quarter-tone A/A# over E minor, and MOVE steps X into it. In the cue, B is 2 3 4 6. The share of its energy on
    the key's notes went 57 % → 73 %; the rest is the patterns' passing tones and the delay. MUTATE is a quarter on
    the pitched patterns and zero on the arps.
  - *The release:* no key change. The last climax is G D Em C (G major, a third over E minor), reached from D, its
    V, as a cadence. It has a melody of its own (G4 … A4), and the lead an octave over. The ending is Am Em, down to
    the low E. The C#7 pivot is gone, and the parts follow each chord change as before.
- *The user, on that:* "I miss rising intensity in the last part, where the euphoria should hit hardest. We also
  lack a clear arp going down, quite high. PULSAR is still a little special next to the rest".
  - *Rising:*
    - The bus ride holds the first climax back (−12 dB RMS), so the release has room. It goes −10.5 → −9.1 dB
      over its eight bars, peaking at 0.98.
    - Its parts grow +5 dB across it. BREAK climbs past the first climax's.
    - The drums go to all 16ths with ratchets for the last four bars.
    - A beat's breath comes before bar 28, where the cello reaches its top B4 (the lead B5).
  - *The falling arp:* GLASS in 16ths down the chord's tones from two octaves up (B6 … C5 over G). It rings longer
    than the 32nd arp, in dotted-quarter delays, from bar 26.
  - *The ostinato:* ION, the bass's patch, at the ostinato's pitch, without its 16th grit split. PULSAR is out of
    the cue.

**STRINGS (2026-11-05).** The user, with two recordings (a cello's C major C2 .. C4 and back, a violin's A major
A3 .. A5 and back): "can we get as close as possible to these scales with our instrument? One and the same
instrument. Analyse the waveforms and match as well as you can."
- *Analysed:* each note's harmonics against its strongest, gathered by absolute frequency: the bodies. The cello:
  a weak fundamental low (C2 at −28 dB), its strongest around 160 … 320 Hz, a dip at 400 … 800, a bump at 1 kHz,
  a shelf near −27 dB to 3.3 kHz, falling past 4 kHz. The violin: the air resonance 250 … 400 Hz, a dip at
  650, peaks at 1 and 1.6 kHz, then −16 dB an octave past 2.5 kHz. Bowed attacks of 150 … 500 ms, little vibrato.
- *The model:* a saw through a body. The Reactor's four BP6s on fixed frequencies (no tracking): stages 1-3 the
  cello's body, 4-6 the violin's, KEY moving POS across them, so one instrument goes from the one to the other with
  the keys; DESTROY unshaped as a four-band level and TAME, KEY closing TAME with the pitch.
- *Matched by search* (`tools/sd_strings.c` with a parameter file; a coordinate descent on the levels, splits,
  TAME, the Reactor's RES and MIX and the two bodies' levels): third-octave RMS 14.6 dB at the first guess, 4.0 at
  the end (cello 4.4, violin 3.3). What is left: the cello's lowest fundamentals (+8 … +13 dB: the same body
  cannot cut 65 Hz and keep the violin's 220), the violin's top over 5 kHz (+3 … +7). Its strongest resonances
  reach the Reactor's soft clip (flat tops in the waveform). No bow noise: the engine has no noise source.
- *Round two, "with all the tools we have":* the noise between the harmonics joins the match (the recordings':
  cello −12.7, violin −21.0 dB under the harmonics).
  - Two sources in the field: bank A a saw whose 2nd and 3rd harmonics' saws carry it (the cello's weak low
    fundamental), B a plain saw; KEY moves the nav A → B with the pitch.
  - The bow: each bank's first oscillator an inharmonic modulator (7, +47 cents) under a short ENV2 (XM 20000):
    a scratch at each note, a hiss under it after. It stands in for the noise source the engine does not have.
  - Tried and dropped: DESTROY's CRUSH as hiss on the top band (it replaces the band, it does not add to it).
  - Bodies 3.6 (cello) and 3.5 dB (violin) RMS by third-octave; noise −12.7 and −20.5 dB (theirs −12.7, −21.0).
- *V4, locked (2026-11-05).* The cello duo's rounds by ear (V1 … V4, `tools/cello_v4.txt`) left the violin and the
  KEY morph behind: a plain saw (WARP 25544, attack 70, sustain 118), 8 cents of vibrato at 5.5 Hz, the Reactor's
  BP6s snapped to the note's harmonics (HARM) on stage 2's cello body, DESTROY's four levels (−22 −4.8 −0.1 +6.8 dB
  at 262 750 2380 Hz), TAME 22000, a small room, mono. The user liked it in the duo and in the second cue, and it
  is now STRINGS itself; `tests/run_sd.sh` renders the duo from the patch and from the file and compares them bit
  for bit.

**The second cue's patches (2026-11-05).** The user: "every sound used in the demo is a patch?" Before this, only
STRINGS, ION (the bass) and ORBITAL (the drums) were, and those last two less their SPACE; the rest were stock
patches the tool changed. Now each is a stock patch of its own, after STRINGS (P22 … P33), and the cue renders bit
for bit as before:
- HALO (NEBULA in a big dark hall, dotted-quarter ping-pong, DROWN), AURORA (GLASS slow and wide), TITAN (DISTROY
  swelling, its top tamed), DRIVE (ION without the grit split on the step, dotted-eighth delays), UNDERTOW (ION
  dry), IMPACT (ORBITAL dry);
- BRAAM and SCREAM (DISTROY whose destroyed band falls through the spectrum in a hit: a once-a-note SAW on D SPLIT,
  30 % and 45 %, and on XM), BEACON (VOYAGE with a 5 Hz vibrato, its top tamed, far back), DEPTH (a sine, slow);
- SHARDS (GLASS short and wide, no detune, the 32nd arp) and PRISM (it ringing longer and darker, the falling
  16ths, dotted-quarter delays).
The voice mode (POLY for HALO, AURORA and TITAN) stays the player's (GLO), as it is for every patch. The CPU is
unchanged (the boot patch is still DISTROY: 40.8 % in the emulator).

**The filter, the envelopes on their page, the pages on their buttons (2026-11-06).** The user, on the first package
in the emulator: "everything is under FX: it must be spread to the right place … I miss a lowpass filter: we must
nearly manage to bake one in"; on the envelopes: "is it impossible with today's setup without breaking anything?"
(no: ENV1 and ENV2 were always in every patch, only without a page).
- *The filter:* a lowpass on each voice (POLY: each note its own), on the banks' carriers before the nav's
  balance: one TPT SVF stage (LP12) or a Butterworth pair (LP24), as the Reactor's and TAME's, SD_RG's warp, MIDI
  16–130. RES by SD_FK, even in dB of the peak: k from flat to 0.1 (+23 dB, LP12) and 0.054 (+25 dB, LP24), never
  self-oscillating; the output held to the render's range. The cutoff: CUTOFF + KEY (0–100 %) + VEL (up to +24
  semitones) + ENV 3 (its own ADSR, ±60 semitones); the matrix has ENV3 as a source and F CUT, F RES as
  destinations. All zero is off: every patch before it renders bit for bit as it did (cue 2 and the duo checked).
  `sd_test`: LP12 −31 dB and LP24 −60 dB at 2–5 kHz with the cutoff near 400 Hz, the lows within 3 dB; ENV 3
  opens it +68 dB; full RES in range.
- *The pages, each its button's:* HOME the voice (NAV) and again the macros; FX REACTOR → DESTROY → SPACE; ENV
  ENV 1 / ENV 2 (SEL) and again FILTER (ENV 3 with SEL); LFO the matrix at LFO 1, LFO 2, then the slots; ARP the
  arpeggiator; SEQ A, B. Their LEDs light with their pages.
- *Fit it* (fm1-emu, DISTROY in POLY, four keys held and the loop playing, the audio's average): filter off 36.9 %,
  LP12 38.1 %, LP24 38.9 % of the chip; DISTROY's loop in MONO as before, 41.1 %. The screen's CPU is the worst
  interrupt of the last second, higher (47–53 % here); the user saw 64 % on some patch, not yet found.
  `tools/emu_cpu.sh` takes a scenario now (EMU_KEYS).

**The edge (2026-11-08).** The user wants the plain FM patches to sound as they should, an EPIANO an EPIANO, "but
the Star Distroyer version": a small edge that makes each its own.
- *Which:* the six plain FM patches. Five are V2's one-bank patches (GLASS, BELL, EPIANO, GROWL, ORGAN); the sixth is
  AURORA, which is GLASS as a pad. DEPTH, a sine sub, stays clean, because the lows stay clean.
- *The rule:* `SD_EDGE` in `sd_patches.h`.
  - Under the top split, DESTROY passes the sound as it was (NONE through LR4).
  - The top band alone is destroyed.
  - Something steps on the beat (a synced S&H).
  - It works in MONO, DUO and POLY alike; a twist on the nav would not, since POLY plays bank A alone.
- *Each:*

  | Patch | Its edge |
  |---|---|
  | EPIANO | the tine's top (over 5 kHz) folded and SHARP: a note's bark glints and spits, the body untouched; the drive a new step each eighth, more on harder notes; a dark dotted-eighth echo far back |
  | BELL | four KP combs snapped to its own harmonics (2 3 4 6 and kin), a new set each quarter (RANDOM), wide, the lows past them, a ninth wet; the strike's top folded |
  | GLASS | a fold that takes a new depth each quarter; its top (over 2.5 kHz) folded to a sheen; a small dark room and echo |
  | ORGAN | a rotary that steps: AP2 a quarter wet, its notches jumping each eighth, all over 800 Hz; the upper mids driven SOFT, the top folded a little |
  | GROWL | the top (over 5 kHz) folded, its drive stepping on eighths with the voice's fold |
  | AURORA | GLASS's sheen, its fold a new depth each bar |

- *Found on the way:*
  - **CMP let the body in.** Its gentle slopes gave the top band's shaper the 1.2–5 kHz range too, +5 to +7 dB there
    on EPIANO and GLASS. The edge uses LR4 instead.
  - **LR4 raises peaks.** Its allpass sums flat in level but 1–3 dB higher in peaks, so the macro's BODY (the level
    under the top) gives a hot patch its headroom back: BELL 27500, GROWL 31000.
  - **A shaper needs drive.** At +3 dB, HARD and SOFT on the top added −51 and −36 dB of new content to it, which is
    nothing. Each top is now driven until the shaper's own content is −4 to −8 dB of the top, with the band's level
    set so the top lands +3 to +6 dB over the patch before.
  - **SHARP and the D HIGH slots lift peaks.** VEL → D HIGH alone added 4 dB to EPIANO's top at velocity 100, so both
    are kept small.
  - **ORGAN's allpasses took its body.** In FILL the Reactor adds the allpasses' wet on the dry, and below 800 Hz their
    phase took 2 dB of the organ's body. The stages moved over 800 Hz, and the Reactor went a quarter wet.
- *Hear it:* `build/audio/twist/ab/StarDistroyer_edge_*.m4a`, each the patch's `sd_render scene` before, a second of
  silence, then after.
- *Measured* (the scene, before → after):

  | Patch | RMS (dB) | Peak (dBFS) | 1.2–5 kHz | Over 5 kHz |
  |---|---|---|---|---|
  | GLASS | −21.0 → −21.0 | −5.9 → −7.3 | +1.3 dB | −70 → −41 dB of all: a sheen where there was none |
  | BELL | −18.8 → −18.8 | −3.3 → −2.1 | +0.4 dB | +4.2 dB |
  | EPIANO | −21.8 → −22.0 | −4.1 → −3.1 | +1.6 dB | +6.0 dB |
  | GROWL | −19.4 → −20.4 | −4.1 → −1.2 | +1.0 dB | +3.9 dB |
  | ORGAN | −17.1 → −17.4 | −1.7 → −1.4 | −0.4 dB (L+R) | +11 dB of almost none |
  | AURORA | −25.7 → −25.6 | −4.8 → −4.4 | +0.8 dB | −69 → −40 dB of all |

  GROWL's RMS includes its DC offset, which DESTROY's DC blockers now take off. No patch has a sample at the ceiling.
- *Tests:* `test_edge`. Each of the six plays an A3 in MONO and a chord in POLY, with the engines the edge gave it on
  and off. The body (30 Hz – 1.2 kHz) must stay within 2 dB (the worst is −1.2), the top must rise less than 12 dB
  (the most is +10.3), and no sample may reach the ceiling. `test_mod` uses GLASS as V2 left it.
- *Fit it:* fm1-emu, in POLY with SEQ A and a four-note chord held: GROWL 33.8 %, EPIANO 37.0 %, GLASS 38.0 %,
  AURORA 39.0 %, ORGAN 40.7 %, BELL 44.5 %.
  - *Open:* DISTROY reads **50.9 %** in the same scenario, over the 50 % rule. Each of its four voices plays bank A,
    the GROWL chain in the costliest kernel. Nothing here caused it; that scenario was not measured before.
- *Demos:* the song, RANGE and the demo play GROWL, and cue 2 plays AURORA, so they now play the new patches.

**Four out (2026-11-08, later).** The user listened to the A/Bs: GLASS, BELL, EPIANO and ORGAN still sounded like
classic FM, edge or not. Given the choice of rebuilding them or taking them out, the user took them out. AURORA ("sounds
fantastic") and GROWL stay, with their edge.
- *The bank:* 29 patches, GROWL first. DISTROY, the boot patch, is now P03. The banks live on inside VOYAGE, DISTROY and
  the cue's patches (`BK_GLASS`, `BK_BELL`, `BK_ORGAN`, `BK_EPIANO_LIGHT`); `BK_EPIANO`, used by EPIANO alone, went.
- *By name:* the code names a patch by `SD_P_*` (an enum in `sd_patches.h`), never by its number, and `test_names`
  checks each against its name. `sd_render` takes a patch's name (`sd_render hold out.wav distroy 45`), and
  `tests/run_sd.sh` uses names.
- *Tests:* GLASS and BELL as V2 made them are now the tests' plain sounds (`T_GLASS`, `T_BELL` in `tests/sd_test.c`).
  `test_edge` checks GROWL and AURORA: the body −0.5 dB at worst, the top +10.3 dB at most, no sample at the ceiling.
- *Renders:* AURORA's loop, hold and macros, and POLY on NEBULA and AURORA, take the four's place.
- *Demos:* the song, RANGE, the demo and both cues play the patches they played before, now named.
- *Fit it:* the image is 132184 B (from 134072), and DISTROY in MONO still reads 40.7 %. DISTROY's 50.9 % in POLY with
  a chord is still open.

**DERELICT and the page (2026-11-08).** With four patches gone, the user asked for a new demo: about two minutes, at
most three or four tracks at once, "a little noisy, a little cinematic, a little staccato, a little flowing, a little
beautiful, a little melancholic", all the banks used, the app's own tools and not classic FM, nothing an FM-1 could not
do. It goes on the page with its own player.
- *How:* `tools/sd_derelict.c`. There are four passes on one FM-1, each in its own process (a fresh FM-1) under
  `tools/sd_play.h`'s rules, so the tool stops at any gesture the FM-1 could not make. They are recorded to the same
  tempo and layered at fixed faders, with one master gain to a −1 dBFS peak and nothing else added.
- *The music:* C minor, 84 BPM, 40 bars.

  | Part | Bars | What plays |
  |---|---|---|
  | DRIFT | 1–8 | AURORA's chords in POLY swell in (MASTER from nothing). DISTROY is held on C2, its four banks at the centre, with BREAK bringing up the noise and MOVE orbiting the nav. It is cut at the bar line. |
  | SIGNAL | 9–24 | PULSAR's staccato 16ths in MONO, with a ratchet and PROB, its nav jumping across its four banks. SCL + key moves it with the chords, and its figure of roots, fifths and ninths has no third to clash. UNDERTOW plays eighths, and the STRINGS cello comes in at bar 17. |
  | BREACH | 25–32 | The chords move to Fm, Abmaj7, Cm9 and G7. PULSAR holds a pedal C with TURING rewriting it (CHANGE 35, RANGE 4); its top band goes to CRUSH with the GAIN up, and BREAK and MOVE rise. The bass's WARP turns toward the saw, and the cello climbs with its Reactor on the brighter stages. |
  | AFTERGLOW | 33–40 | BRAAM hits on the downbeat. PULSAR fades out by MASTER, the cello's last phrase falls to a low C, and the chords drown under MACRO 4, then MASTER lets the hall go. |

- *Levels:* the faders are set from each pass's RMS part by part. In SIGNAL, with all four playing, the cello is
  −20 dBFS, the chords and the bass −25.5 and the pulse −28.6. The whole piece reads RMS −19.4 dBFS (the song is −18.5
  and RANGE −20.1). The first faders put BREAK's spikes on the pulse at bar 32, 11 dB over everything else, so BREAK
  went to 50, CRUSH's GAIN to 50 and the pulse's fader down.
- *Not heard:* the levels and the noise are set by measurement only. Over 5 kHz, the mix sits about 29 dB under the
  whole in DRIFT and BREACH, and about 35 dB under in SIGNAL and AFTERGLOW. Whether that is "a little noisy" is the
  user's call.
- *Heard, and rebuilt as an overture.* The user said it had "a beautiful sound, but only two parts. More an
  overture, its themes closer together; the lead with much more reverb and delay, and lower." So there are now ten
  sections of four bars with a chord a bar, and four themes that follow each other, then come back changed:

  | Bars | Section | What plays |
  |---|---|---|
  | 1–4 | THE CALL | BRAAM's hits on C, Ab and G, under AURORA's chords swelling in |
  | 5–8 | THEME I | the cello's melody over the chords and the bass's long roots |
  | 9–12 | THEME II | PULSAR's staccato 16ths under the cello's answer; the bass in eighths |
  | 13–16 | THEME III | BEACON's high line far back (VOYAGE's banks, COURSE), PULSAR's glints |
  | 17–20 | THEME IV | a pedal C in Phrygian, saved with TURING on (CHANGE 35, RANGE 4), its top band to CRUSH, BREAK and MOVE up; the bass in 16ths, its WARP up |
  | 21–24 | I over II | |
  | 25–28 | III on the cello | |
  | 29–32 | IV again, harder | CHANGE 50, BREAK 66 |
  | 33–40 | the coda | BRAAM, the cello down to a low C, the chords drowning under MACRO 4 |

  Then the user said: "one place is a little disharmonic, and it can be shorter, so there is less repetition." The
  breach was the place. Its G7 bar sat under a pedal in C Phrygian that TURING rewrites, and the scale's Db, Ab and
  Eb clash with G7's B and D. The breach's chords are now Fm, Db, Ab and Fm, all in the pedal's scale, and the repeats
  (III on the cello, IV again) are gone. That leaves 32 bars, 1:41:

  | Bars | Section |
  |---|---|
  | 1–4 | THE CALL |
  | 5–8 | THEME I |
  | 9–12 | THEME II |
  | 13–16 | THEME III (BEACON; its last half bar a rest, so the change back to U01 cuts no note) |
  | 17–20 | THEME I over II |
  | 21–24 | THEME IV, the one breach: CHANGE 45, CRUSH's GAIN 58, BREAK 62, MOVE 65 |
  | 25–32 | the coda |

  The levels, per section: the cello is −27.2 to −28.2 dBFS against the chords' −26.7 to −27.1, and the breach is the
  loudest part (mix −18.7 dBFS, over 5 kHz −47). The whole reads RMS −21.3 dBFS.

  The lead is U01, the player's own place: STRINGS with its SPACE opened and saved (SYNC PP, TIME 1/4D, FDBK 52,
  SIZE 90, DECAY 84). Its MASTER sits at 1500, so it is at the chords' level (−28.7 against −28.0 dBFS in THEME I,
  where before it was 6 dB over). MASTER goes to 4092 for BEACON. The mix reads RMS −21.8 dBFS, with its loudest
  bars in the second breach (bars 29–32).
- *A bug it found (fixed 2026-11-08).* The first CI run of the site crashed in DERELICT's third pass on Linux. It
  ran cleanly on the Mac. Under the sanitizers it was `sd_engine.h`'s `sd_bank_run` writing `bus[254]`.
  - *Why:* an oscillator that becomes a modulator with nothing to its right gets `SD_NOWHERE` as its target. That
    happens when a patch, ALGORITHM or a generator changes a bank's roles while it sounds; here it was BRAAM →
    UNDERTOW. For one sub-block it kept its old level, so it was not skipped and was written past the buses.
  - *What it means:* on the FM-1 this corrupts memory.
  - *The fix:* `sd_bank_params` now silences such an oscillator at once.
  - *The guard:* `test_nowhere` catches it under UBSan. `tests/run_sd.sh` also runs DERELICT under the sanitizers,
    and a demo that stops now shows the end of its log. Before, the log went to `performance.txt` unseen.
  - *Fit it:* DISTROY in MONO reads 40.8 %.
- *CINEMATIC remade, and back (2026-11-08).* The user asked to put the cinematic demo on the page in THE SONG's
  place, "remade the way we would play it; changing the parameters is allowed". It was remade as four passes on one
  FM-1 under DERELICT's rules (HALO; BEACON then IMPACT; PULSAR; MONOLITH, UNDERTOW, BRAAM and DEPTH). The user heard
  that "it has lost what it had": most of the alien's line, the macros stepping each bar, the sub through the build,
  and its eight layers. So the first one (eight parts layered as a multitrack) is back as it was, and it is the
  page's CINEMATIC. What stays from the remake is the shared layering in `tools/sd_layer.h` (DERELICT uses it, bit for
  bit as before).
- *RANGE fine-tuned (2026-11-08).* The user: "it must be fine-tuned and pull things further to the extreme, still
  musical; both the first and the fourth patch have internal feedback toward the end." Measured bar by bar (the share
  over 5 kHz, and the 2–12 kHz band's flatness, 1 = noise):
  - *Scene 1 (VOYAGE):* at bar 7, over a third of the sound was above 5 kHz (−3.9 dB of the whole). XM sat at 70
    under the generators RND and CLUST. Now XM rises to 66 and comes back to 38, WARP comes back toward the sine, and
    the generators are spun past RND and CLUST to MAJ (4 5 6 8), then DOM7 (4 5 6 7). MIN and MIN7 are harmonics 10 to
    20, 3 to 6 kHz on a D4, which is shrill. Bar 7 now reads −12.3 dB and bar 8 −20.1, both tonal.
  - *Scene 4 (DISTROY):* BREAK at 85 raised the band gains inside DISTROY's own loop, and it howled (flatness 0.10 at
    bars 6–9). Now BREAK goes to 62 and MOVE to 78. On DESTROY's LOOP, FDBK comes down to 7 and TAME goes up to 45, and
    then the top band's GAIN rises to 86. The end reads flatness 0.04, the top −18 dB.
  - *Further, every scene:*
    - Scene 2: RES 66, and the filter opened to 54000 with RES 76 over the last bar and a half.
    - Scene 3: the Reactor's DRIVE 44 and RES 74.
    - Scene 5: the arp over four octaves at the end.
    - Scene 6: TURING's CHANGE 60, the tempo to 132.
    - Scene 7: SPACE's FDBK 76 and DECAY 92.
  - *Playable:* the harness refused three gestures on the way, and the score was changed for each: the filter opened
    too fast, a transpose made off its page, and SELECT turned back too fast.
  - *Levels:* MASTER evens out scenes 2 and 3 (3400 and 2000 in the bars before them). Every scene now reads −18.2 to
    −20.6 dBFS RMS, and the whole −20.1, as before.
- *The page:* `web/` holds the page, its installer in the browser (SLOOP's, unchanged), and the demos with their
  spectra as seek bars. CI builds it on every push to `main` (the site job in `.github/workflows/release.yml`).

**HW, the checklist (prepared 2026-11-05, brought up to the Firmware Hub's author checklist and the unified
controls; on hold: the user has no FM-1 yet, so the work goes on without it).** The package:
`build/stardistroyer.fwsc` (`./build.sh`, every check ok; identity FM-1_96200, a dev build; a release X.Y is
FM-1_962XY), installed with `python3 tools/fm1_install.py build/stardistroyer.fwsc` (needs
`pip3 install mido python-rtmidi`; it asks before it writes). The way back: the FM-1 Firmware Hub, or OCT- held at
power-on for the USB rescue.
0. *Before anything:* a full flash backup (M-Flasher or MvaveFM1Unbricker, both chips), kept off the unit. Note
   the flash part: 0x856014 (the first units) or 0xB36014 (the UC25HQ80, units sold since autumn 2026); both must
   save and update (`firmware/hal/fm1_flash_map.h`).
1. *It starts:* the splash, then DISTROY on the NAV page; no crash screen.
2. *Fit it:* PLAY, DISTROY in the loop: the CPU readout's worst against the emulator's 41.2 % (POLY with SEQ and
   ARP: 44.0 %; the emulator counts instructions, the chip cycles: the gap is what this step measures). Then the
   other patches with PRESETS.
3. *Hear it:* the loop of DISTROY, VOWEL, PLUCKS, CHORDS and VOYAGE through the line out against
   `build/audio/loop_*.wav`; no clicks at the knobs, none when SPACE's TIME moves.
4. *The pages:* every page button through its pages as `tests/scripts/u1.txt` tours them (HOME NAV MACROS; FX
   REACTOR DESTROY SPACE; ENV ENV FILTER; LFO LFO1 LFO2 MOD; ARP; SEQ SEQ A, SEQ B, TURING); EDIT switches ON/OFF,
   SCL held is the second layer; the key LEDs follow the screen; the macros move the sound live.
5. *The voices:* `tests/scripts/v4.txt` by hand: ARP refused while SEQ plays in MONO, B locked in MONO, the
   messages; DUO and POLY share the voices without a note cut.
6. *MIDI:* notes over USB and the TRS jack; CC 1 (WHEEL) feeds DISTROY's loop; channel pressure (AFTER) on GLASS
   opens the FM.
7. *Flash:* the octave, tempo, patch and mode kept over a restart (saved when silent); a user patch and a pattern
   SAVEd and back after a power-off.
8. *The update path (the hub's author checklist):* install from the official V15 and from another firmware of the
   hub, both ending with Star Distroyer running; from Star Distroyer, install another firmware (it must start that
   package's loader); unplug once during Writing, plug in, install again: it must go on. `fm1_install.py --info`
   (and the hub) must see the FM-1 as MIDI with the identity FM-1_96200 under a port name the hub knows.

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
7. **SPLIT DAMAGE** — Destroy: clean lows, crushed mids, folded highs, PHAT on the low band.
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
| DESTROY: 6 SVFs + 4 bands × 2 ch × ~35 + feedback + TAME | ~550 | 20 % |
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
