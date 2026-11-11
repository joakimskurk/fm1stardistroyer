# Star Distroyer — Sequencer Plan

**Status:** Plan v0.2, companion to *FM1 Design & Feasibility v0.1*, section 6
**Instrument name:** Star Distroyer (decided 2026-11-02)
**Priority:** sound first. The sequencer waits for VOICE, REACTOR and DISTROY (§7).
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
- Not built: parameter locks, motion lanes. (MIDI clock: built 2026-11-10, below.)
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

**Built 2026-11-09: TURING is a modulation source, not the pattern's.** The user, after playing it in the browser:
"I struggle with the routing from the Turing machine: it goes to pitch by default and I don't understand how to
change that, and pitch is perhaps the last thing I want as the default"; "it also changes my whole sequencer loop
without me understanding why"; "it should work more like a modulation source, to parameters in the synth, the
filter and so on". On the choices: NOTES stays as one destination, the pattern never rewritten; XM the default; the
page on LFO, after the two LFOs; its own DEST and AMOUNT, and a SOURCE in MOD too.
- *The machine* (`sd_tur_tick`, sd_engine.h): a 16-bit register, a step every DIV of the tempo (1/32 .. 1/4,
  ALGORITHM; START lines it up with step 1). The bit LENGTH places back (2..16) comes in again at the newest place,
  flipped CHANGE % of the time: 0 a locked loop, 100 the loop and its inverse. Its 8 newest bits (the newest the
  top) are the output, -1 .. +1, glided an eighth a control tick as S&H is (no click).
- *The patch's* (its four spare bytes: `tur_set` on and CHANGE, `tur_ld` LENGTH and DIV, `tur_dst`, `tur_amt`), so it
  is saved with SAVE and a patch saved before reads as TURING off; the patch's size is unchanged.
- *DEST*: any matrix destination, at AMOUNT −100 .. +100 %; or NOTES: SEQ A's notes moved in the pattern's scale as
  each step starts (`SdSeqT.tur_deg`, ±12 degrees at 100 %), the pattern as written. In MOD, TURING is a source
  (`SD_MS_TUR`) for any slot.
- *The page:* LFO 1 → LFO 2 → TURING → MOD. K1 CHANGE, K2 LENGTH, K3 DEST, K4 AMOUNT, ALGORITHM DIV; EDIT on / off
  (the first time on: XM, 30 %, CHANGE 25). The picture: the last 16 steps as a stepped line, the loop's bits under
  it, where it goes and how many MOD slots it feeds. SEQ is SEQ A → SEQ B again, and its keys transpose only there.
- *Gone:* the rewrites (`sd_pattern_mutate`), their take-back and undo in the app; `SdPattern`'s three TURING bytes
  are kept, unread, so the saved patterns still read.
- *The demos* set it as a player would, while it is off (DEST, AMOUNT, CHANGE), then EDIT on: RANGE's acid line to
  the CUTOFF, its sixth scene's ostinato and DERELICT's pedal by NOTES, THE SONG's bridge by NOTES.
- Tests: `test_turing` (off by default; CHANGE 0 a loop of LENGTH, 100 of twice it; DEST XM moves XM; NOTES in the
  scale, the pattern untouched).
