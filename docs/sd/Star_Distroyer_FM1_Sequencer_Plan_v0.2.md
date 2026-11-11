# Star Distroyer — Sequencer Plan

**Status:** v0.3, built. A step sequencer close to the stock FM-1's, with segments, two tracks, an arpeggiator,
ORBIT over them, and TURING as a modulation source. The code: `firmware/src/sd_seq.h` (the pattern and its editing),
`sd_engine.h` (the playing, the timing, ARP, TURING), `stardistroyer.c` (the pages). What follows is the reference for
how it behaves; how it got there is in the git history.

---

## 1. The design (v0.3)

1. **Simple, close to the stock FM-1 sequencer, with segments:** the stock sequencer's steps plus segments A–F
   (REPEAT · ORDER · SKIP · LENGTH, cut and join on the white keys). Not built: parameter locks and motion lanes.
2. **Nothing generative changes the pattern unasked:** RND rewrites the steps when pressed (UNDO takes it back);
   TURING is a modulation source in the patch, its NOTES moving SEQ A's notes in the scale as they play, the pattern
   never rewritten. Off by default.
3. **Storage:** 32 user patches in slot 0, 32 patterns in slots 1–2 (ORBIT with them in slot 3), saved on SAVE only,
   written when the sound is quiet (an erase stalls the audio for tens of ms).
4. **One grammar for the controls** (as every page): the button's pages on the top line, K1–K4 a row as the knobs
   lie, EDIT ON / OFF, SCL held the second layer, the hint line saying what ALGORITHM and the keys do, the key LEDs
   showing what the keys do.

## 2. The pattern and the steps

- *A pattern* (`SdPattern`): up to 64 steps; LENGTH (a track restarts at step 1 after it; 16 by default), DIV, SWING
  (50..75 %), SCALE, the segments. A new pattern is 16 steps in four segments of four, forwards, once each; SEQ A's
  default is the loop the engine played before the sequencer (A2), SEQ B's every step a rest.
