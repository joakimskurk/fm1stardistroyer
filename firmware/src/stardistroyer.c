/* SPDX-License-Identifier: GPL-3.0-only */
/* Star Distroyer: a starship FM synth with a morphing resonator and multiband destruction, for the M-VAVE FM-1.
 * Built on Dinghy 0.5 (platform.c: updates, the USB rescue, MIDI, keys, knobs, LEDs, the screen, flash).
 *
 * V3 (Sound Plan v0.1 §6): four banks of four oscillators, in the sound engine
 * (sd_engine.h, 32-frame sub-blocks), the Reactor, DISTROY, SPACE, a lowpass a voice, the matrix, the macros, two
 * sequencer tracks, the arpeggiator.
 *
 * The controls, the same on every page (2026-11-07, the user: "functionality on knobs and buttons must recur across
 * the whole app"; "K1-4 lie in a row: we need an indication"; "EDIT a toggle"; "SCL held: the second function"):
 *   a page button  its page; again, the next of its pages (the top line shows them, this one a red tab):
 *                  HOME  NAV -> MACROS        FX   REACTOR -> DISTROY -> SPACE      ENV  ENV -> FILTER
 *                  LFO   LFO 1 -> LFO 2 -> TURING -> MOD   ARP  ARP     SEQ  SEQ A -> SEQ B -> ORBIT
 *   K1..K4     the four values in the row under the top line, left to right as the knobs
 *   SCL held   their second layer (the names turn red); on SEQ the keys transpose the track, at its one: IN KEY
 *              (the default: in SEQ A's scale from its key, so B keeps to A's key) or CHROM, SCL + EDIT there;
 *              SCL + SEQ there (and on ORBIT): the MASTER, A or B, the track whose one leads (its tab underlined)
 *   ALGORITHM  the page's choice, said on the hint line (SCL + ALGORITHM: the second)
 *   EDIT       ON / OFF of what the page shows (the top line's right end; EDIT's light: on): COURSE, REACTOR, DISTROY,
 *              SPACE, FILTER (fx_off: the settings kept), ARP, a track (SEQ B: not in MONO), ORBIT, TURING. While
 *              the master plays, the other track and ORBIT switched on come in at its one (ON blinks till then),
 *              from their step 1
 *   PRESETS    the patch (on SEQ the pattern, P01..P32): it turns a list, the sound as it is; EDIT loads, HOME not
 *              now (2026-11-09)
 *   SELECT     the tempo, 40..240 BPM (a MIDI clock's while one leads)      GLO the mode: MONO -> DUO -> POLY
 *   OCT- OCT+  the keys' octave, on every page, REC or not; on SEQ, SCL + OCT the step bank (2026-11-10, the user:
 *              on SEQ the octave took going into REC, and a recording came back in another octave: "make it smoother");
 *              on TIME, SCL + OCT the loop's octave
 *   PLAY       the patterns on / off (SCL + PLAY: SYNC AUTO / INT, MIDI in)   REC recording into the track SEQ edits
 *              (ENV a TIE, LFO a REST); what is heard is written: a key under the track's transpose (SCL + a key,
 *              ORBIT's XPOSE) goes in back by it, so the step plays the key's note (sd_seq_rec_note)
 *   SAVE       the patch (on SEQ the pattern) to a user place: ALGORITHM picks it, SAVE again writes, HOME: not now
 *   the keys   play; their lights show what they do (keys_lit). On SEQ the white keys are the bank's 16
 *              steps (a step tapped again: RATCHET, PROB), the black keys RND UNDO CLR and, the last one, SEG
 *   the voices MONO 1, DUO 2, POLY 4, shared by quota (sd_plan): a playing track its widest chord, A first, then B,
 *              then ARP one, the keys the rest; no source turns off or takes another's note. ARP and SEQ B are refused
 *              where they do not fit (MONO with SEQ A playing: no ARP, no keys but REC's), and switched off when PLAY,
 *              GLO or a chord takes their room; a message says why. The status line shows who holds them (A B R -)
 *
 * The pages' knobs (SCL: the second layer):
 *   NAV     XM WARP FOLD DETUNE (the Q page, 0..99); SCL: X Y WIDTH, K4 a harmonic generator on the
 *              bank (ALL ODD RND CLUST, 14 chords, CPY). ALGORITHM the bank's roles (SSSC PSSC PPPC SCSC PPCC CSSC
 *              SCCC CCCC); SCL + ALGORITHM the bank, A B C D. EDIT: COURSE
 *   MACROS     MUTATE MOVE BREAK MACRO 4; ALGORITHM MACRO 4's kind (BODY AIR DROWN); the picture: the chain, ON / OFF
 *   REACTOR      POS RES SLEW DRIVE; SCL: FREQ MODEL DIV MIX; ALGORITHM TRACK (OFF KEY HARM KEY+H), SCL + ALGO LOW
 *   DISTROY    ALGORITHM the band 1..4 (LOW LO-MID HI-MID TOP, on the hint line), then LOOP. A band: TYPE (NONE
 *              SOFT HARD FOLD CRUSH) DRIVE, ALT by what it does for the type (KNEE SPACING DEPTH), LEVEL; SCL: BIAS
 *              PAN DYNAMIC (PHAT .. SHARP) SPLIT, SCL + ALGORITHM its BODY (2026-11-09, the user: the page was "hard to
 *              navigate and understand"). LOOP: FDBK TIME SPREAD TAME; SCL: MIX XOVER. EDIT's first time on a patch
 *              with no bands: four that sound (dest_first)
 *   SPACE      the reverb: SIZE DECAY TONE R-LEVEL; SCL: SHIMMER. On the second core a plate (2026-11-10), on one
 *              core the room. ALGORITHM: FREEZE (the player's, not saved: the plate's input closed, its tail held for
 *              ever); SCL + ALGORITHM: SHIMMER (an octave up of the tail back into it, 0..99, s_mode's top bits)
 *   TIME       the delay (2026-11-11, its own page): TIME FDBK TONE D-LEVEL; SCL: the looper's GLIDE LENGTH SPEED
 *              L-LEVEL, SCL + EDIT the delay's MODE (FREE SYNC FREE PP SYNC PP). ALGORITHM: LOOP (the player's: the
 *              looper, always recording what SPACE hears before the reverb, stops and plays its last LENGTH round, at
 *              its SPEED, +-12 semitones; into the reverb as the synth goes; the delay goes on); SCL + ALGORITHM:
 *              REVERSE (it plays it backwards); SCL + OCT: SPEED to the next octave down or up, over GLIDE (OFF: at
 *              once), the loop landing on its own grid (2026-11-11, the user: "if the loop is transposed a whole
 *              octave, it should find the perfect grid again"; "control over how long it takes"). L-LEVEL above 0
 *              arms the looper (0 at the start): it borrows the delay line's second half (5.1 s with the RAM's last 24 KB,
 *              mono mu-law), the delay then 0.5 s
 *   ENV        ENV 1 (the level): ATTACK DECAY SUS RELEASE; SCL: ENV 2 (the FM's amount)
 *   FILTER     a lowpass on each voice: CUTOFF RES ENV (ENV 3's depth) KEY; SCL: ENV 3; ALGORITHM LP12 / LP24,
 *              SCL + ALGORITHM VEL
 *   LFO 1, 2   SHAPE RATE MODE (FREE SYNC FREE+K SYNC+K FREE 1 SYNC 1); the picture: where the matrix sends it
 *   TURING     a Turing machine, a modulation source of the patch (2026-11-09, the user: "more like a modulation
 *              source, to parameters in the synth, the filter"): a loop of bits on the tempo, ALGORITHM its DIV.
 *              K1 CHANGE (the chance a bit flips as it comes round: 0 a locked loop), K2 LENGTH (2..16 steps), K3 DEST
 *              (a matrix destination, or NOTES: SEQ A's notes moved in its scale, the pattern untouched; to PITCH
 *              quantized to SEQ A's key first, sd_tq_snap), K4 AMOUNT;
 *              EDIT on / off (off by default; on the first time: to XM, 30 %). In MOD it is a SOURCE too
 *   MOD        the slot ALGORITHM picks (1..8): SOURCE DEST AMOUNT. DEST (here, on TURING and ORBIT's lanes): 49 in
 *              the chain's order, the voice, its three envelopes' times, the filter, the Reactor, DISTROY, SPACE, the
 *              looper's SPEED (free; TURING's in semitones, 2026-11-11), the LFOs' rates; while it turns a list in the knobs' place, as PRESETS' (2026-11-10, draw_dlist)
 *   ARP        MODE (UP DOWN UP-DN RND ORDER) RATE (1/64 .. 1/2, triplets, dotted) OCTAVES GATE; ALGORITHM HOLD. The
 *              player's, not the patch's. REC there: KEEP, what it just played (the last bar at its RATE) as a SEQ
 *              track's pattern, SEQ A's (B's while A plays), UNDO on SEQ (arp_keep). While the patterns play its
 *              steps fall on their grid from START (a hand under a quarter step late plays at once); stopped, from
 *              the key
 *   SEQ A, B   two tracks on the one sound, each its pattern and LENGTH (a melody and a bass line). The step the
 *              white key picks: NOTE VEL GATE TYPE (NOTE SLIDE TIE REST SKIP); ALGORITHM the next step. SCL: LENGTH
 *              DIV SWING XPOSE; SCL + ALGORITHM the SCALE. SEG: the white keys cut and join, the black A-F and EVEN,
 *              K1 REPEAT K2 ORDER (FWD REV P-P RND) K3 SKIP K4 LENGTH. REC: keys held together, a chord
 *   ORBIT      a sub-sequencer of the master's pattern (2026-11-09): 2..8 steps on the first white keys, each RATE
 *              loops of it; the step the key (or ALGORITHM) picks: K1 XPOSE A, K2 XPOSE B, K3 LANE 1, K4 LANE 2 (each its DEST,
 *              -100..100 %); SCL: the macros' changes (MUTATE MOVE BREAK MACRO 4, -99..99). The last black key: SETUP,
 *              K1 LENGTH K2 RATE (1 2 4 loops) K3 DEST 1 K4 DEST 2; SCL there: GLIDE for lane 1, lane 2, the macros.
 *              EDIT on / off. Every value a change from where the player is (0 none); saved with the pattern
 *   MIDI in    notes from USB or the TRS jack, any channel: they play as the keys do, on every page (ARP takes them
 *              while on), and REC writes them as the keys', at their velocity (midi_note); CC 1 the matrix's WHEEL,
 *              channel pressure its AFTER. A clock (24 a quarter) leads the tempo while it comes (SYNC AUTO; SCL +
 *              PLAY: INT, not heard): the tracks, ARP, TURING, the synced LFOs and MOVE move by its pulses
 *              (sd_clk_tick); START / CONTINUE play from step 1 at the next pulse, STOP stops; the status line shows
 *              its tempo, CLK
 *   MASTER     the volume (platform.c)
 *
 * CPU on the screen: the worst audio interrupt of the last second, in % of its period (256 frames, 5.8 ms).
 * Dinghy's rule is under half (docs/RULES.md). On the host it reads 0: there tools/sd_render.c measures. */
#define APP_NAME "STAR DISTROYER"
static void app_idle(void);
#define LCD_IDLE app_idle                       /* while the screen's DMA runs: MIDI in (platform.c) */
#define CV_MAX (240u * 120u)                    /* the splash in two halves; the nav panel (57.6 KB of the pool) */
#if !defined(DINGHY_HOST) && !defined(SD_ONE_CORE)
#define FM1_CPU1 1                              /* the second core (hal/fm1_cpu1.h): SPACE's plate runs there (a test
                                                   build: DINGHY_DEFS=-DSD_ONE_CORE ./build.sh, one core: the room) */
#endif
#include "platform.c"
#include "sd_engine.h"
#include "sd_logo.h"

static SdEngine eng;
/* SPACE's memory, ~213 KB: the pool (as gfx.c's canvas; the host on macOS: Mach-O has no .pool section) */
#if defined(DINGHY_HOST) && (defined(__APPLE__) || defined(DINGHY_WEB))
static SdSpaceMem space_mem;
#else
static SdSpaceMem space_mem __attribute__((section(".pool")));
#endif
/* the looper's memory (TIME's, 2026-11-11, then "borrow the delay"): mono at half rate, a byte a sample (mu-law),
   in two pieces of one ring: the delay line's second half while the looper is armed (L-LEVEL above 0: the delay
   then has 0.5 s), and what is left of the RAM, 24 KB: 5.1 s. The test build DINGHY_DEFS=-DSD_RAM2=1 looks for a
   second RAM bank at boot (hal/fm1_ram2.h) and, found, gives the looper the largest of 512 KiB .. 4 MiB it holds
   there instead, the delay keeping its whole line */
#define SD_LP_INTB 24576u
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

/* SPACE's plate on the second core (2026-11-11, the user: "the plate on the other core, with a fallback to the old one
 * if anything fails"; "never the plate on core 0: there we have the original"): a block a job, handed over at its
 * end in the audio interrupt and joined in the next (sd_plate_post, sd_plate_join). Its pointers are constants here,
 * as the core may read RAM wrong. When the core cannot run (barred since a failure, silent at the start) or gives
 * up, the block is dropped (sd_plate_drop: its tail fades out) and the room takes the plate's place (eng.sp_room).
 * On a computer (the host, the page's player) there are no hooks: the plate runs in the render */
#if !defined(DINGHY_HOST) && !FM1_CPU1
static int sd_c1_post(void *e, uint32_t b) { (void)e; (void)b; eng.sp_room = 1; return 0; }   /* (one core) */
static int sd_c1_join(void *e) { (void)e; eng.sp_room = 1; return -1; }
#endif
#if FM1_CPU1
volatile uint32_t sd_c1_load[3];                /* its jobs, timer ticks: the longest, all, how many (for fm1-emu's
                                                   --dump: tools/emu_cpu.sh) */
static void sd_c1_plate(uint32_t b)
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
static int sd_c1_post(void *e, uint32_t b)
{
    (void)e;
    if (fm1_cpu1_run(sd_c1_plate, b)) return 1;
    eng.sp_room = 1;
    return 0;
}
static int sd_c1_join(void *e)
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