- *SCL + OCT on SEQ is the octave* (the user: an octave changed in REC "stays, but the buttons don't work on this
  page"): OCT alone is still the step bank there; with SCL held it moves the keys' octave, so the transpose keys
  reach every octave and C4 (no transpose) can always be found. `tests/scripts/x1.txt`.

**Built 2026-11-09: ORBIT, a sequencer over the sequencer.** The user: "a sub-sequencer page where I can set
different parameters in 2-8 steps, so I can make a bigger change, like transposing a sequence over time". On the
choices: a step lasts loops of SEQ A; it sets XPOSE A, XPOSE B, two free lanes and the macros; GLIDE per lane; the
name ORBIT, SEQ's third page, saved with the pattern.
- *The data* (`SdOrbit`, sd_seq.h): on, LENGTH 2..8, RATE 1 / 2 / 4 loops, GLIDE bits (lane 1, lane 2, the macros),
  the lanes' DEST; per step XPOSE A, XPOSE B (±24), the lanes (±100 %), the macros (±99). Every value a change from
  where the player is: 0 changes nothing, so ORBIT never fights the knobs or SCL's transpose.
- *The engine:* handed over as a pattern is (`sd_engine_orbit`); its step moves at A's one after RATE loops
  (`sd_orb_one`), START from step 1; the transposes add to each track's (`SdSeqT.orb_x`); the lanes go into the
  matrix and the macros' changes onto the macros, while A plays; a gliding lane runs from this step's value to the
  next's over the step, its span A's last loop (measured at each one; at START the pattern's length).
- *The page:* SEQ A → SEQ B → ORBIT. The first white keys (or ALGORITHM) pick a step: K1 XPOSE A, K2 XPOSE B, K3
  lane 1, K4 lane 2; SCL: MUTATE MOVE BREAK MACRO 4. The last black key: SETUP (LENGTH RATE DEST 1 DEST 2; SCL:
  GLIDE 1, GLIDE 2, GLIDE M). The keys play nothing there; their lights: the steps, the picked one full, the one
  playing flipped. PRESETS and SAVE are SEQ A's pattern's, and its ORBIT with it (slot 3: the patterns' slots, and
  the ones saved before, unchanged).
- Tests: `test_orbit` (off by default; C F G a loop each, RATE 2; a lane jumping and gliding; a macro's change; the
  pattern untouched); `tests/scripts/o1.txt` the page.
- *PRESETS browses* (the user: "browse the presets without them changing at once"; EDIT, on the choice): turning
  opens the list and the sound stays; EDIT loads the one picked (its light blinks meanwhile), HOME (or 8 s untouched)
  is not now, another button closes the list and does its own. On SEQ and ORBIT the list is the patterns (SAVED,
  EMPTY), the one playing marked NOW. `tests/scripts/b1.txt`; the host scripts press EDIT after their turns.

**Built 2026-11-10: the transposes IN KEY.** The user: "if I transpose SEQ B, will it keep to SEQ A's key and
root?" It did not: a transpose was semitones. Now, by default, a track's transpose (SCL + a key, K4 XPOSE, ORBIT's
XPOSE) moves each note as many degrees of SEQ A's scale as A's key is from the key the transpose names (+5: up a
fourth in the key), a note off the scale keeping its step off it, a chord tone by tone (`sd_seq_xn`, `SdEngine.xkey`).
SCL + EDIT on SEQ or ORBIT switches it to CHROM (semitones, as before); it is the player's setting (`set.xchrom`,
kept in flash), the values marked KEY while it is on, the hint line saying it with SCL held. The demos keep CHROM
(sd_play.h), so they play as they did. `test_xkey` (A +2 in C minor: D F Ab Eb, CHROM D F A E; B +4 to Eb in A's
key; C minor up to Eb major).

