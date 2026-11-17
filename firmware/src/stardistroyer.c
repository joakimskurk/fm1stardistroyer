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
 *                  HOME  MACROS -> NAV -> SETUP   FX  REACTOR -> DISTROY -> SPACE -> TIME   ENV  ENV -> FILTER
 *                  LFO   LFO 1 -> LFO 2 -> RADAR -> MOD    ARP  ARP -> LOOPER   SEQ  SEQ A -> SEQ B -> ORBIT
 *                  (ARP's two are the player's, kept apart from the patch)
 *   K1..K4     the four values in the row under the top line, left to right as the knobs
 *   GLO        held, their second layer (the names turn red); a tap latches it, a tap again (or a page button)
 *              lets it go (GLO lit while it holds). On SEQ the keys transpose the track, at its one: IN KEY (the default: the root to
 *              the key pressed, in the master's key, its notes lit; the other track's notes pulled into it) or CHROM
 *              (from C4, as written), SETUP's XPOSE; GLO + OCT an octave more or less (-24..24)
 *   ALGORITHM  the page's choice, said on the hint line (GLO + ALGORITHM: the second)
 *   EDIT       ON / OFF of what the page shows (the top line's right end; EDIT's light: on): COURSE, REACTOR, DISTROY,
 *              SPACE, FILTER (fx_off: the settings kept), ARP, a track (SEQ B: not in MONO), ORBIT, RADAR. While
 *              the master plays, the other track and ORBIT switched on come in at its one (ON blinks till then),
 *              from their step 1
 *   PRESETS    the patch (on SEQ the pattern, P01..P32): it turns a list, the sound as it is; EDIT loads, HOME not
 *              now
 *   SELECT     which one, where a page has them in a row (select_turn): SEQ's step (SEGMENTS: the part), ORBIT's
 *              step, REACTOR's stage (STAGES), DISTROY's band, MOD's slot, SAVE's place; elsewhere nothing (the tempo
 *              is SETUP's K1)      SEL the voices: MONO -> DUO -> POLY (lit: DUO or POLY)
 *   OCT- OCT+  the keys' octave, on every page, REC or not; a question (PRESETS' list, SAVE, CLR): OCT+ yes, OCT- no;
 *              on SEQ, GLO + OCT the track's transpose an octave (as GLO + a key); on LOOPER, GLO + OCT the loop's
 *              octave
 *   PLAY       the patterns on / off   REC recording into the track SEQ edits
 *              (ENV a TIE, LFO a REST); what is heard is written: a key under the track's transpose (GLO + a key,
 *              ORBIT's XPOSE) goes in back by it, so the step plays the key's note (sd_seq_rec_note)
 *   SAVE       the patch (on SEQ the pattern) to a user place: ALGORITHM picks it, SAVE again (or OCT+) writes, HOME
 *              or OCT-: not now
 *   the keys   play; their lights show what they do (keys_lit). On SEQ the white keys are the bank's 16
 *              steps, the black keys B1 RND B2 UNDO B3 CLR, B4 (C#4) MORE (the step's RATCHET, PROB), B10 (D#5) SEG,
 *              B11 (F#5) the keys into the steps; on ORBIT B10 SETUP. Where the keys play, a key is a function only
 *              with GLO: on REACTOR GLO + B10 STAGES (the hint line names it)
 *   the voices MONO 1, DUO 2, POLY 4, the same sound in each (sd_engine.h's sd_voice_banks, vg), shared by quota (sd_plan): a playing track its widest chord, A first, then B,
 *              then ARP one, the keys the rest; no source turns off or takes another's note. ARP and SEQ B are refused
 *              where they do not fit (MONO with SEQ A playing: no ARP, no keys but REC's), and switched off when PLAY,
 *              SEL or a chord takes their room; a message says why. The status line shows the mode and who holds
 *              them (MONO A: A B the tracks, R ARP, - the keys), always (the CPU is on SETUP)
 *   the screen  from the top: the patch (its name, the master's key, the battery's three bars, its place), the button's pages (EDIT's switch a
 *              lamp), K1..K4 (each on a printed scale, its pointer; grey while the engine is off), the picture (104
 *              rows: SEQ's steps in it), the hint (two fixed places, as a panel's legends: ALGORITHM's job on the left,
 *              the second control's, GLO's or a key's, from the middle; GLO held, ALGO red and its second job; a
 *              toggle there a lamp), the status; a list (PRESETS, a DEST) seven names over the knobs and
 *              the picture; between a starship's screen and a synth's panel: boxes a
 *              pixel off each corner (cv_box), lamps, scales; red is the patch, where you are, what is on, what plays
 *              (the logo: the splash's); a black key's job, wherever it is named, has one mark (key_chip): a small
 *              key whose lamp is lit as the key is (dark, a glow, full)
 *
 * The pages' knobs (GLO: the second layer):
 *   NAV     XM WARP FOLD DETUNE (the Q page, 0..99); GLO: X Y WIDTH, K4 a harmonic generator on the
 *              bank (ALL ODD RND CLUST, 14 chords, CPY; a list as it turns, each with the harmonics it sets).
 *              ALGORITHM the bank's roles (SSSC PSSC PPPC SCSC PPCC CSSC SCCC CCCC; a list as it turns: how many are
 *              heard, the routing); GLO + ALGORITHM the bank, A B C D. EDIT: COURSE OFF, ON (from each note), LOOP
 *              (round on its own, from START)
 *   MACROS     MUTATE MOVE BREAK MACRO 4; ALGORITHM MACRO 4's kind (BODY AIR DROWN); the picture: the chain, ON / OFF
 *   SETUP      the FM-1's, kept: K1 TEMPO, 40..240 BPM (a MIDI clock's while one leads; GLO + K1 SYNC, AUTO: a MIDI
 *              clock leads when it comes; INT: not heard), K2 XPOSE (IN KEY or
 *              CHROM), K3 MASTER (SEQ A or B: whose one leads); a turn right the second; K4 COLOUR, the screen's accent
 *              (RED, the default, ORANGE PINK MAGENTA VIOLET BLUE CYAN GREEN: a list as it turns, its band the
 *              colour; the whole screen at once, the splash too); the picture draws each under its knob (the MIDI
 *              clock into the tempo, the master's scale on an octave's keys, whose one leads; under COLOUR the CPU,
 *              a gauge, a lamp for each core: the second red once it gave up, dark if it never started). ALGORITHM: CLEAR ALL, a start from scratch (OCT+ yes, OCT- no: SEQ A and B empty, ORBIT off
 *              and empty, the transposes 0; the sound, the key and the scale kept; clear_all)
 *   REACTOR      POS RES SLEW DRIVE; GLO: FREQ MODEL DIV MIX; ALGORITHM TRACK (OFF KEY HARM KEY+H), GLO + ALGO LOW.
 *              EDIT's first time on a patch with no stages: six vowels on POS, so it sounds (react_first).
 *              GLO + B10 (D#5): STAGES (B10 alone there: back; D#5 plays otherwise), what each stage holds: the
 *              white keys 1..6 (or ALGORITHM) the stage, POS to it;
 *              K1 TYPE (13 vowels, 13 chords: a list) K2 PITCH K3 WIDTH K4 LEVEL; GLO: F1..F4. The picture: the
 *              filters as peaks across the width, under them the six stages as a piano roll (the one playing red)
 *   DISTROY    ALGORITHM the band 1..4 (LOW LO-MID HI-MID TOP, on the hint line), then LOOP. A band: TYPE (NONE
 *              SOFT HARD FOLD CRUSH) DRIVE, ALT by what it does for the type (KNEE SPACING DEPTH), LEVEL; GLO: BIAS
 *              PAN DYNAMIC (PHAT .. SHARP) SPLIT, GLO + ALGORITHM its BODY. LOOP: FDBK TIME SPREAD TAME; GLO: MIX
 *              XOVER. EDIT's first time on a patch with no bands: four that sound (dest_first)
 *   SPACE      the reverb: SIZE DECAY TONE R-LEVEL; GLO: SHIMMER. On the second core a plate, on one
 *              core the room. ALGORITHM: FREEZE (the player's, not saved: the plate's input closed, its tail held for
 *              ever); SHIMMER (GLO's K1): an octave up of the tail back into it, 0..99, s_mode's top bits;
 *              PITCH (GLO's K2, the player's): the frozen tail up or down, -12 .. 12 semitones, in and out over 50 ms
 *              (on the second core; 0, or not frozen: the plate as ever)
 *   TIME       the delay: TIME FDBK TONE D-LEVEL; ALGORITHM its MODE (FREE SYNC FREE PP SYNC PP)
 *   LOOPER     the looper (the player's, not the patch's; always recording what SPACE hears before the reverb):
 *              GLIDE LENGTH SPEED L-LEVEL (GLO: FILTER, a lowpass left, a highpass right, none in the middle;
 *              R-SEND, D-SEND); EDIT arms it or not, at any L-LEVEL (0: it records, silent). ALGORITHM: LOOP (it stops and plays
 *              its last LENGTH round, at its SPEED, +-12 semitones; into the reverb as the synth goes; the delay
 *              goes on); GLO + ALGORITHM: REVERSE (it plays it backwards); GLO + OCT: SPEED to the next octave down
 *              or up, over GLIDE (OFF: at once), the loop landing on its own grid. Armed, it borrows the delay
 *              line's second half (5.1 s with the RAM's last 24 KB, mono mu-law), the
 *              delay then 0.5 s. An engine off (EDIT), ARP or RADAR off, the LOOPER disarmed: the hint line says
 *              OFF: EDIT TURNS IT ON (not SEQ's pages: a part is written while it is off).
 *              The loop follows PLAY: STOP parks it (it fades out and waits, kept: LOOP WAITS FOR PLAY), PLAY plays
 *              it again in sync, where it stood against the master's one (LOOP BACK ON THE ONE)
 *   ENV        ENV 1 (the level): ATTACK DECAY SUS RELEASE; GLO: ENV 2 (the FM's amount)
 *   FILTER     a lowpass on each voice: CUTOFF RES ENV (ENV 3's depth) KEY; GLO: ENV 3; ALGORITHM LP12 / LP24,
 *              GLO + ALGORITHM VEL
 *   LFO 1, 2   SHAPE RATE DEST AMOUNT: DEST and AMOUNT its first MOD slot's (a free one the first time, +30 %; DEST
 *              OFF lets it go); ALGORITHM its MODE, said on the hint line (FREE RUNNING, SYNC TO THE TEMPO, either
 *              RESTARTING EACH NOTE or ONE CYCLE A NOTE); the picture: where the matrix sends it
 *   RADAR      a loop of random steps (RADAR in the code: tur_*, SD_TUR_*), a modulation source of the patch: a
 *              loop of bits on the tempo, ALGORITHM its DIV.
 *              K1 CHANGE (the chance a bit flips as it comes round: 0 a locked loop), K2 LENGTH (2..16 steps, 16 at first), K3 DEST
 *              (a matrix destination, or NOTES: SEQ A's notes moved in its scale, the pattern untouched; to PITCH
 *              quantized to SEQ A's key first, sd_tq_snap), K4 AMOUNT;
 *              EDIT on / off (off by default; on the first time: to XM, 30 %). In MOD it is a SOURCE too. The
 *              picture: the loop as a scope (pic_tur), each step held at its radius over its arc and a hard jump to
 *              the next (as it sounds), the sweep where it is; a gauge of the value now
 *   MOD        the slot ALGORITHM picks (1..8): SOURCE DEST AMOUNT. SOURCE (20, a list as it turns: by where each comes
 *              from, the envelopes, the LFOs and RADAR, NAV, the playing, the engines). DEST (here, on RADAR and
 *              ORBIT's lanes): 54, by
 *              the page their knob is on and its name there, in the pages' order (on each K1..K4, then GLO's): the
 *              voice, NAV, the envelopes, the filter, REACTOR, DISTROY (each band's DRIVE and SPLIT, one crossover
 *              through another; the loop), SPACE, TIME (the delay, the looper's SPEED: free, RADAR's in semitones),
 *              the LFOs' RATE; while it turns a list in the knobs' place, as PRESETS' (draw_dlist). The picture:
 *              the eight slots in the knobs' columns, each its source over its destination and its amount's bar
 *   ARP        MODE (UP DOWN UP-DN RND ORDER) RATE (1/64 .. 1/2, triplets, dotted) OCTAVES GATE; ALGORITHM HOLD. The
 *              player's, not the patch's. REC there: KEEP, what it just played (the last bar at its RATE) as a SEQ
 *              track's pattern, SEQ A's (B's while A plays), UNDO on SEQ (arp_keep). While the patterns play its
 *              steps fall on their grid from START (a hand under a quarter step late plays at once); stopped, from
 *              the key
 *   SEQ A, B   two tracks on the one sound, each its pattern and LENGTH (a melody and a bass line). The step the
 *              white key picks: NOTE VEL GATE TYPE (NOTE SLIDE TIE REST SKIP); ALGORITHM the next step. GLO: LENGTH
 *              DIV SWING XPOSE; GLO + ALGORITHM the SCALE, GLO + SELECT the ROOT (the key's own, root_own; the
 *              notes as written). Stopped, REC off: the step picked sounds (aud_step). B4 MORE: K1 RATCHET K2 PROB, and the note heard at the
 *              step: K3 MOVE (a step earlier or later: a join stays a join, another note's place never taken) K4
 *              LENGTH (how many steps it holds, its TIEs; sd_note_move, sd_note_resize). The black keys there, lit: B1 RND, B2 UNDO, B3 CLR,
 *              B10 SEG, SEGMENTS (the pattern's parts: a white key picks the part its step is in, framed, the rest
 *              dim; ALGORITHM how many, 1..6, even; K1 REPEAT K2 ORDER (FWD REV P-P RND) K3 SKIP K4 LENGTH, its end
 *              moved; the picture says how they play, PLAYS A B<x2 C), B11 the keys into the steps (REC, B11 again:
 *              back). REC: keys held together, a chord; playing, a key held on writes a TIE into each step it is
 *              held over (a new key while one is held: the next note, legato, unless in the same step: a chord)
 *   ORBIT      a sub-sequencer of the master's pattern: 2..8 steps on the first white keys, each RATE
 *              loops of it; the step the key (or ALGORITHM) picks: K1 XPOS A, K2 XPOS B, K3 LANE 1, K4 LANE 2 (each its DEST,
 *              -100..100 %); GLO: the macros' changes (MUTATE MOVE BREAK MACRO 4, -99..99). B10 (D#5): SETUP,
 *              K1 LENGTH K2 RATE (1 2 4 loops) K3 DEST 1 K4 DEST 2; GLO there: GLIDE for lane 1, lane 2, the macros.
 *              EDIT on / off. Every value a change from where the player is (0 none); saved with the pattern. The
 *              step picked (SELECT, ALGORITHM, a knob, GLO + a key) is held while it is edited (eng.orb_hold, the
 *              hint's lamp): 2 s after the last edit, GLO + its key again, or leaving ORBIT, on in time
 *   MIDI in    notes from USB or the TRS jack, any channel: they play as the keys do, on every page (ARP takes them
 *              while on), and REC writes them as the keys', at their velocity (midi_note); CC 1 the matrix's WHEEL,
 *              channel pressure its AFTER. A clock (24 a quarter) leads the tempo while it comes (SYNC AUTO; GLO +
 *              PLAY: INT, not heard): the tracks, ARP, RADAR, the synced LFOs and MOVE move by its pulses
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
#if !defined(DINGHY_HOST) && defined(SD_TWO_CORES) && SD_TWO_CORES
#define FM1_CPU1 1                              /* the second core (hal/fm1_cpu1.h): SPACE's plate runs there. A build
                                                   of its own since 2026-11-18 (DINGHY_DEFS=-DSD_TWO_CORES=1 ./build.sh):
                                                   on the user's FM-1 the plate's first job after SPACE had rested (the
                                                   first key after a quiet spell) crashed it, the second core's bus read
                                                   in sd_plate_blk taking the first with it, once the audio's code ran
                                                   from RAM (Sound Plan 6.8). One core, the default: the room */
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
volatile uint32_t sd_c1_load[3];                /* its jobs, timer ticks: the longest, all, how many (for fm1-emu's
                                                   --dump: tools/emu_cpu.sh) */
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
    dt = fm1_ticks() - t0;
    if (dt > sd_c1_load[0]) sd_c1_load[0] = dt;
    sd_c1_load[1] += dt;
    sd_c1_load[2]++;
}
/* the second core's half of each sub-block (sd_engine.h's sd_c1_share: banks 3 and 4, REACTOR's resonators 3 and
 * 4, DISTROY's right channel) beside the first core's; the engine's own buffers, so no pointer is read from RAM. Busy
 * with the plate, or retired: the first core does it. OFF (SD_C1_HALVES 0) since 2026-11-17: on the user's FM-1 the
 * second core faulted inside it (its stack limit and a bus write, in sd_bank_run, 12 s up) and the FM-1 crashed; the
 * day before, the plate alone on it (no halves) ran clean. A job each sub-block (1378 a second, not 172) and far more
 * of the shared RAM read: where a core that misreads now and then (fm1_cpu1.h) meets a pointer. On, the gate reads
 * a few points lower; off, the first core renders every bank, the same sound */
#ifndef SD_C1_HALVES
#define SD_C1_HALVES 0
#endif
#if SD_C1_HALVES
SD_HOT static void sd_c1_half(uint32_t arg) { SD_FAR(sd_c1_share)(&eng, arg); }
SD_HOT static int sd_c1_hpost(void *e, uint32_t arg) { (void)e; return fm1_cpu1_run(sd_c1_half, arg); }
SD_HOT static int sd_c1_hjoin(void *e) { (void)e; return fm1_cpu1_wait() ? -1 : 0; }
#endif
SD_HOT static int sd_c1_post(void *e, uint32_t b)
{
    (void)e;
#if defined(SD_C1_NOPLATE) && SD_C1_NOPLATE     /* (a test build: the second core started, given no plate; the room
                                                   here. DINGHY_DEFS=-DSD_C1_NOPLATE=1, 2026-11-17's bisection) */
    (void)b;
#else
    if (fm1_cpu1_run(sd_c1_plate, b)) return 1;
#endif
    eng.sp_room = 1;
    return 0;
}
SD_HOT static int sd_c1_join(void *e)
{
    (void)e;
    if (!fm1_cpu1_wait()) return 0;
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
static uint32_t cpu_worst, cpu_frames;
static volatile uint32_t cpu_permille = 0xFFFFFFFFu;   /* the worst interrupt of the last second, 0.1 % (none yet) */

SD_HOT static void app_audio(int32_t *out, uint32_t n)
{
    uint32_t t0 = sd_ticks(), dt;
    SD_FAR(sd_engine_render)(&eng, out, n);   /* (in RAM: sd_engine.h's SD_RAM) */
    dt = sd_ticks() - t0;
    if (dt > cpu_worst) cpu_worst = dt;
    cpu_frames += n;
#ifndef DINGHY_WEB                              /* (in a browser there is no chip to measure: CPU --) */
    if (cpu_frames >= SD_FS) {                  /* once a second: the worst, against one interrupt's period */
        uint32_t period = n * 1000u / 441u * SD_TICKS_PER_US * 10u;   /* n frames in ticks: n * 22.68 us */
        cpu_permille = period ? cpu_worst * 1000u / period : 0u;
        cpu_worst = 0;
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
static uint8_t bk_led[11];                      /* the black keys' function lights, as keys_lit set them last: 0
                                                   dark, 1 a glow, 2 full (a blink: between the two) */
static int32_t key_chip(int32_t x, int32_t y, uint32_t bk, const char *nm, uint16_t c)   /* a black key's job on the
                                                   screen, the same everywhere: the key with its lamp as the key's own
                                                   (dark, a glow, full), then its name; the x after it */
{
    uint32_t st = bk < 11u ? bk_led[bk] : 0u;
    cv_box(x, y + 2, 5, 11, C_GRID);
    cv_rect(x + 1, y + 3, 3, 4, st == 2u ? C_RED : st ? C_RED_D : C_BG);
    return cv_text(x + 7, y, &FONT_S, nm, st ? c : C_ICE_D);
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
static struct { uint8_t oct, bpm, patch, mode, pattern, amode, adiv, aoct, agate, ahold, xchrom, sync_int, master, accent; } set =
    {0, 120, SD_PATCH_WORST, SD_MODE_MONO, 0, SD_AM_UP, 2, 1, 50, 0, 0, 0, 0, 0};   /* (xchrom: the transposes in
                                                   semitones; 0, the default, IN KEY: in SEQ A's scale. sync_int: a
                                                   MIDI clock not heard; 0, the default, AUTO: one leads when it
                                                   comes. master: the track whose one leads, 0 A, 1 B) */
                                                /* kept in flash; DISTROY first: the costliest patch, so fm1-emu
                                                   measures it. patch: the factory's SD_PATCHES, then the user's */

/* ---- what the player saves (SAVE): 32 patches in slot 0, 32 patterns in slot 1, in the pool (no RAM). A change of
 * either struct's size reads as nothing saved (store.h's length check): the factory and defaults then */
#define UPATCHES 32u
#define UPATTERNS 32u
typedef struct { uint32_t used; char name[UPATCHES][8]; SdPatch p[UPATCHES]; } UserPatches;
typedef struct { uint32_t used; SdPattern p[UPATTERNS]; } UserPatterns;
/* the patterns in two slots (1, 2): 32 of them, with their chords, pass one slot's 16 KB */
#define USEQ_HALF ((uint32_t)(uintptr_t)&((UserPatterns *)0)->p[UPATTERNS / 2u])
_Static_assert(sizeof(UserPatches) <= SLOT_MAX, "a slot holds them");
_Static_assert(sizeof(UserPatterns) / 2u + sizeof(SdPattern) <= SLOT_MAX, "two slots hold the patterns");
typedef struct { uint32_t used; SdOrbit o[UPATTERNS]; } UserOrbits;   /* ORBIT, a pattern's (A's), in slot 3: the
                                                   patterns' own slots (and the ones saved before) unchanged */
_Static_assert(sizeof(UserOrbits) <= SLOT_MAX, "a slot holds the orbits");
#if defined(DINGHY_HOST) && (defined(__APPLE__) || defined(DINGHY_WEB))
static UserPatches upat;
static UserPatterns useq;
static UserOrbits uorb;
#else
static UserPatches upat __attribute__((section(".pool")));
static UserPatterns useq __attribute__((section(".pool")));
static UserOrbits uorb __attribute__((section(".pool")));
#endif
static SdOrbit orbp;                            /* ORBIT as the knobs left it (SEQ A's pattern's) */
static uint32_t orb_cur, orb_setup;             /* the step the keys picked; SETUP (B10, D#5) */
#define ORB_HOLD_MS 2000u
static uint32_t orb_hold_at;                    /* the last edit that held ORBIT's step (2026-11-17, the user: held
                                                   only while it is edited; after it, the page shows ORBIT in time) */
static void orb_hold_step(uint32_t s)           /* the step edited: held, heard, till ORB_HOLD_MS after the last edit */
{
    eng.orb_hold = (uint8_t)(s + 1u);
    orb_hold_at = fm1_ms;
}
static void orb_push(void) { sd_engine_orbit(&eng, &orbp); }
static void orb_load(uint32_t i)                /* pattern i's ORBIT (none saved: off, 4 steps) */
{
    if (uorb.used >> i & 1u) orbp = uorb.o[i];
    else { uint8_t *z = (uint8_t *)&orbp; uint32_t k; for (k = 0; k < sizeof orbp; k++) z[k] = 0; }
    if (orb_cur >= SD_ORB_LEN(&orbp)) orb_cur = 0;
    orb_push();
}
#define NPATCHES (SD_PATCHES + UPATCHES)
static uint32_t save_mode, save_to, save_pending, save_at, saved_at, save_kind;   /* SAVE: picking (1 patch, 2
                                                   pattern), where, waiting for quiet, when it was written, what */

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
static void patch_load(uint32_t i, SdPatch *p) { *p = i < SD_PATCHES ? SD_PATCH[i] : upat.p[i - SD_PATCHES]; }
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
#define PAGE_SETUP 13u                          /* HOME's third: SETUP, the FM-1's settings (kept in flash) */
static uint32_t fx_last = 1u, env_last = PAGE_ENV, arp_last = PAGE_ARP, seq_last;   /* the page buttons: each the
                                                   page it showed last, where it comes back to (SEQ: 0 SEQ A, 1 SEQ B,
                                                   2 ORBIT; LFO: dmod) */
/* the screen's rows (the frame, below) */
#define Y_NAV 24u                               /* (the patch's line on top: the logo is the splash's) */
#define Y_KNOB 46u
#define H_KNOB 50u
#define Y_PIC 98u
#define H_PIC 104u                              /* (the steps' row gone, 2026-11-15: the picture took its room) */
#define Y_HINT 204u
#define Y_STAT 222u
#define H_LIST (Y_PIC + H_PIC - Y_KNOB)         /* PRESETS' list, a DEST's: over the knobs and the picture */
static uint32_t dslot;                          /* the MOD slot last picked */
static uint32_t seq_trk;                        /* the track SEQ edits: 0 A, 1 B */
static SdPattern seqp, seq_undo;                /* the pattern as the keys left it; the one before the last RND, CLR */
static uint32_t seq_has_undo, seq_cur, seq_bank, seq_segmode, seq_dseg, seq_rec, seq_clr_at;
static uint32_t seq_more;                       /* STEP's second layer: RATCHET, PROB (B4, C#4: MORE) */
static uint32_t seq_rng = 0x5D5Du;
static const char *const STYPE_NAME[SD_STYPES] = {"NOTE", "SLIDE", "TIE", "REST", "SKIP"};
static const char *const SORDER_NAME[SD_SORDERS] = {"FWD", "REV", "P-P", "RND"};
static const char *const SORDER_GLYPH[SD_SORDERS] = {"", "<", "<>", "?"};
static const char *const SDIV_NAME[SD_SDIVS] = {"1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4", "1/64", "1/32T",
                                                "1/16D", "1/4T", "1/8D", "1/2"};
static const char *const SCALE_NAME[SD_SCALES] = {"MINOR", "MAJOR", "DORIAN", "PHRYG", "PENTA-", "PENTA+", "CHROM"};
static uint32_t dmod = SD_MODS;                 /* LFO's page: slots 0..7 (MOD, ALGORITHM picks), 8 LFO 1, 9 LFO 2,
                                                   10 RADAR (LFO 1 first) */
#define DMOD_TUR (SD_MODS + 2u)
static const char *const SRC_NAME[SD_MSRCS] = {"OFF", "ENV1", "ENV2", "LFO1", "LFO2", "NAV X", "NAV Y", "VEL", "KEY",
                                               "WHEEL", "AFTER", "RAND", "STEP", "SEGMENT", "D LOW",
                                               "D TOP", "R STAGE", "ECHO", "ENV3", "RADAR"};
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
                                               "SPL LOW", "SPL LM", "DRV LOW", "DRV LM", "DRV HM"};
static const char *const DST_SHORT[SD_MDSTS] = {"--", "PIT", "XM", "WRP", "FLD", "DET", "X", "Y", "WID", "POS", "RFQ",
                                                "RES", "RDR", "RMX", "DRV", "BIA", "LFB", "TAM", "LMX", "TFB", "DLV",
                                                "RLV", "DCY", "DR4", "SP3", "CUT", "FRS", "A1", "D1", "S1", "R1", "A2",
                                                "D2", "S2", "R2", "A3", "D3", "S3", "R3", "FEN", "SLW", "LTM", "SPR",
                                                "TIM", "DTO", "SIZ", "RTO", "L1R", "L2R", "LSP", "SP1",
                                                "SP2", "DR1", "DR2", "DR3"};
/* the destinations' list (as PRESETS' list: easier to see and hit): each its page and its knob's name there, in
   SD_MD_ORDER (the pages' order, and on each its knobs': K1..K4, then GLO's) */
enum { DG_NONE, DG_VOICE, DG_NAV, DG_ENV1, DG_ENV2, DG_FILT, DG_ENV3, DG_REACT, DG_DEST, DG_SPACE, DG_TIME, DG_LFO1,
       DG_LFO2, DG_SEQ };
static const char *const DG_NAME[] = {"", "VOICE", "NAV", "ENV 1", "ENV 2", "FILTER", "ENV 3", "REACTOR", "DISTROY",
                                      "SPACE", "TIME", "LFO 1", "LFO 2", "SEQ A"};
static const uint8_t DST_GRP[SD_MDSTS] = {DG_NONE, DG_VOICE, DG_NAV, DG_NAV, DG_NAV, DG_NAV, DG_NAV, DG_NAV, DG_NAV,
    DG_REACT, DG_REACT, DG_REACT, DG_REACT, DG_REACT, DG_DEST, DG_DEST, DG_DEST, DG_DEST, DG_DEST, DG_TIME,
    DG_TIME, DG_SPACE, DG_SPACE, DG_DEST, DG_DEST, DG_FILT, DG_FILT, DG_ENV1, DG_ENV1, DG_ENV1, DG_ENV1, DG_ENV2,
    DG_ENV2, DG_ENV2, DG_ENV2, DG_ENV3, DG_ENV3, DG_ENV3, DG_ENV3, DG_FILT, DG_REACT, DG_DEST, DG_DEST, DG_TIME,
    DG_TIME, DG_SPACE, DG_SPACE, DG_LFO1, DG_LFO2, DG_TIME, DG_DEST, DG_DEST, DG_DEST, DG_DEST,
    DG_DEST};
static const char *const DST_PARAM[SD_MDSTS] = {"OFF", "PITCH", "XM", "WARP", "FOLD", "DETUNE", "X", "Y", "WIDTH",
    "POS", "FREQ", "RES", "DRIVE", "MIX", "ALL DRIVE", "ALL BIAS", "LOOP FDBK", "LOOP TAME", "LOOP MIX", "FDBK",
    "D-LEVEL", "R-LEVEL", "DECAY", "TOP DRIVE", "HI-MID SPLIT", "CUTOFF", "RES", "ATTACK", "DECAY", "SUSTAIN",
    "RELEASE", "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "ENV", "SLEW",
    "LOOP TIME", "LOOP SPREAD", "TIME", "TONE", "SIZE", "TONE", "RATE", "RATE", "LOOP SPEED", "LOW SPLIT",
    "LO-MID SPLIT", "LOW DRIVE", "LO-MID DRIVE", "HI-MID DRIVE"};
static const char *tdest_name(uint32_t d)       /* RADAR's DEST: a matrix destination, or NOTES */
{
    return d == SD_TD_NOTES ? "NOTES" : d < SD_MDSTS ? DST_NAME[d] : "OFF";
}
static const char *const LSHAPE_NAME[SD_LSHAPES] = {"SINE", "TRI", "SAW", "RAMP", "SQUARE", "S&H"};
#define LMODES 6u
static const char *const LMODE_SAY[LMODES] = {"FREE RUNNING", "SYNC TO THE TEMPO", "FREE, RESTARTS EACH NOTE",
                                             "SYNC, RESTARTS EACH NOTE", "FREE, ONE CYCLE A NOTE",
                                             "SYNC, ONE CYCLE A NOTE"};   /* (on the hint line: what each does) */
static const uint8_t LMODE_BITS[LMODES] = {0, SD_LM_SYNC, SD_LM_KEY, SD_LM_SYNC | SD_LM_KEY, SD_LM_ONE,
                                           SD_LM_SYNC | SD_LM_ONE};   /* ONE: one cycle a note, held at its end */
static uint32_t lmode_of(uint32_t bits) { uint32_t i; for (i = 0; i < LMODES; i++) if (LMODE_BITS[i] == (bits & 7u)) return i; return 0; }
static const char *const LDIV_NAME[SD_LFO_DIVS] = {"1/16", "1/8", "1/8D", "1/4", "1/4D", "1/2", "1/2D", "1 BAR",
                                                   "1.5 BAR", "2 BAR", "3 BAR", "4 BAR"};
static const char *const M4_NAME[SD_M4S] = {"BODY", "AIR", "DROWN"};
static const char *const DIV_NAME[SD_LP_NDIV] = {"1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D",
                                                 "1/4", "1/2T", "1/4D", "1/2", "1 BAR", "2 BAR", "4 BAR"};   /* (the
                                                   last three the looper's LENGTH only) */
static const char *const SPMODE_NAME[4] = {"FREE", "SYNC", "FREE PP", "SYNC PP"};
static uint32_t dband;                          /* DISTROY's band ALGORITHM picks: 0..3, then 4 LOOP (D3) */
static const char *const SHAPE_NAME[SD_DSHAPES] = {"NONE", "SOFT", "HARD", "FOLD", "CRUSH"};
/* DISTROY's page: what ALT does for each type, the bands' names */
static const char *const ALT_NAME[SD_DSHAPES] = {"--", "KNEE", "KNEE", "SPACING", "DEPTH"};
static const char *const BAND_NAME[4] = {"LOW", "LO-MID", "HI-MID", "TOP"};
static const char *const MODEL_NAME[SD_RMODELS] = {"BP6", "BP12", "VBP", "KP", "AP1", "AP2"};
static const char *const MODE_NAME[SD_MODES] = {"MONO", "DUO", "POLY"};
static const char *const AMODE_NAME[SD_AMODES] = {"UP", "DOWN", "UP-DN", "RND", "ORDER"};
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
static uint32_t dst_rank(uint32_t d)            /* its place in the list */
{
    uint32_t r = 0;
    while (r < SD_MDSTS - 1u && SD_MD_ORDER[r] != d) r++;
    return r;
}
static uint32_t dst_step(uint32_t d, int32_t e, uint32_t notes)   /* the next in the list (notes: RADAR's NOTES at
                                                   its end) */
{
    uint32_t n = SD_MDSTS + (notes ? 1u : 0u), r = d == SD_TD_NOTES && notes ? SD_MDSTS : dst_rank(d % SD_MDSTS);
    r = step_mod(r, e, n);
    return r == SD_MDSTS ? SD_TD_NOTES : SD_MD_ORDER[r];
}
static uint32_t sdiv_step(uint32_t d, int32_t e)   /* a division, e detents slower (+) or faster (-) */
{
    uint32_t r = 0;
    while (r < SD_SDIVS - 1u && SD_SDIV_ORDER[r] != d) r++;
    return SD_SDIV_ORDER[step_lim(r, e, SD_SDIVS)];
}

static uint32_t idle_ok;
static uint32_t led_at;                         /* the key and button lights: when last set */

/* GLO: the second layer, held, or latched by a tap (down and up within GL_TAP_MS, nothing else done meanwhile: a
   button, a key, a knob); a tap again lets it go. Held, it is the second layer while it is down, latched or not. (SEL,
   BTN_SEL: the voices, MONO DUO POLY) */
#define GL_TAP_MS 400u
static uint8_t gl_latch, gl_used;
static uint32_t gl_at;
static uint32_t shift(void) { return gl_latch || (fm1_in.buttons >> BTN_GLO & 1u); }
static uint32_t dlist_at, dlist_enc = 0xFFu;    /* the knobs' list (draw_dlist): when it opened, the encoder it is on */
static int32_t enc_take(uint32_t e)             /* an encoder's detents: one turned while GLO is down makes it no tap;
                                                   another than the list's closes the list; the pace (enc_far) */
{
    int32_t v = fm1_enc_take(e);
    if (!v) return 0;
#if !defined(DINGHY_HOST)
    enc_pace(e, v);
#endif
    if (v && (fm1_in.buttons >> BTN_GLO & 1u)) gl_used = 1;
    if (v && dlist_at && e != dlist_enc) dlist_at = 0;
    return v;
}
static uint8_t sp_frz;                          /* SPACE's FREEZE (ALGORITHM there): the player's, not the patch's */
static int8_t sp_pitch;                         /* its PITCH (GLO + K2 there), -12 .. 12: the frozen tail's; the player's */
/* TIME's looper, the player's (not saved with the patch): LOOP (ALGORITHM on TIME), LENGTH (as TIME: ms, or a
   division with SYNC), its speed (semitones x 256), its level */
static uint8_t lp_on, lp_rev;                   /* (lp_rev: REVERSE, GLO + ALGORITHM on TIME) */
static uint8_t lp_arm;                          /* armed (EDIT on LOOPER): it records, whatever L-LEVEL (2026-11-17,
                                                   the user: a loop taken at the bottom, then faded in) */
static uint16_t lp_len = (uint16_t)(5u * 65536u / SD_LP_NDIV + 100u), lp_lvl = 52300u;   /* (1/8, or 334 ms free;
                                                   L-LEVEL 79: only its level now, EDIT arms it) */
static uint16_t lp_filt = 32768u, lp_rsend = 65535u, lp_dsend;   /* GLO on LOOPER: FILTER (none in the middle),
                                                   R-SEND (whole: as it always went into the reverb), D-SEND (none) */
static int16_t lp_speed;
static uint16_t lp_glide;                       /* GLIDE: OFF (a new SPEED at once), else how long it takes */
static const char *msg_txt;                     /* a message in the hint's place, 2 s */
static uint32_t msg_at;
static void msg(const char *t) { msg_txt = t; msg_at = fm1_ms ? fm1_ms : 1u; }

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
    cv_text(w + 40, 2, &FONT_S, s, C_ICE_D);
    p = s;                                      /* its place: P01.., U01.. */
    *p++ = set.patch >= SD_PATCHES ? 'U' : 'P';
    { uint32_t n = set.patch >= SD_PATCHES ? set.patch - SD_PATCHES + 1u : set.patch + 1u; if (n < 10u) *p++ = '0'; p = put_u(p, n); }
    *p = 0;
    cv_text(232 - text_w(&FONT_S, s), 2, &FONT_S, s, C_ICE_D);
    if (batt) batt_icon(232 - text_w(&FONT_S, s) - 24, 6);   /* (the battery, before the place) */
    cv_line(0, 21, 239, 21, C_GRID);
}

/* PRESETS turned: the list over the panel, the sound as it is (browsing changes nothing until EDIT): the one picked
 * in the middle as a red tab, three either side (patches past the empty user places, as the knob steps; on SEQ and
 * ORBIT the patterns), the one playing marked NOW, the place in the whole list on the right. EDIT loads it (its light
 * blinks meanwhile), HOME or 8 s untouched: not now; another button closes it. */
#define BROWSE_MS 8000u
static uint32_t browse_at, browse_to, browse_kind;   /* when PRESETS last moved (0: not browsing); the one picked;
                                                   1 a patch, 2 a pattern */
static char *put_pnum(char *p, uint32_t i)      /* P01..P33, U01..U32 */
{
    uint32_t n = i < SD_PATCHES ? i : i - SD_PATCHES;
    *p++ = i < SD_PATCHES ? 'P' : 'U';
    if (n < 9u) *p++ = '0';
    return put_u(p, n + 1u);
}
static uint32_t patch_step(uint32_t n, int32_t d)
{
    do n = step_mod(n, d, NPATCHES); while (!patch_ok(n));
    return n;
}
static uint32_t browse_step(uint32_t n, int32_t d)   /* the list's next: a patch past the empty places, a pattern */
{
    return browse_kind == 2u ? step_mod(n, d, UPATTERNS) : patch_step(n, d);
}
static void draw_browse(void)                   /* over the knobs and the picture: H_LIST rows, seven names */
{
    char s[8];
    uint32_t r, n = browse_to, total = 0, at = 0, k, pt = browse_kind == 2u, now = pt ? set.pattern : set.patch;
    int32_t y, by;
    cv_frame(2, 0, 236, H_LIST, C_RED);         /* (the frame began the canvas, a strip of it) */
    for (k = 0; k < 3u; k++) n = browse_step(n, -1);
    for (r = 0; r < 7u; r++, n = browse_step(n, 1)) {
        const char *nm = pt ? (useq.used >> n & 1u ? "SAVED" : "EMPTY") : patch_name(n);
        y = 2 + (int32_t)r * 19;
        if (pt) { *s = 'P'; *put_u(s + (n < 9u ? 2 : 1), n + 1u) = 0; if (n < 9u) s[1] = '0'; }
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
    cv_text(10, H_LIST - 20, &FONT_S, pt ? "PATTERNS  OCT+ LOAD  OCT- NO" : "PATCHES  OCT+ LOAD  OCT- NO", C_ICE_D);
    if (pt) { total = UPATTERNS; at = browse_to; }
    else for (k = 0; k < NPATCHES; k++) if (patch_ok(k)) { if (k == browse_to) at = total; total++; }
    cv_rect(228, 6, 3, H_LIST - 30, C_GRID);    /* where in the list */
    by = 6 + (int32_t)(at * (H_LIST - 42u) / (total > 1u ? total - 1u : 1u));
    cv_box(227, by, 5, 12, C_RED);
}

/* REACTOR's STAGES (GLO + B10, D#5, on its page): what each of the six stages holds, on the FM-1. The white keys
 * 1..6 (or ALGORITHM) pick one, and POS goes there so it is what sounds; K1 TYPE puts a vowel or a chord in it from a
 * list (as the plan's presets into a stage: 13 vowels, 13 chords on the stage's root), K2 PITCH moves it a semitone a
 * detent, K3 WIDTH spreads or closes it (F1 to F4, the steps in proportion), K4 LEVEL; GLO: F1..F4, each a semitone */
#define RST_CHORDS 13u
#define RST_N (SD_RVOWELS + RST_CHORDS)
#define RST_CHORD_LV 20000u                     /* (as the patches' chords: a narrow BP12 on a harmonic rings) */
static const char *const RST_CHORD_NAME[RST_CHORDS] = {"MAJ", "MIN", "SUS2", "SUS4", "MAJ7", "MIN7", "DOM7", "ADD9",
                                                       "MIN9", "FIFTHS", "FOURTHS", "DIM7", "AUG"};
static const uint8_t RST_CHORD[RST_CHORDS][4] = {{0, 4, 7, 12}, {0, 3, 7, 12}, {0, 2, 7, 12}, {0, 5, 7, 12},
    {0, 4, 7, 11}, {0, 3, 7, 10}, {0, 4, 7, 10}, {0, 4, 7, 14}, {0, 3, 10, 14}, {0, 7, 12, 19}, {0, 5, 10, 15},
    {0, 3, 6, 9}, {0, 4, 8, 12}};
static const uint16_t RST_VLV[4] = SD_RLV_VOWEL;
static uint8_t r_stages, rst_cur;               /* STAGES on; the stage it edits */
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
enum { DL_MOD = 1, DL_TUR, DL_ORB1, DL_ORB2, DL_SRC, DL_GEN, DL_RST, DL_ALG, DL_BAND, DL_LFO, DL_COL };   /* (DL_BAND: no
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
    static const uint8_t ENC[] = {0, ENC_KNOB2, ENC_KNOB3, ENC_KNOB3, ENC_KNOB4, ENC_KNOB1, ENC_KNOB4, ENC_KNOB1,
                                  ENC_ALGORITHM, ENC_ALGORITHM, ENC_KNOB3, ENC_KNOB4};   /* (each list's knob, in DL_*'s
                                                   order) */
    dlist_kind = kind; dlist_pg = page; dlist_at = fm1_ms ? fm1_ms : 1u;
    dlist_enc = kind < sizeof ENC ? ENC[kind] : 0xFFu;
}
/* the sources in the list's order: the envelopes, the LFOs and RADAR, NAV, the playing, then the engines in the
   chain's order; each where it is set or comes from, and its name there (in the enum's order) */
static const uint8_t SRC_ORDER[SD_MSRCS] = {SD_MS_OFF, SD_MS_ENV1, SD_MS_ENV2, SD_MS_ENV3, SD_MS_LFO1, SD_MS_LFO2,
    SD_MS_TUR, SD_MS_NAVX, SD_MS_NAVY, SD_MS_VEL, SD_MS_KEY, SD_MS_WHEEL, SD_MS_AFTER, SD_MS_RAND, SD_MS_STEP,
    SD_MS_SEG, SD_MS_RSTAGE, SD_MS_DLOW, SD_MS_DTOP, SD_MS_ECHO};
static const char *const SRC_GRP[SD_MSRCS] = {"", "ENV", "ENV", "LFO", "LFO", "NAV", "NAV", "PLAYING", "PLAYING",
    "PLAYING", "PLAYING", "PLAYING", "SEQ A", "SEQ A", "DISTROY", "DISTROY", "REACTOR", "TIME", "FILTER", "LFO"};
static const char *const SRC_PARAM[SD_MSRCS] = {"OFF", "ENV 1", "ENV 2", "LFO 1", "LFO 2", "X", "Y", "VELOCITY",
    "KEY TRACK", "WHEEL", "AFTERTOUCH", "RANDOM EACH NOTE", "STEP", "SEGMENT", "LOW BAND", "TOP BAND", "STAGE",
    "ECHOES", "ENV 3", "RADAR"};
static uint32_t src_rank(uint32_t s) { uint32_t r = 0; while (r < SD_MSRCS - 1u && SRC_ORDER[r] != s) r++; return r; }
static uint32_t src_step(uint32_t s, int32_t e) { return SRC_ORDER[step_mod(src_rank(s % SD_MSRCS), e, SD_MSRCS)]; }
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
static uint32_t dl_n(void) { return dlist_kind == DL_COL ? SD_ACCENTS : dlist_kind == DL_SRC ? SD_MSRCS : dlist_kind == DL_GEN ? SD_GENS : dlist_kind == DL_RST ? RST_N : dlist_kind == DL_ALG ? SD_ALGOS : SD_MDSTS + (dlist_kind == DL_TUR); }
static uint32_t dl_rank(void)                   /* the one picked: its place in the list */
{
    uint32_t d;
    switch (dlist_kind) {
    case DL_SRC: return src_rank(dmod < SD_MODS ? pat.mod[dmod].src : 0u);
    case DL_GEN: return gen < SD_GENS ? gen : 0u;
    case DL_RST: { int32_t m = rst_match(&pat.stage[rst_cur % 6u]); return m < 0 ? 0u : (uint32_t)m; }
    case DL_ALG: { uint32_t a = algo_of(&pat); return a < SD_ALGOS ? a : 0u; }
    case DL_COL: return set.accent % SD_ACCENTS;
    case DL_TUR: d = pat.tur_dst; break;
    case DL_ORB1: d = orbp.dst[0]; break;
    case DL_ORB2: d = orbp.dst[1]; break;
    default: d = mslot() < SD_MODS ? pat.mod[mslot()].dst : 0u;
    }
    return d == SD_TD_NOTES && dlist_kind == DL_TUR ? SD_MDSTS : dst_rank(d % SD_MDSTS);
}
static const char *dl_item(uint32_t r, char *nm)   /* the r-th in the list: where it comes from; its name into nm */
{
    char *p = nm;
    const char *g;
    if (dlist_kind == DL_COL) { p = put(p, ACC_NAME[r % SD_ACCENTS]); g = r ? "" : "DEFAULT"; }   /* (the band:
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
    } else if (r >= SD_MDSTS) { p = put(p, "NOTES"); g = DG_NAME[DG_SEQ]; }
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
    if (dlist_kind == DL_MOD || dlist_kind == DL_SRC) p = put(put_u(put(s, "MOD "), dmod + 1u), dlist_kind == DL_SRC ? " SOURCE" : " DEST");
    else if (dlist_kind == DL_LFO) p = put(put_u(put(s, "LFO "), dmod - SD_MODS + 1u), " DEST");
    else if (dlist_kind == DL_GEN) { p = put(s, "GEN ON BANK "); *p++ = (char)('A' + ebank); }
    else if (dlist_kind == DL_RST) p = put_u(put(s, "REACTOR STAGE "), rst_cur % 6u + 1u);
    else if (dlist_kind == DL_ALG) { p = put(s, "ALGORITHM, BANK "); *p++ = (char)('A' + ebank); }
    else if (dlist_kind == DL_COL) p = put(s, "THE SCREEN'S COLOUR");
    else p = put(put(s, dlist_kind == DL_TUR ? "RADAR" : dlist_kind == DL_ORB1 ? "ORBIT LANE 1" : "ORBIT LANE 2"), " DEST");
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

static char *put_hz(char *p, uint32_t hz)       /* 950 / 1.2k / 12k */
{
    if (hz < 1000u) return put_u(p, hz);
    p = put_u(p, hz / 1000u);
    if (hz < 10000u) { *p++ = '.'; p = put_u(p, hz / 100u % 10u); }
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
static uint32_t time_fold(uint32_t d, uint32_t t, uint32_t lim, char *played)   /* a TIME (or LENGTH: its division d,
                                                   its value t) longer than its line at this tempo: the halvings the
                                                   engine takes (sd_space_sync_q8), and what it plays (into played) */
{
    uint32_t f = 0, k;
    if (pat.s_mode & SD_SP_SYNC) {
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
    return eng.lp_borrow && lp_arm ? SD_SP_LIM2 : SD_SP_LIM;
}
static uint32_t div_fit(uint32_t d, uint32_t was, uint32_t lim)   /* a division turned up to d from was: at most the
                                                   longest that fits its line at this tempo, or where it was (2026-11-17,
                                                   the user: "the divisions jump, 1/4, then back to 1/4": past the line
                                                   the engine plays it halved, sd_space_sync_q8; now the knob stops) */
{
    uint32_t f;
    while (d > was) {
        sd_space_sync_q8(eng.tempo_q8, d, &f, lim);
        if (!f) break;
        d--;
    }
    if (d == was) msg(lim == SD_SP_LIM2 ? "THE LONGEST THAT FITS: 0.5 S" : "THE LONGEST THAT FITS HERE");
    return d;
}
static uint32_t sp_fold(char *played) { return time_fold(sd_sp_div(pat.s_time), pat.s_time, dl_lim(), played); }
static uint32_t lp_fold(char *played) { return time_fold(sd_lp_div(lp_len), lp_len, lp_lim(), played); }
static char *put_glide(char *p)                 /* GLIDE: OFF, a division, or ms */
{
    uint32_t d = sd_lp_glide_div(lp_glide), ms = sd_lp_glide_ms(lp_glide);
    if (pat.s_mode & SD_SP_SYNC) return put(p, d ? DIV_NAME[d - 1u] : "OFF");
    if (!ms) return put(p, "OFF");
    p = put_u(p, ms);
    return put(p, "ms");
}
static char *put_len(char *p)                   /* LENGTH: a division, or ms */
{
    if (pat.s_mode & SD_SP_SYNC) return put(p, DIV_NAME[sd_lp_div(lp_len)]);
    p = put_u(p, sd_sp_ms(lp_len));
    return put(p, "ms");
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
    if (!seq_trk) orb_load(i);
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
    seq_more = 0;
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
static void seq_write_v(uint32_t note, uint32_t type, uint32_t vel)   /* REC: a key (at its velocity), TIE or REST
                                                   into the step */
{
    uint32_t at = eng.seq_run ? eng.t[seq_trk].seq_shown : seq_cur;
    SdStep *s = &seqp.step[at < SD_PSTEPS ? at : 0u];
    if (type == SD_ST_NOTE) {
        s->note = (uint8_t)note; s->vel = (uint8_t)(vel < 1u ? 1u : vel > 127u ? 127u : vel);
        s->chord[0] = s->chord[1] = s->chord[2] = 0;
        rec_at = at < SD_PSTEPS ? at : 0u;      /* keys pressed while this one is held: its chord */
        rec_tie_at = rec_at;
        rec_n = 1;
    }
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
static void seq_keys_up(void)                   /* REC, step recording: every key up, the next step */
{
    if (rec_at == 0xFFu) return;
    if (!eng.seq_run) { seq_cur = seq_next_step(rec_at); seq_bank = seq_cur / 16u; }
    rec_at = 0xFFu;
    rec_n = 0;
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
 * DIV, segments as new; the key and the scale kept), ORBIT off and empty, the transposes back to 0 at the one; the
 * movement (2026-11-17, the user: "MOD destinations, RADAR and so on" stayed): the eight MOD slots empty (the LFOs'
 * routes with them), RADAR off as new, the macros at 0 (the patch as made), COURSE off; the player's tools: ARP off,
 * the looper's LOOP and REVERSE off, disarmed, its SPEED 0, FREEZE off. The sound itself (the banks, the envelopes,
 * the filter, the FX's settings) stays. Saved patterns and patches stay as saved (SAVE writes these) */
static uint32_t clr_all_at;                     /* asked: when (3 s to answer) */
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
    seq_segmode = 0;
    seq_more = 0;
    seq_clr_at = 0;
    for (k = 0; k < SD_MODS; k++) { pat.mod[k].src = pat.mod[k].dst = 0; pat.mod[k].amt = 0; }
    pat.tur_set = 0; pat.tur_ld = 0; pat.tur_dst = 0; pat.tur_amt = 0;
    for (k = 0; k < 4u; k++) pat.mac[k] = 0;
    pat.course = 0;
    sd_engine_patch(&eng, &pat);
    eng.arp_on = 0;
    lp_on = 0; lp_rev = 0; lp_arm = 0; lp_speed = 0; lp_filt = 32768u; lp_rsend = 65535u; lp_dsend = 0;
    sp_frz = 0; sp_pitch = 0;
    clr_all_at = 0;
    msg("ALL CLEAR, THE SOUND KEPT");
}

/* SELECT: which one, where a page has them in a row (the user: "where something is chosen horizontally: steps,
 * segments"): SEQ's step (SEGMENTS: the part), ORBIT's step, REACTOR's stage in STAGES, DISTROY's band, MOD's slot,
 * SAVE's place. Elsewhere nothing (the tempo is SETUP's K1). Returns 1 when the patch changed */
static uint32_t select_turn(int32_t e)
{
    if (save_mode) {
        save_to = step_mod(save_to, e, save_mode == 2u ? UPATTERNS : UPATCHES);
        save_at = fm1_ms;
        return 0;
    }
    switch (page) {
    case PAGE_SEQ:
        if (shift() && !seq_segmode) {          /* GLO + SELECT: the key's ROOT (as GLO + ALGORITHM its scale): IN KEY
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
    case 1:
        if (!r_stages) return 0;
        rst_cur = (uint8_t)step_mod(rst_cur % 6u, e, 6u);
        pat.r_pos = (uint16_t)(rst_cur * 256u);
        dlist_at = 0;
        return 1;
    case 2: dband = step_mod(dband, e, 5u); dlist_at = 0; return 0;   /* (the band's frame moves: no list) */
    case 4: if (dmod < SD_MODS) { dmod = step_mod(dmod, e, SD_MODS); dslot = dmod; } return 0;
    default: return 0;
    }
}

/* The keys on SEQ, REC off, with GLO (2026-11-17; without it they play): the black keys B1 RND, B2 UNDO, B3 CLR,
 * B4 MORE, B10 SEG (SEGMENTS: the white keys pick the part, B10 again: back), B11 KEYS (REC); SELECT picks the step.
 * The key LEDs show it (keys_lit). */
#define BK_MORE 3                               /* B4 (C#4): on SEQ the step's second layer, RATCHET and PROB */
#define BK_SEG 9                                /* the black key before the last (D#5, B10): SEG; ORBIT's SETUP */
#define BK_KEYS 10                              /* the last black key (F#5, B11): on SEQ the keys play into the steps
                                                   (REC) and back */
static void rec_toggle(void)                    /* REC (or B11 on SEQ): recording into the track SEQ edits, on or off */
{
    seq_rec = !seq_rec;
    rec_at = 0xFFu;
    if (seq_rec && seq_trk && set.mode == SD_MODE_MONO) msg("B RECORDS, PLAYS IN DUO/POLY");
}
static uint32_t seq_tool_keys(void)             /* the black keys with a job on SEQ (with GLO): B1 B2 B3 B4 B10 B11 */
{
    uint32_t k, m = 0;
    for (k = 0; k < 27u; k++) {
        int32_t bk = key_black(k);
        if (bk == 0 || bk == 1 || bk == 2 || bk == BK_MORE || bk == BK_SEG || bk == BK_KEYS) m |= 1u << k;
    }
    return m;
}
static void seq_keys(uint32_t down)
{
    uint32_t k;
    for (k = 0; k < 27u; k++) {
        int32_t w, bk;
        if (!(down >> k & 1u)) continue;
        w = key_white(k);
        bk = key_black(k);
        if (bk == BK_KEYS && !seq_segmode) {
            rec_toggle();                       /* the keys play into the steps now (B11 again: back) */
        } else if (bk == BK_SEG) {
            seq_segmode = !seq_segmode;
            if (seq_segmode) seq_dseg = sd_seg_of(&seqp, seq_cur);   /* (the part of the step that was picked) */
            seq_more = 0;
        } else if (bk == BK_MORE && !seq_segmode) {
            seq_more = !seq_more;               /* MORE: the step's RATCHET and PROB (it stays while steps are picked) */
        } else if (w >= 0) {
            uint32_t i = seq_bank * 16u + (uint32_t)w;
            if (i >= seqp.len) continue;
            if (seq_segmode) seq_dseg = sd_seg_of(&seqp, i);   /* SEGMENTS: the part its step is in, picked */
            seq_cur = i;
            if (!seq_segmode) aud_step(i, k);   /* (stopped: heard while the key is held) */
        } else if (seq_segmode) {
            /* (SEGMENTS: the black keys rest, but D#5, the way back) */
        } else if (bk == 0) {                   /* RND: a new line in the scale, now */
            seq_keep_undo();
            sd_pattern_randomize(&seqp, &seq_rng);
            seq_push();
        } else if (bk == 1 && seq_has_undo) {   /* UNDO: the pattern before the last rewrite (and back) */
            SdPattern t = seqp;
            seqp = seq_undo;
            seq_undo = t;
            seq_push();
        } else if (bk == 2) {                   /* CLR, pressed twice (or then OCT+): every step a rest */
            if (seq_clr_at && fm1_ms - seq_clr_at < 3000u) {
                seq_clear();
            } else {
                seq_clr_at = fm1_ms;
            }
        }
    }
    if (seq_clr_at && fm1_ms - seq_clr_at >= 3000u) seq_clr_at = 0;
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

/* The encoders on ORBIT (every one but PRESETS): the step's values (GLO: its macros), or SETUP's (GLO: GLIDE);
 * ALGORITHM the step */
static void orb_knobs(void)
{
    static const uint8_t KN[4] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
    uint32_t s = orb_cur % SD_ORB_STEPS, k, changed = 0, l2 = shift();
    int32_t e;
    if ((e = enc_take(ENC_ALGORITHM)) != 0) {
        orb_cur = step_lim(orb_cur, e, SD_ORB_LEN(&orbp));
        if (!orb_setup) orb_hold_step(orb_cur);   /* (the step picked: heard) */
    }
    for (k = 0; k < 4u; k++) {
        if ((e = enc_take(KN[k])) == 0) continue;
        changed = 1;
        if (!orb_setup || l2 || k < 2u) dlist_at = 0;   /* (another knob: the destinations' list closes) */
        if (orb_setup && l2) {                  /* GLIDE: a turn right on, left off */
            static const uint8_t GB[4] = {SD_OG_L1, SD_OG_L2, SD_OG_MAC, 0};
            if (e > 0) orbp.glide |= GB[k]; else orbp.glide &= (uint8_t)~GB[k];
        } else if (orb_setup) {
            if (k == 0u) orbp.len = (uint8_t)(2u + step_lim(SD_ORB_LEN(&orbp) - 2u, e, 7u));
            else if (k == 1u) orbp.rate = (uint8_t)step_lim(orbp.rate > 2u ? 2u : orbp.rate, e, 3u);
            else { orbp.dst[k - 2u] = (uint8_t)dst_step(orbp.dst[k - 2u], e, 0); dlist_open(k == 2u ? DL_ORB1 : DL_ORB2); }
            if (orb_cur >= SD_ORB_LEN(&orbp)) orb_cur = SD_ORB_LEN(&orbp) - 1u;
        } else if (l2) {                        /* the step's macros: -99..99 */
            int32_t v = orbp.mac[k][s] + enc_far(e, 198u);
            orb_hold_step(s);                   /* (the step edited: heard) */
            orbp.mac[k][s] = (int8_t)(v < -99 ? -99 : v > 99 ? 99 : v);
        } else if (k < 2u) {                    /* XPOSE A, B: -24..24 */
            int8_t *x = k ? &orbp.xb[s] : &orbp.xa[s];
            orb_hold_step(s);
            int32_t v = *x + e;
            *x = (int8_t)(v < -24 ? -24 : v > 24 ? 24 : v);
        } else {                                /* the lanes: -100..100 % */
            int32_t v = orbp.lane[k - 2u][s] + enc_far(e, 200u);
            orb_hold_step(s);
            orbp.lane[k - 2u][s] = (int8_t)(v < -100 ? -100 : v > 100 ? 100 : v);
        }
    }
    if (changed) orb_push();
}

/* The encoders on SEQ (every one but PRESETS). GLO held: K1 LENGTH, K2 DIV, K3 SWING, K4 XPOSE
 * (the track's transpose, taken at its next one, as GLO + a key), ALGORITHM the SCALE */
static void seq_knobs(void)
{
    int32_t e;
    SdStep *st = &seqp.step[seq_cur];
    uint32_t changed = 0;
    if (shift() && !seq_segmode) {
        if ((e = enc_take(ENC_KNOB1)) != 0) seq_len(e), changed = 1;
        if ((e = enc_take(ENC_KNOB2)) != 0) { seqp.div = (uint8_t)sdiv_step(seqp.div % SD_SDIVS, e); changed = 1; }
        if ((e = enc_take(ENC_KNOB3)) != 0) {
            int32_t w = (int32_t)seqp.swing + e;
            seqp.swing = (uint8_t)(w < 50 ? 50 : w > 75 ? 75 : w);
            changed = 1;
        }
        if ((e = enc_take(ENC_KNOB4)) != 0) {
            int32_t x = eng.t[seq_trk].xpose_next + e;
            eng.t[seq_trk].xpose_next = (int8_t)(x < -24 ? -24 : x > 24 ? 24 : x);
        }
        if ((e = enc_take(ENC_ALGORITHM)) != 0) {   /* the scale: IN KEY the master's only, set on its page */
            if (!set.xchrom && key_trk() != seq_trk) msg(key_trk() ? "THE KEY IS SEQ B'S" : "THE KEY IS SEQ A'S");
            else seqp.scale = (uint8_t)step_mod(seqp.scale % SD_SCALES, e, SD_SCALES), changed = 1;
        }
    } else if (seq_segmode) {
        uint8_t bd[SD_SEGS + 1u];
        uint32_t sc = sd_seg_bounds(&seqp, bd), g = seq_dseg < sc ? seq_dseg : sc - 1u;
        if ((e = enc_take(ENC_KNOB1)) != 0) {
            int32_t r = (int32_t)(seqp.seg_rep[g] < 1u ? 1u : seqp.seg_rep[g]) + e;
            seqp.seg_rep[g] = (uint8_t)(r < 1 ? 1 : r > 4 ? 4 : r);
            changed = 1;
        }
        if ((e = enc_take(ENC_KNOB2)) != 0) seqp.seg_order[g] = (uint8_t)step_mod(seqp.seg_order[g] % SD_SORDERS, e, SD_SORDERS), changed = 1;
        if ((e = enc_take(ENC_KNOB3)) != 0) {
            seqp.seg_skip = (uint8_t)(e > 0 ? seqp.seg_skip | 1u << g : seqp.seg_skip & ~(1u << g));
            changed = 1;
        }
        if ((e = enc_take(ENC_KNOB4)) != 0) {
            int32_t l = (int32_t)(bd[g + 1u] - bd[g]) + e;
            sd_seg_set_len(&seqp, g, (uint32_t)(l < 1 ? 1 : l));
            changed = 1;
        }
        if ((e = enc_take(ENC_ALGORITHM)) != 0) {   /* ALGORITHM: how many parts, even (UNDO takes it back) */
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
    } else if (seq_more) {                      /* RATCHET 1..3, PROB in 5 % steps */
        if ((e = enc_take(ENC_KNOB1)) != 0) {
            int32_t r = (int32_t)(st->ratchet < 1u ? 1u : st->ratchet) + e;
            st->ratchet = (uint8_t)(r < 1 ? 1 : r > (int32_t)SD_RATCHET_MAX ? (int32_t)SD_RATCHET_MAX : r);
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
        if ((e = enc_take(ENC_ALGORITHM)) != 0) {
            seq_cur = step_mod(seq_cur, e, seqp.len);
            seq_bank = seq_cur / 16u;
            aud_step(seq_cur, 0xFFu);
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
            st->gate = (uint8_t)(g < 1 ? 1 : g > 100 ? 100 : g);
            changed = 1;
        }
        if ((e = enc_take(ENC_KNOB4)) != 0) st->type = (uint8_t)step_mod(st->type % SD_STYPES, e, SD_STYPES), changed = 1;
        if ((e = enc_take(ENC_ALGORITHM)) != 0) {
            seq_cur = step_mod(seq_cur, e, seqp.len);
            seq_bank = seq_cur / 16u;
            aud_step(seq_cur, 0xFFu);
        }
    }
    if (changed) seq_push();
}

/* ---- SAVE: the patch (or, on SEQ, the pattern) to a user place; SAVE again writes it once the sound is quiet */
static void draw_save(void)                     /* in the picture's place (the frame began the canvas) */
{
    char s[32], *p;
    uint32_t pt = save_mode ? save_mode == 2u : save_kind == 2u;
    cv_frame(2, 0, 236, H_PIC, C_RED);
    if (!save_mode) {
        p = put(s, save_pending ? "SAVING ..." : "SAVED TO ");
        if (!save_pending) { *p++ = pt ? 'P' : 'U'; if (save_to < 9u) *p++ = '0'; p = put_u(p, save_to + 1u); }
        *p = 0;
        cv_text(10, 44, &FONT_S, s, C_RED);
        return;
    }
    cv_text(10, 22, &FONT_S, pt ? "SAVE PATTERN" : "SAVE PATCH", C_RED);
    p = put(s, "TO ");
    *p++ = pt ? 'P' : 'U';
    if (save_to < 9u) *p++ = '0';
    p = put_u(p, save_to + 1u);
    p = put(p, "  ");
    if (pt) p = put(p, useq.used >> save_to & 1u ? "(REPLACE)" : "EMPTY");
    else p = put(p, upat.used >> save_to & 1u ? patch_name(SD_PATCHES + save_to) : "EMPTY");
    *p = 0;
    cv_text(10, 42, &FONT_S, s, C_ICE);
    cv_text(10, 66, &FONT_S, "ALGO PICKS  OCT+ YES  OCT- NO", C_ICE_D);
}
static void save_begin(void)
{
    uint32_t k;
    save_mode = page == PAGE_SEQ || page == PAGE_ORB ? 2u : 1u;
    save_at = fm1_ms;
    if (save_mode == 2u) { save_to = set.pattern; return; }
    if (set.patch >= SD_PATCHES) { save_to = set.patch - SD_PATCHES; return; }
    for (k = 0; k < UPATCHES && (upat.used >> k & 1u); k++) {}
    save_to = k < UPATCHES ? k : 0u;            /* the first empty place */
}
static void save_commit(void)
{
    uint32_t k;
    if (save_mode == 2u) {
        useq.p[save_to] = seqp;
        useq.used |= 1u << save_to;
        if (!seq_trk) { uorb.o[save_to] = orbp; uorb.used |= 1u << save_to; }   /* (A's: its ORBIT too) */
        set.pattern = (uint8_t)save_to;
    } else {
        const char *n = patch_name(set.patch);
        upat.p[save_to] = pat;
        for (k = 0; k < 8u; k++) upat.name[save_to][k] = *n ? *n++ : 0;
        upat.used |= 1u << save_to;
        set.patch = (uint8_t)(SD_PATCHES + save_to);
    }
    save_pending = save_kind = save_mode;
    save_at = fm1_ms;
    save_mode = 0;
    eng.seq_run = 0;                            /* an erase stalls the sound: it waits for quiet */
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
        int32_t v = pat.lfo_shape[k] == SD_LS_SH ? ((int32_t)((ph >> 26) * 2654435761u >> 17) - 32768)   /* steps */
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
    return pat.f_type != SD_FT_OFF;
}
static uint32_t page_fx(void)
{
    return page == 1u ? SD_FX_REACTOR : page == 2u ? SD_FX_DISTROY : page == 3u ? SD_FX_SPACE : page == PAGE_TIME ? SD_FX_TIME :
           page == PAGE_FILT ? SD_FX_FILTER : 0u;
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
enum { SW_NONE, SW_OFF, SW_ON, SW_LOCK, SW_WAIT, SW_LOOP };   /* (WAIT: on, in at A's one; LOOP: COURSE's, on) */
static uint32_t sw_of(const char **what)        /* this page's switch, and its name on the screen */
{
    uint32_t b = page_fx();
    *what = "EDIT";
    if (b) return fx_on(b) ? SW_ON : SW_OFF;
    if (page == 0u) { *what = "EDIT COURSE"; return pat.course == SD_COURSE_LOOP ? SW_LOOP : pat.course ? SW_ON : SW_OFF; }
    if (page == PAGE_ARP) return eng.arp_on ? SW_ON : SW_OFF;
    if (page == PAGE_LOOP) return lp_arm ? SW_ON : SW_OFF;   /* (armed: EDIT) */
    if (page == PAGE_ORB) return !orbp.on ? SW_OFF : eng.seq_run && !eng.orb_live ? SW_WAIT : SW_ON;
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
        }
        return 1;
    }
    if (page == 0u) {                           /* COURSE: OFF, ON (from each note), LOOP (round on its own), OFF */
        pat.course = (uint8_t)((pat.course + 1u) % 3u);
        msg(pat.course == SD_COURSE_LOOP ? "COURSE: ROUND AND ROUND" : pat.course ? "COURSE: FROM EACH NOTE" : "COURSE OFF");
        return 1;
    }
    if (page == 4u && dmod == DMOD_TUR) {       /* RADAR: on the first time to XM, 30 %, CHANGE 25 */
        pat.tur_set ^= 0x80u;
        if (SD_TUR_ON(&pat) && !pat.tur_dst) {
            pat.tur_dst = SD_MD_XM;
            if (!pat.tur_amt) pat.tur_amt = 30;
            if (!(pat.tur_set & 127u)) pat.tur_set |= 25u;
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

/* ---- the top line: the button's pages */
static const char *const NAV_HOME[3] = {"MACROS", "NAV", "SETUP"}, *const NAV_FX[4] = {"REACTOR", "DISTROY", "SPACE", "TIME"},
                         *const NAV_ENV[2] = {"ENV", "FILTER"}, *const NAV_LFO[4] = {"LFO 1", "LFO 2", "RADAR", "MOD"},
                         *const NAV_ARP[2] = {"ARP", "LOOPER"}, *const NAV_SEQ[3] = {"SEQ A", "SEQ B", "ORBIT"};
static uint32_t nav_of(const char *const **n, uint32_t *cur)
{
    switch (page) {
    case 0: case 5: case PAGE_SETUP: *n = NAV_HOME; *cur = page == 0u ? 1u : page == PAGE_SETUP ? 2u : 0u; return 3;
    case 1: case 2: case 3: case PAGE_TIME: *n = NAV_FX; *cur = page == PAGE_TIME ? 3u : page - 1u; return 4;
    case PAGE_ENV: case PAGE_FILT: *n = NAV_ENV; *cur = page == PAGE_FILT; return 2;
    case 4: *n = NAV_LFO; *cur = dmod >= SD_MODS ? dmod - SD_MODS : 3u; return 4;
    case PAGE_ARP: case PAGE_LOOP: *n = NAV_ARP; *cur = page == PAGE_LOOP; return 2;
    default: *n = NAV_SEQ; *cur = page == PAGE_ORB ? 2u : seq_trk; return 3;
    }
}
static void draw_nav(void)
{
    const char *const *n, *what;
    uint32_t cur, cnt = nav_of(&n, &cur), k, sw = sw_of(&what);
    const char *t;
    t = sw == SW_ON || sw == SW_WAIT ? "ON" : sw == SW_LOOP ? "LOOP" :
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
            cv_text(x, 1, &FONT_S, nm[k], (seq_page() || page == PAGE_ORB) && k == 1u && set.mode == SD_MODE_MONO ? C_GRID : C_ICE_D);
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
enum { KN_TXT, KN_UNI, KN_BI };
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

static void env_knobs(const uint8_t *ev)        /* an envelope: A D S R (0..127) */
{
    char t[12];
    *put_ms(t, ev[0]) = 0; kx(0, "ATTACK", t, ev[0] * 99u / 127u);
    *put_ms(t, ev[1]) = 0; kx(1, "DECAY", t, ev[1] * 99u / 127u);
    *put_u(t, ev[2] * 100u / 127u) = 0; kx(2, "SUS", t, ev[2] * 99u / 127u);
    *put_ms(t, ev[3]) = 0; kx(3, "RELEASE", t, ev[3] * 99u / 127u);
}
static char *put_xp(char *p, int32_t x)         /* a transpose: +5, and IN KEY "+5 KEY" (up a fourth in A's key) */
{
    p = put_sgn(p, x);
    return x && !set.xchrom ? put(p, " KEY") : p;
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
static uint32_t has_l2(void)                    /* GLO gives this page a second layer */
{
    return page != 5u && page != 4u && page != PAGE_ARP && page != PAGE_TIME &&
           !(page == PAGE_SEQ && seq_segmode);   /* (TIME, LOOPER: GLO only with a button or ALGORITHM there) */
}
static void knobs_of(void)
{
    char t[16], *p;
    uint32_t l2 = shift() && has_l2();
    switch (page) {
    case 0:
        if (!l2) { ku(0, "XM", q99(pat.xm)); ku(1, "WARP", q99(pat.warp)); ku(2, "FOLD", q99(pat.fold)); ku(3, "DETUNE", q99(pat.dt)); }
        else {
            ku(0, "X", q99(pat.x)); ku(1, "Y", q99(pat.y)); ku(2, "WIDTH", q99(pat.width));
            kt(3, "GEN", gen < SD_GENS ? sd_gen_name(gen) : "TURN");
        }
        break;
    case PAGE_SETUP:                            /* the FM-1's: TEMPO (GLO: SYNC) XPOSE MASTER COLOUR */
        if (l2) { kt(0, "SYNC", set.sync_int ? "INT" : "AUTO"); k0(1); k0(2); k0(3); break; }
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
    case 5:
        ku(0, "MUTATE", q99(pat.mac[0])); ku(1, "MOVE", q99(pat.mac[1])); ku(2, "BREAK", q99(pat.mac[2]));
        ku(3, M4_NAME[pat.mac4 % SD_M4S], q99(pat.mac[3]));
        break;
    case 1:
        if (r_stages) {                         /* STAGES: the stage ALGORITHM (or a white key) picks */
            const SdReactorStage *st = &pat.stage[rst_cur % 6u];
            uint32_t k;
            if (!l2) {
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
        if (!l2) {
            uint32_t pos = pat.r_pos;
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
        if (dband == 4u && !l2) {               /* LOOP: FDBK TIME SPREAD TAME */
            uint32_t tm = ((44u + ((pat.d_ftime * 838u) >> 16)) * 10u + 220u) / 441u;
            ku(0, "FDBK", q99(pat.d_fb));
            p = put_u(t, tm); p = put(p, "ms"); *p = 0; kx(1, "TIME", t, q99(pat.d_ftime));
            ku(2, "SPREAD", q99(pat.d_spread));
            if (pat.d_tame) ku(3, "TAME", q99(pat.d_tame)); else kx(3, "TAME", "OFF", 0);
        } else if (dband == 4u) {               /* LOOP, GLO: the whole of DISTROY: MIX, XOVER */
            ku(0, "MIX", q99(pat.d_mix));
            kt(1, "XOVER", pat.d_xover == SD_DX_LR4 ? "LR4" : "CMP");
            k0(2); k0(3);
        } else if (!l2) {                       /* a band: TYPE DRIVE, ALT by what it does, LEVEL */
            uint32_t k = dband, ty = pat.d_type[k] % SD_DSHAPES;
            kt(0, "TYPE", SHAPE_NAME[ty]);
            p = put(t, "+"); p = put_u(p, pat.d_gain[k] * 36u / 65535u); p = put(p, "dB"); *p = 0;
            kx(1, "DRIVE", t, q99(pat.d_gain[k]));
            if (ty) ku(2, ALT_NAME[ty], q99(pat.d_alt[k])); else k0(2);
            ku(3, "LEVEL", q99(pat.d_level[k]));
        } else {                                /* GLO: BIAS PAN DYNAMIC (PHAT .. SHARP) SPLIT */
            uint32_t k = dband;
            int32_t sv = pat.d_shape[k];
            *put_sgn(t, pat.d_bias[k] / 655) = 0; kb(0, "BIAS", t, pat.d_bias[k] / 331);
            *put_sgn(t, pat.d_pan[k] / 328) = 0; kb(1, "PAN", t, pat.d_pan[k] / 331);
            if (!sv) *put(t, "OFF") = 0;
            else { p = put(t, sv < 0 ? "PHAT " : "SHARP"); *put_u(p, q99((uint32_t)(sv < 0 ? -sv : sv) * 2u)) = 0; }
            kb(2, "DYNAMIC", t, sv / 331);
            *put_hz(t, hz_of(pat.d_split[k < 3u ? k : 2u])) = 0; kt(3, "SPLIT", t);
        }
        break;
    case 3:                                     /* SPACE: the plate (GLO: SHIMMER) */
        if (!l2) {
            ku(0, "SIZE", q99(pat.s_size)); ku(1, "DECAY", q99(pat.s_decay)); ku(2, "TONE", q99(pat.s_rtone));
            ku(3, "R-LEVEL", q99(pat.s_rlevel));
        } else {
            ku(0, "SHIMMER", SD_SP_SHIM(&pat) * 99u / 63u);
            if (sp_pitch) *put_sgn(t, sp_pitch) = 0; else *put(t, "0") = 0;
            kt(1, "PITCH", t);                  /* (the frozen tail's: while FREEZE holds it) */
            k0(2); k0(3);
        }
        break;
    case PAGE_TIME:                             /* TIME: the delay */
        *put_time(t) = 0; kt(0, "TIME", t);
        kn[0].hot = sp_fold(0) != 0u;           /* (red: longer than the line, it plays shorter: the hint says) */
        ku(1, "FDBK", q99(pat.s_fb)); ku(2, "TONE", q99(pat.s_dtone)); ku(3, "D-LEVEL", q99(pat.s_dlevel));
        break;
    case PAGE_LOOP:                             /* LOOPER: GLIDE LENGTH SPEED L-LEVEL; GLO: FILTER R-SEND D-SEND */
        if (l2) {
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
            *put_glide(t) = 0; kt(0, "GLIDE", t);
            *put_len(t) = 0; kt(1, "LENGTH", t);
            kn[1].hot = lp_fold(0) != 0u;       /* (red: longer than its memory) */
            p = t; if (v) *p++ = v < 0 ? '-' : '+';   /* (semitones) */
            *put_u(p, ((uint32_t)a + 128u) >> 8) = 0;
            kb(2, "SPEED", t, v * 99 / 3072);
            ku(3, "L-LEVEL", q99(lp_lvl));
        }
        break;
    case PAGE_ENV:
        env_knobs(l2 ? &pat.e2_a : &pat.e1_a);
        break;
    case PAGE_FILT:
        if (l2) { env_knobs(&pat.e3_a); break; }
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
        if (dmod == DMOD_TUR) {                 /* RADAR: CHANGE LENGTH DEST AMOUNT */
            p = put_u(t, SD_TUR_CHANGE(&pat)); *p++ = '%'; *p = 0; kx(0, "CHANGE", t, SD_TUR_CHANGE(&pat) * 99u / 100u);
            *put_u(t, SD_TUR_LEN(&pat)) = 0; kx(1, "LENGTH", t, (SD_TUR_LEN(&pat) - 2u) * 99u / 14u);
            kt(2, "DEST", tdest_name(pat.tur_dst));
            p = put_sgn(t, pat.tur_amt); *p++ = '%'; *p = 0; kb(3, "AMOUNT", t, pat.tur_amt * 99 / 100);
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
            kt(1, "DEST", DST_NAME[m->dst % SD_MDSTS]);
            *put_sgn(t, a) = 0; kb(2, "AMOUNT", t, a);
            k0(3);
        }
        break;
    case PAGE_ARP:
        kt(0, "MODE", AMODE_NAME[set.amode % SD_AMODES]);
        kt(1, "RATE", SDIV_NAME[set.adiv % SD_SDIVS]);
        *put_u(t, set.aoct) = 0; kx(2, "OCTAVES", t, (set.aoct - 1u) * 33u);
        p = put_u(t, set.agate); *p++ = '%'; *p = 0; kx(3, "GATE", t, set.agate * 99u / 100u);
        break;
    case PAGE_ORB: {
        uint32_t s = orb_cur % SD_ORB_STEPS, k;
        if (orb_setup && l2) {                  /* SETUP, GLO: GLIDE for lane 1, lane 2, the macros */
            kt(0, "GLIDE 1", orbp.glide & SD_OG_L1 ? "GLIDE" : "JUMP");
            kt(1, "GLIDE 2", orbp.glide & SD_OG_L2 ? "GLIDE" : "JUMP");
            kt(2, "GLIDE M", orbp.glide & SD_OG_MAC ? "GLIDE" : "JUMP");
            k0(3);
        } else if (orb_setup) {                 /* SETUP: LENGTH RATE DEST 1 DEST 2 */
            static const char *const RN[3] = {"1 LOOP", "2 LOOPS", "4 LOOPS"};
            *put_u(t, SD_ORB_LEN(&orbp)) = 0; kx(0, "LENGTH", t, (SD_ORB_LEN(&orbp) - 2u) * 99u / 6u);
            kt(1, "RATE", RN[orbp.rate > 2u ? 2u : orbp.rate]);
            kt(2, "DEST 1", DST_NAME[orbp.dst[0] % SD_MDSTS]);
            kt(3, "DEST 2", DST_NAME[orbp.dst[1] % SD_MDSTS]);
        } else if (l2) {                        /* the step's macros */
            static const char *const MN[4] = {"MUTATE", "MOVE", "BREAK", "MACRO 4"};
            for (k = 0; k < 4u; k++) { *put_sgn(t, orbp.mac[k][s]) = 0; kb(k, MN[k], t, orbp.mac[k][s]); }
        } else {                                /* the step: XPOS A, XPOS B, the lanes */
            *put_xp(t, orbp.xa[s]) = 0; kb(0, "XPOS A", t, orbp.xa[s] * 99 / 24);
            *put_xp(t, orbp.xb[s]) = 0; kb(1, "XPOS B", t, orbp.xb[s] * 99 / 24);
            for (k = 0; k < 2u; k++) {
                p = put_sgn(t, orbp.lane[k][s]); *p++ = '%'; *p = 0;
                kb(2u + k, orbp.dst[k] ? DST_NAME[orbp.dst[k] % SD_MDSTS] : k ? "LANE 2" : "LANE 1", t, orbp.lane[k][s] * 99 / 100);
            }
        }
        break;
    }
    default:                                    /* SEQ A, B */
        if (seq_segmode) {
            uint8_t bd[SD_SEGS + 1u];
            uint32_t sc = sd_seg_bounds(&seqp, bd), g = seq_dseg < sc ? seq_dseg : sc - 1u;
            uint32_t r = seqp.seg_rep[g] < 1u ? 1u : seqp.seg_rep[g];
            p = put(t, "x"); *put_u(p, r) = 0; kx(0, "REPEAT", t, (r - 1u) * 33u);
            kt(1, "ORDER", SORDER_NAME[seqp.seg_order[g] % SD_SORDERS]);
            kt(2, "SKIP", (seqp.seg_skip >> g & 1u) ? "SKIP" : "PLAY");
            *put_u(t, bd[g + 1u] - bd[g]) = 0; kt(3, "LENGTH", t);
        } else if (l2) {
            seq_l2_knobs();
        } else {
            const SdStep *st = &seqp.step[seq_cur];
            if (seq_more) {
                p = put(t, "x"); *put_u(p, st->ratchet < 1u ? 1u : st->ratchet) = 0; kt(0, "RATCHET", t);
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
                p = put_u(t, st->gate); *p++ = '%'; *p = 0; kx(2, "GATE", t, st->gate * 99u / 100u);
                kt(3, "TYPE", STYPE_NAME[st->type % SD_STYPES]);
            }
        }
        break;
    }
}
static uint32_t page_off(void);
static void draw_knobs(void)
{
    uint32_t i, l2 = shift() && has_l2(), off = page_off();
    uint16_t cn = off ? C_OFF : l2 ? C_RED : C_ICE_D, cv = off ? C_ICE_D : C_ICE, cb = off ? C_OFF : C_ICE;
    knobs_of();
    cv_begin(240, H_KNOB, C_BG);
    for (i = 0; i < 4u; i++) {
        const Kn *k = &kn[i];
        int32_t x = (int32_t)i * 60, cx = x + 30;
        if (i) cv_line(x, 18, x, (int32_t)H_KNOB - 4, C_GRID);   /* (under the names: seven letters fit) */
        if (!k->nm) continue;                  /* (no knob there: nothing) */
        cv_text(cx - text_w(&FONT_S, k->nm) / 2, 0, &FONT_S, k->nm, cn);
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

static void pic_chain(void)                     /* MACROS: the chain, each engine ON or OFF (EDIT on its page) */
{
    static const char *const EN[5] = {"FILT", "REAC", "DIST", "TIME", "SPAC"};   /* (as FX's tabs) */
    uint32_t k, on[5];
    on[0] = fx_on(SD_FX_FILTER); on[1] = fx_on(SD_FX_REACTOR); on[2] = fx_on(SD_FX_DISTROY); on[3] = fx_on(SD_FX_TIME);
    on[4] = fx_on(SD_FX_SPACE);
    for (k = 0; k < 5u; k++) {
        int32_t x = 4 + (int32_t)k * 47;
        cv_text(x, 30, &FONT_S, EN[k], on[k] ? C_ICE : C_ICE_D);
        cv_box(x, 52, 40, 6, on[k] ? C_RED : C_GRID);
        cv_text(x, 64, &FONT_S, on[k] ? "ON" : "OFF", on[k] ? C_RED : C_ICE_D);
        if (k) { cv_line(x - 6, 50, x - 3, 55, C_ICE_D); cv_line(x - 3, 55, x - 6, 60, C_ICE_D); }   /* > */
    }
}

static void pic_arrow(int32_t x, int32_t y0, int32_t y1, uint16_t c)   /* a line down (or up) with its head */
{
    int32_t d = y1 > y0 ? -1 : 1;
    cv_line(x, y0, x, y1, c);
    cv_line(x - 3, y1 + 3 * d, x, y1, c); cv_line(x + 3, y1 + 3 * d, x, y1, c);
}
static void pic_setup(void)                     /* SETUP: each setting drawn under its knob: SYNC the MIDI clock into the
                                                   tempo (red: one leads now) or cut off; XPOSE an octave's keys, the
                                                   master's scale lit (its root full), SEMITONES all; MASTER the track
                                                   whose one leads, underlined as on SEQ's top line, to the other */
{
    static const uint8_t WK[7] = {0, 2, 4, 5, 7, 9, 11}, BK[5] = {1, 3, 6, 8, 10}, BX[5] = {1, 2, 4, 5, 6};
    const SdPattern *kp = key_pat();
    uint32_t k, midi = !set.sync_int, lead = midi && eng.clk_on, root = sd_pat_key(kp) % 12u,
             m = set.xchrom ? 0xFFFu : SD_SCALE_MASK[kp->scale % SD_SCALES];
    uint16_t cm = !midi ? C_GRID : lead ? C_RED : C_ICE_D;
    for (k = 1; k < 4u; k++) cv_line((int32_t)k * 60, 6, (int32_t)k * 60, 92, C_GRID);   /* (the knobs' columns) */
    cv_frame(10, 8, 40, 18, cm);                /* SYNC */
    cv_text(14, 9, &FONT_S, "MIDI", midi ? C_ICE : C_ICE_D);
    if (midi) pic_arrow(30, 28, 62, cm);
    else { cv_line(30, 28, 30, 38, C_GRID); cv_line(30, 52, 30, 62, C_GRID); cv_line(26, 41, 34, 49, C_ICE_D); cv_line(34, 41, 26, 49, C_ICE_D); }
    cv_frame(10, 66, 40, 18, C_ICE_D);
    cv_text(18, 67, &FONT_S, "BPM", C_ICE);
    for (k = 0; k < 7u; k++) {                  /* XPOSE: the white keys, then the black over them */
        uint32_t st = WK[k], in = (m >> ((st + 12u - root) % 12u)) & 1u;
        int32_t x = 62 + (int32_t)k * 8;
        cv_frame(x, 18, 9, 52, C_ICE_D);
        if (in) cv_rect(x + 1, 19, 7, 50, st == root && !set.xchrom ? C_RED : C_RED_D);
    }
    for (k = 0; k < 5u; k++) {
        uint32_t st = BK[k], in = (m >> ((st + 12u - root) % 12u)) & 1u;
        int32_t x = 62 + (int32_t)BX[k] * 8 - 2;
        cv_rect(x - 1, 18, 7, 31, C_BG);
        cv_rect(x, 18, 5, 30, in ? (st == root && !set.xchrom ? C_RED : C_RED_D) : C_ICE_D);
    }
    for (k = 0; k < 2u; k++) {                  /* MASTER */
        int32_t y = k ? 66 : 8;
        cv_text(130, y + 1, &FONT_S, k ? "SEQ B" : "SEQ A", k == set.master ? C_ICE : C_ICE_D);
        if (k == set.master) cv_rect(130, y + 16, 40, 1, C_RED);
    }
    if (set.master) pic_arrow(150, 62, 28, C_RED); else pic_arrow(150, 28, 62, C_RED);
    cv_text(156, 37, &FONT_S, "1", C_RED);
    {                                           /* the CPU, under COLOUR's column: the worst audio interrupt of the
                                                   last second (red from half, Dinghy's rule); a gauge as TIME's */
        char s[8], *p = s;
        uint32_t pm = cpu_permille, h;
        cv_text(192, 9, &FONT_S, "CPU", C_ICE_D);
#if FM1_CPU1
        {                                       /* the cores, a lamp each (2026-11-17, the user: to see if a unit is one
                                                   of those whose second core reads RAM wrong): the first always lit; the
                                                   second lit while it runs, red once it gave up (misread, a fault,
                                                   late: the room plays the reverb till power-off), dark if it never
                                                   started */
            uint16_t c2 = fm1_cpu1_on && !eng.sp_room ? C_ICE : fm1_cpu1_gave_up ? C_RED : C_GRID;
            cv_box(220, 13, 6, 6, C_ICE);
            cv_box(229, 13, 6, 6, c2);
        }
#endif
        if (pm == 0xFFFFFFFFu || !pm) p = put(p, "--");
        else { p = put_u(p, (pm + 5u) / 10u); *p++ = '%'; }
        *p = 0;
        cv_text(210 - text_w(&FONT_S, s) / 2, 67, &FONT_S, s, pm != 0xFFFFFFFFu && pm >= 500u ? C_RED : C_ICE);
        cv_frame(204, 28, 12, 36, C_ICE_D);
        h = pm == 0xFFFFFFFFu ? 0u : (pm > 1000u ? 1000u : pm) * 34u / 1000u;
        if (h) cv_rect(205, 63 - (int32_t)h, 10, (int32_t)h, pm >= 500u ? C_RED : C_ICE_D);
        cv_line(200, 46, 203, 46, C_ICE_D);     /* (half: the rule) */
        if (batt) {                             /* the battery's volts (the icon's reading, to tell its scale) */
            uint32_t mv = batt_mv();
            p = put_u(s, mv / 1000u); *p++ = '.'; *p++ = (char)('0' + mv / 100u % 10u); *p++ = (char)('0' + mv / 10u % 10u);
            *p++ = 'V'; *p = 0;
            cv_text(210 - text_w(&FONT_S, s) / 2, 87, &FONT_S, s, C_ICE_D);
        }
    }
}

static void pic_reactor(void)                     /* the filters now as peaks across the width; under them the six
                                                   stages as a piano roll: a column each, its four filters notes at
                                                   their pitch, in turn, the one playing red */
{
    enum { X0 = 6, W = 228, SH = 52, SY = 104 - SH, Y0 = 0, H = 104 - SH - 8, SW = W / 6 };
    int32_t x, k, wpx;
    int16_t col[W];
    uint32_t i, pos = eng.r.shown_pos, at = pos >> 8, nx = at == 5u ? 0u : at + 1u, f = pos & 255u;
    uint32_t dom = f < 128u ? at : nx, sec = f ? (f < 128u ? nx : at) : 99u;
    uint16_t on = fx_on(SD_FX_REACTOR), live = on && !r_stages;   /* (live: the engine's filters now) */
    if (!react_set(&pat) && !r_stages) {        /* never set (the Reactor off in this patch): EDIT sets it */
        cv_text(X0, 34, &FONT_S, "NO STAGES SET YET", C_ICE_D);
        cv_text(X0, 54, &FONT_S, "EDIT: ON, SIX VOWELS", C_ICE_D);
        return;
    }
    if (!on || r_stages) {                      /* (off: the patch's stage at POS, not the engine's last; STAGES: the
                                                   one edited) */
        pos = r_stages ? rst_cur % 6u * 256u : (uint32_t)pat.r_pos; at = pos >> 8; nx = at == 5u ? 0u : at + 1u; f = pos & 255u;
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
                                                   their pitch (a piano roll's rows) */
        int32_t sx = X0 + (int32_t)i * SW;
        uint16_t c = i == dom ? C_RED : i == sec ? C_RED_D : C_GRID;
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
    uint32_t i, k = dband;
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
    if (k == 4u) {                              /* LOOP picked: its tag over the bands */
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
 * through. Free or stopped: no grid, LOOP takes the last LENGTH where it is pressed, a second a mark */
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
    uint32_t sync = (pat.s_mode & SD_SP_SYNC) && eng.seq_run && eng.seq_len_q8, looping = eng.sp.pon && lp_on;
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
        for (i = 0; i < LP_COLS; i++) lp_col[i] = lp_arm ? (uint8_t)lp_col_peak(&L, (lp_cw + i * spc) % n, spc) : 0u;
        lp_cw = (lp_cw + LP_COLS * spc) % n;
    }
    for (k = 0; k < 16u && (w + n - lp_cw) % n >= spc && (w + n - lp_cw) % n < n / 2u; k++) {   /* the new ones */
        for (i = 0; i + 1u < LP_COLS; i++) lp_col[i] = lp_col[i + 1u];
        lp_col[LP_COLS - 1u] = lp_arm ? (uint8_t)lp_col_peak(&L, lp_cw, spc) : 0u;
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
            uint16_t c = x >= xs && x <= xe ? (looping ? C_RED : lp_arm ? C_ICE : C_ICE_D) : xn >= 0 && x > xn ? C_ICE_D : C_GRID;
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
        cv_frame(xs, SY - 1, xe - xs + 1 > 4 ? xe - xs + 1 : 4, SH + 2, looping ? C_RED : lp_arm ? C_ICE : C_GRID);
        if (looping) {                          /* its head, through the loop */
            int32_t xh = xs + (int32_t)((uint32_t)(xe - xs) * eng.lp_at / 4096u);
            cv_rect(xh, SY - 3, 2, SH + 6, C_GLINT);
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
}

static void pic_env(void)                       /* ENV 1 (the level) and ENV 2 (the FM's amount), GLO brings 2 up */
{
    uint32_t l2 = shift();
    cv_line(6, 100, 233, 100, C_GRID);
    if (l2) { adsr(&pat.e1_a, 6, 228, 20, 80, C_ICE_D); adsr(&pat.e2_a, 6, 228, 20, 80, C_RED); }
    else { adsr(&pat.e2_a, 6, 228, 20, 80, C_RED_D); adsr(&pat.e1_a, 6, 228, 20, 80, C_ICE); }
    cv_text(6, 0, &FONT_S, "ENV 1 LEVEL", l2 ? C_ICE_D : C_ICE);
    cv_text(234 - text_w(&FONT_S, "ENV 2 FM"), 0, &FONT_S, "ENV 2 FM", l2 ? C_RED : C_RED_D);
}

static void pic_filt(void)                      /* the response; ENV 3 (GLO) on the right */
{
    enum { X0 = 6, W = 150, Y0 = 2, H = 98 };
    int32_t c = 16 * 256 + (int32_t)((uint32_t)pat.f_cut * 114u >> 8), xc = gx(c, X0, W), peak = (int32_t)q99(pat.f_res) / 4;
    int32_t slope = pat.f_type == SD_FT_LP24 ? 24 : 12, xx, y0 = Y0 + 34, prev = y0;
    uint32_t on = fx_on(SD_FX_FILTER), l2 = shift();
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

static void pic_lfo(uint32_t k)                 /* its cycle, the dot where it is; where the matrix sends it */
{
    uint32_t i, n = 0;
    draw_lfo_wave(k, 6, 4, 130, 94);
    cv_line(146, 4, 146, 98, C_GRID);
    cv_text(154, 0, &FONT_S, "TO", C_ICE_D);
    for (i = 0; i < SD_MODS && n < 5u; i++) {
        const SdModSlot *m = &pat.mod[i];
        if (m->src != SD_MS_LFO1 + k || !m->dst || !m->amt) continue;
        cv_text(154, 17 + (int32_t)n * 16, &FONT_S, DST_NAME[m->dst % SD_MDSTS], C_ICE);
        n++;
    }
    if (!n) { cv_text(154, 17, &FONT_S, "NOTHING", C_ICE_D); cv_text(154, 33, &FONT_S, "K3: DEST", C_ICE_D); }
}

/* RADAR: its loop as a scope (a starship's): LENGTH places round it, clockwise from the top, each step's value a
 * blip's distance from the middle (the dotted ring 0, outward up), the sweep where it is (red, the two before it
 * fading), the newest blip red, the older dimmer as they age; beside it a gauge of the value now; where it goes, and
 * the MOD slots it feeds */
static void tur_pt(uint32_t p, uint32_t len, int32_t r, int32_t *x, int32_t *y)   /* place p of len, radius r */
{
    uint32_t ph = p * (0xFFFFFFFFu / len);       /* (a cycle over 2^32) */
    *x = 52 + ((r * sd_sine(ph)) >> 15);
    *y = 52 - ((r * sd_sine(ph + 0x40000000u)) >> 15);
}
static void pic_tur(void)                       /* the loop as a scope: a place a step round the circle, each
                                                   held at its value's radius over its arc, a hard jump to the next
                                                   (as it sounds: a step, not a curve); the sweep where it is */
{
    enum { CX = 52, CY = 52, RO = 50, RZ = 4 + (RO - 7) / 2, M = 8 };   /* (M: an arc's pieces) */
    static int16_t pv[16];
    static uint8_t wk[16];                      /* (each place's step, its low byte: an age up to 255 is enough) */
    static uint32_t seen, have, plen;
    uint32_t c = eng.tur_count, reg = eng.tur_reg, plc = eng.tur_place, i, n, len = SD_TUR_LEN(&pat), on = SD_TUR_ON(&pat),
             cur, tn;
    int32_t x, y, x0, y0, v;
    char s[16], *p;
    if (eng.tur_count != c) { c = eng.tur_count; reg = eng.tur_reg; plc = eng.tur_place; }   /* (a step meanwhile: the
                                                   newer three) */
    if (plc >= len) plc = 0;                    /* (LENGTH shorter: the engine's next step puts it right) */
    if (len != plen) { have = 0; plen = len; }  /* (another LENGTH: the places anew) */
    n = c - seen > 8u ? 8u : c - seen;          /* (the register holds the 8 values before: older ones are gone) */
    for (i = n; i > 0u; i--) {                  /* the steps since the last frame, each into its place */
        uint32_t k = c - (i - 1u), at = (plc + 8u * len - (i - 1u)) % len;   /* (its place in the loop: the 1 at 12) */
        pv[at] = (int16_t)sd_tur_dac(reg >> (i - 1u));
        wk[at] = (uint8_t)k;
        have |= 1u << at;
    }
    seen = c;
    cur = plc;
    tn = len * M;
    for (i = 0; i < 48u; i++) {                 /* the rings: the edge, 0 (dotted), the middle's dot */
        int32_t xa, ya, xb, yb;
        tur_pt(i, 48u, RO, &xa, &ya); tur_pt(i + 1u, 48u, RO, &xb, &yb);
        cv_line(xa, ya, xb, yb, on ? C_ICE_D : C_GRID);
        if (i & 1u) { tur_pt(i, 48u, RZ, &xa, &ya); cv_pset(xa, ya, C_ICE_D); }
    }
    cv_line(CX - RO + 6, CY, CX + RO - 6, CY, C_GRID);   /* the crosshair */
    cv_line(CX, CY - RO + 6, CX, CY + RO - 6, C_GRID);
    cv_rect(CX - 1, CY - 1, 3, 3, C_ICE_D);
    cv_rect(CX - 1, CY - RO, 3, 7, on ? C_ICE : C_ICE_D);   /* the loop's 1, at 12 o'clock: a mark and its number */
    cv_text(CX + 4, CY - RO + 1, &FONT_S, "1", on ? C_ICE : C_ICE_D);
    if (on) {                                   /* the sweep: here, and the two before it fading */
        const uint16_t SW[3] = {C_FILL, C_RED_D, C_RED};
        for (i = 0; i < 3u && i < len; i++) {
            tur_pt((cur + len - 2u + i) % len, len, RO - 2, &x, &y);
            cv_line(CX, CY, x, y, SW[i]);
        }
    }
    for (i = 0; i < len; i++) {                 /* the loop: each place an arc at its radius, then the jump */
        uint32_t j = (i + 1u) % len, q, a = i * M + tn - M / 2u;
        int32_t ri, rj;
        uint16_t lc = on ? C_ICE_D : C_GRID;
        if (!(have >> i & 1u)) continue;
        ri = 4 + (pv[i] + 32767) * (RO - 7) / 65534;
        for (q = 0; q < M; q++) {
            tur_pt((a + q) % tn, tn, ri, &x0, &y0); tur_pt((a + q + 1u) % tn, tn, ri, &x, &y);
            cv_line(x0, y0, x, y, lc);
        }
        if (!(have >> j & 1u)) continue;
        rj = 4 + (pv[j] + 32767) * (RO - 7) / 65534;
        tur_pt((a + M) % tn, tn, ri, &x0, &y0); tur_pt((a + M) % tn, tn, rj, &x, &y);
        cv_line(x0, y0, x, y, lc);
    }
    for (i = 0; i < len; i++) {                 /* the blips: the newest red, the rest as old as they are */
        uint32_t age = (uint8_t)(c - wk[i]);
        if (!(have >> i & 1u)) continue;
        tur_pt(i, len, 4 + (pv[i] + 32767) * (RO - 7) / 65534, &x, &y);
        if (!on) { cv_rect(x - 1, y - 1, 3, 3, C_GRID); continue; }
        if (i == cur && !age) { cv_box(x - 2, y - 2, 5, 5, C_RED); cv_pset(x, y, C_GLINT); }
        else cv_rect(x - 1, y - 1, 3, 3, age < len / 2u ? C_ICE : C_ICE_D);
    }
    v = have >> cur & 1u ? pv[cur] : 0;         /* the gauge: the value now, from its middle */
    cv_frame(112, 6, 12, 92, on ? C_ICE_D : C_GRID);
    for (i = 0; i <= 10u; i++) cv_line(106, 8 + (int32_t)i * 88 / 10, i % 5u ? 108 : 110, 8 + (int32_t)i * 88 / 10, C_ICE_D);
    y = 52 - v * 42 / 32767;
    if (on && y != 52) cv_rect(114, y < 52 ? y : 52, 8, y < 52 ? 52 - y : y - 52, C_RED_D);
    cv_rect(113, y, 10, 2, on ? C_RED : C_ICE_D);
    cv_line(146, 4, 146, 98, C_GRID);
    cv_text(154, 0, &FONT_S, "TO", C_ICE_D);
    cv_text(154, 17, &FONT_S, on && pat.tur_dst ? tdest_name(pat.tur_dst) : "OFF", on && pat.tur_dst ? C_ICE : C_ICE_D);
    for (i = 0, n = 0; i < SD_MODS; i++)        /* and the MOD slots with RADAR as their source */
        if (pat.mod[i].src == SD_MS_TUR && pat.mod[i].dst && pat.mod[i].amt) n++;
    if (n) { p = put(s, "+ MOD x"); *put_u(p, n) = 0; cv_text(154, 33, &FONT_S, s, C_ICE_D); }
}

static void pic_mod(void)                       /* the eight slots as a panel's patch points, in the knobs' columns:
                                                   each its source over its destination (the names as on the knobs)
                                                   and its amount's bar, the picked one red, an empty one -- */
{
    uint32_t k;
    for (k = 0; k < SD_MODS; k++) {
        const SdModSlot *m = &pat.mod[k];
        int32_t x = (int32_t)(k & 3u) * 60, y = k < 4u ? 2 : 54, cx = x + 30, a = amt99(m->amt), w = a * 24 / 99;
        uint32_t use = m->src && m->dst && m->amt;
        uint16_t c = k == dmod ? C_RED : use ? C_ICE : C_ICE_D;
        const char *sn = SRC_NAME[m->src % SD_MSRCS], *dn = DST_NAME[m->dst % SD_MDSTS];
        if (k & 3u) cv_line(x, y, x, y + 44, C_GRID);
        if (!m->src && !m->dst) { cv_text(cx - 8, y + 8, &FONT_S, "--", k == dmod ? C_RED : C_GRID); continue; }
        cv_text(cx - text_w(&FONT_S, sn) / 2, y, &FONT_S, sn, c);
        cv_text(cx - text_w(&FONT_S, dn) / 2, y + 17, &FONT_S, dn, c);
        cv_line(cx - 24, y + 40, cx + 24, y + 40, C_GRID);
        cv_line(cx, y + 37, cx, y + 43, C_ICE_D);
        if (w) cv_box(w > 0 ? cx + 1 : cx + w, y + 38, w > 0 ? w : -w, 5, k == dmod ? C_RED : a < 0 ? C_ICE_D : C_ICE);
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
    uint32_t lock = seq_trk && set.mode == SD_MODE_MONO;
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
    for (k = 0; k < 16u; k++) {                 /* towers as high as the note; TIE a bridge, SLIDE a ramp, REST a dot */
        uint32_t i = seq_bank * 16u + k, seg = sd_seg_of(&seqp, i);
        const SdStep *st = &seqp.step[i];
        int32_t x = 9 + (int32_t)k * 14, base = seq_segmode ? 60 : 78, h;
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
            h = 4 + (int32_t)((nt < lo ? 0u : nt - lo) * (seq_segmode ? 24u : 40u) / (hi - lo));
        }
        switch (st->type) {
        case SD_ST_REST: cv_rect(x + 5, base - 2, 2, 2, C_ICE_D); break;
        case SD_ST_SKIP: cv_line(x + 2, base - 6, x + 9, base - 1, C_RED_D); cv_line(x + 9, base - 6, x + 2, base - 1, C_RED_D); break;
        case SD_ST_TIE: cv_rect(x - 3, base - h + 2, 15, 2, c); cv_rect(x + 2, base - h + 4, 8, h - 4, C_GRID); break;
        default:
            cv_box(x + 2, base - h, 8, h, i == play ? C_RED : dim ? C_GRID : st->vel >= 110u ? C_ICE : C_ICE_D);
            if (st->type == SD_ST_SLIDE) cv_line(x - 3, base - h + 6, x + 2, base - h, C_RED);
            if (st->type == SD_ST_NOTE && st->ratchet > 1u) {   /* RATCHET: the tower cut in its hits */
                int32_t r;
                for (r = 1; r < (int32_t)(st->ratchet > SD_RATCHET_MAX ? SD_RATCHET_MAX : st->ratchet); r++)
                    cv_rect(x + 2, base - h + h * r / (int32_t)st->ratchet, 8, 1, C_BG);
            }
            if (st->prob < 100u) cv_rect(x + 5, base - h - 4, 2, 2, C_RED);   /* PROB: a red dot over it */
            if (st->chord[0]) cv_rect(x + 1, base - h - 2, 10, 1, C_RED);   /* a chord: a red cap */
            break;
        }
        if (i == seq_cur && !seq_segmode) cv_frame(x - 1, 28, 14, 52, C_RED);
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
        cv_text(6, 86, &FONT_S, "KEYS PICK A PART  ALGO PARTS", C_ICE_D);
        return;
    }
    /* the line under: what the keys do now */
    if (seq_rec) { p = put(s, "ENV: A TIE   LFO: A REST"); lc = C_RED; }
    else if (shift()) {                         /* GLO: the black keys' jobs, left to right as on the keys, red (the
                                                   second layer; the other keys transpose: their lights) */
        static const uint8_t BK[6] = {0, 1, 2, BK_MORE, BK_SEG, BK_KEYS};
        static const char *const NM[6] = {"RND", "UNDO", "CLR", "MORE", "SEG", "KEYS"};
        int32_t gx = 6;
        for (k = 0; k < 6u; k++) gx = key_chip(gx + (k == 4u ? 4 : 0), 86, BK[k], NM[k], C_RED) + 2;   /* (230 px) */
        return;
    }
    else if (lock) { p = put(s, "B NEEDS DUO OR POLY: SEL"); lc = C_RED; }
    else if (seq_clr_at) {                      /* CLR asked: its key blinking, the answer's two */
        cv_text(key_chip(6, 86, 2u, "CLR", C_RED), 86, &FONT_S, " / OCT+: CLEAR  OCT- NO", C_RED);
        return;
    } else {                                    /* the keys play (2026-11-17); with GLO the black keys' jobs */
        p = put(s, "KEYS PLAY  GLO: KEY TOOLS");
        lc = C_ICE_D;
    }
    *p = 0;
    cv_text(6, 86, &FONT_S, s, lc);
}

/* ORBIT: its steps as columns, four lanes as rows of bars from their middle (GLO: the macros'), the step picked
 * framed red, the one playing red and its LED under the stripes lit, past LENGTH dim; a lane that glides its name red;
 * under it what plays */
static void pic_orbit(void)
{
    static const char *const ML[4] = {"MU", "MV", "BR", "M4"};
    uint32_t len = SD_ORB_LEN(&orbp), play = eng.seq_run && orbp.on ? eng.orb_shown : 0xFFu, r, s, l2 = shift();
    for (r = 0; r < 4u; r++) {
        int32_t y = 2 + (int32_t)r * 22;
        const char *nm = l2 ? ML[r] : r == 0u ? "XA" : r == 1u ? "XB" : DST_SHORT[orbp.dst[r - 2u] % SD_MDSTS];
        uint32_t gl = l2 ? (orbp.glide & SD_OG_MAC) != 0u : r >= 2u ? (orbp.glide >> (r - 2u) & 1u) : 0u;
        cv_text(4, y + 3, &FONT_S, nm, gl ? C_RED : C_ICE_D);
        for (s = 0; s < SD_ORB_STEPS; s++) {
            int32_t x = 36 + (int32_t)s * 25, v, rng, w;
            uint16_t c = s >= len ? C_GRID : s == play ? C_RED : orbp.on ? C_ICE : C_ICE_D;
            v = l2 ? orbp.mac[r][s] : r == 0u ? orbp.xa[s] : r == 1u ? orbp.xb[s] : orbp.lane[r - 2u][s];
            rng = l2 ? 50 : r < 2u ? 12 : 50;   /* (the cell's half: 12 semitones, 50 %; past it full) */
            w = v * 11 / rng;
            w = w > 11 ? 11 : w < -11 ? -11 : w;
            if (v && w > -2 && w < 2) w = v > 0 ? 2 : -2;   /* (a small change still shows) */
            cv_line(x + 11, y + 3, x + 11, y + 17, s >= len ? C_GRID : C_ICE_D);
            if (w > 0) cv_box(x + 12, y + 5, w, 11, c);
            else if (w < 0) cv_box(x + 11 + w, y + 5, -w, 11, c);
        }
    }
    cv_frame(36 + (int32_t)(orb_cur % SD_ORB_STEPS) * 25 - 1, 0, 25, 92, C_RED);
    for (s = 0; s < len && s < SD_ORB_STEPS; s++)   /* the LEDs under the stripes: the one playing lit, the rest dark
                                                       red; ORBIT off, grey */
        cv_box(36 + (int32_t)s * 25 + 8, 96, 7, 4, s == play ? C_RED : orbp.on ? C_FILL : C_GRID);
}

static void pic(void)
{
    switch (page) {
    case 0: pic_nav(); break;
    case 5: pic_chain(); break;
    case PAGE_SETUP: pic_setup(); break;
    case 1: pic_reactor(); break;
    case 2: pic_dest(); break;
    case 3: pic_space(); break;
    case PAGE_TIME: pic_time(); break;
    case PAGE_LOOP: pic_loop(); break;
    case PAGE_ENV: pic_env(); break;
    case PAGE_FILT: pic_filt(); break;
    case 4: if (dmod == DMOD_TUR) pic_tur(); else if (dmod >= SD_MODS) pic_lfo(dmod - SD_MODS); else pic_mod(); break;
    case PAGE_ARP: pic_arp(); break;
    case PAGE_ORB: pic_orbit(); break;
    default: pic_seq(); break;
    }
}

/* ---- the hint: two fixed places, as the legends under a panel's controls (labels dim, values ice): on the left
   ALGORITHM's job (where it has none, the page's other word), from the middle the second control's (GLO, a black
   key, REC, OCT). GLO held: ALGORITHM's second job on the left, its name red as the knobs'. A toggle has a lamp, as
   the top line's. A message, OFF: EDIT TURNS IT ON, a length the line cannot hold: the whole line */
#define HINT_R 126                              /* (the right place: the longest left, 2>3>4 1>4, ends at 118) */
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
static void hint_lamp(const char *v, uint32_t on, uint32_t glo)   /* ALGORITHM's toggle: its lamp, its name */
{
    hint_algo(0, glo);
    cv_box(hint_x, 3, 8, 8, on ? C_RED : C_GRID);
    if (on) cv_rect(hint_x + 2, 5, 2, 2, C_GLINT);
    cv_text(hint_x + 11, 0, &FONT_S, v, on ? C_ICE : C_ICE_D);
}
static void hint_two(const char *l, const char *v) { hint_x = HINT_R; hint(l, v); }   /* the right place */
static uint32_t page_off(void)                  /* an engine off, ARP, RADAR, the LOOPER disarmed: its knobs set it,
                                                   silent (not SEQ's pages: a part is written while it is off) */
{
    return (page_fx() && !fx_on(page_fx())) || (page == PAGE_LOOP && !lp_arm) || (page == PAGE_ARP && !eng.arp_on) ||
           (page == 4u && dmod == DMOD_TUR && !SD_TUR_ON(&pat));
}
static void hint_key(uint32_t bk, const char *v)   /* a black key's job on the hint line: its indicator, its name */
{
    hint_x = key_chip(hint_x, 0, bk, v, C_ICE) + 16;
}
static void hint_key_r(uint32_t bk, const char *v) { hint_x = HINT_R; hint_key(bk, v); }
static void draw_hint(void)
{
    static const char *const TN[4] = {"OFF", "KEY", "HARM", "KEY+H"};
    char a[16], b[16], *p;
    cv_begin(240, 16, C_BG);
    hint_x = 6;
    if (msg_txt && fm1_ms - msg_at < 1200u) { cv_text(6, 0, &FONT_S, msg_txt, C_RED); return; }   /* (then the hint
                                                   again) */
    msg_txt = 0;
    if (!shift() && !(page == 1u && r_stages) && page_off()) { hint("OFF:", "EDIT TURNS IT ON"); return; }
    switch (page) {
    case 0:                                     /* (the routing, as its list says it: names, not the roles' letters) */
        { uint32_t al = algo_of(&pat); if (al < SD_ALGOS) *put(a, ALG_ROUTE[al % 8u]) = 0; else roles_of(&pat, a); }
        p = put(b, "BANK "); *p++ = (char)('A' + ebank); *p = 0;
        if (shift()) { hint_algo(b, 1); break; }
        hint_algo(a, 0); hint_two("GLO", "BANK");
        break;
    case 5: hint_algo("MACRO 4 KIND", 0); break;
    case PAGE_SETUP:                            /* ALGORITHM: CLEAR ALL, asked (OCT+ yes, OCT- no); GLO: K1 SYNC */
        if (clr_all_at) { cv_text(6, 0, &FONT_S, "CLEAR ALL? OCT+ YES, OCT- NO", C_RED); break; }   /* (the whole line) */
        if (shift()) { hint_algo("CLEAR ALL", 1); hint_two("K1", "SYNC"); break; }
        hint_algo("CLEAR ALL", 0); hint_two("GLO", "SYNC");
        break;
    case 1:
        if (r_stages) { p = put_u(put(a, "STAGE "), rst_cur % 6u + 1u); *p = 0; hint_algo(a, 0); hint_key_r(BK_SEG, "BACK"); break; }
        p = put(b, "LOW ");
        if (pat.r_low) *put_u(p, 50u + ((pat.r_low * 350u) >> 16)) = 0; else *put(p, "OFF") = 0;
        if (shift()) { hint_algo(b, 1); hint_key_r(BK_SEG, "STAGES"); break; }
        hint_algo(TN[pat.r_track & 3u], 0); hint_two("GLO+", 0); hint_x -= 6; hint_key(BK_SEG, "STAGES");
        break;
    case 2:
        if (dband == 4u) { hint_algo("LOOP", 0); hint_two("GLO", "MIX XOVER"); break; }
        if (shift()) {                          /* GLO held: ALGORITHM is the band's BODY */
            p = put_sgn(put(b, "BODY "), -54 + (int32_t)(pat.d_body[dband] * 48u / 65535u)); p = put(p, "dB"); *p = 0;
            hint_algo(b, 1);
            break;
        }
        p = a; *p++ = (char)('1' + dband); *p++ = ' '; p = put(p, BAND_NAME[dband]); *p = 0;
        hint_algo(a, 0); hint_two("GLO", "BODY");
        break;
    case 3:
        hint_lamp("FREEZE", sp_frz, 0);
        if (!shift()) hint_two("GLO", "SHIMMER");   /* (GLO: K1 is it) */
        break;
    case PAGE_TIME: {
        char pd[12];
        if (sp_fold(pd)) { hint(dl_lim() == SD_SP_LIM2 ? "OVER 0.5 S: PLAYS" : "OVER 1 S: PLAYS", pd); break; }   /* (on
                                                   the beat still: TIME red; 0.5 s: the looper has the line's rest) */
        hint("ALGO MODE", SPMODE_NAME[pat.s_mode & 3u]);
        break;
    }
    case PAGE_LOOP: {
        char pd[12];
        if (lp_fold(pd)) { hint("LENGTH: PLAYS", pd); break; }
        if (shift()) { hint_lamp("REV", lp_rev, 1); hint_two("OCT", "OCTAVE"); break; }
        hint_lamp("LOOP", lp_on, 0); hint_two("GLO", "REV");
        break;
    }
    case PAGE_ENV: hint_two("GLO", "ENV 2"); break;   /* (ALGORITHM does nothing there) */
    case PAGE_FILT:
        p = put_u(put(b, "VEL "), pat.f_vel); *p++ = '%'; *p = 0;
        if (shift()) { hint_algo(b, 1); break; }
        hint_algo(pat.f_type == SD_FT_LP24 ? "LP24" : "LP12", 0); hint_two("GLO", "VEL");
        break;
    case 4:
        if (dmod == DMOD_TUR) { hint_algo(SDIV_NAME[SD_TUR_DIV(&pat)], 0); break; }   /* (MOD next: the top line) */
        if (dmod >= SD_MODS) { hint_algo(LMODE_SAY[lmode_of(pat.lfo_mode[dmod - SD_MODS])], 0); break; }   /* an LFO:
                                                   its MODE, said (where it goes: the picture) */
        p = put(a, "SLOT "); *put_u(p, dmod + 1u) = 0;
        hint_algo(a, 0);
        break;
    case PAGE_ARP: hint_lamp("HOLD", set.ahold, 0); hint_two("REC", "KEEP"); break;
    case PAGE_ORB:
        if (orb_setup) { hint_key(BK_SEG, "BACK"); hint_two("GLO", "GLIDE"); break; }
        p = put(a, "STEP "); *put_u(p, orb_cur % SD_ORB_STEPS + 1u) = 0;
        hint_lamp(a, eng.orb_hold != 0, 0);     /* (lit: the step held, heard while edited; GLO + its key again lets
                                                   it go) */
        if (shift()) hint_key_r(BK_SEG, "SETUP");
        else { hint_two("GLO+", 0); hint_x -= 6; hint_key(BK_SEG, "SETUP"); }   /* (the keys play: GLO its tools) */
        break;
    default:
        if (seq_rec) {                          /* (GLO + B11: back; B11 alone records its note) */
            hint("REC", eng.seq_run ? "LIVE" : "STEP");
            if (shift()) hint_key_r(BK_KEYS, "KEYS");
            break;
        }
        if (seq_segmode) {                      /* SEGMENTS: how many parts (ALGORITHM), the way back */
            uint8_t bd[SD_SEGS + 1u];
            p = put_u(a, sd_seg_bounds(&seqp, bd)); *put(p, " PARTS") = 0;
            hint_algo(a, 0); hint_key_r(BK_SEG, "BACK");
            break;
        }
        if (shift()) {                          /* GLO held: ALGORITHM the scale; IN KEY the key (the master's: on the
                                                   other track's page, KEY) */
            const SdPattern *kp = set.xchrom ? &seqp : key_pat();
            p = put_note(a, sd_pat_key(kp)); while (p > a && p[-1] >= '0' && p[-1] <= '9') p--;   /* (the root's name) */
            *p++ = ' '; *put(p, SCALE_NAME[kp->scale % SD_SCALES]) = 0;
            if (!set.xchrom && key_trk() != seq_trk) hint("KEY", a);
            else { hint_algo(a, 1); hint_x = HINT_R; hint_c("SELECT", "ROOT", C_RED); }   /* (GLO + SELECT) */
            break;
        }
        p = put(a, "STEP "); *put_u(p, seq_cur + 1u) = 0;
        hint_algo(a, 0);
        if (!set.xchrom && key_trk() != seq_trk) hint_two("KEY", key_trk() ? "SEQ B" : "SEQ A");
        else hint_two("GLO", "KEY");     /* (GLO + ALGORITHM the scale, GLO + SELECT the root; the key: the top line) */
        break;
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
        p = put_u(p, set.bpm);
        p = put(p, " BPM OCT");
    }
    *p++ = oct < 0 ? '-' : '+';
    *p++ = (char)('0' + (oct < 0 ? -oct : oct));
    *p = 0;
    cv_box(8, 4, 8, 10, run ? C_RED : C_ICE_D);   /* play: a square, red; stopped: dim */
    cv_text(22, 1, &FONT_S, s, run ? C_ICE : C_ICE_D);
    {                                           /* the mode, always (the CPU is SETUP's), then who holds the voices:
                                                   A B the tracks, R ARP, - the keys' */
        uint32_t k, i, n = 0, x;
        static const char L[4] = {'-', 'A', 'B', 'R'};
        static const uint8_t ORD[4] = {SD_SRC_A, SD_SRC_B, SD_SRC_ARP, SD_SRC_HAND};
        for (k = 0; k < 4u; k++) n += eng.quota[ORD[k]];
        const char *mn = MODE_NAME[eng.mode_on < SD_MODES ? eng.mode_on : 0];
        x = 232u - 8u * (n > 4u ? 4u : n);
        cv_text((int32_t)x - 8 - text_w(&FONT_S, mn), 1, &FONT_S, mn, C_ICE);   /* (the mode: how many voices) */
        for (k = 0; k < 4u; k++)
            for (i = 0; i < eng.quota[ORD[k]] && x < 232u; i++, x += 8u) {
                char c[2] = {L[ORD[k]], 0};
                cv_text((int32_t)x, 1, &FONT_S, c, ORD[k] == SD_SRC_HAND ? C_ICE_D : C_ICE);
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
static void draw_frame(void)
{
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
    } else {
        if (listed) { lcd_fill(0, Y_KNOB + H_KNOB, 240, Y_PIC - Y_KNOB - H_KNOB, C_BG); listed = 0; shown[6] = ~0u; }
        draw_knobs(); cv_show(Y_KNOB, 3);
        cv_begin(240, H_PIC, C_BG);
        if (save_mode || save_pending || saved_at) draw_save(); else pic();
        cv_show(Y_PIC, 4);
    }
    draw_hint(); cv_show(Y_HINT, 5);
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
    if (page == PAGE_ORB && !seq_rec && (shift() || orb_setup)) {   /* ORBIT, GLO: its steps on the first white keys
                                                   (picked full, the one playing flips), SETUP B10 (D#5); else as
                                                   played */
        uint32_t len = SD_ORB_LEN(&orbp), play = eng.seq_run && orbp.on ? eng.orb_shown : 0xFFu;
        for (k = 0; k < 27u; k++) {
            int32_t w = key_white(k), bk = key_black(k);
            uint32_t on;
            if (bk == BK_SEG) { if (orb_setup) full |= 1u << k; else glow |= 1u << k; bk_led[BK_SEG] = orb_setup ? 2u : 1u; continue; }
            if (w < 0 || (uint32_t)w >= len) continue;
            on = (uint32_t)w == orb_cur;
            if ((uint32_t)w == play) on = !on;
            if (on) full |= 1u << k; else glow |= 1u << k;
        }
    } else if (seq_page() && !seq_rec && shift() && !seq_segmode) {   /* GLO: the black keys' tools (as below), and
                                                   on the other keys the notes a key may take the track to (IN KEY the
                                                   key's), as printed: OCT's octave left out (it is the playing's) */
        int32_t xn = eng.t[seq_trk].xpose_next, xc = eng.t[seq_trk].xpose, z = (int32_t)key_none() - (int32_t)KEY0;
        uint32_t tools = seq_tool_keys(), tl;
        for (k = 0; k < 27u; k++) if (key_has(KEY0 + k)) glow |= 1u << k;
        if (z + xc >= 0 && z + xc < 27) full |= 1u << (z + xc);
        if (z + xn >= 0 && z + xn < 27 && (xn == xc || (fm1_ms / 200u) & 1u)) full |= 1u << (z + xn);
        full &= ~tools; glow &= ~tools;
        for (k = 0; k < 27u; k++) {
            int32_t bk = key_black(k);
            if (!(tools >> k & 1u)) continue;
            tl = bk == BK_MORE ? (seq_more ? 2u : 1u) : bk == 1 ? (seq_has_undo ? 1u : 0u) :
                 bk == 2 ? (seq_clr_at && (fm1_ms / 200u) & 1u ? 2u : 1u) : 1u;   /* (MORE full while on, UNDO lit
                                                   while there is something to take back, CLR blinking while it asks) */
            if (tl == 2u) full |= 1u << k; else if (tl) glow |= 1u << k;
            bk_led[bk] = (uint8_t)tl;
        }
    } else if (seq_page() && !seq_rec && seq_segmode) {   /* SEGMENTS: the white keys the steps, the part picked
                                                   full; B10 (D#5) the way back; the other black keys rest */
        uint8_t bd[SD_SEGS + 1u];
        uint32_t sc = sd_seg_bounds(&seqp, bd), g = seq_dseg < sc ? seq_dseg : sc - 1u;
        for (k = 0; k < 27u; k++) {
            int32_t w = key_white(k), bk = key_black(k);
            if (w >= 0) {
                i = seq_bank * 16u + (uint32_t)w;
                if (i >= seqp.len) continue;
                if (i >= bd[g] && i < bd[g + 1u]) full |= 1u << k; else glow |= 1u << k;
            } else if (bk == BK_SEG) {
                full |= 1u << k;
            }
            if (bk >= 0) bk_led[bk] = (uint8_t)(full >> k & 1u ? 2u : glow >> k & 1u ? 1u : 0u);
        }
    } else {
        full = held;
        if (seq_page() && seq_rec && shift()) { /* playing into the steps, GLO: B11 lit, the way back */
            for (k = 0; k < 27u; k++) if (key_black(k) == BK_KEYS) full |= 1u << k;
            bk_led[BK_KEYS] = 2u;
        }
        for (i = 0; i < SD_VOICES; i++) {
            const SdVoice *v = &eng.voice[i];
            uint32_t n = v->note;
            if (n < base || n >= base + 27u || v->env1.stage == SD_ENV_OFF) continue;
            if (v->gate) full |= 1u << (n - base);
            else glow |= 1u << (n - base);
        }
        if (page == 1u && !seq_rec)             /* REACTOR: B10 STAGES, with GLO (full while on: back); there the
                                                   stages' white keys 1..6, the one edited full */
            for (k = 0; k < 27u; k++) {
                int32_t w = key_white(k);
                if (key_black(k) == BK_SEG) {
                    if (r_stages) full |= 1u << k; else if (shift()) glow |= 1u << k;
                    bk_led[BK_SEG] = r_stages ? 2u : shift() ? 1u : 0u;
                }
                else if (r_stages && w >= 0 && w < 6) { if ((uint32_t)w == rst_cur % 6u) full |= 1u << k; else glow |= 1u << k; }
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
 * together, a chord; all up, the next step), playing, into the step that plays; what is heard (sd_seq_rec_note). The
 * keys' and MIDI's held notes count together */
static void midi_note(uint32_t n, uint32_t v)
{
    uint32_t bit = 1u << (n & 31u), *w = &midi_held[n >> 5 & 3u];
    if (v) {
        uint32_t others = midi_hn || fm1_in.notes;
        sd_engine_note(&eng, n, v);
        if (!(*w & bit)) { *w |= bit; midi_hn++; }
        if (seq_rec) {
            uint32_t rn = sd_seq_rec_note(&eng, &eng.t[seq_trk], n);
            if (rec_joins() && others) seq_chord(rn);
            else seq_write_v(rn, SD_ST_NOTE, v);
        } else if (!eng.arp_on && eng.seq_run && !eng.quota[SD_SRC_HAND]) {
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
        if (st >= 0xF8u) clk_rt(st, src);
        else if (ty == 0x90u || ty == 0x80u) midi_note(d1 & 127u, ty == 0x90u ? d2 : 0u);
        else if (ty == 0xB0u && d1 == 1u) eng.wheel = d2;   /* C1: the matrix's WHEEL */
        else if (ty == 0xD0u) eng.after = d1;   /* and AFTER (channel pressure) */
    }
    if (clk_cue && fm1_ms - clk_cue_t > 250u) { clk_cue = 0; eng.seq_run = 1; eng.clk_start++; }   /* (no pulse) */
}
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
 * address, which KB of the 32 has one: 2026-11-17, does something write over it?) */
static void sd_diag_service(void)
{
    uint8_t out[7u + 72u * 8u];                 /* (on the stack: the SMK-37 has no RAM to spare; sent before
                                                   it returns) */
    const uint8_t *p;
    uint32_t n, v[72], k = 0, i, j, h = 2166136261u;
    const char *nm = APP_NAME " " APP_VER;
    if (!ota_frame_get(&p, &n)) return;
    if (n != 4u || p[0] != 0x7Du || p[1] != 0x53u || p[2] != 0x44u || p[3] != 0x01u) return;   /* (not ours: on) */
    ota_frame_done();
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
    out[0] = 0xF0u; out[1] = 0x7Du; out[2] = 0x53u; out[3] = 0x44u; out[4] = 0x02u; out[5] = (uint8_t)k;
    for (i = 0; i < k; i++)
        for (j = 0; j < 8u; j++) out[6u + 8u * i + j] = (uint8_t)(v[i] >> (28u - 4u * j) & 15u);
    out[6u + 8u * k] = 0xF7u;
    (void)ota_wire_send(out, 7u + 8u * k);
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
    {                                           /* the pool is not cleared at boot: SPACE's lines start silent */
        uint32_t i, *w = (uint32_t *)(void *)&space_mem;
        for (i = 0; i < sizeof space_mem / 4u; i++) w[i] = 0;
    }
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
    if ((settings_load(&set, sizeof set) && settings_load(&set, (uint32_t)(&set.accent - &set.oct)))   /* (or as
                                                   they were saved before COLOUR, 2026-11-16) */
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
    accent_set(set.accent);
    if (slot_load(0, &upat, sizeof upat)) { uint8_t *z = (uint8_t *)&upat; for (k = 0; k < sizeof upat; k++) z[k] = 0; }
    if (slot_load(1, &useq, USEQ_HALF) || slot_load(2, (uint8_t *)&useq + USEQ_HALF, sizeof useq - USEQ_HALF)) {
        uint8_t *z = (uint8_t *)&useq;
        for (k = 0; k < sizeof useq; k++) z[k] = 0;
    }
    if (slot_load(3, &uorb, sizeof uorb)) { uint8_t *z = (uint8_t *)&uorb; for (k = 0; k < sizeof uorb; k++) z[k] = 0; }
    if (!patch_ok(set.patch)) set.patch = SD_PATCH_WORST;
    eng.mode = set.mode;
    eng.bpm = set.bpm;
    patch_load(set.patch, &pat);
    sd_engine_patch(&eng, &pat);
    if (useq.used >> set.pattern & 1u) seqp = useq.p[set.pattern];
    else sd_pattern_default(&seqp);
    root_own(&seqp);
    seq_push();
    orb_load(set.pattern);
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
            orbp.on = 1; orbp.len = 4; orbp.rate = 0; orbp.glide = SD_OG_L1 | SD_OG_L2 | SD_OG_MAC;
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
        if (seq_page() && seq_rec && shift()) { /* the keys playing into the steps: GLO + B11 (F#5) is the way back, no
                                                   note (B11 alone records its note) */
            uint32_t fk = 0;
            for (k = 0; k < 27u; k++) if (key_black(k) == BK_KEYS) fk = 1u << k;
            if (down & fk) { rec_toggle(); if (fm1_in.buttons >> BTN_GLO & 1u) gl_used = 1; }
            down &= ~fk; keys &= ~fk;
        }
        if (page != 1u) r_stages = 0;           /* (STAGES: REACTOR's only) */
        if (page == 1u && !seq_rec) {           /* REACTOR: GLO + B10 (D#5) STAGES, no note (B10 alone plays; in
                                                   STAGES it is the way back); there the white keys 1..6 pick the stage
                                                   (POS to it), the others play */
            uint32_t fk = 0;
            for (k = 0; k < 27u; k++) {
                int32_t w = key_white(k), bk = key_black(k);
                uint32_t fn = bk == BK_SEG && (r_stages || shift());
                if (fn || (r_stages && w >= 0 && w < 6)) fk |= 1u << k;
                if (!(down >> k & 1u)) continue;
                if (fn) {
                    if (fm1_in.buttons >> BTN_GLO & 1u) gl_used = 1;   /* (GLO held: no tap) */
                    if (!r_stages) gl_latch = 0;    /* (a latched GLO spent: STAGES opens on its first layer) */
                    r_stages = (uint8_t)!r_stages;
                    dlist_at = 0;
                    if (r_stages && !fx_on(SD_FX_REACTOR)) msg("REACTOR OFF: EDIT TURNS IT ON");
                    if (r_stages) { pat.r_pos = (uint16_t)(rst_cur * 256u); sd_engine_patch(&eng, &pat); }
                } else if (r_stages && w >= 0 && w < 6) {
                    rst_cur = (uint8_t)w;
                    pat.r_pos = (uint16_t)(rst_cur * 256u);
                    dlist_at = 0;
                    sd_engine_patch(&eng, &pat);
                }
            }
            down &= ~fk; keys &= ~fk;
        }
        sq = seq_page() && !seq_rec && (shift() || seq_segmode);   /* SEQ: the keys play (2026-11-17, the user: SELECT
                                                   picks the step, so the keys try a line before REC); GLO there the
                                                   black keys' tools and the transpose; SEGMENTS its keys */
        oq = page == PAGE_ORB && !seq_rec && (shift() || orb_setup);   /* ORBIT: the same, GLO its steps and SETUP */
        if (sq && seq_segmode) {
            seq_keys(down);
        } else if (sq) {                        /* GLO: B1 RND, B2 UNDO, B3 CLR, B4 MORE, B10 SEG, B11 KEYS (REC); any
                                                   other key transposes the track, at its next one: IN KEY its root to
                                                   the key pressed, in the key (the keys lit), CHROM from C4; the keys
                                                   as printed, whatever OCT's octave (B5..B9 hold every black note; a
                                                   transpose past them: GLO + K4 XPOSE) */
            uint32_t tools = seq_tool_keys();
            if (down && (fm1_in.buttons >> BTN_GLO & 1u)) gl_used = 1;   /* (GLO held: no tap) */
            seq_keys(down & tools);
            for (k = 0; k < 27u; k++)
                if ((down & ~tools) >> k & 1u) {
                    int32_t x = (int32_t)(KEY0 + k) - (int32_t)key_none();
                    if (!key_has(KEY0 + k)) { msg("NOT IN THE KEY"); continue; }
                    eng.t[seq_trk].xpose_next = (int8_t)(x < -24 ? -24 : x > 24 ? 24 : x);
                }
        } else if (oq) {                        /* ORBIT, GLO: the white keys its steps, B10 (D#5) SETUP */
            if (down && (fm1_in.buttons >> BTN_GLO & 1u)) gl_used = 1;
            for (k = 0; k < 27u; k++)
                if (down >> k & 1u) {
                    int32_t w = key_white(k), bk = key_black(k);
                    if (bk == BK_SEG) orb_setup = !orb_setup;
                    else if (w >= 0 && (uint32_t)w < SD_ORB_LEN(&orbp)) {   /* the step: picked and heard (held);
                                                   the held one again: let go, ORBIT on in time */
                        orb_cur = (uint32_t)w;
                        if (!orb_setup && eng.orb_hold != w + 1) orb_hold_step((uint32_t)w); else eng.orb_hold = 0;
                    }
                }
        }
        if (down && !sq && !oq && !seq_rec && !seq_page() && !eng.arp_on && eng.seq_run && !eng.quota[SD_SRC_HAND])
            msg(set.mode == SD_MODE_MONO ? "MONO: THE SEQ HAS THE VOICE" : "NO FREE VOICE: SEQ PLAYS");
        for (k = 0; k < 27u; k++) {
            if (down >> k & 1u && !sq && !oq) {
                uint32_t others = keys & ~(1u << k) & ~down;   /* keys already held: this one joins their chord */
                held[k] = (uint8_t)(base + k);
                sd_engine_note(&eng, held[k], 100);
                midi_send(0x90u, held[k], 100);
                if (seq_rec) {                  /* (what is heard: back by the track's transpose) */
                    uint32_t rn = sd_seq_rec_note(&eng, &eng.t[seq_trk], held[k]);
                    if (rec_joins() && (others || midi_hn || (down & ((1u << k) - 1u)))) seq_chord(rn);
                    else seq_write(rn, SD_ST_NOTE);
                }
            }
            if (up >> k & 1u && held[k]) { sd_engine_note(&eng, held[k], 0); midi_send(0x80u, held[k], 0); held[k] = 0; }
            if (keys >> k & 1u) lit |= 1u << k;
        }

        if (seq_rec && !keys && !midi_hn) seq_keys_up();   /* REC: every key (and MIDI's) up, the next step */
        if (seq_rec && eng.seq_run && rec_at != 0xFFu && (keys || midi_hn)) {   /* REC, playing: a key still held
                                                   as the steps go on, a TIE each (the note as long as it is held) */
            uint32_t at = eng.t[seq_trk].seq_shown;
            if (at < SD_PSTEPS && at < seqp.len && at != rec_tie_at && seqp.step[at].type != SD_ST_SKIP) {
                if (rec_at < SD_PSTEPS) {       /* (the held note's own: a TIE turned into a NOTE later plays it) */
                    seqp.step[at].note = seqp.step[rec_at].note;
                    seqp.step[at].vel = seqp.step[rec_at].vel;
                }
                seqp.step[at].type = SD_ST_TIE;
                rec_tie_at = at;
                seq_push();
            }
        }
        if (eng.orb_hold && (page != PAGE_ORB || orb_setup || eng.orb_hold > SD_ORB_LEN(&orbp) ||
                             fm1_ms - orb_hold_at >= ORB_HOLD_MS))
            eng.orb_hold = 0;                   /* (ORBIT left, its SETUP, the step past LENGTH, or no edit for 2 s:
                                                   on in time) */
        if (aud_c && (eng.seq_run || seq_rec || !seq_page() || (aud_key < 27u && (up >> aud_key & 1u)) ||
                      (aud_off && (int32_t)(fm1_ms - aud_off) >= 0))) aud_stop();   /* (the step heard: done) */
        b = fm1_input_edges(&rel);
        {                                       /* GLO, the second layer: a tap latches it or lets it go */
            if (b & (1u << BTN_GLO)) { gl_at = fm1_ms; gl_used = 0; }
            else if ((fm1_in.buttons >> BTN_GLO & 1u) && (b || down)) gl_used = 1;   /* (a knob: enc_take) */
            if ((rel >> BTN_GLO & 1u) && !gl_used && fm1_ms - gl_at < GL_TAP_MS) gl_latch = (uint8_t)!gl_latch;
            b &= ~(1u << BTN_GLO);              /* (not a button for the rest: it closes no list) */
        }
        if (b) dlist_at = 0;                    /* (a button: the destinations' list closes) */
        if (b && browse_at) {                   /* the list open: EDIT (or OCT+) loads, HOME (or OCT-) not now; another
                                                   button closes it */
            if (b & (1u << BTN_EDIT | 1u << BTN_OCT_UP)) {
                if (browse_kind == 2u) pattern_take(browse_to);
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
        if (clr_all_at && (page != PAGE_SETUP || fm1_ms - clr_all_at >= 3000u)) clr_all_at = 0;
        if (clr_all_at && (b & (1u << BTN_OCT_UP | 1u << BTN_OCT_DOWN))) {   /* CLEAR ALL asked: OCT+ yes, OCT- no */
            if (b & (1u << BTN_OCT_UP)) clear_all(); else clr_all_at = 0;
            b &= ~(1u << BTN_OCT_UP | 1u << BTN_OCT_DOWN);
        }
        if (b & (1u << BTN_REC)) {
            if (page == PAGE_ARP && !seq_rec) arp_keep();   /* on ARP: KEEP what it played */
            else rec_toggle();
        }
        if (seq_rec && (b & (1u << BTN_ENV))) { seq_write(0, SD_ST_TIE); b &= ~(1u << BTN_ENV); }
        if (seq_rec && (b & (1u << BTN_LFO))) { seq_write(0, SD_ST_REST); b &= ~(1u << BTN_LFO); }
        /* the pages, each its button's, the button again the next of them (the top line shows them): HOME the voice and
           the macros, FX the effects, ENV the envelopes and the filter, LFO the LFOs, RADAR and the matrix, ARP, SEQ
           the tracks and ORBIT */
        {
            uint32_t was = page;
            if (b & (1u << BTN_SEQ)) {
                if (!seq_page() && page != PAGE_ORB) {   /* from another page: the one SEQ showed last */
                    if (seq_last == 2u) { page = PAGE_ORB; seq_track(0); orb_setup = 0; }
                    else { page = PAGE_SEQ; seq_track(seq_last); }
                }
                else if (page == PAGE_SEQ && !seq_trk) seq_track(1);
                else if (page == PAGE_SEQ) { page = PAGE_ORB; seq_track(0); orb_setup = 0; }
                else { page = PAGE_SEQ; seq_track(0); }   /* ORBIT -> SEQ A: round its own pages, as every page
                                                   button */
            }
            if (b & (1u << BTN_HOME)) page = page == 5u ? 0u : page == 0u ? PAGE_SETUP : 5u;   /* (from elsewhere:
                                                   MACROS) */
            if (b & (1u << BTN_FX))             /* (from elsewhere: the one it showed last) */
                page = page == 1u ? 2u : page == 2u ? 3u : page == 3u ? PAGE_TIME : page == PAGE_TIME ? 1u : fx_last;
            if (b & (1u << BTN_ENV)) page = page == PAGE_ENV ? PAGE_FILT : page == PAGE_FILT ? PAGE_ENV : env_last;
            if (b & (1u << BTN_LFO)) {          /* LFO 1, LFO 2, RADAR, the slots (the one last picked); from elsewhere
                                                   the one it showed last (dmod kept) */
                if (page == 4u) dmod = dmod == SD_MODS ? SD_MODS + 1u : dmod == SD_MODS + 1u ? DMOD_TUR :
                                       dmod == DMOD_TUR ? dslot : SD_MODS;
                page = 4u;
            }
            if (b & (1u << BTN_ARP)) page = page == PAGE_ARP ? PAGE_LOOP : page == PAGE_LOOP ? PAGE_ARP : arp_last;
            if (page != was) { seq_segmode = 0; seq_more = 0; }
            if (b & (1u << BTN_HOME | 1u << BTN_FX | 1u << BTN_ENV | 1u << BTN_LFO | 1u << BTN_ARP | 1u << BTN_SEQ))
                gl_latch = 0;                   /* a page button: a latched GLO let go (2026-11-17, the user: the new
                                                   page starts on its first layer; held, it is still held) */
            if (page == 1u || page == 2u || page == 3u || page == PAGE_TIME) fx_last = page;
            if (page == PAGE_ARP || page == PAGE_LOOP) arp_last = page;
            if (page == PAGE_ENV || page == PAGE_FILT) env_last = page;
            if (page == PAGE_SEQ) seq_last = seq_trk; else if (page == PAGE_ORB) seq_last = 2u;
        }
        if (b & (1u << BTN_EDIT)) {             /* EDIT: what this page shows, on / off */
            if (page == PAGE_LOOP) {     /* EDIT on LOOPER: armed or not, L-LEVEL as it is (at 0 it records, silent:
                                                   then faded in) */
                lp_arm = (uint8_t)!lp_arm;
                msg(lp_arm ? (eng.lp_borrow ? "LOOPER ON: DELAY 0.5 S" : "LOOPER ON") : (eng.lp_borrow ? "LOOPER OFF: DELAY 1 S" : "LOOPER OFF"));
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
        if (page == PAGE_LOOP && shift() && b & (1u << BTN_OCT_DOWN | 1u << BTN_OCT_UP)) {   /* GLO + OCT on LOOPER:
                                                   the loop's SPEED to the next octave (-12 0 +12), over GLIDE */
            int32_t v = lp_speed;
            if (b & (1u << BTN_OCT_DOWN)) v = v > 0 ? 0 : -3072;
            if (b & (1u << BTN_OCT_UP)) v = v < 0 ? 0 : 3072;
            msg(v == lp_speed ? (v < 0 ? "SPEED -12: ITS LOWEST" : "SPEED +12: ITS HIGHEST") :
                v < 0 ? "SPEED -12: AN OCTAVE DOWN" : v > 0 ? "SPEED +12: AN OCTAVE UP" : "SPEED 0: AS RECORDED");
            lp_speed = (int16_t)v;
            b &= ~(1u << BTN_OCT_DOWN | 1u << BTN_OCT_UP);
        }
        if (b & (1u << BTN_OCT_DOWN) && oct > -2) { set.oct = (uint8_t)(oct - 1); dirty = 1; t_change = fm1_ms; }
        if (b & (1u << BTN_OCT_UP) && oct < 2) { set.oct = (uint8_t)(oct + 1); dirty = 1; t_change = fm1_ms; }
        {
            uint32_t changed = 0, l2 = shift();
            if ((e = enc_take(ENC_SELECT)) != 0) changed = select_turn(e);   /* SELECT: which one, in a row */
            if ((e = enc_take(ENC_PRESETS)) != 0) {   /* the list (the sound as it is): EDIT loads */
                uint32_t kind = seq_page() || page == PAGE_ORB ? 2u : 1u;   /* (SEQ: the patterns, P01..P32) */
                if (!browse_at || browse_kind != kind) { browse_kind = kind; browse_to = kind == 2u ? set.pattern : set.patch; }
                while (e) {                     /* a detent a place (the patches past the empty user places) */
                    browse_to = browse_step(browse_to, e > 0 ? 1 : -1);
                    e += e > 0 ? -1 : 1;
                }
                browse_at = fm1_ms ? fm1_ms : 1u;
            }
            if (save_mode && (e = enc_take(ENC_ALGORITHM)) != 0) {   /* SAVE: where to */
                save_to = step_mod(save_to, e, save_mode == 2u ? UPATTERNS : UPATCHES);
                save_at = fm1_ms;
            }
            if (seq_page()) seq_knobs();        /* (it takes every encoder but PRESETS: the rest below sees 0) */
            if (page == PAGE_ORB) orb_knobs();  /* (so does ORBIT's) */
            if ((e = enc_take(ENC_ALGORITHM)) != 0) {   /* ALGORITHM: the page's choice (the hint line says it) */
                switch (page) {
                case 0:
                    if (l2) { ebank = step_mod(ebank, e, SD_BANKS); gen = 0xFFFFFFFFu; }   /* GLO: the bank */
                    else {                      /* the bank's roles: the algorithms */
                        uint32_t a = algo_of(&pat);
                        a = a >= SD_ALGOS ? 0u : step_mod(a, e, SD_ALGOS);
                        for (k = 0; k < SD_OPS; k++) pat.bank[ebank].osc[k].role = sd_role_of(SD_ALGO[a][k]);
                        dlist_open(DL_ALG);
                        changed = 1;
                    }
                    break;
                case 5: pat.mac4 = (uint8_t)step_mod(pat.mac4 % SD_M4S, e, SD_M4S); changed = 1; break;
                case PAGE_SETUP: clr_all_at = fm1_ms ? fm1_ms : 1u; break;   /* CLEAR ALL? (OCT+ yes, OCT- no) */
                case 1:
                    if (l2) pat.r_low = knob_q(pat.r_low, e);   /* GLO: LOW */
                    else if (r_stages) {        /* STAGES: the stage, POS to it */
                        rst_cur = (uint8_t)step_mod(rst_cur % 6u, e, 6u);
                        pat.r_pos = (uint16_t)(rst_cur * 256u);
                        dlist_at = 0;
                    } else pat.r_track = (uint8_t)step_mod(pat.r_track & 3u, e, 4u);   /* TRACK */
                    changed = 1;
                    break;
                case 2:
                    if (!l2) { dband = step_mod(dband, e, 5u); dlist_at = 0; }   /* the band, then LOOP: its frame
                                                   moves in the picture (no list, 2026-11-17) */
                    else if (dband < 4u) {      /* GLO: its BODY (where PHAT and SHARP act) */
                        pat.d_body[dband] = knob_q(pat.d_body[dband], e);
                        changed = 1;
                    }
                    break;
                case PAGE_FILT:
                    if (l2) {                   /* GLO: VEL */
                        int32_t v = (int32_t)pat.f_vel + e * 5;
                        pat.f_vel = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
                    } else if (pat.f_type == SD_FT_OFF) {   /* the type (EDIT switches it on) */
                        pat.f_type = SD_FT_LP12;
                        pat.fx_off = (uint8_t)(pat.fx_off | SD_FX_FILTER);
                        filt_first();
                    } else {
                        pat.f_type = pat.f_type == SD_FT_LP12 ? SD_FT_LP24 : SD_FT_LP12;
                    }
                    changed = 1;
                    break;
                case PAGE_ARP:
                    set.ahold = (uint8_t)!set.ahold;
                    dirty = 1;
                    t_change = fm1_ms;
                    break;
                case 4:
                    if (dmod < SD_MODS) { dmod = step_mod(dmod, e, SD_MODS); dslot = dmod; }
                    else if (dmod == DMOD_TUR) {   /* RADAR: its DIV */
                        pat.tur_ld = (uint8_t)((pat.tur_ld & 31u) | (step_lim(SD_TUR_DIV(&pat), e, SD_TUR_DIVS) + 1u) << 5);
                        changed = 1;
                    } else {                    /* an LFO: its MODE */
                        uint32_t lk = dmod - SD_MODS;
                        pat.lfo_mode[lk] = LMODE_BITS[step_mod(lmode_of(pat.lfo_mode[lk]), e, LMODES)];
                        changed = 1;
                    }
                    break;
                case 3:                         /* SPACE: FREEZE (SHIMMER: GLO's K1) */
                    sp_frz = (uint8_t)!sp_frz;
                    msg(sp_frz ? "FREEZE: THE TAIL HELD" : "FREEZE OFF");
                    break;
                case PAGE_TIME: {               /* TIME: the delay's MODE */
                    static const char *const MM[4] = {"MODE: FREE (MS)", "MODE: SYNC", "MODE: FREE, PING-PONG", "MODE: SYNC, PING-PONG"};
                    pat.s_mode = (uint8_t)((pat.s_mode & ~3u) | ((uint32_t)((int32_t)(pat.s_mode & 3u) + (e > 0 ? 1 : 3)) & 3u));
                    msg(MM[pat.s_mode & 3u]);
                    changed = 1;
                    break;
                }
                case PAGE_LOOP:                 /* LOOPER: LOOP (its last LENGTH round and round); GLO: REVERSE (it
                                                   plays it backwards) */
                    if (l2) {
                        lp_rev = (uint8_t)!lp_rev;
                        msg(lp_rev ? "REVERSE: THE LOOP BACKWARDS" : "REVERSE OFF");
                    } else {
                        lp_on = (uint8_t)!lp_on;
                        msg(lp_on ? (lp_arm ? "LOOP: THE LAST LENGTH HELD" : "LOOP: THE LOOPER IS OFF (EDIT)") : "LOOP OFF");
                    }
                    break;
                default: break;                 /* ENV, the LFOs: nothing */
                }
            }
            if (page == PAGE_ENV || (page == PAGE_FILT && l2)) {   /* an envelope: A D S R, 0..127 */
                static const uint8_t KN[4] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
                uint8_t *ev = page == PAGE_FILT ? &pat.e3_a : l2 ? &pat.e2_a : &pat.e1_a;
                for (k = 0; k < 4u; k++)
                    if ((e = enc_take(KN[k])) != 0) {
                        int32_t v = (int32_t)ev[k] + enc_far(e, 64u) * 2;
                        ev[k] = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
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
                    v = v < -99 ? -99 : v > 99 ? 99 : v;
                    pat.f_env = (int16_t)(v * 32767 / 99);
                    changed = 1;
                }
                if ((e = enc_take(ENC_KNOB4)) != 0) {
                    int32_t v = (int32_t)pat.f_key + e * 5;
                    pat.f_key = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
                    changed = 1;
                }
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
            } else if (page == PAGE_SETUP && l2) {   /* SETUP, GLO: K1 SYNC, a turn right INT (the rest: nothing) */
                uint32_t d0 = dirty;
                uint8_t v;
                if ((e = enc_take(ENC_KNOB1)) != 0 && set.sync_int != (v = (uint8_t)(e > 0))) {
                    set.sync_int = v;           /* (INT: a clock ignored; AUTO: it leads when it comes) */
                    clk_cue = 0;
                    dirty = 1;
                }
                (void)enc_take(ENC_KNOB2); (void)enc_take(ENC_KNOB3); (void)enc_take(ENC_KNOB4);
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
            } else if (page == 5u) {            /* MACROS: MUTATE MOVE BREAK MACRO 4 */
                static const uint8_t KN[4] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
                for (k = 0; k < 4u; k++)
                    if ((e = enc_take(KN[k])) != 0) pat.mac[k] = knob_q(pat.mac[k], e), changed = 1;
            } else if (page == 4u && dmod == DMOD_TUR) {   /* RADAR: CHANGE LENGTH DEST AMOUNT */
                if ((e = enc_take(ENC_KNOB1)) != 0) {
                    pat.tur_set = (uint8_t)((pat.tur_set & 0x80u) | step_lim(SD_TUR_CHANGE(&pat), enc_far(e, 101u), 101u));
                    dlist_at = 0;
                    changed = 1;
                }
                if ((e = enc_take(ENC_KNOB2)) != 0) {
                    pat.tur_ld = (uint8_t)((pat.tur_ld & 0xE0u) | (2u + step_lim(SD_TUR_LEN(&pat) - 2u, e, 15u)));
                    dlist_at = 0;
                    changed = 1;
                }
                if ((e = enc_take(ENC_KNOB3)) != 0) {   /* the matrix's destinations, then NOTES: the list */
                    pat.tur_dst = (uint8_t)dst_step(pat.tur_dst, e, 1);
                    dlist_open(DL_TUR);
                    changed = 1;
                }
                if ((e = enc_take(ENC_KNOB4)) != 0) {   /* a percent a detent */
                    int32_t v = (int32_t)pat.tur_amt + enc_far(e, 200u);
                    dlist_at = 0;
                    pat.tur_amt = (int8_t)(v < -100 ? -100 : v > 100 ? 100 : v);
                    changed = 1;
                }
            } else if (page == 4u && dmod < SD_MODS) {   /* MOD, a slot: SOURCE DEST AMOUNT */
                SdModSlot *m = &pat.mod[dmod];
                if ((e = enc_take(ENC_KNOB1)) != 0) m->src = (uint8_t)src_step(m->src, e), dlist_open(DL_SRC), changed = 1;
                if ((e = enc_take(ENC_KNOB2)) != 0) m->dst = (uint8_t)dst_step(m->dst, e, 0), dlist_open(DL_MOD), changed = 1;
                if ((e = enc_take(ENC_KNOB3)) != 0) {   /* a percent a detent */
                    int32_t v = amt99(m->amt) + enc_far(e, 198u);
                    dlist_at = 0;
                    v = v < -99 ? -99 : v > 99 ? 99 : v;
                    m->amt = (int16_t)(v * 32767 / 99);
                    changed = 1;
                }
                (void)enc_take(ENC_KNOB4);
            } else if (page == 4u) {            /* an LFO: SHAPE RATE DEST AMOUNT (ALGORITHM its MODE) */
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
                        if (!pat.mod[s].dst) { pat.mod[s].src = SD_MS_OFF; pat.mod[s].amt = 0; }
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
            } else if (page == 3u) {            /* SPACE: the plate (GLO: SHIMMER) */
                if (l2) {
                    if ((e = enc_take(ENC_KNOB1)) != 0) {
                        int32_t sh = (int32_t)SD_SP_SHIM(&pat) + enc_far(e, 64u);
                        sh = sh < 0 ? 0 : sh > 63 ? 63 : sh;
                        pat.s_mode = (uint8_t)((pat.s_mode & 3u) | (uint32_t)sh << 2);
                        changed = 1;
                    }
                    if ((e = enc_take(ENC_KNOB2)) != 0) {   /* PITCH: the frozen tail, -12 .. 12 semitones */
                        int32_t ps = sp_pitch + e;
                        sp_pitch = (int8_t)(ps < -12 ? -12 : ps > 12 ? 12 : ps);
                        if (!sp_frz) msg("PITCH: WHILE FROZEN");
                    }
                    (void)enc_take(ENC_KNOB3);
                    (void)enc_take(ENC_KNOB4);
                } else {
                    if ((e = enc_take(ENC_KNOB1)) != 0) pat.s_size = knob_q(pat.s_size, e), changed = 1;
                    if ((e = enc_take(ENC_KNOB2)) != 0) pat.s_decay = knob_q(pat.s_decay, e), changed = 1;
                    if ((e = enc_take(ENC_KNOB3)) != 0) pat.s_rtone = knob_q(pat.s_rtone, e), changed = 1;
                    if ((e = enc_take(ENC_KNOB4)) != 0) pat.s_rlevel = knob_q(pat.s_rlevel, e), changed = 1;
                }
            } else if (page == PAGE_LOOP && l2) {   /* LOOPER, GLO: FILTER (left a lowpass, right a highpass, none in
                                                   the middle: it stops there), R-SEND, D-SEND */
                if ((e = enc_take(ENC_KNOB1)) != 0) {
                    int32_t v = (int32_t)lp_filt + enc_far(e, 64u) * 1024, was = (int32_t)lp_filt - 32768;
                    v = v < 0 ? 0 : v > 65535 ? 65535 : v;
                    if ((v - 32768 > -1024 && v - 32768 < 1024) || (was < 0) != (v - 32768 < 0)) v = was ? 32768 : v;
                    lp_filt = (uint16_t)v;
                }
                if ((e = enc_take(ENC_KNOB2)) != 0) lp_rsend = (uint16_t)knob_q(lp_rsend, e);
                if ((e = enc_take(ENC_KNOB3)) != 0) lp_dsend = (uint16_t)knob_q(lp_dsend, e);
                (void)enc_take(ENC_KNOB4);
            } else if (page == PAGE_TIME || page == PAGE_LOOP) {   /* TIME: the delay; LOOPER: the looper */
                if (page == PAGE_LOOP) {
                    if ((e = enc_take(ENC_KNOB1)) != 0) {   /* GLIDE: OFF, then a division a detent synced,
                                                   ~16 ms free */
                        if (pat.s_mode & SD_SP_SYNC) {
                            int32_t d = (int32_t)sd_lp_glide_div(lp_glide) + e;
                            d = d < 0 ? 0 : d > (int32_t)SD_LP_NGLIDE - 1 ? (int32_t)SD_LP_NGLIDE - 1 : d;
                            lp_glide = (uint16_t)((uint32_t)d * 65536u / SD_LP_NGLIDE + (d ? 100u : 0u));
                        } else {
                            int32_t t = (int32_t)lp_glide + enc_far(e, 256u) * 256;
                            lp_glide = (uint16_t)(t < 0 ? 0 : t > 65535 ? 65535 : t);
                        }
                    }
                    if ((e = enc_take(ENC_KNOB2)) != 0) {   /* LENGTH: as TIME, a division a detent synced */
                        if (pat.s_mode & SD_SP_SYNC) {
                            int32_t was = (int32_t)sd_lp_div(lp_len), d = was + e;
                            d = d < 0 ? 0 : d > (int32_t)SD_LP_NDIV - 1 ? (int32_t)SD_LP_NDIV - 1 : d;
                            if (d > was) d = (int32_t)div_fit((uint32_t)d, (uint32_t)was, lp_lim());
                            lp_len = (uint16_t)((uint32_t)d * 65536u / SD_LP_NDIV + 100u);
                        } else {
                            int32_t t = (int32_t)lp_len + enc_far(e, 256u) * 256;
                            lp_len = (uint16_t)(t < 0 ? 0 : t > 65535 ? 65535 : t);
                        }
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
                            int32_t was = (int32_t)sd_sp_div(pat.s_time), d = was + e;
                            d = d < 0 ? 0 : d > (int32_t)SD_SP_NDIV - 1 ? (int32_t)SD_SP_NDIV - 1 : d;
                            if (d > was) d = (int32_t)div_fit((uint32_t)d, (uint32_t)was, dl_lim());
                            pat.s_time = (uint16_t)((uint32_t)d * 65536u / SD_SP_NDIV + 100u);
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
            } else if (page == 2u) {            /* DISTROY, the band ALGORITHM picks */
                uint32_t k2 = dband;
                if (k2 == 4u && l2) {           /* LOOP, GLO: the whole of DISTROY: MIX, XOVER */
                    if ((e = enc_take(ENC_KNOB1)) != 0) pat.d_mix = knob_q(pat.d_mix, e), changed = 1;
                    if ((e = enc_take(ENC_KNOB2)) != 0) pat.d_xover = e > 0 ? SD_DX_LR4 : SD_DX_CMP, changed = 1;
                    (void)enc_take(ENC_KNOB3);
                    (void)enc_take(ENC_KNOB4);
                } else if (k2 == 4u) {          /* LOOP (D3): FDBK TIME SPREAD TAME */
                    if ((e = enc_take(ENC_KNOB1)) != 0) pat.d_fb = knob_q(pat.d_fb, e), changed = 1;
                    if ((e = enc_take(ENC_KNOB2)) != 0) pat.d_ftime = knob_q(pat.d_ftime, e), changed = 1;
                    if ((e = enc_take(ENC_KNOB3)) != 0) pat.d_spread = knob_q(pat.d_spread, e), changed = 1;
                    if ((e = enc_take(ENC_KNOB4)) != 0) pat.d_tame = knob_q(pat.d_tame, e), changed = 1;
                } else if (l2) {
                    if ((e = enc_take(ENC_KNOB4)) != 0) {   /* the band's upper split, between its neighbours */
                        uint32_t si = k2 < 3u ? k2 : 2u;
                        int32_t lo = si ? pat.d_split[si - 1u] + 256 : SD_R_LO * 256 + 256;
                        int32_t hi = si < 2u ? pat.d_split[si + 1u] - 256 : SD_R_HI * 256 - 256;
                        int32_t v = (int32_t)pat.d_split[si] + enc_far(e, (SD_R_HI - SD_R_LO) * 4u) * 64;   /* a quarter semitone a detent */
                        pat.d_split[si] = (uint16_t)(v < lo ? lo : v > hi ? hi : v);
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
            } else if (page == 1u && r_stages) {   /* STAGES: TYPE PITCH WIDTH LEVEL; GLO: F1..F4 */
                SdReactorStage *st = &pat.stage[rst_cur % 6u];
                static const uint8_t KN[4] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
                if (l2) {
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
            } else if (page == 1u) {            /* the Reactor */
                if (l2) {
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
            } else if (page == 0u && l2) {      /* NAV, GLO: X Y WIDTH, K4 a harmonic generator a detent (its list) */
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

        voices_guard();
        eng.rec_trk = seq_rec || seq_page() ? (uint8_t)seq_trk : 0xFFu;   /* REC, or SEQ's page: the keys may take the
                                                   track's voice (you hear what you record, or try a line over it) */
        midi_in();
        batt_poll();                            /* (the battery, once a second) */
        eng.xkey = (uint8_t)!set.xchrom;        /* the transposes IN KEY (SETUP's XPOSE) */
        eng.master = set.master;                /* the track whose one leads (SETUP's MASTER) */
        eng.sp_freeze = sp_frz;                 /* SPACE's FREEZE (ALGORITHM there) */
        eng.sp_pitch = sp_pitch;                /* its PITCH (GLO + K2 there) */
        eng.lp_len = lp_len;                    /* LOOPER's (EDIT there arms it or not: L-LEVEL) */
        eng.lp_speed = lp_speed;
        eng.lp_glide = lp_glide;
        eng.lp_lvl = lp_arm ? lp_lvl : 0;       /* (disarmed: none, so the engine's own rule, a level arms, holds) */
        eng.lp_arm = lp_arm;
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
#if FM1_CPU1
        {                                       /* the second core off (at the start, or it gave up): said once */
            static uint8_t c1_said;
            if (eng.sp_room && !c1_said) c1_said = 1, msg("2ND CORE OFF: ROOM");
        }
#endif
        eng.arp_mode = set.amode; eng.arp_div = set.adiv; eng.arp_oct = set.aoct;
        eng.arp_gate = set.agate; eng.arp_hold = set.ahold;

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
            fm1_led_key(BTN_GLO, shift());      /* GLO lit: the second layer (held or latched) */
            fm1_led_key(BTN_HOME, page == 0u || page == 5u);   /* a page's button lit: you are there */
            fm1_led_key(BTN_FX, (page >= 1u && page <= 3u) || page == PAGE_TIME);
            fm1_led_key(BTN_ENV, page == PAGE_ENV || page == PAGE_FILT);
            fm1_led_key(BTN_LFO, page == 4u);
            fm1_led_key(BTN_ARP, page == PAGE_ARP || page == PAGE_LOOP);
            fm1_led_key(BTN_SEQ, seq_page() || page == PAGE_ORB);
            fm1_led_key(BTN_EDIT, browse_at || sw == SW_WAIT ? (fm1_ms / 200u) & 1u : sw == SW_ON || sw == SW_LOOP);   /* EDIT lit: what this page shows
                                                   is on (the list open: it blinks, it loads) */
            fm1_led_key(BTN_REC, seq_rec);
        }

        if (saved_at && fm1_ms - saved_at > 1500u) saved_at = 0;
        if (browse_at && (fm1_ms - browse_at >= BROWSE_MS || save_mode)) browse_at = 0;   /* untouched: not now */
        if (dlist_at && (fm1_ms - dlist_at >= DLIST_MS || page != dlist_pg || browse_at || save_mode)) dlist_at = 0;
        if (fm1_ms - frame_at >= 40u) {         /* the screen: 25 frames a second, each region sent when it changed */
            frame_at = fm1_ms;
            idle_ok = 1;
            draw_frame();
            idle_ok = 0;
        }

        if (save_pending && (sd_engine_silent(&eng) || fm1_ms - save_at > 3000u)) {   /* quiet (or 3 s on): write */
            if (save_pending == 2u) {
                slot_save(1, &useq, USEQ_HALF);
                slot_save(2, (uint8_t *)&useq + USEQ_HALF, sizeof useq - USEQ_HALF);
                slot_save(3, &uorb, sizeof uorb);
            }
            else slot_save(0, &upat, sizeof upat);
            save_pending = 0;
            saved_at = fm1_ms;
            dirty = 1;
            t_change = fm1_ms - 2000u;
        }
        if (dirty && !eng.seq_run && sd_engine_silent(&eng) && fm1_ms - t_change > 2000u) {   /* silent: an erase stalls the sound */
            settings_save(&set, sizeof set);
            dirty = 0;
        }
#ifdef DINGHY_WEB
    } while (0);
#else
    }
#endif
}

#ifdef DINGHY_WEB
#include "../../web/app/web_lab.h"              /* the lab (web/lab/): the patch opened up to the page */
#endif