static void app_audio(int32_t *out, uint32_t n)
{
    uint32_t t0 = sd_ticks(), dt;
    sd_engine_render(&eng, out, n);
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
#define C_RED RGB(235, 28, 28)                  /* accents, the playing step, a held key */
#define C_RED_D RGB(118, 14, 14)
#define C_FILL RGB(58, 6, 8)                    /* the nav's mix, filled */
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

static void cv_slashes(int32_t x, int32_t y, int32_t h, uint16_t c)   /* "///" h tall */
{
    int32_t k, t;
    for (k = 0; k < 3; k++)
        for (t = 0; t < 3; t++) cv_line(x + k * 6 + t + h / 2, y, x + k * 6 + t, y + h - 1, c);
}

static void cv_frame(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)   /* a panel: corners cut */
{
    cv_line(x + 3, y, x + w - 4, y, c); cv_line(x + 3, y + h - 1, x + w - 4, y + h - 1, c);
    cv_line(x, y + 3, x, y + h - 4, c); cv_line(x + w - 1, y + 3, x + w - 1, y + h - 4, c);
    cv_line(x, y + 3, x + 3, y, c); cv_line(x + w - 4, y, x + w - 1, y + 3, c);
    cv_line(x, y + h - 4, x + 3, y + h - 1, c); cv_line(x + w - 4, y + h - 1, x + w - 1, y + h - 4, c);
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
 * main loop draws its frames a pass at a time: splash_begin, then splash_draw.) */
#define SPLASH_MS 1500u
#define STARS 56u
static int32_t sx[STARS], sy[STARS], sv[STARS];
static void splash_begin(void)
{
    uint32_t seed = 0x5D15u, i;
    for (i = 0; i < STARS; i++) {
        seed = seed * 1103515245u + 12345u; sx[i] = (int32_t)(seed >> 8 & 0xFFFFu) % (240 << 4);
        seed = seed * 1103515245u + 12345u; sy[i] = (int32_t)(seed >> 8 & 0xFFFFu) % 240;
        seed = seed * 1103515245u + 12345u; sv[i] = 6 + (int32_t)(seed >> 8 & 0xFFFFu) % 70;   /* 1/16 px a ms */
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
            for (i = 0; i < STARS; i++) {       /* streaming right to left, the fast ones long and bright */
                int32_t x = (sx[i] - sv[i] * (int32_t)now) % (240 << 4), len = sv[i] / 8;
                uint32_t a = (uint32_t)(1 + sv[i] / 12);
                if (x < 0) x += 240 << 4;
                x >>= 4;
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

/* ---- the patch: the Q page's four knobs, endless, 0..99 (a detent a step) */
static uint32_t q99(uint32_t v) { return (v * 99u + 32767u) / 65535u; }
static uint16_t knob_q(uint16_t v, int32_t e)
{
    int32_t n = (int32_t)q99(v) + e;
    n = n < 0 ? 0 : n > 99 ? 99 : n;
    return (uint16_t)((uint32_t)n * 65535u / 99u);
}

/* ---- MIDI in: from the main loop, and from app_idle while the screen draws (idle_ok) */
static struct { uint8_t oct, bpm, patch, mode, pattern, amode, adiv, aoct, agate, ahold, xchrom, sync_int, master; } set =
    {0, 120, SD_PATCH_WORST, SD_MODE_MONO, 0, SD_AM_UP, 2, 1, 50, 0, 0, 0, 0};   /* (xchrom: the transposes in
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
/* the patterns in two slots (1, 2) since the chords (2026-11-06): 32 of them pass one slot's 16 KB */
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
static uint32_t orb_cur, orb_setup;             /* the step the keys picked; SETUP (the last black key) */
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
static uint32_t ebank, gen = 0xFFFFFFFFu, gen_seed = 1;   /* the edited bank; the last generator (K4, SCL) */
static uint32_t page;                           /* 0 NAV, 1 REACTOR, 2 DISTROY, 3 SPACE, 4 MOD, 5 HOME (the macros),
                                                   6 SEQ, 7 ENV, 8 FILTER, 9 ARP: each its button's (2026-11-06) */
#define PAGE_SEQ 6u
#define PAGE_ENV 7u
#define PAGE_FILT 8u
#define PAGE_ARP 9u
#define PAGE_ORB 10u                            /* SEQ's third: ORBIT (2026-11-09) */
#define PAGE_TIME 11u                           /* FX's fourth: TIME, the delay and the looper (2026-11-11) */
/* the screen's rows (the frame, below) */
#define Y_NAV 44u
#define Y_KNOB 64u
#define H_KNOB 50u
#define Y_PIC 116u
#define H_PIC 68u
#define Y_HINT 186u
#define Y_STEPS 204u
#define Y_STAT 222u
static uint32_t dslot;                          /* the MOD slot last picked */
static uint32_t seq_trk;                        /* the track SEQ edits: 0 A, 1 B */
static SdPattern seqp, seq_undo;                /* the pattern as the keys left it; the one before the last RND, CLR */
static uint32_t seq_has_undo, seq_cur, seq_bank, seq_segmode, seq_dseg, seq_rec, seq_clr_at;
static uint32_t seq_more, seq_tapped = 0xFFu;  /* STEP's second layer: RATCHET, PROB (a step tapped twice); the
                                                   step last tapped */
static uint32_t seq_rng = 0x5D5Du;
static const char *const STYPE_NAME[SD_STYPES] = {"NOTE", "SLIDE", "TIE", "REST", "SKIP"};
static const char *const SORDER_NAME[SD_SORDERS] = {"FWD", "REV", "P-P", "RND"};
static const char *const SORDER_GLYPH[SD_SORDERS] = {"", "<", "<>", "?"};
static const char *const SDIV_NAME[SD_SDIVS] = {"1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4", "1/64", "1/32T",
                                                "1/16D", "1/4T", "1/8D", "1/2"};
static const char *const SCALE_NAME[SD_SCALES] = {"MINOR", "MAJOR", "DORIAN", "PHRYG", "PENTA-", "PENTA+", "CHROM"};
static uint32_t dmod;                           /* LFO's page: slots 0..7 (MOD, ALGORITHM picks), 8 LFO 1, 9 LFO 2,
                                                   10 TURING */
#define DMOD_TUR (SD_MODS + 2u)
static const char *const SRC_NAME[SD_MSRCS] = {"OFF", "ENV1", "ENV2", "LFO1", "LFO2", "NAV X", "NAV Y", "VEL", "KEY",
                                               "WHEEL", "AFTER", "RAND", "STEP", "SEGMENT", "D LOW",
                                               "D TOP", "R STAGE", "ECHO", "ENV3", "TURING"};
static const char *const SRC_SHORT[SD_MSRCS] = {"--", "E1", "E2", "L1", "L2", "NX", "NY", "VL", "KY", "WH", "AT",
                                                "RN", "ST", "SG", "DL", "DT", "RS", "EC", "E3", "TU"};   /* two letters: a slot's
                                                   cell is 57 px */
static const char *const DST_NAME[SD_MDSTS] = {"OFF", "PITCH", "XM", "WP", "FD", "DT", "X", "Y", "WIDTH", "R POS",
                                               "R FREQ", "R RES", "R DRIVE", "R MIX", "D GAIN", "D BIAS", "D FDBK",
                                               "D TAME", "D MIX", "S FDBK", "S D-LVL", "S R-LVL", "S DECAY",
                                               "D HIGH", "D SPLIT", "F CUT", "F RES", "E1 ATK", "E1 DEC", "E1 SUS",
                                               "E1 REL", "E2 ATK", "E2 DEC", "E2 SUS", "E2 REL", "E3 ATK", "E3 DEC",
                                               "E3 SUS", "E3 REL", "F ENV", "R SLEW", "D TIME", "D SPRD", "S TIME",
                                               "S DTONE", "S SIZE", "S RTONE", "L1 RATE", "L2 RATE", "L SPEED"};
static const char *const DST_SHORT[SD_MDSTS] = {"--", "PIT", "XM", "WP", "FD", "DT", "X", "Y", "WID", "POS", "RFQ",
                                                "RES", "DRV", "RMX", "GAN", "BIA", "DFB", "TAM", "DMX", "SFB", "DLV",
                                                "RLV", "DCY", "DHI", "DSP", "CUT", "FRS", "A1", "D1", "S1", "R1", "A2",
                                                "D2", "S2", "R2", "A3", "D3", "S3", "R3", "FEN", "SLW", "DTM", "SPR",
                                                "STM", "DTO", "SIZ", "RTO", "L1R", "L2R", "LSP"};
/* the destinations' list (2026-11-10, the user: "as a list, as the patches when I browse, so it is easier to see
   and hit what you look for"): each its engine and its name there, in SD_MD_ORDER (the chain's order) */
enum { DG_NONE, DG_VOICE, DG_NAV, DG_ENV1, DG_ENV2, DG_FILT, DG_ENV3, DG_REACT, DG_DEST, DG_SPACE, DG_LFO, DG_SEQ,
       DG_LOOP };
static const char *const DG_NAME[] = {"", "VOICE", "NAV", "ENV 1", "ENV 2", "FILTER", "ENV 3", "REACTOR", "DISTROY",
                                      "SPACE", "LFO", "SEQ A", "LOOPER"};
static const uint8_t DST_GRP[SD_MDSTS] = {DG_NONE, DG_VOICE, DG_NAV, DG_NAV, DG_NAV, DG_NAV, DG_NAV, DG_NAV, DG_NAV,
    DG_REACT, DG_REACT, DG_REACT, DG_REACT, DG_REACT, DG_DEST, DG_DEST, DG_DEST, DG_DEST, DG_DEST, DG_SPACE,
    DG_SPACE, DG_SPACE, DG_SPACE, DG_DEST, DG_DEST, DG_FILT, DG_FILT, DG_ENV1, DG_ENV1, DG_ENV1, DG_ENV1, DG_ENV2,
    DG_ENV2, DG_ENV2, DG_ENV2, DG_ENV3, DG_ENV3, DG_ENV3, DG_ENV3, DG_FILT, DG_REACT, DG_DEST, DG_DEST, DG_SPACE,
    DG_SPACE, DG_SPACE, DG_SPACE, DG_LFO, DG_LFO, DG_LOOP};
static const char *const DST_PARAM[SD_MDSTS] = {"OFF", "PITCH", "XM", "WARP", "FOLD", "DETUNE", "X", "Y", "WIDTH",
    "POS", "FREQ", "RES", "DRIVE", "MIX", "GAIN", "BIAS", "LOOP FDBK", "TAME", "MIX", "FDBK", "D-LEVEL", "R-LEVEL",
    "DECAY", "TOP GAIN", "SPLIT", "CUTOFF", "RES", "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "ATTACK", "DECAY",
    "SUSTAIN", "RELEASE", "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "ENV", "SLEW", "LOOP TIME", "SPREAD", "TIME",
    "D-TONE", "SIZE", "R-TONE", "1 RATE", "2 RATE", "SPEED"};
static const char *tdest_name(uint32_t d)       /* TURING's DEST: a matrix destination, or NOTES */
{
    return d == SD_TD_NOTES ? "NOTES" : d < SD_MDSTS ? DST_NAME[d] : "OFF";
}
static const char *const LSHAPE_NAME[SD_LSHAPES] = {"SINE", "TRI", "SAW", "RAMP", "SQUARE", "S&H"};
#define LMODES 6u
static const char *const LMODE_NAME[LMODES] = {"FREE", "SYNC", "FREE+K", "SYNC+K", "FREE 1", "SYNC 1"};
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
/* DISTROY's page (2026-11-09, the user: "hard to navigate and understand"): what ALT does for each type, the bands'
   names (and their short ones, for a narrow band in the picture) */
static const char *const ALT_NAME[SD_DSHAPES] = {"--", "KNEE", "KNEE", "SPACING", "DEPTH"};
static const char *const BAND_NAME[4] = {"LOW", "LO-MID", "HI-MID", "TOP"};
static const char *const BAND_SHORT[4] = {"L", "LM", "HM", "T"};
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
static uint32_t dst_step(uint32_t d, int32_t e, uint32_t notes)   /* the next in the list (notes: TURING's NOTES at
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

static uint32_t shift(void) { return fm1_in.buttons >> BTN_SEL & 1u; }   /* SCL (BTN_SEL) held: the second layer */
static uint8_t sp_frz;                          /* SPACE's FREEZE (ALGORITHM there): the player's, not the patch's */
/* TIME's looper, the player's (not saved with the patch): LOOP (ALGORITHM on TIME), LENGTH (as TIME: ms, or a
   division with SYNC), its speed (semitones x 256), its level */
static uint8_t lp_on, lp_rev;                   /* (lp_rev: REVERSE, SCL + ALGORITHM on TIME) */
static uint16_t lp_len = (uint16_t)(5u * 65536u / SD_LP_NDIV + 100u), lp_lvl;   /* (1/8, or 334 ms free; L-LEVEL 0:
                                                   the looper not armed, the delay its whole line) */
static int16_t lp_speed;
static uint16_t lp_glide;                       /* GLIDE: OFF (a new SPEED at once), else how long it takes */
static const char *msg_txt;                     /* a message in the hint's place, 2 s */
static uint32_t msg_at;
static void msg(const char *t) { msg_txt = t; msg_at = fm1_ms ? fm1_ms : 1u; }

/* ---- the voices (2026-11-07, the user: "notes get cancelled, especially with ARP: rules for what can play and what
 * cannot, and the functions fitted around them"; "no ARP while SEQ is on and playing in MONO, the same for SEQ B: only
 * DUO / POLY can fix it, and there too rules, so we never try to play more than is possible; simple messages in
 * English when you try"). The engine gives each source its quota (sd_plan: a playing track its widest chord, A
 * first, then B, then ARP one, the hand the rest) and no source takes another's voice. Here what is switched on is
 * kept inside the voices: ARP and SEQ B are refused when there is no room (a message says why), and switched off
 * when PLAY, GLO or a new chord takes their room; a chord is recorded only as wide as the room left. */
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


static void draw_header(void)                   /* the logo; the patch's number on the right */
{
    char s[8], *p = s;
    cv_begin(240, 20, C_BG);
    draw_logo(&SD_LOGO_HEADER, 6, 5);
    *p++ = 'P';
    if (set.patch >= SD_PATCHES) { p[-1] = 'U'; if (set.patch - SD_PATCHES < 9u) *p++ = '0'; p = put_u(p, set.patch - SD_PATCHES + 1u); }
    else { if (set.patch < 9u) *p++ = '0'; p = put_u(p, set.patch + 1u); }
    *p = 0;
    cv_text(232 - text_w(&FONT_S, s), 2, &FONT_S, s, C_ICE);
    cv_line(0, 19, 239, 19, C_ICE_D);
}

static void draw_info(void)                     /* the patch as a red tab, the edited bank's roles, the mode */
{
    char s[16], *p = s, r[SD_OPS + 1u];
    int32_t w = text_w(&FONT_S, patch_name(set.patch));
    cv_begin(240, 20, C_BG);
    cv_rect(0, 1, w + 22, 18, C_RED);
    cv_line(w + 22, 1, w + 30, 1, C_RED);       /* the tab's slanted end */
    for (int32_t k = 0; k < 18; k++) cv_line(w + 22, 1 + k, w + 30 - k * 8 / 18, 1 + k, C_RED);
    cv_text(10, 2, &FONT_S, patch_name(set.patch), C_BG);
    roles_of(&pat, r);
    *p++ = (char)('A' + ebank);
    *p++ = ' ';
    p = put(p, r);
    *p = 0;
    cv_text(w + 40, 2, &FONT_S, s, C_ICE);
    p = put(s, MODE_NAME[eng.mode_on < SD_MODES ? eng.mode_on : 0]);
    *p = 0;
    cv_text(232 - text_w(&FONT_S, s), 2, &FONT_S, s, eng.mode_on == SD_MODE_MONO ? C_ICE_D : C_RED);
}

/* PRESETS turned: the list over the panel, the sound as it is (2026-11-09, the user: "browse the presets without
 * them changing at once"): the one picked in the middle as a red tab, two either side (patches past the empty user
 * places, as the knob steps; on SEQ and ORBIT the patterns), the one playing marked NOW, the place in the whole list on
 * the right. EDIT loads it (its light blinks meanwhile), HOME or 8 s untouched: not now; another button closes it. */
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
static void draw_browse(void)                   /* over the knobs and the picture: 120 rows */
{
    char s[8];
    uint32_t r, n = browse_to, total = 0, at = 0, k, pt = browse_kind == 2u, now = pt ? set.pattern : set.patch;
    int32_t y, by;
    cv_begin(240, 120, C_BG);
    cv_frame(2, 0, 236, 120, C_RED);
    for (k = 0; k < 2u; k++) n = browse_step(n, -1);
    for (r = 0; r < 5u; r++, n = browse_step(n, 1)) {
        const char *nm = pt ? (useq.used >> n & 1u ? "SAVED" : "EMPTY") : patch_name(n);
        y = 2 + (int32_t)r * 19;
        if (pt) { *s = 'P'; *put_u(s + (n < 9u ? 2 : 1), n + 1u) = 0; if (n < 9u) s[1] = '0'; }
        else *put_pnum(s, n) = 0;
        if (r == 2u) {
            int32_t w = 64 + text_w(&FONT_S, nm);
            for (k = 0; k < 18u; k++) cv_line(6, y + (int32_t)k, 6 + w + 8 - (int32_t)k * 8 / 18, y + (int32_t)k, C_RED);
            cv_text(14, y + 1, &FONT_S, s, C_BG);
            cv_text(60, y + 1, &FONT_S, nm, C_BG);
        } else {
            cv_text(14, y + 1, &FONT_S, s, C_ICE_D);
            cv_text(60, y + 1, &FONT_S, nm, r == 1u || r == 3u ? C_ICE : C_ICE_D);
        }
        if (n == now) cv_text(190, y + 1, &FONT_S, "NOW", r == 2u ? C_ICE : C_ICE_D);
    }
    cv_text(10, 100, &FONT_S, "EDIT: LOAD  HOME: NOT NOW", C_ICE_D);
    if (pt) { total = UPATTERNS; at = browse_to; }
    else for (k = 0; k < NPATCHES; k++) if (patch_ok(k)) { if (k == browse_to) at = total; total++; }
    cv_rect(228, 6, 3, 90, C_GRID);             /* where in the list */
    by = 6 + (int32_t)(at * 78u / (total > 1u ? total - 1u : 1u));
    cv_rect(227, by, 5, 12, C_RED);
}

/* The destinations' list: while a DEST knob turns (MOD's, TURING's, ORBIT's lanes), over the knobs and the picture
 * as PRESETS' list is: the one picked in the middle, two either side, each its engine and its name, the place in the
 * whole list on the right. It plays as it turns (nothing to load); 1.5 s untouched, another knob, a button or
 * another page: gone */
enum { DL_MOD = 1, DL_TUR, DL_ORB1, DL_ORB2 };
#define DLIST_MS 1500u
static uint32_t dlist_at, dlist_kind, dlist_pg;
static void dlist_open(uint32_t kind) { dlist_kind = kind; dlist_pg = page; dlist_at = fm1_ms ? fm1_ms : 1u; }
static uint32_t dlist_cur(void)
{
    switch (dlist_kind) {
    case DL_TUR: return pat.tur_dst;
    case DL_ORB1: return orbp.dst[0];
    case DL_ORB2: return orbp.dst[1];
    default: return dmod < SD_MODS ? pat.mod[dmod].dst : 0u;
    }
}
static void draw_dlist(void)                    /* over the knobs and the picture: 120 rows */
{
    char s[24], *p;
    uint32_t r, k, notes = dlist_kind == DL_TUR, cur = dlist_cur(), n = cur, at;
    int32_t y, by;
    cv_begin(240, 120, C_BG);
    cv_frame(2, 0, 236, 120, C_RED);
    for (k = 0; k < 2u; k++) n = dst_step(n, -1, notes);
    for (r = 0; r < 5u; r++, n = dst_step(n, 1, notes)) {
        const char *g = n == SD_TD_NOTES ? DG_NAME[DG_SEQ] : DG_NAME[DST_GRP[n % SD_MDSTS]];
        const char *nm = n == SD_TD_NOTES ? "NOTES" : DST_PARAM[n % SD_MDSTS];
        y = 2 + (int32_t)r * 19;
        if (r == 2u) {
            int32_t w = 86 + text_w(&FONT_S, nm);
            for (k = 0; k < 18u; k++) cv_line(6, y + (int32_t)k, 6 + w + 8 - (int32_t)k * 8 / 18, y + (int32_t)k, C_RED);
            cv_text(14, y + 1, &FONT_S, g, C_BG);
            cv_text(92, y + 1, &FONT_S, nm, C_BG);
        } else {
            cv_text(14, y + 1, &FONT_S, g, C_ICE_D);
            cv_text(92, y + 1, &FONT_S, nm, r == 1u || r == 3u ? C_ICE : C_ICE_D);
        }
    }
    if (dlist_kind == DL_MOD) p = put_u(put(s, "MOD "), dmod + 1u);
    else p = put(s, dlist_kind == DL_TUR ? "TURING" : dlist_kind == DL_ORB1 ? "ORBIT LANE 1" : "ORBIT LANE 2");
    p = put(p, " DEST"); *p = 0;
    cv_text(10, 100, &FONT_S, s, C_ICE_D);
    at = cur == SD_TD_NOTES ? SD_MDSTS : dst_rank(cur % SD_MDSTS);
    cv_rect(228, 6, 3, 90, C_GRID);             /* where in the list */
    by = 6 + (int32_t)(at * 78u / (SD_MDSTS + notes - 1u));
    cv_rect(227, by, 5, 12, C_RED);
}

static void cv_bar(int32_t x, int32_t y, uint32_t v99)   /* eight segments, the last ones red as they fill */
{
    uint32_t k, n = (v99 * 8u + 49u) / 99u;
    for (k = 0; k < 8u; k++)
        cv_rect(x + (int32_t)k * 6, y, 4, 10, k >= n ? C_GRID : k < 5u ? C_ICE : C_RED);
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
    return eng.lp_borrow && lp_lvl ? SD_SP_LIM2 : SD_SP_LIM;
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
static void seq_keep_undo(void) { seq_undo = seqp; seq_has_undo = 1; }
static void pattern_take(uint32_t i)            /* pattern i into the track SEQ edits (an empty one: the default; on A
                                                   its ORBIT too) */
{
    set.pattern = (uint8_t)i;
    if (useq.used >> i & 1u) seqp = useq.p[i];
    else if (seq_trk) sd_pattern_empty(&seqp);
    else sd_pattern_default(&seqp);
    if (!seq_trk) orb_load(i);
    seq_cur = seq_bank = 0;
    seq_has_undo = 0;
    seq_push();
}
static SdPattern seq_other;                     /* the track SEQ does not edit now: its pattern as last edited */
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
static uint32_t rec_at = 0xFFu, rec_n;         /* REC: the step the held keys write, its notes so far */
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




/* KEEP (REC on ARP, 2026-11-10, the user: "it is so cool to play over the ARP, and sometimes I wish I could save
 * it"): what ARP just played, the last bars at its RATE (sd_arp_keep_len: 1/16 a bar of 16), as a SEQ track's
 * pattern at that DIV, the bar that ended if REC comes on the one. Into SEQ A, or SEQ B while A plays (DUO, POLY:
 * MONO has no ARP over A); the track on, the steps as heard (sd_seq_rec_note), its gates ARP's GATE. UNDO on SEQ takes
 * it back; SAVE there keeps it */
static void arp_keep(void)
{
    static char m[28];
    uint8_t nt[SD_PSTEPS], vl[SD_PSTEPS];
    uint32_t d = set.adiv % SD_SDIVS, n = sd_arp_keep_len(d), t, i;
    char *p;
    if (!sd_arp_keep(&eng, n, nt, vl)) { msg("ARP: NOTHING TO KEEP YET"); return; }
    t = eng.seq_run && !eng.t[0].mute && trk_has_notes(0) && set.mode != SD_MODE_MONO ? 1u : 0u;
    seq_track(t);
    seq_keep_undo();
    {
        uint8_t sc = seqp.scale, sw = seqp.swing;
        sd_pattern_default(&seqp);
        seqp.scale = sc; seqp.swing = sw;
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

/* The keys on SEQ and TURING, REC off: the white keys the steps of the bank (OCT- OCT+ the bank), the black keys
 * RND UNDO CLR and, on the last one, SEG (segments: the white keys cut and join, the black keys A-F and EVEN; SEG
 * again: back). The key LEDs show it (keys_lit). */
#define BK_SEG 10                               /* the last black key (F#5) */
static void seq_keys(uint32_t down)
{
    uint32_t k;
    for (k = 0; k < 27u; k++) {
        int32_t w, bk;
        if (!(down >> k & 1u)) continue;
        w = key_white(k);
        bk = key_black(k);
        if (bk == BK_SEG) {
            seq_segmode = !seq_segmode;
            seq_more = 0;
        } else if (w >= 0) {
            uint32_t i = seq_bank * 16u + (uint32_t)w;
            if (i >= seqp.len) continue;
            if (seq_segmode) {
                int32_t g = sd_seg_toggle(&seqp, i);
                seq_dseg = g >= 0 ? (uint32_t)g : sd_seg_of(&seqp, i);
                seq_push();
            } else {
                if (i == seq_tapped) seq_more = !seq_more;   /* the same step again: its other layer (the
                                                   layer stays while other steps are picked) */
                seq_tapped = i;
                seq_cur = i;
            }
        } else if (seq_segmode) {
            uint8_t bd[SD_SEGS + 1u];
            uint32_t sc = sd_seg_bounds(&seqp, bd);
            if (bk >= 0 && (uint32_t)bk < sc) seq_dseg = (uint32_t)bk;
            else if (bk == 6) { sd_seg_even(&seqp); seq_push(); }
        } else if (bk == 0) {                   /* RND: a new line in the scale, now */
            seq_keep_undo();
            sd_pattern_randomize(&seqp, &seq_rng);
            seq_push();
        } else if (bk == 1 && seq_has_undo) {   /* UNDO: the pattern before the last rewrite (and back) */
            SdPattern t = seqp;
            seqp = seq_undo;
            seq_undo = t;
            seq_push();
        } else if (bk == 2) {                   /* CLR, pressed twice: every step a rest */
            if (seq_clr_at && fm1_ms - seq_clr_at < 3000u) {
                seq_keep_undo();
                for (k = 0; k < SD_PSTEPS; k++) {
                    seqp.step[k].type = SD_ST_REST;
                    seqp.step[k].chord[0] = seqp.step[k].chord[1] = seqp.step[k].chord[2] = 0;
                }
                seq_push();
                seq_clr_at = 0;
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

/* The encoders on ORBIT (every one but PRESETS): the step's values (SCL: its macros), or SETUP's (SCL: GLIDE);
 * ALGORITHM the step */
static void orb_knobs(void)
{
    static const uint8_t KN[4] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
    uint32_t s = orb_cur % SD_ORB_STEPS, k, changed = 0, l2 = shift();
    int32_t e;
    if ((e = fm1_enc_take(ENC_ALGORITHM)) != 0) orb_cur = step_lim(orb_cur, e, SD_ORB_LEN(&orbp));
    for (k = 0; k < 4u; k++) {
        if ((e = fm1_enc_take(KN[k])) == 0) continue;
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
            int32_t v = orbp.mac[k][s] + e;
            orbp.mac[k][s] = (int8_t)(v < -99 ? -99 : v > 99 ? 99 : v);
        } else if (k < 2u) {                    /* XPOSE A, B: -24..24 */
            int8_t *x = k ? &orbp.xb[s] : &orbp.xa[s];
            int32_t v = *x + e;
            *x = (int8_t)(v < -24 ? -24 : v > 24 ? 24 : v);
        } else {                                /* the lanes: -100..100 % */
            int32_t v = orbp.lane[k - 2u][s] + e;
            orbp.lane[k - 2u][s] = (int8_t)(v < -100 ? -100 : v > 100 ? 100 : v);
        }
    }
    if (changed) orb_push();
}

/* The encoders on SEQ (every one but PRESETS). SCL held: K1 LENGTH, K2 DIV, K3 SWING, K4 XPOSE
 * (the track's transpose, taken at its next one, as SCL + a key), ALGORITHM the SCALE */
static void seq_knobs(void)
{
    int32_t e;
    SdStep *st = &seqp.step[seq_cur];
    uint32_t changed = 0;
    if (shift() && !seq_segmode) {
        if ((e = fm1_enc_take(ENC_KNOB1)) != 0) seq_len(e), changed = 1;
        if ((e = fm1_enc_take(ENC_KNOB2)) != 0) { seqp.div = (uint8_t)sdiv_step(seqp.div % SD_SDIVS, e); changed = 1; }
        if ((e = fm1_enc_take(ENC_KNOB3)) != 0) {
            int32_t w = (int32_t)seqp.swing + e;
            seqp.swing = (uint8_t)(w < 50 ? 50 : w > 75 ? 75 : w);
            changed = 1;
        }
        if ((e = fm1_enc_take(ENC_KNOB4)) != 0) {
            int32_t x = eng.t[seq_trk].xpose_next + e;
            eng.t[seq_trk].xpose_next = (int8_t)(x < -24 ? -24 : x > 24 ? 24 : x);
        }
        if ((e = fm1_enc_take(ENC_ALGORITHM)) != 0) seqp.scale = (uint8_t)step_mod(seqp.scale % SD_SCALES, e, SD_SCALES), changed = 1;
    } else if (seq_segmode) {
        uint8_t bd[SD_SEGS + 1u];
        uint32_t sc = sd_seg_bounds(&seqp, bd), g = seq_dseg < sc ? seq_dseg : sc - 1u;
        if ((e = fm1_enc_take(ENC_KNOB1)) != 0) {
            int32_t r = (int32_t)(seqp.seg_rep[g] < 1u ? 1u : seqp.seg_rep[g]) + e;
            seqp.seg_rep[g] = (uint8_t)(r < 1 ? 1 : r > 4 ? 4 : r);
            changed = 1;
        }
        if ((e = fm1_enc_take(ENC_KNOB2)) != 0) seqp.seg_order[g] = (uint8_t)step_mod(seqp.seg_order[g] % SD_SORDERS, e, SD_SORDERS), changed = 1;
        if ((e = fm1_enc_take(ENC_KNOB3)) != 0) {
            seqp.seg_skip = (uint8_t)(e > 0 ? seqp.seg_skip | 1u << g : seqp.seg_skip & ~(1u << g));
            changed = 1;
        }
        if ((e = fm1_enc_take(ENC_KNOB4)) != 0) {
            int32_t l = (int32_t)(bd[g + 1u] - bd[g]) + e;
            sd_seg_set_len(&seqp, g, (uint32_t)(l < 1 ? 1 : l));
            changed = 1;
        }
        if ((e = fm1_enc_take(ENC_ALGORITHM)) != 0) seq_dseg = step_mod(g, e, sc);   /* ALGORITHM: the segment */
    } else if (seq_more) {                      /* RATCHET 1..3, PROB in 5 % steps */
        if ((e = fm1_enc_take(ENC_KNOB1)) != 0) {
            int32_t r = (int32_t)(st->ratchet < 1u ? 1u : st->ratchet) + e;
            st->ratchet = (uint8_t)(r < 1 ? 1 : r > (int32_t)SD_RATCHET_MAX ? (int32_t)SD_RATCHET_MAX : r);
            changed = 1;
        }
        if ((e = fm1_enc_take(ENC_KNOB2)) != 0) {
            int32_t q = ((int32_t)st->prob + 2) / 5 * 5 + e * 5;
            st->prob = (uint8_t)(q < 0 ? 0 : q > 100 ? 100 : q);
            changed = 1;
        }
        (void)fm1_enc_take(ENC_KNOB3);
        (void)fm1_enc_take(ENC_KNOB4);
        if ((e = fm1_enc_take(ENC_ALGORITHM)) != 0) {
            seq_cur = step_mod(seq_cur, e, seqp.len);
            seq_bank = seq_cur / 16u;
        }
    } else {
        if ((e = fm1_enc_take(ENC_KNOB1)) != 0) {
            int32_t n = (int32_t)st->note + e;
            st->note = (uint8_t)(n < 0 ? 0 : n > 127 ? 127 : n);
            changed = 1;
        }
        if ((e = fm1_enc_take(ENC_KNOB2)) != 0) {
            int32_t v = (int32_t)st->vel + e;
            st->vel = (uint8_t)(v < 1 ? 1 : v > 127 ? 127 : v);
            changed = 1;
        }
        if ((e = fm1_enc_take(ENC_KNOB3)) != 0) {
            int32_t g = (int32_t)st->gate + e;
            st->gate = (uint8_t)(g < 1 ? 1 : g > 100 ? 100 : g);
            changed = 1;
        }
        if ((e = fm1_enc_take(ENC_KNOB4)) != 0) st->type = (uint8_t)step_mod(st->type % SD_STYPES, e, SD_STYPES), changed = 1;
        if ((e = fm1_enc_take(ENC_ALGORITHM)) != 0) {
            seq_cur = step_mod(seq_cur, e, seqp.len);
            seq_bank = seq_cur / 16u;
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
        cv_text(10, 26, &FONT_S, s, C_RED);
        return;
    }
    cv_text(10, 4, &FONT_S, pt ? "SAVE PATTERN" : "SAVE PATCH", C_RED);
    p = put(s, "TO ");
    *p++ = pt ? 'P' : 'U';
    if (save_to < 9u) *p++ = '0';
    p = put_u(p, save_to + 1u);
    p = put(p, "  ");
    if (pt) p = put(p, useq.used >> save_to & 1u ? "(REPLACE)" : "EMPTY");
    else p = put(p, upat.used >> save_to & 1u ? patch_name(SD_PATCHES + save_to) : "EMPTY");
    *p = 0;
    cv_text(10, 24, &FONT_S, s, C_ICE);
    cv_text(10, 44, &FONT_S, "ALGO PICKS  SAVE AGAIN: YES", C_ICE_D);
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









/* ---- ENV, FILTER, ARP (2026-11-06: the user, "everything is under FX: it must go to its place") */
static char *put_ms(char *p, uint32_t i)        /* an envelope time, 0..127: "12ms", "1.2s" */
{
    uint32_t ms = SD_ENV_MS[i & 127u];
    if (ms < 1000u) { p = put_u(p, ms); return put(p, "ms"); }
    p = put_u(p, ms / 1000u);
    if (ms < 10000u) { *p++ = '.'; *p++ = (char)('0' + ms / 100u % 10u); }
    return put(p, "s");
}







/* ==== The screen, the same on every page (2026-11-07, the user: "a tidier interface, more intuitive, the same all
 * through"; "K1-4 lie in a row on top, but every setting is drawn in a column: we need an indication"):
 *     0  the logo, the patch's number           22  the patch, the edited bank's roles, the mode
 *    44  the button's pages, this one a red tab; right: what EDIT switches here, ON / OFF
 *    64  K1 K2 K3 K4, left to right as the knobs: name, value, bar (the names red: SCL held, the second layer)
 *   116  the picture of what they set
 *   186  what ALGORITHM (and SCL + ALGORITHM) do here, or a message
 *   204  the steps of the track playing         222  the transport, the load
 * Each region is drawn into the canvas 25 times a second and sent only when its pixels changed. */


/* ---- what EDIT switches: the page's engine, the same everywhere (the user: "EDIT a toggle") */
static uint32_t fx_on(uint32_t bit)             /* an engine on: not switched off, and in the patch at all */
{
    if (pat.fx_off & bit) return 0;
    if (bit == SD_FX_REACTOR) return pat.r_mix != 0;
    if (bit == SD_FX_DISTROY) return pat.d_mix != 0;
    if (bit == SD_FX_SPACE) return pat.s_rlevel != 0;
    if (bit == SD_FX_TIME) return pat.s_dlevel || lp_lvl;
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
static void filt_first(void)                    /* the filter's first time on: open, ENV3 as ENV1, so it sounds */
{
    if (pat.f_cut || pat.f_res || pat.e3_s || pat.e3_d) return;
    pat.f_cut = 52000;
    pat.e3_a = pat.e1_a; pat.e3_d = pat.e1_d; pat.e3_s = pat.e1_s; pat.e3_r = pat.e1_r;
}
enum { SW_NONE, SW_OFF, SW_ON, SW_LOCK, SW_WAIT };   /* (WAIT: on, in at A's one) */
static uint32_t sw_of(const char **what)        /* this page's switch, and its name on the screen */
{
    uint32_t b = page_fx();
    *what = "EDIT";
    if (b) return fx_on(b) ? SW_ON : SW_OFF;
    if (page == 0u) { *what = "EDIT COURSE"; return pat.course ? SW_ON : SW_OFF; }
    if (page == PAGE_ARP) return eng.arp_on ? SW_ON : SW_OFF;
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
            if (b == SD_FX_DISTROY && !pat.d_mix) pat.d_mix = 65535;
            if (b == SD_FX_DISTROY) dest_first();
            if (b == SD_FX_SPACE && !pat.s_rlevel) pat.s_rlevel = 20000;
            if (b == SD_FX_TIME && !pat.s_dlevel && !lp_lvl) pat.s_dlevel = 20000;
            if (b == SD_FX_FILTER && !pat.f_type) { pat.f_type = SD_FT_LP12; filt_first(); }
        }
        return 1;
    }
    if (page == 0u) { pat.course = (uint8_t)!pat.course; return 1; }
    if (page == 4u && dmod == DMOD_TUR) {       /* TURING: on the first time to XM, 30 %, CHANGE 25 */
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
        if (seq_trk && set.mode == SD_MODE_MONO) msg("B NEEDS DUO OR POLY: GLO");
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
static const char *const NAV_HOME[2] = {"NAV", "MACROS"}, *const NAV_FX[4] = {"REACTOR", "DISTROY", "SPACE", "TIME"},
                         *const NAV_ENV[2] = {"ENV", "FILTER"}, *const NAV_LFO[4] = {"LFO 1", "LFO 2", "TURING", "MOD"},
                         *const NAV_ARP[1] = {"ARP"}, *const NAV_SEQ[3] = {"SEQ A", "SEQ B", "ORBIT"};
static uint32_t nav_of(const char *const **n, uint32_t *cur)
{
    switch (page) {
    case 0: case 5: *n = NAV_HOME; *cur = page == 5u; return 2;
    case 1: case 2: case 3: case PAGE_TIME: *n = NAV_FX; *cur = page == PAGE_TIME ? 3u : page - 1u; return 4;
    case PAGE_ENV: case PAGE_FILT: *n = NAV_ENV; *cur = page == PAGE_FILT; return 2;
    case 4: *n = NAV_LFO; *cur = dmod >= SD_MODS ? dmod - SD_MODS : 3u; return 4;
    case PAGE_ARP: *n = NAV_ARP; *cur = 0; return 1;
    default: *n = NAV_SEQ; *cur = page == PAGE_ORB ? 2u : seq_trk; return 3;
    }
}
static void draw_nav(void)
{
    const char *const *n, *what;
    uint32_t cur, cnt = nav_of(&n, &cur), k, sw = sw_of(&what);
    const char *t;
    if (n == NAV_SEQ && shift()) { what = "MASTER"; sw = SW_ON; }   /* SCL: what SEQ chooses there, the master */
    t = n == NAV_SEQ && shift() ? (set.master ? "B" : "A") : sw == SW_ON || sw == SW_WAIT ? "ON" : sw == SW_OFF ? "OFF" : "MONO";
    int32_t x = 6, gap = 10, tw = 0, bw = text_w(&FONT_S, t) + 6, bx = 238 - bw, lx = bx - 5 - text_w(&FONT_S, what);
    for (k = 0; k < cnt; k++) tw += text_w(&FONT_S, n[k]);
    if (sw && x + tw + 10 * (int32_t)cnt - 4 > lx) gap = 7;   /* (REACTOR DISTROY SPACE: closer, so EDIT fits) */
    cv_begin(240, 18, C_BG);
    for (k = 0; k < cnt; k++) {
        int32_t w = text_w(&FONT_S, n[k]);
        if (k == cur) {
            cv_rect(x - 4, 0, w + 8, 17, C_RED);
            cv_text(x, 0, &FONT_S, n[k], C_BG);
        } else {
            cv_text(x, 0, &FONT_S, n[k], (seq_page() || page == PAGE_ORB) && k == 1u && set.mode == SD_MODE_MONO ? C_GRID : C_ICE_D);
        }
        if (n == NAV_SEQ && k == set.master) cv_rect(x, 14, w, 2, k == cur ? C_BG : C_RED);   /* the master */
        x += w + gap;
    }
    if (sw) {                                   /* the switch: "EDIT" (when it fits) and ON / OFF / MONO */
        uint16_t c = sw == SW_OFF ? C_ICE_D : C_RED;
        if (sw == SW_ON || (sw == SW_WAIT && (fm1_ms / 200u) & 1u)) {   /* (WAIT: it blinks till A's one) */
            cv_rect(bx, 0, bw, 17, C_RED);
            cv_text(bx + 3, 0, &FONT_S, t, C_BG);
        } else {
            cv_frame(bx, 0, bw, 17, c);
            cv_text(bx + 3, 0, &FONT_S, t, c);
        }
        if (lx >= x - 4) cv_text(lx, 0, &FONT_S, what, C_ICE_D);
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
static void seq_l2_knobs(void)                  /* SEQ with SCL held */
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
static uint32_t has_l2(void)                    /* SCL gives this page a second layer */
{
    return page != 5u && page != 4u && page != PAGE_ARP && !(page == PAGE_SEQ && seq_segmode);
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
    case 5:
        ku(0, "MUTATE", q99(pat.mac[0])); ku(1, "MOVE", q99(pat.mac[1])); ku(2, "BREAK", q99(pat.mac[2]));
        ku(3, M4_NAME[pat.mac4 % SD_M4S], q99(pat.mac[3]));
        break;
    case 1:
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
        } else if (dband == 4u) {               /* LOOP, SCL: the whole of DISTROY: MIX, XOVER */
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
        } else {                                /* SCL: BIAS PAN DYNAMIC (PHAT .. SHARP) SPLIT */
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
    case 3:                                     /* SPACE: the plate (SCL: SHIMMER, as SCL + ALGORITHM) */
        if (!l2) {
            ku(0, "SIZE", q99(pat.s_size)); ku(1, "DECAY", q99(pat.s_decay)); ku(2, "TONE", q99(pat.s_rtone));
            ku(3, "R-LEVEL", q99(pat.s_rlevel));
        } else {
            ku(0, "SHIMMER", SD_SP_SHIM(&pat) * 99u / 63u); k0(1); k0(2); k0(3);
        }
        break;
    case PAGE_TIME:                             /* TIME: the delay; SCL: its MODE and the looper */
        if (!l2) {
            *put_time(t) = 0; kt(0, "TIME", t);
            kn[0].hot = sp_fold(0) != 0u;       /* (red: longer than the line, it plays shorter: the hint says) */
            ku(1, "FDBK", q99(pat.s_fb)); ku(2, "TONE", q99(pat.s_dtone)); ku(3, "D-LEVEL", q99(pat.s_dlevel));
        } else {
            int32_t v = lp_speed, a = v < 0 ? -v : v;
            *put_glide(t) = 0; kt(0, "GLIDE", t);
            *put_len(t) = 0; kt(1, "LENGTH", t);
            kn[1].hot = lp_fold(0) != 0u;       /* (red: longer than its memory) */
            p = t; *p++ = v < 0 ? '-' : '+';
            p = put_u(p, (uint32_t)a >> 8); *p++ = '.'; p = put_u(p, (((uint32_t)a & 255u) * 10u) >> 8); *p = 0;
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
        if (dmod == DMOD_TUR) {                 /* TURING: CHANGE LENGTH DEST AMOUNT */
            p = put_u(t, SD_TUR_CHANGE(&pat)); *p++ = '%'; *p = 0; kx(0, "CHANGE", t, SD_TUR_CHANGE(&pat) * 99u / 100u);
            *put_u(t, SD_TUR_LEN(&pat)) = 0; kx(1, "LENGTH", t, (SD_TUR_LEN(&pat) - 2u) * 99u / 14u);
            kt(2, "DEST", tdest_name(pat.tur_dst));
            p = put_sgn(t, pat.tur_amt); *p++ = '%'; *p = 0; kb(3, "AMOUNT", t, pat.tur_amt * 99 / 100);
        } else if (dmod >= SD_MODS) {           /* an LFO */
            uint32_t k = dmod - SD_MODS;
            kt(0, "SHAPE", LSHAPE_NAME[pat.lfo_shape[k] % SD_LSHAPES]);
            *put_rate(t, k) = 0; kt(1, "RATE", t);
            kt(2, "MODE", LMODE_NAME[lmode_of(pat.lfo_mode[k])]);
            k0(3);
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
        if (orb_setup && l2) {                  /* SETUP, SCL: GLIDE for lane 1, lane 2, the macros */
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
        } else {                                /* the step: XPOSE A, XPOSE B, the lanes */
            *put_xp(t, orbp.xa[s]) = 0; kb(0, "XPOSE A", t, orbp.xa[s] * 99 / 24);
            *put_xp(t, orbp.xb[s]) = 0; kb(1, "XPOSE B", t, orbp.xb[s] * 99 / 24);
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
                k0(2); k0(3);
            } else {
                *put_note(t, st->note) = 0; kt(0, "NOTE", t);
                *put_u(t, st->vel) = 0; kx(1, "VEL", t, st->vel * 99u / 127u);
                p = put_u(t, st->gate); *p++ = '%'; *p = 0; kx(2, "GATE", t, st->gate * 99u / 100u);
                kt(3, "TYPE", STYPE_NAME[st->type % SD_STYPES]);
            }
        }
        break;
    }
}
static void draw_knobs(void)
{
    uint32_t i, l2 = shift() && has_l2();
    knobs_of();
    cv_begin(240, H_KNOB, C_BG);
    for (i = 0; i < 4u; i++) {
        const Kn *k = &kn[i];
        int32_t x = (int32_t)i * 60, cx = x + 30;
        if (i) cv_line(x, 18, x, (int32_t)H_KNOB - 4, C_GRID);   /* (under the names: seven letters fit) */
        if (!k->nm) { cv_text(cx - 4, 17, &FONT_S, "-", C_GRID); continue; }
        cv_text(cx - text_w(&FONT_S, k->nm) / 2, 0, &FONT_S, k->nm, l2 ? C_RED : C_ICE_D);
        cv_text(cx - text_w(&FONT_S, k->v) / 2, 17, &FONT_S, k->v, k->hot ? C_RED : C_ICE);
        if (k->kind == KN_UNI) {                /* eight segments, the last ones red as they fill */
            uint32_t s, n = ((uint32_t)k->bar * 8u + 49u) / 99u;
            for (s = 0; s < 8u; s++)
                cv_rect(cx - 24 + (int32_t)s * 6, 38, 4, 7, s >= n ? C_GRID : s < 5u ? C_ICE : C_RED);
        } else if (k->kind == KN_BI) {          /* from the middle: right red, left ice */
            int32_t w = k->bar * 24 / 99;
            cv_rect(cx - 24, 41, 48, 1, C_GRID);
            cv_rect(cx, 37, 1, 9, C_ICE_D);
            if (w) cv_rect(w > 0 ? cx + 1 : cx + w, 38, w > 0 ? w : -w, 7, w > 0 ? C_RED : C_ICE);
        }
    }
}

/* ---- the pictures, 240 x 68 */
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
    int32_t cx = 34, cy = 34, R = 20, qx[4], qy[4];
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
        cv_text(x, 2, &FONT_S, s, o->role == SD_ROLE_C ? C_ICE : C_RED);
        p = put(s, "x"); *put_u(p, o->harm) = 0;
        cv_text(x + 9, 2, &FONT_S, s, C_ICE);
        for (seg = 0; seg < 6u; seg++)
            cv_rect(x + (int32_t)seg * 5, 22, 4, 8, seg >= n ? C_GRID : o->role == SD_ROLE_C ? C_ICE : C_RED);
        p = s;
        if (o->fine) p = put_sgn(p, o->fine); else *p++ = '0';
        *p = 0;
        cv_text(x, 33, &FONT_S, s, C_ICE_D);
        if (k) cv_line(x - 4, 4, x - 4, 48, C_GRID);
    }
    p = put(s, "BANK "); *p++ = (char)('A' + ebank); *p = 0;
    cv_text(86, 51, &FONT_S, s, C_ICE_D);
}

static void pic_chain(void)                     /* MACROS: the chain, each engine ON or OFF (EDIT on its page) */
{
    static const char *const EN[5] = {"FILT", "REACT", "DEST", "TIME", "SPACE"};
    uint32_t k, on[5];
    on[0] = fx_on(SD_FX_FILTER); on[1] = fx_on(SD_FX_REACTOR); on[2] = fx_on(SD_FX_DISTROY); on[3] = fx_on(SD_FX_TIME);
    on[4] = fx_on(SD_FX_SPACE);
    for (k = 0; k < 5u; k++) {
        int32_t x = 4 + (int32_t)k * 47;
        cv_text(x, 2, &FONT_S, EN[k], on[k] ? C_ICE : C_ICE_D);
        cv_rect(x, 22, 40, 4, on[k] ? C_RED : C_GRID);
        cv_text(x, 30, &FONT_S, on[k] ? "ON" : "OFF", on[k] ? C_RED : C_ICE_D);
        if (k) { cv_line(x - 6, 20, x - 3, 24, C_ICE_D); cv_line(x - 3, 24, x - 6, 28, C_ICE_D); }   /* > */
    }
    cv_text(4, 51, &FONT_S, "EACH PAGE: EDIT = ON/OFF", C_ICE_D);
}

static void pic_reactor(void)                     /* the filters now as peaks; the six stages, the one playing red */
{
    enum { X0 = 6, W = 140, Y0 = 2, H = 62 };
    int32_t col[W], x, k, wpx;
    uint32_t i, pos = eng.r.shown_pos, at = pos >> 8, nx = at == 5u ? 0u : at + 1u, f = pos & 255u;
    uint32_t dom = f < 128u ? at : nx, sec = f ? (f < 128u ? nx : at) : 99u;
    uint16_t on = fx_on(SD_FX_REACTOR);
    wpx = 1 + (int32_t)((uint32_t)(SD_RK[pat.r_res >> 9] >> 8) * 22u >> 16);   /* k 2 -> 23 px, k 0.01 -> 1 */
    for (x = 0; x < W; x++) col[x] = 0;
    for (k = 0; k < 4; k++) {
        int32_t xc = gx(eng.r.pitch[k] >> 8, X0, W), h = (eng.r.level[k] >> 8) * (H - 4) / 65536;
        for (x = 0; x < W; x++) {
            int32_t d = X0 + x - xc, y = h * wpx * wpx / (wpx * wpx + d * d);
            if (y > col[x]) col[x] = y;
        }
    }
    cv_line(X0, Y0 + H, X0 + W - 1, Y0 + H, C_GRID);
    for (x = 0; x < W; x++)
        if (col[x]) {
            cv_rect(X0 + x, Y0 + H - col[x], 1, col[x], on ? C_FILL : C_GRID);
            cv_pset(X0 + x, Y0 + H - col[x], on ? C_RED : C_ICE_D);
        }
    for (i = 0; i < 6u; i++) {                  /* the stages: four ticks each at their filters' pitch */
        int32_t sx = 154 + (int32_t)i * 14;
        uint16_t c = i == dom ? C_RED : i == sec ? C_RED_D : C_GRID;
        cv_frame(sx, 2, 12, 62, c);
        for (k = 0; k < 4; k++) {
            int32_t y = 60 - ((int32_t)pat.stage[i].pitch[k] - SD_R_LO * 256) * 56 / ((SD_R_HI - SD_R_LO) * 256);
            cv_rect(sx + 2 + k * 2, y, 2, 2, pat.stage[i].level[k] ? (i == dom ? C_ICE : C_ICE_D) : C_GRID);
        }
    }
}

static void pic_dest(void)                      /* the bands on a log-frequency axis, as tall as their drive: each
                                                   its name and type as far as they fit, the one ALGORITHM picks red,
                                                   the splits in Hz under it */
{
    enum { X0 = 6, W = 228, Y0 = 18, H = 32 };
    uint32_t i, k = dband;
    int32_t xl = -100;
    char t[16], *p;
    if (!dest_set(&pat)) {                      /* never set (DISTROY off in this patch): EDIT sets it */
        cv_text(X0, 20, &FONT_S, "NO BANDS SET YET", C_ICE_D);
        cv_text(X0, 38, &FONT_S, "EDIT: ON, FOUR BANDS", C_ICE_D);
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
        p = put(t, BAND_NAME[i]); *p++ = ' '; p = put(p, SHAPE_NAME[pat.d_type[i] % SD_DSHAPES]); *p = 0;   /* as much as fits */
        if (text_w(&FONT_S, t) > room) *put(t, BAND_NAME[i]) = 0;
        if (text_w(&FONT_S, t) > room) *put(t, BAND_SHORT[i]) = 0;
        if (text_w(&FONT_S, t) > room) { t[0] = (char)('1' + i); t[1] = 0; }
        if (text_w(&FONT_S, t) <= room + 2) cv_text(x0 + 2, 0, &FONT_S, t, c);
    }
    for (i = 0; i < 3u; i++) {                  /* the splits, their Hz under the axis where they do not touch */
        int32_t xs = gx(pat.d_split[i], X0, W), w;
        cv_line(xs, Y0, xs, Y0 + H, C_ICE_D);
        *put_hz(t, hz_of(pat.d_split[i])) = 0;
        w = text_w(&FONT_S, t);
        if (xs - w / 2 > xl + 4 && xs + w / 2 < X0 + W) { cv_text(xs - w / 2, Y0 + H + 2, &FONT_S, t, C_ICE_D); xl = xs + w / 2; }
    }
    if (k == 4u) {                              /* LOOP picked: its tag over the bands */
        cv_rect(X0 + W - 44, 0, 44, 16, C_RED);
        cv_text(X0 + W - 40, 0, &FONT_S, "LOOP", C_BG);
    }
}

static void pic_space(void)                     /* SPACE: the plate's tail (the room's, on one core) */
{
    enum { X0 = 6, W = 228, Y0 = 2, H = 62 };
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
        cv_rect(X0 + W - 60, 0, 60, 16, C_RED);
        cv_text(X0 + W - 56, 0, &FONT_S, "FREEZE", C_BG);
    }
    if (eng.sp_room) {                          /* one core: the room plays in the plate's place (no FREEZE there) */
        cv_rect(X0, 0, 44, 16, C_ICE_D);
        cv_text(X0 + 4, 0, &FONT_S, "ROOM", C_BG);
    }
}

static void pic_time(void)                      /* TIME: the echoes falling by FDBK along 2 s; the looper's length,
                                                   and its tag while it loops */
{
    enum { X0 = 6, W = 228, Y0 = 2, H = 62 };
    uint32_t i, ms, lms;
    int32_t x, h, fb;
    cv_line(X0, Y0 + H, X0 + W - 1, Y0 + H, C_GRID);
    if (pat.s_mode & SD_SP_SYNC)                /* as the engine plays it: the tempo heard, a halving when long */
        ms = sd_space_sync_q8(eng.tempo_q8, sd_sp_div(pat.s_time), 0, dl_lim()) / 11290u;   /* (samples x 256:
                                                   44.1 x 256) */
    else
        ms = sd_sp_ms(pat.s_time);
    fb = (int32_t)((pat.s_fb * 31130u) >> 16);
    h = (int32_t)(pat.s_dlevel * (uint32_t)(H - 6) >> 16);
    for (i = 1; ms && i * ms < 2000u && h > 1; i++) {   /* (ping-pong: in pairs, left red and right ice, x FDBK^2) */
        uint32_t pp = (pat.s_mode & SD_SP_PP) != 0u;
        x = X0 + (int32_t)(i * ms * W / 2000u);
        cv_line(x, Y0 + H, x, Y0 + H - h, pp && (i & 1u) == 0u ? C_ICE : C_RED);
        if (!pp) h = (h * fb) >> 15;
        else if (!(i & 1u)) h = (((h * fb) >> 15) * fb) >> 15;
    }
    {                                           /* the looper: its length on the same 2 s, a bar under the axis */
        uint32_t q = pat.s_mode & SD_SP_SYNC ? sd_space_sync_q8(eng.tempo_q8, sd_lp_div(lp_len), 0, lp_lim())
                                             : sd_sp_ms(lp_len) * 11290u;
        if (q > lp_lim()) q = lp_lim();
        lms = q / 11290u;
        cv_rect(X0, Y0 + H - 3, (int32_t)((lms < 2000u ? lms : 2000u) * W / 2000u) + 1, 3, lp_on ? C_RED : C_ICE_D);
    }
    if (lp_on) {
        cv_rect(X0 + W - 44, 0, 44, 16, lp_lvl ? C_RED : C_ICE_D);
        cv_text(X0 + W - 40, 0, &FONT_S, "LOOP", C_BG);
    }
    if (lp_rev) {                               /* REVERSE: its tag, left of LOOP's */
        cv_rect(X0 + W - 84, 0, 36, 16, lp_on ? C_RED : C_ICE_D);
        cv_text(X0 + W - 80, 0, &FONT_S, "REV", C_BG);
    }
    if (shift()) {                              /* SCL: the longest the looper holds */
        char pd[16], *p;
        uint32_t n = sd_lp_total(&eng), cs = (n > SD_LP_XF + 64u ? n - SD_LP_XF - 32u : 0u) / 220u;   /* (centiseconds) */
        p = put(pd, "MAX ");
        p = put_u(p, cs / 100u); *p++ = '.'; *p++ = (char)('0' + cs / 10u % 10u); *p++ = (char)('0' + cs % 10u);
        *p++ = ' '; *p++ = 'S'; *p = 0;
        cv_text(X0, 0, &FONT_S, pd, C_ICE_D);
    }
}

static void pic_env(void)                       /* ENV 1 (the level) and ENV 2 (the FM's amount), SCL brings 2 up */
{
    uint32_t l2 = shift();
    cv_line(6, 64, 233, 64, C_GRID);
    if (l2) { adsr(&pat.e1_a, 6, 228, 20, 44, C_ICE_D); adsr(&pat.e2_a, 6, 228, 20, 44, C_RED); }
    else { adsr(&pat.e2_a, 6, 228, 20, 44, C_RED_D); adsr(&pat.e1_a, 6, 228, 20, 44, C_ICE); }
    cv_text(6, 0, &FONT_S, "ENV 1 LEVEL", l2 ? C_ICE_D : C_ICE);
    cv_text(234 - text_w(&FONT_S, "ENV 2 FM"), 0, &FONT_S, "ENV 2 FM", l2 ? C_RED : C_RED_D);
}

static void pic_filt(void)                      /* the response; ENV 3 (SCL) on the right */
{
    enum { X0 = 6, W = 150, Y0 = 2, H = 62 };
    int32_t c = 16 * 256 + (int32_t)((uint32_t)pat.f_cut * 114u >> 8), xc = gx(c, X0, W), peak = (int32_t)q99(pat.f_res) / 4;
    int32_t slope = pat.f_type == SD_FT_LP24 ? 24 : 12, xx, y0 = Y0 + 26, prev = y0;
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
    cv_line(164, 4, 164, 62, C_GRID);
    cv_text(170, 0, &FONT_S, "ENV 3", l2 ? C_RED : C_ICE_D);
    adsr(&pat.e3_a, 170, 64, 20, 44, l2 ? C_RED : C_ICE_D);
}

static void pic_lfo(uint32_t k)                 /* its cycle, the dot where it is; where the matrix sends it */
{
    uint32_t i, n = 0;
    draw_lfo_wave(k, 6, 4, 130, 58);
    cv_line(146, 4, 146, 62, C_GRID);
    cv_text(154, 0, &FONT_S, "TO", C_ICE_D);
    for (i = 0; i < SD_MODS && n < 3u; i++) {
        const SdModSlot *m = &pat.mod[i];
        if (m->src != SD_MS_LFO1 + k || !m->dst || !m->amt) continue;
        cv_text(154, 17 + (int32_t)n * 16, &FONT_S, DST_NAME[m->dst % SD_MDSTS], C_ICE);
        n++;
    }
    if (!n) { cv_text(154, 17, &FONT_S, "NOTHING", C_ICE_D); cv_text(154, 33, &FONT_S, "SET: MOD", C_ICE_D); }
}

/* TURING: its last 16 steps as a stepped line (the newest at the right, red), the loop's bits under it (the newest at
 * the right; the 8 the output reads bright, the older dim); where it goes, and the MOD slots it feeds */
static void pic_tur(void)
{
    static int16_t hist[16];
    static uint32_t seen;
    uint32_t c = eng.tur_count, reg = eng.tur_reg, i, n, len = SD_TUR_LEN(&pat), on = SD_TUR_ON(&pat);
    char s[16], *p;
    if (eng.tur_count != c) { c = eng.tur_count; reg = eng.tur_reg; }   /* (a step meanwhile: the newer pair) */
    n = c - seen > 16u ? 16u : c - seen;
    if (n > 8u) n = 8u;                         /* (the register holds the 8 values before: older ones are gone) */
    for (i = n; i > 0u; i--) {                  /* the values since the last frame, from the register */
        uint32_t k;
        for (k = 0; k < 15u; k++) hist[k] = hist[k + 1u];
        hist[15] = (int16_t)sd_tur_dac(reg >> (i - 1u));
    }
    seen = c;
    cv_line(6, 26, 133, 26, C_GRID);
    for (i = 0; i < 16u; i++) {                 /* the steps: each 8 px, joined */
        int32_t x = 6 + (int32_t)i * 8, y = 26 - hist[i] * 22 / 32767;
        uint16_t col = !on ? C_GRID : i == 15u ? C_RED : C_ICE;
        cv_rect(x, y, 8, 2, col);
        if (i) {
            int32_t y0 = 26 - hist[i - 1u] * 22 / 32767;
            cv_rect(x, y0 < y ? y0 : y, 1, (y0 < y ? y - y0 : y0 - y) + 2, on ? C_ICE_D : C_GRID);
        }
    }
    for (i = 0; i < len; i++) {                 /* the loop: bit i at the right going left */
        int32_t x = 6 + (int32_t)(15u - i) * 8;
        uint32_t b = reg >> i & 1u;
        cv_rect(x, 55, 7, 7, !b ? C_GRID : !on ? C_ICE_D : i < 8u ? C_ICE : C_ICE_D);
    }
    cv_line(146, 4, 146, 62, C_GRID);
    cv_text(154, 0, &FONT_S, "TO", C_ICE_D);
    cv_text(154, 17, &FONT_S, on && pat.tur_dst ? tdest_name(pat.tur_dst) : "OFF", on && pat.tur_dst ? C_ICE : C_ICE_D);
    for (i = 0, n = 0; i < SD_MODS; i++)        /* and the MOD slots with TURING as their source */
        if (pat.mod[i].src == SD_MS_TUR && pat.mod[i].dst && pat.mod[i].amt) n++;
    if (n) { p = put(s, "+ MOD x"); *put_u(p, n) = 0; cv_text(154, 33, &FONT_S, s, C_ICE_D); }
    else if (!on) cv_text(154, 33, &FONT_S, "EDIT: ON", C_ICE_D);
}

static void pic_mod(void)                       /* the eight slots, the picked one red; its amount */
{
    uint32_t k;
    char s[12], *p;
    for (k = 0; k < SD_MODS; k++) {
        const SdModSlot *m = &pat.mod[k];
        int32_t x = 4 + (int32_t)(k & 3u) * 59, y = k < 4u ? 0 : 18;
        uint16_t c = k == dmod ? C_RED : m->src && m->dst && m->amt ? C_ICE : C_GRID;
        p = put(s, SRC_SHORT[m->src % SD_MSRCS]);
        *p++ = '>';
        p = put(p, DST_SHORT[m->dst % SD_MDSTS]);
        *p = 0;
        cv_text(x, y, &FONT_S, s, c);
    }
    {
        int32_t a = amt99(pat.mod[dmod % SD_MODS].amt), cx = 120, w = a * 100 / 99;
        cv_line(cx, 42, cx, 60, C_ICE_D);
        cv_line(cx - 100, 51, cx + 100, 51, C_GRID);
        if (w) cv_rect(w > 0 ? cx + 1 : cx + w, 46, w > 0 ? w : -w, 11, a < 0 ? C_ICE : C_RED);
    }
}

static void pic_arp(void)                       /* the run: three notes over OCT octaves in the mode's order */
{
    enum { X0 = 6, W = 228, Y0 = 18, H = 46 };
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
    if (!eng.arp_on) p = put(s, "OFF: EDIT TURNS IT ON");
    else if (!eng.arp_n) p = put(s, "ON: PLAY KEYS");
    else { p = put(s, "ON: "); p = put_u(p, eng.arp_n); p = put(p, eng.arp_n == 1u ? " NOTE" : " NOTES"); }
    if (set.ahold) p = put(p, " HOLD");
    *p = 0;
    cv_text(6, 0, &FONT_S, s, eng.arp_on ? C_RED : C_ICE_D);
}

static void pic_seq(void)                       /* SEQ, TURING: the pattern, the bank's steps as towers, the keys */
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
        if (seq_bank * 16u + k >= seqp.len || st->type >= SD_ST_REST) continue;
        if (st->note < lo) lo = st->note;
        if (st->note > hi) hi = st->note;
    }
    if (hi < lo) lo = hi = 45;
    if (hi - lo < 12u) { lo = lo > 6u ? lo - 6u : 0u; hi = lo + 12u; }
    for (k = 0; k < 16u; k++) {                 /* towers as high as the note; TIE a bridge, SLIDE a ramp, REST a dot */
        uint32_t i = seq_bank * 16u + k, seg = sd_seg_of(&seqp, i);
        const SdStep *st = &seqp.step[i];
        int32_t x = 9 + (int32_t)k * 14, base = 48, h;
        uint16_t c = i == play ? C_RED : C_ICE;
        if (i >= seqp.len) { cv_rect(x + 4, base - 1, 4, 1, C_GRID); continue; }
        if (i == bd[seg] && sc > 1u) {          /* a segment starts here: its letter and order */
            p = s;
            *p++ = (char)('A' + seg);
            p = put(p, SORDER_GLYPH[seqp.seg_order[seg] % SD_SORDERS]);
            *p = 0;
            cv_line(x - 2, 10, x - 2, base, seq_segmode ? C_ICE : C_GRID);
            cv_text(x, 8, &FONT_S, s, seq_segmode && seg == seq_dseg ? C_RED : C_ICE_D);
        }
        h = 4 + (int32_t)((st->note < lo ? 0u : st->note - lo) * 18u / (hi - lo));
        switch (st->type) {
        case SD_ST_REST: cv_rect(x + 5, base - 2, 2, 2, C_ICE_D); break;
        case SD_ST_SKIP: cv_line(x + 2, base - 6, x + 9, base - 1, C_RED_D); cv_line(x + 9, base - 6, x + 2, base - 1, C_RED_D); break;
        case SD_ST_TIE: cv_rect(x - 3, base - h + 2, 15, 2, c); cv_rect(x + 2, base - h + 4, 8, h - 4, C_GRID); break;
        default:
            cv_rect(x + 2, base - h, 8, h, i == play ? C_RED : st->vel >= 110u ? C_ICE : C_ICE_D);
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
        if (i == seq_cur && !seq_segmode) cv_frame(x - 1, 22, 14, 28, C_RED);
    }
    /* the line under: what the keys do now */
    if (seq_rec) { p = put(s, eng.seq_run ? "REC LIVE  ENV TIE LFO REST" : "REC STEP  ENV TIE LFO REST"); lc = C_RED; }
    else if (shift()) {                         /* three pairs, a little more than a space apart (30 characters do
                                                   not fit) */
        static const char *const G[3] = {"KEYS XPOSE", "OCT BANK", "SEQ A/B"};   /* (SEQ: the master, the top line
                                                   says which) */
        int32_t gx = 6, g;
        for (g = 0; g < 3; g++) gx = cv_text(gx, 52, &FONT_S, G[g], C_RED) + 11;
        return;
    }
    else if (lock) { p = put(s, "B NEEDS DUO OR POLY: GLO"); lc = C_RED; }
    else if (seq_segmode) p = put(s, "WHITE CUT  BLACK A-F EVEN SEG");
    else if (seq_clr_at) { p = put(s, "BLACK 3 AGAIN: CLEAR ALL"); lc = C_RED; }
    else if (seq_more) p = put(s, "TAP THE STEP AGAIN: BACK");
    else p = put(s, "BLACK KEYS: RND UNDO CLR  SEG");
    *p = 0;
    cv_text(6, 52, &FONT_S, s, lc);
}

/* ORBIT: its steps as columns, four lanes as rows of bars from their middle (SCL: the macros'), the step picked
 * framed red, the one playing red and its LED under the stripes lit, past LENGTH dim; a lane that glides its name red;
 * under it what plays */
static void pic_orbit(void)
{
    static const char *const ML[4] = {"MU", "MV", "BR", "M4"};
    uint32_t len = SD_ORB_LEN(&orbp), play = eng.seq_run && orbp.on ? eng.orb_shown : 0xFFu, r, s, l2 = shift();
    for (r = 0; r < 4u; r++) {
        int32_t y = (int32_t)r * 14;
        const char *nm = l2 ? ML[r] : r == 0u ? "XA" : r == 1u ? "XB" : DST_SHORT[orbp.dst[r - 2u] % SD_MDSTS];
        uint32_t gl = l2 ? (orbp.glide & SD_OG_MAC) != 0u : r >= 2u ? (orbp.glide >> (r - 2u) & 1u) : 0u;
        cv_text(4, y, &FONT_S, nm, gl ? C_RED : C_ICE_D);
        for (s = 0; s < SD_ORB_STEPS; s++) {
            int32_t x = 36 + (int32_t)s * 25, v, rng, w;
            uint16_t c = s >= len ? C_GRID : s == play ? C_RED : orbp.on ? C_ICE : C_ICE_D;
            v = l2 ? orbp.mac[r][s] : r == 0u ? orbp.xa[s] : r == 1u ? orbp.xb[s] : orbp.lane[r - 2u][s];
            rng = l2 ? 50 : r < 2u ? 12 : 50;   /* (the cell's half: 12 semitones, 50 %; past it full) */
            w = v * 11 / rng;
            w = w > 11 ? 11 : w < -11 ? -11 : w;
            if (v && w > -2 && w < 2) w = v > 0 ? 2 : -2;   /* (a small change still shows) */
            cv_line(x + 11, y + 3, x + 11, y + 11, s >= len ? C_GRID : C_ICE_D);
            if (w > 0) cv_rect(x + 12, y + 4, w, 7, c);
            else if (w < 0) cv_rect(x + 11 + w, y + 4, -w, 7, c);
        }
    }
    cv_frame(36 + (int32_t)(orb_cur % SD_ORB_STEPS) * 25 - 1, 0, 25, 58, C_RED);
    for (s = 0; s < len && s < SD_ORB_STEPS; s++)   /* the LEDs under the stripes: the one playing lit (the user: "a
                                                       marker for the step that is active"), the rest dark red; ORBIT
                                                       off, grey */
        cv_rect(36 + (int32_t)s * 25 + 8, 61, 7, 3, s == play ? C_RED : orbp.on ? C_FILL : C_GRID);
}

static void pic(void)
{
    switch (page) {
    case 0: pic_nav(); break;
    case 5: pic_chain(); break;
    case 1: pic_reactor(); break;
    case 2: pic_dest(); break;
    case 3: pic_space(); break;
    case PAGE_TIME: pic_time(); break;
    case PAGE_ENV: pic_env(); break;
    case PAGE_FILT: pic_filt(); break;
    case 4: if (dmod == DMOD_TUR) pic_tur(); else if (dmod >= SD_MODS) pic_lfo(dmod - SD_MODS); else pic_mod(); break;
    case PAGE_ARP: pic_arp(); break;
    case PAGE_ORB: pic_orbit(); break;
    default: pic_seq(); break;
    }
}

/* ---- the hint: what ALGORITHM does here (labels dim, values ice), or a message */
static int32_t hint_x;
static void hint(const char *l, const char *v)
{
    if (!l) return;
    cv_text(hint_x, 0, &FONT_S, l, C_ICE_D);
    hint_x += text_w(&FONT_S, l) + 8;
    if (v) { cv_text(hint_x, 0, &FONT_S, v, C_ICE); hint_x += text_w(&FONT_S, v) + 16; }
}
static void draw_hint(void)
{
    static const char *const TN[4] = {"OFF", "KEY", "HARM", "KEY+H"};
    char a[16], b[16], *p;
    cv_begin(240, 16, C_BG);
    hint_x = 6;
    if (msg_txt && fm1_ms - msg_at < 2000u) { cv_text(6, 0, &FONT_S, msg_txt, C_RED); return; }
    msg_txt = 0;
    switch (page) {
    case 0:
        roles_of(&pat, a);
        p = put(b, "BANK "); *p++ = (char)('A' + ebank); *p = 0;
        hint("ALGO", a); hint("SCL+ALGO", b);
        break;
    case 5: hint("ALGO", "MACRO 4 KIND"); break;
    case 1:
        if (pat.r_low) *put_u(b, 50u + ((pat.r_low * 350u) >> 16)) = 0; else *put(b, "OFF") = 0;
        hint("ALGO", TN[pat.r_track & 3u]); hint("SCL+ALGO LOW", b);
        break;
    case 2:
        if (dband == 4u) { hint("ALGO", "LOOP"); hint("SCL", "MIX XOVER"); break; }
        if (shift()) {                          /* SCL held: ALGORITHM is the band's BODY */
            p = put_sgn(b, -54 + (int32_t)(pat.d_body[dband] * 48u / 65535u)); p = put(p, "dB"); *p = 0;
            hint("SCL+ALGO BODY", b);
            break;
        }
        p = a; *p++ = (char)('1' + dband); *p++ = ' '; p = put(p, BAND_NAME[dband]); *p = 0;
        hint("ALGO", a); hint("SCL+ALGO", "BODY");
        break;
    case 3:
        if (shift()) {                          /* SCL: ALGORITHM is SHIMMER */
            p = put_u(b, SD_SP_SHIM(&pat) * 99u / 63u); *p++ = '%'; *p = 0;
            hint("SCL+ALGO SHIMMER", b);
            break;
        }
        hint("ALGO", sp_frz ? "FREEZE ON" : "FREEZE OFF"); hint("SCL", "SHIMMER");
        break;
    case PAGE_TIME: {
        char pd[12];
        if (shift()) {                          /* SCL: the looper (the longest it holds in the picture), and the
                                                   delay's MODE */
            if (lp_fold(pd)) { hint("LENGTH: PLAYS", pd); break; }
            hint("ALGO", lp_rev ? "REV ON" : "REV OFF"); hint("EDIT", SPMODE_NAME[pat.s_mode & 3u]);
            break;
        }
        if (sp_fold(pd)) { hint(dl_lim() == SD_SP_LIM2 ? "OVER 0.5 S: PLAYS" : "OVER 1 S: PLAYS", pd); break; }   /* (on
                                                   the beat still: TIME red; 0.5 s: the looper has the line's rest) */
        hint("ALGO", lp_on ? "LOOP ON" : "LOOP OFF"); hint("SCL", "LOOPER");
        break;
    }
    case PAGE_ENV: hint("SCL", "ENV 2, THE FM AMOUNT"); break;
    case PAGE_FILT:
        p = put_u(b, pat.f_vel); *p++ = '%'; *p = 0;
        hint("ALGO", pat.f_type == SD_FT_LP24 ? "LP24" : "LP12"); hint("SCL+ALGO VEL", b);
        break;
    case 4:
        if (dmod == DMOD_TUR) { hint("ALGO", SDIV_NAME[SD_TUR_DIV(&pat)]); hint("LFO", "AGAIN: MOD"); break; }
        if (dmod >= SD_MODS) { hint("LFO", "AGAIN: THE NEXT"); break; }
        p = put(a, "SLOT "); *put_u(p, dmod + 1u) = 0;
        hint("ALGO", a);
        break;
    case PAGE_ARP: hint("ALGO HOLD", set.ahold ? "ON" : "OFF"); hint("REC", "KEEP"); break;
    case PAGE_ORB:
        if (orb_setup) { hint("F#5", "BACK"); hint("SCL", "GLIDE"); break; }
        if (shift()) { hint("SCL", "MACROS"); hint("EDIT", set.xchrom ? "CHROM" : "IN KEY"); break; }
        p = put(a, "STEP "); *put_u(p, orb_cur % SD_ORB_STEPS + 1u) = 0;
        hint("ALGO", a); hint("SCL", "MACROS");
        break;
    default:
        if (seq_segmode) { p = a; *p++ = (char)('A' + seq_dseg); *p = 0; hint("ALGO SEGMENT", a); break; }
        if (shift()) {                          /* SCL held: ALGORITHM the scale, EDIT the transposes' kind */
            hint("ALGO", SCALE_NAME[seqp.scale % SD_SCALES]); hint("EDIT", set.xchrom ? "CHROM" : "IN KEY");
            break;
        }
        p = put(a, "STEP "); *put_u(p, seq_cur + 1u) = 0;
        hint("ALGO", a); hint("SCL+ALGO", SCALE_NAME[seqp.scale % SD_SCALES]);
        break;
    }
}

/* ---- the steps of the track playing (on SEQ the one edited), the transport, the load */
static void draw_steps(void)
{
    uint32_t sp = seq_page(), tk = sp ? seq_trk : 0u, k, on = eng.seq_run, step = eng.t[tk].seq_shown;
    uint32_t bank = sp ? seq_bank : on ? step / 16u : 0u;
    const SdPattern *pp = &eng.t[tk & 1u].pat;
    cv_begin(240, 16, C_BG);
    for (k = 0; k < 16u; k++) {
        uint32_t i = bank * 16u + k;
        const SdStep *st = &pp->step[i];
        uint16_t c = i >= pp->len ? C_BG : st->type >= SD_ST_REST ? C_GRID : st->type == SD_ST_TIE ? C_FILL : C_ICE_D;
        if (on && i == step) c = C_RED;
        cv_rect(8 + (int32_t)k * 14, 1, 12, 10, c);
        cv_rect(8 + (int32_t)k * 14, 13, 12, 2, i >= pp->len ? C_BG : k % 4u ? C_GRID : C_RED_D);   /* the beat */
    }
}
static void draw_status(void)
{
    char s[40], *p = s;
    int32_t oct = (int8_t)set.oct;
    uint32_t run = eng.seq_run, pm = cpu_permille;
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
    cv_rect(8, 4, 8, 10, run ? C_RED : C_ICE_D);   /* play: a square, red; stopped: dim */
    cv_text(22, 1, &FONT_S, s, run ? C_ICE : C_ICE_D);
    {                                           /* who holds the voices: A B, R (ARP), - the keys' */
        uint32_t k, i, x = 134;
        static const char L[4] = {'-', 'A', 'B', 'R'};
        static const uint8_t ORD[4] = {SD_SRC_A, SD_SRC_B, SD_SRC_ARP, SD_SRC_HAND};
        for (k = 0; k < 4u; k++)
            for (i = 0; i < eng.quota[ORD[k]] && x < 166u; i++, x += 8u) {
                char c[2] = {L[ORD[k]], 0};
                cv_text((int32_t)x, 1, &FONT_S, c, ORD[k] == SD_SRC_ARP ? C_RED : ORD[k] == SD_SRC_HAND ? C_ICE_D : C_ICE);
            }
    }
    p = put(s, "CPU ");
    if (pm == 0xFFFFFFFFu) p = put(p, "--");
    else { p = put_u(p, (pm + 5u) / 10u); *p++ = '%'; }   /* (whole percents: ARP fits beside) */
    *p = 0;
    cv_text(232 - text_w(&FONT_S, s), 1, &FONT_S, s, pm != 0xFFFFFFFFu && pm >= 500u ? C_RED : C_ICE_D);
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
    draw_header(); cv_show(0, 0);
    draw_info(); cv_show(22, 1);
    draw_nav(); cv_show(Y_NAV, 2);
    static uint32_t listed;                     /* the list was up: the two rows between the knobs and the picture
                                                   (neither draws them) cleared when it closes */
    if (browse_at || dlist_at) {                /* PRESETS or a DEST turning: the list over the knobs and the picture */
        if (browse_at) draw_browse(); else draw_dlist();
        cv_show(Y_KNOB, 3);
        shown[4] = ~0u;
        listed = 1;
    } else {
        if (listed) { lcd_fill(0, Y_KNOB + H_KNOB, 240, Y_PIC - Y_KNOB - H_KNOB, C_BG); listed = 0; }
        draw_knobs(); cv_show(Y_KNOB, 3);
        cv_begin(240, H_PIC, C_BG);
        if (save_mode || save_pending || saved_at) draw_save(); else pic();
        cv_show(Y_PIC, 4);
    }
    draw_hint(); cv_show(Y_HINT, 5);
    draw_steps(); cv_show(Y_STEPS, 6);
    draw_status(); cv_show(Y_STAT, 7);
}

/* ---- the key LEDs show what the keys do on this page (the user: "use the keys' lights for what is on the screen"):
 * playing, the notes on (full) and ringing out (a glow); ARP, its notes glow. SEQ and TURING: the bank's steps on the
 * white keys (a note full, a rest a glow, past the length dark; the playhead flips its key), the black keys' actions
 * a glow (SEG full while on); SCL held there: the transpose, C4 a glow (0), the track's place full, blinking until the
 * one takes it. */
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
    if (page == PAGE_ORB && !seq_rec) {       /* ORBIT: its steps on the first white keys (picked full, the one
                                                   playing flips), SETUP the last black key */
        uint32_t len = SD_ORB_LEN(&orbp), play = eng.seq_run && orbp.on ? eng.orb_shown : 0xFFu;
        for (k = 0; k < 27u; k++) {
            int32_t w = key_white(k), bk = key_black(k);
            uint32_t on;
            if (bk == BK_SEG) { if (orb_setup) full |= 1u << k; else glow |= 1u << k; continue; }
            if (w < 0 || (uint32_t)w >= len) continue;
            on = (uint32_t)w == orb_cur;
            if ((uint32_t)w == play) on = !on;
            if (on) full |= 1u << k; else glow |= 1u << k;
        }
    } else if (seq_page() && !seq_rec && shift()) {
        int32_t xn = eng.t[seq_trk].xpose_next, xc = eng.t[seq_trk].xpose, z = 60 - (int32_t)base;
        if (z >= 0 && z < 27) glow |= 1u << z;
        if (z + xc >= 0 && z + xc < 27) glow |= 1u << (z + xc);
        if (z + xn >= 0 && z + xn < 27 && (xn == xc || (fm1_ms / 200u) & 1u)) full |= 1u << (z + xn);
    } else if (seq_page() && !seq_rec) {
        uint8_t bd[SD_SEGS + 1u];
        uint32_t sc = sd_seg_bounds(&seqp, bd);
        for (k = 0; k < 27u; k++) {
            int32_t w = key_white(k), bk = key_black(k);
            if (w >= 0) {
                i = seq_bank * 16u + (uint32_t)w;
                if (i >= seqp.len) continue;
                if (seq_segmode) {
                    uint32_t g, start = 0;
                    for (g = 0; g < sc; g++) start |= bd[g] == i;
                    if (start) full |= 1u << k; else glow |= 1u << k;
                } else {
                    uint32_t on = seqp.step[i].type < SD_ST_REST;
                    if (eng.seq_run && i == eng.t[seq_trk].seq_shown) on = !on;   /* the playhead */
                    if (on) full |= 1u << k; else glow |= 1u << k;
                }
            } else if (bk == BK_SEG) {
                if (seq_segmode) full |= 1u << k; else glow |= 1u << k;
            } else if (seq_segmode) {
                if (bk >= 0 && (uint32_t)bk < sc) { if ((uint32_t)bk == seq_dseg) full |= 1u << k; else glow |= 1u << k; }
                else if (bk == 6) glow |= 1u << k;
            } else if (bk >= 0 && bk <= 2) glow |= 1u << k;
        }
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

/* MIDI clock and transport (2026-11-10; SYNC AUTO, SCL + PLAY: INT): the pulses to the engine (sd_clk_tick), START
 * and CONTINUE from step 1 at the next pulse (the downbeat; with none in 250 ms, at once), STOP. CONTINUE starts
 * from the top too: the song position is not passed on. Two clocks (USB and the jack; in the browser, two of its
 * MIDI inputs): the first, until it has been quiet for 500 ms (the same clock on both leads once: counted twice it
 * was twice the tempo, the user heard 100 BPM play at 200 in the browser, 2026-11-10) */
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
/* A note from MIDI (2026-11-11, the user, of a keyboard on the TRS jack: "most interesting is to record into the
 * SEQ, the ARP"): played as the keys' play, on every page (ARP takes it while on, the voices' rules as theirs), and in
 * REC written as theirs, at its velocity: stopped, step by step (held together, a chord; all up, the next step),
 * playing, into the step that plays; what is heard (sd_seq_rec_note). The keys' and MIDI's held notes count together */
static void midi_note(uint32_t n, uint32_t v)
{
    uint32_t bit = 1u << (n & 31u), *w = &midi_held[n >> 5 & 3u];
    if (v) {
        uint32_t others = midi_hn || fm1_in.notes;
        sd_engine_note(&eng, n, v);
        if (!(*w & bit)) { *w |= bit; midi_hn++; }
        if (seq_rec) {
            uint32_t rn = sd_seq_rec_note(&eng, &eng.t[seq_trk], n);
            if (rec_at != 0xFFu && others) seq_chord(rn);
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
#ifndef DINGHY_HOST
    eng.pl_post = sd_c1_post;                   /* (before SPACE has its memory: no block ever runs here) */
    eng.pl_join = sd_c1_join;
#if FM1_CPU1
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
    if (settings_load(&set, sizeof set) || (int8_t)set.oct < -2 || (int8_t)set.oct > 2 || set.bpm < 40u ||
        set.bpm > 240u || set.patch >= NPATCHES || set.mode >= SD_MODES || set.pattern >= UPATTERNS)
        set.oct = 0, set.bpm = 120, set.patch = SD_PATCH_WORST, set.mode = SD_MODE_MONO, set.pattern = 0;
    if (set.amode >= SD_AMODES || set.adiv >= SD_SDIVS || set.aoct < 1u || set.aoct > 4u || set.agate < 1u ||
        set.agate > 100u || set.ahold > 1u)
        set.amode = SD_AM_UP, set.adiv = 2, set.aoct = 1, set.agate = 50, set.ahold = 0;
    if (set.xchrom > 1u) set.xchrom = 0;
    if (set.sync_int > 1u) set.sync_int = 0;
    if (set.master > 1u) set.master = 0;
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
    seq_push();
    orb_load(set.pattern);
    sd_pattern_empty(&seq_other);               /* track B: silent until it has notes */
    sd_engine_pattern_to(&eng, 1, &seq_other);
    eng.seqb = 1;
#ifdef SD_CPU_WORST                             /* a measure, never shipped (DINGHY_DEFS=-DSD_CPU_WORST=1 ./build.sh, then
                                                   tools/emu_cpu.sh with keys held for ARP): the costliest patch with
                                                   every engine on, POLY, both tracks (B in fifths), ARP; 1: also all 8
                                                   slots on what costs most, from fast LFOs, TURING to PITCH (quantized
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
        sq = seq_page() && !seq_rec;            /* the keys are the steps there */
        oq = page == PAGE_ORB && !seq_rec;      /* ORBIT's steps there */
        if (sq && shift()) {                    /* SCL held there: a key transposes the track, at its next one */
            for (k = 0; k < 27u; k++)
                if (down >> k & 1u) {
                    int32_t x = (int32_t)(base + k) - 60;
                    eng.t[seq_trk].xpose_next = (int8_t)(x < -24 ? -24 : x > 24 ? 24 : x);
                }
        } else if (sq) {
            seq_keys(down);
        } else if (oq) {                        /* ORBIT: the white keys its steps, the last black key SETUP */
            for (k = 0; k < 27u; k++)
                if (down >> k & 1u) {
                    int32_t w = key_white(k), bk = key_black(k);
                    if (bk == BK_SEG) orb_setup = !orb_setup;
                    else if (w >= 0 && (uint32_t)w < SD_ORB_LEN(&orbp)) orb_cur = (uint32_t)w;
                }
        }
        if (down && !sq && !oq && !seq_rec && !eng.arp_on && eng.seq_run && !eng.quota[SD_SRC_HAND])
            msg(set.mode == SD_MODE_MONO ? "MONO: THE SEQ HAS THE VOICE" : "NO FREE VOICE: SEQ PLAYS");
        for (k = 0; k < 27u; k++) {
            if (down >> k & 1u && !sq && !oq) {
                uint32_t others = keys & ~(1u << k) & ~down;   /* keys already held: this one joins their chord */
                held[k] = (uint8_t)(base + k);
                sd_engine_note(&eng, held[k], 100);
                midi_send(0x90u, held[k], 100);
                if (seq_rec) {                  /* (what is heard: back by the track's transpose) */
                    uint32_t rn = sd_seq_rec_note(&eng, &eng.t[seq_trk], held[k]);
                    if (rec_at != 0xFFu && (others || midi_hn || (down & ((1u << k) - 1u)))) seq_chord(rn);
                    else seq_write(rn, SD_ST_NOTE);
                }
            }
            if (up >> k & 1u && held[k]) { sd_engine_note(&eng, held[k], 0); midi_send(0x80u, held[k], 0); held[k] = 0; }
            if (keys >> k & 1u) lit |= 1u << k;
        }

        if (seq_rec && !keys && !midi_hn) seq_keys_up();   /* REC: every key (and MIDI's) up, the next step */
        b = fm1_input_edges(&rel);
        if (b) dlist_at = 0;                    /* (a button: the destinations' list closes) */
        if (b && browse_at) {                   /* the list open: EDIT loads, HOME not now; another button closes it */
            if (b & (1u << BTN_EDIT)) {
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
            b &= ~(1u << BTN_EDIT | 1u << BTN_HOME);
            browse_at = 0;
        }
        if (b & (1u << BTN_PLAY)) {
            if (shift()) {                      /* SCL + PLAY: SYNC, AUTO (a MIDI clock leads when it comes) or INT */
                set.sync_int = (uint8_t)!set.sync_int;
                clk_cue = 0;
                msg(set.sync_int ? "SYNC INT: MIDI CLOCK IGNORED" : "SYNC AUTO: A MIDI CLOCK LEADS");
                dirty = 1;
                t_change = fm1_ms;
            } else
                eng.seq_run = (uint8_t)!eng.seq_run;
        }
        if (b & (1u << BTN_GLO)) {              /* the mode; MONO holds SEQ B */
            set.mode = (uint8_t)((set.mode + 1u) % SD_MODES);
            eng.mode = set.mode;
            if (set.mode == SD_MODE_MONO && !eng.t[1].mute && trk_has_notes(1)) msg("MONO: SEQ B WAITS");
            dirty = 1;
            t_change = fm1_ms;
        }
        if (b & (1u << BTN_SAVE)) { if (save_mode) save_commit(); else save_begin(); }
        if (save_mode && ((b & (1u << BTN_HOME)) || fm1_ms - save_at > 10000u)) {   /* HOME: not now */
            save_mode = 0;
            b &= ~(1u << BTN_HOME);
        }
        if (b & (1u << BTN_REC)) {
            if (page == PAGE_ARP && !seq_rec) arp_keep();   /* on ARP: KEEP what it played */
            else {
                seq_rec = !seq_rec;
                rec_at = 0xFFu;
                if (seq_rec && seq_trk && set.mode == SD_MODE_MONO) msg("B RECORDS, PLAYS IN DUO/POLY");
            }
        }
        if (seq_rec && (b & (1u << BTN_ENV))) { seq_write(0, SD_ST_TIE); b &= ~(1u << BTN_ENV); }
        if (seq_rec && (b & (1u << BTN_LFO))) { seq_write(0, SD_ST_REST); b &= ~(1u << BTN_LFO); }
        /* the pages, each its button's, the button again the next of them (the top line shows them): HOME the voice and
           the macros, FX the effects, ENV the envelopes and the filter, LFO the LFOs, TURING and the matrix, ARP, SEQ
           the tracks and ORBIT */
        {
            uint32_t was = page;
            if (b & (1u << BTN_SEQ)) {
                if ((seq_page() || page == PAGE_ORB) && shift()) {   /* SCL + SEQ there: the MASTER, A or B */
                    set.master = (uint8_t)!set.master;
                    msg(set.master ? "MASTER: SEQ B, ITS ONE LEADS" : "MASTER: SEQ A, ITS ONE LEADS");
                    dirty = 1;
                    t_change = fm1_ms;
                }
                else if (!seq_page() && page != PAGE_ORB) { page = PAGE_SEQ; seq_track(0); }
                else if (page == PAGE_SEQ && !seq_trk) seq_track(1);
                else if (page == PAGE_SEQ) { page = PAGE_ORB; seq_track(0); orb_setup = 0; }
                else { page = PAGE_SEQ; seq_track(0); }   /* ORBIT -> SEQ A: round its own pages, as every page
                                                   button (2026-11-10, the user: it went by SPACE, the page before) */
            }
            if (b & (1u << BTN_HOME)) page = page == 0u ? 5u : 0u;
            if (b & (1u << BTN_FX)) page = page == 1u ? 2u : page == 2u ? 3u : page == 3u ? PAGE_TIME : 1u;
            if (b & (1u << BTN_ENV)) page = page == PAGE_ENV ? PAGE_FILT : PAGE_ENV;
            if (b & (1u << BTN_LFO)) {          /* LFO 1, LFO 2, the slots (the one last picked) */
                if (page != 4u) dmod = SD_MODS;
                else if (dmod == SD_MODS) dmod = SD_MODS + 1u;
                else if (dmod == SD_MODS + 1u) dmod = DMOD_TUR;
                else if (dmod == DMOD_TUR) dmod = dslot;
                else dmod = SD_MODS;
                page = 4u;
            }
            if (b & (1u << BTN_ARP)) page = PAGE_ARP;
            if (page != was) { seq_segmode = 0; seq_more = 0; }
        }
        if (b & (1u << BTN_EDIT)) {             /* EDIT: what this page shows, on / off */
            if (shift() && (seq_page() || page == PAGE_ORB)) {   /* SCL + EDIT there: the transposes IN KEY or CHROM */
                set.xchrom = (uint8_t)!set.xchrom;
                msg(set.xchrom ? "XPOSE: CHROMATIC" : "XPOSE: IN SEQ A'S KEY");
                dirty = 1;
                t_change = fm1_ms;
            } else if (shift() && page == PAGE_TIME) {   /* SCL + EDIT on TIME: the delay's MODE, the next of four
                                                   (SHIMMER, the top bits, kept) */
                static const char *const MM[4] = {"MODE: FREE (MS)", "MODE: SYNC", "MODE: FREE, PING-PONG", "MODE: SYNC, PING-PONG"};
                pat.s_mode = (uint8_t)((pat.s_mode & ~3u) | ((pat.s_mode + 1u) & 3u));
                msg(MM[pat.s_mode & 3u]);
                sd_engine_patch(&eng, &pat);
            } else if (edit_toggle()) sd_engine_patch(&eng, &pat);
        }
        if (seq_page() && shift()) {            /* SCL + OCT on SEQ: the step bank, the step edited along at its place
                                                   in it (OCT alone is the octave there too, REC or not) */
            uint32_t last = (seqp.len - 1u) / 16u, nb = seq_bank;
            if (b & (1u << BTN_OCT_DOWN) && nb) nb--;
            if (b & (1u << BTN_OCT_UP) && nb < last) nb++;
            if (nb != seq_bank) {
                seq_bank = nb;
                seq_cur = nb * 16u + seq_cur % 16u;
                if (seq_cur >= seqp.len) seq_cur = seqp.len - 1u;
            }
            b &= ~(1u << BTN_OCT_DOWN | 1u << BTN_OCT_UP);
        }
        if (page == PAGE_TIME && shift() && b & (1u << BTN_OCT_DOWN | 1u << BTN_OCT_UP)) {   /* SCL + OCT on TIME:
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
        if ((e = fm1_enc_take(ENC_SELECT)) != 0 && eng.clk_on) msg("TEMPO: THE MIDI CLOCK'S");
        else if (e) {
            int32_t t = (int32_t)set.bpm + e;
            set.bpm = (uint8_t)(t < 40 ? 40 : t > 240 ? 240 : t);
            eng.bpm = set.bpm;
            dirty = 1;
            t_change = fm1_ms;
        }

        {
            uint32_t changed = 0, l2 = shift();
            if ((e = fm1_enc_take(ENC_PRESETS)) != 0) {   /* the list (the sound as it is): EDIT loads */
                uint32_t kind = seq_page() || page == PAGE_ORB ? 2u : 1u;   /* (SEQ: the patterns, P01..P32) */
                if (!browse_at || browse_kind != kind) { browse_kind = kind; browse_to = kind == 2u ? set.pattern : set.patch; }
                while (e) {                     /* a detent a place (the patches past the empty user places) */
                    browse_to = browse_step(browse_to, e > 0 ? 1 : -1);
                    e += e > 0 ? -1 : 1;
                }
                browse_at = fm1_ms ? fm1_ms : 1u;
            }
            if (save_mode && (e = fm1_enc_take(ENC_ALGORITHM)) != 0) {   /* SAVE: where to */
                save_to = step_mod(save_to, e, save_mode == 2u ? UPATTERNS : UPATCHES);
                save_at = fm1_ms;
            }
            if (seq_page()) seq_knobs();        /* (it takes every encoder but PRESETS: the rest below sees 0) */
            if (page == PAGE_ORB) orb_knobs();  /* (so does ORBIT's) */
            if ((e = fm1_enc_take(ENC_ALGORITHM)) != 0) {   /* ALGORITHM: the page's choice (the hint line says it) */
                switch (page) {
                case 0:
                    if (l2) { ebank = step_mod(ebank, e, SD_BANKS); gen = 0xFFFFFFFFu; }   /* SCL: the bank */
                    else {                      /* the bank's roles: the algorithms */
                        uint32_t a = algo_of(&pat);
                        a = a >= SD_ALGOS ? 0u : step_mod(a, e, SD_ALGOS);
                        for (k = 0; k < SD_OPS; k++) pat.bank[ebank].osc[k].role = sd_role_of(SD_ALGO[a][k]);
                        changed = 1;
                    }
                    break;
                case 5: pat.mac4 = (uint8_t)step_mod(pat.mac4 % SD_M4S, e, SD_M4S); changed = 1; break;
                case 1:
                    if (l2) pat.r_low = knob_q(pat.r_low, e);   /* SCL: LOW */
                    else pat.r_track = (uint8_t)step_mod(pat.r_track & 3u, e, 4u);   /* TRACK */
                    changed = 1;
                    break;
                case 2:
                    if (!l2) dband = step_mod(dband, e, 5u);   /* the band, then LOOP */
                    else if (dband < 4u) {      /* SCL: its BODY (where PHAT and SHARP act) */
                        pat.d_body[dband] = knob_q(pat.d_body[dband], e);
                        changed = 1;
                    }
                    break;
                case PAGE_FILT:
                    if (l2) {                   /* SCL: VEL */
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
                    else if (dmod == DMOD_TUR) {   /* TURING: its DIV */
                        pat.tur_ld = (uint8_t)((pat.tur_ld & 31u) | (step_lim(SD_TUR_DIV(&pat), e, SD_TUR_DIVS) + 1u) << 5);
                        changed = 1;
                    }
                    break;
                case 3:                         /* SPACE: FREEZE; SCL: SHIMMER */
                    if (l2) {
                        int32_t sh = (int32_t)SD_SP_SHIM(&pat) + e;
                        sh = sh < 0 ? 0 : sh > 63 ? 63 : sh;
                        pat.s_mode = (uint8_t)((pat.s_mode & 3u) | (uint32_t)sh << 2);
                        changed = 1;
                    } else {
                        sp_frz = (uint8_t)!sp_frz;
                        msg(sp_frz ? "FREEZE: THE TAIL HELD" : "FREEZE OFF");
                    }
                    break;
                case PAGE_TIME:                 /* TIME: LOOP, the looper's (its last LENGTH round and round); SCL:
                                                   REVERSE (it plays it backwards) */
                    if (l2) {
                        lp_rev = (uint8_t)!lp_rev;
                        msg(lp_rev ? "REVERSE: THE LOOP BACKWARDS" : "REVERSE OFF");
                    } else {
                        lp_on = (uint8_t)!lp_on;
                        msg(lp_on ? (lp_lvl ? "LOOP: THE LAST LENGTH HELD" : "LOOP: L-LEVEL IS 0") : "LOOP OFF");
                    }
                    break;
                default: break;                 /* ENV, the LFOs: nothing */
                }
            }
            if (page == PAGE_ENV || (page == PAGE_FILT && l2)) {   /* an envelope: A D S R, 0..127 */
                static const uint8_t KN[4] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
                uint8_t *ev = page == PAGE_FILT ? &pat.e3_a : l2 ? &pat.e2_a : &pat.e1_a;
                for (k = 0; k < 4u; k++)
                    if ((e = fm1_enc_take(KN[k])) != 0) {
                        int32_t v = (int32_t)ev[k] + e * 2;
                        ev[k] = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
                        changed = 1;
                    }
            } else if (page == PAGE_FILT) {     /* FILTER: CUTOFF RES ENV KEY */
                if ((e = fm1_enc_take(ENC_KNOB1)) != 0) {   /* a quarter semitone a detent */
                    int32_t v = (int32_t)pat.f_cut + e * 147;   /* (65535 / 114 / 4) */
                    pat.f_cut = (uint16_t)(v < 0 ? 0 : v > 65535 ? 65535 : v);
                    changed = 1;
                }
                if ((e = fm1_enc_take(ENC_KNOB2)) != 0) pat.f_res = knob_q(pat.f_res, e), changed = 1;
                if ((e = fm1_enc_take(ENC_KNOB3)) != 0) {   /* -99 .. +99 % */
                    int32_t v = amt99(pat.f_env) + e;
                    v = v < -99 ? -99 : v > 99 ? 99 : v;
                    pat.f_env = (int16_t)(v * 32767 / 99);
                    changed = 1;
                }
                if ((e = fm1_enc_take(ENC_KNOB4)) != 0) {
                    int32_t v = (int32_t)pat.f_key + e * 5;
                    pat.f_key = (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v);
                    changed = 1;
                }
            } else if (page == PAGE_ARP) {      /* ARP: MODE RATE OCT GATE (the player's, not the patch's) */
                uint32_t d0 = dirty;
                if ((e = fm1_enc_take(ENC_KNOB1)) != 0) set.amode = (uint8_t)step_mod(set.amode % SD_AMODES, e, SD_AMODES), dirty = 1;
                if ((e = fm1_enc_take(ENC_KNOB2)) != 0) { set.adiv = (uint8_t)sdiv_step(set.adiv % SD_SDIVS, e); dirty = 1; }
                if ((e = fm1_enc_take(ENC_KNOB3)) != 0) {
                    int32_t v = (int32_t)set.aoct + e;
                    set.aoct = (uint8_t)(v < 1 ? 1 : v > 4 ? 4 : v);
                    dirty = 1;
                }
                if ((e = fm1_enc_take(ENC_KNOB4)) != 0) {
                    int32_t v = (int32_t)set.agate + e * 5;
                    set.agate = (uint8_t)(v < 5 ? 5 : v > 100 ? 100 : v);
                    dirty = 1;
                }
                if (dirty && !d0) t_change = fm1_ms;
            } else if (page == 5u) {            /* MACROS: MUTATE MOVE BREAK MACRO 4 */
                static const uint8_t KN[4] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
                for (k = 0; k < 4u; k++)
                    if ((e = fm1_enc_take(KN[k])) != 0) pat.mac[k] = knob_q(pat.mac[k], e), changed = 1;
            } else if (page == 4u && dmod == DMOD_TUR) {   /* TURING: CHANGE LENGTH DEST AMOUNT */
                if ((e = fm1_enc_take(ENC_KNOB1)) != 0) {
                    pat.tur_set = (uint8_t)((pat.tur_set & 0x80u) | step_lim(SD_TUR_CHANGE(&pat), e, 101u));
                    dlist_at = 0;
                    changed = 1;
                }
                if ((e = fm1_enc_take(ENC_KNOB2)) != 0) {
                    pat.tur_ld = (uint8_t)((pat.tur_ld & 0xE0u) | (2u + step_lim(SD_TUR_LEN(&pat) - 2u, e, 15u)));
                    dlist_at = 0;
                    changed = 1;
                }
                if ((e = fm1_enc_take(ENC_KNOB3)) != 0) {   /* the matrix's destinations, then NOTES: the list */
                    pat.tur_dst = (uint8_t)dst_step(pat.tur_dst, e, 1);
                    dlist_open(DL_TUR);
                    changed = 1;
                }
                if ((e = fm1_enc_take(ENC_KNOB4)) != 0) {   /* a percent a detent */
                    int32_t v = (int32_t)pat.tur_amt + e;
                    dlist_at = 0;
                    pat.tur_amt = (int8_t)(v < -100 ? -100 : v > 100 ? 100 : v);
                    changed = 1;
                }
            } else if (page == 4u && dmod < SD_MODS) {   /* MOD, a slot: SOURCE DEST AMOUNT */
                SdModSlot *m = &pat.mod[dmod];
                if ((e = fm1_enc_take(ENC_KNOB1)) != 0) m->src = (uint8_t)step_mod(m->src % SD_MSRCS, e, SD_MSRCS), dlist_at = 0, changed = 1;
                if ((e = fm1_enc_take(ENC_KNOB2)) != 0) m->dst = (uint8_t)dst_step(m->dst, e, 0), dlist_open(DL_MOD), changed = 1;
                if ((e = fm1_enc_take(ENC_KNOB3)) != 0) {   /* a percent a detent */
                    int32_t v = amt99(m->amt) + e;
                    dlist_at = 0;
                    v = v < -99 ? -99 : v > 99 ? 99 : v;
                    m->amt = (int16_t)(v * 32767 / 99);
                    changed = 1;
                }
                (void)fm1_enc_take(ENC_KNOB4);
            } else if (page == 4u) {            /* an LFO: SHAPE RATE MODE */
                uint32_t lk = dmod - SD_MODS;
                if ((e = fm1_enc_take(ENC_KNOB1)) != 0) pat.lfo_shape[lk] = (uint8_t)step_mod(pat.lfo_shape[lk] % SD_LSHAPES, e, SD_LSHAPES), changed = 1;
                if ((e = fm1_enc_take(ENC_KNOB2)) != 0) {
                    if (pat.lfo_mode[lk] & SD_LM_SYNC) {   /* a division a detent */
                        int32_t d = (int32_t)((pat.lfo_rate[lk] * SD_LFO_DIVS) >> 16) + e;
                        d = d < 0 ? 0 : d > SD_LFO_DIVS - 1 ? SD_LFO_DIVS - 1 : d;
                        pat.lfo_rate[lk] = (uint16_t)((uint32_t)d * 65536u / SD_LFO_DIVS + 100u);
                    } else {                    /* 0..99 as the other knobs: 0.02 .. 40 Hz, ~8 % a detent */
                        pat.lfo_rate[lk] = knob_q(pat.lfo_rate[lk], e);
                    }
                    changed = 1;
                }
                if ((e = fm1_enc_take(ENC_KNOB3)) != 0) pat.lfo_mode[lk] = LMODE_BITS[step_mod(lmode_of(pat.lfo_mode[lk]), e, LMODES)], changed = 1;
                (void)fm1_enc_take(ENC_KNOB4);
            } else if (page == 3u) {            /* SPACE: the plate (SCL: SHIMMER) */
                if (l2) {
                    if ((e = fm1_enc_take(ENC_KNOB1)) != 0) {
                        int32_t sh = (int32_t)SD_SP_SHIM(&pat) + e;
                        sh = sh < 0 ? 0 : sh > 63 ? 63 : sh;
                        pat.s_mode = (uint8_t)((pat.s_mode & 3u) | (uint32_t)sh << 2);
                        changed = 1;
                    }
                    (void)fm1_enc_take(ENC_KNOB2);
                    (void)fm1_enc_take(ENC_KNOB3);
                    (void)fm1_enc_take(ENC_KNOB4);
                } else {
                    if ((e = fm1_enc_take(ENC_KNOB1)) != 0) pat.s_size = knob_q(pat.s_size, e), changed = 1;
                    if ((e = fm1_enc_take(ENC_KNOB2)) != 0) pat.s_decay = knob_q(pat.s_decay, e), changed = 1;
                    if ((e = fm1_enc_take(ENC_KNOB3)) != 0) pat.s_rtone = knob_q(pat.s_rtone, e), changed = 1;
                    if ((e = fm1_enc_take(ENC_KNOB4)) != 0) pat.s_rlevel = knob_q(pat.s_rlevel, e), changed = 1;
                }
            } else if (page == PAGE_TIME) {     /* TIME: the delay (SCL: its MODE, the looper) */
                if (l2) {
                    if ((e = fm1_enc_take(ENC_KNOB1)) != 0) {   /* GLIDE: OFF, then a division a detent synced,
                                                   ~16 ms free */
                        if (pat.s_mode & SD_SP_SYNC) {
                            int32_t d = (int32_t)sd_lp_glide_div(lp_glide) + e;
                            d = d < 0 ? 0 : d > (int32_t)SD_LP_NGLIDE - 1 ? (int32_t)SD_LP_NGLIDE - 1 : d;
                            lp_glide = (uint16_t)((uint32_t)d * 65536u / SD_LP_NGLIDE + (d ? 100u : 0u));
                        } else {
                            int32_t t = (int32_t)lp_glide + e * 256;
                            lp_glide = (uint16_t)(t < 0 ? 0 : t > 65535 ? 65535 : t);
                        }
                    }
                    if ((e = fm1_enc_take(ENC_KNOB2)) != 0) {   /* LENGTH: as TIME, a division a detent synced */
                        if (pat.s_mode & SD_SP_SYNC) {
                            int32_t d = (int32_t)sd_lp_div(lp_len) + e;
                            d = d < 0 ? 0 : d > (int32_t)SD_LP_NDIV - 1 ? (int32_t)SD_LP_NDIV - 1 : d;
                            lp_len = (uint16_t)((uint32_t)d * 65536u / SD_LP_NDIV + 100u);
                        } else {
                            int32_t t = (int32_t)lp_len + e * 256;
                            lp_len = (uint16_t)(t < 0 ? 0 : t > 65535 ? 65535 : t);
                        }
                    }
                    if ((e = fm1_enc_take(ENC_KNOB3)) != 0) {   /* SPEED: an eighth of a semitone a detent */
                        int32_t v = lp_speed + e * 32;
                        lp_speed = (int16_t)(v < -3072 ? -3072 : v > 3072 ? 3072 : v);
                    }
                    if ((e = fm1_enc_take(ENC_KNOB4)) != 0) {   /* L-LEVEL: above 0 the looper armed (borrowing:
                                                   the delay's line's second half, the delay 0.5 s) */
                        uint16_t was = lp_lvl;
                        lp_lvl = (uint16_t)knob_q(lp_lvl, e);
                        if (eng.lp_borrow && !was != !lp_lvl)
                            msg(lp_lvl ? "LOOPER ON: DELAY 0.5 S" : "LOOPER OFF: DELAY 1 S");
                    }
                } else {
                    if ((e = fm1_enc_take(ENC_KNOB1)) != 0) {   /* TIME: a division a detent synced, ~4 ms free */
                        if (pat.s_mode & SD_SP_SYNC) {
                            int32_t d = (int32_t)sd_sp_div(pat.s_time) + e;
                            d = d < 0 ? 0 : d > (int32_t)SD_SP_NDIV - 1 ? (int32_t)SD_SP_NDIV - 1 : d;
                            pat.s_time = (uint16_t)((uint32_t)d * 65536u / SD_SP_NDIV + 100u);
                        } else {
                            int32_t t = (int32_t)pat.s_time + e * 256;
                            pat.s_time = (uint16_t)(t < 0 ? 0 : t > 65535 ? 65535 : t);
                        }
                        changed = 1;
                    }
                    if ((e = fm1_enc_take(ENC_KNOB2)) != 0) pat.s_fb = knob_q(pat.s_fb, e), changed = 1;
                    if ((e = fm1_enc_take(ENC_KNOB3)) != 0) pat.s_dtone = knob_q(pat.s_dtone, e), changed = 1;
                    if ((e = fm1_enc_take(ENC_KNOB4)) != 0) pat.s_dlevel = knob_q(pat.s_dlevel, e), changed = 1;
                }
            } else if (page == 2u) {            /* DISTROY, the band ALGORITHM picks */
                uint32_t k2 = dband;
                if (k2 == 4u && l2) {           /* LOOP, SCL: the whole of DISTROY: MIX, XOVER */
                    if ((e = fm1_enc_take(ENC_KNOB1)) != 0) pat.d_mix = knob_q(pat.d_mix, e), changed = 1;
                    if ((e = fm1_enc_take(ENC_KNOB2)) != 0) pat.d_xover = e > 0 ? SD_DX_LR4 : SD_DX_CMP, changed = 1;
                    (void)fm1_enc_take(ENC_KNOB3);
                    (void)fm1_enc_take(ENC_KNOB4);
                } else if (k2 == 4u) {          /* LOOP (D3): FDBK TIME SPREAD TAME */
                    if ((e = fm1_enc_take(ENC_KNOB1)) != 0) pat.d_fb = knob_q(pat.d_fb, e), changed = 1;
                    if ((e = fm1_enc_take(ENC_KNOB2)) != 0) pat.d_ftime = knob_q(pat.d_ftime, e), changed = 1;
                    if ((e = fm1_enc_take(ENC_KNOB3)) != 0) pat.d_spread = knob_q(pat.d_spread, e), changed = 1;
                    if ((e = fm1_enc_take(ENC_KNOB4)) != 0) pat.d_tame = knob_q(pat.d_tame, e), changed = 1;
                } else if (l2) {
                    if ((e = fm1_enc_take(ENC_KNOB4)) != 0) {   /* the band's upper split, between its neighbours */
                        uint32_t si = k2 < 3u ? k2 : 2u;
                        int32_t lo = si ? pat.d_split[si - 1u] + 256 : SD_R_LO * 256 + 256;
                        int32_t hi = si < 2u ? pat.d_split[si + 1u] - 256 : SD_R_HI * 256 - 256;
                        int32_t v = (int32_t)pat.d_split[si] + e * 64;   /* a quarter semitone a detent */
                        pat.d_split[si] = (uint16_t)(v < lo ? lo : v > hi ? hi : v);
                        changed = 1;
                    }
                    if ((e = fm1_enc_take(ENC_KNOB1)) != 0) {   /* BIAS: -50 .. +50 */
                        int32_t v = pat.d_bias[k2] + e * 655;
                        pat.d_bias[k2] = (int16_t)(v < -32767 ? -32767 : v > 32767 ? 32767 : v);
                        changed = 1;
                    }
                    if ((e = fm1_enc_take(ENC_KNOB2)) != 0) {
                        int32_t v = pat.d_pan[k2] + e * 656;
                        pat.d_pan[k2] = (int16_t)(v < -32767 ? -32767 : v > 32767 ? 32767 : v);
                        changed = 1;
                    }
                    if ((e = fm1_enc_take(ENC_KNOB3)) != 0) {   /* DYNAMIC: PHAT -99 .. 0 .. +99 SHARP */
                        int32_t v = pat.d_shape[k2] + e * 331;
                        pat.d_shape[k2] = (int16_t)(v < -32767 ? -32767 : v > 32767 ? 32767 : v);
                        changed = 1;
                    }
                } else {                        /* TYPE DRIVE ALT LEVEL */
                    if ((e = fm1_enc_take(ENC_KNOB1)) != 0) {
                        pat.d_type[k2] = (uint8_t)step_mod(pat.d_type[k2] % SD_DSHAPES, e, SD_DSHAPES);
                        changed = 1;
                    }
                    if ((e = fm1_enc_take(ENC_KNOB2)) != 0) pat.d_gain[k2] = knob_q(pat.d_gain[k2], e), changed = 1;
                    if ((e = fm1_enc_take(ENC_KNOB3)) != 0) pat.d_alt[k2] = knob_q(pat.d_alt[k2], e), changed = 1;
                    if ((e = fm1_enc_take(ENC_KNOB4)) != 0) pat.d_level[k2] = knob_q(pat.d_level[k2], e), changed = 1;
                }
            } else if (page == 1u) {            /* the Reactor */
                if (l2) {
                    if ((e = fm1_enc_take(ENC_KNOB1)) != 0) {
                        int32_t f = pat.r_freq + e * 256;
                        pat.r_freq = (int16_t)(f < -48 * 256 ? -48 * 256 : f > 48 * 256 ? 48 * 256 : f);
                        changed = 1;
                    }
                    if ((e = fm1_enc_take(ENC_KNOB2)) != 0) pat.r_model = (uint8_t)step_mod(pat.r_model, e, SD_RMODELS), changed = 1;
                    if ((e = fm1_enc_take(ENC_KNOB3)) != 0) {
                        int32_t d = (int32_t)pat.r_div + e;
                        pat.r_div = (uint8_t)(d < 0 ? 0 : d > 16 ? 16 : d);
                        changed = 1;
                    }
                    if ((e = fm1_enc_take(ENC_KNOB4)) != 0) pat.r_mix = knob_q(pat.r_mix, e), changed = 1;
                } else {
                    if ((e = fm1_enc_take(ENC_KNOB1)) != 0) {   /* POS: a tenth of a stage a detent */
                        int32_t q = (int32_t)pat.r_pos + e * 26;
                        pat.r_pos = (uint16_t)(q < 0 ? 0 : q > 1535 ? 1535 : q);
                        changed = 1;
                    }
                    if ((e = fm1_enc_take(ENC_KNOB2)) != 0) pat.r_res = knob_q(pat.r_res, e), changed = 1;
                    if ((e = fm1_enc_take(ENC_KNOB3)) != 0) pat.r_slew = knob_q(pat.r_slew, e), changed = 1;
                    if ((e = fm1_enc_take(ENC_KNOB4)) != 0) pat.r_drive = knob_q(pat.r_drive, e), changed = 1;
                }
            } else if (page == 0u && l2) {      /* NAV, SCL: X Y WIDTH, K4 a harmonic generator a detent */
                if ((e = fm1_enc_take(ENC_KNOB1)) != 0) pat.x = knob_q(pat.x, e), changed = 1;
                if ((e = fm1_enc_take(ENC_KNOB2)) != 0) pat.y = knob_q(pat.y, e), changed = 1;
                if ((e = fm1_enc_take(ENC_KNOB3)) != 0) pat.width = knob_q(pat.width, e), changed = 1;
                if ((e = fm1_enc_take(ENC_KNOB4)) != 0) {
                    gen = gen >= SD_GENS ? 0u : step_mod(gen, e, SD_GENS);
                    gen_seed += fm1_ms;
                    sd_gen(&pat, ebank, gen, &gen_seed);
                    changed = 1;
                }
            } else if (page == 0u) {            /* NAV: the Q page */
                if ((e = fm1_enc_take(ENC_KNOB1)) != 0) pat.xm = knob_q(pat.xm, e), changed = 1;
                if ((e = fm1_enc_take(ENC_KNOB2)) != 0) pat.warp = knob_q(pat.warp, e), changed = 1;
                if ((e = fm1_enc_take(ENC_KNOB3)) != 0) pat.fold = knob_q(pat.fold, e), changed = 1;
                if ((e = fm1_enc_take(ENC_KNOB4)) != 0) pat.dt = knob_q(pat.dt, e), changed = 1;
            }
            if (changed) sd_engine_patch(&eng, &pat);
        }

        voices_guard();
        eng.rec_trk = seq_rec ? (uint8_t)seq_trk : 0xFFu;   /* REC: the keys may take the recorded track's voice */
        midi_in();
        eng.xkey = (uint8_t)!set.xchrom;        /* the transposes IN KEY (SCL + EDIT on SEQ) */
        eng.master = set.master;                /* the track whose one leads (SCL + SEQ on SEQ) */
        eng.sp_freeze = sp_frz;                 /* SPACE's FREEZE (ALGORITHM there) */
        eng.lp_len = lp_len;                    /* TIME's looper (EDIT OFF on TIME: silent) */
        eng.lp_speed = lp_speed;
        eng.lp_glide = lp_glide;
        eng.lp_lvl = pat.fx_off & SD_FX_TIME ? 0u : lp_lvl;
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
        keys_lit(lit, (uint32_t)((int32_t)KEY0 + 12 * oct));
        {
            const char *what;
            uint32_t sw = sw_of(&what);
            fm1_led_key(BTN_OCT_DOWN, oct < 0);
            fm1_led_key(BTN_OCT_UP, oct > 0);
            fm1_led_key(BTN_PLAY, eng.seq_run && eng.t[0].seq_shown % 4u == 0u);   /* blinks on the beat */
            fm1_led_key(BTN_GLO, eng.mode_on != SD_MODE_MONO);
            fm1_led_key(BTN_SEL, shift());
            fm1_led_key(BTN_HOME, page == 0u || page == 5u);   /* a page's button lit: you are there */
            fm1_led_key(BTN_FX, (page >= 1u && page <= 3u) || page == PAGE_TIME);
            fm1_led_key(BTN_ENV, page == PAGE_ENV || page == PAGE_FILT);
            fm1_led_key(BTN_LFO, page == 4u);
            fm1_led_key(BTN_ARP, page == PAGE_ARP);
            fm1_led_key(BTN_SEQ, seq_page() || page == PAGE_ORB);
            fm1_led_key(BTN_EDIT, browse_at || sw == SW_WAIT ? (fm1_ms / 200u) & 1u : sw == SW_ON);   /* EDIT lit: what this page shows
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
