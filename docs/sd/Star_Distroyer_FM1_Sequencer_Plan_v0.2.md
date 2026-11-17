# Star Distroyer — Sequencer Plan

**Status:** v0.3, built. A step sequencer close to the stock FM-1's, with segments, two tracks, an arpeggiator,
ORBIT over them, and TURING (on the screen RADAR since 2026-11-16) as a modulation source. The code: `firmware/src/sd_seq.h` (the pattern and its editing),
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
   lie, EDIT ON / OFF, GLO held the second layer (a tap latches it, GLO lit, so one hand holds the FM-1 and the other
   turns), SEL the voices, OCT+ yes and OCT- no to a question (the list, SAVE, CLR), the hint line saying what
   ALGORITHM and the keys do, the key LEDs showing what the keys do (a black key with a job lit).

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
- *The page* (SEQ A, SEQ B): SELECT picks the step (2026-11-17, the user: "the keys are no longer the steps: try a line
  before REC"; they play there, taking the track's voice; GLO + the black keys the tools below, GLO + the others the
  transpose); until then the white keys were the bank's 16 steps (the bank the edited step's: ALGORITHM or SELECT
  moves it; SELECT, since 2026-11-16 the FM-1's picker wherever a page has things in a row, the user's wish: steps,
  parts; GLO + OCT was the bank until 2026-11-16, now the transpose's octave),
  B4 (C#4, MORE, lit) shows the step's RATCHET and PROB, and (2026-11-16, the user: a note one step late was hard to
  fix) the note heard at the step: K3 MOVE, a step earlier or later, K4 LENGTH, how many steps it holds (its TIEs;
  `sd_note_move`, `sd_note_resize`). A join stays a join: a note legato into the next keeps its end and only its start
  moves; the step it leaves is a TIE where the note before was held into it, else a REST; another note's first step
  is never taken (NO ROOM). The black keys B1 RND, B2 UNDO, B3 CLR (twice, or OCT+),
  B10 SEG, B11 the keys into the steps.
- *Hearing the edit* (2026-11-16, the user): stopped, REC off, the step picked sounds: a white key while held,
  SELECT, ALGORITHM or NOTE turned for 300 ms (the note heard there, a TIE's its note's, its chord, as the track plays
  it: `aud_step`, on the keys' voices).
- *The key's root* (2026-11-16, the user could not get the root off A: it was step 1's note): `SdPattern.root`, the
  byte TURING's old CHANGE held (its values never had bit 7): `SD_ROOT_SET | 0..11`; without the bit step 1's note,
  as before (`sd_pat_key`: the root at or below step 1's note, so RND keeps the line's octave). GLO + SELECT turns it
  (IN KEY on the master's page; the notes as written), and the app gives every pattern it takes its own root from
  what its key was (`root_own`), so a line written on step 1 no longer moves the key. The demos (the engine alone)
  play as they did.
- *CLEAR ALL* (SETUP's ALGORITHM, then OCT+; 2026-11-16, the user: "to start from scratch"): SEQ A and B every step a
  rest, their LENGTH, DIV and segments new (the key and the scale kept), ORBIT off and empty, the transposes 0 at
  the one; since 2026-11-17 (the user: "MOD destinations, RADAR and so on" stayed) the movement too, the MOD slots
  empty, RADAR off as new, the macros at 0, COURSE off, and the player's tools, ARP, the looper (LOOP, REVERSE,
  armed, SPEED, its GLO layer) and FREEZE; the sound itself stays (`clear_all`).

## 3. Segments

- *Data:* `seg_count` (1..6), `seg_split` (where segments 2.. start), `seg_rep` (1..4), `seg_order` (`SD_SO_*`),
  `seg_skip` (a bit a segment). Six, not eight: they fit the black keys next to EVEN.
- *Playing:* a segment's order is its non-skipped steps forwards, or backwards with REV; P-P is that and back without
  its ends (`a b c d c b`); RND a shuffle of it, drawn when the pass is built, not every step. REPEAT plays a segment
  n times before the next; SKIP plays nothing; the segments go A → F and round, inside the LENGTH (2026-11-17, the
  user: the repeats made the loop longer than the pattern, and B and ORBIT drifted off its one): its LENGTH played,
  step 1 again (`sd_seq_advance`, `sq_steps`); a traversal shorter than it (SKIPs) wraps as it ends.
- *TIE, SLIDE and PROB follow the playing order:* they connect to the next step played (in another segment, or the
  same step again under REPEAT); PROB is drawn each time a step plays.
- *Editing (SEG, B10, D#5: SEGMENTS):* since 2026-11-15 (testers fell into it and were lost: a white key used to cut
  and join) a white key only picks the part its step is in (framed red in the picture, the others dim; on the keys
  its steps lit); ALGORITHM sets how many parts, 1..6, spread evenly (64 in 3 is 21 21 22; UNDO takes it back); the
  black keys rest. K1 REPEAT, K2 ORDER (FWD REV P-P RND), K3 SKIP, K4 LENGTH (the part's end moves, the ones after keep
  their lengths, the last gives way). A new pattern LENGTH rescales the segments. Entering, the step picked's part.
- *The picture:* each segment's letter and its order where not forwards, a line where one starts; in SEGMENTS the
  part picked framed, the line PLAYS A B<x2 C D (in turn, each its order and repeats, a skipped one left out) and
  what the keys do.
- *As sources:* STEP (where the pattern is, −1 .. +1 across it) and SEG (which segment) in the matrix.

## 4. Two tracks, the master, the voices

- *Tracks:* SEQ A and SEQ B (`SdSeqT`) on the one sound, one tempo and one PLAY, each its own pattern, LENGTH, DIV and
  segments. EDIT switches a track ON / OFF (silent, in time). B is silent in MONO (its switch shows MONO).
- *The master* (SETUP's K3, HOME's third page; `set.master`, kept; GLO + SEQ until 2026-11-15): the track whose one leads, A by default. The
  other track and ORBIT switched on while it plays come in at its next one, from their step 1 (ON blinks till then);
  off is at once; stopped, START starts them together. The key (IN KEY, REC's note) is the master's; TURING's NOTES stay A's.
- *The voices:* each voice knows its source (the hand, A, B, ARP) and only that source turns it off; `sd_plan` gives
  each a quota each tick (a playing track its widest chord, A first, then B, ARP one, the hand the rest); no source
  takes another's held voice. The app keeps what is on inside it: ARP and B refused with a message where they do not
  fit (MONO with A playing: no ARP), switched off when PLAY, SEL or a chord takes their room; a chord recorded only as
  wide as the room left; REC lets the keys take the recorded track's voice.

## 5. Recording and transposing

- *REC* (the track SEQ edits, on any page; on SEQ B11, F#5, too, lit while it records): stopped, step by step (keys
  held together a chord; every key up, the next step; ENV a TIE, LFO a REST); playing, into the step that plays, and a key held on a TIE into each step it is held over
  (2026-11-16: a note as long as it was held; playing over a wrong one replaces it). A key pressed while another is
  held joins its chord only in the same step (stopped: always); in a later step it is the next note, legato (before,
  in MONO, a legato line lost its notes: ONE NOTE A STEP). It writes what is heard (`sd_seq_rec_note`): a key
  under the track's transpose goes in back by it, so the step plays the key's note.
- *MIDI's notes* (USB or the TRS jack, any channel; `midi_note`) are written as the keys' are, at their velocity (the
  FM-1's keys write 100); MIDI's held notes count with the keys'. They play on every page, the step pages too.
- *The key* (IN KEY): the master's, its step 1 the root and its SCALE the scale (`sd_key_pat`, the app's `key_pat`).
  The other track has none of its own: its notes are pulled to the nearest of the key's (the lower of two as near;
  `sd_key_snap`), its page's hint says whose it follows (KEY SEQ A) and GLO + ALGORITHM there that the key is set on
  the master's page. CHROM (the player's choice) frees it: each track as written.