**Built 2026-11-10: MIDI clock.** The user: "if I connect it to a device with MIDI sync, will the modulation and
the sequencer sync?" They did not: the platform passed the clock and the transport on, and the app dropped them.
Now the main loop counts the pulses (24 a quarter, USB or the jack; two clocks: the first, until it has been quiet
for 500 ms) and the engine moves everything on the tempo by them (`sd_clk_tick`): the tracks, ORBIT (on A's one),
ARP, TURING, the synced LFOs and MOVE advance by a clock time, `clk_adv` a sub-block, which on the FM-1's own tempo
is the sub-block itself and with a clock goes as far as the pulses have, carried between them at their tempo and
never as far as the next one, so nothing can drift. While the tracks play, the step's unit holds and the pulses
carry a new tempo (a step stays 6 pulses); the tempo heard (SPACE's synced time, the status line, CLK) is the mean
of the last 8 beats, taken when it moves by 0.4 % (two beats 6 % away: at once). START and CONTINUE play from step 1
at the next pulse (CONTINUE too: the song position is not passed on); STOP stops; half a second without pulses, the
FM-1's own tempo. The synced LFOs count their cycles on the clock time from the one at START (exact: they keep to
the steps for good, with or without a clock), and MOVE draws on the one. SCL + PLAY switches SYNC AUTO / INT
(`set.sync_int`, kept in flash); SELECT says the tempo is the clock's. `test_clock`
(100 BPM against 120, 4 ms of random lateness, 140 BPM, STOP and START, the pulses gone: every step 0..5.4 ms after
its 6th pulse, never before; the LFO's beat with every 24th), `tests/scripts/k1.txt`.

**Built 2026-11-10: SEQ, OCT and REC smoothed.** The user, playtesting the firmware in the emulator (the page's
player): SEQ "goes by SPACE too before it goes back"; "sometimes I record and it is taken in another octave, and it
is tiresome to have to go into REC to change the octave: make it smoother and more intuitive".
- *SEQ goes round its own pages:* SEQ A → SEQ B → ORBIT → SEQ A, as every page button does. It used to go back to
  the page before SEQ (SPACE, if that was it) on the fourth press; another page's button leaves SEQ.
- *OCT is the keys' octave on every page,* on SEQ too, REC or not: the same button does the same everywhere, and
  the octave never needs REC. The status line shows it (OCT+n). The step bank moved to *SCL + OCT* on SEQ (SCL is
  the second layer); the edited step goes along to its place in the new bank, so K1..K4 never edit a step out of
  sight. The hint line under SCL says so ("KEYS: XPOSE AT 1  OCT: BANK").
- *REC writes what is heard* (`sd_seq_rec_note`): a key under the track's transpose (SCL + a key, ORBIT's XPOSE)
  went in as the key, so it came back moved by the transpose (an octave, a fourth). Now it goes in back by it (IN
  KEY by A's degrees, CHROM by semitones), so the step plays the note the key played; chords too, note by note.
  TURING's NOTES is a modulation and is not taken back.
- Tests: `test_rec_note` (every transpose −24..+24, SCL's, ORBIT's or both, IN KEY and CHROM, A and B: 28518 notes
  play back as played); `web_test` (SEQ from SPACE four times: SEQ A; OCT on SEQ moves the octave; SCL + OCT the
  bank, the octave kept); `tests/scripts/x1.txt` rewritten. (The line under SCL now reads "KEYS XPOSE  OCT BANK
  SEQ A/B", below.)

**Built 2026-11-10: more destinations from a list, ARP faster, and KEEP.** The user, playtesting: "more
destinations, e.g. decay in the envelopes, and I'd like them as a list, as the patches when I browse, so it is
easier to see and hit what you look for"; "should ARP have its own recording? It is so cool to play over, and
sometimes I wish I could save it"; "choose the division over and under, mostly so it can go faster".
- *Destinations:* 22 more (48 in all), after the 26 so a saved slot keeps its meaning: ATTACK, DECAY, SUSTAIN and
  RELEASE of ENV 1, 2 and 3 (a full source at 100 %: the whole 0..127), the filter's ENV, the Reactor's SLEW,
  DISTROY's loop TIME and SPREAD, SPACE's delay TIME (free: it glides, a tape) and TONE, the room's SIZE and TONE,
  LFO 1's and LFO 2's RATE. The envelopes and the LFOs tick before the matrix, so they read what it left the tick
  before (`sd_mod_snap`, 0.73 ms on); with nothing on them, the patch's own: every patch renders as it did.
- *The list* (`draw_dlist`): turning DEST (MOD's, TURING's, ORBIT's lanes) puts PRESETS' list in the knobs' place,
  the destinations in the chain's order (`SD_MD_ORDER`: VOICE, NAV, ENV 1, ENV 2, FILTER, ENV 3, REACTOR, DISTROY,
  SPACE, LFO; TURING's NOTES last), each with its engine. It plays as it turns; 1.5 s, another knob or a button
  closes it.
- *Divisions:* 1/64, 1/32T, 1/16D, 1/4T, 1/8D and 1/2 after the six (a pattern's DIV keeps its meaning; TURING keeps
  the six, its field holds 3 bits); ARP's RATE and SEQ's DIV step them fastest to slowest (`SD_SDIV_ORDER`).
- *KEEP* (REC on ARP, `arp_keep`): the engine keeps ARP's last 64 steps from when it came on (`arp_hn`, a gap as
  rests); REC takes the last bar at its RATE (`sd_arp_keep_len`: the fewest bars, 1..4, holding a whole number of
  steps, 8 at least: 1/16 16, 1/64 64, 1/16T 24, 1/8D 16 in three bars, 1/2 8 in four) as a pattern at that DIV,
  the window ending at the nearest step boundary (REC on the one keeps the bar that ended). Into SEQ A, or SEQ B
  while A plays (DUO, POLY); the track on, the steps as heard (`sd_seq_rec_note`), ARP's GATE; UNDO on SEQ.
- Tests: `test_more` (the order holds each destination once; E1 DECAY by the wheel: the note still sounding at
  200 ms, silent there without; L1 RATE by the wheel, 19 flips a second for 1; the divisions in order; KEEP's lengths, a bar in order,
  on the one, across a gap, a new ring when ARP comes on); the matrix's own (every destination written and
  restored, in range under the sanitizers, on every patch); `web_test` (the list up and gone; REC on ARP into SEQ A).

**Built 2026-11-10: the master's one, and TURING to PITCH in key.** The user, playtesting: "in PLAY, I want SEQ B
and ORBIT, when switched on, to wait until A is at 1 again. If A and B are alike, I'd also like to swap them"; then
"or we can choose the master" (a SWAP was built and taken out again for it); "when TURING is sent to pitch, it must
be quantized to the key and root first".
- *MASTER* (SCL + SEQ on SEQ A, SEQ B or ORBIT; `set.master`, kept): A (the default) or B, the track whose one
  leads. Its tab on the top line is underlined; with SCL held the switch's place says MASTER A or B. The engine runs
  the master first each sub-block (`lead`), so its one is known when the other comes; ORBIT counts its loops and the
  Reactor its steps. SEQ A's key (IN KEY, REC's note) and TURING's NOTES stay A's: they are the music's key, not
  its time.
- *In at the master's one:* while the master plays steps, the other track switched on (EDIT, or MONO left for DUO /
  POLY for B) stays silent until the master's next one and comes in there from its step 1, lined up on it to the
  sample (`t[].in`, `sd_seq_restart`, the time past the one in `a_rem`). ORBIT switched on comes in at that one from
  its step 1 (`orb_live`): its transposes, lanes and macros wait with it. Until then ON blinks on the top line and
  EDIT's light, and the hint says where it comes in. Off stays at once. Stopped (or the master playing no steps),
  at once: START starts them together, as before.
