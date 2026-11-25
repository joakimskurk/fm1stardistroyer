/* SPDX-License-Identifier: GPL-3.0-only */
/* Star Distroyer: a starship FM synth with a morphing resonator and multiband destruction, for the M-VAVE FM-1.
 * Built on Dinghy 0.5 (platform.c: updates, the USB rescue, MIDI, keys, knobs, LEDs, the screen, flash).
 *
 * V3 (Sound Plan v0.1 §6): four banks of four oscillators, in the sound engine
 * (sd_engine.h, 32-frame sub-blocks), the Reactor, DISTROY, SPACE, a lowpass a voice, the matrix, the macros, two
 * sequencer tracks, the arpeggiator.
 *
 * The controls, the same on every page (a knob, a button, EDIT and GLO do the same everywhere):
 *   a page button  the one of its pages it showed last (HOME: MACROS, the boot's too); again, the next (the top line
 *                  shows them, this one a red tab; FX's others in four letters, so its four fit):
 *                  HOME  MACROS -> SETUP   FX  REACTOR -> DISTROY -> TIME -> SPACE   ENV  NAV -> FILTER -> ENV -> SUB
 *                  (NAV HOME's second till 2026-11-21: the voice now on one button, HOME what you play with; each
 *                  button's pages in the sound's order since 2026-11-21: the filter before the amp, the delay into the
 *                  reverb)
 *                  LFO   LFO 1 -> LFO 2 -> RADAR -> MOD    ARP  ARP -> LOOPER   SEQ  SEQ A -> SEQ B -> ORBIT -> DRUM
 *                  (ARP's two are the player's, kept apart from the patch)
 *   K1..K4     the four values in the row under the top line, left to right as the knobs: the layer's
 *   ALGORITHM  the knobs' layer, a row of buttons on the hint line, the one the knobs do lit (2026-11-22, after SEQ's,
 *              the user: "the tidiest we have done"): the first the page's main four, MORE the rest of its settings,
 *              TOOLS what acts and its switches (a turn acts; CLEAR asks, OCT+ yes); each page keeps its own; a page
 *              with one layer (SUB, RADAR, MOD) has no row
 *   GLO        what the keys do instead of playing (no knob layers since 2026-11-22): held, or a tap latches it, a
 *              tap again (or a page button) lets it go (GLO lit while it holds). On SEQ the keys transpose the track,
 *              at its one: IN KEY (the default: the root to the key pressed, in the master's key, its notes lit; the
 *              other track's notes pulled into it) or CHROM (from C4, as written), SETUP's XPOSE; GLO + OCT an octave
 *              more or less (-24..24), GLO + ALGORITHM the SCALE, GLO + SELECT the ROOT. On ORBIT and DRUM the white
 *              keys the steps (DRUM: GLO + SELECT the drum); on MACROS the switches. Elsewhere it neither latches nor
 *              lights (glo_here)
 *   EDIT       ON / OFF of what the page shows (the top line's right end; EDIT's light: on): COURSE, REACTOR, DISTROY,
 *              SPACE, FILTER (fx_off: the settings kept), ARP, a track (SEQ B: not in MONO), ORBIT, RADAR. While
 *              the master plays, the other track and ORBIT switched on come in at its one (ON blinks till then),
 *              from their step 1
 *   PRESETS    the patch (on SEQ the pattern, P01..P32): it turns a list, the sound as it is; EDIT loads, HOME not
 *              now
 *   SELECT     which one, where a page has them in a row (select_turn): SEQ's step (PARTS: the part), ORBIT's
 *              step, DRUM's step, REACTOR's stage (STAGE, F1-F4), DISTROY's band, NAV's bank, MOD's slot, SETUP's
 *              freeze log, SAVE's place; elsewhere nothing      SEL the voices: MONO -> DUO -> POLY (lit: DUO or POLY)
 *   OCT- OCT+  the keys' octave, on every page, REC or not; a question (PRESETS' list, SAVE, CLEAR): OCT+ yes, OCT-
 *              no; on SEQ, GLO + OCT the track's transpose an octave (as GLO + a key)
 *   PLAY       the patterns on / off   REC recording into the track SEQ edits
 *              (ENV a TIE, LFO a REST); what is heard is written: a key under the track's transpose (GLO + a key,
 *              ORBIT's XPOSE) goes in back by it, so the step plays the key's note (sd_seq_rec_note)
 *   SAVE       the patch (on SEQ the pattern) to a user place: ALGORITHM picks it, SAVE again (or OCT+) writes, HOME
 *              or OCT-: not now
 *   the keys   play; their lights show what they do (keys_lit). A key is a function only with GLO, and none is a
 *              page's tool (2026-11-22: the tools are TOOLS' knobs); on DRUM the pads and MUTE keys
 *   the voices MONO 1, DUO 2, POLY 4, the same sound in each (sd_engine.h's sd_voice_banks, vg), shared by quota (sd_plan): a playing track its widest chord, A first, then B,
 *              then ARP one, the keys the rest; no source turns off or takes another's note. ARP and SEQ B are refused
 *              where they do not fit (MONO with SEQ A playing: no ARP, no keys but REC's), and switched off when PLAY,
 *              SEL or a chord takes their room; a message says why. The status line shows the mode and who holds
 *              them (MONO A: A B the tracks, R ARP, - the keys), always (the CPU is on SETUP)
 *   the screen  from the top: the patch (its name, the master's key, the battery's three bars, its place), the button's pages (EDIT's switch a
 *              lamp), K1..K4 (each on a printed scale, its pointer; grey while the engine is off), the picture (104
 *              rows: SEQ's steps in it), the hint (ALGORITHM's row of layers; where it is short, the page's other
 *              word on the right; GLO held on SEQ, ORBIT, DRUM: what GLO does), the status; a list (PRESETS, a DEST)
 *              seven names over the knobs and
 *              the picture; between a starship's screen and a synth's panel: boxes a
 *              pixel off each corner (cv_box), lamps, scales; red is the patch, where you are, what is on, what plays
 *              (the logo: the splash's)
 *
 * The pages' knobs (ALGORITHM's row: the layers, as LY_* name them):
 *   NAV     ENV's first page (HOME's second till 2026-11-21: the voice in its order, NAV ENV FILTER, on one
 *              button). TONE: XM WARP FOLD DETUNE (the Q page, 0..99); X/Y: X Y WIDTH, K4 a harmonic generator
 *              on the bank (ALL ODD RND CLUST, 14 chords, CPY; a list as it turns, each with the harmonics it sets);
 *              ROUTE: K1 the bank's roles (SSSC PSSC PPPC SCSC PPCC CSSC SCCC CCCC; a list as it turns: how many are
 *              heard, the routing), K2 the bank. SELECT the bank, A B C D. EDIT: COURSE OFF, ON (from each note), LOOP
 *              (round on its own, from START)
 *   MACROS     HOME's first (HOME: MACROS <-> SETUP since 2026-11-21). MUTATE MOVE BREAK CUTOFF (the synth's own,
 *              FILTER's K1, 2026-11-23); MORE: BODY AIR DROWN (each its own amount, where MACRO 4 was one of them)
 *              DARK (every cutoff down together, the player's: 0 open, kept as patches change). GLO + a key its switch.
 *              The picture: the switches as the keys that work them, each its key's lamp (pic_chain): on top the
 *              chain FILT REAC DIST TIME SPAC on the black keys F#3..D#4 (B1..B5), in its order; under it the players
 *              ARP LOOP SEQ A SEQ B ORBIT RADAR DRUMS on the white keys under them, F3..E4 (2026-11-21, the user: "what
 *              is on top of the screen on black, what is at the bottom on white"; B6..B11 and G5 before)
 *   SETUP      the FM-1's, kept: K1 TEMPO, 40..240 BPM (a MIDI clock's while one leads; MORE's K1 SYNC, AUTO: a
 *              MIDI clock leads when it comes; INT: not heard; its K2 CORES), K2 XPOSE (IN KEY or
 *              CHROM), K3 MASTER (SEQ A or B: whose one leads); a turn right the second; K4 COLOUR, the screen's accent
 *              (RED, the default, ORANGE PINK MAGENTA VIOLET BLUE CYAN GREEN: a list as it turns, its band the
 *              colour; the whole screen at once, the splash too); the picture draws each under its knob (the MIDI
 *              clock into the tempo, the master's scale on an octave's keys, whose one leads; under COLOUR the CPU,
 *              a gauge, a lamp for each core: the second red once it gave up, dark if it never started; the worst
 *              since the start a mark on the gauge and MAX under it, or in red GAP n, the audio interrupts past their
 *              time, each a click (2026-11-21, the user heard clicks: so the next time tells what it was); MORE the
 *              battery's volts there). TOOLS, each asked (OCT+ yes, OCT- no): DEMO 1 (SINGULARITY), DEMO 2 (ARMADA,
 *              with the drums), CLEAR, a start from scratch (SEQ A and B empty, ORBIT off and empty, the transposes 0;
 *              the sound, the key and the scale kept; clear_all); LOG the freeze log (as SELECT)
 *   REACTOR    REACT: POS RES SLEW DRIVE; TONE: FREQ MODEL DIV MIX; STAGE, what the stage SELECT picks holds (it
 *              opens on the one POS is on; SELECT takes POS along): K1 TYPE (13 vowels, 13 chords: a list) K2 PITCH K3 WIDTH K4 LEVEL; F1-F4
 *              its four; MORE: TRACK (OFF KEY HARM KEY+H), LOW, STAGES how many it uses, 1..6 (SD_R_NST: POS's turn
 *              and the clock across those; 2026-11-21, the user's one-stage monosynths). The keys play in every layer
 *              (STAGES was GLO + B10 till 2026-11-22). EDIT's first time on a patch with no stages: six vowels on
 *              POS, so it sounds (react_first). The picture: the filters as peaks across the width, under them the
 *              six stages as a piano roll (the one playing red)
 *   DISTROY    SELECT the band 1..4 (LOW LO-MID HI-MID TOP, its frame in the picture). BAND: TYPE (NONE SOFT HARD
 *              FOLD CRUSH) DRIVE, ALT by what it does for the type (KNEE SPACING DEPTH), LEVEL; SHAPE: BIAS PAN
 *              DYNAMIC (PHAT .. SHARP) BODY; MIX: the band's SPLIT, the whole's XOVER and MIX; LOOP: FDBK TIME SPREAD
 *              TAME. EDIT's first time on a patch with no bands: four that sound (dest_first)
 *   SPACE      the reverb: PLATE: SIZE DECAY TONE R-LEVEL. On the second core a plate, on one core the room. FREEZE:
 *              K1 FREEZE (the player's, not saved: the plate's input closed, its tail held for ever), K2 PITCH (the
 *              player's: the frozen tail up or down, -12 .. 12 semitones, in and out over 50 ms; on the second core;
 *              0, or not frozen: the plate as ever). MORE: K1 SHIMMER (an octave up of the tail back into it,
 *              0..99, s_mode's top bits; off FREEZE's layer since 2026-11-23, the user: "it does nothing while frozen")
 *   TIME       the delay: DELAY: TIME FDBK TONE D-LEVEL; MODE: K1 FREE or SYNC, K2 PING-PONG
 *   LOOPER     the looper (the player's, not the patch's; always recording what SPACE hears before the reverb):
 *              LOOPER: LOOP (it stops and plays its last LENGTH round, at its SPEED, +-12 semitones, at the level
 *              it was heard at whatever patch comes (its TRIM; 2026-11-24); into the
 *              reverb as the synth goes; the delay goes on; on this layer since 2026-11-23, the user: "LOOP on
 *              LOOPER, GLIDE can move") LENGTH (a division, synced whatever TIME's MODE) SPEED L-LEVEL; SEND:
 *              FILTER (a lowpass left, a highpass right, none in
 *              the middle), R-SEND, D-SEND. EDIT arms it, at any L-LEVEL (0: it records, silent), or turns it off with
 *              its loop KEPT (lp_hold: faded out, not recording, the same loop back when on; 2026-11-21). TOOLS: GLIDE
 *              (how long a new SPEED takes: OFF at once), REV (it plays it backwards), OCTAVE (SPEED to the next
 *              octave down or up, over GLIDE, OFF: at once, the loop landing on its own grid), CLEAR (asked, OCT+ yes:
 *              the delay's line whole again). Armed, it borrows the delay
 *              line's second half (5.1 s with the RAM's last 24 KB, mono mu-law), the
 *              delay then 0.5 s. An engine off (EDIT), ARP or RADAR off, the LOOPER disarmed: its knobs grey, the
 *              hint line says OFF where the row leaves room (not SEQ's pages: a part is written while it is off).
 *              The loop follows PLAY: STOP parks it (it fades out and waits, kept: LOOP WAITS FOR PLAY), PLAY plays
 *              it again in sync, where it stood against the master's one (LOOP BACK ON THE ONE)
 *   ENV        ENV 1 (the level): ATTACK DECAY SUS RELEASE; ENV 2 (the FM's amount)
 *   FILTER     a lowpass on each voice: FILTER: CUTOFF RES ENV (ENV 3's depth) KEY; ENV 3; MORE: TYPE LP12 / LP24,
 *              VEL
 *   LFO 1, 2   LFO: SHAPE RATE DEST AMOUNT: DEST and AMOUNT its first MOD slot's (a free one the first time, +30 %;
 *              DEST OFF lets it go); MODE: K1 CLOCK (FREE or SYNC), K2 CYCLE (LOOPS, NOTE: restarts each note, ONCE: one
 *              cycle a note, KICK SNARE RADAR 1 BAR: one cycle struck by it, synced), the picture says it whole; the
 *              picture: where the matrix sends it
 *   RADAR      a loop of random steps (RADAR in the code: tur_*, SD_TUR_*), a modulation source of the patch: a
 *              loop of bits on the tempo. LFO's third tab (LFO 1, LFO 2, RADAR, MOD; for a day
 *              on 2026-11-19 MOD's ninth, by ALGORITHM: the user found it gone).
 *              K1 SEARCH (CHANGE in the code, 2 % a detent: the chance a bit flips as it comes round, SD_TUR_FLIP,
 *              the knob saying it), locked at both ends, LOCK in red there (the loop as it is: the user, "just LOCK
 *              and LOCK"); between them it drifts, most at 50 (50 %), less towards either end. K2 LENGTH (2..16
 *              steps, 16 at first), K3 OFFSET (its output's middle moved, as a MOD slot's OFFSET, for all it feeds),
 *              K4 DIV (its clock against the tempo, 1/32 .. 1/4; GLO + K2 until 2026-11-21). 2026-11-21, the user:
 *              "no DEST and AMOUNT on this page, rather OFFSET and the division; the lock at K1's ends, with an
 *              indicator; call it SEARCH, a little less travel". Where it goes is MOD's (2026-11-21, the user: "all of
 *              RADAR's DEST in the MOD tab; on the RADAR page the visuals"): a slot with SOURCE RADAR, its DEST a
 *              matrix destination or one of RADAR's own three, ACCENTS, ROLLS (the drums, on a 1, AMOUNT the chance)
 *              and NOTES (SEQ A's notes moved in its scale, the pattern untouched; to PITCH quantized to SEQ A's key
 *              first, sd_tq_snap). An older patch's own RADAR DEST moves into a free slot as it loads (tur_slot).
 *              EDIT on / off (off by default; on the first time, sent nowhere yet: a free slot to XM, 30 %). The
 *              picture in two: the loop as a scope (pic_tur), each step held at its radius over its arc and a hard
 *              jump to the next (as it sounds), the sweep where it is, a gauge of the value now; beside it the loop's
 *              bits as a matrix of lamps, its 16 places in four rows (the 1 top left, the place now red, past LENGTH
 *              a dot, a bit CHANGE flipped this pass a red pip)
 *   MOD        the slot SELECT picks (1..8): SOURCE DEST AMOUNT. SOURCE (22, a list as it turns: by where each comes
 *              from, the envelopes, the LFOs and RADAR, NAV, the playing, the engines, the drums). DEST (here and on
 *              ORBIT's lanes; from RADAR also ACCENTS, ROLLS and NOTES at the list's end): 60, by
 *              the page their knob is on and its name there, in the pages' order (on each its layers' K1..K4): the
 *              voice, NAV, the envelopes, the filter, REACTOR, DISTROY (each band's DRIVE and SPLIT, one crossover
 *              through another; the loop), SPACE, TIME (the delay, the looper's SPEED: free, RADAR's in semitones),
 *              the LFOs' RATE, the drums', the synth's VOLUME; while it turns a list in the knobs' place, as PRESETS'
 *              (draw_dlist). The picture:
 *              the eight slots in the knobs' columns, each its source over its destination and its amount's bar
 *   ARP        ARP: MODE (UP DOWN UP-DN RND ORDER) RATE (1/64 .. 1/2, triplets, dotted) OCTAVES GATE; TOOLS: HOLD. The
 *              player's, not the patch's. REC there: KEEP, what it just played (the last bar at its RATE) as a SEQ
 *              track's pattern, SEQ A's (B's while A plays), UNDO on SEQ (arp_keep). While the patterns play its
 *              steps fall on their grid from START (a hand under a quarter step late plays at once); stopped, from
 *              the key
 *   SEQ A, B   two tracks on the one sound, each its pattern and LENGTH (a melody and a bass line). ALGORITHM picks
 *              what the knobs do, a row of buttons on the hint line (2026-11-22, the user: no key has a job on SEQ):
 *              STEP the step SELECT picks, NOTE VEL GATE TYPE (NOTE SLIDE TIE REST SKIP); MORE its RATCHET PROB, and
 *              the note heard there: MOVE (a step earlier or later: a join stays a join, another note's place never
 *              taken) LENGTH (how many steps it holds, its TIEs; sd_note_move, sd_note_resize); TRACK LENGTH DIV SWING
 *              XPOSE; PARTS the pattern's parts (SELECT the part, framed, the rest dim): PARTS (how many, 1..6, even)
 *              REPEAT ORDER (FWD REV P-P RND, or SKIP) LENGTH (its end moved), the picture says how they play (PLAYS
 *              A B<x2 C); TOOLS RANDOM (a new line each detent) UNDO (the pattern before the last RANDOM, CLEAR, REC
 *              or KEEP, and back) CLEAR (asked: OCT+ yes) DUB (REC adds, takes nothing). Stopped, REC off: the step
 *              picked sounds (aud_step). The keys play; GLO + a key (every one) transposes, GLO + ALGORITHM the SCALE,
 *              GLO + SELECT the ROOT (the key's own, root_own; the notes as written). REC: keys held together, a
 *              chord; playing, a key held on writes a TIE into each step it is held over (a new key while one is
 *              held: the next note, legato, unless in the same step: a chord); DUB: a key on a step with a note joins
 *              its chord (as the voices allow), on an empty one it is the note
 *   ORBIT      the song: 1..16 steps on the white keys (GLO), each LOOPS loops of the master; the step SELECT (or GLO
 *              + its key) picks: SONG K1 SEQ A, K2 SEQ B (the pattern there), K3 DRUMS (the beat there, D01..D16, or
 *              OFF; -- as they play), K4 LOOPS; PITCH XPOS A, XPOS B, LOOP and FREEZE (the loop's and the frozen
 *              tail's pitch, -12..12: sd_orb_tail, 2026-11-22); LANE DEST 1 LANE 1 DEST 2 LANE 2 (each lane its
 *              matrix DEST, a list, and its amount at the step, -100..100 %); MACRO the macros' changes (MUTATE MOVE
 *              BREAK CUTOFF, -99..99); the song's own: SETUP K1 LENGTH K2 END K3 GLIDE (the lanes that glide: JUMP
 *              LANE 1 LANE 2 BOTH) K4 MACROS (theirs, a button). EDIT on / off. Every value a change from where the player is (0
 *              none); saved as a song (S01..S16). The step picked (SELECT, a knob, GLO + a key) is held while it is
 *              edited (eng.orb_hold): 2 s after the last edit, GLO + its key again, or leaving ORBIT, on in time
 *   DRUM       the drums (2026-11-19; sd_drums.h, the second core's): KICK SNARE HAT and a fourth with an ENGINE
 *              (CLAP TOM DATA SONAR ZAP SWELL: KIT's K4) on the master's grid, its SWING; EDIT on / off (off at every
 *              start; in at the bar's one). Its pad picks the drum (GLO + SELECT too, not played), SELECT the step:
 *              DRUM K1..K4 the drum's sound (K4 its macro), TONE its second four; MORE the step's ROLL x1..x4 and PROB
 *              100..25 %; MIX the four's
 *              levels; KIT the drum's PAN, DUCK (the synth under the kick, 0..100 %), the ACCENT, the fourth's ENGINE;
 *              TOOLS RANDOM (the drum's row), UNDO, CLEAR (the row, asked). The keys never play the synth here
 *              (2026-11-20, the user): F3..B3 the four as pads, C4..F4 accented, F#4 G#4 A#4 C#5 MUTE (at once, not
 *              kept), REC the pads into the steps while it plays (the nearest step, on top; UNDO the pass). GLO + a
 *              white key: its step off -> ON -> ACCENT -> RADAR (plays where RADAR's newest bit is 1) -> off.
 *              PRESETS the beats D01..D16 (the first eight the factory's until saved over), SAVE to one;
 *              kept: the sounds and the beat (settings). In the matrix: KICK and SNARE as sources (a sidechain of
 *              its own: KICK to VOLUME, AMOUNT -), each macro, their level and the fourth's TUNE (P TUNE) as
 *              destinations; RADAR's DEST ACCENTS and ROLLS act on the drums' hits; an LFO's ONE CYCLE ON KICK
 *              (SNARE, RADAR 1, EACH BAR)
 *   MIDI in    notes from USB or the TRS jack, any channel: they play as the keys do, on every page (ARP takes them
 *              while on), and REC writes them as the keys', at their velocity (midi_note); CC 1 the matrix's WHEEL,
 *              channel pressure its AFTER. A clock (24 a quarter) leads the tempo while it comes (SYNC AUTO; SETUP's
 *              MORE: INT, not heard): the tracks, ARP, RADAR, the synced LFOs and MOVE move by its pulses
 *              (sd_clk_tick); START / CONTINUE play from step 1 at the next pulse, STOP stops; the status line shows
 *              its tempo, CLK
 *   MASTER     the volume (platform.c)
 *
 * CPU on the screen (SETUP, under COLOUR): the worst audio interrupt of the last second, in % of its period
 * (256 frames, 5.8 ms).
 * Dinghy's rule is under half (docs/RULES.md). On the host it reads 0: there tools/sd_render.c measures. */
#define APP_NAME "STAR DISTROYER"
static void app_idle(void);
#if !defined(DINGHY_HOST)
static void sd_diag_service(void);
#define APP_SYSEX_HOOK sd_diag_service          /* a read-only status over USB (below; tools/fm1_diag.c) */
#endif
#define LCD_IDLE app_idle                       /* while the screen's DMA runs: MIDI in (platform.c) */
#define CV_MAX (240u * 120u)                    /* the splash in two halves; the nav panel (57.6 KB of the pool) */
#if !defined(DINGHY_HOST) && (defined(SD_TWO_CORES) ? SD_TWO_CORES : !BOARD_SMK37)
#define FM1_CPU1 1                              /* the second core (hal/fm1_cpu1.h): SPACE's plate runs there. The
                                                   FM-1's default; the SMK-37 one core (its stock supplies unknown).
                                                   2026-11-18: the user's FM-1 crashed with two at the SPL's supplies
                                                   (1.26 / 1.40 V) and ran clean at M-VAVE's own (1.38 / 1.60 V), raised
                                                   at the boot before the second core starts (hal/fm1_sys.h; Sound Plan
                                                   6.8). SETUP (GLO K2) CORES 1: one core, the room, the supplies left.
                                                   A one-core build: DINGHY_DEFS=-DSD_TWO_CORES=0 */
static int app_cores(void);
#define APP_CORES app_cores                     /* (platform.c's boot asks, before the second core starts) */
#endif
#include "platform.c"
#if !defined(DINGHY_HOST) && !defined(SD_RAM_AUDIO)
#define SD_RAM_AUDIO 1                          /* the audio's hot code from RAM (sd_engine.h, app.ld's .ram_audio; a
                                                   test build from the flash: DINGHY_DEFS=-DSD_RAM_AUDIO=0) */
#endif
#include "sd_engine.h"
#include "sd_logo.h"

static SdEngine eng;
/* SPACE's memory, ~213 KB: the pool (as gfx.c's canvas; the host on macOS: Mach-O has no .pool section) */
#if defined(DINGHY_HOST) && (defined(__APPLE__) || defined(DINGHY_WEB))
static SdSpaceMem space_mem;
#else
static SdSpaceMem space_mem __attribute__((section(".pool")));
#endif
/* the room beside the frozen plate (SPACE's FREEZE: what is played over the held tail goes there; sd_engine.h's
   frm), 15 KB: on the FM-1 in .noinit, the RAM's last free place (the engine clears it as a freeze starts) */
#if defined(DINGHY_HOST)
static SdRoomMem frz_room;
#else
static SdRoomMem frz_room __attribute__((section(".noinit")));
#endif
/* the looper's memory (TIME's, borrowed from the delay): mono at half rate, a byte a sample (mu-law),
   in two pieces of one ring: the delay line's second half while the looper is armed (L-LEVEL above 0: the delay
   then has 0.5 s), and what is left of the RAM, 24 KB: 5.1 s. The test build DINGHY_DEFS=-DSD_RAM2=1 looks for a
   second RAM bank at boot (hal/fm1_ram2.h) and, found, gives the looper the largest of 512 KiB .. 4 MiB it holds
   there instead, the delay keeping its whole line. The SMK-37's build has less RAM left (its keys' velocities, pads
   and wheels): 23 KB there, 5.07 s */
#if BOARD_SMK37
#define SD_LP_INTB 23552u
#else
#define SD_LP_INTB 24576u
#endif
static uint8_t lp_mem[SD_LP_INTB];
#define LP_LENT ((uint8_t *)&space_mem.dl[2u * SD_SP_DHALF])   /* the delay line's second half */
#define LP_LENT_N (SD_SP_DHALF * 4u)
#if !defined(DINGHY_HOST) && defined(SD_RAM2) && SD_RAM2
#include "fm1_ram2.h"
#define RAM2_BYTES(k) ((512u << 10) << ((k) - 1u))   /* kind k (1 .. 4): 512 KiB .. 4 MiB */
#define RAM2_A ((uint8_t *)FM1_RAM2_BASE)
static uint8_t ram2_kind;                       /* 0: none; else what the looper has there */
static uint32_t ram2_size;
#endif

/* the drums' batch (sd_drums.h): a block of their sound ahead, 1 KB, the pool */
#if defined(DINGHY_HOST) && (defined(__APPLE__) || defined(DINGHY_WEB))
static int16_t dr_ob[2u * SD_DR_BATCH * SD_SUB];
#else
static int16_t dr_ob[2u * SD_DR_BATCH * SD_SUB] __attribute__((section(".pool")));
#endif

/* SPACE's plate on the second core (never on core 0: the room is the fallback there): a block a job, handed over at its
 * end in the audio interrupt and joined in the next (sd_plate_post, sd_plate_join). Its pointers are constants here,
 * as the core may read RAM wrong. When the core cannot run (barred since a failure, silent at the start) or gives
 * up, the block is dropped (sd_plate_drop: its tail fades out) and the room takes the plate's place (eng.sp_room).
 * On a computer (the host, the page's player) there are no hooks: the plate runs in the render */
#if !defined(DINGHY_HOST) && !FM1_CPU1
SD_HOT static int sd_c1_post(void *e, uint32_t b) { (void)e; (void)b; eng.sp_room = 1; return 0; }   /* (one core) */
SD_HOT static int sd_c1_join(void *e) { (void)e; eng.sp_room = 1; return -1; }
#endif
#if FM1_CPU1
static uint32_t c1_wait;                        /* this interrupt's ticks waiting for the second core (2026-11-21: a
                                                   late core 2 stalls the audio interrupt; SETUP's WAIT) */
volatile uint32_t sd_c1_load[3];                /* its jobs, timer ticks: the longest, all, how many (for fm1-emu's
                                                   --dump: tools/emu_cpu.sh) */
static uint32_t c1_plate_b;                     /* the block handed over (these not SD_HOT: once a block) */
static volatile uint32_t c1_plate_seen;         /* its last block done, + 1 (the second core's to write) */
SD_HOT static void sd_c1_plate(uint32_t b)
{
    uint32_t t0 = fm1_ticks(), dt;
#if defined(SD_RAM2) && SD_RAM2                 /* (the looper's memory, each kind's constants: the second core may
                                                   read ram2_kind wrong, and then has the RAM's own) */
    switch (ram2_kind) {
    case 1: sd_plate_job_at(&eng, space_mem.u.pl, RAM2_A, RAM2_BYTES(1), lp_mem, SD_LP_INTB, b); break;
    case 2: sd_plate_job_at(&eng, space_mem.u.pl, RAM2_A, RAM2_BYTES(2), lp_mem, SD_LP_INTB, b); break;
    case 3: sd_plate_job_at(&eng, space_mem.u.pl, RAM2_A, RAM2_BYTES(3), lp_mem, SD_LP_INTB, b); break;
    case 4: sd_plate_job_at(&eng, space_mem.u.pl, RAM2_A, RAM2_BYTES(4) - SD_LP_INTB, lp_mem, SD_LP_INTB, b); break;
    default: sd_plate_job_at(&eng, space_mem.u.pl, LP_LENT, LP_LENT_N, lp_mem, SD_LP_INTB, b); break;
    }
#else
    sd_plate_job_at(&eng, space_mem.u.pl, LP_LENT, LP_LENT_N, lp_mem, SD_LP_INTB, b);   /* (the looper, then the
                                                   plate) */
#endif
    SD_BARRIER();
    c1_plate_seen = b + 1u;                     /* (SPACE's: its block back) */
    dt = fm1_ticks() - t0;
    if (dt > sd_c1_load[0]) sd_c1_load[0] = dt;
    sd_c1_load[1] += dt;
    sd_c1_load[2]++;
}
/* the second core's half of each sub-block (sd_engine.h's sd_c1_share: banks 3 and 4, REACTOR's resonators 3 and
 * 4, DISTROY's right channel) beside the first core's; the engine's own buffers, so no pointer is read from RAM. Busy
 * with the plate, or retired: the first core does it. ON again since 2026-11-19 (the user's FM-1, at M-VAVE's own
 * supplies: an hour of SEQ A and B, ARP, the looper and the drums in POLY, 21 million jobs, no misread, no fault;
 * 63 % where one core's half read 85). Off from 2026-11-17: the second core faulted inside it (its stack limit and a
 * bus write, in sd_bank_run, 12 s up), at the boot loader's low supplies (Sound Plan 6.8). A job each sub-block (1378
 * a second, not 172) and far more of the shared RAM read: where a core that misreads now and then (fm1_cpu1.h) meets
 * a pointer. Off (DINGHY_DEFS=-DSD_C1_HALVES=0), the first core renders every bank, the same sound */
#ifndef SD_C1_HALVES
#define SD_C1_HALVES 1
#endif
#if SD_C1_HALVES
SD_HOT static void sd_c1_half(uint32_t arg) { SD_FAR(sd_c1_share)(&eng, arg); }
SD_HOT static int sd_c1_hpost(void *e, uint32_t arg) { (void)e; return fm1_cpu1_run(sd_c1_half, arg); }
SD_HOT static int sd_c1_hjoin(void *e)
{
    uint32_t t0 = fm1_ticks();
    int r = fm1_cpu1_wait() ? -1 : 0;
    (void)e;
    c1_wait += fm1_ticks() - t0;
    return r;
}
#endif
/* the drums (2026-11-19, the user: "the drums only the second core's"): their next batch is the second core's too.
 * When the plate hands its block over in the batch's last sub-block (the usual case: both 256 frames), one job: the
 * batch's first sub-block (heard first), the plate's block (SPACE waits for it in the next sub-block: c1_plate_seen,
 * not the job's end), the batch's rest; else the batch is a job of its own at the audio block's end. The first core
 * waits for each sub-block of the batch it plays (sd_dr_wait, 1.5 ms, then the core's own timeout); a core gone, the
 * rest here (sd_engine.h). (2026-11-19: the plate's block was held back to the audio block's end too, to go with the
 * drums; when its boundary fell inside a block (after a patch, or SPACE's rest), SPACE read each block before it was
 * made, an old one in its place: the reverb went to grit, ~30 dB of it above 3 kHz, the user heard it. Now never later
 * than its boundary) */
static inline uint32_t sd_c1_drums_due(void)    /* the batch's last sub-block, and the drums sound or will */
{
    const SdDrums *r = &eng.dr;
    return r->gj == SD_DR_BATCH - 1u && r->ob && (r->on || r->live || r->req || r->song_due || r->pat_seq != r->pat_taken);
}
SD_NOINLINE static void sd_c1_drums(uint32_t j0, uint32_t j1)
{
    if (!j0) sd_drum_begin(&eng, eng.dr.ext);
    sd_drum_part(&eng, j0, j1);
}
SD_NOINLINE static void sd_c1_job_d(uint32_t b) { (void)b; sd_c1_drums(0, SD_DR_BATCH); }
SD_NOINLINE static void sd_c1_job_pd(uint32_t b) { sd_c1_drums(0, 1); sd_c1_plate(b); sd_c1_drums(1, SD_DR_BATCH); }
SD_NOINLINE static int sd_dr_wait(void *e, uint32_t want)
{
    uint32_t t0 = fm1_ticks();
    int r;
    (void)e;
    for (;;) {
        fm1_cpu1_check();                       /* (its sync, and a fault or a misread: retired) */
        if (eng.dr.done >= want) { r = 0; break; }
        if (!fm1_cpu1_on) { r = -1; break; }
        if ((uint32_t)(fm1_ticks() - t0) >= 1500u * FM1_TICKS_PER_US) {   /* (then the job's end, or the core gone) */
            r = fm1_cpu1_wait() == 0 && eng.dr.done >= want ? 0 : -1;
            break;
        }
    }
    c1_wait += fm1_ticks() - t0;
    return r;
}
SD_NOINLINE static void sd_c1_block_end(void)   /* app_audio's: the batch's job, unless it went with the plate's */
{
    SdDrums *r = &eng.dr;
    if (!sd_c1_drums_due() || r->ext == r->grp + 1u) return;
    r->ext = r->grp + 1u;
    SD_BARRIER();
    if (fm1_cpu1_mb.request != fm1_cpu1_mb.complete) {   /* (the last job's end) */
        uint32_t t0 = fm1_ticks();
        (void)fm1_cpu1_wait();
        c1_wait += fm1_ticks() - t0;
    }
    if (!fm1_cpu1_run(sd_c1_job_d, 0)) r->ext = 0xFFFFFFFFu;   /* (the core gone: the batch here) */
}
SD_HOT static int sd_c1_post(void *e, uint32_t b)
{
    (void)e;
#if defined(SD_C1_NOPLATE) && SD_C1_NOPLATE     /* (a test build: the second core started, given no plate; the room
                                                   here. DINGHY_DEFS=-DSD_C1_NOPLATE=1, 2026-11-17's bisection) */
    (void)b;
#else
    {
        SdDrums *r = &eng.dr;
        uint32_t dr = sd_c1_drums_due();
        if (fm1_cpu1_mb.request != fm1_cpu1_mb.complete) {   /* (a batch still out: its end; the halves are joined
                                                   before SPACE) */
            uint32_t t0 = fm1_ticks();
            (void)fm1_cpu1_wait();
            c1_wait += fm1_ticks() - t0;
        }
        c1_plate_b = b;
        if (dr) r->ext = r->grp + 1u;
        SD_BARRIER();
        if (fm1_cpu1_run(dr ? sd_c1_job_pd : sd_c1_plate, b)) return 1;
        if (dr) r->ext = 0xFFFFFFFFu;           /* (the core gone: the batch here) */
    }
#endif
    eng.sp_room = 1;
    return 0;
}
SD_HOT static int sd_c1_join(void *e)           /* the block back: its own mark, then the job's end, then gone */
{
    uint32_t t0 = fm1_ticks(), want = c1_plate_b + 1u;
    (void)e;
    for (;;) {
        fm1_cpu1_check();
        if (c1_plate_seen == want) { c1_wait += fm1_ticks() - t0; return 0; }
        if (!fm1_cpu1_on) break;
        if ((uint32_t)(fm1_ticks() - t0) >= 1500u * FM1_TICKS_PER_US) {
            if (fm1_cpu1_wait() == 0 && c1_plate_seen == want) { c1_wait += fm1_ticks() - t0; return 0; }
            break;
        }
    }
    c1_wait += fm1_ticks() - t0;
    eng.sp_room = 1;
    return -1;
}
#endif

/* ---- the audio's load: ticks of the timer (FM1_TICKS_PER_US a microsecond) per interrupt */
#ifdef DINGHY_HOST
static uint32_t sd_ticks(void) { return 0; }
#define SD_TICKS_PER_US 24u
#else
static uint32_t sd_ticks(void) { return fm1_ticks(); }
#define SD_TICKS_PER_US FM1_TICKS_PER_US
#endif
/* REC while it plays (2026-11-20, the user: "the notes often land a little behind where I expect"): how far the
 * engine's clocks run ahead of the sound the player heard as they pressed, in frames. The interrupt renders the half
 * after the one playing (HALF_FRAMES), so just after it the engine stands two halves ahead of the speaker, one by the
 * next; a key counts after two scan frames and a pass sees it (~2 ms). A press is placed by what was heard, not by
 * where the engine already is (the steps had taken the press as it reached them: up to ~16 ms late, more for the
 * drums, a block ahead on the second core). In a browser: the frames its ring holds and the page's own latency */
static volatile uint32_t aud_at;                /* the last audio interrupt's start (ticks) */
#define REC_IN_FRAMES 88u                       /* (~2 ms: the key's two scan frames, the pass) */
static uint32_t rec_ahead(void)
{
#if defined(DINGHY_WEB)
    return web_ahead() + REC_IN_FRAMES;
#elif defined(DINGHY_HOST)
    return 3u * HALF_FRAMES / 2u + REC_IN_FRAMES;   /* (no clock: the FM-1's middle) */
#else
    uint32_t us = (sd_ticks() - aud_at) / SD_TICKS_PER_US, el = us < 5805u ? us * 441u / 10000u : HALF_FRAMES;
    return 2u * HALF_FRAMES - (el < HALF_FRAMES ? el : HALF_FRAMES) + REC_IN_FRAMES;
#endif
}
static uint32_t cpu_worst, cpu_frames;
static volatile uint32_t cpu_permille = 0xFFFFFFFFu;   /* the worst interrupt of the last second, 0.1 % (none yet) */
static volatile uint32_t cpu_max, cpu_over;     /* the worst since the start, 0.1 %; the interrupts past their period
                                                   (each a gap: a click), since the start (2026-11-21: the user heard
                                                   clicks; SETUP shows both, so the next time says what it was) */
#ifndef DINGHY_WEB                              /* (a browser has no chip to measure) */
static uint32_t wait_worst;
static volatile uint32_t wait_permille, wait_max;   /* the waiting for the second core inside it: the worst of the
                                                   last second, and since the start, 0.1 % */
#endif
#ifdef SD_CPU_LOG
static uint8_t cpu_log[288];                    /* (half seconds, ARMADA's 2:20; whole %: the RAM's tight) */
#endif

/* the engine's memory and the second core's hooks, after sd_engine_init: at the start, and around the demo (from the
 * audio interrupt: nothing else runs on the engine then). SPACE's lines, the frozen room and the looper's memory
 * cleared: the pool is not cleared at boot, and the demo starts from silence as its render did */
static void eng_setup(void)
{
    uint32_t i, *w = (uint32_t *)(void *)&space_mem;
    for (i = 0; i < sizeof space_mem / 4u; i++) w[i] = 0;
    for (i = 0; i < sizeof frz_room; i++) ((uint8_t *)&frz_room)[i] = 0;
    for (i = 0; i < SD_LP_INTB; i++) lp_mem[i] = 0;
    eng.frm = &frz_room;                        /* (the room beside the frozen plate; one core has the room in the
                                                   plate's place) */
#ifndef DINGHY_HOST
    eng.pl_post = sd_c1_post;                   /* (before SPACE has its memory: no block ever runs here) */
    eng.pl_join = sd_c1_join;
#if FM1_CPU1
#if SD_C1_HALVES
    eng.c1_post = sd_c1_hpost;                 /* (the first core's half of each sub-block: beside it) */
    eng.c1_join = sd_c1_hjoin;
#endif
    eng.sp_room = !fm1_cpu1_on;                 /* (one core: the room from the start) */
#else
    eng.sp_room = 1;
#endif
#endif
    eng.sm = &space_mem;
    sd_engine_looper_borrow(&eng, lp_mem, SD_LP_INTB);   /* the looper's memory (before SPACE runs) */
    eng.dr.ob = dr_ob;                          /* the drums' batch */
#if !defined(DINGHY_HOST) && FM1_CPU1
    eng.dr_wait = sd_dr_wait;
#endif
}

/* ---- the demos (2026-11-19, the user: SINGULARITY on the FM-1, started where it makes sense: SETUP's ALGORITHM, then
 * OCT+; 2026-11-21 a second, ARMADA, with the drums: ALGORITHM turned on, DEMO 2). A performance recorded as what it
 * does to the engine (sd_demo.h, sd_demo2.h, tools/make_demo.sh) is played back here,
 * in the audio interrupt, each event before the render it came before: the render's sound sample for sample
 * (tests/demo_test.c), MASTER as the player turned it over the FM-1's own knob. It starts from a fresh engine and
 * silence; a key, a button or a knob stops it (a fade of 50 ms); after it, the engine fresh again and all the player had
 * there before handed back (the patch, the patterns, ORBIT, the scalars: demo_keep). The looper's tape is not kept */
#include "sd_demo_fmt.h"
#ifdef DINGHY_HOST
#define SD_DEMO_SECTION
#else
#define SD_DEMO_SECTION __attribute__((section(".text.sd_demo")))   /* (after the tables and the hot code: neither
                                                   moves for it) */
#endif
#include "sd_demo.h"
#include "sd_demo2.h"
typedef struct { const char *name; const uint8_t *ev; uint32_t len, bpm, settle, renders; } DemoSong;
static const DemoSong DEMOS[2] = {
    {SD_DEMO_NAME, SD_DEMO, SD_DEMO_LEN, SD_DEMO_BPM, SD_DEMO_SETTLE, SD_DEMO_RENDERS},
    {SD_DEMO2_NAME, SD_DEMO2, SD_DEMO2_LEN, SD_DEMO2_BPM, SD_DEMO2_SETTLE, SD_DEMO2_RENDERS}};
static uint8_t demo_k;                          /* the one playing (SETUP's DEMO 1, DEMO 2) */
enum { DEMO_OFF, DEMO_START, DEMO_PLAY, DEMO_TAIL, DEMO_FADE, DEMO_END, DEMO_RESTORE };
static volatile uint8_t demo_st;                /* the main loop asks START and FADE, takes RESTORE; the audio the rest */
static SdDemoPlay demo;                         /* the playback (the audio's) */
static volatile uint32_t demo_r;                /* the render about to run */
static uint32_t demo_quiet;
static int32_t demo_fade;                       /* FADE: Q12, down to 0 */
static int32_t demo_keep[SD_DEMO_NF];           /* the player's own engine state before it (the main loop's) */
static uint32_t demo_before(void)               /* before the render: 1 to render, 0 silence */
{
    switch (demo_st) {
    case DEMO_START:                            /* a fresh engine, as the performance's render had */
        sd_engine_init(&eng);
        eng_setup();
        sd_demo_begin(&demo, DEMOS[demo_k & 1u].ev, DEMOS[demo_k & 1u].len);
        demo_r = 0;
        demo_quiet = 0;
        demo_st = DEMO_PLAY;
        /* fall through */
    case DEMO_PLAY:
        if (sd_demo_events(&demo, &eng, demo_r)) demo_st = DEMO_TAIL;
        demo_r++;
        return 1;
    case DEMO_TAIL:
    case DEMO_FADE:
        demo_r++;
        return 1;
    case DEMO_END:                              /* done: the engine fresh, the main loop hands back the player's */
        sd_engine_init(&eng);
        eng_setup();
        demo_st = DEMO_RESTORE;
        return 0;
    default:
        return 0;
    }
}
static void demo_after(int32_t *out, uint32_t n)   /* MASTER as the player turned it; the end, quiet; a stop's fade */
{
    uint32_t i, pk = 0;
    int32_t g = demo.gain;
    for (i = 0; i < 2u * n; i++) { uint32_t a = (uint32_t)(out[i] < 0 ? -out[i] : out[i]); if (a > pk) pk = a; }
    if (demo_st == DEMO_TAIL) {                 /* after the score: on till it is quiet half a second, as the render */
        demo_quiet = pk < 16u ? demo_quiet + n : 0u;
        if (demo_quiet >= SD_FS / 2u) demo_st = DEMO_END;
    }
    if (demo_st == DEMO_FADE) {
        demo_fade -= 4096 / 9;
        if (demo_fade <= 0) { demo_fade = 0; demo_st = DEMO_END; }
        g = (g * demo_fade) >> 12;
    }
    for (i = 0; i < 2u * n; i++) {
        int32_t m = (out[i] * g) >> 12;
        out[i] = m > 32767 ? 32767 : m < -32767 ? -32767 : m;
    }
}

/* The brake (2026-11-20, the user's FM-1 froze, then restarted: the watchdog). Past its period the audio interrupt
 * comes again at once and the main loop never runs: the screen, the keys and the watchdog's feed all stop, and 8 s
 * later the FM-1 restarts. One core (the second gave up, or CORES 1) at a heavy patch is enough: ARMADA's climax
 * read 139 % there. So after three interrupts past their period in a row, the next renders nothing (a block of
 * silence) and the main loop has it; while it stays over, every other one. A gap in the sound, said on the screen
 * (CPU FULL), never a freeze */
#ifndef DINGHY_WEB
static uint8_t aud_over_n;                      /* the interrupts past their period in a row */
#endif
static volatile uint32_t aud_held;              /* the blocks the brake held back, since the start */
/* The last run's marks, kept through a restart (.noinit) and said at the next start when the watchdog made it (the
 * main loop stopped 8 s): the page, the buttons last pressed, where the main loop and the audio last were (the audio
 * on while the loop stood: it was starved, or stuck), the CPU, the brake's blocks, the second core */
#define LAST_RUN_MAGIC 0x4C415354u              /* "LAST" */
typedef struct {
    uint32_t magic, ms, isr_ms, page, btn, cpu, held, c1, at, stop_ms, play_ms;
    uint32_t st;                                /* (2026-11-22, the freeze log) the state: PLAY, REC, GLO latched, the
                                                   voices, SEQ's track and layer, ARP, the looper armed */
    uint32_t ev_n;                              /* the inputs since the start (ev_ring holds the last EV_N) */
    uint32_t pc_n, pc[4];                       /* the main loop stood a second: the PCs the audio interrupted, a
                                                   quarter second apart (where it is stuck) */
} LastRun;
#if defined(DINGHY_HOST)
static LastRun last_run;
#else
static LastRun last_run __attribute__((section(".noinit")));
#endif
static LastRun boot_run __attribute__((unused));                      /* the last run's, as the start found it (the USB diagnosis says it) */
static uint32_t boot_wd __attribute__((unused));                      /* 1: the watchdog made that start */
/* where the main loop is (last_run.at; 2026-11-22, the user: now and then a freeze at STOP after a long PLAY): kept
   through the restart, so the diagnosis says where it stood */
enum { LR_INPUT = 2, LR_DRAW = 50, LR_DRAWN = 51, LR_SAVE = 20, LR_SETTINGS = 30, LR_END = 99 };
#define LR_AT(x) (last_run.at = (x))
static char last_said[40];                      /* (said for 8 s after the splash) */
/* CLICK (SETUP's MORE, 2026-11-23, the user: "the drums out, so an optional metronome while I record"): a tick on
 * each beat of the master's grid (sd_arp_tick's, 48 sixteenths from START), the bar's one higher; OFF, REC (while
 * SEQ records), PLAY (whenever it plays). Added after the whole sound, once a block: never in the looper, the FX or
 * a demo, and under -15 dBFS (the master's ceiling past it is hard) */
static volatile uint8_t click_want;             /* (the main loop's: SETUP's CLICK and REC as they stand) */
static uint32_t clk_left, clk_ph, clk_inc, clk_amp, clk_dec;   /* (a tick sounding: its frames left, phase, pitch,
                                                   level Q16 and its fall a frame) */
SD_HOT static void click_add(int32_t *out, uint32_t n)
{
    uint32_t L = 4u * eng.seq_len_q8, span = 48u * eng.seq_len_q8, w = n * 256u, i = 0, end, start, off;
    if (click_want && eng.seq_run && L && span > w && !demo_st) {
        end = eng.grid_q8 + eng.grid_adv;
        if (end >= span) end -= span;
        start = end >= w ? end - w : end + span - w;
        off = start % L ? L - start % L : 0u;   /* (the next beat from the block's start, clock time) */
        if (off < w) {                          /* a beat in this block: its tick from that frame */
            uint32_t one = ((start + off) % span) / L % 4u == 0u;
            i = off >> 8;
            clk_left = 600u; clk_ph = 0;        /* (13.6 ms) */
            clk_inc = one ? 214254000u : 146082000u;   /* (2.2 kHz the one, 1.5 kHz the others) */
            clk_amp = (one ? 5500u : 4000u) << 16;
            clk_dec = clk_amp / 600u;
        }
    }
    for (; i < n && clk_left; i++, clk_left--) {
        int32_t v = (int32_t)(((int64_t)sd_sine(clk_ph) * (int32_t)(clk_amp >> 16)) >> 15);
        out[2u * i] += v;
        out[2u * i + 1u] += v;
        clk_ph += clk_inc;
        clk_amp = clk_amp > clk_dec ? clk_amp - clk_dec : 0u;
    }
}
static volatile uint32_t con_until;             /* (sd_console.h: the console's frames, while on: fm1_ms they stop at) */
static void con_peak(const int32_t *out, uint32_t n);
SD_HOT static void app_audio(int32_t *out, uint32_t n)
{
    uint32_t t0 = sd_ticks(), dt;
    aud_at = t0;                                /* (REC: where the speaker is, rec_ahead) */
#ifndef DINGHY_HOST
    if (aud_over_n >= 3u) {                     /* (out is silence: the platform cleared it) */
        aud_over_n = 2u;
        aud_held++;
        return;
    }
#endif
#ifndef DINGHY_HOST
    {                                           /* the main loop stood a second (a freeze coming: the watchdog's in 8):
                                                   where it is, the PC this interrupt came in on, eight a quarter second
                                                   apart (the freeze log) */
        uint32_t stood = fm1_ms - last_run.ms;
        if (last_run.magic == LAST_RUN_MAGIC && stood >= 1000u && last_run.pc_n < 4u && stood >= 1000u + 250u * last_run.pc_n)
            last_run.pc[last_run.pc_n++] = fm1_reti();
    }
#endif
    if (demo_st && !demo_before()) return;      /* (the demo: its events first; silence while it starts and ends) */
#if !defined(DINGHY_HOST) && FM1_CPU1
    if (!fm1_cpu1_on && !eng.d.lite) {          /* one core left (it gave up, or CORES 1): DISTROY at the rate, so
                                                   one core holds it all (103 % at twice it, a gap every few blocks:
                                                   2026-11-21); its x2 lines cleared (never read again) */
        uint32_t i;
        for (i = 0; i < SD_DOS_D; i++) eng.d.os[0][i] = eng.d.os[1][i] = 0;
        eng.d.lite = 1;
    }
#endif
    SD_FAR(sd_engine_render)(&eng, out, n);   /* (in RAM: sd_engine.h's SD_RAM) */
#if !defined(DINGHY_HOST) && FM1_CPU1
    sd_c1_block_end();                          /* (the second core's job: the drums' next batch, unless it went
                                                   with the plate's) */
#endif
    if (demo_st) demo_after(out, n);
    click_add(out, n);
    if (con_until) con_peak(out, n);            /* (the console's output level, only while its frames run) */
    dt = sd_ticks() - t0;
    if (dt > cpu_worst) cpu_worst = dt;
#if !defined(DINGHY_HOST) && FM1_CPU1
    if (c1_wait > wait_worst) wait_worst = c1_wait;
    c1_wait = 0;
#endif
#ifdef SD_CPU_LOG                               /* a measure, never shipped (DINGHY_DEFS=-DSD_CPU_LOG=1): the demo's
                                                   worst interrupt each half second of it, 1 % (read from the RAM) */
    if (demo_st == DEMO_PLAY && demo_r / 86u < sizeof cpu_log / sizeof cpu_log[0]) {
        uint32_t pm = dt * 1000u / (n * 1000u / 441u * SD_TICKS_PER_US * 10u);
        pm = (pm + 9u) / 10u;
        if (pm > cpu_log[demo_r / 86u]) cpu_log[demo_r / 86u] = (uint8_t)(pm > 255u ? 255u : pm);
    }
#endif
    cpu_frames += n;
#ifndef DINGHY_WEB                              /* (in a browser there is no chip to measure: CPU --) */
    if (dt > n * 1000u / 441u * SD_TICKS_PER_US * 10u) {   /* (past its period: a gap) */
        if (!demo_st) cpu_over++;
#ifndef DINGHY_HOST
        eng.shed = 1;                           /* (a voice let go at the next sub-block, before the brake must
                                                   hold a block back: sd_shed) */
#endif
        if (aud_over_n < 3u) aud_over_n++;
    } else aud_over_n = 0;
    if (cpu_frames >= SD_FS) {                  /* once a second: the worst, against one interrupt's period */
        uint32_t period = n * 1000u / 441u * SD_TICKS_PER_US * 10u;   /* n frames in ticks: n * 22.68 us */
        cpu_permille = period ? cpu_worst * 1000u / period : 0u;
        last_run.cpu = cpu_permille; last_run.isr_ms = fm1_ms; last_run.held = aud_held;   /* (the last run's marks) */
        wait_permille = period ? wait_worst * 1000u / period : 0u;
        if (cpu_permille > cpu_max && cpu_permille != 0xFFFFFFFFu && !demo_st) cpu_max = cpu_permille;
        if (wait_permille > wait_max && !demo_st) wait_max = wait_permille;
        cpu_worst = 0;
        wait_worst = 0;
        cpu_frames = 0;
    }
#endif
}

/* ---- the screen's palette: near black, ice-white lines, red accents */
#define C_BG RGB(6, 8, 12)
#define C_ICE RGB(205, 230, 240)                /* lines, text */
#define C_ICE_D RGB(84, 104, 116)               /* dim text, outlines */
#define C_GRID RGB(24, 32, 40)                  /* empty cells, grids */
/* the accent (SETUP's COLOUR, RED the default): accents, the playing step, a held key (C_RED); dark, a half (C_RED_D);
   the nav's mix, filled, a quarter (C_FILL); an LED's glint, toward white (C_GLINT). One setting: every page draws
   with these four */
#define SD_ACCENTS 8u
static const char *const ACC_NAME[SD_ACCENTS] = {"RED", "ORANGE", "PINK", "MAGENTA", "VIOLET", "BLUE", "CYAN", "GREEN"};
static const uint16_t ACC[SD_ACCENTS][4] = {
    {RGB(235, 28, 28), RGB(118, 14, 14), RGB(58, 6, 8), RGB(255, 140, 140)},
    {RGB(255, 120, 20), RGB(127, 60, 10), RGB(64, 30, 5), RGB(255, 194, 149)},
    {RGB(255, 90, 170), RGB(127, 45, 85), RGB(64, 22, 42), RGB(255, 181, 217)},
    {RGB(225, 40, 215), RGB(112, 20, 107), RGB(56, 10, 54), RGB(242, 158, 237)},
    {RGB(150, 80, 255), RGB(75, 40, 127), RGB(38, 20, 64), RGB(208, 176, 255)},
    {RGB(40, 120, 255), RGB(20, 60, 127), RGB(10, 30, 64), RGB(158, 194, 255)},
    {RGB(0, 200, 230), RGB(0, 100, 115), RGB(0, 50, 58), RGB(140, 230, 244)},
    {RGB(40, 215, 90), RGB(20, 107, 45), RGB(10, 54, 22), RGB(158, 237, 181)}};
static uint16_t c_acc[4] = {RGB(235, 28, 28), RGB(118, 14, 14), RGB(58, 6, 8), RGB(255, 140, 140)};
static void accent_set(uint32_t a) { uint32_t k; for (k = 0; k < 4u; k++) c_acc[k] = ACC[a % SD_ACCENTS][k]; }
#define C_RED c_acc[0]
#define C_RED_D c_acc[1]
#define C_FILL c_acc[2]
#define C_OFF RGB(48, 60, 70)                   /* an engine off: its knobs' names, their bars */
#define C_GLINT c_acc[3]
#define KEY0 53u                                /* key 0 = F3 */
static int black(uint32_t note) { return (0x54Au >> (note % 12u)) & 1u; }

static void cv_blend(int32_t x, int32_t y, uint16_t c, uint32_t a)   /* c over the canvas at a / 7 */
{
    uint32_t i, e, r, g, b;
    if (x < 0 || y < 0 || (uint32_t)x >= cv_w || (uint32_t)y >= cv_h || !a) return;
    i = (uint32_t)y * cv_w + (uint32_t)x;
    e = swap16(cv_px[i]);
    r = e >> 11; g = e >> 5 & 63u; b = e & 31u;
    r += (((c >> 11) - r) * a * 37u) >> 8;      /* (c - e) * a / 7, unsigned wrap undone by the mask below */
    g += (((c >> 5 & 63u) - g) * a * 37u) >> 8;
    b += (((c & 31u) - b) * a * 37u) >> 8;
    cv_px[i] = swap16((uint16_t)((r & 31u) << 11 | (g & 63u) << 5 | (b & 31u)));
}

static void draw_logo(const SdLogo *l, int32_t x, int32_t y)   /* into the canvas, blended (sd_logo.h) */
{
    uint32_t r, c, stride = (l->w + 1u) / 2u;
    for (r = 0; r < l->h; r++)
        for (c = 0; c < l->w; c++) {
            uint32_t v = l->px[r * stride + c / 2u] >> (c & 1u ? 0 : 4) & 15u;
            if (v) cv_blend(x + (int32_t)c, y + (int32_t)r, v & 8u ? C_RED : C_ICE, v & 7u);
        }
}

static void cv_frame(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)   /* a panel: corners cut */
{
    cv_line(x + 3, y, x + w - 4, y, c); cv_line(x + 3, y + h - 1, x + w - 4, y + h - 1, c);
    cv_line(x, y + 3, x, y + h - 4, c); cv_line(x + w - 1, y + 3, x + w - 1, y + h - 4, c);
    cv_line(x, y + 3, x + 3, y, c); cv_line(x + w - 4, y, x + w - 1, y + 3, c);
    cv_line(x, y + h - 4, x + 3, y + h - 1, c); cv_line(x + w - 4, y + h - 1, x + w - 1, y + h - 4, c);
}

static void cv_box(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)   /* a filled box, a pixel off each corner
                                                   (as a key's edge: the boxes of the panel, not the graphs) */
{
    if (w < 3 || h < 3) { cv_rect(x, y, w, h, c); return; }
    cv_rect(x + 1, y, w - 2, 1, c);
    cv_rect(x, y + 1, w, h - 2, c);
    cv_rect(x + 1, y + h - 1, w - 2, 1, c);
}
static void cv_edge(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)   /* cv_box's outline, a pixel off
                                                   each corner (2026-11-23, the user: a button the same shape lit or
                                                   not, the straighter one, as the page's tabs) */
{
    if (w < 3 || h < 3) { cv_rect(x, y, w, h, c); return; }
    cv_rect(x + 1, y, w - 2, 1, c);
    cv_rect(x + 1, y + h - 1, w - 2, 1, c);
    cv_rect(x, y + 1, 1, h - 2, c);
    cv_rect(x + w - 1, y + 1, 1, h - 2, c);
}
static int32_t ink_col(const char *s, uint32_t last)   /* FONT_S: the text's first inked column from its pen, or
                                                   (last) its last: a word centred by what shows, not by its advance */
{
    const felucca_font_t *f = &FONT_S;
    const char *p = s;
    const uint8_t *gd;
    int32_t pen = 0, gx, gy, w, bpr, step;
    uint32_t gi;
    if (!*s) return 0;
    if (last) { while (p[1]) pen += f->adv[glyph(f, (uint8_t)*p++)]; }
    gi = glyph(f, (uint8_t)*p);
    w = f->bw[gi]; bpr = (w + 1) / 2; gd = f->data + f->off[gi];
    step = last ? -1 : 1;
    for (gx = last ? w - 1 : 0; gx >= 0 && gx < w; gx += step)
        for (gy = 0; gy < (int32_t)f->h; gy++) {
            uint32_t v = gd[gy * bpr + gx / 2];
            if ((gx & 1) ? (v & 15u) : (v >> 4)) return pen + gx - f->pad;
        }
    return last ? pen + f->adv[gi] - 1 : pen;   /* (a space: its advance) */
}
static void btn_text(int32_t x, int32_t y, int32_t w, const char *nm, uint16_t c)   /* a button's word, its ink in
                                                   the middle: the capitals 3 rows from the box's top and bottom */
{
    cv_text(x + (w - 1 - ink_col(nm, 0) - ink_col(nm, 1)) / 2, y + 1, &FONT_S, nm, c);
}
static uint8_t bk_led[11];                      /* the black keys' function lights, as keys_lit set them last: 0
                                                   dark, 1 a glow, 2 full (a blink: between the two) */
static int32_t btn(int32_t x, int32_t y, int32_t w, const char *nm, uint32_t st)   /* a panel button (2026-11-22,
                                                   the user's tidier pages): its name in the middle, st 0 out (an
                                                   outline), 1 lit (red), 2 lit but its engine off (a dark red); the
                                                   x after it. 16 rows, as ALGORITHM's: the same shape lit or not */
{
    if (st) cv_box(x, y, w, 16, st == 1u ? C_RED : C_RED_D);
    else cv_edge(x, y, w, 16, C_ICE_D);
    btn_text(x, y, w, nm, st == 1u ? C_BG : C_ICE_D);
    return x + w;
}
static void cv_quad_fill(const int32_t *px, const int32_t *py, uint16_t c)   /* a convex quad, by rows */
{
    int32_t y, ymin = py[0], ymax = py[0], k;
    for (k = 1; k < 4; k++) { if (py[k] < ymin) ymin = py[k]; if (py[k] > ymax) ymax = py[k]; }
    for (y = ymin; y <= ymax; y++) {
        int32_t lo = 9999, hi = -9999;
        for (k = 0; k < 4; k++) {
            int32_t a = k, b = (k + 1) & 3, x;
            if ((py[a] <= y && py[b] >= y) || (py[b] <= y && py[a] >= y)) {
                x = py[a] == py[b] ? px[a] : px[a] + (px[b] - px[a]) * (y - py[a]) / (py[b] - py[a]);
                if (x < lo) lo = x;
                if (x > hi) hi = x;
                if (py[a] == py[b]) { if (px[b] < lo) lo = px[b]; if (px[b] > hi) hi = px[b]; }
            }
        }
        if (lo <= hi) cv_rect(lo, y, hi - lo + 1, 1, c);
    }
}


static char *put(char *p, const char *a) { while (*a) *p++ = *a++; return p; }
static char *put_u(char *p, uint32_t v)
{
    char t[10];
    uint32_t i = 0;
    do { t[i++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (i) *p++ = t[--i];
    return p;
}


/* The splash: the logo, stars streaming past behind it and now and then one shooting across. 1.5 s, drawn in two
 * halves a frame; the main loop's platform_poll keeps USB and MIDI alive meanwhile. (In a browser, web/app/, the
 * main loop draws its frames a pass at a time: splash_begin, then splash_draw.) The stars drift as the page's sky
 * does (web/index.html): each at a depth z, 6 .. 96 px a second as z squared, its trail 1 .. 23 px, the near ones
 * brighter. Spread even: one in each cell of a grid of 7 x 8, somewhere in it, so no part of the sky is empty */
#define SPLASH_MS 1500u
#define STARS 56u
static int32_t sx[STARS];                       /* (1/1024 px) */
static int16_t sy[STARS], sv[STARS];
static uint8_t sz[STARS];
static void splash_begin(void)
{
    uint32_t seed = 0x5D15u, i;
    for (i = 0; i < STARS; i++) {
        seed = seed * 1103515245u + 12345u;
        sx[i] = (int32_t)(((i % 7u) * (240u << 10) + (seed >> 8 & 0xFFFFFFu) % (240u << 10)) / 7u);
        seed = seed * 1103515245u + 12345u; sy[i] = (int16_t)((i / 7u) * 30u + (seed >> 8 & 0xFFFFu) % 30u);
        seed = seed * 1103515245u + 12345u; sz[i] = (uint8_t)(seed >> 8 & 0xFFu);   /* the depth, 0 .. 255 */
        sv[i] = (int16_t)(6 + sz[i] * sz[i] * 92 / 65025);   /* 1/1024 px a ms: 0.006 .. 0.096 px (the page's) */
    }
}
static void splash_draw(uint32_t now);
static void splash(void)
{
    uint32_t t0 = fm1_ms, last = ~0u;
    splash_begin();
    while (fm1_ms - t0 < SPLASH_MS) {
        uint32_t now = fm1_ms - t0;
        platform_poll();
        if (now / 33u == last) continue;        /* ~30 frames a second */
        last = now / 33u;
        splash_draw(now);
    }
}
static void splash_draw(uint32_t now)           /* a frame, now ms in */
{
    uint32_t i, half;
    {
        for (half = 0; half < 2u; half++) {
            int32_t y0 = (int32_t)half * 120;
            cv_begin(240, 120, C_BG);
            for (i = 0; i < STARS; i++) {       /* drifting right to left, the near ones long and bright */
                int32_t x = (sx[i] - sv[i] * (int32_t)now) % (240 << 10), len = 1 + sz[i] * sz[i] * 22 / 65025;
                uint32_t a = (uint32_t)(1 + sz[i] * 5 / 255);
                if (x < 0) x += 240 << 10;
                x >>= 10;
                if (sy[i] < y0 || sy[i] >= y0 + 120) continue;
                cv_blend(x, sy[i] - y0, C_ICE, a > 7u ? 7u : a);
                for (int32_t k = 1; k <= len; k++) cv_blend(x + k, sy[i] - y0, C_ICE, (a * (uint32_t)(len - k + 1)) / (uint32_t)(len + 2));
            }
            {                                   /* the shooting star: top right to lower left, 0.2 .. 1.1 s */
                int32_t f = (int32_t)now - 200;
                if (f >= 0 && f < 900) {
                    int32_t hx = 270 - f * 330 / 900, hy = 14 + f * 165 / 900, k;
                    for (k = 70; k >= 0; k--) { /* its trail, fading: ice near the head, red behind */
                        int32_t x = hx + k * 2, y = hy - k;
                        uint32_t a = (uint32_t)(7 - k * 6 / 70);
                        uint16_t c = k < 18 ? C_ICE : C_RED;
                        cv_blend(x, y - y0, c, a);
                        cv_blend(x + 1, y - y0, c, a);
                        cv_blend(x, y + 1 - y0, c, a / 2u);
                    }
                    for (k = -2; k <= 2; k++) { /* the head, a small glow */
                        cv_blend(hx + k, hy - y0, C_ICE, 7u - (uint32_t)(k < 0 ? -k : k));
                        cv_blend(hx, hy + k - y0, C_ICE, 7u - (uint32_t)(k < 0 ? -k : k) * 2u);
                    }
                }
            }
            if (!half) {
                draw_logo(&SD_LOGO_STAR, (240 - SD_LOGO_DISTROYER.w) / 2, 120 - SD_LOGO_STAR.h - 9);   /* (the lines
                                                   start together, 9 + 2 px apart) */
            } else {
                draw_logo(&SD_LOGO_DISTROYER, (240 - SD_LOGO_DISTROYER.w) / 2, 2);
                cv_text((240 - text_w(&FONT_S, "FM-1 EDITION")) / 2, 40, &FONT_S, "FM-1 EDITION", C_ICE_D);
            }
            cv_blit(0, (uint32_t)y0);
        }
    }
}

/* ---- K1..K4 on the FM-1: a fast turn goes further. Slow, a step a detent; faster, more a detent, till a fast
 * spin (60 detents a second and up) crosses a value's whole range in about ENC_FAR_DET detents. enc_take keeps each
 * knob's pace (the detents' interval, smoothed; still for ENC_FAR_REST ms or turned back: a step a detent again) and
 * leaves the last take's in enc_fr; enc_far(e, n) scales a take for a value n detents long. knob_q and the long
 * values take it; a list, a note, a semitone's transpose or a step stays a detent each, and so do SELECT, ALGORITHM and
 * PRESETS. In a browser (DINGHY_HOST) the knobs keep their own feel: a detent a step */
#define ENC_FAR_DET 24u
#define ENC_FAR_REST 100u
static uint8_t enc_fr;                          /* the last take's pace: 0 (slow, or not K1..K4) .. 64 (a fast spin) */
#if !defined(DINGHY_HOST)
static uint32_t kn_at[4];                       /* K1..K4: the last take's time, ms */
static uint8_t kn_iv[4];                        /* the detents' interval, smoothed, ms (0: from rest) */
static int8_t kn_dir[4];
static void enc_pace(uint32_t e, int32_t v)
{
    uint32_t k = e - (uint32_t)ENC_KNOB1, n = (uint32_t)(v < 0 ? -v : v), i, s;
    enc_fr = 0;
    if (k >= 4u) return;
    i = (fm1_ms - kn_at[k]) / n;
    kn_at[k] = fm1_ms;
    if (i > ENC_FAR_REST || (v < 0) != (kn_dir[k] < 0)) kn_iv[k] = 0;   /* from rest, or back: fine again */
    else {
        if (!i) i = 1;
        kn_iv[k] = (uint8_t)(kn_iv[k] ? (kn_iv[k] * 3u + i + 2u) >> 2 : i);
        s = 1000u / kn_iv[k];                   /* detents a second: 12 and under none, 60 and up all */
        enc_fr = (uint8_t)(s <= 12u ? 0u : s >= 60u ? 64u : (s - 12u) * 4u / 3u);
    }
    kn_dir[k] = (int8_t)(v < 0 ? -1 : 1);
}
#endif
static int32_t enc_far(int32_t e, uint32_t n)
{
    uint32_t top = n > ENC_FAR_DET ? n * 64u / ENC_FAR_DET : 64u;
    return e * (int32_t)((64u + (top - 64u) * enc_fr / 64u + 32u) / 64u);
}

/* ---- the patch: the Q page's four knobs, endless, 0..99 (a detent a step, slow) */
static uint32_t q99(uint32_t v) { return (v * 99u + 32767u) / 65535u; }
static uint16_t knob_q(uint16_t v, int32_t e)
{
    int32_t n = (int32_t)q99(v) + enc_far(e, 99u);
    n = n < 0 ? 0 : n > 99 ? 99 : n;
    return (uint16_t)((uint32_t)n * 65535u / 99u);
}

/* ---- MIDI in: from the main loop, and from app_idle while the screen draws (idle_ok) */
static struct { uint8_t oct, bpm, patch, mode, pattern, amode, adiv, aoct, agate, ahold, xchrom, sync_int, master, accent,
                one_core, song, drum, dpad; SdDrumKit dkit; SdDrumPat dpat; SdDrumMore dmore; uint8_t dperc, click; } set =
    {0, 120, SD_PATCH_WORST, SD_MODE_MONO, 0, SD_AM_UP, 2, 1, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, {{{{0}}}}, {{{0}}}, {{{0}}}, 0, 0};   /* (xchrom: the transposes
                                                   in semitones; 0, the default, IN KEY: in SEQ A's scale. sync_int: a
                                                   MIDI clock not heard; 0, the default, AUTO: one leads when it
                                                   comes. master: the track whose one leads, 0 A, 1 B. one_core:
                                                   SETUP's CORES 1, read at the next start; 0, the default, two. song:
                                                   ORBIT's, the place it was loaded from or saved to. drum: the
                                                   beat's place, D01..D16; dkit, dpat: the drums' sounds and the beat
                                                   as the page left them, kept; the drums themselves off at every
                                                   start. dmore: the steps' ROLL and PROB, dperc the fourth's ENGINE
                                                   (2026-11-20: after the rest, so a record from before still loads).
                                                   click: SETUP's CLICK, 0 OFF, 1 REC, 2 PLAY (2026-11-23, in the
                                                   pad byte: a record from before has it OFF) */
                                                /* kept in flash; DISTROY first: the costliest patch, so fm1-emu
                                                   measures it. patch: the factory's SD_PATCHES, then the user's */
_Static_assert(sizeof set + STORE_FOOT <= SECTOR_MAX, "a sector holds the settings (and its copy's number)");

#if FM1_CPU1
static int app_cores(void)                      /* SETUP's CORES as saved: 1 or 2 (none saved: 2) */
{
    __typeof__(set) s = set;
    if (settings_load(&s, sizeof s) && settings_load(&s, (uint32_t)(&s.drum - &s.oct)) &&
        settings_load(&s, (uint32_t)(&s.song - &s.oct))) return 2;   /* (as saved before the drums or the songs, or
                                                   none) */
    return s.one_core == 1u ? 1 : 2;
}
#endif

/* ---- what the player saves (SAVE): 32 patches in slot 0, 32 patterns in slots 1 and 2, 16 songs (ORBIT) in slot 3,
 * in the pool (no RAM). A change of a struct's size reads as nothing saved (store.h's length check): the factory and
 * defaults then, unless the start moves the old size over (the patches before OFFSET, the ORBITs before songs) */
#define UPATCHES 32u
#define UPATTERNS 32u
typedef struct { uint32_t used; char name[UPATCHES][8]; SdPatch p[UPATCHES]; } UserPatches;
typedef struct { uint32_t used; SdPattern p[UPATTERNS]; } UserPatterns;
/* the patterns in two slots (1, 2): 32 of them, with their chords, pass one slot's 16 KB */
#define USEQ_HALF ((uint32_t)__builtin_offsetof(UserPatterns, p[UPATTERNS / 2u]))
_Static_assert(sizeof(UserPatches) + STORE_FOOT <= SLOT_MAX, "a slot holds them (and its copy's number, store.h)");
_Static_assert(sizeof(UserPatterns) / 2u + sizeof(SdPattern) + STORE_FOOT <= SLOT_MAX, "two slots hold the patterns");
#define USONGS 16u
typedef struct { uint32_t used; SdOrbit o[USONGS]; } UserSongs;   /* ORBIT, the songs, in slot 3 */
_Static_assert(sizeof(UserSongs) + STORE_FOOT <= SLOT_B_MAX(3), "slot 3 holds the songs (its second copy a blob)");
typedef struct {                                /* ORBIT as saved till 2026-11-19, a pattern's (SEQ A's): 8 steps, one
                                                   RATE (0 1 2: 1, 2, 4 loops) for all */
    uint8_t on, len, rate, glide, dst[2], pad[2];
    int8_t xa[8], xb[8], lane[2][8], mac[4][8];
} SdOrbitV1;
#define UORB_V1 (4u + UPATTERNS * (uint32_t)sizeof(SdOrbitV1))   /* (its slot's size then: 2308 B) */
_Static_assert(UORB_V1 <= sizeof(UserSongs), "the old ORBITs read into the songs' place");
#define UORB_V2 (4u + USONGS * ((uint32_t)sizeof(SdOrbit) - SD_ORB_STEPS - SD_ORB_TAIL_B))   /* (the songs before
                                                   their drums, 2026-11-19) */
/* the drums' beats, D01..D16, in slot 4 (2026-11-19, the user: "patterns under PRESETS in drum mode"). A place never
 * saved holds the factory's beat (the first eight, named) or nothing */
#define UDRUMS SD_DR_PLACES
typedef struct { uint32_t used; SdDrumPat p[UDRUMS]; SdDrumMore m[UDRUMS]; } UserDrums;   /* (m: 2026-11-20) */
_Static_assert(sizeof(UserDrums) + STORE_FOOT <= SLOT_B_MAX(4), "slot 4 holds the beats (its second copy a blob)");
#define DR_FACTORY 8u
static const char *const DR_FNAME[DR_FACTORY] = {"DRIVE", "HALFTIME", "FOUR", "MOTORIK", "CINEMA", "PULSE", "RADAR",
                                                 "BREAK"};
static const char DR_FBEAT[DR_FACTORY][SD_DRS][17] = {   /* KICK SNARE HAT CLAP, a step each: 0 off 1 on 2 accent
                                                   3 RADAR (plays where RADAR's newest bit is 1) */
    {"2000100020011000", "0000200000002001", "1010102010101021", "0000000000001000"},   /* the lab's */
    {"2000000000100000", "0000000020000000", "1010101010101010", "0000000010000000"},
    {"2000200020002000", "0000000000000000", "0020002000200020", "0000100000001000"},
    {"2000000020100000", "0000100000001000", "2111211121112111", "0000000000000000"},
    {"2000000000020000", "0000000020000000", "0000000000000000", "0000000020000000"},
    {"2000000020000000", "0000000000000000", "1121112111211121", "0000100000001000"},
    {"2000003020000030", "0000100000001000", "3333333333333333", "0000000000000300"},
    {"2000002000100000", "0000100000001001", "1010101010101010", "0000000001000000"}};
#if defined(DINGHY_HOST) && (defined(__APPLE__) || defined(DINGHY_WEB))
static UserPatches upat;
static UserPatterns useq;
static UserSongs usong;
static UserDrums udr;
#else
static UserPatches upat __attribute__((section(".pool")));
static UserPatterns useq __attribute__((section(".pool")));
static UserSongs usong __attribute__((section(".pool")));
#if BOARD_SMK37
static UserDrums udr __attribute__((section(".pool")));
#else
static UserDrums udr;                           /* (the FM-1's pool is full: its RAM has room, 2026-11-20) */
#endif
#endif
/* ---- ALGORITHM's row: the knobs' layers, each page's own (2026-11-22, after SEQ's: the user, "the tidiest we have
 * done; now everywhere"). The row of buttons on the hint line names them, the one the knobs do lit; ALGORITHM turns
 * it, round. The first layer the page's main four, MORE the rest of its settings, TOOLS what acts (a turn: RANDOM,
 * UNDO; CLEAR asked, OCT+ yes) and its switches. GLO hides no knobs any more: it is what the keys do instead of
 * playing (SEQ: the transposes, its key; ORBIT, DRUM: the steps; MACROS: the switches). A page keeps its layer while
 * the FM-1 runs; a page with one layer (SUB, RADAR, MOD) has no row */
static uint8_t pg_lay[16];                      /* each page's layer (SEQ's: seq_layer) */
enum { LN_TONE, LN_XY, LN_ROUTE };            /* NAV */
enum { LR_REACT, LR_TONE, LR_STAGE, LR_FREQ, LR_MORE };   /* REACTOR */
enum { LD_BAND, LD_SHAPE, LD_MIX, LD_LOOP };    /* DISTROY */
enum { LF_FILT, LF_ENV, LF_MORE };              /* FILTER */
enum { LL_LOOPER, LL_SEND, LL_TOOLS };          /* LOOPER */
enum { LO_SONG, LO_PITCH, LO_LANE, LO_MACRO, LO_SETUP };   /* ORBIT */
enum { LK_SOUND, LK_TONE, LK_MORE, LK_MIX, LK_KIT, LK_TOOLS };   /* DRUM */
enum { LU_SETUP, LU_MORE, LU_TOOLS };           /* SETUP */
#define lay_is(pg, l) (pg_lay[pg] == (l))
static SdOrbit orbp;                            /* ORBIT, the song, as the knobs left it */
static uint32_t orb_cur;                        /* the step SELECT (or GLO + its key) picked */
#define orb_setup lay_is(PAGE_ORB, LO_SETUP)    /* (the song's own: LENGTH END GLIDE) */
#define orb_more lay_is(PAGE_ORB, LO_MACRO)          /* (the step's macros) */
#define ORB_HOLD_MS 2000u
static uint32_t orb_hold_at;                    /* the last edit that held ORBIT's step (2026-11-17, the user: held
                                                   only while it is edited; after it, the page shows ORBIT in time) */
static void orb_hold_step(uint32_t s)           /* the step edited: held, heard, till ORB_HOLD_MS after the last edit */
{
    eng.orb_hold = (uint8_t)(s + 1u);
    orb_hold_at = fm1_ms;
}
static void orb_push(void) { sd_engine_orbit(&eng, &orbp); }
static void orb_load(uint32_t i)                /* song i (none saved: off, 4 steps) */
{
    if (usong.used >> i & 1u) orbp = usong.o[i];
    else { uint8_t *z = (uint8_t *)&orbp; uint32_t k; for (k = 0; k < sizeof orbp; k++) z[k] = 0; }
    if (orb_cur >= SD_ORB_LEN(&orbp)) orb_cur = 0;
    orb_push();
}
/* the ORBITs saved with the patterns till 2026-11-19 (slot 3 as it was: 32 of them, 8 steps, one RATE) into songs:
 * each that changes something, in order, a song of its own (16 at most), its pattern SEQ A's on its step 1, its
 * RATE each step's LOOPS (as it played). The slot keeps the old ones till a song is saved. None: no songs */
static void song_migrate(void)
{
    uint8_t *z = (uint8_t *)&usong, from[USONGS];
    uint32_t i, k, m = 0, used;
    if (slot_load(3, &usong, UORB_V1)) { for (k = 0; k < sizeof usong; k++) z[k] = 0; return; }
    used = usong.used;
    for (i = 0; i < UPATTERNS && m < USONGS; i++) {   /* those with something in them to the front (in the old
                                                   size: a place at or below where each was) */
        const uint8_t *s = z + 4u + i * (uint32_t)sizeof(SdOrbitV1);
        const SdOrbitV1 *o = (const SdOrbitV1 *)s;
        uint32_t any = o->on;
        if (!(used >> i & 1u)) continue;
        for (k = 0; k < 8u; k++)
            any |= (uint32_t)(o->xa[k] | o->xb[k] | o->lane[0][k] | o->lane[1][k] | o->mac[0][k] | o->mac[1][k] |
                              o->mac[2][k] | o->mac[3][k]);
        if (!any) continue;
        if (m != i) { uint8_t *d = z + 4u + m * (uint32_t)sizeof(SdOrbitV1); for (k = 0; k < sizeof(SdOrbitV1); k++) d[k] = s[k]; }
        from[m++] = (uint8_t)i;
    }
    for (i = m; i-- > 0u;) {                    /* then each to its new place, the last first (the new places lie
                                                   above the old ones, and each is read before it is written over) */
        SdOrbitV1 v = *(const SdOrbitV1 *)(z + 4u + i * (uint32_t)sizeof(SdOrbitV1));
        SdOrbit *d = &usong.o[i];
        uint8_t *dz = (uint8_t *)d;
        for (k = 0; k < sizeof *d; k++) dz[k] = 0;
        d->on = v.on; d->len = v.len; d->end = SD_OE_LOOP; d->glide = v.glide;
        d->dst[0] = v.dst[0]; d->dst[1] = v.dst[1];
        for (k = 0; k < 8u; k++) {
            d->xa[k] = v.xa[k]; d->xb[k] = v.xb[k];
            d->lane[0][k] = v.lane[0][k]; d->lane[1][k] = v.lane[1][k];
            d->mac[0][k] = v.mac[0][k]; d->mac[1][k] = v.mac[1][k]; d->mac[2][k] = v.mac[2][k]; d->mac[3][k] = v.mac[3][k];
            d->loops[k] = (uint8_t)(1u << (v.rate > 2u ? 2u : v.rate));
        }
        if (useq.used >> from[i] & 1u) d->pat[0][0] = (uint8_t)(from[i] + 1u);
    }
    for (k = 4u + m * (uint32_t)sizeof(SdOrbit); k < sizeof usong; k++) z[k] = 0;
    usong.used = m ? (1u << m) - 1u : 0u;
}
#define NPATCHES (SD_PATCHES + UPATCHES)
static uint32_t save_mode, save_to, save_pending, save_at, saved_at, save_kind;   /* SAVE: picking (1 patch, 2
                                                   pattern, 3 song, 4 beat), where, what waits to be written (bits 1,
                                                   2, 4 and 8: the patches', the patterns', the songs', the beats'),
                                                   the time it waits from, when it was saved (the screen's 1.5 s),
                                                   what */
static uint32_t save_places(uint32_t kind)
{
    return kind == 4u ? UDRUMS : kind == 3u ? USONGS : kind == 2u ? UPATTERNS : UPATCHES;
}
static uint32_t pat_waiting;                    /* the pattern + 1 PRESETS loaded while it plays, till the one (a
                                                   song: 0x100 more) */

static const char *patch_name(uint32_t i)       /* the factory's name, or the user's (the one it was saved from) */
{
    static char n[9];
    uint32_t k;
    if (i < SD_PATCHES) return SD_PATCH_NAME[i];
    for (k = 0; k < 8u; k++) n[k] = upat.name[i - SD_PATCHES][k];
    n[8] = 0;
    return n;
}
static uint32_t patch_ok(uint32_t i) { return i < SD_PATCHES || (i < NPATCHES && upat.used >> (i - SD_PATCHES) & 1u); }
static void tur_slot(SdPatch *p)                /* RADAR's own DEST (a patch from before 2026-11-21) into the first
                                                   free MOD slot: to the matrix AMOUNT x 327, the engine's own sum; to
                                                   RADAR's own three the knob's number, as the engine reads it there; so
                                                   it sounds the same (none free: it stays, the engine still plays it) */
{
    uint32_t i;
    if (!p->tur_dst) return;
    for (i = 0; i < SD_MODS; i++)
        if (!p->mod[i].src && !p->mod[i].dst) {
            p->mod[i].src = SD_MS_TUR;
            p->mod[i].dst = p->tur_dst;
            p->mod[i].amt = (int16_t)(p->tur_dst >= SD_MDSTS ? p->tur_amt * 32767 / 99 : p->tur_amt * 327);
            p->mod_off[i] = 0;
            p->tur_dst = 0;
            p->tur_amt = 0;
            return;
        }
}
static void patch_load(uint32_t i, SdPatch *p)
{
    *p = i < SD_PATCHES ? SD_PATCH[i] : upat.p[i - SD_PATCHES];
    tur_slot(p);
    sd_patch_levels(p);                         /* (one from before the make-ups, 2026-11-21: onto them, as it sounded) */
    sd_patch_macros(p);                         /* (one from before HOME's MORE, 2026-11-23: its MACRO 4 to its knob) */
    if (p->f_type == SD_FT_OFF && !p->f_cut && !p->f_res && !p->f_env && !p->f_vel && !p->f_key) {   /* the filter never
                                                   set: on, at 12 kHz, as good as open (the engine runs it once it is
                                                   heard), so its knobs work at once (2026-11-19, the user) */
        p->f_type = SD_FT_LP12;
        p->f_cut = SD_FCUT_OPEN;
    }
}
static SdPatch pat;                             /* the patch as the knobs left it (not saved: PRESETS reloads it) */
static uint32_t ebank, gen = 0xFFFFFFFFu, gen_seed = 1;   /* the edited bank; the last generator (K4, GLO) */
static uint32_t page = 5u;                      /* 0 NAV, 1 REACTOR, 2 DISTROY, 3 SPACE, 4 MOD, 5 HOME (the macros:
                                                   the boot's page), 6 SEQ, 7 ENV, 8 FILTER, 9 ARP: each its button's */
#define PAGE_SEQ 6u
#define PAGE_ENV 7u
#define PAGE_FILT 8u
#define PAGE_ARP 9u
#define PAGE_ORB 10u                            /* SEQ's third: ORBIT */
#define PAGE_TIME 11u                           /* FX's fourth: TIME, the delay */
#define PAGE_LOOP 12u                           /* ARP's second: LOOPER, the looper (the player's, not the patch's) */
#define PAGE_SETUP 13u                          /* HOME's second: SETUP, the FM-1's settings (kept in flash; its third
                                                   till 2026-11-21, NAV between) */
#define PAGE_SUB 15u                            /* ENV's fourth: SUB, the sub oscillator (2026-11-21) */
#define PAGE_DRUM 14u                           /* SEQ's fourth: DRUM, the drums (sd_drums.h; off at every start) */
static uint32_t fx_last = 1u, env_last = 0u, arp_last = PAGE_ARP, seq_last;   /* the page buttons: each the page it
                                                   showed last, where it comes back to (ENV: NAV at first; SEQ: 0 SEQ A,
                                                   1 SEQ B, 2 ORBIT, 3 DRUM; LFO: dmod) */
/* the screen's rows (the frame, below) */
#define Y_NAV 24u                               /* (the patch's line on top: the logo is the splash's) */
#define Y_KNOB 46u
#define H_KNOB 50u
#define Y_PIC 98u
#define H_PIC 104u                              /* (the steps' row gone, 2026-11-15: the picture took its room) */
#define H_PIC_TUR 122u                          /* RADAR's: down to the status (the hint's row under it, drawn over its
                                                   foot only while it has something to say: draw_frame) */
#define Y_HINT 204u
#define Y_STAT 222u
#define H_LIST (Y_PIC + H_PIC - Y_KNOB)         /* PRESETS' list, a DEST's: over the knobs and the picture */
static uint32_t dslot;                          /* the MOD slot last picked (RADAR was MOD's ninth for a day,
                                                   2026-11-19: the user found it gone; its own tab again, LFO's third) */
static uint32_t seq_trk;                        /* the track SEQ edits: 0 A, 1 B */
static SdPattern seqp, seq_undo;                /* the pattern as the keys left it; the one before the last RND, CLR */
static uint32_t seq_has_undo, seq_cur, seq_bank, seq_dseg, seq_rec, seq_clr_at;
/* SEQ's layers (2026-11-22, the user: "we can't have G5, find something smarter", as the other pages were tidied): what
   the knobs do, picked with ALGORITHM from a row of buttons on the hint line (ALGORITHM had been a second SELECT there).
   No key has a job on SEQ any more: they play, and with GLO every one of them transposes */
enum { SL_STEP, SL_MORE, SL_TRACK, SL_PARTS, SL_TOOLS, SL_N };
static const char *const SL_NAME[SL_N] = {"STEP", "MORE", "TRACK", "PARTS", "TOOLS"};
static uint32_t seq_layer;                      /* STEP: NOTE VEL GATE TYPE; MORE: RATCHET PROB MOVE LENGTH; TRACK:
                                                   LENGTH DIV SWING XPOSE; PARTS: PARTS REPEAT ORDER LENGTH (SELECT the
                                                   part); TOOLS: RANDOM UNDO CLEAR DUB */
#define seq_segmode (seq_layer == SL_PARTS)     /* (read only: the parts' layer) */
#define seq_more (seq_layer == SL_MORE)         /* (read only: the step's second layer) */
static uint8_t seq_dub;                         /* TOOLS' DUB: REC adds to the steps, takes nothing (the player's) */
static uint32_t seq_roll_at, seq_back_at;       /* TOOLS: the last RANDOM, UNDO (a turn's detents one roll's undo, one
                                                   UNDO) */
static uint32_t seq_rng = 0x5D5Du;

/* ---- the freeze log (2026-11-22, the user: "make something that logs every freeze properly"). A freeze is the main
 * loop stopped: the screen, the keys and the watchdog's feed with it, and 8 s later the watchdog restarts the FM-1.
 * What it leaves, kept through that restart (.noinit): the last EV_N inputs (ev_ring: each button, key and knob, its
 * time and page), last_run (where the main loop stood, the page, PLAY, REC, the CPU) and, once the loop has stood a
 * second, where it is stuck (last_run.pc: app_audio samples the PC it interrupted). The start after it (or after a
 * crash's screen) writes all of it into the log, LOG_SECT sectors of its own after the store's copies (store.h):
 * FRZ_RECS records, the oldest written over. SETUP shows them, SELECT the newest first (a photo of the screen is the
 * report); tools/fm1_diag -l reads them whole over USB. Turned off before the watchdog's 8 s, nothing is kept */
#define EV_N 24u                                /* (the restart's RAM, NOINIT, has room for so many: 2026-11-22) */
typedef struct { uint16_t t8; uint8_t kc; int8_t val; } SdEv;   /* t8: fm1_ms / 8 (its low 16 bits: 8 minutes round);
                                                   kc: the kind << 5 | the button, key or encoder; val: a knob's turn */
enum { EV_BTN = 1, EV_BTN_UP, EV_KEY, EV_KEY_UP, EV_ENC };
#if defined(DINGHY_HOST)
static SdEv ev_ring[EV_N];
#else
static SdEv ev_ring[EV_N] __attribute__((section(".noinit")));   /* (last_run.ev_n counts them) */
#endif
static void ev_put(uint32_t kind, uint32_t code, int32_t val)
{
    SdEv *v = &ev_ring[last_run.ev_n++ % EV_N];
    v->t8 = (uint16_t)(fm1_ms >> 3); v->kc = (uint8_t)(kind << 5 | (code & 31u));
    v->val = (int8_t)(val < -128 ? -128 : val > 127 ? 127 : val);
}
#define FRZ_MAGIC 0x534C4F47u                   /* "SLOG" */
#define FRZ_RECS 16u                            /* 512 bytes each, 8 a sector */
enum { FRZ_FREEZE = 1, FRZ_CRASH = 2 };
typedef struct {
    uint32_t magic, seq, kind, crc;             /* (the crc: of what follows) */
    char ver[16];                               /* the firmware it ran (APP_VER) */
    LastRun run;                                /* as the start found it */
    uint32_t crash[16];                         /* a crash: fm1_crash, as its screen showed it */
    uint32_t ev_n;                              /* the inputs in that run (the last EV_N below, oldest first) */
    SdEv ev[EV_N];
} FrzRec;
_Static_assert(sizeof(FrzRec) <= 512u, "a freeze record: 512 bytes, 8 a sector");
static uint32_t frz_n, frz_top;                 /* the records kept; the newest's number */
static uint32_t setup_log;                      /* SETUP's SELECT: 0 the settings; k the freeze log's k-th newest */
#ifndef DINGHY_HOST
_Static_assert(FRZ_RECS == LOG_SECT * 8u, "the freeze log: 8 records a sector");
static uint32_t frz_crc(const FrzRec *r) { return crc32((const uint8_t *)r + 16, sizeof *r - 16u); }
static int frz_read(uint32_t i, FrzRec *r)      /* place i: 0 if a whole record */
{
    if (!flash_ok || st_read(LOG_AT + i * 512u, r, sizeof *r)) return -1;
    return r->magic == FRZ_MAGIC && r->crc == frz_crc(r) ? 0 : -1;
}
static void frz_write(FrzRec *r)                /* at its number's place; a sector erased when its first place comes,
                                                   or when the place is not blank (another firmware's bytes) */
{
    uint32_t i = r->seq % FRZ_RECS, at = LOG_AT + i * 512u, w[16], k, j, blank = 1, took;
    r->magic = FRZ_MAGIC;
    r->crc = frz_crc(r);
    if (!flash_ok) return;
    for (k = 0; k < 512u && blank; k += sizeof w)
        if (st_read(at + k, w, sizeof w)) blank = 0;
        else for (j = 0; j < 16u; j++) blank &= w[j] == 0xFFFFFFFFu;
    if (i % 8u == 0u || !blank) fl_erase4k(LOG_AT + (i / 8u) * 0x1000u, &took);
    fl_write(at, (const uint8_t *)r, sizeof *r);
}
#else
static FrzRec frz_host[FRZ_RECS];               /* (on a computer, in a browser: in RAM, for the tests) */
static int frz_read(uint32_t i, FrzRec *r) { *r = frz_host[i % FRZ_RECS]; return r->magic == FRZ_MAGIC ? 0 : -1; }
static void frz_write(FrzRec *r) { r->magic = FRZ_MAGIC; frz_host[r->seq % FRZ_RECS] = *r; }
#endif
static void frz_scan(void)                      /* the start: how many, the newest's number */
{
    FrzRec r;
    uint32_t i;
    frz_n = frz_top = 0;
    for (i = 0; i < FRZ_RECS; i++)
        if (!frz_read(i, &r)) { if (!frz_n || (int32_t)(r.seq - frz_top) > 0) frz_top = r.seq; frz_n++; }
}
static int frz_get(uint32_t k, FrzRec *r)       /* the k-th newest (0: the newest) */
{
    return k < frz_n && !frz_read((frz_top - k) % FRZ_RECS, r) && r->seq == frz_top - k ? 0 : -1;
}
static void frz_add(uint32_t kind, const LastRun *run, const uint32_t *crash)   /* what the last run left, the
                                                   inputs oldest first */
{
    FrzRec r;
    uint32_t i, n = run->magic == LAST_RUN_MAGIC ? run->ev_n : 0u;
    const char *v = APP_VER;
    uint8_t *z = (uint8_t *)&r;
    for (i = 0; i < sizeof r; i++) z[i] = 0;
    r.seq = frz_n ? frz_top + 1u : 0u;
    r.kind = kind;
    for (i = 0; i < 15u && v[i]; i++) r.ver[i] = v[i];
    r.run = *run;
    if (crash) for (i = 0; i < 16u; i++) r.crash[i] = crash[i];
    r.ev_n = n;
    if (n) for (i = 0; i < EV_N; i++) r.ev[i] = ev_ring[(n + i) % EV_N];   /* (fewer than EV_N: blanks first) */
    if (n && n < EV_N) for (i = 0; i < EV_N - n; i++) r.ev[i].kc = 0;
    frz_write(&r);
    frz_top = r.seq;
    if (frz_n < FRZ_RECS) frz_n++;
}
static const char *const STYPE_NAME[SD_STYPES] = {"NOTE", "SLIDE", "TIE", "REST", "SKIP"};
static const char *const SORDER_NAME[SD_SORDERS] = {"FWD", "REV", "P-P", "RND"};
static const char *const SORDER_GLYPH[SD_SORDERS] = {"", "<", "<>", "?"};
static const char *const SDIV_NAME[SD_SDIVS] = {"1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4", "1/64", "1/32T",
                                                "1/16D", "1/4T", "1/8D", "1/2"};
static const char *const CLICK_NAME[3] = {"OFF", "REC", "PLAY"};   /* (SETUP's CLICK: never, while SEQ records,
                                                   whenever it plays) */
static const char *const SLIDE_NAME[SD_SLIDES] = {"30 MS", "60 MS", "100 MS", "150 MS", "250 MS", "400 MS", "600 MS",
                                                  "1 S"};   /* (a SLIDE step's TIME, on MORE's K1) */
static const char *const SCALE_NAME[SD_SCALES] = {"MINOR", "MAJOR", "DORIAN", "PHRYG", "PENTA-", "PENTA+", "CHROM",
                                                  "LYDIAN", "MIXO", "HMINOR", "PHRDOM", "DBLHRM", "WHOLE", "HIRA",
                                                  "BLUES"};
static uint32_t dmod = SD_MODS;                 /* LFO's page: slots 0..7 and 10 RADAR (RADAR: MOD's, ALGORITHM picks
                                                   round the nine), 8 LFO 1, 9 LFO 2 (LFO 1 first) */
#define DMOD_TUR (SD_MODS + 2u)
static const char *const SRC_NAME[SD_MSRCS] = {"OFF", "ENV1", "ENV2", "LFO1", "LFO2", "NAV X", "NAV Y", "VEL", "KEY",
                                               "WHEEL", "AFTER", "RAND", "STEP", "SEGMENT", "D LOW",
                                               "D TOP", "R STAGE", "ECHO", "ENV3", "RADAR", "KICK", "SNARE",
                                               "SEQ A", "SEQ B", "RADAR 1"};
/* the destinations, in the enum's order (SD_MD_*). In the list (draw_dlist): the page the knob is on (DG_*) and its
   name there; on a knob (7 characters) and in the LFOs' pictures the name alone, with the FX page's letter where
   another page has a knob of that name (R REACTOR, the band's or LP the loop on DISTROY; as the pages' R-LEVEL,
   D-LEVEL and L-LEVEL, R- the reverb on SPACE, D- the delay and L- the looper on TIME); in ORBIT's lanes three
   letters */
static const char *const DST_NAME[SD_MDSTS] = {"OFF", "PITCH", "XM", "WARP", "FOLD", "DETUNE", "NAV X", "NAV Y",
                                               "WIDTH", "R POS", "R FREQ", "R RES", "R DRIVE", "R MIX", "DRV ALL",
                                               "BIAS", "LP FDBK", "LP TAME", "LP MIX", "D-FDBK", "D-LEVEL", "R-LEVEL",
                                               "R-DECAY", "DRV TOP", "SPL HM", "CUTOFF", "F RES", "E1 ATK", "E1 DEC",
                                               "E1 SUS", "E1 REL", "E2 ATK", "E2 DEC", "E2 SUS", "E2 REL", "E3 ATK",
                                               "E3 DEC", "E3 SUS", "E3 REL", "F ENV", "R SLEW", "LP TIME", "LP SPRD",
                                               "D-TIME", "D-TONE", "R-SIZE", "R-TONE", "L1 RATE", "L2 RATE", "L-SPEED",
                                               "SPL LOW", "SPL LM", "DRV LOW", "DRV LM", "DRV HM", "BOOM", "SIZE", "AIR",
                                               "P MACRO", "DRUMS", "VOLUME", "P TUNE", "FRZ PCH"};
/* the destinations' list (as PRESETS' list: easier to see and hit): each its page and its knob's name there, in
   SD_MD_ORDER (the pages' order, and on each its knobs': K1..K4, then GLO's) */
enum { DG_NONE, DG_VOICE, DG_NAV, DG_ENV1, DG_ENV2, DG_FILT, DG_ENV3, DG_REACT, DG_DEST, DG_SPACE, DG_TIME, DG_LFO1,
       DG_LFO2, DG_SEQ, DG_DRUM, DG_SYNTH };
static const char *const DG_NAME[] = {"", "VOICE", "NAV", "ENV 1", "ENV 2", "FILTER", "ENV 3", "REACTOR", "DISTROY",
                                      "SPACE", "TIME", "LFO 1", "LFO 2", "SEQ A", "DRUM", "SYNTH"};
static const uint8_t DST_GRP[SD_MDSTS] = {DG_NONE, DG_VOICE, DG_NAV, DG_NAV, DG_NAV, DG_NAV, DG_NAV, DG_NAV, DG_NAV,
    DG_REACT, DG_REACT, DG_REACT, DG_REACT, DG_REACT, DG_DEST, DG_DEST, DG_DEST, DG_DEST, DG_DEST, DG_TIME,
    DG_TIME, DG_SPACE, DG_SPACE, DG_DEST, DG_DEST, DG_FILT, DG_FILT, DG_ENV1, DG_ENV1, DG_ENV1, DG_ENV1, DG_ENV2,
    DG_ENV2, DG_ENV2, DG_ENV2, DG_ENV3, DG_ENV3, DG_ENV3, DG_ENV3, DG_FILT, DG_REACT, DG_DEST, DG_DEST, DG_TIME,
    DG_TIME, DG_SPACE, DG_SPACE, DG_LFO1, DG_LFO2, DG_TIME, DG_DEST, DG_DEST, DG_DEST, DG_DEST,
    DG_DEST, DG_DRUM, DG_DRUM, DG_DRUM, DG_DRUM, DG_DRUM, DG_SYNTH, DG_DRUM, DG_SPACE};
static const char *const DST_PARAM[SD_MDSTS] = {"OFF", "PITCH", "XM", "WARP", "FOLD", "DETUNE", "X", "Y", "WIDTH",
    "POS", "FREQ", "RES", "DRIVE", "MIX", "ALL DRIVE", "ALL BIAS", "LOOP FDBK", "LOOP TAME", "LOOP MIX", "FDBK",
    "D-LEVEL", "R-LEVEL", "DECAY", "TOP DRIVE", "HI-MID SPLIT", "CUTOFF", "RES", "ATTACK", "DECAY", "SUSTAIN",
    "RELEASE", "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "ENV", "SLEW",
    "LOOP TIME", "LOOP SPREAD", "TIME", "TONE", "SIZE", "TONE", "RATE", "RATE", "LOOP SPEED", "LOW SPLIT",
    "LO-MID SPLIT", "LOW DRIVE", "LO-MID DRIVE", "HI-MID DRIVE", "KICK BOOM", "SNARE SIZE", "HAT AIR",
    "PERC MACRO", "LEVEL", "VOLUME", "PERC TUNE", "FREEZE PITCH"};
/* RADAR 1's own DESTs (SD_TG_*): on a knob and in MOD's table; in the list each its page and what it does there */
static const char *const TG_NAME[SD_TGS] = {"RESTART", "R NEXT", "SEQ A", "SEQ B", "ARP", "REPEAT"};
static const char *const TG_GRP[SD_TGS] = {"ENV 1 2 3", "REACTOR", "SEQ A", "SEQ B", "ARP", "LOOPER"};
static const char *const TG_PARAM[SD_TGS] = {"RESTART ON 1", "NEXT STAGE ON 1", "PLAYS ON 1", "PLAYS ON 1",
                                             "PLAYS ON 1", "REPEATS ON 1"};
#define TG_IS(d) ((d) >= SD_TD_TRIG && (d) < SD_TD_TRIG + SD_TGS)
static const char *tdest_name(uint32_t d)       /* RADAR's DEST: a matrix destination, or NOTES, or the drums', or
                                                   RADAR 1's */
{
    return d == SD_TD_NOTES ? "NOTES" : d == SD_TD_DACC ? "ACCENTS" : d == SD_TD_DROLL ? "ROLLS" :
           TG_IS(d) ? TG_NAME[d - SD_TD_TRIG] : d < SD_MDSTS ? DST_NAME[d] : "OFF";
}
static const char *const LSHAPE_NAME[SD_LSHAPES] = {"SINE", "TRI", "SAW", "RAMP", "SQUARE", "S&H"};
#define LMODES 10u
static const char *const LMODE_SAY[LMODES] = {"FREE RUNNING", "SYNC TO THE TEMPO", "FREE, RESTARTS EACH NOTE",
                                             "SYNC, RESTARTS EACH NOTE", "FREE, ONE CYCLE A NOTE",
                                             "SYNC, ONE CYCLE A NOTE", "ONE CYCLE ON KICK", "ONE CYCLE ON SNARE",
                                             "ONE CYCLE ON RADAR 1", "ONE CYCLE EACH BAR"};   /* (on the hint line:
                                                   what each does) */
static const char *const LCYC_NAME[7] = {"LOOPS", "NOTE", "ONCE", "KICK", "SNARE", "RADAR 1", "BAR"};   /* MODE's
                                                   K2, CYCLE: it loops on, restarts each note, one cycle a note, one
                                                   struck by KICK, SNARE, RADAR's 1, the bar (those four synced) */
static const uint8_t LMODE_BITS[LMODES] = {0, SD_LM_SYNC, SD_LM_KEY, SD_LM_SYNC | SD_LM_KEY, SD_LM_ONE,
                                           SD_LM_SYNC | SD_LM_ONE, SD_LM_SYNC | SD_LM_ONE | 1u << 3,
                                           SD_LM_SYNC | SD_LM_ONE | 2u << 3, SD_LM_SYNC | SD_LM_ONE | 3u << 3,
                                           SD_LM_SYNC | SD_LM_ONE | 4u << 3};   /* ONE: one cycle a note (or a KICK, a
                                                   SNARE, RADAR's 1, the bar: 2026-11-20, an envelope struck by the
                                                   beat), held at its end */
static uint32_t lmode_of(uint32_t bits) { uint32_t i; for (i = 0; i < LMODES; i++) if (LMODE_BITS[i] == (bits & 0x3Fu)) return i; return 0; }
static uint32_t lcyc_of(uint32_t m) { return m < 6u ? m / 2u : m - 3u; }   /* a mode's CYCLE (LCYC_NAME) */
static uint32_t lmode_at(uint32_t cyc, uint32_t sync) { return cyc < 3u ? cyc * 2u + (sync ? 1u : 0u) : cyc + 3u; }
static const char *const LDIV_NAME[SD_LFO_DIVS] = {"1/16", "1/8", "1/8D", "1/4", "1/4D", "1/2", "1/2D", "1 BAR",
                                                   "1.5 BAR", "2 BAR", "3 BAR", "4 BAR"};
static const char *const DIV_NAME[SD_LP_NDIV] = {"1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D",
                                                 "1/4", "1/2T", "1/4D", "1/2", "1 BAR", "2 BAR", "4 BAR"};   /* (the
                                                   last three the looper's LENGTH only) */
static const char *const SUB_NAME[SD_SUBS] = {"A", "B", "ROOT"};   /* (what SUB follows) */
/* each page's layers (pg_lay), in its row's order */
static const char *const LY_NAV[3] = {"TONE", "X/Y", "ROUTE"}, *const LY_REAC[5] = {"REACT", "TONE", "STAGE", "F1-F4", "MORE"},
                         *const LY_DIST[4] = {"BAND", "SHAPE", "MIX", "LOOP"}, *const LY_SPACE[3] = {"PLATE", "FREEZE", "MORE"},
                         *const LY_TIME[2] = {"DELAY", "MODE"}, *const LY_FILT[3] = {"FILTER", "ENV 3", "MORE"},
                         *const LY_ENV[2] = {"ENV 1", "ENV 2"}, *const LY_LFO[2] = {"LFO", "MODE"},
                         *const LY_ARP[2] = {"ARP", "TOOLS"}, *const LY_LOOP[3] = {"LOOPER", "SEND", "TOOLS"},
                         *const LY_ORB[5] = {"SONG", "PITCH", "LANE", "MACRO", "SETUP"},
                         *const LY_DRUM[6] = {"DRUM", "TONE", "MORE", "MIX", "KIT", "TOOLS"},
                         *const LY_MAC[2] = {"MACROS", "MORE"}, *const LY_SET[3] = {"SETUP", "MORE", "TOOLS"};
static uint32_t lay_names(const char *const **nm)   /* the page's layers: how many (1: no row), their names */
{
    switch (page) {
    case 0: *nm = LY_NAV; return 3;
    case 1: *nm = LY_REAC; return 5;
    case 2: *nm = LY_DIST; return 4;
    case 3: *nm = LY_SPACE; return 3;
    case 4: if (dmod >= SD_MODS && dmod != DMOD_TUR) { *nm = LY_LFO; return 2; } break;   /* (the LFOs; RADAR, MOD: one) */
    case 5: *nm = LY_MAC; return 2;
    case 6: *nm = SL_NAME; return SL_N;
    case 7: *nm = LY_ENV; return 2;
    case 8: *nm = LY_FILT; return 3;
    case 9: *nm = LY_ARP; return 2;
    case 10: *nm = LY_ORB; return 5;
    case 11: *nm = LY_TIME; return 2;
    case 12: *nm = LY_LOOP; return 3;
    case 13: *nm = LY_SET; return 3;
    case 14: *nm = LY_DRUM; return 6;
    default: break;                             /* (SUB) */
    }
    *nm = 0;
    return 1;
}
static uint32_t lay(void) { return page == 6u ? seq_layer : pg_lay[page & 15u]; }   /* the layer the knobs do here */
static uint32_t dband;                         /* DISTROY's band SELECT picks, 0..3 (LOOP: a layer since 2026-11-22,
                                                   ALGORITHM's fifth band before) */
#define d_loop lay_is(2u, LD_LOOP)
static const char *const SHAPE_NAME[SD_DSHAPES] = {"NONE", "SOFT", "HARD", "FOLD", "CRUSH"};
/* DISTROY's page: what ALT does for each type, the bands' names */
static const char *const ALT_NAME[SD_DSHAPES] = {"--", "KNEE", "KNEE", "SPACING", "DEPTH"};
static const char *const BAND_NAME[4] = {"LOW", "LO-MID", "HI-MID", "TOP"};
static const char *const MODEL_NAME[SD_RMODELS] = {"BP6", "BP12", "VBP", "KP", "AP1", "AP2"};
static const char *const MODE_NAME[SD_MODES] = {"MONO", "DUO", "POLY"};
static const char *const AMODE_NAME[SD_AMODES] = {"UP", "DOWN", "UP-DN", "RND", "ORDER", "IN", "OUT", "PEDAL", "UPx2"};
#include "sd_gen.h"

static void roles_of(const SdPatch *p, char *s)  /* "SSSC": oscillators 1..4, left to right */
{
    uint32_t k;
    for (k = 0; k < SD_OPS; k++) {
        uint32_t r = p->bank[ebank].osc[k].role;
        s[k] = r == SD_ROLE_S ? 'S' : r == SD_ROLE_P ? 'P' : 'C';
    }
    s[SD_OPS] = 0;
}

static uint32_t algo_of(const SdPatch *p)       /* the roles as SD_ALGO's index, or SD_ALGOS when none matches */
{
    char r[SD_OPS + 1u];
    uint32_t a, k;
    roles_of(p, r);
    for (a = 0; a < SD_ALGOS; a++) {
        for (k = 0; k < SD_OPS && SD_ALGO[a][k] == r[k]; k++) {}
        if (k == SD_OPS) return a;
    }
    return SD_ALGOS;
}

static uint32_t step_mod(uint32_t v, int32_t e, uint32_t n)   /* v + e detents, round a list of n */
{
    int32_t t = ((int32_t)v + e % (int32_t)n + (int32_t)n) % (int32_t)n;
    return (uint32_t)t;
}
static uint32_t step_lim(uint32_t v, int32_t e, uint32_t n)   /* v + e detents, held at 0 .. n - 1 */
{
    int32_t t = (int32_t)v + e;
    return (uint32_t)(t < 0 ? 0 : t > (int32_t)n - 1 ? (int32_t)n - 1 : t);
}
static const char *slot_dname(const SdModSlot *m)   /* a MOD slot's DEST as the knob says it (RADAR's own three) */
{
    return m->dst >= SD_MDSTS ? tdest_name(m->dst) : DST_NAME[m->dst];
}
#define N_SRC (SD_MSRCS - (SD_DRUMS ? 0u : 2u))   /* the sources' list (no drums: KICK, SNARE, SRC_ORDER's last two,
                                                   not in it) */
#define N_DST (SD_MDSTS - (SD_DRUMS ? 0u : 6u))   /* the destinations' (no drums: their six, SD_MD_ORDER's last) */
static uint32_t dst_rank(uint32_t d)            /* its place in the list */
{
    uint32_t r = 0;
    while (r < SD_MDSTS - 1u && SD_MD_ORDER[r] != d) r++;
    return r;
}
static uint32_t tdst_rank(uint32_t d)           /* RADAR's list: the matrix's, then DRM ACC, DRM ROLL, NOTES last (so a
                                                   turn down from OFF is NOTES, as it was) */
{
    return d == SD_TD_DACC ? N_DST : d == SD_TD_DROLL ? N_DST + 1u : d == SD_TD_NOTES ? N_DST + 2u * SD_DRUMS : dst_rank(d % SD_MDSTS);
}
#define N_TDST (N_DST + 1u + 2u * SD_DRUMS)     /* (RADAR's list: the matrix's, the drums' two, NOTES) */
static uint32_t tdst_of(uint32_t r)
{
    return r < N_DST ? SD_MD_ORDER[r] : SD_DRUMS && r == N_DST ? SD_TD_DACC : SD_DRUMS && r == N_DST + 1u ? SD_TD_DROLL :
           SD_TD_NOTES;
}
#define N_T1DST (N_DST + SD_TGS)                /* (RADAR 1's list: the matrix's, then its own) */
static uint32_t t1dst_rank(uint32_t d) { return TG_IS(d) ? N_DST + d - SD_TD_TRIG : dst_rank(d % SD_MDSTS); }
static uint32_t t1dst_of(uint32_t r) { return r < N_DST ? SD_MD_ORDER[r] : SD_TD_TRIG + (r - N_DST) % SD_TGS; }
static uint32_t dst_ok(uint32_t src, uint32_t d)   /* a DEST a slot from src may have (RADAR's and RADAR 1's own:
                                                   theirs only) */
{
    return d < SD_MDSTS || (src == SD_MS_TUR && (d == SD_TD_NOTES || d == SD_TD_DACC || d == SD_TD_DROLL)) ||
           (src == SD_MS_TUR1 && TG_IS(d));
}
static uint32_t dst_step(uint32_t d, int32_t e, uint32_t src)   /* the next in the list (src RADAR or RADAR 1: with
                                                   its own at its end) */
{
    if (src == SD_MS_TUR) return tdst_of(step_mod(tdst_rank(d), e, N_TDST));
    if (src == SD_MS_TUR1) return t1dst_of(step_mod(t1dst_rank(d), e, N_T1DST));
    return SD_MD_ORDER[step_mod(dst_rank(d % SD_MDSTS), e, N_DST)];
}
static uint32_t sdiv_step(uint32_t d, int32_t e)   /* a division, e detents slower (+) or faster (-) */
{
    uint32_t r = 0;
    while (r < SD_SDIVS - 1u && SD_SDIV_ORDER[r] != d) r++;
    return SD_SDIV_ORDER[step_lim(r, e, SD_SDIVS)];
}

static uint32_t idle_ok;
static uint32_t led_at;                         /* the key and button lights: when last set */

/* GLO: what the keys do instead of playing, held, or latched by a tap (down and up within GL_TAP_MS, nothing else
   done meanwhile: a button, a key, a knob); a tap again lets it go. Only where it does something (2026-11-22, the
   knobs' layers on ALGORITHM: SEQ, ORBIT, DRUM, MACROS); elsewhere it neither latches nor lights. (SEL, BTN_SEL: the
   voices, MONO DUO POLY) */
#define GL_TAP_MS 400u
static uint8_t gl_latch, gl_used;
static uint32_t gl_at;
static uint32_t glo_here(void) { return page == 5u || page == 6u || page == 10u || page == 14u; }
static uint32_t shift(void) { return glo_here() && (gl_latch || (fm1_in.buttons >> BTN_GLO & 1u)); }
static uint32_t dlist_at, dlist_enc = 0xFFu;    /* the knobs' list (draw_dlist): when it opened, the encoder it is on */
static int32_t enc_take(uint32_t e)             /* an encoder's detents: one turned while GLO is down makes it no tap;
                                                   another than the list's closes the list; the pace (enc_far) */
{
    int32_t v = fm1_enc_take(e);
    if (!v) return 0;
    ev_put(EV_ENC, e, v);                       /* (the freeze log) */
#if !defined(DINGHY_HOST)
    enc_pace(e, v);
#endif
    if (v && (fm1_in.buttons >> BTN_GLO & 1u)) gl_used = 1;
    if (v && dlist_at && e != dlist_enc) dlist_at = 0;
    return v;
}
static uint8_t sp_frz;                          /* SPACE's FREEZE (its FREEZE layer's K1): the player's, not the patch's */
static int8_t sp_pitch;                         /* its PITCH (K2 there), -12 .. 12: the frozen tail's; the player's */
/* LOOPER, the player's (not saved with the patch): LOOP and REVERSE (TOOLS), LENGTH (a division, synced whatever
   TIME's MODE: 2026-11-24), its speed (semitones x 256), its level */
static uint8_t lp_on, lp_rev;
static uint8_t lp_arm;                          /* armed (EDIT on LOOPER): it records, whatever L-LEVEL (2026-11-17,
                                                   the user: a loop taken at the bottom, then faded in) */
static uint8_t lp_hold;                         /* switched off, its loop kept (eng.lp_hold): EDIT (or HOME's LOOP)
                                                   brings it back; TOOLS' CLEAR clears it, asked (2026-11-21) */
static uint32_t lp_clr_at;                      /* TOOLS' CLEAR on LOOPER asked: CLEAR THE LOOP? OCT+ yes */
static void msg(const char *t);
static void lp_switch(void)                     /* EDIT on LOOPER, HOME's LOOP: on, or off with its loop kept */
{
    if (lp_arm) { lp_arm = 0; lp_hold = 1; msg("LOOPER OFF: ITS LOOP KEPT"); }
    else { lp_arm = 1; lp_hold = 0; msg(eng.lp_borrow ? "LOOPER ON: DELAY 0.5 S" : "LOOPER ON"); }
}
static void lp_clear(void)                      /* the loop let go: the memory back to the delay */
{
    lp_arm = 0; lp_hold = 0; lp_on = 0; lp_clr_at = 0;
    msg(eng.lp_borrow ? "LOOP CLEARED: DELAY 1 S" : "LOOP CLEARED");
}
static uint16_t lp_len = (uint16_t)(5u * 65536u / SD_LP_NDIV + 100u), lp_lvl = 52300u;   /* (1/8;
                                                   L-LEVEL 79: only its level now, EDIT arms it) */
static uint16_t home_filt;                      /* HOME's DARK (MORE's K4; FILTER on K4 2026-11-19): every cutoff down, 0
                                                   open (eng.filt; the player's, not the patch's: kept as patches change) */
static uint16_t lp_filt = 32768u, lp_rsend = 65535u, lp_dsend;   /* LOOPER's SEND: FILTER (none in the middle),
                                                   R-SEND (whole: as it always went into the reverb), D-SEND (none) */
static int16_t lp_speed;
static void lp_octave(uint32_t up)              /* TOOLS' OCTAVE on LOOPER: SPEED to the next octave (-12 0 +12), over
                                                   GLIDE (GLO + OCT till 2026-11-22) */
{
    int32_t v = lp_speed;
    v = up ? (v < 0 ? 0 : 3072) : (v > 0 ? 0 : -3072);
    msg(v == lp_speed ? (v < 0 ? "SPEED -12: ITS LOWEST" : "SPEED +12: ITS HIGHEST") :
        v < 0 ? "SPEED -12: AN OCTAVE DOWN" : v > 0 ? "SPEED +12: AN OCTAVE UP" : "SPEED 0: AS RECORDED");
    lp_speed = (int16_t)v;
}
static uint16_t lp_glide;                       /* GLIDE: OFF (a new SPEED at once), else how long it takes */
static const char *msg_txt;                     /* a message in the hint's place, 2 s */
static uint32_t msg_at;
static void msg(const char *t) { msg_txt = t; msg_at = fm1_ms ? fm1_ms : 1u; }

/* ---- DRUM, SEQ's fourth page (2026-11-19): the drums on the master's grid (sd_drums.h). The page edits set.dkit (the
 * sounds) and set.dpat (the beat), both kept; the engine has its own copies (dr_kit_push, dr_push). EDIT on / off (off
 * at every start; in at the master's one). Its pad picks the drum; ALGORITHM the layer (dr_knobs). GLO + a white key:
 * its step off -> ON -> ACCENT -> RADAR (it plays where RADAR's newest bit is 1) -> off. Stopped, a step set, a knob
 * turned, a drum picked: it sounds (dr_hear). PRESETS the beats D01..D16, SAVE to one; a song's step names one (or the
 * drums off) */
static uint32_t dr_cur, dr_clr_at, dr_has_undo, dr_heard_at;   /* the drum edited (its pad picks it); CLR asked;
                                                   UNDO's; the last audition */
static uint32_t dr_step, dr_rec;                /* the step picked (SELECT); REC (the pads into the steps) */
#define dr_more lay_is(PAGE_DRUM, LK_MORE)            /* (the step's ROLL, PROB) */
#define dr_mix lay_is(PAGE_DRUM, LK_MIX)              /* (the four levels) */
static uint32_t dr_roll_at, dr_back_at;         /* TOOLS: the last RANDOM, UNDO (as SEQ's) */
static uint8_t dr_mute;                         /* bit d: drum d's steps silent (its black key; not kept) */
static uint8_t dr_dirty, dr_keep_on;            /* a change to keep (the main loop saves the settings); the demo's */
static SdDrumPat dr_undo;
static SdDrumMore dr_undo_m;
static uint32_t dr_rng = 0x6D2B79F5u;
static void dr_push(uint32_t wait)              /* the beat to the engine: at once, or (wait) at the bar's one */
{
    eng.dr.pat_seq++;
    SD_BARRIER();
    eng.dr.pat_in = set.dpat;
    eng.dr.more_in = set.dmore;
    eng.dr.pat_wait = (uint8_t)wait;
    SD_BARRIER();
    eng.dr.pat_seq++;
}
static void dr_kit_push(void) { eng.dr.kit = set.dkit; eng.dr.perc = (uint8_t)(set.dperc % SD_PCS); }
static void dr_places(void)                     /* the places a song's step may name: the saved, the factory's */
{
    eng.dr.pats = udr.p;
    eng.dr.mores = udr.m;
    eng.dr.pats_has = (uint16_t)(udr.used | ((1u << DR_FACTORY) - 1u));
}
static void dr_hear(uint32_t d, uint32_t acc, uint32_t force)   /* stopped (or the drums off): drum d sounds */
{
    if (eng.seq_run && eng.dr.on) return;
    if (!force && fm1_ms - dr_heard_at < 90u) return;   /* (a knob turning: not every detent) */
    dr_heard_at = fm1_ms;
    eng.dr.req = (uint8_t)(eng.dr.req | 1u << d | (acc ? 1u << (4u + d) : 0u));
}
static void dr_keep(void) { dr_undo = set.dpat; dr_undo_m = set.dmore; dr_has_undo = 1; }
static void dr_row_plain(uint32_t d)            /* a row anew: its steps' ROLL and PROB as plain */
{
    uint32_t i;
    for (i = 0; i < SD_DR_STEPS; i++) sd_dr_more_set(&set.dmore, d, i, 0, 0);
}
static uint32_t dr_rand(void) { dr_rng = dr_rng * 1664525u + 1013904223u; return dr_rng >> 8; }
static void dr_rnd_row(uint32_t d, uint32_t keep)   /* RANDOM: a new row for the drum, as its kind plays: the kick on
                                                   the one and the three, the snare on the backbeat, the hat in eighths
                                                   and sixteenths, the clap on the four; a few more, a few accents
                                                   (keep: the beat before it for UNDO, a turn's first detent) */
{
    static const uint8_t KEEP[SD_DRS][SD_DR_STEPS] = {   /* the chance of a step, % */
        {100, 0, 10, 0, 20, 0, 25, 10, 90, 0, 35, 15, 15, 0, 20, 10}, {0, 0, 0, 5, 95, 0, 0, 10, 0, 5, 0, 10, 95, 5, 15, 25},
        {70, 25, 70, 25, 70, 25, 70, 30, 70, 25, 70, 25, 70, 30, 70, 40}, {0, 0, 0, 0, 25, 0, 0, 5, 0, 0, 0, 5, 80, 0, 10, 10}};
    uint32_t i;
    if (keep) dr_keep();
    dr_row_plain(d);
    for (i = 0; i < SD_DR_STEPS; i++) {
        uint32_t on = dr_rand() % 100u < KEEP[d][i], acc = on && (i == 0u || dr_rand() % 100u < 20u);
        sd_dr_step_set(&set.dpat, d, i, acc ? SD_DS_ACC : on ? SD_DS_ON : SD_DS_OFF);
    }
    dr_push(0);
    dr_dirty = 1;
}
static void dr_clear_row(uint32_t d)
{
    uint32_t i;
    dr_keep();
    dr_row_plain(d);
    for (i = 0; i < SD_DR_STEPS; i++) sd_dr_step_set(&set.dpat, d, i, SD_DS_OFF);
    dr_push(0);
    dr_dirty = 1;
    dr_clr_at = 0;
}
static void dr_load(uint32_t i)                 /* PRESETS: place i (a beat while they play: at the bar's one) */
{
    set.drum = (uint8_t)i;
    set.dpat = udr.p[i];
    set.dmore = udr.m[i];
    dr_has_undo = 0;
    dr_push(eng.seq_run && eng.dr.on);
    dr_dirty = 1;
}

/* ---- the voices (rules for what can play together: never more than is possible, a short message when not). The
 * engine gives each source its quota (sd_plan: a playing track its widest chord, A first, then B, then ARP one, the
 * hand the rest) and no source takes another's voice. Here what is switched on is kept inside the voices: ARP and SEQ
 * B are refused when there is no room (a message says why), and switched off when PLAY, SEL or a new chord takes
 * their room; a chord is recorded only as wide as the room left. */
static uint32_t voices_n(void) { return set.mode == SD_MODE_POLY ? 4u : set.mode == SD_MODE_DUO ? 2u : 1u; }
static uint32_t trk_need(uint32_t k)            /* the voices track k takes when it plays */
{
    if (eng.t[k].mute || (k && set.mode == SD_MODE_MONO)) return 0;
    return eng.t[k].width;
}
static uint32_t arp_room(void)                  /* ARP fits beside the tracks playing */
{
    uint32_t n = voices_n(), a = trk_need(0), b = trk_need(1);
    if (!eng.seq_run) return 1;
    a = a < n ? a : n;
    b = b < n - a ? b : n - a;
    return a + b < n;
}
static uint32_t chords_cut_was;
static void voices_guard(void)                  /* every loop: what no longer fits goes off, and says so */
{
    uint32_t n = voices_n(), na = trk_need(0), nb = trk_need(1), a, b, cut;
    if (!eng.seq_run) { chords_cut_was = 0; return; }
    a = na < n ? na : n;
    if (nb && a >= n) { eng.t[1].mute = 1; msg("SEQ B OFF: NO VOICE LEFT"); nb = 0; }
    b = nb < n - a ? nb : n - a;
    if (eng.arp_on && a + b >= n) {
        eng.arp_on = 0;
        msg(n == 1u ? "ARP OFF: SEQ PLAYS IN MONO" : "ARP OFF: NO VOICE LEFT");
    }
    cut = na > a || nb > b;
    if (cut && !chords_cut_was) msg("CHORDS CUT: TOO FEW VOICES");
    chords_cut_was = cut;
}


static const SdPattern *key_pat(void);
static char *put_note(char *p, uint32_t n);
/* The battery (the FM-1's divider on ADC 3, read once a second, smoothed): 0..1023, 0 not known (a computer, the
 * SMK-37: not drawn). The scale is a first guess (a 1/2 divider on 3.3 V: 640 = 4.13 V; another firmware draws its 3
 * bars from 591, 3.81 V): SETUP shows the volts, so a reading off can be told. 2026-11-16, the user: "a battery
 * indicator at the top" */
static uint32_t batt;
static void batt_poll(void)
{
#if !defined(DINGHY_HOST) && !BOARD_SMK37
    static uint32_t batt_at;
    int32_t a;
    if (batt_at && fm1_ms - batt_at < 1000u) return;
    batt_at = fm1_ms | 1u;
    if ((a = fm1_adc_read(FM1_ADC_BATT)) > 0) batt = batt ? (batt * 3u + (uint32_t)a + 2u) / 4u : (uint32_t)a;
#elif defined(SD_BATT_TEST)                     /* (a computer: a reading to see the icon, -DSD_BATT_TEST=600) */
    batt = SD_BATT_TEST;
#endif
}
static uint32_t batt_bars(void) { return batt >= 591u ? 3u : batt >= 560u ? 2u : batt >= 530u ? 1u : 0u; }
static uint32_t batt_mv(void) { return batt * 6600u / 1024u; }
static void batt_icon(int32_t x, int32_t y)     /* a cell with its nub, three bars (none left: red) */
{
    uint32_t n = batt_bars(), k;
    uint16_t c = n ? C_ICE_D : C_RED;
    cv_frame(x, y, 15, 9, c);
    cv_rect(x + 15, y + 3, 2, 3, c);
    for (k = 0; k < n; k++) cv_rect(x + 2 + (int32_t)k * 4, y + 2, 3, 5, n == 1u ? C_RED : C_ICE);
}
/* the CPU's warning (2026-11-21, the user: "a warning somewhere, I'd hate it to freeze and lose what I make"; B of a
   sheet, the top): from 90 % the worst interrupt of the last second, red, in the patch's place (P03) on the top line;
   at 100 % and over (the sound skipping blocks) it blinks. 0: none */
static uint32_t cpu_warn(void)
{
#ifdef SD_CPU_FAKE
    uint32_t pm = SD_CPU_FAKE;                  /* (the host's screens for a sheet: a reading pinned) */
#else
    uint32_t pm = cpu_permille;
#endif
    return pm != 0xFFFFFFFFu && pm >= 900u && !demo_st ? pm : 0u;
}
static char *put_cpu(char *p, uint32_t pm)      /* CPU 96% */
{
    p = put(p, "CPU ");
    p = put_u(p, (pm + 5u) / 10u);
    *p++ = '%';
    return p;
}
static void draw_info(void)                     /* the patch as a red tab, the key (the master's), the mode */
{
    char s[16], *p = s;
    const SdPattern *kp = key_pat();
    int32_t w = text_w(&FONT_S, patch_name(set.patch));
    cv_begin(240, 20, C_BG);
    cv_rect(0, 1, w + 22, 18, C_RED);
    cv_line(w + 22, 1, w + 30, 1, C_RED);       /* the tab's slanted end */
    for (int32_t k = 0; k < 18; k++) cv_line(w + 22, 1 + k, w + 30 - k * 8 / 18, 1 + k, C_RED);
    cv_text(10, 2, &FONT_S, patch_name(set.patch), C_BG);
    p = put_note(s, sd_pat_key(kp)); while (p > s && p[-1] >= '0' && p[-1] <= '9') p--;   /* (the root's name) */
    *p++ = ' ';
    *put(p, SCALE_NAME[kp->scale % SD_SCALES]) = 0;
    {
        char r[12], *q = r;
        uint32_t pm = cpu_warn();
        int32_t xr, xk;
        if (pm) q = put_cpu(r, pm);             /* the CPU's warning in the place's stead */
        else {                                  /* its place: P01.., U01.. */
            uint32_t n = set.patch >= SD_PATCHES ? set.patch - SD_PATCHES + 1u : set.patch + 1u;
            *q++ = set.patch >= SD_PATCHES ? 'U' : 'P';
            if (n < 10u) *q++ = '0';
            q = put_u(q, n);
        }
        *q = 0;
        xr = 232 - text_w(&FONT_S, r);
        xk = batt && !pm ? xr - 24 : xr;        /* (the battery before the place; not beside the warning) */
        if (w + 40 + text_w(&FONT_S, s) <= xk - 6) cv_text(w + 40, 2, &FONT_S, s, C_ICE_D);   /* (the key, where it fits) */
        if (!pm) cv_text(xr, 2, &FONT_S, r, C_ICE_D);
        else if (pm < 1000u || (fm1_ms / 300u) & 1u) cv_text(xr, 2, &FONT_S, r, C_RED);   /* (100 % and over: it blinks) */
        if (batt && !pm) batt_icon(xr - 24, 6);
    }
    cv_line(0, 21, 239, 21, C_GRID);
}

/* PRESETS turned: the list over the panel, the sound as it is (browsing changes nothing until EDIT): the one picked
 * in the middle as a red tab, three either side (patches past the empty user places, as the knob steps; on SEQ and
 * ORBIT the patterns), the one playing marked NOW, the place in the whole list on the right. EDIT loads it (its light
 * blinks meanwhile), HOME or 8 s untouched: not now; another button closes it. */
#define BROWSE_MS 8000u
static uint32_t browse_at, browse_to, browse_kind;   /* when PRESETS last moved (0: not browsing); the one picked;
                                                   1 a patch, 2 a pattern, 3 a song, 4 a beat */
static char *put_pnum(char *p, uint32_t i)      /* P01..P33, U01..U32 */
{
    uint32_t n = i < SD_PATCHES ? i : i - SD_PATCHES;
    *p++ = i < SD_PATCHES ? 'P' : 'U';
    if (n < 9u) *p++ = '0';
    return put_u(p, n + 1u);
}
static char *put_place(char *p, uint32_t kind, uint32_t i)   /* U01 (a patch), P01 (a pattern), S01 (a song), D01
                                                   (a beat) */
{
    *p++ = kind == 4u ? 'D' : kind == 3u ? 'S' : kind == 2u ? 'P' : 'U';
    if (i < 9u) *p++ = '0';
    return put_u(p, i + 1u);
}
static uint32_t patch_step(uint32_t n, int32_t d)
{
    do n = step_mod(n, d, NPATCHES); while (!patch_ok(n));
    return n;
}
static uint32_t browse_step(uint32_t n, int32_t d)   /* the list's next: a patch past the empty places, a pattern */
{
    return browse_kind >= 2u ? step_mod(n, d, save_places(browse_kind)) : patch_step(n, d);
}
static void draw_browse(void)                   /* over the knobs and the picture: H_LIST rows, seven names */
{
    char s[8];
    uint32_t r, n = browse_to, total = 0, at = 0, k, pt = browse_kind >= 2u;
    uint32_t now = browse_kind == 4u ? set.drum : browse_kind == 3u ? set.song : pt ? set.pattern : set.patch;
    uint32_t used = browse_kind == 4u ? udr.used : browse_kind == 3u ? usong.used : useq.used;
    int32_t y, by;
    cv_frame(2, 0, 236, H_LIST, C_RED);         /* (the frame began the canvas, a strip of it) */
    for (k = 0; k < 3u; k++) n = browse_step(n, -1);
    for (r = 0; r < 7u; r++, n = browse_step(n, 1)) {
        const char *nm = pt ? (used >> n & 1u ? "SAVED" : browse_kind == 4u && n < DR_FACTORY ? DR_FNAME[n] : "EMPTY") :
                         patch_name(n);
        y = 2 + (int32_t)r * 19;
        if (pt) *put_place(s, browse_kind, n) = 0;
        else *put_pnum(s, n) = 0;
        if (r == 3u) {
            int32_t w = 64 + text_w(&FONT_S, nm);
            for (k = 0; k < 18u; k++) cv_line(6, y + (int32_t)k, 6 + w + 8 - (int32_t)k * 8 / 18, y + (int32_t)k, C_RED);
            cv_text(14, y + 2, &FONT_S, s, C_BG);
            cv_text(60, y + 2, &FONT_S, nm, C_BG);
        } else {
            cv_text(14, y + 2, &FONT_S, s, C_ICE_D);
            cv_text(60, y + 2, &FONT_S, nm, r == 2u || r == 4u ? C_ICE : C_ICE_D);
        }
        if (n == now) cv_text(190, y + 2, &FONT_S, "NOW", r == 3u ? C_ICE : C_ICE_D);
    }
    cv_text(10, H_LIST - 20, &FONT_S, browse_kind == 4u ? "BEATS  OCT+ LOAD  OCT- NO" : browse_kind == 3u ? "SONGS  OCT+ LOAD  OCT- NO" :
            pt ? "PATTERNS  OCT+ LOAD  OCT- NO" : "PATCHES  OCT+ LOAD  OCT- NO", C_ICE_D);
    if (pt) { total = save_places(browse_kind); at = browse_to; }
    else for (k = 0; k < NPATCHES; k++) if (patch_ok(k)) { if (k == browse_to) at = total; total++; }
    cv_rect(228, 6, 3, H_LIST - 30, C_GRID);    /* where in the list */
    by = 6 + (int32_t)(at * (H_LIST - 42u) / (total > 1u ? total - 1u : 1u));
    cv_box(227, by, 5, 12, C_RED);
}

/* REACTOR's STAGES (GLO + B10, D#5, on its page): what each of the six stages holds, on the FM-1, and how many it
 * uses (ALGORITHM, 1..6). The white keys (or SELECT) pick one, and POS goes there so it is what sounds; K1 TYPE puts a vowel or a chord in it from a
 * list (as the plan's presets into a stage: 13 vowels, 13 chords on the stage's root), K2 PITCH moves it a semitone a
 * detent, K3 WIDTH spreads or closes it (F1 to F4, the steps in proportion), K4 LEVEL; GLO: F1..F4, each a semitone */
#define RST_CHORDS 18u
#define RST_N (SD_RVOWELS + RST_CHORDS)
#define RST_CHORD_LV 20000u                     /* (as the patches' chords: a narrow BP12 on a harmonic rings) */
static const char *const RST_CHORD_NAME[RST_CHORDS] = {"MAJ", "MIN", "SUS2", "SUS4", "MAJ7", "MIN7", "DOM7", "ADD9",
                                                       "MIN9", "FIFTHS", "FOURTHS", "DIM7", "AUG", "MAJ9", "MIN11",
                                                       "PHRYG", "CLUSTER", "SUS24"};   /* (2026-11-23: five more for
                                                   film: the dark PHRYG, a CLUSTER of seconds) */
static const uint8_t RST_CHORD[RST_CHORDS][4] = {{0, 4, 7, 12}, {0, 3, 7, 12}, {0, 2, 7, 12}, {0, 5, 7, 12},
    {0, 4, 7, 11}, {0, 3, 7, 10}, {0, 4, 7, 10}, {0, 4, 7, 14}, {0, 3, 10, 14}, {0, 7, 12, 19}, {0, 5, 10, 15},
    {0, 3, 6, 9}, {0, 4, 8, 12}, {0, 4, 11, 14}, {0, 3, 10, 17}, {0, 1, 7, 12}, {0, 2, 3, 5}, {0, 2, 5, 7}};
static const uint16_t RST_VLV[4] = SD_RLV_VOWEL;
static uint8_t rst_cur;                         /* the stage STAGE edits (SELECT) */
#define r_stages (page == 1u && (lay_is(1u, LR_STAGE) || lay_is(1u, LR_FREQ)))   /* (a stage's layers) */
static uint16_t rpos_at(uint32_t st)            /* POS on stage st (0..) of the ones in use: POS's turn spans them */
{
    uint32_t ns = SD_R_NST(&pat);
    return (uint16_t)((st * 1536u + ns - 1u) / ns);
}
static int32_t rst_clamp(int32_t v) { return v < SD_R_LO * 256 ? SD_R_LO * 256 : v > SD_R_HI * 256 ? SD_R_HI * 256 : v; }
static int32_t rst_match(const SdReactorStage *st)   /* the list's entry the stage is (a vowel's formants, a chord's
                                                   steps on any root), or -1: its own */
{
    uint32_t i, k, ok;
    for (i = 0; i < SD_RVOWELS; i++) {
        for (k = 0, ok = 1; k < 4u; k++) ok &= st->pitch[k] == SD_RVOWEL[i][k];
        if (ok) return (int32_t)i;
    }
    if (st->pitch[0] & 255u) return -1;
    for (i = 0; i < RST_CHORDS; i++) {
        for (k = 0, ok = 1; k < 4u; k++) ok &= st->pitch[k] == st->pitch[0] + RST_CHORD[i][k] * 256u;
        if (ok) return (int32_t)(SD_RVOWELS + i);
    }
    return -1;
}
static void rst_put(SdReactorStage *st, uint32_t n)   /* entry n into the stage: a vowel as it is, a chord on the
                                                   stage's root (C3 when it has none in range) */
{
    uint32_t k, any = 0;
    for (k = 0; k < 4u; k++) any |= st->level[k];
    if (n < SD_RVOWELS) {
        for (k = 0; k < 4u; k++) { st->pitch[k] = SD_RVOWEL[n][k]; st->level[k] = RST_VLV[k]; }
    } else {
        int32_t r = ((int32_t)st->pitch[0] + 128) >> 8;
        n -= SD_RVOWELS;
        if (!any || r < SD_R_LO || r + RST_CHORD[n][3] > SD_R_HI) r = 48;
        for (k = 0; k < 4u; k++) { st->pitch[k] = (uint16_t)rst_clamp((r + RST_CHORD[n][k]) * 256); st->level[k] = RST_CHORD_LV; }
    }
}
static void rst_pitch(SdReactorStage *st, int32_t e)   /* every filter e semitones, as far as all stay in range */
{
    int32_t lo = 0x7FFF, hi = 0, k, d = e * 256;
    for (k = 0; k < 4; k++) { if (st->pitch[k] < lo) lo = st->pitch[k]; if (st->pitch[k] > hi) hi = st->pitch[k]; }
    if (lo + d < SD_R_LO * 256) d = SD_R_LO * 256 - lo;
    if (hi + d > SD_R_HI * 256) d = SD_R_HI * 256 - hi;
    for (k = 0; k < 4; k++) st->pitch[k] = (uint16_t)(st->pitch[k] + d);
}
static void rst_width(SdReactorStage *st, int32_t e)   /* F1 to F4 a semitone wider or closer, the steps in
                                                   proportion (from none: F1 F2 F3 F4 evenly) */
{
    int32_t p0 = st->pitch[0], s = (int32_t)st->pitch[3] - p0, t = s + e * 256, k;
    if (t < 0) t = 0;
    if (p0 + t > SD_R_HI * 256) t = SD_R_HI * 256 - p0;
    for (k = 1; k < 4; k++)
        st->pitch[k] = (uint16_t)rst_clamp(s > 0 ? p0 + ((int32_t)st->pitch[k] - p0) * t / s : p0 + k * t / 3);
}
static void rst_level(SdReactorStage *st, int32_t e)   /* the stage's level, its filters' balance kept */
{
    uint32_t m = 0, k;
    int32_t t;
    for (k = 0; k < 4u; k++) if (st->level[k] > m) m = st->level[k];
    t = (int32_t)m + e * 662;
    t = t < 0 ? 0 : t > 65535 ? 65535 : t;
    for (k = 0; k < 4u; k++) st->level[k] = (uint16_t)(m ? st->level[k] * (uint32_t)t / m : (uint32_t)t);
}

/* The lists a knob turns through: DEST (MOD's, RADAR's, ORBIT's lanes), MOD's SOURCE and NAV's generator (GLO +
 * K4). While it turns, over the knobs and the picture as PRESETS' list is: the one picked in the middle, three either
 * side, each where it comes from and its name, the place in the whole list on the right. It plays as it turns
 * (nothing to load); 1.5 s untouched, another knob, a button or another page: gone */
enum { DL_MOD = 1, DL_TUR, DL_ORB1, DL_ORB2, DL_SRC, DL_GEN, DL_RST, DL_ALG, DL_BAND, DL_LFO, DL_COL, DL_PERC, DL_TUR1 };   /* (DL_BAND: no
                                                   longer opened, 2026-11-17: DISTROY's band moves its frame in the
                                                   picture; its place kept, the table below in step) */
/* NAV's algorithms (SD_ALGO's order): how many oscillators are heard, and the routing, 1..4 left to right
   (S: into the next, P: into the nearest heard one to the right, C: heard) */
static const char *const ALG_HEARD[8] = {"1 HEARD", "1 HEARD", "1 HEARD", "2 HEARD", "2 HEARD", "2 HEARD", "3 HEARD",
                                         "4 HEARD"};
static const char *const ALG_ROUTE[8] = {"1>2>3>4", "2>3>4 1>4", "1+2+3>4", "1>2 3>4", "1+2>3 4", "1 2>3>4", "1>2 3 4",
                                         "1 2 3 4"};
#define DLIST_MS 1500u
static uint32_t dlist_kind, dlist_pg;
static void dlist_open(uint32_t kind)
{
    static const uint8_t ENC[] = {0, ENC_KNOB2, ENC_KNOB2, ENC_KNOB1, ENC_KNOB3, ENC_KNOB1, ENC_KNOB4, ENC_KNOB1,
                                  ENC_KNOB1, ENC_ALGORITHM, ENC_KNOB3, ENC_KNOB4, ENC_KNOB4, ENC_KNOB2};   /* (each list's knob, in DL_*'s
                                                   order) */
    dlist_kind = kind; dlist_pg = page; dlist_at = fm1_ms ? fm1_ms : 1u;
    dlist_enc = kind < sizeof ENC ? ENC[kind] : 0xFFu;
}
/* the sources in the list's order: the envelopes, the LFOs and RADAR, NAV, the playing, then the engines in the
   chain's order; each where it is set or comes from, and its name there (in the enum's order) */
static const uint8_t SRC_ORDER[SD_MSRCS] = {SD_MS_OFF, SD_MS_ENV1, SD_MS_ENV2, SD_MS_ENV3, SD_MS_LFO1, SD_MS_LFO2,
    SD_MS_TUR, SD_MS_TUR1, SD_MS_NAVX, SD_MS_NAVY, SD_MS_VEL, SD_MS_KEY, SD_MS_WHEEL, SD_MS_AFTER, SD_MS_RAND,
    SD_MS_SEQA, SD_MS_STEP, SD_MS_SEG, SD_MS_SEQB, SD_MS_RSTAGE, SD_MS_DLOW, SD_MS_DTOP, SD_MS_ECHO, SD_MS_KICK,
    SD_MS_SNARE};
static const char *const SRC_GRP[SD_MSRCS] = {"", "ENV", "ENV", "LFO", "LFO", "NAV", "NAV", "PLAYING", "PLAYING",
    "PLAYING", "PLAYING", "PLAYING", "SEQ A", "SEQ A", "DISTROY", "DISTROY", "REACTOR", "TIME", "FILTER", "LFO", "DRUM",
    "DRUM", "SEQ A", "SEQ B", "LFO"};
static const char *const SRC_PARAM[SD_MSRCS] = {"OFF", "ENV 1", "ENV 2", "LFO 1", "LFO 2", "X", "Y", "VELOCITY",
    "KEY TRACK", "WHEEL", "AFTERTOUCH", "RANDOM EACH NOTE", "STEP", "SEGMENT", "LOW BAND", "TOP BAND", "STAGE",
    "ECHOES", "ENV 3", "RADAR", "KICK (DUCK: AMOUNT -)", "SNARE", "NOTE", "NOTE", "RADAR 1, ON OR OFF"};
static uint32_t src_rank(uint32_t s) { uint32_t r = 0; while (r < SD_MSRCS - 1u && SRC_ORDER[r] != s) r++; return r; }
static uint32_t src_step(uint32_t s, int32_t e) { return SRC_ORDER[step_mod(src_rank(s % SD_MSRCS), e, N_SRC)]; }
static uint32_t lfo_slot(uint32_t k)            /* the MOD slot LFO k (0, 1) sends from: the first going somewhere, else
                                                   the first it is in; SD_MODS: none */
{
    uint32_t i, s = k ? SD_MS_LFO2 : SD_MS_LFO1;
    for (i = 0; i < SD_MODS; i++) if (pat.mod[i].src == s && pat.mod[i].dst) return i;
    for (i = 0; i < SD_MODS; i++) if (pat.mod[i].src == s) return i;
    return SD_MODS;
}
static uint32_t mslot(void)                     /* the slot this page edits: MOD's, an LFO's (SD_MODS: none) */
{
    return dmod < SD_MODS ? dmod : dmod < DMOD_TUR ? lfo_slot(dmod - SD_MODS) : SD_MODS;
}
static const char *const PC_WHAT[SD_PCS] = {"HANDS", "SKIN", "BLIPS", "PINGS", "LASER", "REVERSED"};   /* (DRUM's
                                                   fourth, its list) */
static uint32_t dl_n(void) { return dlist_kind == DL_PERC ? SD_PCS : dlist_kind == DL_COL ? SD_ACCENTS : dlist_kind == DL_SRC ? N_SRC : dlist_kind == DL_GEN ? SD_GENS : dlist_kind == DL_RST ? RST_N : dlist_kind == DL_ALG ? SD_ALGOS : dlist_kind == DL_TUR ? N_TDST : dlist_kind == DL_TUR1 ? N_T1DST : N_DST; }
static uint32_t dl_rank(void)                   /* the one picked: its place in the list */
{
    uint32_t d;
    switch (dlist_kind) {
    case DL_SRC: return src_rank(dmod < SD_MODS ? pat.mod[dmod].src : 0u);
    case DL_GEN: return gen < SD_GENS ? gen : 0u;
    case DL_RST: { int32_t m = rst_match(&pat.stage[rst_cur % 6u]); return m < 0 ? 0u : (uint32_t)m; }
    case DL_ALG: { uint32_t a = algo_of(&pat); return a < SD_ALGOS ? a : 0u; }
    case DL_COL: return set.accent % SD_ACCENTS;
    case DL_PERC: return set.dperc % SD_PCS;
    case DL_TUR: case DL_TUR1: d = dmod < SD_MODS ? pat.mod[dmod].dst : 0u; break;   /* (a MOD slot's from RADAR or
                                                   RADAR 1: their own too) */
    case DL_ORB1: d = orbp.dst[0]; break;
    case DL_ORB2: d = orbp.dst[1]; break;
    default: d = mslot() < SD_MODS ? pat.mod[mslot()].dst : 0u;
    }
    return dlist_kind == DL_TUR ? tdst_rank(d) : dlist_kind == DL_TUR1 ? t1dst_rank(d) : dst_rank(d % SD_MDSTS);
}
static const char *dl_item(uint32_t r, char *nm)   /* the r-th in the list: where it comes from; its name into nm */
{
    char *p = nm;
    const char *g;
    if (dlist_kind == DL_PERC) { p = put(p, SD_PC_NAME[r % SD_PCS]); g = PC_WHAT[r % SD_PCS]; }
    else if (dlist_kind == DL_COL) { p = put(p, ACC_NAME[r % SD_ACCENTS]); g = r ? "" : "DEFAULT"; }   /* (the band:
                                                   the colour itself) */
    else if (dlist_kind == DL_SRC) { p = put(p, SRC_PARAM[SRC_ORDER[r]]); g = SRC_GRP[SRC_ORDER[r]]; }
    else if (dlist_kind == DL_ALG) { p = put(p, SD_ALGO[r % SD_ALGOS]); *p++ = ' '; p = put(p, ALG_ROUTE[r % 8u]); g = ALG_HEARD[r % 8u]; }
    else if (dlist_kind == DL_RST) {            /* a stage's: the vowels, the chords (their steps) */
        uint32_t k;
        if (r < SD_RVOWELS) { p = put(p, SD_RVOWEL_NAME[r]); g = "VOWEL"; }
        else {
            p = put(p, RST_CHORD_NAME[r - SD_RVOWELS]);
            for (k = 0; k < 4u; k++) { *p++ = ' '; p = put_u(p, RST_CHORD[r - SD_RVOWELS][k]); }
            g = "CHORD";
        }
    }
    else if (dlist_kind == DL_GEN) {            /* the generators: the harmonics they set, where they are fixed */
        static const char *const FIRST[4] = {"ALL  1 2 3 4", "ODD  1 3 5 7", "RND  1 .. 32", "CLUST NEIGHBOURS"};
        uint32_t k;
        if (r < 4u) { p = put(p, FIRST[r]); g = r < 2u ? "SERIES" : "RANDOM"; }
        else if (r < 4u + SD_CHORDS) {
            p = put(p, SD_CHORD_NAME[r - 4u]);
            for (k = 0; k < SD_OPS; k++) { *p++ = ' '; p = put_u(p, SD_CHORD[r - 4u][k]); }
            g = "CHORD";
        } else {
            p = put(p, "CPY ");
            *p++ = (char)('A' + ebank);
            p = put(p, " TO");
            for (k = 0; k < SD_BANKS; k++) if (k != ebank) { *p++ = ' '; *p++ = (char)('A' + k); }
            g = "BANK";
        }
    } else if (dlist_kind == DL_TUR1 && r >= N_DST) { p = put(p, TG_PARAM[(r - N_DST) % SD_TGS]); g = TG_GRP[(r - N_DST) % SD_TGS]; }
    else if (r >= N_DST && tdst_of(r) == SD_TD_NOTES) { p = put(p, "NOTES"); g = DG_NAME[DG_SEQ]; }
    else if (r >= N_DST) { p = put(p, r == N_DST ? "ACCENTS ON 1" : "ROLLS ON 1"); g = DG_NAME[DG_DRUM]; }
    else { p = put(p, DST_PARAM[SD_MD_ORDER[r]]); g = DG_NAME[DST_GRP[SD_MD_ORDER[r]]]; }
    *p = 0;
    return g;
}
static void draw_dlist(void)                    /* over the knobs and the picture: H_LIST rows, seven names */
{
    char s[24], nm[24], *p;
    uint32_t r, k, n = dl_n(), at = dl_rank();
    int32_t y, by;
    cv_frame(2, 0, 236, H_LIST, C_RED);         /* (the frame began the canvas, a strip of it) */
    for (r = 0; r < 7u; r++) {
        const char *g;
        if (n < 7u && (r + (n - 1u) / 2u < 3u || r > 3u + n / 2u)) continue;   /* (a short list: each once) */
        g = dl_item((at + 3u * n + r - 3u) % n, nm);
        y = 2 + (int32_t)r * 19;
        if (r == 3u) {
            int32_t w = 86 + text_w(&FONT_S, nm);
            for (k = 0; k < 18u; k++) cv_line(6, y + (int32_t)k, 6 + w + 8 - (int32_t)k * 8 / 18, y + (int32_t)k, C_RED);
            cv_text(14, y + 2, &FONT_S, g, C_BG);
            cv_text(92, y + 2, &FONT_S, nm, C_BG);
        } else {
            cv_text(14, y + 2, &FONT_S, g, C_ICE_D);
            cv_text(92, y + 2, &FONT_S, nm, r == 2u || r == 4u ? C_ICE : C_ICE_D);
        }
    }
    if (dlist_kind == DL_MOD || dlist_kind == DL_SRC || dlist_kind == DL_TUR || dlist_kind == DL_TUR1) p = put(put_u(put(s, "MOD "), dmod + 1u), dlist_kind == DL_SRC ? " SOURCE" : " DEST");
    else if (dlist_kind == DL_LFO) p = put(put_u(put(s, "LFO "), dmod - SD_MODS + 1u), " DEST");
    else if (dlist_kind == DL_GEN) { p = put(s, "GEN ON BANK "); *p++ = (char)('A' + ebank); }
    else if (dlist_kind == DL_RST) p = put_u(put(s, "REACTOR STAGE "), rst_cur % 6u + 1u);
    else if (dlist_kind == DL_ALG) { p = put(s, "ALGORITHM, BANK "); *p++ = (char)('A' + ebank); }
    else if (dlist_kind == DL_COL) p = put(s, "THE SCREEN'S COLOUR");
    else if (dlist_kind == DL_PERC) p = put(s, "THE 4TH DRUM'S ENGINE");   /* (s holds 23) */
    else p = put(put(s, dlist_kind == DL_ORB1 ? "ORBIT LANE 1" : "ORBIT LANE 2"), " DEST");
    *p = 0;
    cv_text(10, H_LIST - 20, &FONT_S, s, C_ICE_D);
    cv_rect(228, 6, 3, H_LIST - 30, C_GRID);    /* where in the list */
    by = 6 + (int32_t)(at * (H_LIST - 42u) / (n - 1u));
    cv_box(227, by, 5, 12, C_RED);
}






/* ---- DISTROY's page */
static uint32_t hz_of(int32_t m256)             /* MIDI x 256 -> Hz, rounded (a note's increment x fs / 2^32) */
{
    uint32_t n = (uint32_t)m256 >> 8, c = ((uint32_t)m256 & 255u) * 100u >> 8, up = n > 119u;
    uint32_t inc = (uint32_t)(((uint64_t)sd_note_inc(n - 12u * up) * SD_CENTS[99u + c]) >> 16) << up;
    return (uint32_t)(((uint64_t)inc * SD_FS + (1ull << 31)) >> 32);
}

static char *put_hz(char *p, uint32_t hz)       /* 950 / 1.2k / 12k, to the nearest (2026-11-19: 11.96 kHz read 11k) */
{
    uint32_t d = (hz + 50u) / 100u;             /* tenths of a kHz */
    if (hz < 1000u) return put_u(p, hz);
    if (d < 100u) { p = put_u(p, d / 10u); *p++ = '.'; p = put_u(p, d % 10u); }
    else p = put_u(p, (hz + 500u) / 1000u);
    *p++ = 'k';
    return p;
}

static char *put_sgn(char *p, int32_t v)
{
    *p++ = v < 0 ? '-' : '+';
    return put_u(p, (uint32_t)(v < 0 ? -v : v));
}


/* ---- SPACE's page */
static char *put_time(char *p)                  /* TIME: a division, or ms */
{
    if (pat.s_mode & SD_SP_SYNC) return put(p, DIV_NAME[sd_sp_div(pat.s_time)]);
    p = put_u(p, sd_sp_ms(pat.s_time));
    return put(p, "ms");
}
static uint32_t time_fold(uint32_t d, uint32_t t, uint32_t lim, char *played, uint32_t sync)   /* a TIME (or LENGTH:
                                                   its division d, its value t; synced, or ms) longer than its line at
                                                   this tempo: the halvings the engine takes (sd_space_sync_q8), and
                                                   what it plays (into played) */
{
    uint32_t f = 0, k;
    if (sync) {
        sd_space_sync_q8(eng.tempo_q8, d, &f, lim);
        for (k = 0; k < f; k++) d = sd_sp_half(d);
        if (played) *put(played, DIV_NAME[d]) = 0;
    } else {
        uint32_t q = sd_sp_ms(t) * 11290u;
        while (q > lim) q >>= 1, f++;
        if (played) { char *p = put_u(played, q / 11290u); *put(p, "ms") = 0; }
    }
    return f;
}
static uint32_t lp_lim(void)                    /* the looper's longest, frames x 256 (as the engine's) */
{
    uint32_t n = sd_lp_total(&eng);
    return n > SD_LP_XF + 64u ? (n - SD_LP_XF - 32u) * 512u : 512u;
}
static uint32_t dl_lim(void)                    /* the delay's longest: half its line while the looper has the rest */
{
    return eng.lp_borrow && (lp_arm || lp_hold) ? SD_SP_LIM2 : SD_SP_LIM;
}
static uint32_t div_turn(uint32_t d, int32_t e, uint32_t n, uint32_t lim)   /* a synced TIME (or LENGTH: n its
                                                   divisions) turned e detents, from the division it plays now: up
                                                   the next longer, down the next shorter, never one its line has no room
                                                   for (2026-11-17, the user: "the divisions jump"; 2026-11-23: "still
                                                   not rising, it jumps between divisions that are quite different": one
                                                   set before the looper armed, or at a faster tempo, plays halved,
                                                   sd_space_sync_q8, and a turn down from it went to longer ones, halved
                                                   too; now it starts where it plays, and up stops at the longest that
                                                   fits) */
{
    uint32_t f, k;
    sd_space_sync_q8(eng.tempo_q8, d, &f, lim);
    for (k = 0; k < f; k++) d = sd_sp_half(d);  /* (where it plays: past its line, halved) */
    for (; e > 0; e--) {
        if (d + 1u >= n) break;
        sd_space_sync_q8(eng.tempo_q8, d + 1u, &f, lim);
        if (f) { msg(lim == SD_SP_LIM2 ? "THE LONGEST THAT FITS: 0.5 S" : "THE LONGEST THAT FITS HERE"); break; }
        d++;
    }
    for (; e < 0 && d; e++) d--;
    return d;
}
static uint32_t sp_fold(char *played) { return time_fold(sd_sp_div(pat.s_time), pat.s_time, dl_lim(), played, pat.s_mode & SD_SP_SYNC); }
static uint32_t lp_fold(char *played) { return time_fold(sd_lp_div(lp_len), lp_len, lp_lim(), played, 1u); }   /* (LENGTH:
                                                   always synced) */
static char *put_glide(char *p)                 /* GLIDE: OFF, a division, or ms */
{
    uint32_t d = sd_lp_glide_div(lp_glide), ms = sd_lp_glide_ms(lp_glide);
    if (pat.s_mode & SD_SP_SYNC) return put(p, d ? DIV_NAME[d - 1u] : "OFF");
    if (!ms) return put(p, "OFF");
    p = put_u(p, ms);
    return put(p, "ms");
}
static void lp_glide_turn(int32_t e)            /* the looper's GLIDE (TOOLS, K1): OFF, then a division a detent
                                                   synced, ~16 ms free */
{
    if (pat.s_mode & SD_SP_SYNC) {
        int32_t d = (int32_t)sd_lp_glide_div(lp_glide) + e;
        d = d < 0 ? 0 : d > (int32_t)SD_LP_NGLIDE - 1 ? (int32_t)SD_LP_NGLIDE - 1 : d;
        lp_glide = (uint16_t)((uint32_t)d * 65536u / SD_LP_NGLIDE + (d ? 100u : 0u));
    } else {
        int32_t t = (int32_t)lp_glide + enc_far(e, 256u) * 256;
        lp_glide = (uint16_t)(t < 0 ? 0 : t > 65535 ? 65535 : t);
    }
}
static char *put_len(char *p)                   /* LENGTH: a division, whatever TIME's MODE (2026-11-24, the user) */
{
    return put(p, DIV_NAME[sd_lp_div(lp_len)]);
}








/* ---- the SEQ page (its own button): the pattern's steps on the white keys, its segments, recording */
static int32_t key_white(uint32_t k)            /* key k (0 = F3): its place among the 16 white keys, or -1 */
{
    uint32_t i, w = 0;
    if (black(KEY0 + k)) return -1;
    for (i = 0; i < k; i++) w += !black(KEY0 + i);
    return (int32_t)w;
}
static int32_t key_black(uint32_t k)            /* its place among the 11 black keys, or -1 */
{
    uint32_t i, n = 0;
    if (!black(KEY0 + k)) return -1;
    for (i = 0; i < k; i++) n += black(KEY0 + i);
    return (int32_t)n;
}
static char *put_note(char *p, uint32_t n)      /* A2, C#4 */
{
    static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    p = put(p, NN[n % 12u]);
    if (n < 12u) { *p++ = '-'; return put_u(p, 1u); }
    return put_u(p, n / 12u - 1u);
}
static void seq_push(void) { sd_engine_pattern_to(&eng, seq_trk, &seqp); }
static void root_own(SdPattern *p)              /* the key's root its own, from what it was (step 1's note): a step
                                                   written later no longer moves the key (2026-11-16, the user: "it says
                                                   C MAJOR, I am in A minor": a line begun on C had made it C) */
{
    if (!(p->root & SD_ROOT_SET)) p->root = (uint8_t)(SD_ROOT_SET | p->step[0].note % 12u);
}
static void seq_keep_undo(void) { seq_undo = seqp; seq_has_undo = 1; }
static void pattern_take(uint32_t i)            /* pattern i into the track SEQ edits (an empty one: the default; on A
                                                   its ORBIT too) */
{
    set.pattern = (uint8_t)i;
    if (useq.used >> i & 1u) seqp = useq.p[i];
    else if (seq_trk) sd_pattern_empty(&seqp);
    else sd_pattern_default(&seqp);
    root_own(&seqp);
    seq_cur = seq_bank = 0;
    seq_has_undo = 0;
    seq_push();
}
static SdPattern seq_other;                     /* the track SEQ does not edit now: its pattern as last edited */
/* The key IN KEY plays in is the master's (its step 1 the root, its SCALE the scale): the other track has none of its
   own there (sd_key_pat); CHROM, the player's choice (SETUP's XPOSE), frees it */
static uint32_t key_trk(void) { return set.master & 1u; }
static const SdPattern *key_pat(void) { return key_trk() == seq_trk ? &seqp : &seq_other; }
static uint32_t key_has(uint32_t n)             /* note n in the key (CHROM: every note) */
{
    const SdPattern *k = key_pat();
    return set.xchrom || (SD_SCALE_MASK[k->scale % SD_SCALES] >> ((n + 12u - sd_pat_key(k) % 12u) % 12u) & 1u);
}
static uint32_t key_none(void) { return set.xchrom ? 60u : 60u + sd_pat_key(key_pat()) % 12u; }   /* the key that
                                                   transposes by nothing: C4 (CHROM), the root in C4's octave (IN KEY) */
static void seq_track(uint32_t k)               /* SEQ edits track k */
{
    static SdPattern tp;
    if (k == seq_trk) return;
    tp = seqp;
    seqp = seq_other;
    seq_other = tp;
    seq_trk = k;
    seq_cur = seq_bank = 0;
    seq_has_undo = 0;
}
static uint32_t trk_has_notes(uint32_t k)       /* a track's pattern plays a note somewhere */
{
    const SdPattern *p = &eng.t[k].pat;
    uint32_t i;
    for (i = 0; i < p->len && i < SD_PSTEPS; i++) if (p->step[i].type <= SD_ST_SLIDE) return 1;
    return 0;
}
static uint32_t seq_next_step(uint32_t k)       /* the next step in the pattern that is not SKIP */
{
    uint32_t i;
    for (i = 1; i <= seqp.len; i++) {
        uint32_t s = (k + i) % seqp.len;
        if (seqp.step[s].type != SD_ST_SKIP) return s;
    }
    return k;
}
static const SdStep *seq_heard(const SdPattern *p, uint32_t i)   /* the step whose note is heard at step i: a TIE's
                                                   note, back round the loop's end as it plays (2026-11-17, the user:
                                                   B showed A's notes: a TIE kept the note field B's empty pattern had,
                                                   the default's, and was drawn and named by it); else step i */
{
    uint32_t n = p->len < 1u ? 1u : p->len > SD_PSTEPS ? SD_PSTEPS : p->len, k;
    i %= n;
    for (k = 0; k < n && p->step[i].type == SD_ST_TIE; k++) i = (i + n - 1u) % n;
    return &p->step[i];
}
static uint32_t rec_at = 0xFFu, rec_n;         /* REC: the step the held keys write, its notes so far */
static uint32_t rec_tie_at = 0xFFu;             /* REC, playing: the last step a held key wrote (its NOTE or a TIE) */
static uint32_t rec_joins(void)                 /* a key now joins the held one's chord: stopped, or playing while
                                                   its step still plays (later it is the next note: legato) */
{
    return rec_at != 0xFFu && (!eng.seq_run || eng.t[seq_trk].seq_shown == rec_at);
}
static uint32_t midi_held[4], midi_hn;          /* MIDI in: the notes held, how many (REC: a chord, the next step) */
static void seq_chord(uint32_t note);
static void seq_write_v(uint32_t note, uint32_t type, uint32_t vel)   /* REC: a key (at its velocity), TIE or REST
                                                   into the step */
{
    uint32_t at = eng.seq_run ? eng.t[seq_trk].seq_shown : seq_cur;
    SdStep *s = &seqp.step[at < SD_PSTEPS ? at : 0u];
    if (type == SD_ST_NOTE && seq_dub && s->type != SD_ST_REST && s->type != SD_ST_SKIP) {   /* DUB (TOOLS' K4): a
                                                   step with a note keeps it, the key joins its chord (as the voices
                                                   allow, seq_chord); its GATE stays, no TIE after it; a held note's
                                                   TIE takes nothing */
        rec_at = at < SD_PSTEPS ? at : 0u;
        rec_tie_at = 0xFEu;
        if (s->type == SD_ST_TIE) { rec_n = 4u; msg("DUB: A HELD NOTE THERE"); return; }
        rec_n = 1u + (s->chord[0] != 0) + (s->chord[1] != 0) + (s->chord[2] != 0);
        if (rec_n >= 4u) msg("DUB: THE STEP HOLDS FOUR");
        else seq_chord(note);
        return;
    }
    if (type == SD_ST_NOTE) {
        s->note = (uint8_t)note; s->vel = (uint8_t)(vel < 1u ? 1u : vel > 127u ? 127u : vel);
        s->chord[0] = s->chord[1] = s->chord[2] = 0;
        rec_at = at < SD_PSTEPS ? at : 0u;      /* keys pressed while this one is held: its chord */
        rec_tie_at = rec_at;
        rec_n = 1;
    }
    if (type != SD_ST_SLIDE && s->ratchet > SD_RATCHET_MAX) s->ratchet = 1;   /* (a SLIDE's TIME, from 17, is no
                                                   RATCHET) */
    s->type = (uint8_t)type;
    if (!eng.seq_run && type != SD_ST_NOTE) { seq_cur = seq_next_step(seq_cur); seq_bank = seq_cur / 16u; }
    seq_push();
}
static void seq_write(uint32_t note, uint32_t type) { seq_write_v(note, type, 100u); }   /* (the FM-1's keys: 100) */
static void seq_chord(uint32_t note)            /* REC: a key while another is held: a chord note on that step */
{
    SdStep *s = &seqp.step[rec_at];
    int32_t d = (int32_t)note - (int32_t)s->note;
    uint32_t n = voices_n(), other = trk_need(seq_trk ^ 1u) + (eng.arp_on ? 1u : 0u);
    if (rec_n >= 4u || !d || d < -127 || d > 127) return;
    if (rec_n + 1u + other > n) {               /* wider than the voices left for it */
        msg(n == 1u ? "MONO: ONE NOTE A STEP" : "CHORD FULL: NO VOICE LEFT");
        return;
    }
    s->chord[rec_n - 1u] = (int8_t)d;
    rec_n++;
    seq_push();
}
static uint32_t rec_down_ms, rec_ties;           /* REC, playing: when the key went down; the TIEs it wrote since */
static void seq_keys_up(void)                   /* REC, step recording: every key up, the next step */
{
    if (rec_at == 0xFFu) return;
    if (eng.seq_run && rec_at < SD_PSTEPS && rec_tie_at != 0xFEu) {   /* playing: its last step's GATE as long as the
                                                   key was held (2026-11-22, the user: "record the gate's length in
                                                   REC"); held past it: 100, on to the next note */
        uint32_t last = rec_tie_at < SD_PSTEPS ? rec_tie_at : rec_at, len = eng.t[seq_trk].seq_step_q8 / 100u + 1u;
        uint32_t ms = fm1_ms - rec_down_ms, d = (ms > 60000u ? 60000u : ms) * 11290u, done = rec_ties * len * 100u;
        uint32_t g = d > done ? (d - done) / len : 1u;   /* (samples x 256 a ms: 11290) */
        seqp.step[last].gate = (uint8_t)(g < 1u ? 1u : g > SD_GATE_MAX ? SD_GATE_MAX : g);
        seq_push();
    }
    if (!eng.seq_run) { seq_cur = seq_next_step(rec_at); seq_bank = seq_cur / 16u; }
    rec_at = 0xFFu;
    rec_n = 0;
}
/* REC, playing: a key goes to the step nearest where the player heard it (rec_ahead back from the engine's place):
 * the one playing, or the next when it was past the middle (a little early for the next step lands on it, not a
 * whole step before). The next step's notes wait till it begins, then are written: it sounds now (the key), and the
 * step does not play it a second time this round */
static uint8_t rec_pend[4], rec_pend_n, rec_pend_v, rec_pend_from, rec_pend_trk;
static void rec_pend_take(void)                 /* each pass: the next step begun, its notes written */
{
    uint32_t i, n = rec_pend_n;
    if (!n) return;
    if (!seq_rec || !eng.seq_run || seq_trk != rec_pend_trk) { rec_pend_n = 0; return; }
    if (eng.t[seq_trk].seq_shown == rec_pend_from) return;
    rec_pend_n = 0;
    seq_write_v(rec_pend[0], SD_ST_NOTE, rec_pend_v);
    for (i = 1; i < n; i++) seq_chord(rec_pend[i]);
    if (!fm1_in.notes && !midi_hn) seq_keys_up();   /* (let go already: the next key is a note of its own) */
}
static void rec_live(uint32_t note, uint32_t vel, uint32_t others)   /* REC: a key (MIDI's too) into the steps;
                                                   others: keys held with it (a chord) */
{
    const SdSeqT *t = &eng.t[seq_trk];
    if (!others) { rec_down_ms = fm1_ms; rec_ties = 0; }   /* (how long it is held: its GATE) */
    rec_pend_take();
    if (eng.seq_run && (rec_pend_n || t->seq_pos_q8 >= t->seq_step_q8 / 2u + rec_ahead() * 256u)) {
        if (rec_pend_n && others) {             /* (the next step's chord) */
            if (rec_pend_n < 4u) rec_pend[rec_pend_n++] = (uint8_t)note;
        } else {
            rec_pend[0] = (uint8_t)note; rec_pend_n = 1;
            rec_pend_v = (uint8_t)vel;
            rec_pend_from = t->seq_shown;
            rec_pend_trk = (uint8_t)seq_trk;
        }
        return;
    }
    if (rec_joins() && others) seq_chord(note);
    else seq_write_v(note, SD_ST_NOTE, vel);
}




/* KEEP (REC on ARP): what ARP just played, the last bars at its RATE (sd_arp_keep_len: 1/16 a bar of 16), as a SEQ
 * track's pattern at that DIV, the bar that ended if REC comes on the one. Into SEQ A, or SEQ B while A plays (DUO,
 * POLY: MONO has no ARP over A); the track on, the steps as heard (sd_seq_rec_note), its gates ARP's GATE. UNDO on SEQ
 * takes it back; SAVE there keeps it */
static void arp_keep(void)
{
    static char m[28];
    uint8_t nt[SD_PSTEPS], vl[SD_PSTEPS];
    uint32_t d = set.adiv % SD_SDIVS, n = sd_arp_keep_len(d), t, i;
    char *p;
    if (!sd_arp_keep(&eng, n, nt, vl)) { msg("ARP: NOTHING TO KEEP YET"); return; }
    t = eng.seq_run && !eng.t[0].mute && trk_has_notes(0) && set.mode != SD_MODE_MONO ? 1u : 0u;
    seq_track(t);
    seq_last = t;                               /* (SEQ shows it next) */
    seq_keep_undo();
    {
        uint8_t sc = seqp.scale, sw = seqp.swing, rt = (uint8_t)(SD_ROOT_SET | sd_pat_key(&seqp) % 12u);
        sd_pattern_default(&seqp);
        seqp.scale = sc; seqp.swing = sw; seqp.root = rt;   /* (the key as it was) */
    }
    seqp.len = (uint8_t)n; seqp.div = (uint8_t)d; seqp.seg_count = 1;
    for (i = 0; i < n; i++) {
        SdStep *s = &seqp.step[i];
        s->gate = set.agate; s->ratchet = 1; s->prob = 100;
        s->chord[0] = s->chord[1] = s->chord[2] = 0;
        if (nt[i]) { s->type = SD_ST_NOTE; s->note = (uint8_t)sd_seq_rec_note(&eng, &eng.t[t], nt[i]); s->vel = vl[i]; }
        else s->type = SD_ST_REST;
    }
    seq_cur = seq_bank = 0;
    seq_push();
    eng.t[t].mute = 0;
    p = put(m, "KEPT IN SEQ "); *p++ = (char)('A' + t); p = put(p, ": "); p = put_u(p, n); p = put(p, " STEPS"); *p = 0;
    msg(m);
}

static uint32_t seq_page(void) { return page == PAGE_SEQ; }   /* SEQ A, B */
static void seq_clear(void)                     /* CLR: every step a rest (UNDO takes it back) */
{
    uint32_t k;
    seq_keep_undo();
    for (k = 0; k < SD_PSTEPS; k++) {
        seqp.step[k].type = SD_ST_REST;
        seqp.step[k].chord[0] = seqp.step[k].chord[1] = seqp.step[k].chord[2] = 0;
    }
    seq_push();
    seq_clr_at = 0;
}

/* SEQ stopped, REC off: the step picked sounds (the user: "I must hear the notes I edit"): a white key, SELECT or
 * ALGORITHM to a step, its NOTE turned. The note heard there (a TIE: the note it holds, sd_note_at), its chord, as the
 * track plays it (sd_seq_xn), at its velocity; while the key is held, else AUD_MS. On the keys' voices */
#define AUD_MS 300u
static uint8_t aud_n[4], aud_c, aud_key = 0xFFu;   /* the notes sounding, how many, the key holding them */
static uint32_t aud_off;                        /* when they stop (0: while aud_key is held) */
static void aud_stop(void)
{
    uint32_t i;
    for (i = 0; i < aud_c; i++) sd_engine_note(&eng, aud_n[i], 0);
    aud_c = 0;
    aud_key = 0xFFu;
    aud_off = 0;
}
static void aud_step(uint32_t s, uint32_t key)  /* key: the key that holds it, or 0xFF (AUD_MS) */
{
    int32_t s0;
    const SdStep *st;
    uint32_t i, v;
    aud_stop();
    if (eng.seq_run || seq_rec || (s0 = sd_note_at(&seqp, s)) < 0) return;
    st = &seqp.step[s0];
    v = st->vel < 1u ? 1u : st->vel > 127u ? 127u : st->vel;
    aud_n[aud_c++] = (uint8_t)sd_seq_xn(&eng, &eng.t[seq_trk], st->note);
    for (i = 0; i < 3u && aud_c < voices_n(); i++)
        if (st->chord[i]) {
            int32_t c = (int32_t)st->note + st->chord[i];
            if (c >= 0 && c <= 127) aud_n[aud_c++] = (uint8_t)sd_seq_xn(&eng, &eng.t[seq_trk], (uint32_t)c);
        }
    for (i = 0; i < aud_c; i++) sd_engine_note(&eng, aud_n[i], v);
    aud_key = (uint8_t)key;
    aud_off = key == 0xFFu ? (fm1_ms + AUD_MS) | 1u : 0u;
}

/* SETUP's CLEAR ALL (ALGORITHM, then OCT+): a start from scratch. SEQ A and B every step a rest (each its LENGTH,
 * DIV, segments as new; the key and the scale kept), ORBIT off and empty, the drums off and their beat empty (their
 * sounds kept), the transposes back to 0 at the one; the
 * movement (2026-11-17, the user: "MOD destinations, RADAR and so on" stayed): the eight MOD slots empty (the LFOs'
 * routes with them), RADAR off as new, the macros at 0 (the patch as made), COURSE off; the player's tools: ARP off,
 * the looper's LOOP and REVERSE off, disarmed, its SPEED 0, FREEZE off. The sound itself (the banks, the envelopes,
 * the filter, the FX's settings) stays. Saved patterns and patches stay as saved (SAVE writes these) */
static uint32_t clr_all_at;                     /* asked: when (3 s to answer) */
static uint32_t setup_ask;                      /* what SETUP's TOOLS asks: 2 DEMO 1, 3 DEMO 2, 1 CLEAR ALL */
static uint32_t fx_on(uint32_t bit);
static void lay_to(uint32_t l)                  /* ALGORITHM: the page's layer l (a list open, a question asked: gone) */
{
    uint32_t was = pg_lay[page & 15u], st0 = r_stages;
    pg_lay[page & 15u] = (uint8_t)l;
    if (l == was) return;
    dlist_at = 0;
    lp_clr_at = 0; clr_all_at = 0; dr_clr_at = 0;
    if (r_stages && !st0) {                     /* REACTOR, a stage's layer: the stage POS is on, the one heard (POS
                                                   itself untouched: a layer passed on ALGORITHM's way changes nothing;
                                                   SELECT then takes POS to the stage it picks) */
        uint32_t ns = SD_R_NST(&pat), at = ((uint32_t)pat.r_pos * ns / 6u + 128u) >> 8;
        rst_cur = (uint8_t)(at < ns ? at : ns - 1u);
        if (!fx_on(SD_FX_REACTOR)) msg("REACTOR OFF: EDIT TURNS IT ON");
    }
}
static void clear_all(void)
{
    uint32_t k;
    for (k = 0; k < 2u; k++) {
        SdPattern *p = k == seq_trk ? &seqp : &seq_other;
        uint8_t sc = p->scale, root = (uint8_t)(SD_ROOT_SET | sd_pat_key(p) % 12u);
        sd_pattern_empty(p);
        p->scale = sc;
        p->root = root;
        sd_engine_pattern_to(&eng, k, p);
        eng.t[k].xpose_next = 0;
    }
    { uint8_t *z = (uint8_t *)&orbp; for (k = 0; k < sizeof orbp; k++) z[k] = 0; }
    orb_cur = 0;
    orb_push();
    seq_cur = seq_bank = 0;
    seq_has_undo = 0;
    seq_layer = SL_STEP;
    seq_clr_at = 0;
    for (k = 0; k < SD_MODS; k++) { pat.mod[k].src = pat.mod[k].dst = 0; pat.mod[k].amt = 0; pat.mod_off[k] = 0; }
    pat.tur_set = 0; pat.tur_ld = 0; pat.tur_dst = 0; pat.tur_amt = 0; pat.tur_off = 0;
    for (k = 0; k < 4u; k++) pat.mac[k] = 0;
    home_filt = 0; eng.filt = 0;
    pat.course = 0;
    sd_engine_patch(&eng, &pat);
    eng.arp_on = 0;
    lp_on = 0; lp_rev = 0; lp_arm = 0; lp_hold = 0; lp_clr_at = 0; lp_speed = 0; lp_filt = 32768u; lp_rsend = 65535u; lp_dsend = 0;
    sp_frz = 0; sp_pitch = 0;
    for (k = 0; k < SD_DRS * SD_DR_STEPS; k++) sd_dr_step_set(&set.dpat, k / SD_DR_STEPS, k % SD_DR_STEPS, SD_DS_OFF);
    for (k = 0; k < SD_DRS; k++) dr_row_plain(k);
    dr_mute = 0; eng.dr.mute = 0;
    dr_push(0);                                 /* (the drums: their beat empty, off; their sounds kept) */
    eng.dr.on = 0;
    dr_has_undo = 0;
    dr_dirty = 1;
    clr_all_at = 0;
    msg("ALL CLEAR, THE SOUND KEPT");
}

static void dr_perc_set(uint32_t pc);
/* SELECT: which one, where a page has them in a row (the user: "where something is chosen horizontally: steps,
 * segments"): SEQ's step (SEGMENTS: the part), ORBIT's step, REACTOR's stage in STAGES, DISTROY's band, MOD's slot,
 * SAVE's place. Elsewhere nothing (the tempo is SETUP's K1). Returns 1 when the patch changed */
static uint32_t select_turn(int32_t e)
{
    if (save_mode) {
        save_to = step_mod(save_to, e, save_places(save_mode));
        save_at = fm1_ms;
        return 0;
    }
    switch (page) {
    case PAGE_SEQ:
        if (shift()) {                          /* GLO + SELECT: the key's ROOT (as GLO + ALGORITHM its scale): IN KEY
                                                   the master's only, set on its page; the notes as written */
            if (!set.xchrom && key_trk() != seq_trk) { msg(key_trk() ? "THE KEY IS SEQ B'S" : "THE KEY IS SEQ A'S"); return 0; }
            seqp.root = (uint8_t)(SD_ROOT_SET | step_mod(sd_pat_key(&seqp) % 12u, e, 12u));
            seq_push();
            return 0;
        }
        if (seq_segmode) {
            uint8_t bd[SD_SEGS + 1u];
            uint32_t sc = sd_seg_bounds(&seqp, bd);
            seq_dseg = step_lim(seq_dseg < sc ? seq_dseg : sc - 1u, e, sc);
            seq_cur = bd[seq_dseg];
            seq_bank = seq_cur / 16u;
        } else {
            seq_cur = step_mod(seq_cur, e, seqp.len);
            seq_bank = seq_cur / 16u;
            aud_step(seq_cur, 0xFFu);           /* (stopped: heard) */
        }
        return 0;
    case PAGE_ORB:
        orb_cur = step_lim(orb_cur, e, SD_ORB_LEN(&orbp));
        if (!orb_setup) orb_hold_step(orb_cur);   /* (the step picked: heard) */
        return 0;
    case PAGE_DRUM:                             /* the step; GLO: the drum, round, as its pad picks it but without
                                                   playing it (so REC need not write a hit; ALGORITHM's till 2026-11-22) */
        if (shift()) { dr_cur = step_mod(dr_cur % SD_DRS, e, SD_DRS); dr_hear(dr_cur, 0, 1); return 0; }
        dr_step = step_lim(dr_step, e, SD_DR_STEPS);
        return 0;
    case 1:
        if (!r_stages) return 0;
        rst_cur = (uint8_t)step_mod(rst_cur % SD_R_NST(&pat), e, SD_R_NST(&pat));   /* (the stages in use) */
        pat.r_pos = rpos_at(rst_cur);
        dlist_at = 0;
        return 1;
    case 2: dband = step_mod(dband, e, 4u); dlist_at = 0; return 0;   /* (the band's frame moves: no list) */
    case 0:                                     /* NAV: the bank, A B C D (GLO + ALGORITHM till 2026-11-22) */
        ebank = step_mod(ebank, e, SD_BANKS);
        gen = 0xFFFFFFFFu;
        dlist_at = 0;
        return 0;
    case PAGE_SETUP:                            /* the freeze log: the newest first, back to the settings at 0 */
        if (!frz_n) { msg("NO FREEZE IN THE LOG"); return 0; }
        setup_log = step_lim(setup_log, e, frz_n + 1u);
        return 0;
    case 4: if (dmod < SD_MODS) { dmod = step_mod(dmod, e, SD_MODS); dslot = dmod; } return 0;
    default: return 0;
    }
}

static void rec_toggle(void)                    /* REC: recording into the track SEQ edits, on or off (UNDO: the pattern
                                                   before the take) */
{
    seq_rec = !seq_rec;
    rec_at = 0xFFu;
    rec_pend_n = 0;
    if (seq_rec) seq_keep_undo();
    if (seq_rec && seq_trk && set.mode == SD_MODE_MONO) msg("B RECORDS, PLAYS IN DUO/POLY");
}
static void seq_roll(void)                      /* RANDOM: a new line in the scale, now; IN KEY on the track that is not
                                                   the master in the master's key, root and scale, as it plays there
                                                   (2026-11-19, the user: B did not follow A's: it took its own, never
                                                   set) */
{
    if (!set.xchrom && key_trk() != seq_trk) {
        const SdPattern *kp = key_pat();
        seqp.root = (uint8_t)(SD_ROOT_SET | sd_pat_key(kp) % 12u);
        seqp.scale = kp->scale;
    }
    sd_pattern_randomize(&seqp, &seq_rng);
    seq_push();
}
static void seq_back(void)                      /* UNDO: the pattern before the last rewrite (and back) */
{
    SdPattern t = seqp;
    seqp = seq_undo;
    seq_undo = t;
    seq_push();
}

static void dr_rec_put(uint32_t d, uint32_t s, uint32_t acc)   /* REC: a pad's hit into step s, on top */
{
    uint32_t st = SD_DR_STEP(&set.dpat, d, s);
    sd_dr_step_set(&set.dpat, d, s, acc || st == SD_DS_ACC ? SD_DS_ACC : SD_DS_ON);
    dr_push(0);
    dr_dirty = 1;
}
static uint8_t dr_pend_m, dr_pend_a, dr_pend_to;   /* REC: pads for the next step (the drums, accented), which */
static void dr_pend_take(void)                  /* each pass: the next step begun (or REC, PLAY gone), its pads written */
{
    uint32_t d;
    if (!dr_pend_m) return;
    if (dr_rec && eng.seq_run && eng.dr.on && eng.dr.shown != dr_pend_to && eng.dr.shown < SD_DR_STEPS &&
        (eng.dr.shown + 1u) % SD_DR_STEPS == dr_pend_to) return;   /* (still the step before it) */
    if (dr_rec && eng.seq_run && eng.dr.on)
        for (d = 0; d < SD_DRS; d++) if (dr_pend_m >> d & 1u) dr_rec_put(d, dr_pend_to, dr_pend_a >> d & 1u);
    dr_pend_m = dr_pend_a = 0;
}
static void dr_rec_hit(uint32_t d, uint32_t acc)   /* REC: a pad into the steps while they play, on the nearest step
                                                   as it was heard (rec_ahead back from the master's grid: the drums'
                                                   own place is a block further on, on the second core); one just
                                                   ahead waits till the drums begin it (dr_pend_take: it sounds now,
                                                   and not twice), on top */
{
    uint32_t g = eng.grid_q8, lat = (rec_ahead() - SD_SUB) * 256u, span = eng.seq_len_q8 * 48u, s;   /* (the grid:
                                                   its last sub-block's start) */
    if (!dr_rec || !eng.seq_run || !eng.dr.on || !span || eng.dr.shown >= SD_DR_STEPS) return;
    dr_pend_take();
    g = g % span;
    g = g >= lat % span ? g - lat % span : g + span - lat % span;
    s = sd_dr_at(&eng, g);
    if (sd_dr_frac(&eng, g) < 128u) { dr_rec_put(d, s, acc); return; }   /* (begun already: heard next round) */
    s = (s + 1u) % SD_DR_STEPS;
    if (eng.dr.shown == s) { dr_rec_put(d, s, acc); return; }   /* (the drums began it already, a block ahead) */
    dr_pend_to = (uint8_t)s;
    dr_pend_m = (uint8_t)(dr_pend_m | 1u << d);
    if (acc) dr_pend_a = (uint8_t)(dr_pend_a | 1u << d);
}
static void dr_pads(uint32_t down)              /* DRUM, no GLO: F3 G3 A3 B3 the four drums, C4 D4 E4 F4 accented; F#4 G#4
                                                   A#4 C#5 MUTE them; nothing plays the synth here */
{
    uint32_t k;
    for (k = 0; k < 27u; k++) {
        int32_t w, bk;
        if (!(down >> k & 1u)) continue;
        w = key_white(k);
        bk = key_black(k);
        if (w >= 0 && w < 8) {
            uint32_t d = (uint32_t)w & 3u, acc = w >= 4;
            eng.dr.req = (uint8_t)(eng.dr.req | 1u << d | (acc ? 1u << (4u + d) : 0u));   /* (heard now, played or not) */
            dr_cur = d;
            dr_rec_hit(d, acc);
        } else if (bk >= 5 && bk <= 8) {
            dr_mute ^= (uint8_t)(1u << (bk - 5));
            eng.dr.mute = dr_mute;
        }
    }
}
static void dr_keys(uint32_t down)              /* DRUM, GLO: the white keys the drum's steps (its row's RANDOM UNDO
                                                   CLEAR: TOOLS, since 2026-11-22; B1 B2 B3 with GLO before) */
{
    uint32_t k;
    for (k = 0; k < 27u; k++) {
        int32_t w;
        if (!(down >> k & 1u)) continue;
        w = key_white(k);
        if (w >= 0 && (uint32_t)w < SD_DR_STEPS) {   /* the step: off, ON, ACCENT, RADAR, off */
            uint32_t st = (SD_DR_STEP(&set.dpat, dr_cur, (uint32_t)w) + 1u) & 3u;
            sd_dr_step_set(&set.dpat, dr_cur, (uint32_t)w, st);
            dr_push(0);
            dr_dirty = 1;
            if (st == SD_DS_RADAR && !SD_TUR_ON(&pat)) msg("RADAR OFF: ITS STEPS REST (MOD)");
            if (st) dr_hear(dr_cur, st == SD_DS_ACC, 1);
        }
    }
}
static void dr_back(void)                       /* UNDO: the beat before the last RANDOM, CLEAR, REC (and back) */
{
    SdDrumPat t = set.dpat;
    SdDrumMore tm = set.dmore;
    set.dpat = dr_undo; set.dmore = dr_undo_m;
    dr_undo = t; dr_undo_m = tm;
    dr_push(0);
    dr_dirty = 1;
}

static void seq_len(int32_t e)                  /* LENGTH: the segments scale with it */
{
    uint32_t old = seqp.len;
    int32_t l = (int32_t)seqp.len + e;
    seqp.len = (uint8_t)(l < 1 ? 1 : l > (int32_t)SD_PSTEPS ? (int32_t)SD_PSTEPS : l);
    sd_seg_rescale(&seqp, old);
    if (seq_cur >= seqp.len) seq_cur = seqp.len - 1u;
    if (seq_bank * 16u >= seqp.len) seq_bank = (seqp.len - 1u) / 16u;
}

/* The encoders on ORBIT, the song (K1..K4; ALGORITHM the layer, SELECT the step), on the step picked: SONG SEQ A, SEQ B
 * (the pattern there: as it plays, or a saved one), DRUMS (their beat there, a place D01..D16, or OFF; as they play),
 * LOOPS; LANE XPOS A, XPOS B, the lanes; MACRO the macros; the song's own: SETUP LENGTH, END, DEST 1, DEST 2; GLIDE */
static uint32_t orb_pat_step(uint32_t v, int32_t e)   /* a step's pattern: 0 (as it plays), then the saved ones */
{
    for (; e > 0; e--) {
        uint32_t n = v + 1u;
        while (n <= UPATTERNS && !(useq.used >> (n - 1u) & 1u)) n++;
        if (n <= UPATTERNS) v = n;
    }
    for (; e < 0 && v; e++) {
        do v--; while (v && !(useq.used >> (v - 1u) & 1u));
    }
    return v;
}
static uint32_t orb_dr_step(uint32_t v, int32_t e)   /* a step's beat: 0 (as they play), the places that hold one,
                                                   OFF */
{
    uint32_t has = eng.dr.pats_has;
    for (; e > 0; e--) {
        uint32_t n = v + 1u;
        while (n <= UDRUMS && !(has >> (n - 1u) & 1u)) n++;
        if (n <= SD_DR_SONG_OFF) v = n;
    }
    for (; e < 0 && v; e++) {
        do v--; while (v && v <= UDRUMS && !(has >> (v - 1u) & 1u));
    }
    return v;
}
static void orb_knobs(void)
{
    static const uint8_t KN[4] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
    uint32_t s = orb_cur % SD_ORB_STEPS, k, changed = 0, l = pg_lay[PAGE_ORB];
    int32_t e;
    for (k = 0; k < 4u; k++) {
        if ((e = enc_take(KN[k])) == 0) continue;
        changed = 1;
        if (l == LO_SETUP) {                    /* LENGTH, END, GLIDE (the lanes that glide: JUMP, LANE 1, LANE 2,
                                                   BOTH), MACROS (theirs: a turn right GLIDE, left JUMP) */
            if (k == 0u) orbp.len = (uint8_t)(1u + step_lim(SD_ORB_LEN(&orbp) - 1u, e, SD_ORB_STEPS));
            else if (k == 1u) orbp.end = (uint8_t)step_lim(orbp.end % SD_OES, e, SD_OES);
            else if (k == 2u) orbp.glide = (uint8_t)((orbp.glide & ~3u) | step_lim(orbp.glide & 3u, e, 4u));
            else orbp.glide = (uint8_t)(e > 0 ? orbp.glide | SD_OG_MAC : orbp.glide & ~SD_OG_MAC);
            if (orb_cur >= SD_ORB_LEN(&orbp)) orb_cur = SD_ORB_LEN(&orbp) - 1u;
        } else if (l == LO_MACRO) {             /* the step's macros: -99..99 */
            int32_t v = orbp.mac[k][s] + enc_far(e, 198u);
            orb_hold_step(s);                   /* (the step edited: heard) */
            orbp.mac[k][s] = (int8_t)(v < -99 ? -99 : v > 99 ? 99 : v);
        } else if (l == LO_PITCH && k < 2u) {   /* XPOS A, B: -24..24 */
            int8_t *x = k ? &orbp.xb[s] : &orbp.xa[s];
            int32_t v = *x + e;
            orb_hold_step(s);
            *x = (int8_t)(v < -24 ? -24 : v > 24 ? 24 : v);
        } else if (l == LO_PITCH) {             /* LOOP, FREEZE: the loop's and the frozen tail's pitch, -12..12 (the
                                                   user, 2026-11-22: "I miss pitch on the loop and FREEZE in ORBIT") */
            orb_hold_step(s);
            sd_orb_tail_set(&orbp, k - 2u, s, sd_orb_tail(&orbp, k - 2u, s) + e);
        } else if (l == LO_LANE && !(k & 1u)) { /* DEST 1, DEST 2: where each lane goes (its list) */
            orbp.dst[k >> 1] = (uint8_t)dst_step(orbp.dst[k >> 1], e, 0);
            dlist_open(k ? DL_ORB2 : DL_ORB1);
        } else if (l == LO_LANE) {              /* LANE 1, LANE 2: -100..100 % at the step (its DEST first) */
            if (!orbp.dst[k >> 1]) { msg(k >> 1 ? "K3 FIRST: WHERE IT GOES" : "K1 FIRST: WHERE IT GOES"); changed = 0; continue; }
            {
                int32_t v = orbp.lane[k >> 1][s] + enc_far(e, 200u);
                dlist_at = 0;
                orb_hold_step(s);
                orbp.lane[k >> 1][s] = (int8_t)(v < -100 ? -100 : v > 100 ? 100 : v);
            }
        } else if (k < 2u) {                    /* SEQ A, SEQ B: the pattern there (at the master's one) */
            orb_hold_step(s);
            if (!useq.used) msg("SAVE A PATTERN ON SEQ FIRST");
            orbp.pat[k][s] = (uint8_t)orb_pat_step(orbp.pat[k][s], e);
        } else if (k == 2u) {                   /* DRUMS: their beat there (at the bar's one), or off */
            if (!SD_DRUMS) { changed = 0; continue; }   /* (no drums: no knob) */
            orb_hold_step(s);
            orbp.drm[s] = (uint8_t)orb_dr_step(orbp.drm[s], e);
        } else {                                /* LOOPS: 1..8 of the master */
            orb_hold_step(s);
            orbp.loops[s] = (uint8_t)(1u + step_lim(SD_ORB_LOOPS(&orbp, s) - 1u, e, SD_ORB_LOOPS_MAX));
        }
    }
    if (changed) orb_push();
}

/* The encoders on DRUM (K1..K4; ALGORITHM the layer, SELECT the step: select_turn): DRUM the drum's first four (its
 * pad picks it; K4 its macro), TONE its second four; MORE the step's ROLL and PROB; MIX the four's levels; KIT the
 * drum's PAN, the kit's DUCK and ACCENT, the fourth's ENGINE; TOOLS its row's RANDOM UNDO CLEAR. A value moves in its
 * own steps (a semitone, an envelope's time, a percent) and stays in its range (sd_drumkit.h) */
static void dr_perc_set(uint32_t pc)            /* the fourth's ENGINE: its values as chosen, its level and pan kept */
{
    uint32_t k;
    set.dperc = (uint8_t)(pc % SD_PCS);
    for (k = 0; k < 8u; k++) set.dkit.d[SD_DR_CLAP].v[k] = SD_PC_DEF[set.dperc][k];
    dr_kit_push();
    dr_dirty = 1;
    dr_cur = SD_DR_CLAP;
    dr_hear(SD_DR_CLAP, 0, 1);
}
static void dr_knobs(void)
{
    static const uint8_t KN[4] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
    uint32_t k, d = dr_cur % SD_DRS, pc = set.dperc % SD_PCS, l = pg_lay[PAGE_DRUM];
    int32_t e;
    for (k = 0; k < 4u; k++) {
        if ((e = enc_take(KN[k])) == 0) continue;
        if (l == LK_TOOLS) {                    /* RANDOM (a new row each detent; a turn's first keeps the beat before
                                                   for UNDO), UNDO (one a turn), CLEAR (asked: OCT+ yes) */
            if (k == 0u) {
                dr_rnd_row(d, !dr_roll_at || fm1_ms - dr_roll_at > 1000u);
                dr_roll_at = fm1_ms ? fm1_ms : 1u;
            } else if (k == 1u) {
                if (!dr_has_undo) msg("NOTHING TO UNDO");
                else if (!dr_back_at || fm1_ms - dr_back_at > 600u) dr_back();
                dr_back_at = fm1_ms ? fm1_ms : 1u;
            } else if (k == 2u) dr_clr_at = fm1_ms ? fm1_ms : 1u;
            continue;
        }
        if (l == LK_MORE) {                     /* MORE: the step's ROLL x1..x4, PROB 100 75 50 25 % */
            uint32_t s = dr_step % SD_DR_STEPS, r = SD_DR_ROLL(&set.dmore, d, s), pb = SD_DR_PROB(&set.dmore, d, s);
            if (k == 0u) r = step_lim(r, e, 4u);
            else if (k == 1u) pb = 3u - step_lim(3u - pb, e, 4u);
            else continue;
            sd_dr_more_set(&set.dmore, d, s, r, pb);
            dr_push(0);
            dr_dirty = 1;
            continue;
        }
        if (l == LK_MIX) {                      /* MIX: the four's levels, 0..100 */
            int32_t v = (int32_t)set.dkit.d[k].level + enc_far(e, 100u);
            set.dkit.d[k].level = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
            dr_hear(k, 0, 0);
        } else if (l == LK_KIT) {               /* KIT: the drum's PAN -60..60 (never hard left or right); DUCK, the
                                                   synth under the kick, and the ACCENT, 0..100 %; the fourth's ENGINE
                                                   (its list; GLO + SELECT till 2026-11-22) */
            if (k == 0u) {
                int32_t v = set.dkit.d[d].pan + e * 4;
                set.dkit.d[d].pan = (int8_t)(v < -60 ? -60 : v > 60 ? 60 : v);
                dr_hear(d, 0, 0);
            } else if (k == 1u) {
                int32_t a = (int32_t)set.dkit.duck + e * 5;
                set.dkit.duck = (uint8_t)(a < 0 ? 0 : a > 100 ? 100 : a);
            } else if (k == 2u) {
                int32_t a = (int32_t)set.dkit.accent + e * 5;
                set.dkit.accent = (uint8_t)(a < 0 ? 0 : a > 100 ? 100 : a);
                dr_hear(d, 1, 0);
            } else {
                dr_perc_set(step_mod(set.dperc % SD_PCS, e, SD_PCS));
                dlist_open(DL_PERC);
                continue;
            }
        } else {                                /* SOUND, TONE: the drum's eight */
            uint32_t i = k + (l == LK_TONE ? 4u : 0u);
            int32_t lo = sd_dr_min(d, pc, i), hi = sd_dr_max(d, pc, i), kind = (int32_t)sd_dr_kind(d, pc, i);
            int32_t v = set.dkit.d[d].v[i] + (kind == SD_DK_MAC || kind == SD_DK_PCT ? enc_far(e, (uint32_t)(hi - lo)) : e);
            set.dkit.d[d].v[i] = (int16_t)(v < lo ? lo : v > hi ? hi : v);
            dr_hear(d, 0, 0);
        }
        dr_kit_push();
        dr_dirty = 1;
    }
}

/* The encoders on SEQ (every one but PRESETS): ALGORITHM the layer (GLO: the SCALE), the knobs the layer's (seq_layer;
 * GLO leaves them so); SELECT the step (select_turn) */
static void seq_knobs(void)
{
    int32_t e;
    SdStep *st = &seqp.step[seq_cur];
    uint32_t changed = 0;
    if (shift()) {
        if ((e = enc_take(ENC_ALGORITHM)) != 0) {   /* the scale: IN KEY the master's only, set on its page */
            if (!set.xchrom && key_trk() != seq_trk) msg(key_trk() ? "THE KEY IS SEQ B'S" : "THE KEY IS SEQ A'S");
            else seqp.scale = (uint8_t)step_mod(seqp.scale % SD_SCALES, e, SD_SCALES), changed = 1;
        }
    } else if ((e = enc_take(ENC_ALGORITHM)) != 0) {   /* the layer: what the knobs do */
        seq_layer = step_mod(seq_layer, e, SL_N);
        seq_clr_at = 0;
        if (seq_layer == SL_PARTS) seq_dseg = sd_seg_of(&seqp, seq_cur);   /* (the part of the step picked) */
    }
    if (seq_layer == SL_TRACK) {
        if ((e = enc_take(ENC_KNOB1)) != 0) seq_len(e), changed = 1;
        if ((e = enc_take(ENC_KNOB2)) != 0) { seqp.div = (uint8_t)sdiv_step(seqp.div % SD_SDIVS, e); changed = 1; }
        if ((e = enc_take(ENC_KNOB3)) != 0) {
            int32_t w = (int32_t)seqp.swing + e;
            seqp.swing = (uint8_t)(w < 50 ? 50 : w > 75 ? 75 : w);
            changed = 1;
        }
        if ((e = enc_take(ENC_KNOB4)) != 0) {   /* XPOSE: the track's transpose, taken at its next one (as GLO + a key) */
            int32_t x = eng.t[seq_trk].xpose_next + e;
            eng.t[seq_trk].xpose_next = (int8_t)(x < -24 ? -24 : x > 24 ? 24 : x);
        }
    } else if (seq_layer == SL_TOOLS) {
        if ((e = enc_take(ENC_KNOB1)) != 0) {   /* RANDOM: a new line each detent; a turn's first keeps the line before
                                                   it for UNDO */
            if (!seq_roll_at || fm1_ms - seq_roll_at > 1000u) seq_keep_undo();
            seq_roll_at = fm1_ms ? fm1_ms : 1u;
            seq_roll();
        }
        if ((e = enc_take(ENC_KNOB2)) != 0) {   /* UNDO: one a turn (its other detents within 0.6 s nothing) */
            if (!seq_has_undo) msg("NOTHING TO UNDO");
            else if (!seq_back_at || fm1_ms - seq_back_at > 600u) seq_back();
            seq_back_at = fm1_ms ? fm1_ms : 1u;
        }
        if ((e = enc_take(ENC_KNOB3)) != 0) seq_clr_at = fm1_ms ? fm1_ms : 1u;   /* CLEAR: asked, OCT+ yes */
        if ((e = enc_take(ENC_KNOB4)) != 0) seq_dub = e > 0;   /* DUB: a turn right on, left off */
    } else if (seq_segmode) {
        uint8_t bd[SD_SEGS + 1u];
        uint32_t sc = sd_seg_bounds(&seqp, bd), g = seq_dseg < sc ? seq_dseg : sc - 1u;
        if ((e = enc_take(ENC_KNOB1)) != 0) {   /* PARTS: how many, even (UNDO takes it back) */
            int32_t c = (int32_t)sc + e, mx = seqp.len < SD_SEGS ? (int32_t)seqp.len : (int32_t)SD_SEGS;
            c = c < 1 ? 1 : c > mx ? mx : c;
            if ((uint32_t)c != sc) {
                seq_keep_undo();
                seqp.seg_count = (uint8_t)c;
                sd_seg_even(&seqp);
                if (seq_dseg >= (uint32_t)c) seq_dseg = (uint32_t)c - 1u;
                changed = 1;
            }
        }
        if ((e = enc_take(ENC_KNOB2)) != 0) {
            int32_t r = (int32_t)(seqp.seg_rep[g] < 1u ? 1u : seqp.seg_rep[g]) + e;
            seqp.seg_rep[g] = (uint8_t)(r < 1 ? 1 : r > 4 ? 4 : r);
            changed = 1;
        }
        if ((e = enc_take(ENC_KNOB3)) != 0) {   /* ORDER: FWD REV P-P RND, then SKIP (the part left out) */
            uint32_t o = (seqp.seg_skip >> g & 1u) ? SD_SORDERS : seqp.seg_order[g] % SD_SORDERS;
            o = step_lim(o, e, SD_SORDERS + 1u);
            if (o == SD_SORDERS) seqp.seg_skip = (uint8_t)(seqp.seg_skip | 1u << g);
            else { seqp.seg_skip = (uint8_t)(seqp.seg_skip & ~(1u << g)); seqp.seg_order[g] = (uint8_t)o; }
            changed = 1;
        }
        if ((e = enc_take(ENC_KNOB4)) != 0) {
            int32_t l = (int32_t)(bd[g + 1u] - bd[g]) + e;
            sd_seg_set_len(&seqp, g, (uint32_t)(l < 1 ? 1 : l));
            changed = 1;
        }
    } else if (seq_more) {                      /* RATCHET 1..3 (a SLIDE: its TIME, 1..SD_SLIDES), PROB in 5 % steps */
        if ((e = enc_take(ENC_KNOB1)) != 0) {
            if (st->type == SD_ST_SLIDE) {      /* (a SLIDE: its TIME, kept as SD_SLIDE_R) */
                int32_t r = (int32_t)SD_SLIDE_T(st->ratchet) + e;
                st->ratchet = (uint8_t)SD_SLIDE_R((uint32_t)(r < 1 ? 1 : r > (int32_t)SD_SLIDES ? (int32_t)SD_SLIDES : r));
            } else {
                int32_t r = (int32_t)(st->ratchet < 1u ? 1u : st->ratchet) + e;
                st->ratchet = (uint8_t)(r < 1 ? 1 : r > (int32_t)SD_RATCHET_MAX ? (int32_t)SD_RATCHET_MAX : r);
            }
            changed = 1;
        }
        if ((e = enc_take(ENC_KNOB2)) != 0) {
            int32_t q = ((int32_t)st->prob + 2) / 5 * 5 + e * 5;
            st->prob = (uint8_t)(q < 0 ? 0 : q > 100 ? 100 : q);
            changed = 1;
        }
        if ((e = enc_take(ENC_KNOB3)) != 0) {   /* MOVE: the note heard at the step, a step a detent (sd_note_move) */
            int32_t s0 = sd_note_at(&seqp, seq_cur);
            if (s0 < 0) msg("NO NOTE ON THIS STEP");
            else {
                for (; e; e += e > 0 ? -1 : 1) {
                    int32_t s1 = sd_note_move(&seqp, (uint32_t)s0, e);
                    if (s1 < 0) { msg(e > 0 ? "NO ROOM LATER" : "NO ROOM EARLIER"); break; }
                    s0 = s1;
                    changed = 1;
                }
                seq_cur = (uint32_t)s0;         /* (the step picked goes with it) */
                seq_bank = seq_cur / 16u;
            }
        }
        if ((e = enc_take(ENC_KNOB4)) != 0) {   /* LENGTH: how many steps it holds (its TIEs) */
            int32_t s0 = sd_note_at(&seqp, seq_cur);
            if (s0 < 0) msg("NO NOTE ON THIS STEP");
            else
                for (; e; e += e > 0 ? -1 : 1) {
                    if (!sd_note_resize(&seqp, (uint32_t)s0, e)) {
                        if (e > 0) msg("NO ROOM: A NOTE OR THE END");
                        break;
                    }
                    changed = 1;
                }
            if (s0 >= 0 && seq_cur >= (uint32_t)s0 + sd_note_len(&seqp, (uint32_t)s0)) {
                seq_cur = (uint32_t)s0 + sd_note_len(&seqp, (uint32_t)s0) - 1u;   /* (shorter than the step picked) */
                seq_bank = seq_cur / 16u;
            }
        }
    } else {
        if ((e = enc_take(ENC_KNOB1)) != 0) {
            int32_t n = (int32_t)st->note + e;
            st->note = (uint8_t)(n < 0 ? 0 : n > 127 ? 127 : n);
            changed = 1;
            aud_step(seq_cur, 0xFFu);           /* (stopped: the new note heard) */
        }
        if ((e = enc_take(ENC_KNOB2)) != 0) {
            int32_t v = (int32_t)st->vel + enc_far(e, 127u);
            st->vel = (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v);
            changed = 1;
        }
        if ((e = enc_take(ENC_KNOB3)) != 0) {
            int32_t g = (int32_t)st->gate + enc_far(e, 100u);
            st->gate = (uint8_t)(g < 1 ? 1 : g > (int32_t)SD_GATE_MAX ? (int32_t)SD_GATE_MAX : g);
            changed = 1;
        }
        if ((e = enc_take(ENC_KNOB4)) != 0) {
            uint32_t ty = step_mod(st->type % SD_STYPES, e, SD_STYPES);
            if (ty != SD_ST_SLIDE && st->ratchet > SD_RATCHET_MAX) st->ratchet = 1;   /* (RATCHET and a SLIDE's TIME
                                                   share a byte: a TIME, from 17, is no RATCHET; a RATCHET 1..3 a SLIDE
                                                   reads as its first TIME, and keeps) */
            st->type = (uint8_t)ty;
            changed = 1;
        }
    }
    if (changed) seq_push();
}

/* ---- SAVE: what the page shows to a place of your own (the patch; on SEQ the pattern, on ORBIT the song); it plays
 * on, into the flash once PLAY stops and the sound is quiet */
static void draw_save(void)                     /* in the picture's place (the frame began the canvas) */
{
    static const char *const TITLE[5] = {"", "SAVE PATCH", "SAVE PATTERN", "SAVE SONG", "SAVE BEAT"};
    char s[32], *p;
    uint32_t kind = save_mode ? save_mode : save_kind, used = kind == 4u ? udr.used : kind == 3u ? usong.used :
             kind == 2u ? useq.used : upat.used;
    cv_frame(2, 0, 236, H_PIC, C_RED);
    if (!save_mode) {
        *put_place(put(s, "SAVED TO "), kind, save_to) = 0;
        cv_text(10, 44, &FONT_S, s, C_RED);
        if (save_pending) cv_text(10, 66, &FONT_S, eng.seq_run ? "IN THE FLASH AT STOP" : "INTO THE FLASH ...", C_ICE_D);
        return;
    }
    cv_text(10, 22, &FONT_S, TITLE[kind < 5u ? kind : 0u], C_RED);
    p = put(put_place(put(s, "TO "), kind, save_to), "  ");
    if (kind != 1u) p = put(p, used >> save_to & 1u ? "(REPLACE)" : kind == 4u && save_to < DR_FACTORY ? DR_FNAME[save_to] : "EMPTY");
    else p = put(p, used >> save_to & 1u ? patch_name(SD_PATCHES + save_to) : "EMPTY");
    *p = 0;
    cv_text(10, 42, &FONT_S, s, C_ICE);
    cv_text(10, 66, &FONT_S, "ALGO PICKS OCT+ YES OCT- NO", C_ICE_D);
}
static void save_begin(void)
{
    uint32_t k;
    save_mode = page == PAGE_DRUM ? 4u : page == PAGE_ORB ? 3u : page == PAGE_SEQ ? 2u : 1u;
    save_at = fm1_ms;
    if (save_mode == 4u) { save_to = set.drum; return; }
    if (save_mode == 3u) { save_to = set.song; return; }
    if (save_mode == 2u) { save_to = set.pattern; return; }
    if (set.patch >= SD_PATCHES) { save_to = set.patch - SD_PATCHES; return; }
    for (k = 0; k < UPATCHES && (upat.used >> k & 1u); k++) {}
    save_to = k < UPATCHES ? k : 0u;            /* the first empty place */
}
static void save_commit(void)
{
    uint32_t k;
    if (save_mode == 4u) {
        udr.p[save_to] = set.dpat;
        udr.m[save_to] = set.dmore;
        udr.used |= 1u << save_to;
        set.drum = (uint8_t)save_to;
        dr_places();
        dr_dirty = 1;
    } else if (save_mode == 3u) {
        usong.o[save_to] = orbp;
        usong.used |= 1u << save_to;
        set.song = (uint8_t)save_to;
    } else if (save_mode == 2u) {
        eng.pats_seq++;                         /* (the songs' steps read these: not while one is written) */
        SD_BARRIER();
        useq.p[save_to] = seqp;
        useq.used |= 1u << save_to;
        eng.pats_used = useq.used;
        SD_BARRIER();
        eng.pats_seq++;
        set.pattern = (uint8_t)save_to;
    } else {
        const char *n = patch_name(set.patch);
        upat.p[save_to] = pat;
        for (k = 0; k < 8u; k++) upat.name[save_to][k] = *n ? *n++ : 0;
        upat.used |= 1u << save_to;
        set.patch = (uint8_t)(SD_PATCHES + save_to);
    }
    save_pending |= save_mode == 4u ? 8u : save_mode == 3u ? 4u : save_mode;   /* (into the flash once PLAY stops and the sound is quiet: an
                                                   erase stalls the sound; 2026-11-19, the user: saving takes time.
                                                   It no longer stops the sequencer) */
    save_kind = save_mode;
    save_at = saved_at = fm1_ms ? fm1_ms : 1u;
    save_mode = 0;
}

/* nothing heard: the voices silent and every tail at rest (REACTOR, DISTROY, SPACE: its delay, plate or room and the
 * looper), so a flash erase's stall (~50 ms, the audio held) is not heard. 2026-11-19, the user: a click right after
 * STOP; the settings changed while playing were written once the seq stopped and the voices were silent, under the
 * tails still ringing */
static int out_quiet(void)
{
    return sd_engine_silent(&eng) && (!eng.r.on || eng.r.rest) && (!eng.d.on || eng.d.rest) && (!eng.sp.on || eng.sp.rest) &&
           !eng.dr.live;
}

/* ---- C1: the MOD page (the matrix's eight slots, the two LFOs) and HOME (the four macros) */
static char *put_rate(char *p, uint32_t k)      /* an LFO's rate: a division synced, else Hz */
{
    uint32_t hz;
    if (pat.lfo_mode[k] & SD_LM_SYNC) return put(p, LDIV_NAME[(pat.lfo_rate[k] * SD_LFO_DIVS) >> 16]);
    hz = SD_LFO_HZ100[pat.lfo_rate[k] >> 9];    /* Hz x 100 */
    p = put_u(p, hz / 100u);
    if (hz < 1000u) { *p++ = '.'; *p++ = (char)('0' + hz / 10u % 10u); if (hz < 100u) *p++ = (char)('0' + hz % 10u); }
    return put(p, "Hz");
}
static int32_t amt99(int32_t a) { return a >= 0 ? (a * 99 + 16383) / 32767 : -((-a * 99 + 16383) / 32767); }

static void draw_lfo_wave(uint32_t k, int32_t x0, int32_t y0, int32_t w, int32_t h)   /* a cycle, the dot now */
{
    int32_t x, py = 0, mid = y0 + h / 2;
    cv_line(x0, mid, x0 + w - 1, mid, C_GRID);
    for (x = 0; x < w; x++) {
        uint32_t ph = (uint32_t)x * (0xFFFFFFFFu / (uint32_t)w);
        int32_t v = pat.lfo_shape[k] == SD_LS_SH ? ((int32_t)(((ph >> 28) + 1u) * 2654435761u >> 16) - 32768)   /* steps
                                                   (16 across: stairs, 2026-11-22) */
                  : sd_lfo_wave(pat.lfo_shape[k], ph, 0);
        int32_t y = mid - v * (h / 2 - 1) / 32768;
        if (x) cv_line(x0 + x - 1, py, x0 + x, y, C_ICE_D);
        py = y;
    }
    {
        int32_t v = eng.lfo_out[k], y = mid - v * (h / 2 - 1) / 32768, xx = x0 + (int32_t)((eng.lfo_ph[k] >> 16) * (uint32_t)w >> 16);
        cv_rect(xx - 2, y - 2, 5, 5, C_RED);
    }
}









/* ---- ENV, FILTER, ARP: each on its own button */
static char *put_ms(char *p, uint32_t i)        /* an envelope time, 0..127: "12ms", "1.2s" */
{
    uint32_t ms = SD_ENV_MS[i & 127u];
    if (ms < 1000u) { p = put_u(p, ms); return put(p, "ms"); }
    p = put_u(p, ms / 1000u);
    if (ms < 10000u) { *p++ = '.'; *p++ = (char)('0' + ms / 100u % 10u); }
    return put(p, "s");
}







/* ==== The screen, the same on every page (K1-4 a row on top, as the knobs lie):
 *     0  the logo, the patch's number           22  the patch, the edited bank's roles, the mode
 *    44  the button's pages, this one a red tab; right: what EDIT switches here, ON / OFF
 *    64  K1 K2 K3 K4, left to right as the knobs: name, value, bar (the names red: GLO held, the second layer)
 *   116  the picture of what they set
 *   186  what ALGORITHM (and GLO + ALGORITHM) do here, or a message
 *   204  the steps of the track playing         222  the transport, the load
 * Each region is drawn into the canvas 25 times a second and sent only when its pixels changed. */


/* ---- what EDIT switches: the page's engine, the same everywhere (a toggle) */
static uint32_t fx_on(uint32_t bit)             /* an engine on: not switched off, and in the patch at all */
{
    if (pat.fx_off & bit) return 0;
    if (bit == SD_FX_REACTOR) return pat.r_mix != 0;
    if (bit == SD_FX_DISTROY) return pat.d_mix != 0;
    if (bit == SD_FX_SPACE) return pat.s_rlevel != 0;
    if (bit == SD_FX_TIME) return pat.s_dlevel != 0;
    if (bit == SD_FX_SUB) return pat.sub_lvl != 0;
    return pat.f_type != SD_FT_OFF;
}
static uint32_t page_fx(void)
{
    return page == 1u ? SD_FX_REACTOR : page == 2u ? SD_FX_DISTROY : page == 3u ? SD_FX_SPACE : page == PAGE_TIME ? SD_FX_TIME :
           page == PAGE_FILT ? SD_FX_FILTER : page == PAGE_SUB ? SD_FX_SUB : 0u;
}
static uint32_t dest_set(const SdPatch *p)     /* DISTROY's bands are set: the splits rising, in range */
{
    return p->d_split[0] > SD_R_LO * 256 && p->d_split[0] < p->d_split[1] && p->d_split[1] < p->d_split[2];
}
static void dest_first(void)                    /* DISTROY's first time on (never set: the splits at 0): four bands
                                                   at 200 Hz, 1.2 kHz, 5 kHz, each SOFT with a little drive, 1 x */
{
    uint32_t k;
    if (dest_set(&pat)) return;
    pat.d_split[0] = SD_DHZ_200; pat.d_split[1] = SD_DHZ_1200; pat.d_split[2] = SD_DHZ_5000;
    pat.d_xover = SD_DX_LR4;
    for (k = 0; k < 4u; k++) {
        pat.d_type[k] = SD_DS_SOFT;
        pat.d_gain[k] = 9000;                   /* +5 dB */
        pat.d_alt[k] = 16000;
        pat.d_level[k] = 26000;
        pat.d_body[k] = 32768;
    }
}
static uint32_t react_set(const SdPatch *p)     /* the Reactor's stages are set: one with a level */
{
    uint32_t i, k, any = 0;
    for (i = 0; i < 6u; i++) for (k = 0; k < 4u; k++) any |= p->stage[i].level[k];
    return any != 0;
}
static void react_first(void)                   /* the Reactor's first time on (never set: no stage with a level):
                                                   six vowels on POS (A E EE AW OO ER), BP12, a little resonance, slew
                                                   and drive, the filters spread, as the boot patch's, so it sounds */
{
    static const SdReactorStage V[6] = {SD_RST_VOWEL(SD_RV_A), SD_RST_VOWEL(SD_RV_E), SD_RST_VOWEL(SD_RV_EE),
                                        SD_RST_VOWEL(SD_RV_AW), SD_RST_VOWEL(SD_RV_OO), SD_RST_VOWEL(SD_RV_ER)};
    uint32_t i;
    if (react_set(&pat)) return;
    for (i = 0; i < 6u; i++) pat.stage[i] = V[i];
    pat.r_model = SD_RM_BP12;
    if (!pat.r_res) pat.r_res = 36000;
    if (!pat.r_slew) pat.r_slew = 24000;
    if (!pat.r_drive) pat.r_drive = 9000;
    if (!pat.r_pan[0] && !pat.r_pan[1] && !pat.r_pan[2] && !pat.r_pan[3]) {
        pat.r_pan[0] = -24000; pat.r_pan[1] = 24000; pat.r_pan[2] = -8000; pat.r_pan[3] = 8000;
    }
    if (!pat.r_max) pat.r_max = 5;
}
static void filt_first(void)                    /* the filter's first time on: open, ENV3 as ENV1, so it sounds */
{
    if (pat.f_cut || pat.f_res || pat.e3_s || pat.e3_d) return;
    pat.f_cut = 52000;
    pat.e3_a = pat.e1_a; pat.e3_d = pat.e1_d; pat.e3_s = pat.e1_s; pat.e3_r = pat.e1_r;
}
enum { SW_NONE, SW_OFF, SW_ON, SW_LOCK, SW_WAIT, SW_LOOP, SW_KEPT };   /* (WAIT: on, in at A's one; LOOP: COURSE's,
                                                   on; KEPT: the LOOPER off, its loop kept: the lamp dark) */
static uint32_t sw_of(const char **what)        /* this page's switch, and its name on the screen */
{
    uint32_t b = page_fx();
    *what = "EDIT";
    if (b) return fx_on(b) ? SW_ON : SW_OFF;
    if (page == 0u) { *what = "EDIT COURSE"; return pat.course == SD_COURSE_LOOP ? SW_LOOP : pat.course ? SW_ON : SW_OFF; }
    if (page == PAGE_ARP) return eng.arp_on ? SW_ON : SW_OFF;
    if (page == PAGE_LOOP) return lp_arm ? SW_ON : lp_hold ? SW_KEPT : SW_OFF;   /* (armed: EDIT; KEPT: EDIT plays it
                                                   again; the hint line said so till 2026-11-22) */
    if (page == PAGE_ORB) return !orbp.on ? SW_OFF : eng.seq_run && !eng.orb_live ? SW_WAIT : SW_ON;
    if (page == PAGE_DRUM) return !eng.dr.on ? SW_OFF : eng.seq_run && !eng.dr.in ? SW_WAIT : SW_ON;
    if (page == PAGE_SEQ)
        return seq_trk && set.mode == SD_MODE_MONO ? SW_LOCK : eng.t[seq_trk].mute ? SW_OFF :
               eng.seq_run && !eng.t[seq_trk].in ? SW_WAIT : SW_ON;
    if (page == 4u && dmod == DMOD_TUR) return SD_TUR_ON(&pat) ? SW_ON : SW_OFF;
    return SW_NONE;
}
static uint32_t edit_toggle(void)               /* EDIT: what this page shows, on or off; 1: the patch changed */
{
    uint32_t b = page_fx();
    if (b) {
        if (fx_on(b)) pat.fx_off = (uint8_t)(pat.fx_off | b);
        else {
            pat.fx_off = (uint8_t)(pat.fx_off & ~b);   /* (off by its level in the patch: a level to hear) */
            if (b == SD_FX_REACTOR && !pat.r_mix) pat.r_mix = 32768;
            if (b == SD_FX_REACTOR) react_first();
            if (b == SD_FX_DISTROY && !pat.d_mix) pat.d_mix = 65535;
            if (b == SD_FX_DISTROY) dest_first();
            if (b == SD_FX_SPACE && !pat.s_rlevel) pat.s_rlevel = 20000;
            if (b == SD_FX_TIME && !pat.s_dlevel) pat.s_dlevel = 20000;
            if (b == SD_FX_FILTER && !pat.f_type) { pat.f_type = SD_FT_LP12; filt_first(); }
            if (b == SD_FX_SUB && !pat.sub_lvl) pat.sub_lvl = 36000;   /* (its first time: a level to hear, a sine,
                                                   under A) */
        }
        return 1;
    }
    if (page == 0u) {                           /* COURSE: OFF, ON (from each note), LOOP (round on its own), OFF */
        pat.course = (uint8_t)((pat.course + 1u) % 3u);
        msg(pat.course == SD_COURSE_LOOP ? "COURSE: ROUND AND ROUND" : pat.course ? "COURSE: FROM EACH NOTE" : "COURSE OFF");
        return 1;
    }
    if (page == 4u && dmod == DMOD_TUR) {       /* RADAR: on the first time, sent nowhere yet, a free MOD slot to XM,
                                                   30 %; CHANGE 25 */
        uint32_t i, to = pat.tur_dst != 0u;
        pat.tur_set ^= 0x80u;
        for (i = 0; i < SD_MODS; i++) if (pat.mod[i].src == SD_MS_TUR && pat.mod[i].dst) to = 1;
        if (SD_TUR_ON(&pat) && !to) {
            if (!(pat.tur_set & 127u)) pat.tur_set |= 25u;
            for (i = 0; i < SD_MODS && pat.mod[i].src; i++) ;
            if (i < SD_MODS) {
                static char s[32];             /* (msg keeps the pointer) */
                pat.mod[i] = (SdModSlot){SD_MS_TUR, SD_MD_XM, (int16_t)(30 * 32767 / 99)};
                pat.mod_off[i] = 0;
                *put_u(put(s, "RADAR TO XM: MOD SLOT "), i + 1u) = 0;
                msg(s);
            } else msg("MOD FULL: RADAR GOES NOWHERE");
        }
        return 1;
    }
    if (page == PAGE_ARP) {
        if (!eng.arp_on && !arp_room()) msg(set.mode == SD_MODE_MONO ? "NO ARP WHILE SEQ PLAYS: MONO" : "NO VOICE LEFT FOR ARP");
        else eng.arp_on = (uint8_t)!eng.arp_on;
    }
    else if (page == PAGE_ORB) {
        orbp.on = (uint8_t)!orbp.on;
        orb_push();
        if (orbp.on && eng.a_wait) msg(set.master ? "ORBIT IN AT B'S ONE" : "ORBIT IN AT A'S ONE");
    }
    else if (page == PAGE_DRUM) {               /* the drums: in at the master's one while it plays */
        eng.dr.on = (uint8_t)!eng.dr.on;
        if (eng.dr.on && eng.seq_run && eng.a_wait) msg(set.master ? "DRUMS IN AT B'S ONE" : "DRUMS IN AT A'S ONE");
        else if (eng.dr.on && !eng.seq_run) msg("DRUMS ON: PLAY STARTS THEM");
    }
    else if (page == PAGE_SEQ) {
        if (seq_trk && set.mode == SD_MODE_MONO) msg("B NEEDS DUO OR POLY: SEL");
        else if (seq_trk && eng.t[1].mute && eng.t[1].width && eng.seq_run &&
                 (trk_need(0) < voices_n() ? trk_need(0) : voices_n()) >= voices_n()) msg("NO VOICE LEFT FOR SEQ B");
        else {
            eng.t[seq_trk].mute = (uint8_t)!eng.t[seq_trk].mute;
            if (seq_trk != set.master && !eng.t[seq_trk].mute && eng.a_wait)
                msg(seq_trk ? "SEQ B IN AT A'S ONE" : "SEQ A IN AT B'S ONE");
        }
    } else msg("NOTHING TO SWITCH HERE");
    return 0;
}

/* MACROS, GLO + the keys F3..E4: on and off from HOME, as EDIT on each one's page (2026-11-19, the user: "HOME
 * with the macros has become a battledeck, but I miss switching things on and off"): the chain's five in its order
 * (FILTER REACTOR DISTROY TIME SPACE), then ARP, LOOPER, SEQ A, SEQ B, ORBIT, RADAR; the keys lit where it is on */
#define HOME_N (11u + SD_DRUMS)                 /* MACROS' switches: the chain's five on the black keys B1..B5
                                                   (F#3..D#4), the players' seven on the white keys under them (F3..E4);
                                                   the rest of the keys play (2026-11-21: the drums had come on G5, the
                                                   players on B6..B11) */
static int32_t home_sw(uint32_t k)              /* key k's switch (0..HOME_N - 1), or -1: a key that plays */
{
    int32_t b = key_black(k), w = key_white(k);
    return b >= 0 && b < 5 ? b : w >= 0 && w < (int32_t)HOME_N - 5 ? 5 + w : -1;
}
static const uint8_t HOME_PG[HOME_N] = {PAGE_FILT, 1, 2, PAGE_TIME, 3, PAGE_ARP, PAGE_LOOP, PAGE_SEQ, PAGE_SEQ, PAGE_ORB, 4
#if SD_DRUMS
                                        , PAGE_DRUM
#endif
};
static const char *const HOME_SW[HOME_N] = {"FILTER", "REACTOR", "DISTROY", "TIME", "SPACE", "ARP", "LOOPER", "SEQ A",
                                            "SEQ B", "ORBIT", "RADAR"
#if SD_DRUMS
                                            , "DRUMS"
#endif
};
static uint32_t home_at(uint32_t i, uint32_t flip)   /* item i's EDIT lamp (SW_*), or (flip) EDIT there: 1 the patch
                                                   changed */
{
    uint32_t pg = page, dm = dmod, tr = seq_trk, r;
    const char *w;
    page = HOME_PG[i % HOME_N];
    if (i == 7u || i == 8u) seq_trk = i - 7u;
    if (i == 10u) dmod = DMOD_TUR;
    if (!flip) r = sw_of(&w);
    else if (page == PAGE_LOOP) { lp_switch(); r = 0; }   /* (off: its loop kept) */
    else r = edit_toggle();
    page = pg; dmod = dm; seq_trk = tr;
    return r;
}
static uint32_t home_on(uint32_t i) { uint32_t s = home_at(i, 0); return s == SW_ON || s == SW_WAIT || s == SW_LOOP; }
static uint32_t home_toggle(uint32_t i)         /* GLO + B(i + 1) on MACROS; 1: the patch changed */
{
    static char t[24];
    uint32_t was = msg_at, r = home_at(i, 1);
    if (msg_at == was) {                        /* (its own word, a refusal or IN AT THE ONE, else ON / OFF) */
        char *p = put(t, HOME_SW[i % HOME_N]);
        *put(p, home_on(i) ? ": ON" : ": OFF") = 0;
        msg(t);
    }
    return r;
}

/* ---- the top line: the button's pages */
static const char *const NAV_HOME[2] = {"MACROS", "SETUP"}, *const NAV_FX[4] = {"REACTOR", "DISTROY", "TIME", "SPACE"},
                         *const NAV_ENV[4] = {"NAV", "FILTER", "ENV", "SUB"}, *const NAV_LFO[4] = {"LFO 1", "LFO 2", "RADAR", "MOD"},
                         *const NAV_ARP[2] = {"ARP", "LOOPER"}, *const NAV_SEQ[4] = {"SEQ A", "SEQ B", "ORBIT", "DRUM"};
static uint32_t nav_of(const char *const **n, uint32_t *cur)
{
    switch (page) {
    case 5: case PAGE_SETUP: *n = NAV_HOME; *cur = page == PAGE_SETUP; return 2;
    case 0: *n = NAV_ENV; *cur = 0; return 4;
    case 1: case 2: case 3: case PAGE_TIME: *n = NAV_FX; *cur = page == PAGE_TIME ? 2u : page == 3u ? 3u : page - 1u; return 4;
    case PAGE_ENV: case PAGE_FILT: case PAGE_SUB: *n = NAV_ENV; *cur = page == PAGE_SUB ? 3u : page == PAGE_ENV ? 2u : 1u; return 4;
    case 4: *n = NAV_LFO; *cur = dmod >= SD_MODS ? dmod - SD_MODS : 3u; return 4;
    case PAGE_ARP: case PAGE_LOOP: *n = NAV_ARP; *cur = page == PAGE_LOOP; return 2;
    default: *n = NAV_SEQ; *cur = page == PAGE_DRUM ? 3u : page == PAGE_ORB ? 2u : seq_trk; return SD_DRUMS ? 4 : 3;
    }
}
static void draw_nav(void)
{
    const char *const *n, *what;
    uint32_t cur, cnt = nav_of(&n, &cur), k, sw = sw_of(&what);
    const char *t;
    t = sw == SW_ON || sw == SW_WAIT ? "ON" : sw == SW_LOOP ? "LOOP" : sw == SW_KEPT ? "KEPT" :
        sw == SW_OFF ? "OFF" : "MONO";
    int32_t x = 6, gap = 10, tw = 0, bw = text_w(&FONT_S, t) + 6, bx = 238 - bw,
            lx = bx - 15 - text_w(&FONT_S, what);   /* (EDIT: left of the lamp) */
    int32_t end = 238 - text_w(&FONT_S, "LOOP") - 6 - 8;   /* (where the names stop: before the lamp at its widest
                                                   word, so a button's names stay as they are whatever EDIT says) */
    char sn[5][5];                              /* (when all do not fit, FX's: the one shown whole, the others cut
                                                   to four, and no EDIT: the lamp and its word say it) */
    const char *nm[5];
    for (k = 0; k < cnt && k < 5u; k++) nm[k] = n[k];
    for (k = 0; k < cnt; k++) tw += text_w(&FONT_S, nm[k]);
    if (cnt > 1u && x + tw + 3 * (int32_t)(cnt - 1u) + 4 > end) {
        for (k = 0, tw = 0; k < cnt && k < 5u; k++) {
            if (k != cur) {
                uint32_t i;
                for (i = 0; i < 4u && n[k][i]; i++) sn[k][i] = n[k][i];
                sn[k][i] = 0; nm[k] = sn[k];
            }
            tw += text_w(&FONT_S, nm[k]);
        }
        lx = -99;
    }
    if (x + tw + gap * (int32_t)(cnt - 1u) + 14 > lx) lx = -99;   /* (EDIT only clear of the names, further off
                                                   than their gap, so it never reads as a page) */
    if (sw && cnt > 1u && x + tw + gap * (int32_t)(cnt - 1u) + 4 > bx - 8) {   /* (and closer, so the box stops
                                                   before the lamp, the picked one's red as wide) */
        gap = (bx - 8 - 4 - x - tw) / (int32_t)(cnt - 1u);
        if (gap < 3) gap = 3;
    }
    cv_begin(240, 18, C_BG);
    for (k = 0; k < cnt; k++) {
        int32_t w = text_w(&FONT_S, nm[k]);
        if (k == cur) {
            cv_box(x - 4, 0, w + 8, 16, C_RED);   /* (16 rows: the capitals 3 from either edge) */
            cv_text(x, 1, &FONT_S, nm[k], C_BG);
        } else {
            cv_text(x, 1, &FONT_S, nm[k], n == NAV_SEQ && k == 1u && set.mode == SD_MODE_MONO ? C_GRID : C_ICE_D);
        }
        if (n == NAV_SEQ && k == set.master) cv_rect(x, 14, w, 1, k == cur ? C_BG : C_RED);   /* the master */
        x += w + gap;
    }
    if (sw) {                                   /* the switch: "EDIT" (when it fits) and ON / OFF / MONO */
        uint16_t c = sw == SW_OFF ? C_ICE_D : C_RED;
        int32_t lit = sw == SW_ON || sw == SW_LOOP || (sw == SW_WAIT && (fm1_ms / 200u) & 1u);   /* (WAIT: it blinks
                                                   till A's one) */
        cv_box(bx - 6, 4, 8, 8, lit ? C_RED : C_GRID);   /* an LED, as on a panel, and its word */
        if (lit) cv_rect(bx - 4, 6, 2, 2, C_GLINT);
        cv_text(bx + 5, 1, &FONT_S, t, lit ? C_ICE : sw == SW_OFF ? C_ICE_D : c);
        if (lx >= 0) cv_text(lx, 1, &FONT_S, what, C_ICE_D);
    }
}

/* ---- the four knobs */
typedef struct { const char *nm; char v[12]; int16_t bar; uint8_t kind, hot; } Kn;
enum { KN_TXT, KN_UNI, KN_BI, KN_BTN };       /* (KN_BTN: its value a panel button, bar its state: btn()'s st) */
static Kn kn[4];
static void kt(uint32_t i, const char *nm, const char *v)   /* a name and a text (7 characters fit) */
{
    char *p = kn[i].v;
    uint32_t n = 0;
    kn[i].nm = nm; kn[i].kind = KN_TXT; kn[i].bar = 0; kn[i].hot = 0;
    while (v && *v && n < 7u) { *p++ = *v++; n++; }
    *p = 0;
}
static void kx(uint32_t i, const char *nm, const char *v, uint32_t v99) { kt(i, nm, v); kn[i].kind = KN_UNI; kn[i].bar = (int16_t)(v99 > 99u ? 99u : v99); }
static void ku(uint32_t i, const char *nm, uint32_t v99) { char t[12]; *put_u(t, v99) = 0; kx(i, nm, t, v99); }
static void kb(uint32_t i, const char *nm, const char *v, int32_t b99)   /* a bar from the middle, -99..99 */
{
    kt(i, nm, v);
    kn[i].kind = KN_BI;
    kn[i].bar = (int16_t)(b99 < -99 ? -99 : b99 > 99 ? 99 : b99);
}
static void k0(uint32_t i) { kn[i].nm = 0; kn[i].v[0] = 0; kn[i].kind = KN_TXT; kn[i].hot = 0; }
static void kbt(uint32_t i, const char *nm, const char *v, uint32_t st) { kt(i, nm, v); kn[i].kind = KN_BTN; kn[i].bar = (int16_t)st; }

static void env_knobs(const uint8_t *ev)        /* an envelope: A D S R (0..127) */
{
    char t[12];
    *put_ms(t, ev[0]) = 0; kx(0, "ATTACK", t, ev[0] * 99u / 127u);
    *put_ms(t, ev[1]) = 0; kx(1, "DECAY", t, ev[1] * 99u / 127u);
    *put_u(t, ev[2] * 100u / 127u) = 0; kx(2, "SUS", t, ev[2] * 99u / 127u);
    *put_ms(t, ev[3]) = 0; kx(3, "RELEASE", t, ev[3] * 99u / 127u);
}
static char *put_xp(char *p, int32_t x)         /* a transpose: +5, and IN KEY "+5 KEY" (up a fourth in A's key; two
                                                   digits "+24KEY": a knob's column holds 6 of these characters) */
{
    p = put_sgn(p, x);
    return x && !set.xchrom ? put(p, x > 9 || x < -9 ? "KEY" : " KEY") : p;
}
static void seq_l2_knobs(void)                  /* SEQ with GLO held */
{
    char t[12], *p;
    int32_t x = eng.t[seq_trk].xpose_next;
    uint32_t sw = seqp.swing < 50u ? 50u : seqp.swing;
    *put_u(t, seqp.len) = 0; kx(0, "LENGTH", t, (seqp.len - 1u) * 99u / 63u);
    kt(1, "DIV", SDIV_NAME[seqp.div % SD_SDIVS]);
    p = put_u(t, sw); *p++ = '%'; *p = 0; kx(2, "SWING", t, (sw - 50u) * 99u / 25u);
    *put_xp(t, x) = 0; kb(3, "XPOSE", t, x * 99 / 24);
    kn[3].hot = x != eng.t[seq_trk].xpose;      /* (red: waiting for the one) */
}
static void knobs_of(void)
{
    char t[16], *p;
    uint32_t l = lay();
    switch (page) {
    case 0:                                     /* NAV: TONE, X/Y, ROUTE (SELECT the bank) */
        if (l == LN_TONE) { ku(0, "XM", q99(pat.xm)); ku(1, "WARP", q99(pat.warp)); ku(2, "FOLD", q99(pat.fold)); ku(3, "DETUNE", q99(pat.dt)); }
        else if (l == LN_XY) {
            ku(0, "X", q99(pat.x)); ku(1, "Y", q99(pat.y)); ku(2, "WIDTH", q99(pat.width));
            kt(3, "GEN", gen < SD_GENS ? sd_gen_name(gen) : "TURN");
        } else {                                /* ROUTE: the bank's roles (its list as it turns), the bank */
            uint32_t al = algo_of(&pat);
            if (al < SD_ALGOS) kt(0, "ROUTING", ALG_ROUTE[al % 8u]); else { roles_of(&pat, t); kt(0, "ROUTING", t); }
            p = t; *p++ = (char)('A' + ebank); *p = 0; kt(1, "BANK", t);
            k0(2); k0(3);
        }
        break;
    case PAGE_SETUP:                            /* the FM-1's: TEMPO XPOSE MASTER COLOUR; MORE SYNC CORES; TOOLS */
        if (l == LU_TOOLS) {                    /* (the picture draws these: page_full) */
            k0(0); k0(1); k0(2); k0(3);
            break;
        }
        if (l == LU_MORE) {
            kt(0, "SYNC", set.sync_int ? "INT" : "AUTO");
#if FM1_CPU1
            kt(1, "CORES", set.one_core ? "1" : "2");
#else
            k0(1);
#endif
            kt(2, "CLICK", CLICK_NAME[set.click % 3u]);
            k0(3);
            break;
        }
        if (eng.clk_on) {                       /* (a MIDI clock leads: its tempo, CLK) */
            uint32_t b = eng.clk_beat;
            p = b ? put_u(t, (SD_FS * 60u + b / 2u) / b) : put(t, "---");
            *put(p, " CLK") = 0;
            kt(0, "TEMPO", t);
        } else { *put_u(t, set.bpm) = 0; kx(0, "TEMPO", t, ((uint32_t)set.bpm - 40u) * 99u / 200u); }
        kt(1, "XPOSE", set.xchrom ? "CHROM" : "IN KEY");
        kt(2, "MASTER", set.master ? "SEQ B" : "SEQ A");
        kt(3, "COLOUR", ACC_NAME[set.accent % SD_ACCENTS]);
        break;
    case 5:                                     /* MUTATE MOVE BREAK CUTOFF (the synth's); MORE: BODY AIR DROWN DARK */
        if (l == 1u) { ku(0, "BODY", q99(pat.mac[3])); ku(1, "AIR", q99(pat.mac_air * 257u)); ku(2, "DROWN", q99(pat.mac_drown * 257u));
                       ku(3, "DARK", q99(home_filt)); break; }
        ku(0, "MUTATE", q99(pat.mac[0])); ku(1, "MOVE", q99(pat.mac[1])); ku(2, "BREAK", q99(pat.mac[2]));
        *put_hz(t, hz_of(16 * 256 + (int32_t)((uint32_t)pat.f_cut * 114u >> 8))) = 0;
        kx(3, "CUTOFF", t, q99(pat.f_cut));
        break;
    case 1:
        if (r_stages) {                         /* STAGE, F1-F4: the stage SELECT picks */
            const SdReactorStage *st = &pat.stage[rst_cur % 6u];
            uint32_t k;
            if (l == LR_STAGE) {
                int32_t m = rst_match(st), w = ((int32_t)st->pitch[3] - (int32_t)st->pitch[0] + 128) >> 8;
                uint32_t lv = 0;
                for (k = 0; k < 4u; k++) if (st->level[k] > lv) lv = st->level[k];
                kt(0, "TYPE", m < 0 ? "CUSTOM" : m < (int32_t)SD_RVOWELS ? SD_RVOWEL_NAME[m] : RST_CHORD_NAME[m - (int32_t)SD_RVOWELS]);
                *put_note(t, (st->pitch[0] + 128u) >> 8) = 0; kt(1, "PITCH", t);
                *put_sgn(t, w) = 0; kt(2, "WIDTH", t);
                ku(3, "LEVEL", q99(lv));
            } else {
                static const char *const FN[4] = {"F1", "F2", "F3", "F4"};
                for (k = 0; k < 4u; k++) { *put_note(t, (st->pitch[k] + 128u) >> 8) = 0; kt(k, FN[k], t); }
            }
            break;
        }
        if (l == LR_MORE) {                     /* MORE: TRACK, LOW, how many stages */
            static const char *const TN[4] = {"OFF", "KEY", "HARM", "KEY+H"};
            kt(0, "TRACK", TN[pat.r_track & 3u]);
            if (pat.r_low) *put_u(t, 50u + ((pat.r_low * 350u) >> 16)) = 0; else *put(t, "OFF") = 0;
            kx(1, "LOW", t, q99(pat.r_low));
            *put_u(t, SD_R_NST(&pat)) = 0; kx(2, "STAGES", t, (SD_R_NST(&pat) - 1u) * 99u / 5u);
            k0(3);
        } else if (l == LR_REACT) {
            uint32_t pos = (uint32_t)pat.r_pos * SD_R_NST(&pat) / 6u;   /* (the stage it is on, of those in use) */
            p = t; *p++ = (char)('1' + pos / 256u); *p++ = '.'; p = put_u(p, (pos & 255u) * 10u / 256u); *p = 0;
            kx(0, "POS", t, pos * 99u / 1535u);
            ku(1, "RES", q99(pat.r_res)); ku(2, "SLEW", q99(pat.r_slew)); ku(3, "DRIVE", q99(pat.r_drive));
        } else {
            int32_t f = pat.r_freq / 256;
            *put_sgn(t, f) = 0; kb(0, "FREQ", t, f * 99 / 48);
            kt(1, "MODEL", MODEL_NAME[pat.r_model % SD_RMODELS]);
            if (pat.r_div) *put_u(t, pat.r_div) = 0; else *put(t, "OFF") = 0;
            kt(2, "DIV", t);
            ku(3, "MIX", q99(pat.r_mix));
        }
        break;
    case 2:
        if (l == LD_LOOP) {                     /* LOOP: FDBK TIME SPREAD TAME */
            uint32_t tm = ((44u + ((pat.d_ftime * 838u) >> 16)) * 10u + 220u) / 441u;
            ku(0, "FDBK", q99(pat.d_fb));
            p = put_u(t, tm); p = put(p, "ms"); *p = 0; kx(1, "TIME", t, q99(pat.d_ftime));
            ku(2, "SPREAD", q99(pat.d_spread));
            if (pat.d_tame) ku(3, "TAME", q99(pat.d_tame)); else kx(3, "TAME", "OFF", 0);
        } else if (l == LD_MIX) {               /* MIX: the band's upper SPLIT, the whole's XOVER and MIX */
            *put_hz(t, hz_of(pat.d_split[dband < 3u ? dband : 2u])) = 0; kt(0, "SPLIT", t);
            kt(1, "XOVER", pat.d_xover == SD_DX_LR4 ? "LR4" : "CMP");
            ku(2, "MIX", q99(pat.d_mix));
            k0(3);
        } else if (l == LD_BAND) {              /* a band: TYPE DRIVE, ALT by what it does, LEVEL */
            uint32_t k = dband, ty = pat.d_type[k] % SD_DSHAPES;
            kt(0, "TYPE", SHAPE_NAME[ty]);
            p = put(t, "+"); p = put_u(p, pat.d_gain[k] * 36u / 65535u); p = put(p, "dB"); *p = 0;
            kx(1, "DRIVE", t, q99(pat.d_gain[k]));
            if (ty) ku(2, ALT_NAME[ty], q99(pat.d_alt[k])); else k0(2);
            ku(3, "LEVEL", q99(pat.d_level[k]));
        } else {                                /* SHAPE: BIAS PAN DYNAMIC (PHAT .. SHARP) BODY */
            uint32_t k = dband;
            int32_t sv = pat.d_shape[k];
            *put_sgn(t, pat.d_bias[k] / 655) = 0; kb(0, "BIAS", t, pat.d_bias[k] / 331);
            *put_sgn(t, pat.d_pan[k] / 328) = 0; kb(1, "PAN", t, pat.d_pan[k] / 331);
            if (!sv) *put(t, "OFF") = 0;
            else { p = put(t, sv < 0 ? "PHAT " : "SHARP"); *put_u(p, q99((uint32_t)(sv < 0 ? -sv : sv) * 2u)) = 0; }
            kb(2, "DYNAMIC", t, sv / 331);
            p = put_sgn(t, -54 + (int32_t)(pat.d_body[k] * 48u / 65535u)); p = put(p, "dB"); *p = 0;
            kx(3, "BODY", t, q99(pat.d_body[k]));
        }
        break;
    case 3:                                     /* SPACE: the plate; FREEZE (the player's), PITCH; MORE: SHIMMER */
        if (l == 0u) {
            ku(0, "SIZE", q99(pat.s_size)); ku(1, "DECAY", q99(pat.s_decay)); ku(2, "TONE", q99(pat.s_rtone));
            ku(3, "R-LEVEL", q99(pat.s_rlevel));
        } else if (l == 1u) {
            kbt(0, "FREEZE", sp_frz ? "ON" : "OFF", sp_frz ? 1u : 0u);
            if (sp_pitch) *put_sgn(t, sp_pitch) = 0; else *put(t, "0") = 0;
            kb(1, "PITCH", t, sp_pitch * 99 / 12);   /* (the frozen tail's: while FREEZE holds it) */
            k0(2); k0(3);
        } else {
            ku(0, "SHIMMER", SD_SP_SHIM(&pat) * 99u / 63u);
            k0(1); k0(2); k0(3);
        }
        break;
    case PAGE_TIME:                             /* TIME: the delay; MODE: FREE or SYNC, PING-PONG */
        if (l == 1u) {
            kbt(0, "MODE", pat.s_mode & SD_SP_SYNC ? "SYNC" : "FREE", pat.s_mode & SD_SP_SYNC ? 1u : 0u);
            kbt(1, "PING", pat.s_mode & 2u ? "ON" : "OFF", pat.s_mode & 2u ? 1u : 0u);
            k0(2); k0(3);
            break;
        }
        *put_time(t) = 0; kt(0, "TIME", t);
        kn[0].hot = sp_fold(0) != 0u;           /* (red: longer than the line, it plays shorter: the hint says) */
        ku(1, "FDBK", q99(pat.s_fb)); ku(2, "TONE", q99(pat.s_dtone)); ku(3, "D-LEVEL", q99(pat.s_dlevel));
        break;
    case PAGE_LOOP:                             /* LOOPER: LOOP LENGTH SPEED L-LEVEL; SEND: FILTER R-SEND D-SEND; TOOLS:
                                                   GLIDE REV OCTAVE CLEAR */
        if (l == LL_TOOLS) {
            int32_t o = lp_speed / 3072;
            *put_glide(t) = 0; kt(0, "GLIDE", t);
            kbt(1, "REV", lp_rev ? "ON" : "OFF", lp_rev ? 1u : 0u);
            if (o) *put_sgn(t, o) = 0; else *put(t, "0") = 0;
            kb(2, "OCTAVE", t, o * 99);
            kbt(3, "CLEAR", "CLEAR", lp_clr_at ? 1u : 0u);
            break;
        }
        if (l == LL_SEND) {
            int32_t f = (int32_t)lp_filt - 32768;
            if (f > -1024 && f < 1024) kt(0, "FILTER", "OFF");
            else { p = put(t, f < 0 ? "LP " : "HP "); *put_u(p, (uint32_t)(f < 0 ? -f : f) * 99u / 32768u) = 0; kb(0, "FILTER", t, f * 99 / 32768); }
            ku(1, "R-SEND", q99(lp_rsend));
            ku(2, "D-SEND", q99(lp_dsend));
            k0(3);
            break;
        }
        {
            int32_t v = lp_speed, a = v < 0 ? -v : v;
            kbt(0, "LOOP", lp_on ? "ON" : "OFF", lp_on ? 1u : 0u);
            *put_len(t) = 0; kt(1, "LENGTH", t);
            kn[1].hot = lp_fold(0) != 0u;       /* (red: longer than its memory) */
            p = t; if (v) *p++ = v < 0 ? '-' : '+';   /* (semitones) */
            *put_u(p, ((uint32_t)a + 128u) >> 8) = 0;
            kb(2, "SPEED", t, v * 99 / 3072);
            ku(3, "L-LEVEL", q99(lp_lvl));
        }
        break;
    case PAGE_ENV:
        env_knobs(l ? &pat.e2_a : &pat.e1_a);
        break;
    case PAGE_SUB: {                            /* SUB: SHAPE, OCTAVE, LEVEL (what it follows: ALGORITHM) */
        int32_t o = (int32_t)pat.sub_oct - 1;   /* (-3..+3 from the note; -1 the usual) */
        p = pat.sub_shape == 0u ? put(t, "SINE") : pat.sub_shape >= 100u ? put(t, "TRI") : put(put_u(t, pat.sub_shape), "%");
        *p = 0;
        kx(0, "SHAPE", t, pat.sub_shape * 99u / 100u);
        if (o) *put_sgn(t, o) = 0; else *put(t, "0") = 0;
        kb(1, "OCTAVE", t, o * 33);
        kt(2, "FOLLOWS", SUB_NAME[pat.sub_src % SD_SUBS]);   /* (ALGORITHM's till 2026-11-22) */
        ku(3, "LEVEL", q99(pat.sub_lvl));
        break;
    }
    case PAGE_FILT:
        if (l == LF_ENV) { env_knobs(&pat.e3_a); break; }
        if (l == LF_MORE) {                     /* MORE: the TYPE (LP12 LP24), VEL (how much the velocity opens it) */
            kt(0, "TYPE", pat.f_type == SD_FT_LP24 ? "LP24" : pat.f_type == SD_FT_OFF ? "OFF" : "LP12");
            p = put_u(t, pat.f_vel); *p++ = '%'; *p = 0; kx(1, "VEL", t, pat.f_vel * 99u / 100u);
            k0(2); k0(3);
            break;
        }
        {
            int32_t ev = amt99(pat.f_env);
            *put_hz(t, hz_of(16 * 256 + (int32_t)((uint32_t)pat.f_cut * 114u >> 8))) = 0;
            kx(0, "CUTOFF", t, q99(pat.f_cut));
            ku(1, "RES", q99(pat.f_res));
            *put_sgn(t, ev) = 0; kb(2, "ENV", t, ev);
            p = put_u(t, pat.f_key); *p++ = '%'; *p = 0; kx(3, "KEY", t, pat.f_key * 99u / 100u);
        }
        break;
    case 4:
        if (dmod == DMOD_TUR) {                 /* RADAR: SEARCH LENGTH OFFSET DIV (where it goes: MOD's, 2026-11-21) */
            uint32_t ch = SD_TUR_CHANGE(&pat), fl = SD_TUR_FLIP(&pat);
            if (!fl) *put(t, "LOCK") = 0;       /* (both ends locked: said, in red) */
            else { p = put_u(t, fl); *p++ = '%'; *p = 0; }   /* (the chance: up to 50 in the middle, down again) */
            kx(0, "SEARCH", t, ch * 99u / 100u);
            kn[0].hot = !fl;
            *put_u(t, SD_TUR_LEN(&pat)) = 0; kx(1, "LENGTH", t, (SD_TUR_LEN(&pat) - 2u) * 99u / 14u);
            p = put_sgn(t, pat.tur_off); *p++ = '%'; *p = 0; kb(2, "OFFSET", t, pat.tur_off * 99 / 100);
            kt(3, "DIV", SDIV_NAME[SD_TUR_DIV(&pat)]);   /* (its clock: ALGORITHM's before 2026-11-19, GLO + K2's
                                                   till 2026-11-21) */
        } else if (dmod >= SD_MODS && l == 1u) {   /* an LFO's MODE (the picture says it whole) */
            uint32_t m = lmode_of(pat.lfo_mode[dmod - SD_MODS]);   /* (CLOCK: FREE or SYNC; CYCLE: when it starts) */
            kbt(0, "CLOCK", m & 1u || m >= 6u ? "SYNC" : "FREE", m & 1u || m >= 6u ? 1u : 0u);
            kt(1, "CYCLE", LCYC_NAME[lcyc_of(m)]);
            k0(2); k0(3);
        } else if (dmod >= SD_MODS) {           /* an LFO: SHAPE RATE, DEST AMOUNT (its slot's) */
            uint32_t k = dmod - SD_MODS, s = lfo_slot(k);
            kt(0, "SHAPE", LSHAPE_NAME[pat.lfo_shape[k] % SD_LSHAPES]);
            *put_rate(t, k) = 0; kt(1, "RATE", t);
            kt(2, "DEST", s < SD_MODS ? DST_NAME[pat.mod[s].dst % SD_MDSTS] : "OFF");
            if (s < SD_MODS) { int32_t a = amt99(pat.mod[s].amt); *put_sgn(t, a) = 0; kb(3, "AMOUNT", t, a); }
            else kt(3, "AMOUNT", "-");
        } else {                                /* a slot */
            const SdModSlot *m = &pat.mod[dmod];
            int32_t a = amt99(m->amt);
            kt(0, "SOURCE", SRC_NAME[m->src % SD_MSRCS]);
            kt(1, "DEST", slot_dname(m));
            *put_sgn(t, a) = 0; kb(2, "AMOUNT", t, a);
            if (TG_IS(m->dst)) kt(3, "ON", m->amt < 0 ? "0" : "1");   /* (RADAR 1's own: on its 1s or 0s, AMOUNT's
                                                   sign) */
            else if (m->dst >= SD_MDSTS) k0(3); /* (RADAR's own three: no OFFSET, a 1 is a 1) */
            else {
                p = put_sgn(t, pat.mod_off[dmod]); *p++ = '%'; *p = 0;   /* OFFSET: + up only, - down only (2026-11-19) */
                kb(3, "OFFSET", t, pat.mod_off[dmod] * 99 / 100);
            }
        }
        break;
    case PAGE_ARP:
        if (l == 1u) { kbt(0, "HOLD", set.ahold ? "ON" : "OFF", set.ahold ? 1u : 0u); k0(1); k0(2); k0(3); break; }
        kt(0, "MODE", AMODE_NAME[set.amode % SD_AMODES]);
        kt(1, "RATE", SDIV_NAME[set.adiv % SD_SDIVS]);
        *put_u(t, set.aoct) = 0; kx(2, "OCTAVES", t, (set.aoct - 1u) * 33u);
        p = put_u(t, set.agate); *p++ = '%'; *p = 0; kx(3, "GATE", t, set.agate * 99u / 100u);
        break;
    case PAGE_ORB: {
        uint32_t s = orb_cur % SD_ORB_STEPS, k;
        if (orb_setup) {                        /* SETUP: LENGTH, END, GLIDE (the lanes that glide), MACROS (theirs:
                                                   a button, lit where they glide) */
            static const char *const EN[SD_OES] = {"LOOP", "STOP"}, *const GL[4] = {"JUMP", "LANE 1", "LANE 2", "BOTH"};
            *put_u(t, SD_ORB_LEN(&orbp)) = 0; kx(0, "LENGTH", t, (SD_ORB_LEN(&orbp) - 1u) * 99u / (SD_ORB_STEPS - 1u));
            kt(1, "END", EN[orbp.end % SD_OES]);
            kt(2, "GLIDE", GL[orbp.glide & 3u]);   /* (the lanes that glide) */
            kbt(3, "MACROS", orbp.glide & SD_OG_MAC ? "GLIDE" : "JUMP", orbp.glide & SD_OG_MAC ? 1u : 0u);
        } else if (orb_more) {                  /* MACRO: the step's macros */
            static const char *const MN[4] = {"MUTATE", "MOVE", "BREAK", "CUTOFF"};
            for (k = 0; k < 4u; k++) { *put_sgn(t, orbp.mac[k][s]) = 0; kb(k, MN[k], t, orbp.mac[k][s]); }
        } else if (l == LO_PITCH) {             /* PITCH: XPOS A, XPOS B, the LOOP's, the FREEZE's (semitones) */
            *put_xp(t, orbp.xa[s]) = 0; kb(0, "XPOS A", t, orbp.xa[s] * 99 / 24);
            *put_xp(t, orbp.xb[s]) = 0; kb(1, "XPOS B", t, orbp.xb[s] * 99 / 24);
            for (k = 0; k < 2u; k++) {
                int32_t v = sd_orb_tail(&orbp, k, s);
                if (v) *put_sgn(t, v) = 0; else *put(t, "0") = 0;
                kb(2u + k, k ? "FREEZE" : "LOOP", t, v * 99 / 12);
            }
        } else if (l == LO_LANE) {              /* LANE: DEST 1, LANE 1 (its amount at the step), DEST 2, LANE 2 */
            for (k = 0; k < 2u; k++) {
                kt(2u * k, k ? "DEST 2" : "DEST 1", orbp.dst[k] ? DST_NAME[orbp.dst[k] % SD_MDSTS] : "NONE");
                p = put_sgn(t, orbp.lane[k][s]); *p++ = '%'; *p = 0;
                if (!orbp.dst[k]) kt(2u * k + 1u, k ? "LANE 2" : "LANE 1", "--");
                else kb(2u * k + 1u, k ? "LANE 2" : "LANE 1", t, orbp.lane[k][s] * 99 / 100);
            }
        } else {                                /* the step: SEQ A, SEQ B, DRUMS, LOOPS */
            uint32_t dv = orbp.drm[s];
            for (k = 0; k < 2u; k++) {
                if (orbp.pat[k][s]) *put_place(t, 2u, orbp.pat[k][s] - 1u) = 0; else *put(t, "--") = 0;
                kt(k, k ? "SEQ B" : "SEQ A", t);
            }
            if (dv == SD_DR_SONG_OFF) *put(t, "OFF") = 0;
            else if (dv && dv <= UDRUMS) *put_place(t, 4u, dv - 1u) = 0;
            else *put(t, "--") = 0;
            if (SD_DRUMS) kt(2, "DRUMS", t); else k0(2);
            *put_u(t, SD_ORB_LOOPS(&orbp, s)) = 0; kx(3, "LOOPS", t, (SD_ORB_LOOPS(&orbp, s) - 1u) * 99u / 7u);
        }
        break;
    }
    case PAGE_DRUM: {                           /* the drum's four, TONE its second four; MORE the step's ROLL and PROB;
                                                   MIX the four's levels; KIT its PAN, DUCK, ACCENT, the fourth's
                                                   ENGINE; TOOLS its row's */
        uint32_t d = dr_cur % SD_DRS, k, pc = set.dperc % SD_PCS;
        if (l == LK_TOOLS) {
            kbt(0, "RANDOM", "ROLL", dr_roll_at && fm1_ms - dr_roll_at < 300u ? 1u : 0u);
            kbt(1, "UNDO", "BACK", dr_back_at && fm1_ms - dr_back_at < 300u ? 1u : dr_has_undo ? 2u : 0u);
            kbt(2, "CLEAR", "ROW", dr_clr_at ? 1u : 0u);
            k0(3);
            break;
        }
        if (dr_more) {
            uint32_t s2 = dr_step % SD_DR_STEPS, r = SD_DR_ROLL(&set.dmore, d, s2), pb = SD_DR_PROB(&set.dmore, d, s2);
            p = t; *p++ = 'x'; *p++ = (char)('1' + r); *p = 0; kx(0, "ROLL", t, r * 33u);
            p = put_u(t, 100u - pb * 25u); *p++ = '%'; *p = 0; kx(1, "PROB", t, 99u - pb * 33u);
            k0(2); k0(3);
            break;
        }
        if (dr_mix) {                           /* the four's levels, each its name */
            for (k = 0; k < 4u; k++) {
                const SdDrumP *dp = &set.dkit.d[k];
                p = put_u(t, dp->level); *p++ = '%'; *p = 0;
                kx(k, sd_dr_name(k, pc), t, dp->level * 99u / 100u);
            }
            break;
        }
        if (l == LK_KIT) {                      /* the drum's PAN; the kit's DUCK, ACCENT; the fourth's ENGINE */
            int32_t pn = set.dkit.d[d].pan;
            p = t;
            if (!pn) p = put(p, "MID"); else { *p++ = pn < 0 ? 'L' : 'R'; p = put_u(p, (uint32_t)(pn < 0 ? -pn : pn)); }
            *p = 0;
            kb(0, "PAN", t, pn * 99 / 60);
            p = put_u(t, set.dkit.duck); *p++ = '%'; *p = 0; kx(1, "DUCK", t, set.dkit.duck * 99u / 100u);
            p = put_u(t, set.dkit.accent); *p++ = '%'; *p = 0; kx(2, "ACCENT", t, set.dkit.accent * 99u / 100u);
            kt(3, "ENGINE", SD_PC_NAME[pc]);
            break;
        }
        for (k = 0; k < 4u; k++) {
            uint32_t i = k + (l == LK_TONE ? 4u : 0u);
            int32_t v = set.dkit.d[d].v[i], lo = sd_dr_min(d, pc, i), hi = sd_dr_max(d, pc, i);
            uint32_t bar = (uint32_t)((v - lo) * 99 / (hi > lo ? hi - lo : 1));
            const char *nm = sd_dr_pname(d, pc, i);
            switch (sd_dr_kind(d, pc, i)) {
            case SD_DK_NOTE: {                  /* (Hz: "46 HZ", "9.4k") */
                uint32_t hz = hz_of(v * 256);
                p = put_hz(t, hz);
                *(hz < 1000u ? put(p, " HZ") : p) = 0;
                kx(k, nm, t, bar);
                break;
            }
            case SD_DK_TIME: *put_ms(t, (uint32_t)v) = 0; kx(k, nm, t, bar); break;
            case SD_DK_SEMI: p = put_u(t, (uint32_t)v); *put(p, " ST") = 0; kx(k, nm, t, bar); break;
            case SD_DK_MAC: p = put_sgn(t, v); *p++ = '%'; *p = 0; kb(k, nm, t, v * 99 / 100); break;
            case SD_DK_N: *put_u(t, (uint32_t)v) = 0; kx(k, nm, t, bar); break;
            case SD_DK_DIV: kx(k, nm, SDIV_NAME[SD_SDIV_ORDER[(uint32_t)v % SD_SDIVS]], bar); break;
            case SD_DK_SST: p = put_sgn(t, v); *put(p, " ST") = 0; kb(k, nm, t, hi > 0 ? v * 99 / hi : 0); break;
            default: p = put_u(t, (uint32_t)v); *p++ = '%'; *p = 0; kx(k, nm, t, bar); break;
            }
        }
        break;
    }
    default:                                    /* SEQ A, B: the layer's (ALGORITHM) */
        if (seq_layer == SL_TOOLS) {            /* RANDOM UNDO CLEAR DUB, each a button: lit as it acts, UNDO's dim
                                                   with nothing to take back, CLEAR's while it asks, DUB's ON lit */
            kbt(0, "RANDOM", "ROLL", seq_roll_at && fm1_ms - seq_roll_at < 300u ? 1u : 0u);
            kbt(1, "UNDO", "BACK", seq_back_at && fm1_ms - seq_back_at < 300u ? 1u : seq_has_undo ? 2u : 0u);
            kbt(2, "CLEAR", "CLEAR", seq_clr_at ? 1u : 0u);
            kbt(3, "DUB", seq_dub ? "ON" : "OFF", seq_dub ? 1u : 0u);
        } else if (seq_layer == SL_TRACK) {
            seq_l2_knobs();
        } else if (seq_segmode) {
            uint8_t bd[SD_SEGS + 1u];
            uint32_t sc = sd_seg_bounds(&seqp, bd), g = seq_dseg < sc ? seq_dseg : sc - 1u;
            uint32_t r = seqp.seg_rep[g] < 1u ? 1u : seqp.seg_rep[g];
            *put_u(t, sc) = 0; kt(0, "PARTS", t);
            p = put(t, "x"); *put_u(p, r) = 0; kx(1, "REPEAT", t, (r - 1u) * 33u);
            kt(2, "ORDER", (seqp.seg_skip >> g & 1u) ? "SKIP" : SORDER_NAME[seqp.seg_order[g] % SD_SORDERS]);
            *put_u(t, bd[g + 1u] - bd[g]) = 0; kt(3, "LENGTH", t);
        } else {
            const SdStep *st = &seqp.step[seq_cur];
            if (seq_more) {
                if (st->type == SD_ST_SLIDE) kt(0, "SLIDE", SLIDE_NAME[SD_SLIDE_T(st->ratchet) - 1u]);
                else { p = put(t, "x"); *put_u(p, st->ratchet < 1u ? 1u : st->ratchet) = 0; kt(0, "RATCHET", t); }
                p = put_u(t, st->prob); *p++ = '%'; *p = 0; kx(1, "PROB", t, st->prob * 99u / 100u);
                {                               /* the note heard here: where it starts, how many steps it holds */
                    int32_t s0 = sd_note_at(&seqp, seq_cur);
                    if (s0 < 0) { k0(2); k0(3); }
                    else {
                        uint32_t l = sd_note_len(&seqp, (uint32_t)s0);
                        *put_u(put(t, "STEP "), (uint32_t)s0 + 1u) = 0; kt(2, "MOVE", t);
                        p = put_u(t, l); *put(p, l == 1u ? " STEP" : " STEPS") = 0; kt(3, "LENGTH", t);
                    }
                }
            } else {                            /* (a TIE: the note it holds, seq_heard) */
                const SdStep *hs = st->type == SD_ST_TIE ? seq_heard(&seqp, seq_cur) : st;
                *put_note(t, hs->note) = 0; kt(0, "NOTE", t);
                *put_u(t, hs->vel) = 0; kx(1, "VEL", t, hs->vel * 99u / 127u);
                p = put_u(t, st->gate); *p++ = '%'; *p = 0; kx(2, "GATE", t, st->gate * 99u / SD_GATE_MAX);
                kt(3, "TYPE", STYPE_NAME[st->type % SD_STYPES]);
            }
        }
        break;
    }
}
static uint32_t page_off(void);
static void draw_knobs(void)
{
    uint32_t i, off = page_off();
    uint16_t cn = off ? C_OFF : C_ICE_D, cv = off ? C_ICE_D : C_ICE, cb = off ? C_OFF : C_ICE;
    knobs_of();
    cv_begin(240, H_KNOB, C_BG);
    for (i = 0; i < 4u; i++) {
        const Kn *k = &kn[i];
        int32_t x = (int32_t)i * 60, cx = x + 30;
        if (i) cv_line(x, 18, x, (int32_t)H_KNOB - 4, C_GRID);   /* (under the names: seven letters fit) */
        if (!k->nm) continue;                  /* (no knob there: nothing) */
        cv_text(cx - text_w(&FONT_S, k->nm) / 2, 0, &FONT_S, k->nm, cn);
        if (k->kind == KN_BTN) { btn(x + 4, 17, 52, k->v, (uint32_t)k->bar); continue; }   /* (TOOLS: a button) */
        cv_text(cx - text_w(&FONT_S, k->v) / 2, 17, &FONT_S, k->v, k->hot ? C_RED : cv);
        if (k->kind == KN_UNI || k->kind == KN_BI) {   /* a printed scale (eleven ticks, the middle and the ends
                                                   long), filled to the value, its pointer there */
            int32_t t, px = k->kind == KN_UNI ? cx - 22 + k->bar * 44 / 99 : cx + k->bar * 22 / 99;
            for (t = 0; t <= 10; t++) cv_rect(cx - 22 + t * 44 / 10, 42, 1, t % 5 ? 2 : 4, C_ICE_D);
            cv_rect(cx - 22, 45, 45, 1, C_GRID);
            if (k->kind == KN_BI) cv_rect(k->bar < 0 ? px : cx, 44, k->bar < 0 ? cx - px : px - cx, 2, cb);
            else cv_rect(cx - 22, 44, px - cx + 22, 2, cb);
            cv_box(px - 1, 37, 3, 9, off ? C_OFF : C_ICE);
        }
    }
}

/* ---- the pictures, 240 x 104 (H_PIC) */
static int32_t gx(int32_t m256, int32_t x0, int32_t w)   /* MIDI x 256 -> a graph's column, the Reactor's range */
{
    return x0 + (m256 - SD_R_LO * 256) * w / ((SD_R_HI - SD_R_LO) * 256);
}
static void adsr(const uint8_t *ev, int32_t x0, int32_t w, int32_t y0, int32_t h, uint16_t c)   /* a shape */
{
    int32_t yb = y0 + h, top = y0 + 2, sus = yb - (int32_t)(ev[2] * (uint32_t)(yb - top) / 127u), q = w / 4;
    int32_t wa = 2 + (int32_t)ev[0] * q / 127, wd = 2 + (int32_t)ev[1] * q / 127, wr = 2 + (int32_t)ev[3] * q / 127;
    int32_t xs = x0 + wa + wd, xr = x0 + w - 1 - wr;
    cv_line(x0, yb, x0 + wa, top, c);
    cv_line(x0 + wa, top, xs, sus, c);
    cv_line(xs, sus, xr, sus, c);
    cv_line(xr, sus, x0 + w - 1, yb, c);
}

static void pic_nav(void)                       /* the field as a diamond, the banks' mix in it; the bank's oscillators */
{
    int32_t cx = 40, cy = 50, R = 28, qx[4], qy[4];
    uint32_t k;
    char s[12], *p;
    cv_line(cx, cy - R, cx + R, cy, C_ICE_D); cv_line(cx + R, cy, cx, cy + R, C_ICE_D);
    cv_line(cx, cy + R, cx - R, cy, C_ICE_D); cv_line(cx - R, cy, cx, cy - R, C_ICE_D);
    cv_line(cx - R, cy, cx + R, cy, C_GRID); cv_line(cx, cy - R, cx, cy + R, C_GRID);
    if (eng.mode_on != SD_MODE_POLY) {          /* each bank's weight along its axis: A up, B right, C down, D left */
        int32_t wa = eng.w[0] * R >> 15, wb = eng.w[1] * R >> 15, wc = eng.w[2] * R >> 15, wd = eng.w[3] * R >> 15;
        qx[0] = cx; qy[0] = cy - wa; qx[1] = cx + wb; qy[1] = cy; qx[2] = cx; qy[2] = cy + wc; qx[3] = cx - wd; qy[3] = cy;
        cv_quad_fill(qx, qy, C_FILL);
        for (k = 0; k < 4u; k++) cv_line(qx[k], qy[k], qx[(k + 1u) & 3u], qy[(k + 1u) & 3u], C_RED);
        {                                       /* the nav's point: x + y - 1 across, x - y down */
            int32_t px = cx + (((int32_t)eng.nav_x + (int32_t)eng.nav_y - 65535) * R >> 16);
            int32_t py = cy + (((int32_t)eng.nav_x - (int32_t)eng.nav_y) * R >> 16);
            cv_rect(px - 2, py - 2, 5, 5, C_ICE);
        }
    }
    cv_text(cx - 4, cy - R - 16, &FONT_S, "A", C_ICE);
    cv_text(cx + R + 3, cy - 8, &FONT_S, "B", C_ICE);
    cv_text(cx - 4, cy + R - 1, &FONT_S, "C", C_ICE);
    cv_text(cx - R - 11, cy - 8, &FONT_S, "D", C_ICE);
    for (k = 0; k < SD_OPS; k++) {              /* the edited bank: role and harmonic, level, fine tune */
        const SdOscPatch *o = &pat.bank[ebank].osc[k];
        int32_t x = 86 + (int32_t)k * 38;
        uint32_t seg, n = (o->level * 6u + 32767u) / 65535u;
        p = s;
        *p++ = o->role == SD_ROLE_S ? 'S' : o->role == SD_ROLE_P ? 'P' : 'C';
        *p = 0;
        cv_text(x, 6, &FONT_S, s, o->role == SD_ROLE_C ? C_ICE : C_ICE_D);
        p = put(s, "x"); *put_u(p, o->harm) = 0;
        cv_text(x + 9, 6, &FONT_S, s, C_ICE);
        for (seg = 0; seg < 6u; seg++)
            cv_box(x + (int32_t)seg * 5, 28, 4, 22, seg >= n ? C_GRID : o->role == SD_ROLE_C ? C_ICE : C_ICE_D);
        p = s;
        if (o->fine) p = put_sgn(p, o->fine); else *p++ = '0';
        *p = 0;
        cv_text(x, 56, &FONT_S, s, C_ICE_D);
        if (k) cv_line(x - 4, 6, x - 4, 74, C_GRID);
    }
    p = put(s, "BANK "); *p++ = (char)('A' + ebank); *p = 0;
    cv_text(86, 84, &FONT_S, s, C_ICE_D);
}

static void pic_chain(void)                     /* MACROS (2026-11-21, B of two; 2026-11-22 as lit panel buttons, the
                                                   user took the sheet): the chain in its order on top (the black keys
                                                   B1..B5 with GLO), a line between them; the players under it in the
                                                   knobs' columns (the white keys F3..E4); lit on, blinking in at the
                                                   one, an outline off */
{
    static const char *const NM[HOME_N] = {"FILT", "REAC", "DIST", "TIME", "SPAC", "ARP", "LOOP", "SEQ A", "SEQ B",
                                           "ORBIT", "RADAR"
#if SD_DRUMS
                                           , "DRUMS"
#endif
    };
    uint32_t k;
    for (k = 0; k < HOME_N; k++) {
        uint32_t sw = home_at(k, 0), lit = sw == SW_ON || sw == SW_LOOP || (sw == SW_WAIT && (fm1_ms / 200u) & 1u);
        if (k < 5u) {
            int32_t x = 2 + (int32_t)k * 48;
            btn(x, 8, 42, NM[k], lit);
            if (k) cv_line(x - 5, 16, x - 1, 16, C_ICE_D);
        } else btn(4 + (int32_t)((k - 5u) & 3u) * 60, k < 9u ? 46 : 72, 52, NM[k], lit);
    }
    cv_line(4, 36, 236, 36, C_GRID);
}

static void pic_reactor(void)                     /* the filters now as peaks across the width; under them the six
                                                   stages as a piano roll: a column each, its four filters notes at
                                                   their pitch, in turn, the one playing red */
{
    enum { X0 = 6, W = 228, SH = 52, SY = 104 - SH, Y0 = 0, H = 104 - SH - 8, SW = W / 6 };
    int32_t x, k, wpx;
    int16_t col[W];
    uint32_t i, ns = SD_R_NST(&pat), pos = eng.r.shown_pos, at = pos >> 8, nx = at + 1u >= ns ? 0u : at + 1u, f = pos & 255u;
    uint32_t dom = f < 128u ? at : nx, sec = f ? (f < 128u ? nx : at) : 99u;
    uint16_t on = fx_on(SD_FX_REACTOR), live = on && !r_stages;   /* (live: the engine's filters now) */
    if (!react_set(&pat) && !r_stages) {        /* never set (the Reactor off in this patch): EDIT sets it */
        cv_text(X0, 34, &FONT_S, "NO STAGES SET YET", C_ICE_D);
        cv_text(X0, 54, &FONT_S, "EDIT: ON, SIX VOWELS", C_ICE_D);
        return;
    }
    if (!on || r_stages) {                      /* (off: the patch's stage at POS, not the engine's last; STAGES: the
                                                   one edited) */
        pos = r_stages ? rst_cur % 6u * 256u : (uint32_t)pat.r_pos * ns / 6u; at = pos >> 8; nx = at + 1u >= ns ? 0u : at + 1u;
        f = pos & 255u;
        dom = f < 128u ? at : nx; sec = f ? (f < 128u ? nx : at) : 99u;
    }
    wpx = 1 + (int32_t)((uint32_t)(SD_RK[pat.r_res >> 9] >> 8) * 36u >> 16);   /* k 2 -> 37 px, k 0.01 -> 1 (on the
                                                   228 px axis: as wide in pitch as on the old 140) */
    for (x = 0; x < W; x++) col[x] = 0;
    for (k = 0; k < 4; k++) {
        int32_t xc = gx(live ? eng.r.pitch[k] >> 8 : pat.stage[dom % 6u].pitch[k], X0, W);
        int32_t h = (live ? eng.r.level[k] >> 8 : (int32_t)pat.stage[dom % 6u].level[k]) * (H - 4) / 65536;
        for (x = 0; x < W; x++) {
            int32_t d = X0 + x - xc, y = h * wpx * wpx / (wpx * wpx + d * d);
            if (y > col[x]) col[x] = (int16_t)y;
        }
    }
    cv_line(X0, Y0 + H, X0 + W - 1, Y0 + H, C_GRID);
    for (x = 0; x < W; x++)
        if (col[x]) {
            cv_rect(X0 + x, Y0 + H - col[x], 1, col[x], on ? C_FILL : C_GRID);
            cv_pset(X0 + x, Y0 + H - col[x], on ? C_RED : C_ICE_D);
        }
    for (i = 0; i < 6u; i++) {                  /* the stages: each its four filters as notes, in turn, high as
                                                   their pitch (a piano roll's rows); past the ones in use only a
                                                   dotted frame */
        int32_t sx = X0 + (int32_t)i * SW;
        uint16_t c = i == dom ? C_RED : i == sec ? C_RED_D : C_GRID;
        if (i >= ns) {
            int32_t d;
            for (d = 0; d < SW - 3; d += 4) { cv_pset(sx + d, SY, C_GRID); cv_pset(sx + d, SY + SH - 1, C_GRID); }
            for (d = 0; d < SH; d += 4) { cv_pset(sx, SY + d, C_GRID); cv_pset(sx + SW - 4, SY + d, C_GRID); }
            continue;
        }
        cv_frame(sx, SY, SW - 3, SH, c);
        for (k = 0; k < 4; k++) {
            int32_t y = SY + SH - 5 - ((int32_t)pat.stage[i].pitch[k] - SD_R_LO * 256) * (SH - 10) / ((SD_R_HI - SD_R_LO) * 256);
            cv_rect(sx + 4 + k * 7, y, 6, 2, pat.stage[i].level[k] ? (i == dom ? C_ICE : C_ICE_D) : C_GRID);
        }
    }
}

static void pic_dest(void)                      /* the bands on a log-frequency axis, as tall as their drive: each
                                                   its name (its number where it does not fit), the one ALGORITHM picks red,
                                                   the splits in Hz under it */
{
    enum { X0 = 6, W = 228, Y0 = 18, H = 66 };
    uint32_t i, k = d_loop ? 4u : dband;        /* (LOOP: no band red) */
    int32_t xl = -100;
    char t[16];
    if (!dest_set(&pat)) {                      /* never set (DISTROY off in this patch): EDIT sets it */
        cv_text(X0, 34, &FONT_S, "NO BANDS SET YET", C_ICE_D);
        cv_text(X0, 54, &FONT_S, "EDIT: ON, FOUR BANDS", C_ICE_D);
        return;
    }
    cv_line(X0, Y0 + H, X0 + W - 1, Y0 + H, C_GRID);
    for (i = 0; i < 4u; i++) {
        int32_t x0 = i ? gx(pat.d_split[i - 1u], X0, W) + 1 : X0, x1 = i < 3u ? gx(pat.d_split[i], X0, W) - 1 : X0 + W - 1;
        int32_t h = 3 + (int32_t)pat.d_gain[i] * (H - 4) / 65535, room = x1 - x0 - 3;
        uint16_t c = i == k ? C_RED : C_ICE_D;
        if (x1 <= x0) continue;
        cv_rect(x0, Y0 + H - h, x1 - x0 + 1, h, i == k ? C_RED_D : C_FILL);
        cv_line(x0, Y0 + H - h, x1, Y0 + H - h, c);
        *put(t, BAND_NAME[i]) = 0;               /* (its type: K1 for the picked one) */
        if (text_w(&FONT_S, t) > room + 2) { t[0] = (char)('1' + i); t[1] = 0; }   /* (the number ALGORITHM says) */
        if (text_w(&FONT_S, t) <= room + 2) cv_text((x0 + x1 + 1 - text_w(&FONT_S, t)) / 2, 0, &FONT_S, t, c);   /* (in
                                                   its band's middle, as a section's legend) */
    }
    for (i = 0; i < 3u; i++) {                  /* the splits, their Hz under the axis where they do not touch */
        int32_t xs = gx(pat.d_split[i], X0, W), w;
        cv_line(xs, Y0, xs, Y0 + H, C_ICE_D);
        *put_hz(t, hz_of(pat.d_split[i])) = 0;
        w = text_w(&FONT_S, t);
        if (xs - w / 2 > xl + 4 && xs + w / 2 < X0 + W) { cv_text(xs - w / 2, Y0 + H + 2, &FONT_S, t, C_ICE_D); xl = xs + w / 2; }
    }
    if (d_loop) {                               /* LOOP's layer: its tag over the bands */
        cv_box(X0 + W - 44, 0, 44, 16, C_RED);
        cv_text(X0 + W - 40, 1, &FONT_S, "LOOP", C_BG);
    }
}

static void pic_space(void)                     /* SPACE: the plate's tail (the room's, on one core) */
{
    enum { X0 = 6, W = 228, Y0 = 2, H = 98 };
    uint32_t tail;
    int32_t x, h;
    cv_line(X0, Y0 + H, X0 + W - 1, Y0 + H, C_GRID);
    tail = sp_frz ? 0xFFFFFFFFu : SD_PL_T60[pat.s_decay >> 9];   /* T60, ms (FREEZE: for ever) */
    h = (int32_t)(pat.s_rlevel * (uint32_t)(H - 6) >> 16);
    for (x = 0; x < W; x++) {
        int32_t tt = x * 4000 / W, y = tail == 0xFFFFFFFFu ? h : h - (int32_t)((int64_t)h * tt / (int32_t)tail);
        if (y > 0) cv_rect(X0 + x, Y0 + H - y, 1, y, C_FILL);
        if (y > 0) cv_rect(X0 + x, Y0 + H - y, 1, 1, C_RED_D);
    }
    if (sp_frz) {                               /* FREEZE: its tag over the tail */
        cv_box(X0 + W - 60, 0, 60, 16, C_RED);
        cv_text(X0 + W - 56, 1, &FONT_S, "FREEZE", C_BG);
    }
    if (eng.sp_room) {                          /* one core: the room plays in the plate's place (no FREEZE there) */
        cv_box(X0, 0, 44, 16, C_ICE_D);
        cv_text(X0 + 4, 1, &FONT_S, "ROOM", C_BG);
    }
}

static void pic_time(void)                      /* TIME: the echoes falling by FDBK along 2 s */
{
    enum { X0 = 6, W = 228, Y0 = 2, H = 98 };
    uint32_t i, ms;
    int32_t x, h, fb;
    cv_line(X0, Y0 + H, X0 + W - 1, Y0 + H, C_GRID);
    if (pat.s_mode & SD_SP_SYNC)                /* as the engine plays it: the tempo heard, a halving when long */
        ms = sd_space_sync_q8(eng.tempo_q8, sd_sp_div(pat.s_time), 0, dl_lim()) / 11290u;   /* (samples x 256:
                                                   44.1 x 256) */
    else
        ms = sd_sp_ms(pat.s_time);
    fb = (int32_t)((pat.s_fb * 31130u) >> 16);
    h = (int32_t)(pat.s_dlevel * (uint32_t)(H - 6) >> 16);
    if (dl_lim() == SD_SP_LIM2) {               /* the looper armed: it has half the line, the delay 0.5 s at most */
        cv_text(X0 + 2, Y0, &FONT_S, "LOOPER ON: UP TO 0.5 S", C_ICE_D);
        h = (int32_t)(pat.s_dlevel * (uint32_t)(H - 20) >> 16);   /* (the echoes under it) */
    }
    for (i = 1; ms && i * ms < 2000u && h > 1; i++) {   /* (ping-pong: in pairs, left red and right ice, x FDBK^2) */
        uint32_t pp = (pat.s_mode & SD_SP_PP) != 0u;
        x = X0 + (int32_t)(i * ms * W / 2000u);
        cv_line(x, Y0 + H, x, Y0 + H - h, pp && (i & 1u) == 0u ? C_ICE : C_RED);
        if (!pp) h = (h * fb) >> 15;
        else if (!(i & 1u)) h = (((h * fb) >> 15) * fb) >> 15;
    }
}

/* LOOPER (2026-11-17, the user: "how a loop is taken must show more: now it feels random and off the grid, though it
 * locks"): a tape running past its head on the right, what it records drawn as it comes (each column the loudest of
 * its samples, read from the looper's memory a frame's new ones at a time), the grid it locks to over it (while the
 * tracks play, synced: the master's bars bright, the beats dim), the LENGTH LOOP takes framed with its ends on that
 * grid (the last whole one; the one coming dim). LOOP: the tape stops, the loop it holds red, its head running
 * through. Stopped: no grid, LOOP takes the last LENGTH where it is pressed, a second a mark */
#define LP_COLS 228u
#if defined(DINGHY_HOST) && (defined(__APPLE__) || defined(DINGHY_WEB))
static uint8_t lp_col[LP_COLS];                 /* each column's loudest, 0..54 (the strip's half height x 2) */
#else
static uint8_t lp_col[LP_COLS] __attribute__((section(".pool")));   /* (in the pool: the RAM's .bss is full) */
#endif
static uint32_t lp_cw, lp_cspc, lp_cn;          /* where the columns reach in the memory; samples a column; the ring */
static uint32_t lp_frz_grid;                    /* LOOP: the grid's time when the tape stopped (its lines stay) */
static uint32_t lp_col_peak(const SdLpMem *L, uint32_t k, uint32_t n)   /* the loudest of n samples from k */
{
    uint32_t i, m = 0, st = n > 64u ? n / 64u : 1u;   /* (64 a column at most: a long LENGTH stays light) */
    for (i = 0; i < n; i += st) {
        int32_t v = sd_lp_get(L, (k + i) % L->n);
        uint32_t a = (uint32_t)(v < 0 ? -v : v);
        if (a > m) m = a;
    }
    m = m * 54u / 16384u;                       /* (Q14: full scale the strip's height) */
    return m > 54u ? 54u : m;
}
static void pic_loop(void)
{
    enum { X0 = 6, W = 228, SY = 20, SH = 56, SM = SY + SH / 2 };
    SdLpMem L;
    char pd[16], *p;
    uint32_t n = sd_lp_total(&eng), w = eng.sp.lw, ln = (uint32_t)eng.sp.llen >> 8, span, spc, i, k;
    uint32_t sync = eng.seq_run && eng.seq_len_q8, looping = eng.sp.pon && lp_on;   /* (LENGTH synced always: the
                                                   grid while the tracks play) */
    uint16_t lc = lp_hold ? C_ICE_D : C_RED;    /* (the loop held, LOOPER off: dim, its head going round in time) */
    uint32_t bar = 16u * eng.seq_len_q8 / 512u, beat = 4u * eng.seq_len_q8 / 512u;   /* (half-rate samples) */
    uint32_t now = (looping ? lp_frz_grid : eng.grid_q8) / 512u;   /* the grid's time at the head, half-rate */
    L.a = eng.lpa; L.na = eng.lpa ? eng.lpa_n : 0u; L.b = eng.lpb; L.n = L.na + (eng.lpb ? eng.lpb_n : 0u);
    if (!looping) lp_frz_grid = eng.grid_q8;
    if (n < SD_LP_XF + 64u || L.n != n) {       /* no memory: what it would hold */
        cv_text(X0, SY + 12, &FONT_S, "NO LOOPER MEMORY", C_ICE_D);
        return;
    }
    if (ln < 64u) ln = 64u;
    span = 2u * ln;                             /* the tape: two LENGTHs, a bar at least while synced; inside what it
                                                   holds */
    if (sync && span < bar) span = bar;
    if (span > n - SD_LP_XF - 64u) span = n - SD_LP_XF - 64u;
    spc = span / LP_COLS ? span / LP_COLS : 1u;
    if (w >= n) w = 0;
    if (spc != lp_cspc || L.n != lp_cn || (w + n - lp_cw) % n > span) {   /* a new scale, or far behind: anew */
        lp_cspc = spc; lp_cn = L.n;
        lp_cw = (w + n - LP_COLS * spc % n) % n;
        for (i = 0; i < LP_COLS; i++) lp_col[i] = lp_arm || lp_hold ? (uint8_t)lp_col_peak(&L, (lp_cw + i * spc) % n, spc) : 0u;
        lp_cw = (lp_cw + LP_COLS * spc) % n;
    }
    for (k = 0; k < 16u && (w + n - lp_cw) % n >= spc && (w + n - lp_cw) % n < n / 2u; k++) {   /* the new ones */
        for (i = 0; i + 1u < LP_COLS; i++) lp_col[i] = lp_col[i + 1u];
        lp_col[LP_COLS - 1u] = lp_arm || lp_hold ? (uint8_t)lp_col_peak(&L, lp_cw, spc) : 0u;
        lp_cw = (lp_cw + spc) % n;
    }
    {   /* the window LOOP takes (or holds): its ends on the tape, columns back from the head */
        int32_t xe, xs, xn = -1, q;
        if (looping) {                          /* the loop it holds: ending at lbase, as long as it was taken */
            uint32_t pl = eng.sp.pln >> 8 ? eng.sp.pln >> 8 : ln, back = (lp_cw + n - eng.sp.lbase % n) % n;
            xe = X0 + W - 1 - (int32_t)(back / spc);
            xs = xe - (int32_t)(pl / spc);
        } else if (sync) {                      /* the last whole one on its grid (the loop's lines, a bar at most) */
            uint32_t g = ln < bar ? ln : bar, since = g ? now % g : 0u;
            xe = X0 + W - 1 - (int32_t)(since / spc);
            xs = xe - (int32_t)(ln / spc);
            xn = xe;                            /* (the one coming: from there to the head) */
        } else {                                /* free: the last LENGTH, where it is pressed */
            xe = X0 + W - 1;
            xs = xe - (int32_t)(ln / spc);
        }
        for (i = 0; i < LP_COLS; i++) {         /* the tape: what it recorded, the window bright */
            int32_t x = X0 + (int32_t)i, a = lp_col[i] / 2;
            uint16_t c = x >= xs && x <= xe ? (looping ? lc : lp_arm ? C_ICE : C_ICE_D) : xn >= 0 && x > xn ? C_ICE_D : C_GRID;
            if (a) cv_line(x, SM - a, x, SM + a, c);
            else cv_pset(x, SM, c);
        }
        if (sync) {                             /* the grid it locks to: the bars bright, the beats dim */
            for (q = 0; q < 64; q++) {
                uint32_t t = now - now % beat - (uint32_t)q * beat;   /* (half-rate samples, back from the head) */
                int32_t x = X0 + W - 1 - (int32_t)((now - t) / spc);
                if (x < X0 || (uint32_t)q * beat > now + bar * 64u) break;
                cv_line(x, SY - 2, x, SY + 3, t % bar == 0u ? C_ICE : C_ICE_D);
                cv_line(x, SY + SH - 3, x, SY + SH + 2, t % bar == 0u ? C_ICE : C_ICE_D);
                if (t % bar == 0u) cv_line(x, SY + 4, x, SY + SH - 4, C_GRID);
            }
        }
        if (xs < X0) xs = X0;
        cv_frame(xs, SY - 1, xe - xs + 1 > 4 ? xe - xs + 1 : 4, SH + 2, looping ? lc : lp_arm ? C_ICE : C_GRID);
        if (looping) {                          /* its head, through the loop */
            int32_t xh = xs + (int32_t)((uint32_t)(xe - xs) * eng.lp_at / 4096u);
            cv_rect(xh, SY - 3, 2, SH + 6, lp_hold ? C_ICE : C_GLINT);
        } else if (lp_arm) {                    /* the record head, on the right */
            cv_rect(X0 + W - 2, SY - 3, 2, SH + 6, C_RED);
        }
    }
    /* under it: the scale (bars while synced, else seconds back from the head), the tags */
    cv_line(X0, SY + SH + 6, X0 + W - 1, SY + SH + 6, C_ICE_D);
    p = put(pd, sync ? "BARS" : "S");
    *p = 0;
    if (!sync) {
        uint32_t cs = span * 100u / 22050u, sec;   /* (centiseconds the tape holds) */
        for (sec = 1u; sec * 100u <= cs; sec++) {
            int32_t x = X0 + W - 1 - (int32_t)(sec * 22050u / spc);
            if (x < X0) break;
            cv_line(x, SY + SH + 3, x, SY + SH + 9, C_ICE_D);
            *put_u(pd, sec) = 0;
            cv_text(x - 4, SY + SH + 10, &FONT_S, pd, C_ICE_D);
        }
    }
    p = put_len(pd); *p = 0;
    cv_text(X0, SY + SH + 10, &FONT_S, pd, C_ICE_D);   /* (LENGTH, as its knob says it) */
    if (!lp_lvl && lp_arm) cv_text(X0 + 60, SY + SH + 10, &FONT_S, "L-LEVEL 0", C_RED);   /* (it records, silent) */
    if (lp_on) {
        cv_box(X0 + W - 44, 0, 44, 16, lp_arm ? C_RED : C_ICE_D);
        cv_text(X0 + W - 40, 1, &FONT_S, "LOOP", C_BG);
    }
    if (lp_rev) {                               /* REVERSE: its tag, left of LOOP's */
        cv_box(X0 + W - 84, 0, 36, 16, lp_on ? C_RED : C_ICE_D);
        cv_text(X0 + W - 80, 1, &FONT_S, "REV", C_BG);
    }
    if (lp_arm && !looping) { cv_box(X0, 0, 36, 16, C_RED); cv_text(X0 + 4, 1, &FONT_S, "REC", C_BG); }
    else if (lp_hold) { cv_box(X0, 0, 44, 16, C_ICE_D); cv_text(X0 + 4, 1, &FONT_S, "KEPT", C_BG); }   /* (off, its loop
                                                   kept: EDIT brings it back) */
}

static void pic_env(void)                       /* ENV 1 (the level) and ENV 2 (the FM's amount), its layer brings 2 up */
{
    uint32_t l2 = pg_lay[PAGE_ENV] == 1u;
    cv_line(6, 100, 233, 100, C_GRID);
    if (l2) { adsr(&pat.e1_a, 6, 228, 20, 80, C_ICE_D); adsr(&pat.e2_a, 6, 228, 20, 80, C_RED); }
    else { adsr(&pat.e2_a, 6, 228, 20, 80, C_RED_D); adsr(&pat.e1_a, 6, 228, 20, 80, C_ICE); }
    cv_text(6, 0, &FONT_S, "ENV 1 LEVEL", l2 ? C_ICE_D : C_ICE);
    cv_text(234 - text_w(&FONT_S, "ENV 2 FM"), 0, &FONT_S, "ENV 2 FM", l2 ? C_RED : C_RED_D);
}

static void pic_filt(void)                      /* the response; ENV 3 (its layer red) on the right */
{
    enum { X0 = 6, W = 150, Y0 = 2, H = 98 };
    int32_t c = 16 * 256 + (int32_t)((uint32_t)pat.f_cut * 114u >> 8), xc = gx(c, X0, W), peak = (int32_t)q99(pat.f_res) / 4;
    int32_t slope = pat.f_type == SD_FT_LP24 ? 24 : 12, xx, y0 = Y0 + 34, prev = y0;
    uint32_t on = fx_on(SD_FX_FILTER), l2 = lay_is(PAGE_FILT, LF_ENV);
    uint16_t col = on ? C_ICE : C_ICE_D;
    cv_line(X0, Y0 + H, X0 + W - 1, Y0 + H, C_GRID);
    for (xx = X0; xx < X0 + W; xx++) {          /* flat to the cutoff, RES's peak on it, then 12 or 24 dB an octave */
        int32_t m = SD_R_LO * 256 + (xx - X0) * ((SD_R_HI - SD_R_LO) * 256) / W, db, d = m - c, y, dd = d < 0 ? -d : d;
        db = d <= 0 ? 0 : -(slope * d) / (12 * 256);
        if (dd < 6 * 256) db += peak * (6 * 256 - dd) / (6 * 256);
        y = y0 - db;
        y = y < Y0 ? Y0 : y > Y0 + H ? Y0 + H : y;
        if (xx > X0) cv_line(xx - 1, prev, xx, y, col);
        prev = y;
    }
    cv_line(xc, Y0 + H - 4, xc, Y0 + H, on ? C_RED : C_ICE_D);
    cv_line(164, 4, 164, 98, C_GRID);
    cv_text(170, 0, &FONT_S, "ENV 3", l2 ? C_RED : C_ICE_D);
    adsr(&pat.e3_a, 170, 64, 20, 80, l2 ? C_RED : C_ICE_D);
}

/* SUB: its cycle at SHAPE (as the engine makes it: a triangle bent toward a sine), its level the wave's height; right,
 * what it follows as three lamps, and the note it plays now under the one it follows */
static void pic_sub(void)                       /* its shape in a frame, the width's (2026-11-22, the user took the
                                                   sheet); under it FOLLOWS as buttons (ALGORITHM picks), the note it
                                                   follows, then the one it plays (its OCTAVE from it) */
{
    enum { X0 = 8, W = 224, Y0 = 4, H = 76 };
    int32_t x, py = 0, mid = Y0 + H / 2, sh = (int32_t)(pat.sub_shape > 100u ? 100u : pat.sub_shape) * 327;
    int32_t amp = 8 + (int32_t)((uint32_t)(H / 2 - 9) * q99(pat.sub_lvl) / 99u);
    uint32_t on = fx_on(SD_FX_SUB), k, src = pat.sub_src % SD_SUBS;
    char t[16], *p;
    cv_frame(4, 0, 232, 84, C_ICE_D);
    cv_line(X0, mid, X0 + W - 1, mid, C_GRID);
    for (x = 0; x < W; x++) {
        int32_t u = (int32_t)((uint32_t)x * 65536u / (uint32_t)W) + 16384, tri, sn, v, y;   /* (from the middle, up) */
        u &= 65535;
        tri = u < 32768 ? u * 2 - 32768 : 98303 - u * 2;
        sn = (tri * (98304 - ((tri * tri) >> 15))) >> 16;
        v = sn + (((tri - sn) * sh) >> 15);
        y = mid - v * amp / 32768;
        if (x) cv_line(X0 + x - 1, py, X0 + x, y, on ? C_ICE : C_ICE_D);
        py = y;
    }
    cv_text(6, 88, &FONT_S, "FOLLOWS", C_ICE_D);
    for (k = 0, x = 66; k < SD_SUBS; k++) x = btn(x, 87, k == 2u ? 40 : 20, SUB_NAME[k], k == src ? (on ? 1u : 2u) : 0u) + 4;
    if (on && eng.sub_note) {                   /* the note now, beside the one it follows (an arrow between) */
        p = put_note(t, eng.sub_from); *p = 0;
        cv_text(166, 88, &FONT_S, t, C_ICE_D);
        cv_line(193, 96, 200, 96, C_ICE_D); cv_line(197, 93, 200, 96, C_ICE_D); cv_line(197, 99, 200, 96, C_ICE_D);
        p = put_note(t, eng.sub_note); *p = 0;
        cv_text(204, 88, &FONT_S, t, C_RED);
    }
}

static void pic_lfo(uint32_t k)                 /* its cycle in a frame, the width's (2026-11-22, the user took the
                                                   sheet), the dot where it is; under it where the matrix sends it */
{
    uint32_t i, n = 0;
    int32_t x = 30;
    cv_frame(4, 0, 232, 84, C_ICE_D);
    draw_lfo_wave(k, 8, 4, 224, 76);
    if (pg_lay[4] == 1u) { cv_text(6, 88, &FONT_S, LMODE_SAY[lmode_of(pat.lfo_mode[k])], C_ICE); return; }   /* (MODE:
                                                   what it does, whole) */
    cv_text(6, 88, &FONT_S, "TO", C_ICE_D);
    for (i = 0; i < SD_MODS; i++) {
        const SdModSlot *m = &pat.mod[i];
        const char *dn;
        if (m->src != SD_MS_LFO1 + k || !m->dst || !m->amt) continue;
        dn = DST_NAME[m->dst % SD_MDSTS];
        if (x + text_w(&FONT_S, dn) > 238) { cv_text(x, 88, &FONT_S, "..", C_ICE_D); break; }
        x = cv_text(x, 88, &FONT_S, dn, C_ICE) + 10;
        n++;
    }
    if (!n) cv_text(30, 88, &FONT_S, "NOTHING  K3: DEST", C_ICE_D);
}

/* RADAR, in two (2026-11-21, the user: "on the RADAR page the visuals, in two: the radar first,
 * as it was but bigger; beside it the 1010 row, a matrix, room for the longest LENGTH"). Left the scope (a
 * starship's): LENGTH places round it, clockwise from the top, each step's value a blip's distance from the middle
 * (the dotted ring 0, outward up), held over its arc with a hard jump to the next (as it sounds; faint), no edge
 * ring but its bearings ticked, the sweep turning through each step with a trail fading behind it (2026-11-23, the
 * user: "more like a real radar"), the newest blip red, the older dimmer as they age. Since 2026-11-23 (the user: "RADAR
 * feels squeezed: the little meter between the matrix and the radar out, the radar as big as it goes in between; the
 * trail a little fainter; the lines between the dots seen better again"): no gauge, the scope 120 across in the
 * middle of the space left of the readout, down into the hint line's rows (a message drawn over its foot while it
 * shows: draw_frame), the trail fainter, the lines between the blips ICE_D while it runs.
 * Right the loop's bits, its 16 places in rows of TG_C (the 1 top left, as the scope's at 12), a small readout in
 * a starship's frame, low in contrast so the scope leads (2026-11-21, the user: "less white, a little more sci-fi";
 * "the digits take all the focus: draw a 1 and a 0 smaller"): each place a small cell, its bit a 3 x 5 digit, a 1 a
 * dim lamp behind it, a 0 the digit alone; the place now red, its row's pointer red at the left; past LENGTH a dot;
 * a bit SEARCH flipped as it came round a dark red point till the sweep is back. Where it goes is MOD's: not here */
enum { TG_C = 4, TG_X = 154, TG_Y = 34, TG_P = 15, TG_W = 11 };   /* (the matrix: 4 x 4 cells of 11, 15 apart, beside
                                                   the scope's middle) */
enum { TUR_CX = 72, TUR_CY = 61, TUR_RO = 60 }; /* (the scope: its middle and its radius in the picture of H_PIC_TUR) */
static void tur_bit(int32_t x, int32_t y, uint32_t b, uint16_t c)   /* a 3 x 5 digit, its top left x, y */
{
    if (b) { cv_line(x + 1, y, x + 1, y + 4, c); cv_pset(x, y + 1, c); cv_line(x, y + 4, x + 2, y + 4, c); }
    else { cv_line(x, y, x + 2, y, c); cv_line(x, y + 4, x + 2, y + 4, c); cv_line(x, y + 1, x, y + 3, c); cv_line(x + 2, y + 1, x + 2, y + 3, c); }
}
static void tur_at(uint32_t ph, int32_t r, int32_t *x, int32_t *y)   /* an angle (a turn over 2^32, 0 at 12,
                                                   clockwise), radius r */
{
    *x = TUR_CX + ((r * sd_sine(ph)) >> 15);
    *y = TUR_CY - ((r * sd_sine(ph + 0x40000000u)) >> 15);
}
static void tur_pt(uint32_t p, uint32_t len, int32_t r, int32_t *x, int32_t *y)   /* place p of len, radius r */
{
    tur_at(p * (0xFFFFFFFFu / len), r, x, y);
}
static void pic_tur(void)
{
    enum { CX = TUR_CX, CY = TUR_CY, RO = TUR_RO, RZ = 3 + (RO - 6) / 2, M = 8 };   /* (M: an arc's pieces) */
    static int16_t pv[16];
    static uint8_t wk[16], wf[16];              /* (each place's step, its low byte: an age up to 255 is enough; the
                                                   step its bit flipped at, +1: 0 none) */
    static uint32_t seen, have, plen, preg;
    uint32_t c = eng.tur_count, reg = eng.tur_reg, plc = eng.tur_place, i, n, len = SD_TUR_LEN(&pat), on = SD_TUR_ON(&pat),
             cur, tn, hist;
    int32_t x, y, x0, y0;
    if (eng.tur_count != c) { c = eng.tur_count; reg = eng.tur_reg; plc = eng.tur_place; }   /* (a step meanwhile: the
                                                   newer three) */
    if (plc >= len) plc = 0;                    /* (LENGTH shorter: the engine's next step puts it right) */
    if (len != plen) { have = 0; plen = len; for (i = 0; i < 16u; i++) wf[i] = 0; }   /* (another LENGTH: the places
                                                   anew) */
    n = c - seen > 8u ? 8u : c - seen;          /* (the register holds the 8 values before: older ones are gone) */
    hist = preg << n | (reg & ((1u << n) - 1u));   /* (the register as the last frame left it, then each bit since:
                                                   the bits that left are in it too) */
    for (i = n; i > 0u; i--) {                  /* the steps since the last frame, each into its place */
        uint32_t k = c - (i - 1u), at = (plc + 8u * len - (i - 1u)) % len;   /* (its place in the loop: the 1 at 12) */
        pv[at] = (int16_t)sd_tur_dac(reg >> (i - 1u));
        wk[at] = (uint8_t)k;
        wf[at] = (uint8_t)(have >> at & 1u && ((hist >> (i - 1u) ^ hist >> (i + len - 1u)) & 1u) ? (k & 255u) + 1u : 0u);
                                                /* (flipped: the bit in is not the one that left) */
        have |= 1u << at;
    }
    seen = c;
    preg = reg;
    cur = plc;
    tn = len * M;
    for (i = 1; i < 48u; i += 2u) {             /* the ring 0 (dotted) */
        int32_t xa, ya;
        tur_pt(i, 48u, RZ, &xa, &ya); cv_pset(xa, ya, C_ICE_D);
    }
    for (i = 0; i < 36u; i++) {                 /* no edge ring (2026-11-23, the user: "more like a real radar", from
                                                   a sheet of four): its bearings, a tick each 10 degrees, longer each 30 */
        int32_t xa, ya, xb, yb;
        tur_pt(i, 36u, RO, &xa, &ya); tur_pt(i, 36u, RO - (i % 3u ? 2 : 5), &xb, &yb);
        cv_line(xa, ya, xb, yb, i % 3u ? C_GRID : on ? C_ICE_D : C_GRID);
    }
    cv_line(CX - RO + 6, CY, CX + RO - 6, CY, C_GRID);   /* the crosshair */
    cv_line(CX, CY - RO + 6, CX, CY + RO - 6, C_GRID);
    cv_rect(CX - 1, CY - 1, 3, 3, C_ICE_D);
    cv_rect(CX - 1, CY - RO, 3, 7, on ? C_ICE : C_ICE_D);   /* the loop's 1, at 12 o'clock: a mark and its number */
    cv_text(CX + 4, CY - RO + 1, &FONT_S, "1", on ? C_ICE : C_ICE_D);
    if (on) {                                   /* the sweep turning through the step (where RADAR's clock is in it),
                                                   its trail fading behind it over 36 degrees */
        static const uint8_t TR[20] = {0, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3};   /* (fainter:
                                                   the hand red, then the dark red, the fill, the grid) */
        uint32_t d = SD_TUR_DIV(&pat), sl = eng.seq_len_q8 * SD_SDIV_NUM[d] / SD_SDIV_DEN[d], fr = 0, st = 0xFFFFFFFFu / len, ph;
        uint16_t tc[4];
        int32_t k;
        tc[0] = C_RED; tc[1] = C_RED_D; tc[2] = C_FILL; tc[3] = C_GRID;
        if (eng.seq_run && sl >> 8) fr = (eng.tur_pos_q8 >> 8) * 256u / ((sl >> 8) + 1u);
        ph = cur * st + (st >> 8) * (fr > 255u ? 255u : fr);
        for (k = 19; k >= 0; k--) {
            tur_at(ph - (uint32_t)k * 0x01555555u, RO - 2, &x, &y);   /* (1.8 degrees apart) */
            cv_line(CX, CY, x, y, tc[TR[k]]);
        }
    }
    for (i = 0; i < len; i++) {                 /* the loop: each place an arc at its radius, then the jump */
        uint32_t j = (i + 1u) % len, q, a = i * M + tn - M / 2u;
        int32_t ri, rj;
        uint16_t lc = on ? C_ICE_D : C_GRID;    /* (seen again, 2026-11-23: dim, the sweep and the blips lead) */
        if (!(have >> i & 1u)) continue;
        ri = 3 + (sd_tur_off(&pat, pv[i]) + 32767) * (RO - 6) / 65534;
        for (q = 0; q < M; q++) {
            tur_pt((a + q) % tn, tn, ri, &x0, &y0); tur_pt((a + q + 1u) % tn, tn, ri, &x, &y);
            cv_line(x0, y0, x, y, lc);
        }
        if (!(have >> j & 1u)) continue;
        rj = 3 + (sd_tur_off(&pat, pv[j]) + 32767) * (RO - 6) / 65534;
        tur_pt((a + M) % tn, tn, ri, &x0, &y0); tur_pt((a + M) % tn, tn, rj, &x, &y);
        cv_line(x0, y0, x, y, lc);
    }
    for (i = 0; i < len; i++) {                 /* the blips: the newest red, the rest as old as they are */
        uint32_t age = (uint8_t)(c - wk[i]);
        if (!(have >> i & 1u)) continue;
        tur_pt(i, len, 3 + (sd_tur_off(&pat, pv[i]) + 32767) * (RO - 6) / 65534, &x, &y);
        if (!on) { cv_rect(x - 1, y - 1, 3, 3, C_GRID); continue; }
        if (i == cur && !age) { cv_box(x - 2, y - 2, 5, 5, C_RED); cv_pset(x, y, C_GLINT); }
        else cv_rect(x - 1, y - 1, 3, 3, age < len / 2u ? C_ICE : C_ICE_D);
    }
    {                                           /* the readout's frame: four corners round it */
        const int32_t x0 = TG_X - 8, y0 = TG_Y - 8, x1 = TG_X + 3 * TG_P + TG_W + 7, y1 = TG_Y + 3 * TG_P + TG_W + 7;
        const uint16_t fc = on ? C_ICE_D : C_GRID;
        cv_line(x0, y0, x0 + 5, y0, fc); cv_line(x0, y0, x0, y0 + 5, fc);
        cv_line(x1 - 5, y0, x1, y0, fc); cv_line(x1, y0, x1, y0 + 5, fc);
        cv_line(x0, y1, x0 + 5, y1, fc); cv_line(x0, y1 - 5, x0, y1, fc);
        cv_line(x1 - 5, y1, x1, y1, fc); cv_line(x1, y1 - 5, x1, y1, fc);
    }
    if (on) {                                   /* the row playing: a pointer at its left */
        int32_t py = TG_Y + (int32_t)(cur / TG_C) * TG_P + TG_W / 2;
        cv_line(TG_X - 5, py - 2, TG_X - 3, py, C_RED); cv_line(TG_X - 5, py + 2, TG_X - 3, py, C_RED);
    }
    for (i = 0; i < 16u; i++) {                 /* the bits: a cell a place, rows of TG_C */
        int32_t bx = TG_X + (int32_t)(i % TG_C) * TG_P, by = TG_Y + (int32_t)(i / TG_C) * TG_P;
        uint32_t bit = reg >> ((cur + len - i) % len) & 1u, now = on && i == cur;
        uint32_t fl = on && wf[i] && (uint8_t)(c - (wf[i] - 1u)) < len;   /* (flipped this pass: till the sweep's back) */
        if (i >= len) { cv_pset(bx + TG_W / 2, by + TG_W / 2, C_GRID); continue; }
        if (bit && on) cv_box(bx, by, TG_W, TG_W, now ? C_FILL : C_GRID);   /* a 1: a dim lamp */
        if (now) {                              /* the place now: red corners */
            cv_line(bx - 1, by - 1, bx + 1, by - 1, C_RED); cv_line(bx - 1, by - 1, bx - 1, by + 1, C_RED);
            cv_line(bx + TG_W - 2, by - 1, bx + TG_W, by - 1, C_RED); cv_line(bx + TG_W, by - 1, bx + TG_W, by + 1, C_RED);
            cv_line(bx - 1, by + TG_W, bx + 1, by + TG_W, C_RED); cv_line(bx - 1, by + TG_W - 2, bx - 1, by + TG_W, C_RED);
            cv_line(bx + TG_W - 2, by + TG_W, bx + TG_W, by + TG_W, C_RED); cv_line(bx + TG_W, by + TG_W - 2, bx + TG_W, by + TG_W, C_RED);
        }
        tur_bit(bx + TG_W / 2 - 1, by + TG_W / 2 - 2, bit,
                !on ? C_GRID : now ? (bit ? C_GLINT : C_RED) : bit ? C_ICE : C_ICE_D);
        if (fl && !now) cv_pset(bx + TG_W - 2, by + 1, C_RED);
    }
}

/* The pages drawn over the knobs' row and the picture at once (2026-11-22, the user took a sheet of tidier pages:
 * "the picture is the knobs", what a knob sets in one place): MOD's slots as a patch bay, SETUP's choices as panel
 * buttons. 0 .. H_LIST from the knobs' top, in two strips as the lists (draw_frame) */
static uint32_t page_full(void) { return (page == 4u && dmod < SD_MODS) || page == PAGE_SETUP; }
static void put7(char *t, const char *s) { uint32_t n = 0; while (s && *s && n < 7u) { *t++ = *s++; n++; } *t = 0; }
static void pic_mod_full(void)                  /* MOD: a row a slot, its columns the knobs' (SOURCE DEST AMOUNT OFFSET,
                                                   their names on top as the knobs'): its source, where it goes, its
                                                   amount's bar from the middle (an OFFSET: the swing), the OFFSET; the
                                                   slot picked red and framed (the knobs turn it), an empty one -- */
{
    static const char *const HD[4] = {"SOURCE", "DEST", "AMOUNT", "OFFSET"};
    uint32_t k;
    char t[12], *p;
    for (k = 0; k < 4u; k++) {
        cv_text(30 + (int32_t)k * 60 - text_w(&FONT_S, HD[k]) / 2, 0, &FONT_S, HD[k], C_ICE_D);
        if (k) cv_line((int32_t)k * 60, 18, (int32_t)k * 60, 150, C_GRID);
    }
    for (k = 0; k < SD_MODS; k++) {
        const SdModSlot *m = &pat.mod[k];
        int32_t y = 20 + (int32_t)k * 16, a = amt99(m->amt), w = a * 22 / 99;
        uint32_t use = m->src && m->dst && m->amt, sel = k == dmod;
        uint16_t c = sel ? C_RED : use ? C_ICE : C_ICE_D, cb = sel ? C_RED : a < 0 ? C_ICE_D : C_ICE;
        if (sel) cv_frame(0, y - 1, 240, 17, C_RED);
        if (!m->src && !m->dst) {
            cv_text(22, y, &FONT_S, "--", sel ? C_RED : C_GRID);
            cv_text(82, y, &FONT_S, "--", sel ? C_RED : C_GRID);
            continue;
        }
        put7(t, SRC_NAME[m->src % SD_MSRCS]); cv_text(30 - text_w(&FONT_S, t) / 2, y, &FONT_S, t, c);
        put7(t, slot_dname(m)); cv_text(90 - text_w(&FONT_S, t) / 2, y, &FONT_S, t, c);
        cv_line(126, y + 8, 174, y + 8, C_GRID);
        cv_line(150, y + 5, 150, y + 11, C_ICE_D);
        if (w && pat.mod_off[k] && m->dst < SD_MDSTS) {   /* (an OFFSET: the swing about the set value, as the
                                                   slot's bar had it) */
            int32_t o = pat.mod_off[k], d = 100 + (o < 0 ? -o : o), l = w * (o - 100) / d, h = w * (o + 100) / d;
            if (l > h) { int32_t t2 = l; l = h; h = t2; }
            if (h - l >= 1) cv_rect(150 + l, y + 7, h - l, 3, cb);
        } else if (w) cv_rect(w > 0 ? 151 : 150 + w, y + 7, w > 0 ? w : -w, 3, cb);
        if (TG_IS(m->dst)) cv_text(210 - text_w(&FONT_S, m->amt < 0 ? "ON 0" : "ON 1") / 2, y, &FONT_S,
                                   m->amt < 0 ? "ON 0" : "ON 1", sel ? C_RED : C_ICE);   /* (RADAR 1's own: where) */
        else if (m->dst < SD_MDSTS) {           /* (RADAR's own three: no OFFSET) */
            p = put_sgn(t, pat.mod_off[k]); *p++ = '%'; *p = 0;
            cv_text(210 - text_w(&FONT_S, t) / 2, y, &FONT_S, t, sel ? C_RED : pat.mod_off[k] ? C_ICE : C_ICE_D);
        }
    }
}
static const char *const PG_NAME[16] = {"NAV", "REACTOR", "DISTROY", "SPACE", "LFO", "MACROS", "SEQ", "ENV", "FILTER",
                                        "ARP", "ORBIT", "TIME", "LOOPER", "SETUP", "DRUM", "SUB"};
static const char *btn_name(uint32_t b)
{
    return b == BTN_OCT_DOWN ? "OCT-" : b == BTN_OCT_UP ? "OCT+" : b == BTN_FX ? "FX" : b == BTN_SEL ? "SEL" :
           b == BTN_ENV ? "ENV" : b == BTN_LFO ? "LFO" : b == BTN_EDIT ? "EDIT" : b == BTN_GLO ? "GLO" :
           b == BTN_HOME ? "HOME" : b == BTN_SAVE ? "SAVE" : b == BTN_ARP ? "ARP" : b == BTN_SEQ ? "SEQ" :
           b == BTN_PLAY ? "PLAY" : b == BTN_REC ? "REC" : "BTN";
}
static const char *enc_name(uint32_t e)
{
    return e == ENC_SELECT ? "SEL" : e == ENC_ALGORITHM ? "ALG" : e == ENC_PRESETS ? "PRE" : e == ENC_KNOB1 ? "K1" :
           e == ENC_KNOB2 ? "K2" : e == ENC_KNOB3 ? "K3" : e == ENC_KNOB4 ? "K4" : "ENC";
}
static char *put_hex(char *p, uint32_t v)
{
    uint32_t k;
    for (k = 0; k < 8u; k++) *p++ = "0123456789ABCDEF"[v >> (28u - 4u * k) & 15u];
    return p;
}
static char *put_2(char *p, uint32_t v) { *p++ = (char)('0' + v / 10u % 10u); *p++ = (char)('0' + v % 10u); return p; }
static void pic_frz(void)                       /* SETUP, SELECT: a freeze (or a crash) from the log, whole: a photo of
                                                   it is the report. What, when, where it stood and was stuck, the
                                                   inputs before it, the newest first */
{
    FrzRec r;
    char s[48], t[12], *p;
    uint32_t i, up, y = 74, ln = 0, first = 1;
    int32_t k;
    if (frz_get(setup_log - 1u, &r)) { cv_text(6, 0, &FONT_S, "THE LOG CANNOT BE READ", C_RED); return; }
    p = put(s, r.kind == FRZ_CRASH ? "CRASH " : "FREEZE "); p = put_u(p, setup_log); p = put(p, " OF "); *put_u(p, frz_n) = 0;
    cv_text(6, 0, &FONT_S, s, C_RED);
    r.ver[15] = 0;
    cv_text(234 - text_w(&FONT_S, r.ver), 0, &FONT_S, r.ver, C_ICE_D);
    up = r.run.ms / 1000u;
    p = put_u(put(s, "AFTER "), up / 3600u); *p++ = ':'; p = put_2(p, up / 60u % 60u); *p++ = ':'; p = put_2(p, up % 60u);
    p = put(p, r.run.st & 1u ? "  PLAY" : "  STOP");
    if (r.run.st & 2u) p = put(p, " REC");
    if (r.run.st & 4u) p = put(p, " GLO");
    *p = 0;
    cv_text(6, 18, &FONT_S, s, C_ICE);
    p = put(s, PG_NAME[r.run.page & 15u]);
    if (r.run.page == PAGE_SEQ) p = put(p, r.run.st >> 6 & 1u ? " B " : " A "), p = put(p, SL_NAME[(r.run.st >> 8 & 7u) % SL_N]);
    p = put_u(put(p, "  CPU "), (r.run.cpu + 5u) / 10u); *p++ = '%';
    p = put(p, "  AT ");
    if (r.run.at == LR_INPUT) p = put(p, "INPUT");   /* (where the main loop stood, LR_*) */
    else if (r.run.at >= LR_SAVE && r.run.at < LR_SETTINGS) p = put(p, "SAVE");
    else if (r.run.at == LR_SETTINGS) p = put(p, "SETUP");
    else if (r.run.at == LR_DRAW) p = put(p, "DRAW");
    else if (r.run.at == LR_DRAWN) p = put(p, "DRAWN");
    else if (r.run.at == LR_END) p = put(p, "END");
    else p = put_u(p, r.run.at);
    *p = 0;
    cv_text(6, 36, &FONT_S, s, C_ICE);
    if (r.kind == FRZ_CRASH) { p = put_hex(put(s, "PC "), r.crash[3]); p = put_hex(put(p, " "), r.crash[4]); }
    else if (r.run.pc_n) {                      /* where it was stuck: the first and the last PC sampled */
        p = put_hex(put(s, "STUCK "), r.run.pc[0]);
        if (r.run.pc_n > 1u && r.run.pc[r.run.pc_n - 1u] != r.run.pc[0]) p = put_hex(put(p, " "), r.run.pc[r.run.pc_n - 1u]);
    } else p = put(s, (int32_t)(r.run.isr_ms - r.run.ms) < 1500 ? "STUCK --, THE AUDIO STOOD TOO" : "STUCK --");
    *p = 0;
    cv_text(6, 54, &FONT_S, s, C_ICE);
    p = s; *p = 0;
    for (k = (int32_t)EV_N - 1; k >= 0 && ln < 4u; k--) {   /* the inputs, newest first, as words, four lines */
        const SdEv *v = &r.ev[k];
        char *q = t;
        uint32_t kind = v->kc >> 5, code = v->kc & 31u;
        if (!kind || kind == EV_KEY_UP || (kind == EV_BTN_UP && code != BTN_GLO)) continue;
        if (first) {                            /* (how long before the freeze the newest came) */
            uint32_t d = (((r.run.ms >> 3) - v->t8) & 0xFFFFu) * 8u;
            char h[32], *hp = put(h, "INPUTS BEFORE, NEWEST ");
            hp = put_u(hp, d / 1000u); *hp++ = '.'; *hp++ = (char)('0' + d / 100u % 10u); *put(hp, "S") = 0;
            cv_text(6, y, &FONT_S, h, C_ICE_D);
            y += 18u; first = 0;
        }
        if (kind == EV_BTN) q = put(q, btn_name(code));
        else if (kind == EV_BTN_UP) q = put(put(q, btn_name(code)), "^");
        else if (kind == EV_KEY) q = put_note(q, KEY0 + code);
        else { q = put(q, enc_name(code)); *q++ = v->val < 0 ? '-' : '+'; q = put_u(q, (uint32_t)(v->val < 0 ? -v->val : v->val)); }
        *q = 0;
        if ((uint32_t)(p - s) + (uint32_t)(q - t) + 1u > 29u) {   /* (the line full: the next) */
            cv_text(6, (int32_t)y, &FONT_S, s, C_ICE);
            y += 18u; ln++; p = s; *p = 0;
            if (ln >= 4u) break;
        }
        if (p != s) *p++ = ' ';
        p = put(p, t); *p = 0;
    }
    if (p != s && ln < 4u) cv_text(6, (int32_t)y, &FONT_S, s, C_ICE);
    (void)i;
}
static void pic_setup_full(void)                /* SETUP: each knob's choices as buttons under its name, the one lit
                                                   the setting (GLO: SYNC over TEMPO's, CORES over XPOSE's); TEMPO
                                                   its number and scale (a MIDI clock leading: its tempo, CLK); COLOUR
                                                   its name (the user: not the colours, they break the design); the
                                                   CPU a strip of its own under them: the cores' lamps, the worst
                                                   interrupt of the last second as a gauge (red from half, Dinghy's
                                                   rule), the worst since the start a mark on it, MAX or the GAPs (GLO:
                                                   the battery's volts) */
{
    if (setup_log) { pic_frz(); return; }      /* (SELECT: the freeze log) */
    static const char *const NM[4] = {"TEMPO", "XPOSE", "MASTER", "COLOUR"}, *const TN[4] = {"DEMO 1", "DEMO 2", "CLEAR", "LOG"};
    uint32_t l2 = lay_is(PAGE_SETUP, LU_MORE), tl = lay_is(PAGE_SETUP, LU_TOOLS), k, pm = cpu_permille;
#if FM1_CPU1
    const uint32_t cores = 1u;
#else
    const uint32_t cores = 0u;
#endif
    char s[16], *p;
    for (k = 0; k < 4u; k++) {
        const char *nm = tl ? TN[k] : !l2 ? NM[k] : k == 0u ? "SYNC" : k == 1u && cores ? "CORES" : 0;
        if (k) cv_line((int32_t)k * 60, 18, (int32_t)k * 60, 90, C_GRID);
        if (nm) cv_text(30 + (int32_t)k * 60 - text_w(&FONT_S, nm) / 2, 0, &FONT_S, nm, C_ICE_D);
    }
    if (tl) {                                   /* TOOLS: each a button, lit while it asks (OCT+ yes); LOG how many
                                                   (dark red: some), its knob (or SELECT) opens the newest */
        btn(4, 17, 52, "PLAY", clr_all_at && setup_ask == 2u);
        btn(64, 17, 52, "PLAY", clr_all_at && setup_ask == 3u);
        btn(124, 17, 52, "ALL", clr_all_at && setup_ask == 1u);
        if (frz_n) *put_u(s, frz_n) = 0; else *put(s, "NONE") = 0;
        btn(184, 17, 52, s, frz_n ? 2u : 0u);
    } else if (l2) {                            /* MORE: SYNC and CORES, each its choices (the battery's volts in the
                                                   strip under them) */
        btn(4, 20, 52, "AUTO", !set.sync_int);  /* SYNC: AUTO, a MIDI clock leads when it comes; INT: not heard */
        btn(4, 42, 52, "INT", set.sync_int);
#if FM1_CPU1
        btn(64, 20, 52, "2", !set.one_core);    /* CORES, from the next start */
        btn(64, 42, 52, "1", set.one_core);
#endif
        { uint32_t c; for (c = 0; c < 3u; c++) btn(124, 20 + (int32_t)c * 22, 52, CLICK_NAME[c], set.click == c); }   /* CLICK */
    } else {                                    /* TEMPO (a MIDI clock's: its own, CLK) XPOSE MASTER COLOUR */
        if (eng.clk_on) {
            uint32_t b = eng.clk_beat;
            p = b ? put_u(s, (SD_FS * 60u + b / 2u) / b) : put(s, "---");
            *put(p, " CLK") = 0;
        } else *put_u(s, set.bpm) = 0;
        cv_text(30 - text_w(&FONT_S, s) / 2, 17, &FONT_S, s, eng.clk_on ? C_RED : C_ICE);
        if (!eng.clk_on) {                      /* (its scale, as a knob's) */
            int32_t t, px = 8 + (int32_t)((uint32_t)set.bpm - 40u) * 44 / 200;
            for (t = 0; t <= 10; t++) cv_rect(8 + t * 44 / 10, 38, 1, t % 5 ? 2 : 4, C_ICE_D);
            cv_rect(8, 41, 45, 1, C_GRID);
            cv_rect(8, 40, px - 8, 2, C_ICE);
            cv_box(px - 1, 33, 3, 9, C_ICE);
        }
        btn(64, 20, 52, "IN KEY", !set.xchrom);
        btn(64, 42, 52, "CHROM", set.xchrom);
        btn(124, 20, 52, "SEQ A", !set.master);
        btn(124, 42, 52, "SEQ B", set.master);
        cv_text(210 - text_w(&FONT_S, ACC_NAME[set.accent % SD_ACCENTS]) / 2, 17, &FONT_S, ACC_NAME[set.accent % SD_ACCENTS], C_ICE);
    }
    cv_frame(2, 100, 236, 52, C_ICE_D);         /* the CPU */
    cv_text(8, 104, &FONT_S, "CPU", C_ICE_D);
#if FM1_CPU1
    {                                           /* the cores, a lamp each (2026-11-17): the first always lit; the second
                                                   lit while it runs, red once it gave up, dark if it never started */
        uint16_t c2 = fm1_cpu1_on && !eng.sp_room ? C_ICE :
                      fm1_cpu1_gave_up && fm1_cpu1_gave_up != FM1_CPU1_OFF ? C_RED : C_GRID;
        cv_box(44, 108, 7, 7, C_ICE);
        cv_box(57, 108, 7, 7, c2);
    }
#endif
    {
        uint32_t f = pm == 0xFFFFFFFFu ? 0u : (pm > 1000u ? 1000u : pm) * 104u / 1000u;
        cv_rect(80, 110, 104, 4, C_GRID);
        if (f) cv_rect(80, 110, (int32_t)f, 4, pm >= 500u ? C_RED : C_ICE_D);
        cv_rect(132, 107, 1, 2, C_ICE_D);       /* (half: the rule) */
        if (cpu_max) {
            uint32_t hm = (cpu_max > 1000u ? 1000u : cpu_max) * 104u / 1000u;
            cv_rect(80 + (int32_t)hm, 106, 1, 12, cpu_max >= 1000u ? C_RED : C_ICE);
        }
        if (pm == 0xFFFFFFFFu || !pm) p = put(s, "--");
        else { p = put_u(s, (pm + 5u) / 10u); *p++ = '%'; }
        *p = 0;
        cv_text(192, 104, &FONT_S, s, pm != 0xFFFFFFFFu && pm >= 500u ? C_RED : C_ICE);
    }
    if (!l2 || !batt) {                         /* since the start: the gaps in red, else the worst */
        if (cpu_over) { p = put_u(put(s, "GAP "), cpu_over > 999u ? 999u : cpu_over); *p = 0; }
        else if (cpu_max) { p = put(put_u(put(s, "MAX "), (cpu_max + 5u) / 10u), "%"); *p = 0; }
        else *s = 0;
        if (*s) cv_text(80, 128, &FONT_S, s, cpu_over ? C_RED : C_ICE_D);
        if (frz_n) { p = put_u(put(s, "LOG "), frz_n); *p = 0; cv_text(232 - text_w(&FONT_S, s), 128, &FONT_S, s, C_RED); }   /* (a
                                                   freeze kept: SELECT, or TOOLS' LOG, shows it) */
    } else {                                    /* GLO: the battery's volts */
        uint32_t mv = batt_mv();
        p = put_u(s, mv / 1000u); *p++ = '.'; *p++ = (char)('0' + mv / 100u % 10u); *p++ = (char)('0' + mv / 10u % 10u);
        *p++ = 'V'; *p = 0;
        cv_text(80, 128, &FONT_S, s, C_ICE_D);
    }
}

static void pic_arp(void)                       /* the run: three notes over OCT octaves in the mode's order */
{
    enum { X0 = 6, W = 228, Y0 = 18, H = 82 };
    uint32_t k, len = 3u * set.aoct, n = len > 12u ? 12u : len, w = W / 12u;
    char s[24], *p;
    cv_line(X0, Y0 + H, X0 + W - 1, Y0 + H, C_GRID);
    for (k = 0; k < 12u; k++) {
        uint32_t j, h;
        if (set.amode == SD_AM_RND) j = (k * 7u + 3u) % len;
        else if (set.amode == SD_AM_UPDN && len > 1u) { uint32_t c = 2u * len - 2u, q = k % c; j = q < len ? q : c - q; }
        else { j = k % len; if (set.amode == SD_AM_DOWN) j = len - 1u - j; }
        h = 4u + j * (uint32_t)(H - 8) / (len > 1u ? len - 1u : 1u);
        cv_rect(X0 + (int32_t)(k * w), Y0 + H - (int32_t)h, (int32_t)(w * set.agate / 100u) + 1, (int32_t)h,
                k < n || set.amode == SD_AM_UPDN ? (eng.arp_on ? C_ICE : C_ICE_D) : C_GRID);
    }
    if (!eng.arp_on) p = s;                     /* (off: the top line's lamp says it, and the hint line what EDIT
                                                   does) */
    else if (!eng.arp_n) p = put(s, "ON: PLAY KEYS");
    else { p = put(s, "ON: "); p = put_u(p, eng.arp_n); p = put(p, eng.arp_n == 1u ? " NOTE" : " NOTES"); }
    if (set.ahold) p = put(p, p > s ? " HOLD" : "HOLD");
    *p = 0;
    cv_text(6, 0, &FONT_S, s, eng.arp_on ? C_RED : C_ICE_D);
}

static void pic_seq(void)                       /* SEQ, RADAR: the pattern, the bank's steps as towers, the keys */
{
    uint8_t bd[SD_SEGS + 1u];
    uint32_t sc = sd_seg_bounds(&seqp, bd), k, lo = 127, hi = 0, play = eng.seq_run ? eng.t[seq_trk].seq_shown : 0xFFu;
    uint32_t lock = seq_trk && set.mode == SD_MODE_MONO, ts;
    int32_t tb;
    char s[40], *p;
    uint16_t lc = C_ICE_D;
    for (k = 0; k < SD_PSTEPS; k++) {           /* the whole pattern: segments in turn ice and dim, the playhead red */
        uint32_t seg = sd_seg_of(&seqp, k);
        uint16_t c = k >= seqp.len ? C_GRID : (seqp.seg_skip >> seg & 1u) ? C_RED_D : seg & 1u ? C_ICE_D : C_ICE;
        if (k == play) c = C_RED;
        cv_rect(8 + (int32_t)k * 3 + (int32_t)(k / 16u) * 3, 0, 2, k < seqp.len ? 4 : 2, c);
    }
    cv_rect(8 + (int32_t)seq_bank * 51, 6, 48, 1, C_RED);   /* the bank on the keys */
    for (k = 0; k < 16u; k++) {
        const SdStep *st = &seqp.step[seq_bank * 16u + k];
        uint32_t nt;
        if (seq_bank * 16u + k >= seqp.len || st->type >= SD_ST_REST) continue;
        nt = seq_heard(&seqp, seq_bank * 16u + k)->note;   /* (a TIE: the note it holds) */
        if (nt < lo) lo = nt;
        if (nt > hi) hi = nt;
    }
    if (hi < lo) lo = hi = 45;
    if (hi - lo < 12u) { lo = lo > 6u ? lo - 6u : 0u; hi = lo + 12u; }
    {                                           /* (2026-11-22, the user took the sheet: the line under only when it
                                                   says something, REC, GLO, TOOLS, a lock, CLR; else the towers take
                                                   its room) */
        uint32_t line = seq_rec || shift() || lock || seq_clr_at;
        tb = seq_segmode ? 60 : line ? 78 : 96;
        ts = seq_segmode ? 24u : line ? 40u : 56u;
    }
    for (k = 0; k < 16u; k++) {                 /* towers as high as the note; TIE a bridge, SLIDE a ramp, REST a dot */
        uint32_t i = seq_bank * 16u + k, seg = sd_seg_of(&seqp, i);
        const SdStep *st = &seqp.step[i];
        int32_t x = 9 + (int32_t)k * 14, base = tb, h;
        uint16_t c = i == play ? C_RED : C_ICE;
        uint32_t dim = seq_segmode && seg != (seq_dseg < sc ? seq_dseg : sc - 1u);   /* (SEGMENTS: not the part picked) */
        if (i >= seqp.len) { cv_rect(x + 4, base - 1, 4, 1, C_GRID); continue; }
        if (i == bd[seg] && sc > 1u) {          /* a segment starts here: its letter and order */
            p = s;
            *p++ = (char)('A' + seg);
            p = put(p, SORDER_GLYPH[seqp.seg_order[seg] % SD_SORDERS]);
            *p = 0;
            cv_line(x - 2, 10, x - 2, base, seq_segmode ? C_ICE : C_GRID);
            cv_text(x, 8, &FONT_S, s, seq_segmode && seg == seq_dseg ? C_RED : C_ICE_D);
        }
        {
            uint32_t nt = seq_heard(&seqp, i)->note;   /* (a TIE as high as the note it holds) */
            h = 4 + (int32_t)((nt < lo ? 0u : nt - lo) * ts / (hi - lo));
        }
        switch (st->type) {
        case SD_ST_REST: cv_rect(x + 5, base - 2, 2, 2, C_ICE_D); break;
        case SD_ST_SKIP: cv_line(x + 2, base - 6, x + 9, base - 1, C_RED_D); cv_line(x + 9, base - 6, x + 2, base - 1, C_RED_D); break;
        case SD_ST_TIE: cv_rect(x - 3, base - h + 2, 15, 2, c); cv_rect(x + 2, base - h + 4, 8, h - 4, C_GRID); break;
        default: {                              /* as wide as its GATE: 100 % the whole step */
            uint32_t g = st->gate < 1u ? 1u : st->gate > SD_GATE_MAX ? SD_GATE_MAX : st->gate;
            int32_t w = 2 + (int32_t)(g * 12u / 100u);
            cv_box(x + 1, base - h, w, h, i == play ? C_RED : dim ? C_GRID : st->vel >= 110u ? C_ICE : C_ICE_D);
            if (st->type == SD_ST_SLIDE) cv_line(x - 3, base - h + 6, x + 1, base - h, C_RED);
            if (st->type == SD_ST_NOTE && st->ratchet > 1u) {   /* RATCHET: the tower cut in its hits */
                int32_t r;
                for (r = 1; r < (int32_t)(st->ratchet > SD_RATCHET_MAX ? SD_RATCHET_MAX : st->ratchet); r++)
                    cv_rect(x + 1, base - h + h * r / (int32_t)st->ratchet, w, 1, C_BG);
            }
            if (st->prob < 100u) cv_rect(x + 5, base - h - 4, 2, 2, C_RED);   /* PROB: a red dot over it */
            if (st->chord[0]) cv_rect(x + 1, base - h - 2, w, 1, C_RED);   /* a chord: a red cap */
            break;
        }
        }
        if (i == seq_cur && !seq_segmode) cv_frame(x - 1, tb - (int32_t)ts - 10, 14, (int32_t)ts + 12, C_RED);
    }
    if (seq_segmode) {                          /* the part picked framed (as far as this bank shows it); then how
                                                   the parts play, in turn: each its letter, its order, its repeats
                                                   (a skipped one left out); then what the keys do */
        uint32_t g = seq_dseg < sc ? seq_dseg : sc - 1u, f = bd[g], l = bd[g + 1u] - 1u, b0 = seq_bank * 16u;
        if (l >= b0 && f < b0 + 16u) {
            int32_t x0 = 9 + (int32_t)((f > b0 ? f : b0) - b0) * 14 - 1, x1 = 9 + (int32_t)((l < b0 + 15u ? l : b0 + 15u) - b0) * 14 + 13;
            cv_frame(x0, 22, x1 - x0, 42, C_RED);
        }
        p = put(s, "PLAYS");
        for (k = 0; k < sc && p < s + 33; k++) {   /* (six at most a part: 39 and its end fit) */
            uint32_t r = seqp.seg_rep[k] < 1u ? 1u : seqp.seg_rep[k];
            if (seqp.seg_skip >> k & 1u) continue;
            *p++ = ' '; *p++ = (char)('A' + k);
            p = put(p, SORDER_GLYPH[seqp.seg_order[k] % SD_SORDERS]);
            if (r > 1u) { *p++ = 'x'; *p++ = (char)('0' + r); }
        }
        *p = 0;
        cv_text(6, 68, &FONT_S, s, C_ICE);
        if (!seq_rec && !shift() && !seq_clr_at) { cv_text(6, 86, &FONT_S, "SELECT PICKS THE PART", C_ICE_D); return; }
    }
    /* the line under: what the keys do now */
    if (seq_clr_at) {                           /* CLEAR asked (TOOLS' K3) */
        cv_text(6, 86, &FONT_S, seq_trk ? "CLEAR SEQ B? OCT+ YES" : "CLEAR SEQ A? OCT+ YES", C_RED);
        return;
    }
    if (seq_rec) {                              /* REC (or DUB) STEP or LIVE, the two buttons a TIE and a REST */
        int32_t x = cv_text(6, 86, &FONT_S, seq_dub ? "DUB" : "REC", C_RED);
        x = cv_text(x + 8, 86, &FONT_S, eng.seq_run ? "LIVE" : "STEP", C_ICE);
        cv_text(x + 16, 86, &FONT_S, "ENV TIE  LFO REST", C_ICE_D);
        return;
    }
    if (shift()) { p = put(s, "KEYS TRANSPOSE"); lc = C_RED; }   /* GLO: every key transposes (their lights) */
    else if (lock) { p = put(s, "B NEEDS DUO OR POLY: SEL"); lc = C_RED; }
    else return;                                /* (the keys play: no line; GLO says what they do then) */
    *p = 0;
    cv_text(6, 86, &FONT_S, s, lc);
}

/* ORBIT, the song, as its knobs are. Its layers as every page's (2026-11-19, night: the user found the row of tabs over
 * the song odd, a black key's job up there): GLO's red on the knobs (LANES), and with GLO (or a layer open) a line at
 * the bottom, as SEQ's, with the black keys' jobs, MACRO (B4) and SETUP (B10), lit where one is open. The picture a
 * timeline (2026-11-22, the user picked A of a sheet, "ORBIT rethought"): the song left to right, each step a block as
 * wide as its LOOPS (how long it lasts), a row each for SEQ A's pattern, SEQ B's ("--": as it plays, a line) and the
 * drums' beat (a crossed box: off there), the pattern's number in the block where it fits; the steps' ticks and
 * numbers under them; under those the changes as curves over the whole song (a step's value held, or with GLIDE a
 * slope to the next step's); the step picked red, a red line where the song plays now. SONG: the blocks full, the
 * first two curves that move dim; LANES (GLO) and MACRO (B4): the blocks a thin strip, their four curves (XPOS A,
 * XPOS B, the two lanes; the four macros) bright, the picked step's piece red */
enum { OT_X0 = 22, OT_W = 214 };              /* the timeline's left (a curve's name before it), its width */
static int32_t orb_vat(uint32_t row, uint32_t st)   /* a curve's value at step st: XPOS A, XPOS B, lane 1, lane 2,
                                                   the four macros, then LOOP's and FREEZE's pitch */
{
    st %= SD_ORB_STEPS;
    return row == 0u ? orbp.xa[st] : row == 1u ? orbp.xb[st] : row < 4u ? orbp.lane[row - 2u][st] :
           row < 8u ? orbp.mac[row - 4u][st] : sd_orb_tail(&orbp, row - 8u, st);
}
static void orb_curve(uint32_t row, int32_t ym, int32_t half, const uint32_t *cum, uint32_t total, uint32_t len,
                      int32_t pick, uint16_t c)   /* a curve over the song, ym its 0, half its reach (pick: its red
                                                   piece, -1 none) */
{
    static const char *const NM[10] = {"XA", "XB", "L1", "L2", "MU", "MV", "BR", "M4", "LP", "FZ"};
    uint32_t i, gl = row == 2u ? (orbp.glide & SD_OG_L1) : row == 3u ? (orbp.glide & SD_OG_L2) : row >= 4u && row < 8u ? (orbp.glide & SD_OG_MAC) : 0u;
    int32_t rng = row < 2u ? 24 : row >= 8u ? 12 : 100, py = ym;
    cv_text(2, ym - 8, &FONT_S, NM[row % 10u], gl ? C_RED : c == C_ICE ? C_ICE_D : C_GRID);
    for (i = 0; i < len; i++) {
        int32_t x0 = OT_X0 + (int32_t)(cum[i] * OT_W / total), x1 = OT_X0 + (int32_t)(cum[i + 1u] * OT_W / total) - 1;
        int32_t v = orb_vat(row, i), y = ym - v * half / rng;
        uint16_t cc = (int32_t)i == pick ? C_RED : c;
        if (i) cv_line(x0, py, x0, y, c);
        if (gl) {                               /* GLIDE: through the step to the next one's value (the end: back to
                                                   the first, or held) */
            int32_t vn = i + 1u < len ? orb_vat(row, i + 1u) : orbp.end == SD_OE_STOP ? v : orb_vat(row, 0);
            int32_t yn = ym - vn * half / rng;
            cv_line(x0, y, x1, yn, cc);
            py = yn;
        } else { cv_line(x0, y, x1, y, cc); py = y; }
    }
}
static void pic_orbit(void)
{
    uint32_t len = SD_ORB_LEN(&orbp), pick = orb_cur % SD_ORB_STEPS, layer = pg_lay[PAGE_ORB];
    uint32_t curves = layer == LO_PITCH || layer == LO_LANE || layer == LO_MACRO;
    uint32_t cum[SD_ORB_STEPS + 1u], total, i, r;
    int32_t rh = curves ? 5 : 12, rs = curves ? 7 : 16, ry = curves ? 22 : 49, bottom = 100;
    if (len > SD_ORB_STEPS) len = SD_ORB_STEPS;
    cum[0] = 0;
    for (i = 0; i < len; i++) cum[i + 1u] = cum[i] + SD_ORB_LOOPS(&orbp, i);
    total = cum[len] ? cum[len] : 1u;
    for (r = 0; r < 2u + SD_DRUMS; r++) {       /* the blocks: SEQ A, SEQ B, the drums */
        int32_t y = 1 + (int32_t)r * rs;
        if (!curves) cv_text(4, y - 2, &FONT_S, r == 0u ? "A" : r == 1u ? "B" : "D", C_ICE_D);
        for (i = 0; i < len; i++) {
            uint32_t v = r < 2u ? orbp.pat[r][i] : orbp.drm[i], sel = i == pick;
            int32_t x0 = OT_X0 + (int32_t)(cum[i] * OT_W / total), x1 = OT_X0 + (int32_t)(cum[i + 1u] * OT_W / total) - 2;
            int32_t w = x1 - x0 + 1;
            uint16_t c = sel ? C_RED : curves ? C_OFF : orbp.on ? C_ICE_D : C_OFF;
            if (w < 1) w = 1;
            if (r == 2u && v == SD_DR_SONG_OFF) {   /* (the drums off there: a crossed box) */
                cv_edge(x0, y, w, rh, c);
                cv_line(x0 + 1, y + 1, x0 + w - 2, y + rh - 2, c);
                cv_line(x0 + 1, y + rh - 2, x0 + w - 2, y + 1, c);
                continue;
            }
            if (!v) {                           /* (as it plays: a line; picked, a dark red box round it) */
                if (sel) cv_edge(x0, y, w, rh, C_RED_D);
                cv_rect(x0, y + rh / 2, w, 1, sel ? C_RED : C_GRID);
                continue;
            }
            if (sel) cv_box(x0, y, w, rh, C_RED); else cv_edge(x0, y, w, rh, c);   /* (the same shape picked or not) */
            if (!curves && w >= 20) {
                char t[3] = {(char)('0' + v / 10u), (char)('0' + v % 10u), 0};
                cv_text(x0 + 3, y - 1, &FONT_S, t, sel ? C_BG : orbp.on ? C_ICE : C_ICE_D);
            }
        }
    }
    for (i = 0; i <= len; i++) {                /* the steps: a tick where each starts, its number where it fits (the
                                                   picked one's always, red, a red bar under it) */
        int32_t x0 = OT_X0 + (int32_t)(cum[i] * OT_W / total), x1 = i < len ? OT_X0 + (int32_t)(cum[i + 1u] * OT_W / total) : x0;
        cv_rect(x0, ry, 1, 4, C_ICE_D);
        if (i == pick) cv_rect(x0, ry + 5, x1 - x0 > 1 ? x1 - x0 - 1 : 1, 2, C_RED);
        if (i < len && (i == pick || x1 - x0 >= (i >= 9u ? 24 : 15))) {
            char t[4];
            *put_u(t, i + 1u) = 0;
            cv_text(x0 + 3, ry - 3, &FONT_S, t, i == pick ? C_RED : C_ICE_D);
        }
        if (i == len && orbp.end == SD_OE_STOP) cv_box(x0 - 2, ry - 1, 4, 6, C_RED);   /* (END STOP: it ends here) */
    }
    if (curves) {                               /* PITCH, LANE, MACRO: their curves, bright (PITCH: XPOS A, XPOS B,
                                                   LOOP, FREEZE; LANE: the two lanes; MACRO: the four) */
        static const uint8_t ROWS[3][4] = {{0, 1, 8, 9}, {2, 3, 0xFF, 0xFF}, {4, 5, 6, 7}};
        const uint8_t *rw = ROWS[layer == LO_PITCH ? 0 : layer == LO_LANE ? 1 : 2];
        for (r = 0; r < 4u; r++)
            if (rw[r] != 0xFFu) orb_curve(rw[r], 40 + (int32_t)r * 13, 5, cum, total, len, (int32_t)pick, C_ICE);
    } else {                                    /* SONG, SETUP: the first two that move, dim */
        static const uint8_t ROWS[6] = {0, 1, 8, 9, 2, 3};
        uint32_t n = 0;
        for (r = 0; r < 6u && n < 2u; r++) {
            uint32_t moves = 0;
            for (i = 0; i < len; i++) moves |= orb_vat(ROWS[r], i) != 0;
            if (!moves) continue;
            orb_curve(ROWS[r], 72 + (int32_t)n * 19, 7, cum, total, len, -1, C_ICE_D);
            n++;
        }
    }
    if (orbp.on && eng.seq_run && eng.orb_shown < len) {   /* where it plays now: its step, the loops done there, the
                                                   way through this loop (the master's, measured) */
        uint32_t st = eng.orb_shown, ld = st == eng.orb_step ? eng.orb_loops : 0u, sb = eng.orb_loop_sb, t = eng.orb_loop_t;
        uint32_t f = sb ? (t >= sb ? 255u : t * 256u / sb) : 0u, pos = (cum[st] + ld) * 256u + f;
        int32_t x = OT_X0 + (int32_t)(pos * OT_W / (total * 256u));
        cv_rect(x, 0, 1, bottom, C_RED);
    }
}

/* DRUM: the four drums' rows of 16 steps (in fours, as the white keys), 17 rows apart (2026-11-22, the user took the
 * sheet of tidier pages: more air), the drum edited's name red with a bar at the left, a muted one's name and steps
 * dark (its MUTE key's state: the line of MUTE lamps under them gone). Each step a button, square, a pixel off each
 * corner as the panel's boxes: off dark, ON lit dim, ACCENT lit full, RADAR dark with its lamp (red: RADAR on); the
 * step SELECT picked framed red, the one playing a red pip under its column. Under them RADAR's loop, a lamp a place
 * (lit: a 1, the place now red); at the bottom the line that says something: REC's, with GLO the black keys' jobs, in
 * MORE the drum edited's ROLLs (x2..x4) and PROBs (a bar shorter than full) */
static int32_t dr_x(uint32_t i) { return 36 + (int32_t)i * 12 + (int32_t)(i / 4u) * 3; }
static void pic_drum(void)
{
    uint32_t d, i, play = eng.dr.on && eng.seq_run ? eng.dr.shown : 0xFFu, on = eng.dr.on, rad = SD_TUR_ON(&pat);
    uint32_t pc = set.dperc % SD_PCS, dc = dr_cur % SD_DRS, pick = dr_step % SD_DR_STEPS;
    for (d = 0; d < SD_DRS; d++) {
        int32_t by = 2 + (int32_t)d * 17;
        uint32_t mu = dr_mute >> d & 1u;
        if (d == dc) cv_rect(0, by, 2, 10, C_RED);
        cv_text(5, by - 3, &FONT_S, sd_dr_short(d, pc), d == dc ? (mu ? C_RED_D : C_RED) : mu ? C_OFF : C_ICE);
        for (i = 0; i < SD_DR_STEPS; i++) {
            int32_t x = dr_x(i);
            uint32_t st = SD_DR_STEP(&set.dpat, d, i), lit = on && !mu;
            cv_box(x, by, 10, 10, st == SD_DS_OFF ? C_GRID : mu ? C_OFF : st == SD_DS_ACC ? (lit ? C_ICE : C_ICE_D) :
                                  st == SD_DS_ON ? (lit ? C_ICE_D : C_OFF) : C_GRID);
            if (st == SD_DS_RADAR) cv_box(x + 3, by + 3, 4, 4, rad && lit ? C_RED : C_ICE_D);
            if (d == dc && i == pick) cv_frame(x - 2, by - 2, 14, 14, C_RED);
        }
    }
    if (play < SD_DR_STEPS) cv_rect(dr_x(play), 66, 10, 2, C_RED);
    cv_text(5, 70, &FONT_S, "RAD", rad ? C_ICE_D : C_GRID);
    {                                           /* RADAR's loop: place p's bit (the newest the place now) */
        uint32_t len = SD_TUR_LEN(&pat), reg = eng.tur_reg, cur = eng.tur_place;
        if (cur >= len) cur = 0;
        for (i = 0; i < len && i < SD_DR_STEPS; i++) {   /* (a lamp a place, not a digit: the font's reads
                                                   took the synth's code from the flash's cache, 1.5 points) */
            uint32_t b = reg >> ((cur + len - i) % len) & 1u;
            cv_box(dr_x(i) + 2, 75, 6, 6, !rad ? C_GRID : i == cur ? C_RED : b ? C_ICE_D : C_GRID);
            if (rad && b && i != cur) cv_rect(dr_x(i) + 4, 77, 2, 2, C_ICE);
        }
    }
    if (dr_rec) { cv_text(6, 88, &FONT_S, "REC: THE PADS INTO THE STEPS", C_RED); return; }
    if (dr_mix || lay_is(PAGE_DRUM, LK_KIT)) {   /* MIX, KIT: the four as a mixer's strips, each its name, its level
                                                   a bar, its pan a pip under it (the drum edited red) */
        for (d = 0; d < SD_DRS; d++) {
            const SdDrumP *dp = &set.dkit.d[d];
            int32_t x = 6 + (int32_t)d * 58, w = 48 * (int32_t)dp->level / 100, px = x + 24 + dp->pan * 24 / 60;
            uint16_t c = d == dc ? C_RED : C_ICE_D;
            cv_text(x, 86, &FONT_S, sd_dr_short(d, pc), c);
            cv_rect(x + 26, 92, 26, 1, C_GRID);
            cv_rect(x + 26, 91, w * 26 / 48, 3, c);
            cv_rect(x + 26 + (px - x) * 26 / 48 - 1, 96, 3, 4, c);
        }
        return;
    }
    if (dr_more) {                              /* MORE: the drum edited's ROLLs and PROBs */
        cv_text(5, 86, &FONT_S, "ROL", C_ICE_D);
        for (i = 0; i < SD_DR_STEPS; i++) {
            uint32_t r = SD_DR_ROLL(&set.dmore, dc, i), pb = SD_DR_PROB(&set.dmore, dc, i);
            int32_t x = dr_x(i);
            if (r) { char t[2] = {(char)('1' + r), 0}; cv_text(x + 1, 86, &FONT_S, t, i == pick ? C_RED : C_ICE_D); }
            else cv_rect(x + 4, 93, 2, 2, C_GRID);
            if (pb) cv_rect(x, 102, 10 - (int32_t)pb * 2, 2, i == pick ? C_RED : C_ICE_D);
        }
    }
}

static void pic(void)
{
    switch (page) {
    case 0: pic_nav(); break;
    case 5: pic_chain(); break;
    case PAGE_SETUP: break;                     /* (SETUP, MOD: page_full's) */
    case 1: pic_reactor(); break;
    case 2: pic_dest(); break;
    case 3: pic_space(); break;
    case PAGE_TIME: pic_time(); break;
    case PAGE_LOOP: pic_loop(); break;
    case PAGE_ENV: pic_env(); break;
    case PAGE_FILT: pic_filt(); break;
    case PAGE_SUB: pic_sub(); break;
    case 4: if (dmod == DMOD_TUR) pic_tur(); else if (dmod >= SD_MODS) pic_lfo(dmod - SD_MODS); break;
    case PAGE_ARP: pic_arp(); break;
    case PAGE_ORB: pic_orbit(); break;
    case PAGE_DRUM: pic_drum(); break;
    default: pic_seq(); break;
    }
}

/* ---- the hint: two fixed places, as the legends under a panel's controls (labels dim, values ice): on the left
   ALGORITHM's job (where it has none, the page's other word), from the middle the second control's (GLO, a black
   key, REC, OCT). GLO held: ALGORITHM's second job on the left, its name red as the knobs'. A toggle has a lamp, as
   the top line's. A message, OFF: EDIT TURNS IT ON, a length the line cannot hold: the whole line */
#define HINT_R 126                              /* (the right place: the longest left, 2>3>4 1>4, ends at 118) */
#if defined(DINGHY_HOST) && (defined(__APPLE__) || defined(DINGHY_WEB))
#define SD_POOLED                               /* (a small state of the screen's in the pool: the RAM's .bss is full) */
#else
#define SD_POOLED __attribute__((section(".pool")))
#endif
/* each page's line the first time it opens since power-on (2026-11-23, "HOME more customizable?": what a page is, 2.5 s
   in the hint's place); MOD and RADAR apart from the LFOs on their page (help_id) */
static const char *const HELP[18] = {"NAV: THE VOICE ON X/Y", "REACTOR: A RINGING BODY", "DISTROY: BANDS DESTROYED",
    "SPACE: PLATE AND FREEZE", "LFO: A WAVE FOR MOD", "MACROS: FOUR BIG KNOBS", "SEQ: STEPS CHORDS PARTS",
    "ENV: THE SHAPE OF A NOTE", "FILTER: CUTOFF RES ENV 3", "ARP: HELD NOTES IN TURN", "ORBIT: THE SONG IN STEPS",
    "TIME: THE DELAY", "LOOPER: A TAPE ON LOOP", "SETUP: TEMPO MIDI CORES", "DRUM: THE DRUMS", "SUB: AN OCTAVE UNDER",
    "MOD: WHAT MOVES WHAT", "RADAR: A LOOP OF BITS"};
#ifndef SD_HELP_MS
#define SD_HELP_MS 2500u                        /* (0: none; the README's screens are made so: their hints) */
#endif
static uint32_t help_id(void) { return page == 4u ? (dmod == DMOD_TUR ? 17u : dmod < SD_MODS ? 16u : 4u) : page & 15u; }
static int32_t hint_x;
static void hint_c(const char *l, const char *v, uint16_t lc)
{
    if (!l) return;
    cv_text(hint_x, 0, &FONT_S, l, lc);
    hint_x += text_w(&FONT_S, l) + 8;
    if (v) { cv_text(hint_x, 0, &FONT_S, v, C_ICE); hint_x += text_w(&FONT_S, v) + 16; }
}
static void hint(const char *l, const char *v) { hint_c(l, v, C_ICE_D); }
static void hint_algo(const char *v, uint32_t glo) { hint_x = 6; hint_c("ALGO", v, glo ? C_RED : C_ICE_D); }
static void hint_two(const char *l, const char *v) { hint_x = HINT_R; hint(l, v); }   /* the right place */
static uint32_t page_off(void)                  /* an engine off, ARP, RADAR, the LOOPER disarmed: its knobs set it,
                                                   silent (not SEQ's pages: a part is written while it is off) */
{
    return (page_fx() && !fx_on(page_fx())) || (page == PAGE_LOOP && !lp_arm) || (page == PAGE_ARP && !eng.arp_on) ||
           (page == 4u && dmod == DMOD_TUR && !SD_TUR_ON(&pat));
}
static int32_t draw_layers(const char *const *ln, uint32_t n, uint32_t cur)   /* ALGORITHM's row: a button a layer,
                                                   the one the knobs do lit; the x after it */
{
    uint32_t k;
    int32_t x = 2, tw = 0, pad = 10, gap = 3;   /* (pad: the frame's two and 4 either side of the ink) */
    for (k = 0; k < n; k++) tw += ink_col(ln[k], 1) - ink_col(ln[k], 0) + 1;
    if (tw + (int32_t)n * pad + ((int32_t)n - 1) * gap > 236) gap = 2;   /* (five: closer) */
    if (tw + (int32_t)n * pad + ((int32_t)n - 1) * gap > 236) pad = 8;   /* (DRUM's six: 3 either side) */
    for (k = 0; k < n; k++) {
        int32_t w = ink_col(ln[k], 1) - ink_col(ln[k], 0) + 1 + pad;
        if (k == cur) cv_box(x, 0, w, 16, C_RED);
        else cv_edge(x, 0, w, 16, C_ICE_D);
        btn_text(x, 0, w, ln[k], k == cur ? C_BG : C_ICE_D);
        x += w + gap;
    }
    return x;
}
static void draw_hint(void)
{
    char a[16], b[16], *p;
    cv_begin(240, 16, C_BG);
    hint_x = 6;
    if (msg_txt && fm1_ms - msg_at < 1200u) { cv_text(6, 0, &FONT_S, msg_txt, C_RED); return; }   /* (then the hint
                                                   again) */
    msg_txt = 0;
    if (pat_waiting && eng.seq_run && (eng.t[0].pat_wait || eng.t[1].pat_wait || eng.orb_wait || eng.dr.pat_wait)) {   /* a
                                                   pattern (a song, a beat) loaded while it plays: blinking till the
                                                   one takes it */
        char w[24];
        p = put(put_place(w, pat_waiting >> 9 ? 4u : pat_waiting >> 8 ? 3u : 2u, (pat_waiting & 0xFFu) - 1u), ": AT THE ONE"); *p = 0;
        if ((fm1_ms / 250u) & 1u) cv_text(6, 0, &FONT_S, w, C_RED);
        return;
    }
    pat_waiting = 0;
    if (page == PAGE_LOOP && lp_clr_at) { cv_text(6, 0, &FONT_S, "CLEAR THE LOOP? OCT+ YES", C_RED); return; }
    if (page == PAGE_DRUM && dr_clr_at) { cv_text(6, 0, &FONT_S, "CLEAR THE ROW? OCT+ YES", C_RED); return; }
    if (page == PAGE_SETUP && clr_all_at) {     /* (the whole line: the question TOOLS asks) */
        if (setup_ask >= 2u) {                  /* DEMO 1 SINGULARITY? OCT+ YES (29 characters at most: the line) */
            char d[40];
            p = put(d, "DEMO "); *p++ = (char)('1' + (setup_ask - 2u)); *p++ = ' ';
            p = put(p, DEMOS[(setup_ask - 2u) & 1u].name); p = put(p, "? OCT+ YES"); *p = 0;
            cv_text(6, 0, &FONT_S, d, C_RED);
        } else cv_text(6, 0, &FONT_S, "CLEAR ALL? OCT+ YES, OCT- NO", C_RED);
        return;
    }
    if (page == PAGE_SETUP && setup_log) { hint("SELECT", "OLDER, LEFT: BACK"); return; }   /* (the log) */
    {                                           /* a page new since power-on: its line, 2.5 s (not while GLO is held) */
        static struct { uint32_t seen, at; uint8_t id; } hl SD_POOLED;
        uint32_t id = help_id();
        if (id + 1u != hl.id) {
            hl.id = (uint8_t)(id + 1u);
            hl.at = hl.seen >> id & 1u ? 0u : fm1_ms ? fm1_ms : 1u;
            hl.seen |= 1u << id;
        }
        if (SD_HELP_MS && hl.at && fm1_ms - hl.at < SD_HELP_MS && !shift()) { cv_text(6, 0, &FONT_S, HELP[id], C_ICE_D); return; }
    }
    if (shift() && seq_page()) {                /* GLO held on SEQ: ALGORITHM the scale; IN KEY the key (the master's:
                                                   on the other track's page, KEY) */
        const SdPattern *kp = set.xchrom ? &seqp : key_pat();
        p = put_note(a, sd_pat_key(kp)); while (p > a && p[-1] >= '0' && p[-1] <= '9') p--;   /* (the root's name) */
        *p++ = ' '; *put(p, SCALE_NAME[kp->scale % SD_SCALES]) = 0;
        if (!set.xchrom && key_trk() != seq_trk) hint("KEY", a);
        else { hint_algo(a, 1); hint_x = HINT_R; hint_c("SELECT", "ROOT", C_RED); }   /* (GLO + SELECT) */
        return;
    }
    if (shift() && (page == PAGE_ORB || page == PAGE_DRUM)) {   /* GLO held: the white keys the steps (DRUM: GLO +
                                                   SELECT the drum) */
        hint_c("KEYS", "STEPS", C_RED);
        if (page == PAGE_DRUM) { hint_x = HINT_R; hint_c("SELECT", "DRUM", C_RED); }
        return;
    }
    {                                           /* ALGORITHM's row: the layers; on the right, where it is short, the
                                                   page's other word */
        const char *const *ln;
        uint32_t n = lay_names(&ln), off = page_off();
        if (n > 1u) {
            if (draw_layers(ln, n, lay()) > HINT_R - 4) return;
            switch (page) {
            case 3:                             /* SPACE: FREEZE's lamp (its layer's K1, there its button) */
                if (off) { hint_two("OFF:", "EDIT ON"); break; }
                if (lay()) break;
                cv_box(HINT_R, 3, 8, 8, sp_frz ? C_RED : C_GRID);
                if (sp_frz) cv_rect(HINT_R + 2, 5, 2, 2, C_GLINT);
                cv_text(HINT_R + 11, 0, &FONT_S, "FREEZE", sp_frz ? C_ICE : C_ICE_D);
                break;
            case PAGE_TIME: {                   /* TIME: past its line, what it plays (TIME red) */
                char pd[12];
                if (sp_fold(pd)) hint_two("PLAYS", pd);
                else if (off) hint_two("OFF:", "EDIT ON");
                break;
            }
            case PAGE_ARP: if (off) hint_two("OFF:", "EDIT ON"); else hint_two("REC", "KEEP"); break;
            case 5:                             /* MACROS: the switches on the keys, with GLO (the picture draws them) */
                if (shift()) { hint_x = HINT_R; hint_c("KEYS", "ON/OFF", C_RED); }
                else hint_two("GLO+KEY", "ON/OFF");
                break;
            default: if (off) hint_two("OFF:", "EDIT ON"); break;
            }
            return;
        }
        if (off) { hint("OFF:", "EDIT TURNS IT ON"); return; }
        if (page == 4u && dmod < SD_MODS) {     /* MOD: the slot SELECT picks, its amount (the table has its bar) */
            p = put(a, "SLOT "); *put_u(p, dmod + 1u) = 0;
            hint("SELECT", a);
            *put_sgn(b, amt99(pat.mod[dmod].amt)) = 0;
            hint_two("AMOUNT", b);
        }
    }
}

static void draw_status(void)
{
    char s[40], *p = s;
    int32_t oct = (int8_t)set.oct;
    uint32_t run = eng.seq_run;
    cv_begin(240, 18, C_BG);
    if (eng.clk_on) {                           /* a MIDI clock leads: its tempo (--- until a beat is measured), CLK */
        uint32_t b = eng.clk_beat;
        p = b ? put_u(p, (SD_FS * 60u + b / 2u) / b) : put(p, "---");
        p = put(p, " CLK OCT");
    } else {
        p = put_u(p, demo_st ? eng.bpm : set.bpm);   /* (the demo: its own) */
        p = put(p, " BPM OCT");
    }
    *p++ = oct < 0 ? '-' : '+';
    *p++ = (char)('0' + (oct < 0 ? -oct : oct));
    *p = 0;
    cv_box(8, 4, 8, 10, run ? C_RED : C_ICE_D);   /* play: a square, red; stopped: dim */
    cv_text(22, 1, &FONT_S, s, run ? C_ICE : C_ICE_D);
    {                                           /* the mode, always (the CPU is SETUP's), then the voices, a box each
                                                   (2026-11-22, the user: how many are used, how many left): lit with
                                                   who holds it, A B the tracks, R ARP; an empty frame the keys' (free) */
        uint32_t k, i, n = 0, x;
        static const char L[4] = {'-', 'A', 'B', 'R'};
        static const uint8_t ORD[4] = {SD_SRC_A, SD_SRC_B, SD_SRC_ARP, SD_SRC_HAND};
        for (k = 0; k < 4u; k++) n += eng.quota[ORD[k]];
        const char *mn = MODE_NAME[eng.mode_on < SD_MODES ? eng.mode_on : 0];
        x = 236u - 12u * (n > 4u ? 4u : n);
        cv_text((int32_t)x - 6 - text_w(&FONT_S, mn), 1, &FONT_S, mn, C_ICE);   /* (the mode: how many voices) */
        for (k = 0; k < 4u; k++)
            for (i = 0; i < eng.quota[ORD[k]] && x < 236u; i++, x += 12u) {
                char c[2] = {L[ORD[k]], 0};
                int32_t bx = (int32_t)x;
                if (ORD[k] == SD_SRC_HAND) {    /* (free: the box's outline, its corners a pixel off as the lit one's) */
                    cv_rect(bx + 1, 2, 8, 1, C_ICE_D); cv_rect(bx + 1, 13, 8, 1, C_ICE_D);
                    cv_rect(bx, 3, 1, 10, C_ICE_D); cv_rect(bx + 9, 3, 1, 10, C_ICE_D);
                } else { cv_box(bx, 2, 10, 12, C_ICE); cv_text(bx + 1, 1, &FONT_S, c, C_BG); }
            }
    }
}

/* ---- a frame: every region drawn, each sent only when its pixels changed */
static uint32_t cv_hash(void)
{
    uint32_t h = 2166136261u, i, n = cv_w * cv_h;
    for (i = 0; i < n; i++) h = (h ^ cv_px[i]) * 16777619u;
    return h;
}
static uint32_t shown[8] = {~0u, ~0u, ~0u, ~0u, ~0u, ~0u, ~0u, ~0u};
static void cv_show(uint32_t y, uint32_t r)
{
    uint32_t h = cv_hash();
    if (h != shown[r]) { shown[r] = h; cv_blit(0, y); }
}
/* the demo's screen: its name on the tab, where it is (the time, the bar, a bar of the whole), what the player does
 * now and just before (as the performance says it), the way out */
static char demo_said[3][32];                   /* the newest first */
static uint32_t demo_says_seen;
static void demo_take_said(void)
{
    uint32_t k, j = 0;
    if (demo.says == demo_says_seen) return;
    demo_says_seen = demo.says;
    if (demo.say_len && demo.s[demo.say_off] == '(') return;   /* (the player's notes, not a move) */
    for (k = 2; k > 0u; k--) for (j = 0; j < 32u; j++) demo_said[k][j] = demo_said[k - 1u][j];
    for (k = 0, j = 0; k < demo.say_len && j < 29u && demo.say_off + k < demo.n; k++) {
        char c = (char)demo.s[demo.say_off + k];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        if (c >= 32 && c <= 126) demo_said[0][j++] = c;
    }
    demo_said[0][j] = 0;
}
static void draw_demo(void)
{
    char s[32], *p;
    const DemoSong *d = &DEMOS[demo_k & 1u];
    uint32_t r = demo_r, k, played = r > d->settle ? r - d->settle : 0u, all = d->renders - d->settle;
    uint32_t sec = played * 256u / SD_FS, tot = all * 256u / SD_FS;
    uint32_t bar = played * 256u / 60u * d->bpm / SD_FS * 4u / 16u + 1u;   /* (sixteenths: frames x BPM x 4 / 60) */
    int32_t w;
    demo_take_said();
    cv_begin(240, 20, C_BG);
    w = text_w(&FONT_S, "DEMO");
    cv_rect(0, 1, w + 22, 18, C_RED);
    for (k = 0; k < 18u; k++) cv_line(w + 22, 1 + (int32_t)k, w + 30 - (int32_t)k * 8 / 18, 1 + (int32_t)k, C_RED);
    cv_text(10, 2, &FONT_S, "DEMO", C_BG);
    cv_text(w + 40, 2, &FONT_S, d->name, C_ICE);
    cv_show(0, 1);
    cv_begin(240, 20, C_BG);
    p = put_u(s, sec / 60u); *p++ = ':'; *p++ = (char)('0' + sec % 60u / 10u); *p++ = (char)('0' + sec % 10u);
    p = put(p, " / "); p = put_u(p, tot / 60u); *p++ = ':'; *p++ = (char)('0' + tot % 60u / 10u);
    *p++ = (char)('0' + tot % 10u); *p = 0;
    cv_text(6, 1, &FONT_S, s, C_ICE);
    p = put_u(put(s, "BAR "), bar); *p = 0;
    cv_text(234 - text_w(&FONT_S, s), 1, &FONT_S, s, C_ICE_D);
    cv_show(Y_NAV, 2);
    cv_begin(240, H_KNOB, C_BG);                /* the whole of it, and where it is */
    cv_frame(6, 16, 228, 14, C_ICE_D);
    w = (int32_t)(played < all ? played * 224u / all : 224u);
    if (w > 0) cv_box(8, 18, w, 10, demo_st == DEMO_PLAY ? C_RED : C_ICE_D);
    for (k = 1; k < 4u; k++) cv_line(6 + (int32_t)k * 57, 32, 6 + (int32_t)k * 57, 35, C_GRID);
    cv_show(Y_KNOB, 3);
    cv_begin(240, H_PIC, C_BG);                 /* what the player does */
    cv_text(6, 6, &FONT_S, demo_st == DEMO_START ? "STARTING ..." : demo_st >= DEMO_FADE ? "ENDING ..." :
                           demo_st == DEMO_TAIL ? "THE LAST NOTES RING OUT" : "THE PLAYER:", C_ICE_D);
    for (k = 0; k < 3u; k++) cv_text(6, 34 + (int32_t)k * 22, &FONT_S, demo_said[k], k ? C_ICE_D : C_ICE);
    cv_show(Y_PIC, 4);
    cv_begin(240, 16, C_BG);
    cv_text(6, 0, &FONT_S, "ANY KEY OR KNOB STOPS IT", C_ICE_D);
    cv_show(Y_HINT, 5);
    draw_status(); cv_show(Y_STAT, 7);
}
static void demo_go(uint32_t k)                 /* SETUP's DEMO k + 1, OCT+: what the player has in the engine kept,
                                                   then it starts (the audio's next interrupt) */
{
    uint32_t i;
    demo_k = (uint8_t)(k & 1u);
    if (aud_c) aud_stop();
    if (seq_rec) rec_toggle();
    for (i = 0; i < SD_DEMO_NF; i++) demo_keep[i] = sd_demo_get(&eng, i);
    dr_keep_on = eng.dr.on;                     /* (the drums: back on after it, if they were) */
    for (i = 0; i < 3u; i++) demo_said[i][0] = 0;
    demo_says_seen = 0;
    demo_st = DEMO_START;
}
static void demo_restore(void)                  /* after it: the engine fresh, all the player had there handed back */
{
    uint32_t i;
    for (i = 0; i < SD_DEMO_NF; i++) sd_demo_put(&eng, i, demo_keep[i]);
    eng.seq_run = 0;
    sd_engine_patch(&eng, &pat);
    sd_engine_pattern_to(&eng, seq_trk, &seqp);
    sd_engine_pattern_to(&eng, seq_trk ^ 1u, &seq_other);
    orb_push();
    eng.pats = useq.p;
    eng.pats_used = useq.used;
    dr_places();
    dr_kit_push();
    dr_push(0);
    eng.dr.on = dr_keep_on;
    eng.filt = home_filt;
    demo_st = DEMO_OFF;
    msg("DEMO OVER: YOUR SOUND BACK");
}
static void draw_frame(void)
{
    static struct { uint32_t blank; uint8_t over, was_tall, ok; } hb SD_POOLED;   /* an empty hint row's hash; RADAR:
                                                   the hint drawn over its picture's foot; RADAR the last frame */
    uint32_t tall = page == 4u && dmod == DMOD_TUR && !save_mode && !saved_at && !browse_at && !dlist_at, hint_on = 0;
    if (!hb.ok) { cv_begin(240, 16, C_BG); hb.blank = cv_hash(); hb.ok = 1; }
    if (demo_st) { draw_demo(); return; }
    draw_info(); cv_show(0, 1);
    draw_nav(); cv_show(Y_NAV, 2);
    static uint32_t listed;                     /* the list was up: the two rows between the knobs and the picture
                                                   (neither draws them) cleared when it closes */
    if (browse_at || dlist_at) {                /* PRESETS or a DEST turning: the list over the knobs and the picture,
                                                   in two strips (the canvas holds 124 rows): its first 120, the rest */
        uint32_t y;
        for (y = 0; y < H_LIST; y += 120u) {
            cv_begin(240, H_LIST - y > 120u ? 120u : H_LIST - y, C_BG);
            cv_oy = -(int32_t)y;
            if (browse_at) draw_browse(); else draw_dlist();
            cv_oy = 0;
            cv_show(Y_KNOB + y, y ? 6u : 3u);
        }
        shown[4] = ~0u;
        listed = 1;
    } else if (page_full() && !save_mode && !saved_at) {   /* MOD, SETUP: over the knobs' row and the picture
                                                   at once, as the lists */
        uint32_t y;
        for (y = 0; y < H_LIST; y += 120u) {
            cv_begin(240, H_LIST - y > 120u ? 120u : H_LIST - y, C_BG);
            cv_oy = -(int32_t)y;
            if (page == PAGE_SETUP) pic_setup_full(); else pic_mod_full();
            cv_oy = 0;
            cv_show(Y_KNOB + y, y ? 6u : 3u);
        }
        shown[4] = ~0u;
        listed = 1;
    } else {
        if (listed) { lcd_fill(0, Y_KNOB + H_KNOB, 240, Y_PIC - Y_KNOB - H_KNOB, C_BG); listed = 0; shown[6] = ~0u; }
        draw_knobs(); cv_show(Y_KNOB, 3);
        if (tall) {                             /* RADAR's tall picture: is there a hint to draw over its foot? */
            draw_hint();
            hint_on = cv_hash() != hb.blank;
            if (hint_on != hb.over) shown[4] = ~0u;   /* (the picture back where the hint was, or the hint new) */
        }
        cv_begin(240, tall ? H_PIC_TUR : H_PIC, C_BG);
        if (save_mode || saved_at) draw_save(); else pic();
        {
            uint32_t was = shown[4];
            cv_show(Y_PIC, 4);
            if (tall && shown[4] != was) shown[5] = ~0u;   /* (the picture blitted over the hint's row: again) */
        }
    }
    if (hb.was_tall && !tall) {                 /* left RADAR: its picture lies in the hint's row, and in the two
                                                   between it and the picture's (no page draws them) */
        shown[5] = ~0u;
        lcd_fill(0, Y_PIC + H_PIC, 240, Y_HINT - Y_PIC - H_PIC, C_BG);
    }
    hb.was_tall = (uint8_t)tall;
    hb.over = (uint8_t)(tall && hint_on);
    if (!tall || hint_on) { draw_hint(); cv_show(Y_HINT, 5); }
    draw_status(); cv_show(Y_STAT, 7);
}

/* ---- the key LEDs show what the keys do on this page, as the screen does:
 * playing, the notes on (full) and ringing out (a glow); ARP, its notes glow. SEQ and RADAR: the bank's steps on the
 * white keys (a note full, a rest a glow, past the length dark; the playhead flips its key), the black keys' actions
 * a glow (SEG full while on); GLO held there: the notes a key may transpose the track to a glow (IN KEY the key's),
 * the track's place full, the next blinking until the one takes it. */
static void leds_set(uint32_t full, uint32_t glow)
{
    static uint32_t full_was = ~0u, glow_was = ~0u;
    uint32_t k;
    glow &= ~full;
    if (full == full_was && glow == glow_was) return;
    full_was = full;
    glow_was = glow;
#ifdef SD_LED_TRACE
    printf("%6u ms  keys full %07x glow %07x\n", fm1_ms, full, glow);
#endif
    for (k = 0; k < 27u; k++) {
        fm1_led_key(KEY_LED(k), full >> k & 1u);
        fm1_led_key_glow(KEY_LED(k), glow >> k & 1u);
    }
}
static void keys_lit(uint32_t held, uint32_t base)
{
    uint32_t full = 0, glow = 0, k, i;
    for (k = 0; k < 11u; k++) bk_led[k] = 0;
    if (page == PAGE_ORB && !seq_rec && shift()) {   /* ORBIT, GLO: its steps on the first white keys (picked full,
                                                   the one playing flips); else as played */
        uint32_t len = SD_ORB_LEN(&orbp), play = eng.seq_run && orbp.on ? eng.orb_shown : 0xFFu;
        for (k = 0; k < 27u; k++) {
            int32_t w = key_white(k);
            uint32_t on;
            if (w < 0 || (uint32_t)w >= len) continue;
            on = (uint32_t)w == orb_cur;
            if ((uint32_t)w == play) on = !on;
            if (on) full |= 1u << k; else glow |= 1u << k;
        }
    } else if (page == PAGE_DRUM && !shift()) {   /* DRUM: the pads (the drum edited's full, the others a glow), the
                                                   MUTE keys (full: it sounds) */
        for (k = 0; k < 27u; k++) {
            int32_t w = key_white(k), bk = key_black(k);
            if (w >= 0 && w < 8) { if (((uint32_t)w & 3u) == dr_cur % SD_DRS) full |= 1u << k; else glow |= 1u << k; }
            else if (bk >= 5 && bk <= 8 && !(dr_mute >> (bk - 5) & 1u)) full |= 1u << k;
            if (bk >= 0) bk_led[bk] = bk >= 5 && bk <= 8 ? (uint8_t)(dr_mute >> (bk - 5) & 1u ? 0u : 2u) : 0u;
        }
    } else if (page == PAGE_DRUM) {             /* DRUM, GLO: the drum's steps on the white keys (ON a glow, ACCENT
                                                   full, RADAR blinking; the one playing flips) */
        uint32_t play = eng.dr.on && eng.seq_run ? eng.dr.shown : 0xFFu, blink = (fm1_ms / 200u) & 1u;
        for (k = 0; k < 27u; k++) {
            int32_t w = key_white(k);
            uint32_t tl = 0;
            if (w >= 0 && (uint32_t)w < SD_DR_STEPS) {
                uint32_t st = SD_DR_STEP(&set.dpat, dr_cur, (uint32_t)w);
                tl = st == SD_DS_ACC ? 2u : st == SD_DS_ON ? 1u : st == SD_DS_RADAR ? blink : 0u;
                if ((uint32_t)w == play) tl = tl ? 0u : 2u;
            }
            if (tl == 2u) full |= 1u << k; else if (tl) glow |= 1u << k;
        }
    } else if (page == 5u && shift()) {         /* MACROS, GLO: F3..E4 switch (home_toggle): full where on, a glow
                                                   where off; the other keys as played */
        full = held & ~0u;
        for (k = 0; k < 27u; k++) {
            int32_t i = home_sw(k), bk = key_black(k);
            if (i < 0) continue;
            full &= ~(1u << k);
            if (home_on((uint32_t)i)) full |= 1u << k; else glow |= 1u << k;
            if (bk >= 0) bk_led[bk] = (uint8_t)(full >> k & 1u ? 2u : 1u);
        }
    } else if (seq_page() && !seq_rec && shift()) {   /* GLO: on every key the notes a key may take the track to (IN
                                                   KEY the key's), as printed: OCT's octave left out (it is the
                                                   playing's); where it is lit, where it goes blinking */
        int32_t xn = eng.t[seq_trk].xpose_next, xc = eng.t[seq_trk].xpose, z = (int32_t)key_none() - (int32_t)KEY0;
        for (k = 0; k < 27u; k++) if (key_has(KEY0 + k)) glow |= 1u << k;
        if (z + xc >= 0 && z + xc < 27) full |= 1u << (z + xc);
        if (z + xn >= 0 && z + xn < 27 && (xn == xc || (fm1_ms / 200u) & 1u)) full |= 1u << (z + xn);
    } else {
        full = held;
        for (i = 0; i < SD_VOICES; i++) {
            const SdVoice *v = &eng.voice[i];
            uint32_t n = v->note;
            if (n < base || n >= base + 27u || v->env1.stage == SD_ENV_OFF) continue;
            if (v->gate) full |= 1u << (n - base);
            else glow |= 1u << (n - base);
        }
        if (eng.arp_on)                         /* the arpeggio's notes */
            for (i = 0; i < eng.arp_n && i < 8u; i++) {
                uint32_t n = eng.arp_note[i];
                if (n >= base && n < base + 27u) glow |= 1u << (n - base);
            }
    }
    leds_set(full, glow);
}

/* MIDI clock and transport (SYNC AUTO, SETUP: INT): the pulses to the engine (sd_clk_tick), START
 * and CONTINUE from step 1 at the next pulse (the downbeat; with none in 250 ms, at once), STOP. CONTINUE starts
 * from the top too: the song position is not passed on. Two clocks (USB and the jack; in the browser, two of its
 * MIDI inputs): the first, until it has been quiet for 500 ms (the same clock on both leads once: counted twice it
 * was twice the tempo) */
static uint8_t clk_cue, clk_src;
static uint32_t clk_cue_t, clk_src_t;
static void clk_rt(uint32_t st, int src)
{
    if (set.sync_int || (clk_src && src != clk_src && fm1_ms - clk_src_t < 500u)) return;
    clk_src = (uint8_t)src;
    clk_src_t = fm1_ms;
    if (st == 0xF8u) {
        eng.clk_in++;
        if (clk_cue) { clk_cue = 0; eng.seq_run = 1; eng.clk_start++; }
    } else if (st == 0xFAu || st == 0xFBu) {
        clk_cue = 1;
        clk_cue_t = fm1_ms;
    } else if (st == 0xFCu) {
        clk_cue = 0;
        eng.seq_run = 0;
    }
}
/* A note from MIDI (a keyboard on USB or the TRS jack): played as the keys' play, on every page (ARP takes it while
 * on, the voices' rules as theirs), and in REC written as theirs, at its velocity: stopped, step by step (held
 * together, a chord; all up, the next step), playing, into the step nearest where it was heard (rec_live); what is
 * heard (sd_seq_rec_note). The
 * keys' and MIDI's held notes count together */
static void midi_note(uint32_t n, uint32_t v)
{
    uint32_t bit = 1u << (n & 31u), *w = &midi_held[n >> 5 & 3u];
    if (v) {
        uint32_t others = midi_hn || fm1_in.notes;
        sd_engine_note(&eng, n, v);
        if (!(*w & bit)) { *w |= bit; midi_hn++; }
        if (seq_rec) rec_live(sd_seq_rec_note(&eng, &eng.t[seq_trk], n), v, others);
        else if (!eng.arp_on && eng.seq_run && !eng.quota[SD_SRC_HAND]) {
            msg(set.mode == SD_MODE_MONO ? "MONO: THE SEQ HAS THE VOICE" : "NO FREE VOICE: SEQ PLAYS");
        }
    } else {
        sd_engine_note(&eng, n, 0);
        if (*w & bit) { *w &= ~bit; midi_hn--; }
        if (seq_rec && !midi_hn && !fm1_in.notes) seq_keys_up();   /* every key up: the next step */
    }
}
static void midi_in(void)
{
    uint8_t st, d1, d2;
    int src;
    while ((src = midi_get(&st, &d1, &d2)) != 0) {
        uint32_t ty = st & 0xF0u;
        if (demo_st) continue;                  /* (the demo plays: MIDI in, its clock too, not heard) */
        if (st >= 0xF8u) clk_rt(st, src);
        else if (ty == 0x90u || ty == 0x80u) midi_note(d1 & 127u, ty == 0x90u ? d2 : 0u);
        else if (ty == 0xB0u && d1 == 1u) eng.wheel = d2;   /* C1: the matrix's WHEEL */
        else if (ty == 0xD0u) eng.after = d1;   /* and AFTER (channel pressure) */
    }
    if (clk_cue && fm1_ms - clk_cue_t > 250u) { clk_cue = 0; eng.seq_run = 1; eng.clk_start++; }   /* (no pulse) */
}
#include "sd_console.h"                         /* the site's CONSOLE (W1): the state a frame at a time */
#if !defined(DINGHY_HOST)
/* ---- the diagnosis over USB (2026-11-17, the user: "can you read what happens in the FM-1 while it is connected?").
 * F0 7D 53 44 01 F7 ("SD", 7D the non-commercial ID: never the installer's 00 59) asks; the reply is
 * F0 7D 53 44 02 <n> <n words, 8 nibbles each, high first> F7. It only reads: nothing written, no flash. The words
 * (tools/fm1_diag.c names them): its version 1; the build; uptime ms; the CPU (0.1 %); mode | PLAY << 8 | page << 16;
 * core 2: on | gave_up << 8 | cut at the flash gate << 16; its bar (magic, barred); the mailbox (request, complete,
 * rejected, misread, jobs); its fault (count, DBG, EMU, PC, RETS); its stack used; its jobs' load (longest, all,
 * how many: ticks); the zero word's misreads (count, last value); the P33 timeouts; the battery; the last crash
 * (fm1_crash, 16 words: kept through the restart); core 2's record at it (fm1_c1_last, 11 words); the RAM code
 * against its copy in the flash (_rt_start.._rt_end vs _rt_load: the words that differ, the first and last such
 * address, which KB of the 32 has one: 2026-11-17, does something write over it?); the supplies at the boot and as
 * set; the clock at the boot (measured, MHz x 10; SYS_DIV, CLK_CON0, CLK_CON2, PLL_CON0/1, PLL2_CON0/1).
 * F0 7D 53 44 03 F7 asks for the second page (two cores only: F0 7D 53 44 04 <n> ... F7): 2; the watch as it stands
 * (fm1_cpu1.h's fm1_cpu1_watch_state, 6 words); each core's record at its last fault (fm1_dump[0], [1]: 57 words
 * each). 2026-11-18, who writes where it must not */
#if FM1_CPU1
#define SD_DIAG_MAXW 121u
_Static_assert(1u + 6u + 2u * (sizeof(fm1_dump_t) / 4u) <= SD_DIAG_MAXW, "the second page holds both records");
#else
#define SD_DIAG_MAXW 72u
#endif
static void sd_diag_send(uint32_t kind, const uint32_t *v, uint32_t k)
{
    uint8_t out[7u + SD_DIAG_MAXW * 8u];        /* (on the stack: the SMK-37 has no RAM to spare; sent before
                                                   it returns) */
    uint32_t i, j;
    out[0] = 0xF0u; out[1] = 0x7Du; out[2] = 0x53u; out[3] = 0x44u; out[4] = (uint8_t)kind; out[5] = (uint8_t)k;
    for (i = 0; i < k; i++)
        for (j = 0; j < 8u; j++) out[6u + 8u * i + j] = (uint8_t)(v[i] >> (28u - 4u * j) & 15u);
    out[6u + 8u * k] = 0xF7u;
    (void)ota_wire_send(out, 7u + 8u * k);
}
static void con_tick(void)                      /* the console's frames: 25 a second while asked, each only when
                                                   the last has left (a host not reading: none, never a wait) */
{
    static uint32_t last;
    uint8_t f[6u + SD_CON_MAX];
    uint32_t n;
    if (!con_until) return;
    if ((int32_t)(fm1_ms - con_until) >= 0 || !usb.config) { con_until = 0; return; }
    if (fm1_ms - last < 40u || so_w != so_r) return;
    last = fm1_ms;
    f[0] = 0xF0u; f[1] = 0x7Du; f[2] = 0x53u; f[3] = 0x44u; f[4] = 0x08u;
    n = sd_con_frame(f + 5);
    f[5u + n] = 0xF7u;
    (void)ota_wire_send(f, 6u + n);
}
static void sd_diag_service(void)
{
    const uint8_t *p;
    uint32_t n, v[SD_DIAG_MAXW], k = 0, i, h = 2166136261u;
    const char *nm = APP_NAME " " APP_VER;
    con_tick();                                 /* (the console's frames, while asked) */
    if (!ota_frame_get(&p, &n)) return;
    if (n == 5u && p[0] == 0x7Du && p[1] == 0x53u && p[2] == 0x44u && p[3] == 0x07u) {   /* the console: F0 7D 53 44
                                                   07 01 F7 frames for 3 s more, 07 00 F7 none (sd_console.h) */
        con_until = p[4] ? (fm1_ms + 3000u) | 1u : 0u;
        ota_frame_done();
        return;
    }
    if (n >= 7u && p[0] == 0x7Du && p[1] == 0x53u && p[2] == 0x44u && p[3] == 0x09u) {   /* the console's EDIT: F0 7D 53
                                                   44 09 <offset, 14 bits> <n> <n bytes, each as two nibbles, low
                                                   first> F7, into the patch as the knobs leave it, then to the engine
                                                   as a knob does (nothing in the flash: SAVE keeps it) */
        uint32_t off = (uint32_t)p[4] << 7 | p[5], c = p[6];
        if (n == 7u + 2u * c && off + c <= sizeof pat) {
            for (i = 0; i < c; i++) ((uint8_t *)&pat)[off + i] = (uint8_t)((p[7u + 2u * i] & 15u) | (p[8u + 2u * i] & 15u) << 4);
            gen = 0xFFFFFFFFu;
            sd_engine_patch(&eng, &pat);       /* (the screen draws from it: its knobs show it) */
        }
        ota_frame_done();
        return;
    }
    if (n == 4u && p[0] == 0x7Du && p[1] == 0x53u && p[2] == 0x44u && p[3] == 0x0Bu) {   /* the patch asked for: F0 7D 53
                                                   44 0C <its size, 14 bits> <its bytes as nibbles> F7 (a file of it
                                                   on the console; the size says the layout matches) */
        uint8_t out[8u + 2u * sizeof pat];      /* (on the stack: no RAM to spare; sent before it returns) */
        const uint8_t *b = (const uint8_t *)&pat;
        ota_frame_done();
        out[0] = 0xF0u; out[1] = 0x7Du; out[2] = 0x53u; out[3] = 0x44u; out[4] = 0x0Cu;
        out[5] = (uint8_t)(sizeof pat >> 7 & 127u); out[6] = (uint8_t)(sizeof pat & 127u);
        for (i = 0; i < sizeof pat; i++) { out[7u + 2u * i] = b[i] & 15u; out[8u + 2u * i] = b[i] >> 4; }
        out[7u + 2u * sizeof pat] = 0xF7u;
        (void)ota_wire_send(out, 8u + 2u * sizeof pat);
        return;
    }
    if (n == 5u && p[0] == 0x7Du && p[1] == 0x53u && p[2] == 0x44u && p[3] == 0x05u) {   /* the freeze log: F0 7D 53
                                                   44 05 <i> F7, record i / 2 (0 the newest), its half i & 1, answered
                                                   F0 7D 53 44 06 <n> <i, the record's words> F7 (tools/fm1_diag -l) */
        FrzRec r;
        uint32_t ix = p[4], hw = (sizeof r / 4u + 1u) / 2u;
        ota_frame_done();
        v[k++] = ix;
        if (!frz_get(ix >> 1, &r))
            for (i = (ix & 1u) * hw; i < (ix & 1u) * hw + hw && i < sizeof r / 4u; i++) v[k++] = ((const uint32_t *)(const void *)&r)[i];
        sd_diag_send(0x06u, v, k);
        return;
    }
    if (n != 4u || p[0] != 0x7Du || p[1] != 0x53u || p[2] != 0x44u || (p[3] != 0x01u && p[3] != 0x03u))
        return;                                 /* (not ours: on) */
    ota_frame_done();
    if (p[3] == 0x03u) {                        /* the second page */
#if FM1_CPU1
        v[k++] = 2u;
        fm1_cpu1_watch_state(&v[k]);
        k += 6u;
        for (i = 0; i < 2u * (sizeof fm1_dump[0] / 4u); i++) v[k++] = ((const uint32_t *)(const void *)fm1_dump)[i];
        sd_diag_send(0x04u, v, k);
#endif
        return;
    }
    while (*nm) h = (h ^ (uint8_t)*nm++) * 16777619u;
    v[k++] = 1u;
    v[k++] = h;
    v[k++] = fm1_ms;
    v[k++] = cpu_permille;
    v[k++] = eng.mode_on | (uint32_t)eng.seq_run << 8 | page << 16;
#if FM1_CPU1
    v[k++] = fm1_cpu1_on | (uint32_t)fm1_cpu1_gave_up << 8 | (uint32_t)fm1_flash_cpu1_cut << 16;
    v[k++] = fm1_cpu1_bar.magic;
    v[k++] = fm1_cpu1_bar.barred;
    v[k++] = fm1_cpu1_mb.request;
    v[k++] = fm1_cpu1_mb.complete;
    v[k++] = fm1_cpu1_mb.rejected;
    v[k++] = fm1_cpu1_mb.misread;
    v[k++] = fm1_cpu1_mb.jobs;
    v[k++] = fm1_cpu1_fault.count;
    v[k++] = fm1_cpu1_fault.dbg;
    v[k++] = fm1_cpu1_fault.emu;
    v[k++] = fm1_cpu1_fault.pc;
    v[k++] = fm1_cpu1_fault.rets;
    v[k++] = fm1_cpu1_stack_used();
    v[k++] = sd_c1_load[0];
    v[k++] = sd_c1_load[1];
    v[k++] = sd_c1_load[2];
    v[k++] = fm1_cpu1_probe.bad;
    v[k++] = fm1_cpu1_probe.last;
#else
    for (i = 0; i < 19u; i++) v[k++] = 0;       /* (one core: none of the second's) */
#endif
    v[k++] = fm1_p33_timeouts;
    v[k++] = batt;
    for (i = 0; i < 16u; i++) v[k++] = ((const uint32_t *)(const void *)&fm1_crash)[i];
#if FM1_CPU1
    for (i = 0; i < 11u; i++) v[k++] = ((const uint32_t *)(const void *)&fm1_c1_last)[i];
#else
    for (i = 0; i < 11u; i++) v[k++] = 0;
#endif
    {                                           /* the RAM code as loaded (it is never written after the boot) */
        extern uint32_t _rt_start[], _rt_end[], _rt_load[];
        const volatile uint32_t *r = _rt_start, *f = _rt_load;
        uint32_t nw = (uint32_t)(_rt_end - _rt_start), bad = 0, first = 0, last = 0, kb = 0, a;
        for (i = 0; i < nw; i++)
            if (r[i] != f[i]) {
                a = (uint32_t)(uintptr_t)&r[i];
                if (!bad) first = a;
                last = a;
                bad++;
                kb |= 1u << ((a - 0x01C00000u) >> 10 & 31u);
            }
        v[k++] = bad;
        v[k++] = first;
        v[k++] = last;
        v[k++] = kb;
    }
#if FM1_CPU1
    v[k++] = fm1_supply_was;                    /* the supplies (P3_ANA_CON6 | CON9 << 8): as the boot found them, */
    v[k++] = fm1_supply_now;                    /* as it left them */
#else
    v[k++] = 0; v[k++] = 0;
#endif
    for (i = 0; i < 8u; i++) v[k++] = fm1_clk[i];   /* the clock at the boot: MHz x 10 measured, its registers */
    v[k++] = cpu_max;                           /* (2026-11-21) the worst since the start, the gaps, the waiting for */
    v[k++] = cpu_over;                          /* the second core: the last second's worst and since the start */
    v[k++] = wait_permille;
    v[k++] = wait_max;
    v[k++] = boot_wd;                           /* (2026-11-22) the last run, as this start found it: the watchdog's? */
    v[k++] = boot_run.at;                       /* where its main loop stood (LR_*) */
    v[k++] = boot_run.ms;                       /* its main loop's last pass, ms */
    v[k++] = boot_run.isr_ms;                   /* the audio's last second's end, ms */
    v[k++] = boot_run.page;
    v[k++] = boot_run.btn;
    v[k++] = boot_run.cpu;
    v[k++] = boot_run.held;
    v[k++] = boot_run.c1;
    v[k++] = boot_run.stop_ms;                  /* its last STOP, ms, */
    v[k++] = boot_run.play_ms;                  /* after how long a PLAY */
    sd_diag_send(0x02u, v, k);
}
#endif
static void app_idle(void)
{
    if (idle_ok) midi_in();
}

#ifdef DINGHY_WEB
#define APP_STATE static                        /* in a browser (web/app/): a pass a call, its state kept between */
#else
#define APP_STATE
#endif
static void app_main(void)
{
    APP_STATE uint32_t prev = 0, k, t_change = 0, dirty = 0, frame_at = 0;
    APP_STATE uint8_t held[27] = {0};
#ifdef DINGHY_WEB
    static uint32_t started, splash_t0, splash_last = ~0u;
    if (!started) {
        started = 1;
#endif

    sd_engine_init(&eng);
    eng_setup();                                /* (the pool is not cleared at boot: SPACE's lines start silent) */
#if !defined(DINGHY_HOST) && defined(SD_RAM2) && SD_RAM2
    {                                           /* a second RAM bank, if one answers: the looper's (cleared) */
        uint32_t k;
        ram2_size = fm1_ram2_probe();
        for (k = 4; k > 0u && RAM2_BYTES(k) > ram2_size; k--) {}
        if (k) {
            fm1_ram2_clear(RAM2_BYTES(k));
            sd_engine_looper(&eng, RAM2_A, k == 4u ? RAM2_BYTES(k) - SD_LP_INTB : RAM2_BYTES(k), lp_mem, SD_LP_INTB);
            ram2_kind = (uint8_t)k;
        }
    }
#endif
    k = !settings_load(&set, sizeof set);       /* (the drums' kit and beat in it) */
    if (!k && !settings_load(&set, (uint32_t)((uint8_t *)&set.dmore - (uint8_t *)&set))) {   /* (as saved before the
                                                   steps' ROLL and PROB and the fourth's ENGINE, 2026-11-20) */
        uint8_t *z = (uint8_t *)&set.dmore;
        for (k = 0; k < sizeof set.dmore; k++) z[k] = 0;
        set.dperc = 0;
        k = 1;
    }
    if ((!k && settings_load(&set, (uint32_t)(&set.drum - &set.oct)) && settings_load(&set, (uint32_t)(&set.song - &set.oct)) &&
         settings_load(&set, (uint32_t)(&set.one_core - &set.oct)) &&
         settings_load(&set, (uint32_t)(&set.accent - &set.oct)))   /* (or as they were saved before the drums and
                                                   the songs, 2026-11-19, before CORES, 2026-11-18, before COLOUR,
                                                   2026-11-16) */
        || (int8_t)set.oct < -2 || (int8_t)set.oct > 2 || set.bpm < 40u ||
        set.bpm > 240u || set.patch >= NPATCHES || set.mode >= SD_MODES || set.pattern >= UPATTERNS)
        set.oct = 0, set.bpm = 120, set.patch = SD_PATCH_WORST, set.mode = SD_MODE_MONO, set.pattern = 0;
    if (set.amode >= SD_AMODES || set.adiv >= SD_SDIVS || set.aoct < 1u || set.aoct > 4u || set.agate < 1u ||
        set.agate > 100u || set.ahold > 1u)
        set.amode = SD_AM_UP, set.adiv = 2, set.aoct = 1, set.agate = 50, set.ahold = 0;
    if (set.xchrom > 1u) set.xchrom = 0;
    if (set.sync_int > 1u) set.sync_int = 0;
    if (set.master > 1u) set.master = 0;
    if (set.accent >= SD_ACCENTS) set.accent = 0;
    if (set.one_core > 1u) set.one_core = 0;
    if (set.click > 2u) set.click = 0;
    if (set.song >= USONGS) set.song = 0;
    if (!k) { set.dkit = SD_DR_KIT0; sd_dr_pat0(&set.dpat); set.drum = 0; }   /* (none saved: the lab's kit, its beat) */
    if (set.drum >= UDRUMS) set.drum = 0;
    if (set.dperc >= SD_PCS) set.dperc = 0;
    {                                           /* the fourth's values outside its engine's ranges (RIM and COW gone,
                                                   2026-11-22: DATA and SONAR in their places): the engine's as chosen */
        uint32_t i, out = 0;
        for (i = 0; i < 8u; i++) {
            int32_t v = set.dkit.d[SD_DR_CLAP].v[i];
            out |= v < sd_dr_min(SD_DR_CLAP, set.dperc, i) || v > sd_dr_max(SD_DR_CLAP, set.dperc, i);
        }
        if (out) for (i = 0; i < 8u; i++) set.dkit.d[SD_DR_CLAP].v[i] = SD_PC_DEF[set.dperc][i];
    }
    set.dpad = 0;
    accent_set(set.accent);
    if (slot_load(0, &upat, sizeof upat)) {    /* (or as saved before HOME's MORE, 2026-11-23, before SUB, 2026-11-21, before TRIM and the make-ups' mark, before
                                                   RADAR's OFFSET, 2026-11-21, or before MOD's OFFSETs, 2026-11-19: each
                                                   patch that less the fields at its end, moved to its place, those 0) */
        const uint32_t po = (uint32_t)__builtin_offsetof(UserPatches, p), sz = (uint32_t)sizeof(SdPatch);
        const uint32_t OLD[5] = {sz - 2u, sz - 6u, sz - 8u, sz - 10u, sz - 10u - SD_MODS};
        uint8_t *z = (uint8_t *)&upat;
        uint32_t v;
        for (v = 0; v < 5u && slot_load(0, &upat, po + UPATCHES * OLD[v]); v++) ;
        if (v < 5u) {
            for (k = UPATCHES; k-- > 0u;) {     /* (the last first, back to front: the new place lies above) */
                uint8_t *d = z + po + k * sz;
                const uint8_t *s = z + po + k * OLD[v];
                uint32_t i;
                for (i = OLD[v]; i-- > 0u;) d[i] = s[i];
                for (i = OLD[v]; i < sz; i++) d[i] = 0;
            }
        } else {
            for (k = 0; k < sizeof upat; k++) z[k] = 0;
        }
    }
    if (slot_load(1, &useq, USEQ_HALF) || slot_load(2, (uint8_t *)&useq + USEQ_HALF, sizeof useq - USEQ_HALF)) {
        uint8_t *z = (uint8_t *)&useq;
        for (k = 0; k < sizeof useq; k++) z[k] = 0;
    }
    if (slot_load(3, &usong, sizeof usong)) {  /* (or as saved before LOOP's and FREEZE's pitch, 2026-11-22, or
                                                   before their drums, 2026-11-19: each song that less its end, moved
                                                   to its place, the rest 0 (as they play, no pitch); or the ORBITs
                                                   saved with the patterns; or nothing) */
        static const uint8_t LESS[2] = {SD_ORB_TAIL_B, SD_ORB_TAIL_B + SD_ORB_STEPS};
        uint8_t *z = (uint8_t *)&usong;
        uint32_t v, done = 0;
        for (v = 0; v < 2u && !done; v++) {
            const uint32_t old = (uint32_t)sizeof(SdOrbit) - LESS[v];
            if (slot_load(3, &usong, 4u + USONGS * old)) continue;
            done = 1;
            for (k = USONGS; k-- > 0u;) {       /* (the last first, back to front: the new place lies above) */
                uint8_t *d = z + 4u + k * sizeof(SdOrbit);
                const uint8_t *sp = z + 4u + k * old;
                uint32_t i;
                for (i = old; i-- > 0u;) d[i] = sp[i];
                for (i = old; i < sizeof(SdOrbit); i++) d[i] = 0;
            }
        }
        if (!done) song_migrate();
    }
    if (slot_load(4, &udr, sizeof udr)) {      /* the beats (none saved: the factory's) */
        uint8_t *z = (uint8_t *)&udr;
        if (slot_load(4, &udr, (uint32_t)__builtin_offsetof(UserDrums, m))) for (k = 0; k < sizeof udr; k++) z[k] = 0;
        else for (k = (uint32_t)__builtin_offsetof(UserDrums, m); k < sizeof udr; k++) z[k] = 0;   /* (saved before
                                                   their ROLLs, 2026-11-20) */
    }
    for (k = 0; k < UDRUMS; k++) {              /* a place never saved: the factory's beat, or nothing */
        uint32_t d, i;
        if (udr.used >> k & 1u) continue;
        for (d = 0; d < SD_DRS; d++)
            for (i = 0; i < SD_DR_STEPS; i++)
                sd_dr_step_set(&udr.p[k], d, i, k < DR_FACTORY ? (uint32_t)(DR_FBEAT[k][d][i] - '0') & 3u : 0u);
    }
    dr_places();
    dr_kit_push();
    dr_push(0);                                 /* (the drums themselves off: EDIT on DRUM) */
    eng.pats = useq.p;                          /* (the songs' steps load the saved patterns from here) */
    eng.pats_used = useq.used;
    if (!patch_ok(set.patch)) set.patch = SD_PATCH_WORST;
    eng.mode = set.mode;
    eng.bpm = set.bpm;
    patch_load(set.patch, &pat);
    sd_engine_patch(&eng, &pat);
    if (useq.used >> set.pattern & 1u) seqp = useq.p[set.pattern];
    else sd_pattern_default(&seqp);
    root_own(&seqp);
    seq_push();
    orb_load(set.song);
    sd_pattern_empty(&seq_other);               /* track B: silent until it has notes */
    root_own(&seq_other);
    sd_engine_pattern_to(&eng, 1, &seq_other);
    eng.seqb = 1;
#ifdef SD_CPU_WORST                             /* a measure, never shipped (DINGHY_DEFS=-DSD_CPU_WORST=1 ./build.sh, then
                                                   tools/emu_cpu.sh with keys held for ARP): the costliest patch with
                                                   every engine on, POLY, both tracks (B in fifths), ARP; 1: also all 8
                                                   slots on what costs most, from fast LFOs, RADAR to PITCH (quantized
                                                   each bank), ORBIT on, its lanes and macros gliding; 2: no modulation */
    {
        static const SdModSlot W[SD_MODS] = {
            {SD_MS_LFO1, SD_MD_RFREQ, 12000}, {SD_MS_LFO2, SD_MD_DSPLIT, 12000}, {SD_MS_LFO1, SD_MD_FCUT, 9000},
            {SD_MS_LFO2, SD_MD_FRES, 9000}, {SD_MS_ENV1, SD_MD_SSIZE, 9000}, {SD_MS_LFO1, SD_MD_STIME, 3000},
            {SD_MS_TUR, SD_MD_RPOS, 12000}, {SD_MS_ENV2, SD_MD_XM, 9000}};
        uint32_t i;
        pat.fx_off = 0;
        if (!pat.r_mix) pat.r_mix = 32768;
        if (!pat.d_mix) pat.d_mix = 65535;
        pat.s_dlevel = 20000; pat.s_rlevel = 20000;
        if (!pat.f_type) pat.f_type = SD_FT_LP24;
        for (i = 0; i < SD_MODS && SD_CPU_WORST == 1; i++) pat.mod[i] = W[i];
        if (SD_CPU_WORST == 1) {
            pat.lfo_rate[0] = 52000; pat.lfo_rate[1] = 44000; pat.lfo_mode[0] = pat.lfo_mode[1] = 0;
            pat.tur_set = 0x80u | 50u; pat.tur_ld = 8u; pat.tur_dst = SD_MD_PITCH; pat.tur_amt = 40;
            for (i = 0; i < 4u; i++) pat.mac[i] = 20000;
            orbp.on = 1; orbp.len = 4; orbp.glide = SD_OG_L1 | SD_OG_L2 | SD_OG_MAC;
            orbp.dst[0] = SD_MD_DTIME; orbp.dst[1] = SD_MD_RRES;
            for (i = 0; i < 4u; i++) {
                orbp.xa[i] = (int8_t)(i * 2u); orbp.xb[i] = (int8_t)(i * 3u);
                orbp.lane[0][i] = (int8_t)(i & 1u ? 60 : -60); orbp.lane[1][i] = (int8_t)(i & 1u ? -50 : 50);
                orbp.mac[0][i] = (int8_t)(i & 1u ? 60 : -40); orbp.mac[1][i] = 50; orbp.mac[2][i] = (int8_t)(i * 20u);
                orbp.mac[3][i] = (int8_t)(i & 1u ? -50 : 50);
            }
            orb_push();
        }
        set.mode = SD_MODE_POLY;
        sd_pattern_default(&seq_other);
        for (i = 0; i < SD_PSTEPS; i++) seq_other.step[i].chord[0] = 7;
        sd_engine_pattern_to(&eng, 1, &seq_other);
        eng.mode = set.mode;
        eng.arp_on = 1;
        sd_engine_patch(&eng, &pat);
    }
#endif
#ifdef SD_CPU_USER                              /* a measure, never shipped (2026-11-23, the user's dropouts on the FM-1:
                                                   DINGHY_DEFS="-DSD_CPU_USER=1 -DSD_PATCH_WORST=SD_P_SIGNAL"): the boot
                                                   patch as it is, POLY, both tracks (B the default in fifths), the drums */
    {
        uint32_t i;
        set.mode = SD_MODE_POLY;
        eng.mode = set.mode;
        sd_pattern_default(&seq_other);
        for (i = 0; i < SD_PSTEPS; i++) seq_other.step[i].chord[0] = 7;
        sd_engine_pattern_to(&eng, 1, &seq_other);
        eng.dr.on = 1;
    }
#endif

#ifdef DINGHY_WEB
        splash_begin();
        splash_t0 = fm1_ms;
    }
    if (splash_t0 != ~0u) {                     /* the splash, as splash() draws it: a frame each 33 ms */
        uint32_t now = fm1_ms - splash_t0;
        platform_poll();
        if (now < SPLASH_MS) {
            if (now / 33u != splash_last) { splash_last = now / 33u; splash_draw(now); }
            return;
        }
        splash_t0 = ~0u;
        lcd_fill(0, 0, 240, 240, C_BG);
        return;
    }
    do {                                        /* one pass (a millisecond) a call */
#else
#ifndef DINGHY_HOST
    {                                           /* the freeze log: what the last run left, when the watchdog made this
                                                   start (a freeze: the main loop stood 8 s) or a crash's screen did (its
                                                   count new); said for 12 s after the splash, SETUP shows it whole */
        static uint32_t crash_seen __attribute__((section(".noinit")));   /* (the crash count last logged) */
        boot_run = last_run;
        boot_wd = last_run.magic == LAST_RUN_MAGIC && (fm1_boot.p3_rst & 4u);
        if (last_run.magic != LAST_RUN_MAGIC) boot_run.pc_n = 0;
        frz_scan();
        if (boot_wd) {
            frz_add(FRZ_FREEZE, &boot_run, 0);
            *put(last_said, "FREEZE LOGGED: SETUP, SELECT") = 0;
        } else if (!(fm1_boot.p3_rst & 1u) && fm1_crash.magic == FM1_CRASH_MAGIC && fm1_crash.count != crash_seen) {
            frz_add(FRZ_CRASH, &boot_run, (const uint32_t *)(const void *)&fm1_crash);
            *put(last_said, "CRASH LOGGED: SETUP, SELECT") = 0;
        }
        crash_seen = fm1_crash.count;
    }
#endif
#if defined(DINGHY_HOST) && defined(SD_FRZ_FAKE)  /* (a test build: a freeze in the log, to see SETUP show it) */
    {
        static const uint8_t EVK[12][3] = {{EV_BTN, BTN_PLAY, 1}, {EV_KEY, 7, 1}, {EV_KEY_UP, 7, 0}, {EV_ENC, ENC_KNOB3, 2},
                                           {EV_BTN, BTN_SEQ, 1}, {EV_ENC, ENC_ALGORITHM, 1}, {EV_KEY, 12, 1}, {EV_KEY_UP, 12, 0},
                                           {EV_BTN, BTN_GLO, 1}, {EV_BTN_UP, BTN_GLO, 0}, {EV_BTN, BTN_GLO, 1}, {EV_BTN_UP, BTN_GLO, 0}};
        LastRun r;
        uint32_t i;
        uint8_t *z = (uint8_t *)&r;
        for (i = 0; i < sizeof r; i++) z[i] = 0;
        r.magic = LAST_RUN_MAGIC; r.ms = 754321u; r.isr_ms = 761000u; r.page = PAGE_SEQ; r.cpu = 781u; r.at = LR_DRAW;
        r.st = 1u | 4u | SD_MODE_POLY << 4 | SL_STEP << 8; r.pc_n = 4u; r.pc[0] = 0xA1C4u; r.pc[3] = 0xA1D0u;   /* (not addresses: the lint) */
        last_run.ev_n = 0;
        for (i = 0; i < 12u; i++) { ev_put(EVK[i][0], EVK[i][1], EVK[i][2]); ev_ring[i].t8 = (uint16_t)((740000u + i * 1200u) >> 3); }
        r.ev_n = 12u;
        frz_add(FRZ_FREEZE, &r, 0);
    }
#endif
    last_run.ev_n = 0;                          /* (this run's inputs from here) */
    last_run.magic = 0;
    last_run.at = 0; last_run.stop_ms = 0; last_run.play_ms = 0;
    splash();
    lcd_fill(0, 0, 240, 240, C_BG);

    for (;;) {
#endif
        uint32_t keys, down, up, rel, b, lit = 0, sq, oq;
        int32_t oct = (int8_t)set.oct, e;
        uint32_t base = (uint32_t)((int32_t)KEY0 + 12 * oct);
        platform_poll();

        keys = fm1_in.notes;                    /* bit k: key k (0 = F3 .. 26 = G5) */
        down = keys & ~prev;
        up = prev & ~keys;
        prev = keys;
        if (demo_st) {                          /* the demo plays: a key, a button or a knob stops it; nothing else
                                                   reaches the app meanwhile */
            static const uint8_t EN[7] = {ENC_SELECT, ENC_ALGORITHM, ENC_PRESETS, ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
            uint32_t rel2, bb = fm1_input_edges(&rel2), i;
            int32_t turned = 0;
            for (i = 0; i < 7u; i++) turned |= fm1_enc_take(EN[i]);
            if ((down || bb || turned) && (demo_st == DEMO_PLAY || demo_st == DEMO_TAIL)) { demo_fade = 4096; demo_st = DEMO_FADE; }
            keys = down = up = 0;
        }
        if (demo_st == DEMO_RESTORE) demo_restore();
        sq = seq_page() && !seq_rec && shift();   /* SEQ: the keys play (2026-11-17, the user: SELECT picks the step,
                                                   so the keys try a line before REC); GLO there every one transposes */
        oq = page == PAGE_ORB && !seq_rec && shift();   /* ORBIT: the same, GLO its steps */
        if (page == PAGE_DRUM) {                /* DRUM (2026-11-20, the user: "no synth on the drums"): the pads and
                                                   the MUTE keys; GLO the steps */
            if (down && (fm1_in.buttons >> BTN_GLO & 1u)) gl_used = 1;
            if (shift()) dr_keys(down); else dr_pads(down);
            oq = 1;                             /* (as ORBIT's: no key plays the synth) */
        }
        if (page != PAGE_DRUM) dr_clr_at = 0, dr_rec = 0;
        if (page == 5u && shift()) {            /* MACROS, GLO: F3..E4 switch (home_toggle, home_sw): not played */
            uint32_t hk = 0;
            for (k = 0; k < 27u; k++)
                if (home_sw(k) >= 0 && (keys >> k & 1u)) {
                    uint32_t i = (uint32_t)home_sw(k);
                    if (down >> k & 1u && (fm1_in.buttons >> BTN_GLO & 1u)) gl_used = 1;   /* (GLO held for a switch:
                                                   no tap, so it does not latch; its key leaves `down` here, before the
                                                   tap's look) */
                    if (down >> k & 1u && home_toggle(i)) sd_engine_patch(&eng, &pat);
                    hk |= 1u << k;
                }
            down &= ~hk; keys &= ~hk;
        }
        if (sq) {                               /* GLO: every key transposes the track, at its next one: IN KEY its
                                                   root to the key pressed, in the key (the keys lit), CHROM from C4;
                                                   the keys as printed, whatever OCT's octave (past them: TRACK's
                                                   XPOSE, GLO + OCT) */
            if (down && (fm1_in.buttons >> BTN_GLO & 1u)) gl_used = 1;   /* (GLO held: no tap) */
            for (k = 0; k < 27u; k++)
                if (down >> k & 1u) {
                    int32_t x = (int32_t)(KEY0 + k) - (int32_t)key_none();
                    if (!key_has(KEY0 + k)) { msg("NOT IN THE KEY"); continue; }
                    eng.t[seq_trk].xpose_next = (int8_t)(x < -24 ? -24 : x > 24 ? 24 : x);
                }
        } else if (oq && page == PAGE_ORB) {    /* ORBIT, GLO: the white keys its steps */
            if (down && (fm1_in.buttons >> BTN_GLO & 1u)) gl_used = 1;
            for (k = 0; k < 27u; k++)
                if (down >> k & 1u) {
                    int32_t w = key_white(k);
                    if (w >= 0 && (uint32_t)w < SD_ORB_LEN(&orbp)) {   /* the step: picked and heard (held);
                                                   the held one again: let go, ORBIT on in time */
                        orb_cur = (uint32_t)w;
                        if (!orb_setup && eng.orb_hold != w + 1) orb_hold_step((uint32_t)w); else eng.orb_hold = 0;
                    }
                }
        }
        if (down && !sq && !oq && !seq_rec && !seq_page() && !eng.arp_on && eng.seq_run && !eng.quota[SD_SRC_HAND])
            msg(set.mode == SD_MODE_MONO ? "MONO: THE SEQ HAS THE VOICE" : "NO FREE VOICE: SEQ PLAYS");
        rec_pend_take();                        /* (REC: the next step begun, what waited for it) */
        dr_pend_take();
        for (k = 0; k < 27u; k++) {
            if (down >> k & 1u && !sq && !oq) {
                uint32_t others = keys & ~(1u << k) & ~down;   /* keys already held: this one joins their chord */
                held[k] = (uint8_t)(base + k);
                sd_engine_note(&eng, held[k], 100);
                midi_send(0x90u, held[k], 100);
                if (seq_rec)                    /* (what is heard: back by the track's transpose) */
                    rec_live(sd_seq_rec_note(&eng, &eng.t[seq_trk], held[k]), 100u,
                             others || midi_hn || (down & ((1u << k) - 1u)));
            }
            if (up >> k & 1u && held[k]) { sd_engine_note(&eng, held[k], 0); midi_send(0x80u, held[k], 0); held[k] = 0; }
            if (keys >> k & 1u) lit |= 1u << k;
        }

        if (seq_rec && !keys && !midi_hn) seq_keys_up();   /* REC: every key (and MIDI's) up, the next step */
        {                                       /* the song loaded a pattern (its step's, at the master's one): SEQ's
                                                   copy follows, so its page shows and edits what plays */
            static uint32_t song_seen[2];
            uint32_t k;
            for (k = 0; k < 2u && !demo_st; k++) {
                uint32_t q = eng.t[k].song_seq, n = eng.t[k].song_n;
                if (q == song_seen[k]) continue;
                song_seen[k] = q;
                if (!n || n > UPATTERNS) continue;
                if (k == seq_trk) {
                    seqp = useq.p[n - 1u];
                    set.pattern = (uint8_t)(n - 1u);
                    if (seq_cur >= seqp.len) seq_cur = seq_bank = 0;
                    seq_has_undo = 0;
                    seq_push();                 /* (the same pattern: an edit sent just before the load is undone) */
                } else {
                    seq_other = useq.p[n - 1u];
                }
            }
        }
        if (!demo_st) {                         /* the song loaded a beat (its step's, at the bar's one): DRUM's copy
                                                   follows */
            static uint32_t dr_song_seen;
            uint32_t q = eng.dr.song_seq, n = eng.dr.song_n;
            if (q != dr_song_seen) {
                dr_song_seen = q;
                if (n && n <= UDRUMS) { set.dpat = udr.p[n - 1u]; set.dmore = udr.m[n - 1u]; set.drum = (uint8_t)(n - 1u); dr_has_undo = 0; dr_dirty = 1; }
            }
        }
        if (dr_dirty) { dr_dirty = 0; dirty = 1; t_change = fm1_ms; }   /* (the drums' sounds and beat: kept) */
        {                                       /* MIDI clock out over USB (the engine's clko_*: 24 a quarter while
                                                   the FM-1 keeps the tempo): a START before the pulse it came with,
                                                   the pulses (after a stall at most 8), a STOP as PLAY stops
                                                   (2026-11-19, the user: no sync over USB; a computer follows now) */
            static uint32_t co_n, co_st;
            static uint8_t co_run;
            uint32_t n = eng.clko_n, st = eng.clko_starts, n0 = eng.clko_n0;   /* (in this order: a START between
                                                   them, its pulse next pass, never before the START) */
            if (st != co_st) {                  /* the pulses before it, START, then the one's pulse */
                co_st = st;
                if ((int32_t)(n0 - co_n) > 8) co_n = n0 - 8u;
                while ((int32_t)(n0 - co_n) > 0) { midi_send(0xF8u, 0, 0); co_n++; }
                midi_send(0xFAu, 0, 0);
            }
            if ((int32_t)(n - co_n) > 8) co_n = n - 8u;
            while ((int32_t)(n - co_n) > 0) { midi_send(0xF8u, 0, 0); co_n++; }
            if (co_run && !eng.seq_run && !eng.clk_on) midi_send(0xFCu, 0, 0);
            co_run = eng.seq_run;
        }
        if (seq_rec && eng.seq_run && rec_at != 0xFFu && (keys || midi_hn)) {   /* REC, playing: a key still held
                                                   as the steps go on, a TIE each (the note as long as it is held):
                                                   held past the middle of the next step, not a key let go a little
                                                   late; and never round the loop over its own note (2026-11-18, the
                                                   user: TIEs everywhere, nothing but the keys pressed) */
            const SdSeqT *tt = &eng.t[seq_trk];
            uint32_t at = tt->seq_shown;
            if (at == rec_at && rec_tie_at != rec_at) rec_tie_at = 0xFEu;   /* (round to its note: no more) */
            if (rec_tie_at != 0xFEu && at < SD_PSTEPS && at < seqp.len && at != rec_tie_at && at != rec_at &&
                seqp.step[at].type != SD_ST_SKIP && tt->seq_pos_q8 >= tt->seq_step_q8 / 2u + rec_ahead() * 256u) {
                                                /* (past the middle as heard: rec_ahead) */
                if (seq_dub && seqp.step[at].type != SD_ST_REST) rec_tie_at = 0xFEu;   /* (DUB: a held note stops
                                                   at the next note, takes nothing) */
                else {
                    if (rec_at < SD_PSTEPS) {   /* (the held note's own: a TIE turned into a NOTE later plays it) */
                        seqp.step[at].note = seqp.step[rec_at].note;
                        seqp.step[at].vel = seqp.step[rec_at].vel;
                    }
                    seqp.step[at].type = SD_ST_TIE;
                    rec_tie_at = at;
                    rec_ties++;
                    seq_push();
                }
            }
        }
        if (eng.orb_hold && (page != PAGE_ORB || orb_setup || eng.orb_hold > SD_ORB_LEN(&orbp) ||
                             fm1_ms - orb_hold_at >= ORB_HOLD_MS))
            eng.orb_hold = 0;                   /* (ORBIT left, its SETUP, the step past LENGTH, or no edit for 2 s:
                                                   on in time) */
        if (aud_c && (eng.seq_run || seq_rec || !seq_page() || (aud_key < 27u && (up >> aud_key & 1u)) ||
                      (aud_off && (int32_t)(fm1_ms - aud_off) >= 0))) aud_stop();   /* (the step heard: done) */
        b = fm1_input_edges(&rel);
        last_run.magic = LAST_RUN_MAGIC; last_run.ms = fm1_ms; last_run.page = page;   /* (the last run's marks) */
        last_run.pc_n = 0;                      /* (a pass: not stuck) */
#ifdef SD_FREEZE_TEST                           /* (a test build, DINGHY_DEFS=-DSD_FREEZE_TEST=6000: the main loop stands
                                                   from that ms while the log is empty; the watchdog's restart logs it) */
        if (fm1_ms > SD_FREEZE_TEST && !frz_n) { volatile uint32_t z = 0; LR_AT(77u); for (;;) z++; }
#endif
        last_run.st = eng.seq_run | seq_rec << 1 | (uint32_t)gl_latch << 2 | set.mode << 4 | seq_trk << 6 |
                      seq_layer << 8 | (uint32_t)seq_dub << 12 | (uint32_t)eng.arp_on << 13 | (uint32_t)lp_arm << 14;
        if (b | rel | down | up) {              /* the freeze log's inputs: each button and key, down and up */
            uint32_t q;
            for (q = 0; q < 32u; q++) {
                if (b >> q & 1u) ev_put(EV_BTN, q, 1);
                if (rel >> q & 1u) ev_put(EV_BTN_UP, q, 0);
                if (q < 27u && (down >> q & 1u)) ev_put(EV_KEY, q, 1);
                if (q < 27u && (up >> q & 1u)) ev_put(EV_KEY_UP, q, 0);
            }
        }
        LR_AT(LR_INPUT);
        {                                       /* STOP: when, and after how long a PLAY */
            static uint32_t ran, ran_at;
            if (eng.seq_run && !ran) ran_at = fm1_ms;
            if (!eng.seq_run && ran) { last_run.stop_ms = fm1_ms; last_run.play_ms = fm1_ms - ran_at; }
            ran = eng.seq_run;
        }
        if (b) last_run.btn = b;
#if FM1_CPU1
        last_run.c1 = fm1_cpu1_on | (uint32_t)fm1_cpu1_gave_up << 8;
#endif
        if (last_said[0] && fm1_ms < 12000u) msg(last_said);
        {                                       /* GLO, the second layer: a tap latches it or lets it go */
            if (b & (1u << BTN_GLO)) { gl_at = fm1_ms; gl_used = 0; }
            else if ((fm1_in.buttons >> BTN_GLO & 1u) && (b || down)) gl_used = 1;   /* (a knob: enc_take) */
            if ((rel >> BTN_GLO & 1u) && !gl_used && fm1_ms - gl_at < GL_TAP_MS && glo_here()) gl_latch = (uint8_t)!gl_latch;
            b &= ~(1u << BTN_GLO);              /* (not a button for the rest: it closes no list) */
        }
        if (b) dlist_at = 0;                    /* (a button: the destinations' list closes) */
        if (b && browse_at) {                   /* the list open: EDIT (or OCT+) loads, HOME (or OCT-) not now; another
                                                   button closes it */
            if (b & (1u << BTN_EDIT | 1u << BTN_OCT_UP)) {
                if (browse_kind == 4u) {        /* a beat while they play: at the bar's one */
                    if (eng.seq_run && eng.dr.on) pat_waiting = 0x200u | (browse_to + 1u);
                    dr_load(browse_to);
                } else if (browse_kind == 3u) { /* a song while it plays: at the master's one, from its step 1 */
                    if (eng.seq_run) { eng.orb_wait = 1; pat_waiting = 0x100u | (browse_to + 1u); }
                    set.song = (uint8_t)browse_to;
                    orb_load(browse_to);
                } else if (browse_kind == 2u) { /* a pattern while it plays: at the one, from step 1 (2026-11-19,
                                                   the user: seamless at the next one) */
                    if (eng.seq_run) { eng.t[seq_trk].pat_wait = 1; pat_waiting = browse_to + 1u; }
                    pattern_take(browse_to);
                }
                else {
                    set.patch = (uint8_t)browse_to;
                    patch_load(set.patch, &pat);
                    gen = 0xFFFFFFFFu;
                    sd_engine_patch(&eng, &pat);
                }
                dirty = 1;
                t_change = fm1_ms;
            }
            b &= ~(1u << BTN_EDIT | 1u << BTN_HOME | 1u << BTN_OCT_UP | 1u << BTN_OCT_DOWN);
            browse_at = 0;
        }
        if (b & (1u << BTN_PLAY)) {
            {
                eng.seq_run = (uint8_t)!eng.seq_run;
                if (eng.lp_on && lp_arm) msg(eng.seq_run ? "LOOP BACK ON THE ONE" : "LOOP WAITS FOR PLAY");
            }
        }
        if (b & (1u << BTN_SEL)) {              /* SEL: the voices, MONO DUO POLY; MONO holds SEQ B */
            set.mode = (uint8_t)((set.mode + 1u) % SD_MODES);
            eng.mode = set.mode;
            if (set.mode == SD_MODE_MONO && !eng.t[1].mute && trk_has_notes(1)) msg("MONO: SEQ B WAITS");
            dirty = 1;
            t_change = fm1_ms;
        }
        if (b & (1u << BTN_SAVE)) { if (save_mode) save_commit(); else save_begin(); }
        else if (save_mode && (b & (1u << BTN_OCT_UP))) { save_commit(); b &= ~(1u << BTN_OCT_UP); }   /* OCT+: yes */
        if (save_mode && ((b & (1u << BTN_HOME | 1u << BTN_OCT_DOWN)) || fm1_ms - save_at > 10000u)) {   /* HOME, OCT-:
                                                   not now */
            save_mode = 0;
            b &= ~(1u << BTN_HOME | 1u << BTN_OCT_DOWN);
        }
        if (seq_clr_at && seq_page() && (b & (1u << BTN_OCT_UP | 1u << BTN_OCT_DOWN))) {   /* CLR asked: OCT+ yes,
                                                   OCT- no */
            if (b & (1u << BTN_OCT_UP)) seq_clear(); else seq_clr_at = 0;
            b &= ~(1u << BTN_OCT_UP | 1u << BTN_OCT_DOWN);
        }
        if (lp_clr_at && (page != PAGE_LOOP || fm1_ms - lp_clr_at >= 3000u)) lp_clr_at = 0;
        if (lp_clr_at && (b & (1u << BTN_OCT_UP | 1u << BTN_OCT_DOWN))) {   /* LOOPER's clear asked: OCT+ yes, OCT- no */
            if (b & (1u << BTN_OCT_UP)) lp_clear(); else lp_clr_at = 0;
            b &= ~(1u << BTN_OCT_UP | 1u << BTN_OCT_DOWN);
        }
        if (dr_clr_at && fm1_ms - dr_clr_at >= 3000u) dr_clr_at = 0;   /* (asked: 3 s to answer) */
        if (dr_clr_at && page == PAGE_DRUM && (b & (1u << BTN_OCT_UP | 1u << BTN_OCT_DOWN))) {   /* DRUM's CLR asked:
                                                   OCT+ yes, OCT- no */
            if (b & (1u << BTN_OCT_UP)) dr_clear_row(dr_cur); else dr_clr_at = 0;
            b &= ~(1u << BTN_OCT_UP | 1u << BTN_OCT_DOWN);
        }
        if (clr_all_at && (page != PAGE_SETUP || fm1_ms - clr_all_at >= 3000u)) clr_all_at = 0;
        if (clr_all_at && (b & (1u << BTN_OCT_UP | 1u << BTN_OCT_DOWN))) {   /* asked: OCT+ yes, OCT- no */
            if ((b & (1u << BTN_OCT_UP)) && setup_ask >= 2u) { clr_all_at = 0; demo_go(setup_ask - 2u); }
            else if (b & (1u << BTN_OCT_UP)) clear_all();
            else clr_all_at = 0;
            b &= ~(1u << BTN_OCT_UP | 1u << BTN_OCT_DOWN);
        }
        if (b & (1u << BTN_REC)) {
            if (seq_rec) { rec_toggle(); msg("REC OFF"); }   /* SEQ recording: REC ends it on any page (2026-11-22,
                                                   the user: REC left on SEQ was stuck elsewhere, DRUM's REC its own) */
            else if (page == PAGE_ARP) arp_keep();   /* on ARP: KEEP what it played */
            else if (page == PAGE_DRUM) {       /* on DRUM: the pads into the steps (UNDO: the beat before) */
                dr_rec = !dr_rec;
                if (dr_rec) { dr_keep(); if (!eng.dr.on) msg("DRUMS OFF: EDIT TURNS THEM ON"); }
            } else rec_toggle();
        }
        if (seq_rec && seq_page() && (b & (1u << BTN_ENV))) { seq_write(0, SD_ST_TIE); b &= ~(1u << BTN_ENV); }   /* (on
                                                   SEQ only: elsewhere ENV and LFO go to their pages) */
        if (seq_rec && seq_page() && (b & (1u << BTN_LFO))) { seq_write(0, SD_ST_REST); b &= ~(1u << BTN_LFO); }
        /* the pages, each its button's, the button again the next of them (the top line shows them): HOME the voice and
           the macros, FX the effects, ENV the envelopes and the filter, LFO the LFOs, RADAR and the matrix, ARP, SEQ
           the tracks and ORBIT */
        {
            if (b & (1u << BTN_SEQ)) {
                if (!seq_page() && page != PAGE_ORB && page != PAGE_DRUM) {   /* from another page: the one SEQ showed
                                                   last */
                    if (seq_last == 3u && SD_DRUMS) page = PAGE_DRUM;
                    else if (seq_last == 2u) { page = PAGE_ORB; seq_track(0); }
                    else { page = PAGE_SEQ; seq_track(seq_last); }
                }
                else if (page == PAGE_SEQ && !seq_trk) seq_track(1);
                else if (page == PAGE_SEQ) { page = PAGE_ORB; seq_track(0); }
                else if (page == PAGE_ORB && SD_DRUMS) page = PAGE_DRUM;
                else { page = PAGE_SEQ; seq_track(0); }   /* DRUM -> SEQ A: round its own pages, as every page
                                                   button */
            }
            if (b & (1u << BTN_HOME)) page = page == 5u ? PAGE_SETUP : 5u;   /* MACROS <-> SETUP (from elsewhere:
                                                   MACROS; NAV went to ENV, 2026-11-21) */
            if (b & (1u << BTN_FX))             /* the chain in its order: REACTOR, DISTROY, TIME, SPACE (the delay into the
                                                   reverb; from elsewhere: the one it showed last) */
                page = page == 1u ? 2u : page == 2u ? PAGE_TIME : page == PAGE_TIME ? 3u : page == 3u ? 1u : fx_last;
            if (b & (1u << BTN_ENV))            /* the voice in its order: NAV, FILTER, ENV, SUB (2026-11-21, the user: "HOME
                                                   to make more sense"; NAV was HOME's second; SUB the same day; then "the
                                                   pages in the right order": the filter before the amp) */
                page = page == 0u ? PAGE_FILT : page == PAGE_FILT ? PAGE_ENV : page == PAGE_ENV ? PAGE_SUB : page == PAGE_SUB ? 0u :
                       env_last;
            if (b & (1u << BTN_LFO)) {          /* LFO 1, LFO 2, RADAR, MOD (the slot picked last); from elsewhere
                                                   the one it showed last (dmod kept) */
                if (page == 4u) dmod = dmod == SD_MODS ? SD_MODS + 1u : dmod == SD_MODS + 1u ? DMOD_TUR :
                                       dmod == DMOD_TUR ? dslot : SD_MODS;
                page = 4u;
            }
            if (b & (1u << BTN_ARP)) page = page == PAGE_ARP ? PAGE_LOOP : page == PAGE_LOOP ? PAGE_ARP : arp_last;
            if (b & (1u << BTN_HOME | 1u << BTN_FX | 1u << BTN_ENV | 1u << BTN_LFO | 1u << BTN_ARP | 1u << BTN_SEQ))
                gl_latch = 0;                   /* a page button: a latched GLO let go (2026-11-17, the user: the new
                                                   page starts on its first layer; held, it is still held) */
            if (page == 1u || page == 2u || page == 3u || page == PAGE_TIME) fx_last = page;
            if (page == PAGE_ARP || page == PAGE_LOOP) arp_last = page;
            if (page == 0u || page == PAGE_ENV || page == PAGE_FILT || page == PAGE_SUB) env_last = page;
            if (page == PAGE_SEQ) seq_last = seq_trk; else if (page == PAGE_ORB) seq_last = 2u;
            else if (page == PAGE_DRUM) seq_last = 3u;
        }
        if (b & (1u << BTN_EDIT)) {             /* EDIT: what this page shows, on / off */
            if (page == PAGE_LOOP) {            /* EDIT on LOOPER: on, or off with its loop kept (L-LEVEL as it
                                                   is: at 0 it records, silent, then faded in) */
                lp_switch();
            } else if (edit_toggle()) sd_engine_patch(&eng, &pat);
        }
        if (seq_page() && !seq_rec && shift() && b & (1u << BTN_OCT_DOWN | 1u << BTN_OCT_UP)) {   /* GLO + OCT on SEQ:
                                                   the track's transpose an octave, at its next one, as GLO + a key
                                                   (OCT alone is the octave there too, REC or not; the steps past 16:
                                                   ALGORITHM, the bank along) */
            int32_t x = eng.t[seq_trk].xpose_next;
            if (b & (1u << BTN_OCT_DOWN)) x = x - 12 < -24 ? x : x - 12;
            if (b & (1u << BTN_OCT_UP)) x = x + 12 > 24 ? x : x + 12;
            if (x == eng.t[seq_trk].xpose_next) msg(x < 0 ? "XPOSE: ITS LOWEST OCTAVE" : "XPOSE: ITS HIGHEST OCTAVE");
            eng.t[seq_trk].xpose_next = (int8_t)x;
            b &= ~(1u << BTN_OCT_DOWN | 1u << BTN_OCT_UP);
        }
        if (b & (1u << BTN_OCT_DOWN) && oct > -2) { set.oct = (uint8_t)(oct - 1); dirty = 1; t_change = fm1_ms; }
        if (b & (1u << BTN_OCT_UP) && oct < 2) { set.oct = (uint8_t)(oct + 1); dirty = 1; t_change = fm1_ms; }
        {
            uint32_t changed = 0, l = lay();
            if ((e = enc_take(ENC_SELECT)) != 0) changed = select_turn(e);   /* SELECT: which one, in a row */
            if ((e = enc_take(ENC_PRESETS)) != 0) {   /* the list (the sound as it is): EDIT loads */
                uint32_t kind = page == PAGE_DRUM ? 4u : page == PAGE_ORB ? 3u : seq_page() ? 2u : 1u;   /* (SEQ: the
                                                   patterns, P01..P32; ORBIT: the songs, S01..S16; DRUM: the beats,
                                                   D01..D16) */
                if (!browse_at || browse_kind != kind) {
                    browse_kind = kind;
                    browse_to = kind == 4u ? set.drum : kind == 3u ? set.song : kind == 2u ? set.pattern : set.patch;
                }
                while (e) {                     /* a detent a place (the patches past the empty user places) */
                    browse_to = browse_step(browse_to, e > 0 ? 1 : -1);
                    e += e > 0 ? -1 : 1;
                }
                browse_at = fm1_ms ? fm1_ms : 1u;
            }
            if (save_mode && (e = enc_take(ENC_ALGORITHM)) != 0) {   /* SAVE: where to */
                save_to = step_mod(save_to, e, save_places(save_mode));
                save_at = fm1_ms;
            }
            if (seq_page()) seq_knobs();        /* (it takes every encoder but PRESETS: the rest below sees 0) */
            if (page == PAGE_ORB) orb_knobs();  /* (so does ORBIT's) */
            if (page == PAGE_DRUM) dr_knobs();  /* (and DRUM's) */
            if ((e = enc_take(ENC_ALGORITHM)) != 0) {   /* ALGORITHM: the knobs' layer, round (the row on the hint
                                                   line; SEQ's own, above: GLO there its SCALE) */
                const char *const *ln;
                uint32_t n = lay_names(&ln), was = pg_lay[page & 15u];
                if (n > 1u) lay_to(step_mod(was, e, n));
            }
            if (page == PAGE_ENV || (page == PAGE_FILT && l == LF_ENV)) {   /* an envelope: A D S R, 0..127 */
                static const uint8_t KN[4] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
                uint8_t *ev = page == PAGE_FILT ? &pat.e3_a : l ? &pat.e2_a : &pat.e1_a;
                for (k = 0; k < 4u; k++)
                    if ((e = enc_take(KN[k])) != 0) {
                        int32_t v = (int32_t)ev[k] + enc_far(e, 64u) * 2;
                        ev[k] = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
                        changed = 1;
                    }
            } else if (page == PAGE_FILT && l == LF_MORE) {   /* FILTER's MORE: the TYPE (on a patch without one:
                                                   LP12, off till EDIT), VEL (5 % a detent) */
                if ((e = enc_take(ENC_KNOB1)) != 0) {
                    if (pat.f_type == SD_FT_OFF) {
                        pat.f_type = SD_FT_LP12;
                        pat.fx_off = (uint8_t)(pat.fx_off | SD_FX_FILTER);
                        filt_first();
                    } else pat.f_type = e > 0 ? SD_FT_LP24 : SD_FT_LP12;
                    changed = 1;
                }
                if ((e = enc_take(ENC_KNOB2)) != 0) {
                    int32_t v = (int32_t)pat.f_vel + e * 5;
                    pat.f_vel = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
                    changed = 1;
                }
            } else if (page == PAGE_FILT) {     /* FILTER: CUTOFF RES ENV KEY */
                if ((e = enc_take(ENC_KNOB1)) != 0) {   /* a quarter semitone a detent */
                    int32_t v = (int32_t)pat.f_cut + enc_far(e, 456u) * 147;   /* (65535 / 114 / 4) */
                    pat.f_cut = (uint16_t)(v < 0 ? 0 : v > 65535 ? 65535 : v);
                    changed = 1;
                }
                if ((e = enc_take(ENC_KNOB2)) != 0) pat.f_res = knob_q(pat.f_res, e), changed = 1;
                if ((e = enc_take(ENC_KNOB3)) != 0) {   /* -99 .. +99 % */
                    int32_t v = amt99(pat.f_env) + enc_far(e, 198u);
                    if (!pat.f_env && !pat.e3_a && !pat.e3_d && !pat.e3_s && !pat.e3_r)   /* (ENV3 never set: as
                                                   ENV1, so the depth sounds at once; a MOD slot's ENV3 too) */
                        pat.e3_a = pat.e1_a, pat.e3_d = pat.e1_d, pat.e3_s = pat.e1_s, pat.e3_r = pat.e1_r;
                    v = v < -99 ? -99 : v > 99 ? 99 : v;
                    pat.f_env = (int16_t)(v * 32767 / 99);
                    changed = 1;
                }
                if ((e = enc_take(ENC_KNOB4)) != 0) {
                    int32_t v = (int32_t)pat.f_key + e * 5;
                    pat.f_key = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
                    changed = 1;
                }
            } else if (page == PAGE_SUB) {      /* SUB: SHAPE (2 % a detent), OCTAVE (one a detent), LEVEL */
                if ((e = enc_take(ENC_KNOB1)) != 0) {
                    int32_t v = (int32_t)pat.sub_shape + enc_far(e, 50u) * 2;
                    pat.sub_shape = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
                    changed = 1;
                }
                if ((e = enc_take(ENC_KNOB2)) != 0) {   /* (2026-11-21, the user: "choose the octave, +/- 3") */
                    int32_t v = (int32_t)pat.sub_oct + e;
                    pat.sub_oct = (int8_t)(v < -2 ? -2 : v > 4 ? 4 : v);
                    changed = 1;
                }
                if ((e = enc_take(ENC_KNOB3)) != 0) {   /* FOLLOWS: A, B, ROOT (ALGORITHM's till 2026-11-22) */
                    pat.sub_src = (uint8_t)step_mod(pat.sub_src % SD_SUBS, e, SD_SUBS);
                    changed = 1;
                }
                if ((e = enc_take(ENC_KNOB4)) != 0) pat.sub_lvl = knob_q(pat.sub_lvl, e), changed = 1;
            } else if (page == PAGE_ARP && l == 1u) {   /* ARP's TOOLS: HOLD, a turn right on, left off */
                if ((e = enc_take(ENC_KNOB1)) != 0 && set.ahold != (uint8_t)(e > 0)) {
                    set.ahold = (uint8_t)(e > 0);
                    dirty = 1;
                    t_change = fm1_ms;
                }
                (void)enc_take(ENC_KNOB2); (void)enc_take(ENC_KNOB3); (void)enc_take(ENC_KNOB4);
            } else if (page == PAGE_ARP) {      /* ARP: MODE RATE OCT GATE (the player's, not the patch's) */
                uint32_t d0 = dirty;
                if ((e = enc_take(ENC_KNOB1)) != 0) set.amode = (uint8_t)step_mod(set.amode % SD_AMODES, e, SD_AMODES), dirty = 1;
                if ((e = enc_take(ENC_KNOB2)) != 0) { set.adiv = (uint8_t)sdiv_step(set.adiv % SD_SDIVS, e); dirty = 1; }
                if ((e = enc_take(ENC_KNOB3)) != 0) {
                    int32_t v = (int32_t)set.aoct + e;
                    set.aoct = (uint8_t)(v < 1 ? 1 : v > 4 ? 4 : v);
                    dirty = 1;
                }
                if ((e = enc_take(ENC_KNOB4)) != 0) {
                    int32_t v = (int32_t)set.agate + e * 5;
                    set.agate = (uint8_t)(v < 5 ? 5 : v > 100 ? 100 : v);
                    dirty = 1;
                }
                if (dirty && !d0) t_change = fm1_ms;
            } else if (page == PAGE_SETUP && l == LU_TOOLS) {   /* SETUP's TOOLS: DEMO 1, DEMO 2, CLEAR ALL (each
                                                   asked: OCT+ yes, OCT- no), LOG (the freeze log, as SELECT) */
                if ((e = enc_take(ENC_KNOB1)) != 0) setup_ask = 2u, clr_all_at = fm1_ms ? fm1_ms : 1u, setup_log = 0;
                if ((e = enc_take(ENC_KNOB2)) != 0) setup_ask = 3u, clr_all_at = fm1_ms ? fm1_ms : 1u, setup_log = 0;
                if ((e = enc_take(ENC_KNOB3)) != 0) setup_ask = 1u, clr_all_at = fm1_ms ? fm1_ms : 1u, setup_log = 0;
                if ((e = enc_take(ENC_KNOB4)) != 0) changed = select_turn(e);
            } else if (page == PAGE_SETUP && l == LU_MORE) {   /* SETUP's MORE: K1 SYNC, a turn right INT; K2 CORES, a
                                                   turn left 1, right 2: at the next start (the rest: nothing) */
                uint32_t d0 = dirty;
                uint8_t v;
                if ((e = enc_take(ENC_KNOB1)) != 0 && set.sync_int != (v = (uint8_t)(e > 0))) {
                    set.sync_int = v;           /* (INT: a clock ignored; AUTO: it leads when it comes) */
                    clk_cue = 0;
                    dirty = 1;
                }
#if FM1_CPU1
                if ((e = enc_take(ENC_KNOB2)) != 0 && set.one_core != (v = (uint8_t)(e < 0))) {
                    set.one_core = v;
                    msg(v ? "CORES 1: AT THE NEXT START" : "CORES 2: AT THE NEXT START");
                    dirty = 1;
                }
#else
                (void)enc_take(ENC_KNOB2);
#endif
                if ((e = enc_take(ENC_KNOB3)) != 0) {   /* CLICK: OFF, REC, PLAY (a turn a step, no wrap) */
                    int32_t c = (int32_t)set.click + (e > 0 ? 1 : -1);
                    c = c < 0 ? 0 : c > 2 ? 2 : c;
                    if (c != (int32_t)set.click) { set.click = (uint8_t)c; dirty = 1; }
                }
                (void)enc_take(ENC_KNOB4);
                if (dirty && !d0) t_change = fm1_ms;   /* (only as it changes: else, while it stays, never written) */
            } else if (page == PAGE_SETUP) {    /* SETUP: TEMPO, XPOSE MASTER (a turn right the second), COLOUR (kept
                                                   in flash) */
                uint32_t d0 = dirty;
                uint8_t v;
                if ((e = enc_take(ENC_KNOB1)) != 0 && eng.clk_on) msg("TEMPO: THE MIDI CLOCK'S");
                else if (e) {                   /* the tempo, 40..240 BPM */
                    int32_t t = (int32_t)set.bpm + enc_far(e, 200u);
                    set.bpm = (uint8_t)(t < 40 ? 40 : t > 240 ? 240 : t);
                    eng.bpm = set.bpm;
                    dirty = 1;
                }
                if ((e = enc_take(ENC_KNOB2)) != 0 && set.xchrom != (v = (uint8_t)(e > 0))) set.xchrom = v, dirty = 1;
                if ((e = enc_take(ENC_KNOB3)) != 0 && set.master != (v = (uint8_t)(e > 0))) set.master = v, dirty = 1;
                if ((e = enc_take(ENC_KNOB4)) != 0) {   /* COLOUR: the screen's accent, from a list as it turns */
                    set.accent = (uint8_t)step_mod(set.accent % SD_ACCENTS, e, SD_ACCENTS);
                    accent_set(set.accent);
                    dlist_open(DL_COL);
                    dirty = 1;
                }
                if (dirty && !d0) t_change = fm1_ms;
            } else if (page == 5u) {            /* MACROS: MUTATE MOVE BREAK CUTOFF; MORE: BODY AIR DROWN DARK */
                static const uint8_t KN[4] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
                for (k = 0; k < 4u; k++) {
                    if ((e = enc_take(KN[k])) == 0) continue;
                    if (l == 1u) {
                        uint8_t *m = k == 1u ? &pat.mac_air : &pat.mac_drown;
                        if (k == 3u) { home_filt = knob_q(home_filt, e); eng.filt = home_filt; }
                        else if (!k) pat.mac[3] = knob_q(pat.mac[3], e), changed = 1;
                        else *m = sd_mac8(knob_q((uint16_t)(*m * 257u), e)), changed = 1;
                    } else if (k == 3u) {       /* CUTOFF as FILTER's K1: a quarter semitone a detent */
                        int32_t v = (int32_t)pat.f_cut + enc_far(e, 456u) * 147;
                        pat.f_cut = (uint16_t)(v < 0 ? 0 : v > 65535 ? 65535 : v);
                        changed = 1;
                    } else pat.mac[k] = knob_q(pat.mac[k], e), changed = 1;
                }
            } else if (page == 4u && dmod == DMOD_TUR) {   /* RADAR: SEARCH LENGTH OFFSET DIV (where it goes: MOD) */
                if ((e = enc_take(ENC_KNOB1)) != 0) {   /* SEARCH: 2 % a detent (half the travel, 2026-11-21) */
                    pat.tur_set = (uint8_t)((pat.tur_set & 0x80u) | step_lim(SD_TUR_CHANGE(&pat), 2 * enc_far(e, 51u), 101u));
                    dlist_at = 0;
                    changed = 1;
                }
                if ((e = enc_take(ENC_KNOB2)) != 0) {
                    pat.tur_ld = (uint8_t)((pat.tur_ld & 0xE0u) | (2u + step_lim(SD_TUR_LEN(&pat) - 2u, e, 15u)));
                    dlist_at = 0;
                    changed = 1;
                }
                if ((e = enc_take(ENC_KNOB3)) != 0) {   /* OFFSET: a percent a detent */
                    int32_t v = (int32_t)pat.tur_off + enc_far(e, 200u);
                    dlist_at = 0;
                    pat.tur_off = (int8_t)(v < -100 ? -100 : v > 100 ? 100 : v);
                    changed = 1;
                }
                if ((e = enc_take(ENC_KNOB4)) != 0) {   /* DIV: its clock (GLO + K2's till 2026-11-21) */
                    pat.tur_ld = (uint8_t)((pat.tur_ld & 31u) | (step_lim(SD_TUR_DIV(&pat), e, SD_TUR_DIVS) + 1u) << 5);
                    dlist_at = 0;
                    changed = 1;
                }
            } else if (page == 4u && dmod < SD_MODS) {   /* MOD, a slot: SOURCE DEST AMOUNT */
                SdModSlot *m = &pat.mod[dmod];
                if ((e = enc_take(ENC_KNOB1)) != 0) {
                    m->src = (uint8_t)src_step(m->src, e), dlist_open(DL_SRC), changed = 1;
                    if (!dst_ok(m->src, m->dst)) m->dst = 0;   /* (RADAR's and RADAR 1's own: theirs only) */
                }
                if ((e = enc_take(ENC_KNOB2)) != 0) {   /* (from RADAR or RADAR 1: their own at the list's end) */
                    m->dst = (uint8_t)dst_step(m->dst, e, m->src);
                    dlist_open(m->src == SD_MS_TUR ? DL_TUR : m->src == SD_MS_TUR1 ? DL_TUR1 : DL_MOD);
                    changed = 1;
                }
                if ((e = enc_take(ENC_KNOB3)) != 0) {   /* a percent a detent */
                    int32_t v = amt99(m->amt) + enc_far(e, 198u);
                    dlist_at = 0;
                    v = v < -99 ? -99 : v > 99 ? 99 : v;
                    m->amt = (int16_t)(v * 32767 / 99);
                    changed = 1;
                }
                if ((e = enc_take(ENC_KNOB4)) != 0 && TG_IS(m->dst)) {   /* RADAR 1's own: ON, 1 (right) or 0 (left):
                                                   AMOUNT's sign */
                    int32_t a = m->amt < 0 ? -m->amt : m->amt;
                    dlist_at = 0;
                    m->amt = (int16_t)(e > 0 ? a : -a);
                    changed = 1;
                } else if (e && m->dst < SD_MDSTS) {   /* OFFSET: a percent a detent, -100 .. 100 (RADAR's own: none) */
                    int32_t v = (int32_t)pat.mod_off[dmod] + enc_far(e, 200u);
                    dlist_at = 0;
                    pat.mod_off[dmod] = (int8_t)(v < -100 ? -100 : v > 100 ? 100 : v);
                    changed = 1;
                }
            } else if (page == 4u && l == 1u) { /* an LFO's MODE */
                uint32_t lk = dmod - SD_MODS;
                uint32_t m = lmode_of(pat.lfo_mode[lk]), c = lcyc_of(m), sy = m & 1u || m >= 6u, m0 = m;
                if ((e = enc_take(ENC_KNOB1)) != 0) {   /* CLOCK: a turn right SYNC, left FREE (struck by KICK, SNARE,
                                                   RADAR, the bar: synced) */
                    if (c >= 3u) msg("STRUCK BY A BEAT: SYNCED");
                    else m = lmode_at(c, e > 0);
                }
                if ((e = enc_take(ENC_KNOB2)) != 0) {   /* CYCLE: LOOPS NOTE ONCE KICK SNARE RADAR 1 BAR (no drums:
                                                   KICK and SNARE not in the turn) */
                    uint32_t v = SD_DRUMS || c < 3u ? c : c < 5u ? 3u : c - 2u;
                    v = step_lim(v, e, SD_DRUMS ? 7u : 5u);
                    m = lmode_at(SD_DRUMS || v < 3u ? v : v + 2u, sy);
                }
                if (m != m0) pat.lfo_mode[lk] = LMODE_BITS[m % LMODES], changed = 1;
                (void)enc_take(ENC_KNOB3); (void)enc_take(ENC_KNOB4);
            } else if (page == 4u) {            /* an LFO: SHAPE RATE DEST AMOUNT (MODE its second layer) */
                uint32_t lk = dmod - SD_MODS, s = lfo_slot(lk);
                if ((e = enc_take(ENC_KNOB1)) != 0) pat.lfo_shape[lk] = (uint8_t)step_mod(pat.lfo_shape[lk] % SD_LSHAPES, e, SD_LSHAPES), changed = 1;
                if ((e = enc_take(ENC_KNOB2)) != 0) {
                    if (pat.lfo_mode[lk] & SD_LM_SYNC) {   /* a division a detent */
                        int32_t d = (int32_t)((pat.lfo_rate[lk] * SD_LFO_DIVS) >> 16) + e;
                        d = d < 0 ? 0 : d > SD_LFO_DIVS - 1 ? SD_LFO_DIVS - 1 : d;
                        pat.lfo_rate[lk] = (uint16_t)((uint32_t)d * 65536u / SD_LFO_DIVS + 100u);
                    } else {                    /* 0..99 as the other knobs: 0.02 .. 40 Hz, ~8 % a detent */
                        pat.lfo_rate[lk] = knob_q(pat.lfo_rate[lk], e);
                    }
                    changed = 1;
                }
                if ((e = enc_take(ENC_KNOB3)) != 0) {   /* DEST: its slot's, a free one the first time (+30 %);
                                                   OFF lets the slot go */
                    uint32_t i;
                    if (s == SD_MODS)
                        for (i = 0; i < SD_MODS; i++)
                            if (!pat.mod[i].src) {
                                s = i;
                                pat.mod[i].src = (uint8_t)(lk ? SD_MS_LFO2 : SD_MS_LFO1);
                                pat.mod[i].dst = 0;
                                pat.mod[i].amt = (int16_t)(30 * 32767 / 99);
                                break;
                            }
                    if (s == SD_MODS) msg("MOD FULL: 8 SLOTS IN USE");
                    else {
                        pat.mod[s].dst = (uint8_t)dst_step(pat.mod[s].dst, e, 0);
                        if (!pat.mod[s].dst) { pat.mod[s].src = SD_MS_OFF; pat.mod[s].amt = 0; pat.mod_off[s] = 0; }
                        dlist_open(DL_LFO);
                        changed = 1;
                    }
                }
                if ((e = enc_take(ENC_KNOB4)) != 0) {   /* AMOUNT: a percent a detent */
                    if (s == SD_MODS) msg("K3 FIRST: WHERE IT GOES");
                    else {
                        int32_t v = amt99(pat.mod[s].amt) + enc_far(e, 198u);
                        dlist_at = 0;
                        v = v < -99 ? -99 : v > 99 ? 99 : v;
                        pat.mod[s].amt = (int16_t)(v * 32767 / 99);
                        changed = 1;
                    }
                }
            } else if (page == 3u) {            /* SPACE: the plate; FREEZE: FREEZE PITCH; MORE: SHIMMER */
                if (l == 2u) {
                    if ((e = enc_take(ENC_KNOB1)) != 0) {
                        int32_t sh = (int32_t)SD_SP_SHIM(&pat) + enc_far(e, 64u);
                        sh = sh < 0 ? 0 : sh > 63 ? 63 : sh;
                        pat.s_mode = (uint8_t)((pat.s_mode & 3u) | (uint32_t)sh << 2);
                        changed = 1;
                    }
                    (void)enc_take(ENC_KNOB2); (void)enc_take(ENC_KNOB3); (void)enc_take(ENC_KNOB4);
                } else if (l == 1u) {
                    if ((e = enc_take(ENC_KNOB1)) != 0 && sp_frz != (uint8_t)(e > 0)) {   /* FREEZE (the player's, not
                                                   saved): a turn right on, left off */
                        sp_frz = (uint8_t)(e > 0);
                        msg(sp_frz ? "FREEZE: THE TAIL HELD" : "FREEZE OFF");
                    }
                    if ((e = enc_take(ENC_KNOB2)) != 0) {   /* PITCH: the frozen tail, -12 .. 12 semitones */
                        int32_t ps = sp_pitch + e;
                        sp_pitch = (int8_t)(ps < -12 ? -12 : ps > 12 ? 12 : ps);
                        if (!sp_frz) msg("PITCH: WHILE FROZEN");
                    }
                    (void)enc_take(ENC_KNOB3); (void)enc_take(ENC_KNOB4);
                } else {
                    if ((e = enc_take(ENC_KNOB1)) != 0) pat.s_size = knob_q(pat.s_size, e), changed = 1;
                    if ((e = enc_take(ENC_KNOB2)) != 0) pat.s_decay = knob_q(pat.s_decay, e), changed = 1;
                    if ((e = enc_take(ENC_KNOB3)) != 0) pat.s_rtone = knob_q(pat.s_rtone, e), changed = 1;
                    if ((e = enc_take(ENC_KNOB4)) != 0) pat.s_rlevel = knob_q(pat.s_rlevel, e), changed = 1;
                }
            } else if (page == PAGE_LOOP && l == LL_TOOLS) {   /* LOOPER's TOOLS: GLIDE (how long a new SPEED
                                                   takes), REV (it plays it backwards), OCTAVE (SPEED to the next
                                                   octave, -12 0 +12, over GLIDE), CLEAR (asked, OCT+ yes); a turn
                                                   right on (up), left off (down) */
                if ((e = enc_take(ENC_KNOB1)) != 0) lp_glide_turn(e);
                if ((e = enc_take(ENC_KNOB2)) != 0 && lp_rev != (uint8_t)(e > 0)) {
                    lp_rev = (uint8_t)(e > 0);
                    msg(lp_rev ? "REVERSE: THE LOOP BACKWARDS" : "REVERSE OFF");
                }
                if ((e = enc_take(ENC_KNOB3)) != 0) lp_octave(e > 0);
                if ((e = enc_take(ENC_KNOB4)) != 0) {
                    if (lp_arm || lp_hold) lp_clr_at = fm1_ms ? fm1_ms : 1u; else msg("NO LOOP TO CLEAR");
                }
            } else if (page == PAGE_LOOP && l == LL_SEND) {   /* LOOPER's SEND: FILTER (left a lowpass, right a
                                                   highpass, none in the middle: it stops there), R-SEND, D-SEND */
                if ((e = enc_take(ENC_KNOB1)) != 0) {
                    int32_t v = (int32_t)lp_filt + enc_far(e, 64u) * 1024, was = (int32_t)lp_filt - 32768;
                    v = v < 0 ? 0 : v > 65535 ? 65535 : v;
                    if ((v - 32768 > -1024 && v - 32768 < 1024) || (was < 0) != (v - 32768 < 0)) v = was ? 32768 : v;
                    lp_filt = (uint16_t)v;
                }
                if ((e = enc_take(ENC_KNOB2)) != 0) lp_rsend = (uint16_t)knob_q(lp_rsend, e);
                if ((e = enc_take(ENC_KNOB3)) != 0) lp_dsend = (uint16_t)knob_q(lp_dsend, e);
                (void)enc_take(ENC_KNOB4);
            } else if (page == PAGE_TIME && l == 1u) {   /* TIME's MODE: SYNC (a turn right) or FREE, PING-PONG on
                                                   (right) or off */
                if ((e = enc_take(ENC_KNOB1)) != 0) {
                    pat.s_mode = (uint8_t)(e > 0 ? pat.s_mode | SD_SP_SYNC : pat.s_mode & ~SD_SP_SYNC);
                    changed = 1;
                }
                if ((e = enc_take(ENC_KNOB2)) != 0) {
                    pat.s_mode = (uint8_t)(e > 0 ? pat.s_mode | 2u : pat.s_mode & ~2u);
                    changed = 1;
                }
                (void)enc_take(ENC_KNOB3); (void)enc_take(ENC_KNOB4);
            } else if (page == PAGE_TIME || page == PAGE_LOOP) {   /* TIME: the delay; LOOPER: the looper */
                if (page == PAGE_LOOP) {
                    if ((e = enc_take(ENC_KNOB1)) != 0 && lp_on != (uint8_t)(e > 0)) {   /* LOOP: its last LENGTH
                                                   round and round; a turn right on, left off */
                        lp_on = (uint8_t)(e > 0);
                        msg(lp_on ? (lp_arm ? "LOOP: THE LAST LENGTH HELD" : "LOOP: LOOPER OFF, EDIT ARMS") : "LOOP OFF");
                    }
                    if ((e = enc_take(ENC_KNOB2)) != 0) {   /* LENGTH: a division a detent, synced whatever
                                                   TIME's MODE (2026-11-24, the user: a FREE patch made 2 BAR 868 ms) */
                        uint32_t d = div_turn(sd_lp_div(lp_len), e, SD_LP_NDIV, lp_lim());
                        lp_len = (uint16_t)(d * 65536u / SD_LP_NDIV + 100u);
                    }
                    if ((e = enc_take(ENC_KNOB3)) != 0) {   /* SPEED: a semitone a detent (2026-11-17, the user: "in
                                                   semitones, not cents"), -12 .. +12 */
                        int32_t s = (lp_speed + (lp_speed < 0 ? -128 : 128)) / 256 + e;
                        s = s < -12 ? -12 : s > 12 ? 12 : s;
                        lp_speed = (int16_t)(s * 256);
                    }
                    if ((e = enc_take(ENC_KNOB4)) != 0) lp_lvl = (uint16_t)knob_q(lp_lvl, e);   /* L-LEVEL: its level
                                                   only (EDIT arms it) */
                } else {
                    if ((e = enc_take(ENC_KNOB1)) != 0) {   /* TIME: a division a detent synced, ~4 ms free */
                        if (pat.s_mode & SD_SP_SYNC) {
                            uint32_t d = div_turn(sd_sp_div(pat.s_time), e, SD_SP_NDIV, dl_lim());
                            pat.s_time = (uint16_t)(d * 65536u / SD_SP_NDIV + 100u);
                        } else {
                            int32_t t = (int32_t)pat.s_time + enc_far(e, 256u) * 256;
                            pat.s_time = (uint16_t)(t < 0 ? 0 : t > 65535 ? 65535 : t);
                        }
                        changed = 1;
                    }
                    if ((e = enc_take(ENC_KNOB2)) != 0) pat.s_fb = knob_q(pat.s_fb, e), changed = 1;
                    if ((e = enc_take(ENC_KNOB3)) != 0) pat.s_dtone = knob_q(pat.s_dtone, e), changed = 1;
                    if ((e = enc_take(ENC_KNOB4)) != 0) pat.s_dlevel = knob_q(pat.s_dlevel, e), changed = 1;
                }
            } else if (page == 2u) {            /* DISTROY, the band SELECT picks */
                uint32_t k2 = dband & 3u;
                if (l == LD_MIX) {              /* MIX: the band's upper SPLIT, between its neighbours; the whole's
                                                   XOVER and MIX */
                    if ((e = enc_take(ENC_KNOB1)) != 0) {
                        uint32_t si = k2 < 3u ? k2 : 2u;
                        int32_t lo = si ? pat.d_split[si - 1u] + 256 : SD_R_LO * 256 + 256;
                        int32_t hi = si < 2u ? pat.d_split[si + 1u] - 256 : SD_R_HI * 256 - 256;
                        int32_t v = (int32_t)pat.d_split[si] + enc_far(e, (SD_R_HI - SD_R_LO) * 4u) * 64;   /* a quarter semitone a detent */
                        pat.d_split[si] = (uint16_t)(v < lo ? lo : v > hi ? hi : v);
                        changed = 1;
                    }
                    if ((e = enc_take(ENC_KNOB2)) != 0) pat.d_xover = e > 0 ? SD_DX_LR4 : SD_DX_CMP, changed = 1;
                    if ((e = enc_take(ENC_KNOB3)) != 0) pat.d_mix = knob_q(pat.d_mix, e), changed = 1;
                    (void)enc_take(ENC_KNOB4);
                } else if (l == LD_LOOP) {      /* LOOP (D3): FDBK TIME SPREAD TAME */
                    if ((e = enc_take(ENC_KNOB1)) != 0) pat.d_fb = knob_q(pat.d_fb, e), changed = 1;
                    if ((e = enc_take(ENC_KNOB2)) != 0) pat.d_ftime = knob_q(pat.d_ftime, e), changed = 1;
                    if ((e = enc_take(ENC_KNOB3)) != 0) pat.d_spread = knob_q(pat.d_spread, e), changed = 1;
                    if ((e = enc_take(ENC_KNOB4)) != 0) pat.d_tame = knob_q(pat.d_tame, e), changed = 1;
                } else if (l == LD_SHAPE) {     /* SHAPE: BIAS PAN DYNAMIC BODY */
                    if ((e = enc_take(ENC_KNOB4)) != 0) {   /* BODY: where PHAT and SHARP act (GLO + ALGORITHM till
                                                   2026-11-22) */
                        pat.d_body[k2] = knob_q(pat.d_body[k2], e);
                        changed = 1;
                    }
                    if ((e = enc_take(ENC_KNOB1)) != 0) {   /* BIAS: -50 .. +50 */
                        int32_t v = pat.d_bias[k2] + enc_far(e, 100u) * 655;
                        pat.d_bias[k2] = (int16_t)(v < -32767 ? -32767 : v > 32767 ? 32767 : v);
                        changed = 1;
                    }
                    if ((e = enc_take(ENC_KNOB2)) != 0) {
                        int32_t v = pat.d_pan[k2] + enc_far(e, 100u) * 656;
                        pat.d_pan[k2] = (int16_t)(v < -32767 ? -32767 : v > 32767 ? 32767 : v);
                        changed = 1;
                    }
                    if ((e = enc_take(ENC_KNOB3)) != 0) {   /* DYNAMIC: PHAT -99 .. 0 .. +99 SHARP */
                        int32_t v = pat.d_shape[k2] + enc_far(e, 198u) * 331;
                        pat.d_shape[k2] = (int16_t)(v < -32767 ? -32767 : v > 32767 ? 32767 : v);
                        changed = 1;
                    }
                } else {                        /* TYPE DRIVE ALT LEVEL */
                    if ((e = enc_take(ENC_KNOB1)) != 0) {
                        pat.d_type[k2] = (uint8_t)step_mod(pat.d_type[k2] % SD_DSHAPES, e, SD_DSHAPES);
                        changed = 1;
                    }
                    if ((e = enc_take(ENC_KNOB2)) != 0) pat.d_gain[k2] = knob_q(pat.d_gain[k2], e), changed = 1;
                    if ((e = enc_take(ENC_KNOB3)) != 0) pat.d_alt[k2] = knob_q(pat.d_alt[k2], e), changed = 1;
                    if ((e = enc_take(ENC_KNOB4)) != 0) pat.d_level[k2] = knob_q(pat.d_level[k2], e), changed = 1;
                }
            } else if (page == 1u && r_stages) {   /* STAGE: TYPE PITCH WIDTH LEVEL; F1-F4 */
                SdReactorStage *st = &pat.stage[rst_cur % 6u];
                static const uint8_t KN[4] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
                if (l == LR_FREQ) {
                    for (k = 0; k < 4u; k++)
                        if ((e = enc_take(KN[k])) != 0) {
                            st->pitch[k] = (uint16_t)rst_clamp((int32_t)st->pitch[k] + enc_far(e, SD_R_HI - SD_R_LO) * 256);
                            dlist_at = 0;
                            changed = 1;
                        }
                } else {
                    if ((e = enc_take(ENC_KNOB1)) != 0) {   /* TYPE: the list */
                        int32_t m = rst_match(st);
                        rst_put(st, m < 0 ? (e > 0 ? 0u : RST_N - 1u) : step_mod((uint32_t)m, e, RST_N));
                        dlist_open(DL_RST);
                        changed = 1;
                    }
                    if ((e = enc_take(ENC_KNOB2)) != 0) rst_pitch(st, enc_far(e, SD_R_HI - SD_R_LO)), dlist_at = 0, changed = 1;
                    if ((e = enc_take(ENC_KNOB3)) != 0) rst_width(st, enc_far(e, SD_R_HI - SD_R_LO)), dlist_at = 0, changed = 1;
                    if ((e = enc_take(ENC_KNOB4)) != 0) rst_level(st, enc_far(e, 99u)), dlist_at = 0, changed = 1;
                }
            } else if (page == 1u && l == LR_MORE) {   /* the Reactor's MORE: TRACK (OFF KEY HARM KEY+H), LOW, how
                                                   many STAGES, 1..6 (2026-11-21, the user's one-stage monosynths:
                                                   POS's turn and the clock across those) */
                if ((e = enc_take(ENC_KNOB1)) != 0) {
                    pat.r_track = (uint8_t)((pat.r_track & ~3u) | step_lim(pat.r_track & 3u, e, 4u));
                    changed = 1;
                }
                if ((e = enc_take(ENC_KNOB2)) != 0) pat.r_low = knob_q(pat.r_low, e), changed = 1;
                if ((e = enc_take(ENC_KNOB3)) != 0) {
                    int32_t n = (int32_t)SD_R_NST(&pat) + e;
                    n = n < 1 ? 1 : n > 6 ? 6 : n;
                    pat.r_track = (uint8_t)((pat.r_track & 0x8Fu) | (n < 6 ? (uint32_t)n << 4 : 0u));
                    rst_cur = (uint8_t)(rst_cur % (uint32_t)n);
                    changed = 1;
                }
                (void)enc_take(ENC_KNOB4);
            } else if (page == 1u) {            /* the Reactor: REACT, TONE */
                if (l == LR_TONE) {
                    if ((e = enc_take(ENC_KNOB1)) != 0) {
                        int32_t f = pat.r_freq + enc_far(e, 96u) * 256;
                        pat.r_freq = (int16_t)(f < -48 * 256 ? -48 * 256 : f > 48 * 256 ? 48 * 256 : f);
                        changed = 1;
                    }
                    if ((e = enc_take(ENC_KNOB2)) != 0) pat.r_model = (uint8_t)step_mod(pat.r_model, e, SD_RMODELS), changed = 1;
                    if ((e = enc_take(ENC_KNOB3)) != 0) {
                        int32_t d = (int32_t)pat.r_div + e;
                        pat.r_div = (uint8_t)(d < 0 ? 0 : d > 16 ? 16 : d);
                        changed = 1;
                    }
                    if ((e = enc_take(ENC_KNOB4)) != 0) pat.r_mix = knob_q(pat.r_mix, e), changed = 1;
                } else {
                    if ((e = enc_take(ENC_KNOB1)) != 0) {   /* POS: a tenth of a stage a detent */
                        int32_t q = (int32_t)pat.r_pos + enc_far(e, 60u) * 26;
                        pat.r_pos = (uint16_t)(q < 0 ? 0 : q > 1535 ? 1535 : q);
                        changed = 1;
                    }
                    if ((e = enc_take(ENC_KNOB2)) != 0) pat.r_res = knob_q(pat.r_res, e), changed = 1;
                    if ((e = enc_take(ENC_KNOB3)) != 0) pat.r_slew = knob_q(pat.r_slew, e), changed = 1;
                    if ((e = enc_take(ENC_KNOB4)) != 0) pat.r_drive = knob_q(pat.r_drive, e), changed = 1;
                }
            } else if (page == 0u && l == LN_ROUTE) {   /* NAV's ROUTE: the bank's roles, the algorithms (a list as it
                                                   turns: how many heard, the routing; ALGORITHM's till 2026-11-22);
                                                   the bank (as SELECT) */
                if ((e = enc_take(ENC_KNOB1)) != 0) {
                    uint32_t a = algo_of(&pat);
                    a = a >= SD_ALGOS ? 0u : step_mod(a, e, SD_ALGOS);
                    for (k = 0; k < SD_OPS; k++) pat.bank[ebank].osc[k].role = sd_role_of(SD_ALGO[a][k]);
                    dlist_open(DL_ALG);
                    changed = 1;
                }
                if ((e = enc_take(ENC_KNOB2)) != 0) changed = select_turn(e);
                (void)enc_take(ENC_KNOB3); (void)enc_take(ENC_KNOB4);
            } else if (page == 0u && l == LN_XY) {   /* NAV's X/Y: X Y WIDTH, K4 a harmonic generator a detent
                                                   (its list) */
                if ((e = enc_take(ENC_KNOB1)) != 0) pat.x = knob_q(pat.x, e), dlist_at = 0, changed = 1;
                if ((e = enc_take(ENC_KNOB2)) != 0) pat.y = knob_q(pat.y, e), dlist_at = 0, changed = 1;
                if ((e = enc_take(ENC_KNOB3)) != 0) pat.width = knob_q(pat.width, e), dlist_at = 0, changed = 1;
                if ((e = enc_take(ENC_KNOB4)) != 0) {
                    gen = gen >= SD_GENS ? 0u : step_mod(gen, e, SD_GENS);
                    gen_seed += fm1_ms;
                    sd_gen(&pat, ebank, gen, &gen_seed);
                    dlist_open(DL_GEN);
                    changed = 1;
                }
            } else if (page == 0u) {            /* NAV: the Q page */
                if ((e = enc_take(ENC_KNOB1)) != 0) pat.xm = knob_q(pat.xm, e), dlist_at = 0, changed = 1;
                if ((e = enc_take(ENC_KNOB2)) != 0) pat.warp = knob_q(pat.warp, e), dlist_at = 0, changed = 1;
                if ((e = enc_take(ENC_KNOB3)) != 0) pat.fold = knob_q(pat.fold, e), dlist_at = 0, changed = 1;
                if ((e = enc_take(ENC_KNOB4)) != 0) pat.dt = knob_q(pat.dt, e), dlist_at = 0, changed = 1;
            }
            if (changed) sd_engine_patch(&eng, &pat);
        }

        midi_in();
        batt_poll();                            /* (the battery, once a second) */
        if (!demo_st) {                         /* the engine as the player set it (the demo: its own) */
        voices_guard();
        eng.rec_trk = seq_rec || seq_page() ? (uint8_t)seq_trk : 0xFFu;   /* REC, or SEQ's page: the keys may take the
                                                   track's voice (you hear what you record, or try a line over it) */
        eng.xkey = (uint8_t)!set.xchrom;        /* the transposes IN KEY (SETUP's XPOSE) */
        eng.master = set.master;                /* the track whose one leads (SETUP's MASTER) */
        eng.sp_freeze = sp_frz;                 /* SPACE's FREEZE (ALGORITHM there) */
        eng.sp_pitch = sp_pitch;                /* its PITCH (GLO + K2 there) */
        eng.lp_len = lp_len;                    /* LOOPER's (EDIT there arms it or not: L-LEVEL) */
        eng.lp_speed = lp_speed;
        eng.lp_glide = lp_glide;
        eng.lp_lvl = lp_arm || lp_hold ? lp_lvl : 0;   /* (disarmed: none, so the engine's own rule, a level arms,
                                                   holds; held: its level, faded out by LOOP's own fade) */
        eng.lp_arm = lp_arm;
        eng.lp_hold = lp_hold;
        eng.lp_filt = lp_filt; eng.lp_rsend = lp_rsend; eng.lp_dsend = lp_dsend;
        eng.lp_on = lp_on;
        eng.lp_rev = lp_rev;
#if !defined(DINGHY_HOST) && defined(SD_RAM2) && SD_RAM2
        {                                       /* the RAM2 test: what it found, once after the splash */
            static uint8_t r2_said;
            static char r2[28];
            if (!r2_said && fm1_ms > 3500u) {
                char *q = put(r2, "2ND RAM: ");
                r2_said = 1;
                if (fm1_ram2_hung()) q = put(q, "THE TEST HUNG");
                else if (!ram2_size) q = put(q, "NONE");
                else { q = put_u(q, ram2_size >> 10); q = put(q, " KB OK"); }
                *q = 0;
                msg(r2);
            }
        }
#endif
        {                                       /* the brake (app_audio): blocks held back, the CPU full; or, before
                                                   it, a voice let go (sd_shed) */
            static uint32_t held_seen, held_at, shed_seen;
            if (aud_held != held_seen && fm1_ms - held_at >= 3000u) {
                held_seen = aud_held; held_at = fm1_ms;
                msg("CPU FULL: TURN AN ENGINE OFF");
            } else if (eng.shed_n != shed_seen && fm1_ms - held_at >= 3000u) {
                shed_seen = eng.shed_n; held_at = fm1_ms;
                msg("CPU FULL: A VOICE LET GO");
            }
        }
#if FM1_CPU1
        {                                       /* the second core off (at the start, or it gave up): said once */
            static uint8_t c1_said;
            if (eng.sp_room && !c1_said)
                c1_said = 1, msg(fm1_cpu1_gave_up == FM1_CPU1_OFF ? "CORES 1 (SETUP): THE ROOM" : "2ND CORE OFF: THE ROOM");
        }
#endif
        eng.arp_mode = set.amode; eng.arp_div = set.adiv; eng.arp_oct = set.aoct;
        eng.arp_gate = set.agate; eng.arp_hold = set.ahold;
        }

        oct = (int8_t)set.oct;
        if (fm1_ms - led_at >= 10u) {           /* the lights, 100 a second (2026-11-17: every pass of the loop they
                                                   took a quarter of the first core's time, and their code pushed the
                                                   audio's out of the flash cache between its interrupts) */
            const char *what;
            uint32_t sw = sw_of(&what);
            led_at = fm1_ms;
            keys_lit(lit, (uint32_t)((int32_t)KEY0 + 12 * oct));
            fm1_led_key(BTN_OCT_DOWN, oct < 0);
            fm1_led_key(BTN_OCT_UP, oct > 0);
            fm1_led_key(BTN_PLAY, eng.seq_run && eng.t[0].seq_shown % 4u == 0u);   /* blinks on the beat */
            fm1_led_key(BTN_SEL, eng.mode_on != SD_MODE_MONO);   /* SEL lit: DUO or POLY */
            fm1_led_key(BTN_GLO, shift());      /* GLO lit: held or latched, where it does something */
            fm1_led_key(BTN_HOME, page == 5u || page == PAGE_SETUP);   /* a page's button lit: you are there */
            fm1_led_key(BTN_FX, (page >= 1u && page <= 3u) || page == PAGE_TIME);
            fm1_led_key(BTN_ENV, page == 0u || page == PAGE_ENV || page == PAGE_FILT || page == PAGE_SUB);
            fm1_led_key(BTN_LFO, page == 4u);
            fm1_led_key(BTN_ARP, page == PAGE_ARP || page == PAGE_LOOP);
            fm1_led_key(BTN_SEQ, seq_page() || page == PAGE_ORB || page == PAGE_DRUM);
            fm1_led_key(BTN_EDIT, browse_at || sw == SW_WAIT ? (fm1_ms / 200u) & 1u : sw == SW_ON || sw == SW_LOOP);   /* EDIT lit: what this page shows
                                                   is on (the list open: it blinks, it loads) */
            fm1_led_key(BTN_REC, seq_rec || dr_rec);
        }
        click_want = (uint8_t)(set.click == 2u || (set.click == 1u && seq_rec));

        if (saved_at && fm1_ms - saved_at > 1500u) saved_at = 0;
        if (browse_at && (fm1_ms - browse_at >= BROWSE_MS || save_mode)) browse_at = 0;   /* untouched: not now */
        if (dlist_at && (fm1_ms - dlist_at >= DLIST_MS || page != dlist_pg || browse_at || save_mode)) dlist_at = 0;
#ifdef SD_NO_DRAW                               /* a measure, never shipped (DINGHY_DEFS=-DSD_NO_DRAW=1): what the screen
                                                   costs the audio (it draws once, then not again) */
        if (frame_at == 0u && fm1_ms - frame_at >= 40u) {
#else
        if (fm1_ms - frame_at >= 40u) {         /* the screen: 25 frames a second, each region sent when it changed */
#endif
            frame_at = fm1_ms;
            idle_ok = 1;
            LR_AT(LR_DRAW);
            draw_frame();
            LR_AT(LR_DRAWN);
            idle_ok = 0;
        }

        if (save_pending && eng.seq_run) save_at = fm1_ms;   /* (the wait counts from STOP) */
        if (save_pending && (eng.seq_run || demo_st)) save_at = fm1_ms;   /* (the demo: after it) */
        if (save_pending && !eng.seq_run && !demo_st && (out_quiet() || fm1_ms - save_at > 3000u)) {   /* stopped, quiet (or 3 s
                                                   on): write */
            int bad = 0;                        /* (the device reads each save back: one that did not take says so;
                                                   the last whole copy stays, store_io.h) */
            if (save_pending & 2u) {
                LR_AT(LR_SAVE + 1);
                bad |= slot_save(1, &useq, USEQ_HALF);
                LR_AT(LR_SAVE + 2);
                bad |= slot_save(2, (uint8_t *)&useq + USEQ_HALF, sizeof useq - USEQ_HALF);
            }
            LR_AT(LR_SAVE + 3);
            if (save_pending & 4u) bad |= slot_save(3, &usong, sizeof usong);
            LR_AT(LR_SAVE + 4);
            if (save_pending & 8u) bad |= slot_save(4, &udr, sizeof udr);
            LR_AT(LR_SAVE + 5);
            if (save_pending & 1u) bad |= slot_save(0, &upat, sizeof upat);
            if (bad) msg("SAVE FAILED");
            save_pending = 0;
            dirty = 1;
            t_change = fm1_ms - 2000u;
        }
        if (dirty && !eng.seq_run && !demo_st && out_quiet() && fm1_ms - t_change > 2000u) {   /* quiet: an erase stalls the sound */
            LR_AT(LR_SETTINGS);
            if (settings_save(&set, sizeof set)) msg("SAVE FAILED");
            dirty = 0;
        }
        LR_AT(LR_END);
#ifdef DINGHY_WEB
    } while (0);
#else
    }
#endif
}

#ifdef DINGHY_WEB
#include "../../web/app/web_lab.h"              /* the lab (web/lab/): the patch opened up to the page */
/* the console (sd_console.h, web/console/): the player's frames, as the FM-1 sends them over USB */
static uint8_t web_con[SD_CON_MAX];
WEB_EXPORT(web_con_ptr) uint8_t *web_con_ptr(void) { return web_con; }
WEB_EXPORT(web_con_frame) uint32_t web_con_frame(void) { con_until = 1u; return sd_con_frame(web_con); }
#endif
