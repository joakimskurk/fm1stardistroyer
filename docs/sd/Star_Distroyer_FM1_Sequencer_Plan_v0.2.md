# Star Distroyer — Sequencer Plan

**Status:** Plan v0.2, companion to *FM1 Design & Feasibility v0.1* (written under the working name Skurky Vector Machine), section 6
**Instrument name:** Star Distroyer (decided 2026-11-02)
**Priority:** sound first. The sequencer waits for VOICE, RIDER and DESTROY (§7).
**Scope:** Only the sequencer is advanced beyond the baseline spec. It stays a standard step sequencer; the one
addition is **segments** (§3).

---

## v0.3 revisions (the user, 2026-11-05) — these override what follows

1. **Simpler, closer to the stock FM-1 sequencer, segments kept.** V1 is what the stock sequencer does plus
   segments A–F (REPEAT · ORDER · SKIP · LENGTH, cut and join on the white keys). Out of V1:
   parameter locks and motion lanes (§6.1–6.2 of the spec, §4.1–4.2 here), RATCHET and PROB unless the stock
   sequencer has them. The rest of this document stays the reference for how segments behave.
2. **A Turing machine / randomizer (MUTATE).** It rewrites the sequence (or generates notes from a
   shift register), but it does **not** mutate while playing unless switched on; switched on, it mutates once at
   each loop's one: the loop plays as it is, and the next one is a
   variation. One UNDO for the last rewrite.
3. **Storage, whatever is best and fits.** Dinghy has settings (4 KiB), 8 blobs of 4 KiB and 8 slots of 16 KiB.
   A patch with its modulation is ~440 bytes, a V1 pattern ~0.5 KiB. Proposed: 32 user patches in slot 0, 32
   patterns in slots 1–2, the rest kept free; saved on SAVE only, with the sound stopped (an erase stalls the
   audio for tens of ms).


**Built 2026-11-05 (S0–S2 of §7, the v0.3 way).** The user, on the plan: "simpler and more like the original FM-1
sequencer, but with segments"; "it should look a bit different / sci-fi, but nobody should scratch their head";
MUTATE "off by default, 0 %"; storage "whatever is best and actually fits".
- `firmware/src/sd_seq.h`: the pattern (282 bytes: 64 steps of note, velocity, gate and type; LENGTH DIV SWING
  MUTATE SCALE; segments A–F), the segment editing (cut and join, EVEN, LENGTH, the rescale), RND and
  MUTATE in a scale on the first step's note.
- The engine plays a copy, handed over as the patch is: the traversal a pass at a time (the next pass built when
  one starts, so a TIE or SLIDE ahead keeps the note on past its gate), SLIDE a glide of the held voice (~30 ms),
  MUTATE in the audio at the loop's one (the main loop takes the result back, undoable). No play list double buffer
  is needed: a pass is built in the audio, at most 126 steps, once per pass.
- The default pattern is the loop the engine played before (A2, 55 %): every render is the same sample for sample.
- The SEQ page has its own button. The white keys are a bank of steps, the black keys RND UNDO CLR (SEGMENT: A–F,
  EVEN); the bottom line always says what the keys do. REC records on any page. SAVE keeps 32 patches (slot 0,
  14.6 KB) and 32 patterns (slot 1, 9 KB), in the pool, written when the sound is quiet.