- *TURING to PITCH* (its own DEST, or a MOD slot with TURING as the source): quantized before it reaches the pitch.
  Its step (unglided) as semitones (±24 at 100 %, as PITCH) moves each voice from its own note to the nearest note of
  SEQ A's scale from its key (`sd_tq_snap`: 13 places round it, the lower of two as near; CHROM: the nearest
  semitone), so every voice stays in the key, a key off the scale too. The other sources to PITCH (a vibrato) go on
  top as they were. Nothing else changed: with no TURING to PITCH a patch renders bit for bit as before (131 renders
  checked).
- Tests: `test_cue` (B ON at A's step 2: silent till A's one, its step 1 on the same sub-block; ORBIT on there: A's
  steps 3 and 4 as they were, +7 from the one, +5 a loop on; ORBIT off at once; MASTER B: A ON at B's step 2 in at
  B's one, ORBIT's steps at B's ones); `test_tq` (a held note under TURING to PITCH, A minor and CHROM: every tick a
  whole number of semitones from the same engine without it, on A minor's notes, an off-scale key too); `web_test`
  (ON blinking till the one, then lit, for B, ORBIT, and A under MASTER B; SCL + SEQ marks the master).

**Built 2026-11-10: ARP on the grid.** The user: "I found in an earlier version that ARP is not quite in sync;
we check this too before we build". Measured: ARP's steps kept the moment the keys went down, 9 to 55 ms off SEQ's
beat at 120 BPM (up to half a step), the same offset for good (no drift: under 1 ms in 60 s), a new hand a new one.
- *The grid* (`grid_q8`): clock time since the master's START (a MIDI clock's too), within 48 sixteenths, the span
  every division fits a whole number of times (1/2 .. 1/64, triplets, dotted). While the patterns play, ARP's place in
  its step is the grid's (an exact 32-bit remainder a tick), and a step falls where it comes round: in the same
  sub-block as SEQ's steps, never drifting, whatever the tempo or the clock.