- *The transpose:* GLO held on SEQ, a key takes the track's root to it, in the key (IN KEY: the root in C4's octave
  none, a key off the key: NOT IN THE KEY; CHROM: from C4), or K4 XPOSE (±24), taken at its next one; the keys show
  it (the key's notes a glow, every key in CHROM; its place full, blinking while it waits). OCT is the keys' octave.
- *IN KEY* (the default; `SdEngine.xkey`): a transpose (GLO's, K4's, ORBIT's XPOSE) moves each note as many degrees of
  the key as its root is from the note it names (+5 up a fourth in the key), the master's notes off the scale keeping
  their step off it, a chord tone by tone (`sd_seq_xn`). SETUP's XPOSE (K2) switches to CHROM (semitones;
  `set.xchrom`).

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
- *The page:* the first white keys (or ALGORITHM) pick a step: K1 XPOS A, K2 XPOS B, K3 lane 1, K4 lane 2; GLO: the
  macros. B10 (D#5): SETUP (LENGTH RATE DEST 1 DEST 2; GLO: the GLIDEs). The keys play nothing there.
  The step picked (a white key, SELECT, ALGORITHM, a knob on it) is held while ORBIT plays (`orb_hold`, step + 1: its
  values, no glide; stopped too, so the keys hear it), its own place counted on; the same key again, SETUP or leaving
  ORBIT lets it go, in time (2026-11-16, the user: "I must hear the change while I work").

## 9. The clock

- *MIDI clock* (USB or the jack; SYNC AUTO, SETUP's GLO + K1: INT; the tempo SETUP's K1, SELECT's until 2026-11-16): the main loop counts the pulses (24 a quarter) and the
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
`test_arp_grid`, `test_note_move` (MOVE and LENGTH, the joins kept, no note taken). Host scripts: `q1.txt` (the page, segments, REC), `q2.txt` (SAVE and recall), `q3.txt` (RATCHET and
PROB), `u1.txt` (every page), `o1.txt` (ORBIT), `b1.txt` (PRESETS' list), `x1.txt` (OCT, GLO), `k1.txt` (the clock),
`m1.txt` (MIDI recorded), `w1.txt` (SELECT, MORE's MOVE and LENGTH, ROOT, SETUP's TEMPO and CLEAR ALL, RADAR), `w2.txt` (LOOPER's tape and
GLO layer, DISTROY's band, RADAR's 1); `web_test` in the browser's player.