- *A step* (`SdStep`): NOTE, VEL, GATE and TYPE: NOTE, SLIDE (a glide into it, no new attack; a single note only), TIE
  (the note before held on), REST, SKIP (out of the pattern). A NOTE can RATCHET (2 or 3 hits in its time, each x 0.75
  softer, never closer than 50 ms) and has a PROB (5 % steps; a step that loses its draw is a rest), and holds a chord:
  up to three more notes as semitones from its note, moving with it (RND, the transposes, TURING's NOTES).
- *DIV:* 1/32 1/16T 1/16 1/8T 1/8 1/4, then 1/64 1/32T 1/16D 1/4T 1/8D 1/2 (added after the six, so a saved pattern's
  index keeps its meaning); ARP's RATE and SEQ's DIV step them fastest to slowest (`SD_SDIV_ORDER`).
- *The page* (SEQ A, SEQ B): the white keys are the bank's 16 steps (SCL + OCT the bank; the edited step goes along),
  a step tapped again shows RATCHET and PROB; the black keys RND UNDO CLR (twice) and, the last, SEG.

## 3. Segments

- *Data:* `seg_count` (1..6), `seg_split` (where segments 2.. start), `seg_rep` (1..4), `seg_order` (`SD_SO_*`),
  `seg_skip` (a bit a segment). Six, not eight: they fit the black keys next to EVEN.
- *Playing:* a segment's order is its non-skipped steps forwards, or backwards with REV; P-P is that and back without
  its ends (`a b c d c b`); RND a shuffle of it, drawn when the pass is built, not every step. REPEAT plays a segment
  n times before the next; SKIP plays nothing; the segments go A → F and round.
- *TIE, SLIDE and PROB follow the playing order:* they connect to the next step played (in another segment, or the
  same step again under REPEAT); PROB is drawn each time a step plays.
- *Editing (SEG, the last black key):* a white key cuts (a segment starts there, the new half copies the old one's
  settings and is picked); where one already starts, it joins it to the one before; step 1 never cuts; no seventh.
  The black keys pick A–F and EVEN (spread evenly: 64 in 3 is 21 21 22). K1 REPEAT, K2 ORDER (FWD REV P-P RND), K3 SKIP,
  K4 LENGTH (the segment's end moves, the ones after keep their lengths, the last gives way). A new pattern LENGTH
  rescales the segments.
- *The picture:* each segment's letter and its order where not forwards, a cut where one starts, a skipped one struck
  through in red, the playhead's key flipped.
- *As sources:* STEP (where the pattern is, −1 .. +1 across it) and SEG (which segment) in the matrix.

## 4. Two tracks, the master, the voices

- *Tracks:* SEQ A and SEQ B (`SdSeqT`) on the one sound, one tempo and one PLAY, each its own pattern, LENGTH, DIV and
  segments. EDIT switches a track ON / OFF (silent, in time). B is silent in MONO (its switch shows MONO).
- *The master* (SCL + SEQ on SEQ A, SEQ B or ORBIT; `set.master`, kept): the track whose one leads, A by default. The
  other track and ORBIT switched on while it plays come in at its next one, from their step 1 (ON blinks till then);
  off is at once; stopped, START starts them together. SEQ A's key (IN KEY, REC's note) and TURING's NOTES stay A's.
- *The voices:* each voice knows its source (the hand, A, B, ARP) and only that source turns it off; `sd_plan` gives
  each a quota each tick (a playing track its widest chord, A first, then B, ARP one, the hand the rest); no source
  takes another's held voice. The app keeps what is on inside it: ARP and B refused with a message where they do not
  fit (MONO with A playing: no ARP), switched off when PLAY, GLO or a chord takes their room; a chord recorded only as
  wide as the room left; REC lets the keys take the recorded track's voice.

## 5. Recording and transposing

- *REC* (the track SEQ edits, on any page): stopped, step by step (keys held together a chord; every key up, the next
  step; ENV a TIE, LFO a REST); playing, into the step that plays. It writes what is heard (`sd_seq_rec_note`): a key
  under the track's transpose goes in back by it, so the step plays the key's note.
- *MIDI's notes* (USB or the TRS jack, any channel; `midi_note`) are written as the keys' are, at their velocity (the
  FM-1's keys write 100); MIDI's held notes count with the keys'. They play on every page, the step pages too.
- *The transpose:* SCL held on SEQ, a key sets the track's transpose from C4 (or K4 XPOSE, ±24), taken at its next
  one; the keys show it (C4 a glow, its place full, blinking while it waits). OCT is the keys' octave on every page.
- *IN KEY* (the default; `SdEngine.xkey`): a transpose (SCL's, K4's, ORBIT's XPOSE) moves each note as many degrees of
  SEQ A's scale as A's key is from the key it names (+5 up a fourth in the key), a note off the scale keeping its step
  off it, a chord tone by tone (`sd_seq_xn`). SCL + EDIT on SEQ or ORBIT switches to CHROM (semitones; `set.xchrom`).

## 6. ARP and KEEP

- *ARP* (`sd_arp_*`, in the audio, the player's settings, kept): the keys' and MIDI's notes in a list as played; UP
  DOWN UP-DN RND ORDER over 1–4 octaves, RATE 1/64 .. 1/2 (triplets, dotted), GATE, HOLD (ALGORITHM). It takes one voice.
- *On the grid* (`grid_q8`): while the patterns play, ARP's steps fall on the clock time since the master's START
  (within 48 sixteenths, where every division fits), in the same sub-block as SEQ's; a hand under a quarter of a step
  after a point plays at once, later it waits for the next. Stopped, ARP runs from the key.
- *KEEP* (REC on ARP, `arp_keep`): the engine keeps ARP's last 64 steps from when it came on; REC takes the last bar
  at its RATE (`sd_arp_keep_len`: the fewest bars, 1..4, holding a whole number of steps, 8 at least) as a pattern at
  that DIV, the window ending at the nearest step (REC on the one keeps the bar that ended): into SEQ A, or SEQ B while
  A plays; the track on, the steps as heard, ARP's GATE; UNDO on SEQ.

## 7. TURING

- *The machine* (`sd_tur_tick`): a 16-bit register, a step every DIV (1/32 .. 1/4; START lines it up with step 1). The
  bit LENGTH places back (2..16) comes in again, flipped CHANGE % of the time: 0 a locked loop, 100 the loop and its
  inverse. Its 8 newest bits are the output, −1 .. +1, glided as S&H is.
- *The patch's* (its four spare bytes, so a patch saved before reads as TURING off): DEST any matrix destination at
  AMOUNT ±100 %, or NOTES (SEQ A's notes moved in its scale as each step starts, ±12 degrees at 100 %); a SOURCE in MOD
  too. To PITCH it is quantized first to SEQ A's key (`sd_tq_snap`: each voice from its own note to the nearest note
  of the scale; CHROM the nearest semitone), the other sources to PITCH on top.
- *The page* (LFO 1 → LFO 2 → TURING → MOD): K1 CHANGE, K2 LENGTH, K3 DEST, K4 AMOUNT, ALGORITHM DIV; EDIT on / off
  (the first time: XM, 30 %, CHANGE 25). `SdPattern`'s old TURING bytes are kept, unread, so saved patterns read.

## 8. ORBIT

- *A sequencer over the sequencer* (`SdOrbit`, SEQ's third page, saved with SEQ A's pattern): LENGTH 2..8 steps, each
  RATE (1, 2, 4) loops of the master; per step XPOSE A, XPOSE B (±24), two free lanes (each its matrix DEST, ±100 %),
  the four macros' changes (±99). Every value a change from where the player is: 0 changes nothing.
- *Playing:* its step moves at the master's one after RATE loops (`sd_orb_one`), START from step 1; the lanes and the
  macros jump at a step or glide through it to the next (GLIDE, a lane each).
- *The page:* the first white keys (or ALGORITHM) pick a step: K1 XPOSE A, K2 XPOSE B, K3 lane 1, K4 lane 2; SCL: the
  macros. The last black key: SETUP (LENGTH RATE DEST 1 DEST 2; SCL: the GLIDEs). The keys play nothing there.

## 9. The clock

- *MIDI clock* (USB or the jack; SYNC AUTO, SCL + PLAY: INT): the main loop counts the pulses (24 a quarter) and the
  engine moves everything on the tempo by them (`sd_clk_tick`: the tracks, ORBIT, ARP, TURING, the synced LFOs, MOVE),
  carried between pulses at their tempo and never past the next, so nothing drifts. While the tracks play a step stays
  6 pulses; the tempo shown is the mean of the last 8 beats. START and CONTINUE play from step 1 at the next pulse,
  STOP stops; half a second without pulses, the FM-1's own tempo. Two clocks (USB and the jack, or two inputs in the
  browser): the first, until it has been quiet for 500 ms.

## 10. The tests

`sd_test`: the segments, orders, skips, DIV, SWING, TIE, SLIDE and the editing; `test_switches` (EDIT OFF is the
engine at level 0, bit for bit; a track OFF; B in MONO; the transpose at the one; the voices' rules), `test_turing`,
`test_orbit`, `test_xkey`, `test_clock`, `test_rec_note` (every transpose, IN KEY and CHROM, A and B: notes play back as
played), `test_more` (the destinations' order, the divisions, KEEP), `test_cue` (in at the master's one), `test_tq`,
`test_arp_grid`. Host scripts: `q1.txt` (the page, segments, REC), `q2.txt` (SAVE and recall), `q3.txt` (RATCHET and
PROB), `u1.txt` (every page), `o1.txt` (ORBIT), `b1.txt` (PRESETS' list), `x1.txt` (SCL + OCT), `k1.txt` (the clock),
`m1.txt` (MIDI recorded); `web_test` in the browser's player.