- *A new hand*: under a quarter of a step after a grid point it plays at once (as played: a hand a little late
  still on the beat), the next note on the grid; later, it waits for the next point. START with keys held: ARP from
  the one, with A.
- Stopped, ARP runs from the key, as before.
- Tests: `test_arp_grid` (SEQ A at 1/16, ARP at 1/16, 1/16T, 1/16D, 1/64: every note within a sub-block of the grid
  over 60 s and after a new hand; a hand 600 frames after a 16th at once, one 3000 after at the next; at 1/16 all of
  A's steps with ARP's note in the same sub-block).

**Built 2026-11-11: MIDI notes recorded.** The user, of a MIDI keyboard on the TRS jack: "most interesting is to
record into the SEQ, the ARP, and maybe play freely over what is already happening"; then, offered a way to play
over ARP: "ARP is to be ARP. We do not play over ARP".
- *REC* (`midi_note`): a note from MIDI (USB or the jack, any channel) is written as a key's is: stopped, step by step
  (held together, a chord, with the FM-1's keys too; all up, the next step), playing, into the step that plays;
  what is heard (`sd_seq_rec_note`). Its velocity goes into the step's VEL (the FM-1's keys write 100, as before).
  MIDI's held notes count with the keys' (`midi_held`, `midi_hn`): a quick note after another lifted is the next
  step, never the same one.
- *Already so, kept:* MIDI's notes play on every page, the step pages too (the FM-1's keys are the steps there);
  ARP takes them while on, so KEEP keeps what they played; over the tracks they take the voices left (DUO, POLY), and
  where none is left they say so as the keys do (*MONO: THE SEQ HAS THE VOICE*, *NO FREE VOICE: SEQ PLAYS*).
- *Not:* pitch bend and the sustain pedal (CC 64) are not heard; OCT moves the FM-1's keys, not MIDI's.
- Test: `tests/scripts/m1.txt` (DUO, CLR, REC stopped: C4 at 40 from the jack, E4 + G4 one step with G4 as +3, a REST,
  A4 at 127 from USB on channel 2, C5 with the FM-1's C4 one step (-12), D4 and F4 10 ms apart two steps; playing:
  G5 into step 4, F5 into step 9; POLY, ARP on C E G from MIDI, KEEP: SEQ B's 16 steps of it; ARP off: a MIDI note on
  a voice of its own over A and B), checked with a dump of the tracks. RAM 96,796 B of 98,304 (+16: the held
  notes); the audio's load as before (the gate 44.0 %).

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
| Nav mode is **one** large voice; Poly is 4 × 4-op | Sequencer is monophonic. TIE and SLIDE are well-defined in Nav mode; in Poly mode SLIDE means legato on the same voice. |
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
new place. This is what makes segments useful for sound design: `REACTOR.POS` locks on steps 1–4 played P-P give a
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

`SEQ.SEG_PHASE → REACTOR.POS` or `NAV X` gives each segment its own sweep without a single lock. Optional for V1, but
essentially free.

### 4.5 Not in V1

Per-segment overrides (transpose, a Reactor state, a macro offset per segment). Attractive, but they add a data layer
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

**Sound comes first.** Phase 0–3 (benchmark harness, VOICE, REACTOR, DISTROY) get the attention; the sequencer does not
run in parallel with them. The only sequencer work before then is a minimal note driver for auditioning patches
(a fixed 16-step loop, no segments, no locks) — enough to hear VOICE → REACTOR → DISTROY move in time. S0–S6 start
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
| S6 | Connect to real VOICE / REACTOR / DISTROY | Seq + mod inside its 5–10 % CPU budget under the worst-case patch (Gate B) |

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