- RATCHET and PROB, added the same day. The user: "not as extreme and fast as it often gets …". So a
  NOTE ratchets 2 or 3 times (not 4), its hits never closer than 50 ms (fewer hits where a step is shorter: at
  240 BPM a 16th has one), each hit x 0.75 softer than the one before; PROB in 5 % steps, a step that loses its
  draw is a rest. On the page: the step tapped again shows its second layer (K1 RATCHET, K2 PROB), and the hint
  says so; a ratchet cuts its tower, a PROB under 100 puts a red dot over it. `sd_render pattern` plays a 32-step
  line with all of it (`build/audio/q1_pattern_*.wav`); `tests/scripts/q3.txt`. fm1-emu: 47.1 % worst, 39.9 % on
  average (within the readout's noise of before).
- Not built: parameter locks, motion lanes, MIDI clock.
- *Fit it:* fm1-emu, DISTROY in the loop: the worst interrupt 46.9 % (46.6 % before), 39.7 % on average. Image
  110.8 KB, RAM 48.0 KB, pool 276 KB of 344 KB. Tests: `sd_test` (segments, orders, skips, DIV, SWING, TIE, SLIDE,
  MUTATE at the one, the editing), `tests/scripts/q1.txt` (the page, segments, REC), `q2.txt` (SAVE and recall).

**Built 2026-11-06: chords, track B, the arpeggiator.** The user, on the hardware build: "SEQ is good, but cannot
record chords in POLY: either chords, or actually two separate sequencers. I want to be able to choose either: then I
can have a melody and a bass line with the same patch at the same time"; "ARP does not work"; "they must reset to
one after the given length, default 16".
- *Chords:* a step keeps up to three more notes as semitones from its note (`SdStep.chord`, 9 bytes a step now), so
  they move with it (RND, MUTATE). REC: keys held together write one step, its first key the note, the rest the
  chord; step recording steps on once every key is up. A chord step plays all its notes (DUO two of them, POLY
  four), ratchets and gates them together; a SLIDE glides a single note only. On the page: a red cap on its tower,
  the notes named on the bottom line.
- *Track B:* the engine plays two tracks (`SdSeqT`) on the one sound, one tempo and one PLAY, each its own pattern,
  LENGTH, DIV, segments, MUTATE. SEQ: A, again B, again back. B starts empty (every step a rest) and is silent
  until it has notes, so there is no switch to forget. SAVE and PRESETS on SEQ work on the track shown.
- *LENGTH:* both tracks restart at step 1 after their LENGTH (16 by default); REC never writes past it.
- *The arpeggiator* (`sd_arp_*`, in the audio, on the tempo): the keys' notes (and MIDI's) in a list in the order
  played; UP DOWN UP-DN RND ORDER over 1–4 octaves, RATE 1/32 … 1/4, GATE, HOLD. The ARP page; ARP there turns it on
  and off. The player's settings, kept over a restart.
- *Storage:* the 32 patterns are 19 KB now, past a slot: slots 1 and 2 hold 16 each. Patterns saved before read
  as none (their size changed).

**Built 2026-11-07: one grammar for the controls, TURING, the transpose.** The user, after playing it a lot: "a bit
unintuitive, especially how things are switched on and off"; "hard to understand the sequencers and the ARP; I cannot
find the Turing machine"; "K1-4 lie in a row on top, but every setting is drawn in a column: we need an indication";
"use the keys' lights for what is on the screen"; "in MONO it should be impossible to switch on the extra
sequencer"; "a mode to transpose a track: SCL, the keyboard shows where it is, the change at the next one"; "EVOLVE
and Turing should be the same". On the choices: EDIT is ON / OFF ("a toggle, not hold"), SCL held the second function
(on the sequencer: the transpose), the knobs' row on top, near the knobs.
- *Every page the same:* the button's pages on the top line (the current one a red tab, EDIT's ON / OFF on the
  right), K1–K4 as four boxes in a row, the picture, the hint line (what ALGORITHM does), the steps, the status.
- *TURING* is SEQ's third page (SEQ A → SEQ B → TURING): the old MUTATE with its own switch (`SdPattern.tur`, off by
  default), K1 CHANGE (0 a locked loop), K2 RANGE (degrees either way, `SdPattern.range`), K3 RESTS, K4 LENGTH,
  ALGORITHM the track. Step 1 keeps the key. The steps it changed last are underlined red.
- *Tracks:* EDIT switches a track ON / OFF (`SdSeqT.mute`: silent, in time). B is silent in MONO (`quiet`), its
  switch shows MONO and EDIT says "B NEEDS DUO OR POLY: GLO".
- *Transpose:* SCL held on SEQ or TURING, a key sets the track's transpose from C4 (or K4 XPOSE, ±24), taken at its
  next one (or START); the keys show it (C4 a glow, its place full, blinking while it waits).
- *SEG* moved from EDIT to the last black key (F#5); the black keys glow as the actions they are.
- Tests: `test_switches` (EDIT OFF is the engine at level 0, bit for bit; a track OFF; B in MONO; the transpose at
  the one), the TURING switch in the MUTATE test; `tests/scripts/u1.txt` tours every page. fm1-emu: 40.7 % on
  average, as before.
- *The voices' rules* (the user: "notes get cancelled, especially with ARP"; "no ARP while SEQ is on and playing in
  MONO, the same for SEQ B; only DUO / POLY can fix it, and there too rules, so we never try to play more than is
  possible; simple messages in English"). Each voice knows who played it (`SdVoice.src`: the hand, A, B, ARP) and
  only that source turns it off; `sd_plan` gives each a quota each tick (a playing track its widest step,
  `SdSeqT.width`, A first, then B, ARP one, the hand the rest) and no source takes another's held voice. The app
  keeps what is on inside it: ARP and B refused with a message, switched off when PLAY, GLO or a chord takes their
  room, chords recorded only as wide as the room left; REC lets the keys take the recorded track's voice. Before:
  A cut 100 % of its steps by ARP in MONO, in DUO with B, in POLY with chords and B; now never (`test_switches`).
  Every render bit for bit as before; fm1-emu 41.2 % (+0.5).

---

## 0. Decision in one paragraph

A monophonic 64-step sequencer (note, velocity, gate, tie, slide, ratchet, probability, mute/skip) with parameter locks
and 2–4 motion lanes, as the baseline spec says. On top of it, segments: the sequence is cut into up to
six segments A–F, each with REPEAT 1–4, ORDER (FWD, REV, P-P, RND), SKIP and LENGTH, edited by cutting and joining on
the white keys.

---

## 1. FM-1 restrictions that shape the design

| Restriction | Consequence for the sequencer |
|---|---|
| 27 keys: **16 white**, 11 black | White keys = 16 step positions. 64 steps = **4 banks** of 16, OCT-/OCT+ changes bank (Control Grammar: "For domains longer than 16 positions, OCT DOWN/UP may change the edit bank"). Black keys = action rail: 6 for A–F + EVEN fits (B0–B5, B7). |
| **4 encoders** K1–K4 + SELECT | Each page/layer has exactly four values. SEGMENT stays one layer: REPEAT · ORDER · SKIP · LENGTH. STEP gets layers on SELECT (TRIG, NOTE, LOCKS, SEQ). |
| **240 × 240** screen | 16 steps per bank drawn large; a 64-step overview strip on top (64 × 3 px = 192 px) with segment colours, so the whole structure is visible from any bank. |
| Dinghy: one audio interrupt, 256 frames @ 44.1 kHz (5.8 ms), rendered as 8 sub-blocks of 32 (Sound Plan §2.2) | Sequencer ticks at sub-block boundaries inside the audio interrupt; event jitter ≤ 0.73 ms. No rebuilds, allocations or flash access in the audio context. |
| DSP budget is the scarce resource | Sequencer + locks + motion budgeted inside the spec's "mod/seq/control 5–10 %". It must be integer/lookup work only. |
| RAM: no measured figure yet | Everything is static and bounded (§5). Total sequencer runtime state well under 2 KB. |
| Flash: writes stall; not from audio | Save only on SAVE, never on edit. A pattern must fit one erase sector (4 KiB on Dinghy; widen `FL_RANGE_OK` for the pattern area, save only when silent). |
| Vector mode is **one** large voice; Poly is 4 × 4-op | Sequencer is monophonic. TIE and SLIDE are well-defined in Vector mode; in Poly mode SLIDE means legato on the same voice. |
| Platform: **Dinghy** (decided, §8) — MIDI IN on TRS from 0.4 | MIDI clock sync and note input via TRS. |

---

## 2. Step model (the standard part)

Per step:

| Field | Range | Note |
|---|---|---|
| NOTE | MIDI 0–127, shown in scale | Written from the keyboard in REC (KEYS) mode |
| VEL | 1–127 | |
| GATE | 5–100 % | |
| TIE | on/off | Holds into the next **played** step (playback order, not step number) |
| SLIDE | on/off | Glide into the next played step |
| RATCHET | 1–4 | Within the step |
| PROB | 0–100 % | Evaluated each time the step plays |
| MUTE | on/off | Step keeps its time, makes no sound (tap a white key) |
| SKIP | on/off | Step is removed from traversal |

Pattern: LENGTH 1–64, DIV, SWING, RESET (FREE or 1–8 BAR), tempo.

**Not taken:** PULSES (a step held for 1–8 clocks), which a segment engine flattens into its play
list; here a step is one clock long and TIE covers held notes. This halves the play list and keeps the sequencer
"standard". Can be added later without changing segments (§4.4).

---

## 3. Segments

### 3.1 Data (per pattern)

```c
#define SD_MAX_SEGMENTS 6u

uint8_t seg_count;                       /* 1..6 */
uint8_t seg_splits[SD_MAX_SEGMENTS - 1u];/* first step of segments B.. */
uint8_t seg_repeats[SD_MAX_SEGMENTS];   /* 1..4 */
uint8_t seg_direction[SD_MAX_SEGMENTS]; /* FWD, REV */
uint8_t seg_mode[SD_MAX_SEGMENTS];      /* LINEAR, PINGPONG, RANDOM */
uint8_t seg_skip_mask;                   /* bit per segment */
```

6 segments, not 8: six fits the black-key rail next to EVEN.

### 3.2 Playback semantics (from `build_segment_runtime` / `advance_segment_traversal`)

- A segment's base order is its non-skipped steps, forwards, or backwards with REV.
- **P-P**: base + base reversed without its endpoints (`a b c d c b`), length 2n−2.
- **RND**: Fisher–Yates over the base order. Shuffled when the play list is built (on START and on edit), —
  not on every pass.
- **REPEAT**: the segment plays n times before the next one.
- **SKIP**: the segment plays nothing; the next one follows.
- Segments play A → F in order and wrap.
- **RESET n BAR** restarts all segments together after a fixed musical length; FREE lets the segment structure set the
  loop length (sum of length × repeats).

### 3.3 Editing (from DECISIONS.md "Segments made easy")

- **Where:** a white key cuts — a segment starts at that step, the new half copies the old one's settings and is
  picked. Where a segment already starts, the key joins it to the one before. Step 1 never cuts; no seventh segment.
  Keys are lit where segments start; the playhead flips its key.
- **Which:** B0–B5 pick A–F (dim where none); B7 **EVEN** spreads the segments evenly (64 in 3: 21, 21, 22).
- **What:** K1 REPEAT 1–4 · K2 ORDER (FWD, REV, P-P, RND) · K3 SKIP · K4 LENGTH (the segment's end moves, the ones after
  keep their lengths, the last gives way; the last segment's LENGTH moves its start).
- **LEN change:** segments scale with the pattern length.
- **New pattern:** 16 steps in four segments of four, forwards, once each — plays like a plain sequencer, ready to
  change.

**FM-1 addition for 64 steps:** cutting needs the step's bank, so OCT-/OCT+ moves the edit bank on SEGMENT as on STEP.
The 64-step overview strip shows all cuts at once; the bank in view is framed.

### 3.4 Picture

Each segment's letter and its order where not forwards (`<`, `<>`, `?`) over its steps; a white cut where one
starts; a skipped one struck through in red; under them one pill per segment with letter, length and × repeats.
Segment colours A–F recur on HOME, STEP and SEGMENT.

---

## 4. How segments meet the rest of the instrument

These are the decisions segments force once there are locks and motion lanes.

### 4.1 Parameter locks follow the step

A lock belongs to a step, not to a time position. In a REV, P-P or RND segment the step plays its own locks in its
new place. This is what makes segments useful for sound design: `RIDER.POS` locks on steps 1–4 played P-P give a
vowel that goes there and back without new data.

### 4.2 Motion lanes follow the step, interpolating along playback order

Motion is stored one value per step per lane (baseline spec §6.2). At playback the value interpolates from the current
step's value toward the **next played step's** value, so a reversed segment reverses the motion smoothly instead of
jumping. Recording writes to the step under the playhead, so recording in a REV segment writes backwards — the same
rule as locks, no special case.

### 4.3 TIE, SLIDE and probability follow playback order

TIE and SLIDE connect to the next played step (which may be in another segment, or the same step again under REPEAT
of a one-step segment). PROB is drawn per play, so a repeated segment can vary.

### 4.4 Sequencer events into the modulation fabric

Cheap, useful sources for the global matrix (baseline §7):

- `SEQ.STEP` (0–1 over the pattern), `SEQ.SEG` (segment index), `SEQ.SEG_PHASE` (0–1 inside the current segment)
- a trigger on SEGMENT ENTER

`SEQ.SEG_PHASE → RIDER.POS` or `VECTOR X` gives each segment its own sweep without a single lock. Optional for V1, but
essentially free.

### 4.5 Not in V1

Per-segment overrides (transpose, a Rider state, a macro offset per segment). Attractive, but they add a data layer
and a UI layer; revisit after V1 is in hands.

---

## 5. Runtime architecture

```text
main loop (UI, edits)                     audio interrupt, per 32-frame sub-block
─────────────────────                     ──────────────────────────────────────────
edit pattern ──► build play list          clock += block; while step due:
                 (≤ 128 step indices        read active play list (pointer)
                  + segment table)          advance segment position / repeat
                 into the idle buffer       resolve PROB, RATCHET, TIE, SLIDE
             ──► publish at step boundary   apply locks → control snapshot
                 (one pointer swap)         emit note events to voice allocator
                                            interpolate motion lanes (control rate)
```

- **Play list of step indices**, not of pulses: at most 64 steps × 2 (P-P) = 128 bytes, plus a segment table
  (offset, length, project segment) of 6 × 4 bytes. Double-buffered: ≈ 2 × 160 B.
- Built in the UI context, never in the audio callback; swapped atomically at the next step boundary, so an edit never
  glitches the running pattern. (With the main loop and the audio interrupt both touching it, a double buffer is the safe form.)
- Advancing is an increment, a compare, a modulo. O(1) per step.
- RND reshuffles on START and on edit through the same build path.

### Memory per pattern

| Part | Size |
|---|---|
| 64 steps × ~6 B | ~384 B |
| Segments | ~24 B |
| Locks, sparse pool: 256 × (step, param, value) | ~768 B |
| Motion: 4 lanes × 64 × 1 B | 256 B |
| Header (length, div, swing, reset, tempo) | ~16 B |
| **Total** | **≈ 1.5 KB** — one 4 KB sector with room |

Runtime (play lists, transport, lock snapshot): < 1 KB. The lock pool size is the first number to tune if a pattern
must shrink.

---

## 6. Pages and controls

The control grammar: PLAY = transport, REC = record, EDIT = edit state, white keys =
positions, black keys = action rail, SELECT = layers, K1–K4 = musical values.

| Page | SELECT layers | K1 · K2 · K3 · K4 | White keys | Black keys |
|---|---|---|---|---|
| HOME | — | MUTATE · MOVE · BREAK · MACRO 4 | tap: mute step · hold: step on knobs | UNDO, … |
| STEP | TRIG | NOTE · VEL · GATE · RATCHET | pick step (bank on OCT±) | TIE · SLIDE · MUTE · SKIP |
| STEP | NOTE | NOTE · OCT · PROB · — | | |
| STEP | LOCKS | 4 lock slots of the step (STEP + param elsewhere creates them) | | CLR LOCK |
| STEP | SEQ | LENGTH · DIV · SWING · RESET | | |
| SEGMENT | — | REPEAT · ORDER · SKIP · LENGTH | cut / join | A B C D E F · EVEN |
| MOTION | — | lane 1–4 depth / select | pick step | REC lane, CLR lane |

REC with EDIT on = live recording; stopped, a key writes the next step (step recording).

---

## 7. Work plan

**Sound comes first.** Phase 0–3 (benchmark harness, VOICE, RIDER, DESTROY) get the attention; the sequencer does not
run in parallel with them. The only sequencer work before then is a minimal note driver for auditioning patches
(a fixed 16-step loop, no segments, no locks) — enough to hear VOICE → RIDER → DESTROY move in time. S0–S6 start
when Gate D (musical identity) is passed for all three engines. Because the sequencer has almost no DSP risk, waiting
costs nothing: the design here is settled.

| Step | Content | Done when |
|---|---|---|
| S0 | The segment code (the bounds, the runtime, the traversal; a cut or join, EVEN, a length, the rescale) without pulses; its editing tests | Host tests pass; the play order is right for every config (golden comparison test) |
| S1 | Standard sequencer: steps, transport, DIV, SWING, RESET, TIE, SLIDE, RATCHET, PROB, MUTE, SKIP; test voice | Plays in fm1-emu; jitter ≤ one 32-frame sub-block measured |
| S2 | Double-buffered play list (main loop builds, interrupt reads); SEGMENT page with 64-step strip and banks | Editing during playback never glitches (stress test: random edits every step for 10 min) |
| S3 | Parameter locks (sparse pool), STEP + param grammar | Locks follow steps under REV/P-P/RND (test) |
| S4 | Motion lanes, interpolation along playback order, REC + param | Reversed segment reverses motion without jumps (test) |
| S5 | Pattern save/load in one flash sector; `SEQ.*` mod sources | Save/load round-trip; no flash access from audio (asserted) |
| S6 | Connect to real VOICE / RIDER / DESTROY | Seq + mod inside its 5–10 % CPU budget under the worst-case patch (Gate B) |

### Validation gates for the sequencer

- **Timing:** no step late by more than one 32-frame sub-block (0.73 ms) under the worst-case DSP patch.
- **Segment fidelity:** the golden comparison in S0 holds for every combination of 1–6 segments × orders × repeats ×
  skips (generated, not hand-picked).
- **Edit safety:** no edit is ever applied mid-step; no rebuild in the audio context.
- **Identity:** a 16-step pattern in four segments of four, with one segment on P-P and one on RND, must sound like a
  segmented sequence, not a plain loop.

---

## 8. Decisions (all made 2026-11-02)

1. **Platform: Dinghy** (0.3, with 0.4's MIDI IN on TRS). Phase 0's benchmark harness is built on Dinghy; if it shows the DSP
   lacks headroom there, that is a new decision, not a silent switch.
2. **PULSES: not in V1** (§2). TIE covers held notes. Adding it later only changes how the play list is built.
3. **Six segments A–F** for 64 steps, to fit the black-key rail.
4. **RND reshuffles on START and on edit**. No "every pass" variant in V1.
