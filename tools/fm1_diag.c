/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* Star Distroyer's diagnosis over USB, on a Mac: reads what the FM-1 is doing while it plays, and what it kept from its
 * last crash. Read only: it sends the FM-1 two questions, F0 7D 53 44 01 F7 and (the second page: the write watch
 * and each core's record at its last fault, two cores only) F0 7D 53 44 03 F7 (the app's sd_diag_service answers), and nothing
 * else, never the installer's messages; the FM-1 writes nothing for it, and no flash.
 *   cc -O2 -o build/fm1_diag tools/fm1_diag.c -framework CoreMIDI -framework CoreFoundation
 *   build/fm1_diag                       one reading, every field named
 *   build/fm1_diag -l                    the freeze log (2026-11-22): each freeze or crash the FM-1 kept in its flash,
 *                                        the newest first: when, where it stood and was stuck, the inputs before it
 *                                        (F0 7D 53 44 05 <i> F7: record i / 2, its half i & 1)
 *   build/fm1_diag -w [ms] [-o log.txt]  a reading every ms (default 250), a line each (and into the log); when the
 *                                        FM-1 restarts (its uptime back) the crash it kept, in full
 * The FM-1 is the USB MIDI device "Felucca" (Dinghy's name for it). */
#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static const char *const NAME[] = {
    "version", "build", "uptime ms", "CPU (0.1 %)", "mode | PLAY << 8 | page << 16",
    "core 2: on | gave up << 8 | cut << 16", "core 2 bar: magic", "core 2 bar: barred",
    "mailbox: request", "mailbox: complete", "mailbox: rejected", "mailbox: misread", "mailbox: jobs",
    "core 2 fault: count", "core 2 fault: DBG", "core 2 fault: EMU", "core 2 fault: PC", "core 2 fault: RETS",
    "core 2 stack used (B)", "core 2 job: longest (ticks)", "core 2 jobs: all (ticks)", "core 2 jobs: how many",
    "zero word: misreads", "zero word: last value", "P33 timeouts", "battery (ADC)",
    "last crash: magic", "last crash: count", "last crash: VEC", "last crash: PC", "last crash: RETS",
    "last crash: EMU", "last crash: DBG", "last crash: SP", "last crash: PSR", "last crash: ICFG",
    "last crash: uptime ms", "last crash: ETM0", "last crash: ETM1", "last crash: ETM2", "last crash: ETM3",
    "last crash: early",
    "core 2 at it: magic", "core 2 at it: faults", "core 2 at it: DBG", "core 2 at it: EMU", "core 2 at it: PC",
    "core 2 at it: RETS", "core 2 at it: gave up", "core 2 at it: rejected", "core 2 at it: misread",
    "core 2 at it: zero word misreads", "core 2 at it: zero word last",
    "RAM code: words unlike the flash", "RAM code: first unlike", "RAM code: last unlike",
    "RAM code: the KB with one (bits)", "supplies at the boot", "supplies as set",
    "clock: measured (MHz x 10)", "clock: SYS_DIV", "clock: CLK_CON0", "clock: CLK_CON2", "clock: PLL_CON0",
    "clock: PLL_CON1", "clock: PLL2_CON0", "clock: PLL2_CON1",
    "CPU: worst since the start (0.1 %)", "CPU: gaps since the start (interrupts past their time)",
    "core 2 wait: worst of the last second (0.1 %)", "core 2 wait: worst since the start (0.1 %)",
    "last run: the watchdog restarted it (1)", "last run: where the main loop stood (1 loop 2 input 21-25 a save "
    "30 settings 50 drawing 51 drawn 99 end)", "last run: main loop's last pass (ms)", "last run: audio's last (ms)",
    "last run: page", "last run: buttons", "last run: CPU (0.1 %)", "last run: blocks held", "last run: core 2",
    "last run: its last STOP (ms)", "last run: PLAY before it (ms)",
    "OCT- at power-on: buttons", "OCT- at power-on: rows seen (OCT- col | OCT+ col << 8 | any key << 16)",
    "OCT- at power-on: frames", "OCT- at power-on: TIMER4 at it",
    "OCT- second look: buttons", "OCT- second look: rows seen (OCT- col | OCT+ col << 8 | any key << 16)",
    "OCT- second look: frames", "OCT- second look: TIMER4 at it"};
#define NNAME ((int)(sizeof NAME / sizeof NAME[0]))
/* the second page: the watch, then each core's record (hal/fm1_cpu1.h's fm1_dump_t, 57 words) */
static const char *const NAME2[] = {"page", "watch: CPU0's windows", "watch: core 2's windows", "watch: DMA's windows",
                                    "watch: DBG_EN", "core 2 now: PC", "core 2 now: last branch"};
static const char *const DUMP[] = {
    "magic", "TIMER4 at it", "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "r13",
    "r14", "r15", "PC (reti)", "rets", "psr", "usp", "ssp", "sp", "branch 0", "branch 1", "branch 2", "branch 3",
    "EMU", "limit SSP high", "limit SSP low", "limit USP high", "limit USP low", "DBG", "AXI read invalid: whose",
    "AXI write invalid: whose", "CPU0's window hit", "core 2's window hit", "a DMA's window hit", "the DMA's ID",
    "the other core's PC"};
#define NDUMP 57
static const char *const DBGB[22] = {"watchdog", "DMA read MMU", "DMA write in a watch", "DMA write MMU",
    "AXI read invalid", "AXI write invalid", "core 2 read MMU", "CPU0 read MMU", "core 2 write MMU", "CPU0 write MMU",
    "core 2 PC limit", "core 2 write in a watch", "CPU0 PC limit", "CPU0 write in a watch", "dmc", "dbg",
    "core 2 fetch bus invalid", "core 2 read bus invalid", "core 2 write bus invalid", "CPU0 fetch bus invalid",
    "CPU0 read bus invalid", "CPU0 write bus invalid"};
enum { W_UP = 2, W_CPU = 3, W_MODE = 4, W_C1 = 5, W_REJ = 10, W_MIS = 11, W_JOBS = 12, W_C1F = 13, W_STACK = 18,
       W_PBAD = 22, W_PLAST = 23, W_CRASH = 26, W_C1L = 42 };

static uint8_t rx[4096];
static size_t rxn;
static int in_sx;
static uint32_t w[128];
static volatile int nw, got, want = 2;

static void on_read(const MIDIPacketList *pl, void *ref, void *src)
{
    const MIDIPacket *pk = &pl->packet[0];
    UInt32 i;
    (void)ref; (void)src;
    for (i = 0; i < pl->numPackets; i++) {
        UInt16 j;
        for (j = 0; j < pk->length; j++) {
            uint8_t b = pk->data[j];
            if (b == 0xF0) { in_sx = 1; rxn = 0; }
            if (!in_sx) continue;
            if (rxn < sizeof rx) rx[rxn++] = b;
            if (b != 0xF7) continue;
            in_sx = 0;
            if (rxn >= 7 && rx[1] == 0x7D && rx[2] == 0x53 && rx[3] == 0x44 && rx[4] == want) {
                int k = rx[5], a, d;
                if (k > 128 || rxn < (size_t)(7 + 8 * k)) continue;
                for (a = 0; a < k; a++) {
                    uint32_t v = 0;
                    for (d = 0; d < 8; d++) v = v << 4 | (rx[6 + 8 * a + d] & 15u);
                    w[a] = v;
                }
                nw = k;
                __sync_synchronize();
                got = 1;
            }
        }
        pk = MIDIPacketNext(pk);
    }
}

static int named(MIDIEndpointRef e, const char *want)
{
    CFStringRef s = NULL;
    char b[256] = "";
    if (MIDIObjectGetStringProperty(e, kMIDIPropertyDisplayName, &s) != noErr || !s) return 0;
    CFStringGetCString(s, b, sizeof b, kCFStringEncodingUTF8);
    CFRelease(s);
    return strcasestr(b, want) != NULL;
}

static uint8_t ask[6] = {0xF0, 0x7D, 0x53, 0x44, 0x01, 0xF7};
static MIDISysexSendRequest req;
static volatile int sent;
static void on_sent(MIDISysexSendRequest *r) { (void)r; sent = 1; }

static int reading_p(MIDIEndpointRef dst, int ms, int page)   /* ask once; 1 when the answer came in ms */
{
    int t;
    ask[4] = (uint8_t)(2 * page - 1);           /* 01 03 */
    want = 2 * page;
    got = 0;
    sent = 0;
    memset(&req, 0, sizeof req);
    req.destination = dst;
    req.data = ask;
    req.bytesToSend = sizeof ask;
    req.completionProc = on_sent;
    if (MIDISendSysex(&req) != noErr) return 0;
    for (t = 0; t < ms && !got; t += 5) usleep(5000);
    __sync_synchronize();
    return got;
}
static int reading(MIDIEndpointRef dst, int ms) { return reading_p(dst, ms, 1); }
static uint8_t ask_log[7] = {0xF0, 0x7D, 0x53, 0x44, 0x05, 0x00, 0xF7};
static int reading_log(MIDIEndpointRef dst, int ms, int ix)   /* the freeze log's record ix / 2, its half ix & 1 */
{
    int t;
    ask_log[5] = (uint8_t)ix;
    want = 6;
    got = 0;
    sent = 0;
    memset(&req, 0, sizeof req);
    req.destination = dst;
    req.data = ask_log;
    req.bytesToSend = sizeof ask_log;
    req.completionProc = on_sent;
    if (MIDISendSysex(&req) != noErr) return 0;
    for (t = 0; t < ms && !got; t += 5) usleep(5000);
    __sync_synchronize();
    return got;
}
/* a record (stardistroyer.c's FrzRec, 67 words, sent in two halves): magic seq kind crc, ver (4), LastRun (magic ms isr_ms page btn cpu
   held c1 at stop_ms play_ms st ev_n pc_n pc[4]), crash (16), ev_n, ev (24: t8 | kind << 21 | code << 16 | val << 24) */
static const char *const PG[16] = {"NAV", "REACTOR", "DISTROY", "SPACE", "LFO", "MACROS", "SEQ", "ENV", "FILTER", "ARP",
                                   "ORBIT", "TIME", "LOOPER", "SETUP", "DRUM", "SUB"};
static const char *const BT[14] = {"OCT-", "OCT+", "FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                   "PLAY", "REC"};
static const char *const EN[7] = {"SELECT", "ALGORITHM", "K1", "K2", "K3", "K4", "PRESETS"};
static void log_print(FILE *f, int k, const uint32_t *r)
{
    static const char *const LAYER[5] = {"STEP", "MORE", "TRACK", "PARTS", "TOOLS"};
    char ver[17];
    const uint32_t *run = r + 8, *cr = run + 18;
    uint32_t st = run[11], i, n = cr[16] < 24u ? cr[16] : 24u;
    memcpy(ver, r + 4, 16); ver[16] = 0;
    fprintf(f, "#%d (%u): a %s, firmware %s, %u:%02u:%02u after the start\n", k + 1, r[1], r[2] == 2u ? "CRASH" : "FREEZE",
            ver, run[1] / 3600000u, run[1] / 60000u % 60u, run[1] / 1000u % 60u);
    fprintf(f, "  page %s%s%s, %s%s%s, voices %u, ARP %s, looper %s, CPU %.1f %%, the brake's blocks %u, core 2 %u\n",
            PG[run[3] & 15u], run[3] == 6u ? (st >> 6 & 1u ? " B " : " A ") : "", run[3] == 6u ? LAYER[(st >> 8 & 7u) % 5u] : "",
            st & 1u ? "PLAY" : "STOP", st & 2u ? " REC" : "", st & 4u ? " GLO latched" : "", (st >> 4 & 3u) + 1u,
            st >> 13 & 1u ? "on" : "off", st >> 14 & 1u ? "armed" : "off", run[5] / 10.0, run[6], run[7] & 1u);
    fprintf(f, "  the main loop stood at %u (2 input, 20-25 a save, 30 settings, 50 drawing, 51 drawn, 99 end), its last pass "
            "%u ms, the audio's last second %u ms; its last STOP %u ms after %u ms of PLAY\n",
            run[8], run[1], run[2], run[9], run[10]);
    fprintf(f, "  stuck at (the PCs the audio interrupted, a quarter second apart from 1 s):");
    for (i = 0; i < run[13] && i < 4u; i++) fprintf(f, " %08X", run[14 + i]);
    fprintf(f, run[13] ? "\n" : " none (the audio stood too, or it was not stuck)\n");
    if (r[2] == 2u) fprintf(f, "  the crash: VEC %u PC %08X RETS %08X EMU %08X DBG %08X SP %08X, %u ms up\n",
                            cr[2], cr[3], cr[4], cr[5], cr[6], cr[7], cr[10]);
    fprintf(f, "  the last %u inputs of %u, the oldest first (seconds before):\n   ", n, cr[16]);
    for (i = 24u - n; i < 24u; i++) {
        uint32_t e = cr[17 + i], t8 = e & 0xFFFFu, kind = e >> 21 & 7u, code = e >> 16 & 31u;
        int val = (int8_t)(e >> 24);
        double ago = (double)(((run[1] >> 3) - t8) & 0xFFFFu) * 0.008;
        if (kind == 1u || kind == 2u) fprintf(f, " %s%s", code < 14u ? BT[code] : "BTN", kind == 2u ? "^" : "");
        else if (kind == 3u || kind == 4u) fprintf(f, " key%u%s", code, kind == 4u ? "^" : "");
        else if (kind == 5u) fprintf(f, " %s%+d", code < 7u ? EN[code] : "ENC", val);
        else continue;
        fprintf(f, "(%.1f)", ago);
    }
    fprintf(f, "\n");
}

static int find(MIDIEndpointRef *src, MIDIEndpointRef *dst)   /* the FM-1's ports: 1 when both are there */
{
    ItemCount i;
    *src = *dst = 0;
    for (i = 0; i < MIDIGetNumberOfSources(); i++) if (named(MIDIGetSource(i), "Felucca")) *src = MIDIGetSource(i);
    for (i = 0; i < MIDIGetNumberOfDestinations(); i++) if (named(MIDIGetDestination(i), "Felucca")) *dst = MIDIGetDestination(i);
    return *src && *dst;
}

static void full(FILE *f)
{
    int i;
    for (i = 0; i < nw && i < NNAME; i++) {
        fprintf(f, "  %-38s %08X  (%u)", NAME[i], w[i], w[i]);
        if (i >= 57 && i <= 58 && w[i])         /* (P3_ANA_CON6 | CON9 << 8: VDC14 bits 0..2, SYSVDD bits 0..3) */
            fprintf(f, "  VDC14 %u (%.2f V), SYSVDD %u (%.2f V)", w[i] & 7u, 1.25 + 0.05 * (w[i] & 7u),
                    w[i] >> 8 & 15u, 0.93 + 0.03 * (w[i] >> 8 & 15u));
        if (i == 59 && w[i]) fprintf(f, "  = %.1f MHz", w[i] / 10.0);
        fputc('\n', f);
    }
}
static void bits(FILE *f, const char *what, uint32_t v)   /* a cause register's bits by name */
{
    int b;
    if (!v) return;
    fprintf(f, "      %s:", what);
    for (b = 0; b < 32; b++) {
        if (!(v >> b & 1u)) continue;
        if (!strcmp(what, "DBG")) fprintf(f, " [%s]", b < 22 ? DBGB[b] : "?");
        else fprintf(f, " [%s]", b == 0 ? "misaligned" : b == 1 ? "illegal" : b == 2 ? "div 0" : b == 3 ? "stack limit"
                                 : b == 8 ? "watchpoint" : b >= 16 && b <= 20 ? "fpu" : "?");
    }
    fputc('\n', f);
}
static void full2(FILE *f)                      /* the second page (call after reading_p(.., 2)) */
{
    int i, c;
    for (i = 0; i < 7 && i < nw; i++) fprintf(f, "  %-38s %08X  (%u)\n", NAME2[i], w[i], w[i]);
    for (c = 0; c < 2; c++) {
        const uint32_t *d = &w[7 + NDUMP * c];
        if (7 + NDUMP * (c + 1) > nw) break;
        fprintf(f, "  --- %s's record at its last fault%s\n", c ? "core 2" : "CPU0",
                d[0] == 0x504D5544u ? "" : " (none: no magic)");
        if (d[0] != 0x504D5544u) continue;
        for (i = 1; i < 41; i++) {
            fprintf(f, "  %-38s %08X  (%u)", DUMP[i], d[i], d[i]);
            if (i == 1) fprintf(f, "  = %.3f s", d[i] / 24e6);
            fputc('\n', f);
            if (i == 28) bits(f, "EMU", d[i]);
            if (i == 33) bits(f, "DBG", d[i]);
        }
        for (i = 0; i < 16; i++) fprintf(f, "  stack sp+%-3d                          %08X  (%u)\n", 184 + 4 * i,
                                         d[41 + i], d[41 + i]);
    }
}

int main(int argc, char **argv)
{
    MIDIClientRef cl;
    MIDIPortRef in;
    MIDIEndpointRef src = 0, dst = 0;
    int watch = 0, every = 250, a, miss = 0;
    const char *logname = NULL;
    FILE *lg = NULL;
    uint32_t last_up = 0;
    for (a = 1; a < argc; a++) {
        if (!strcmp(argv[a], "-w")) { watch = 1; if (a + 1 < argc && argv[a + 1][0] != '-') every = atoi(argv[++a]); }
        else if (!strcmp(argv[a], "-o") && a + 1 < argc) logname = argv[++a];
        else if (!strcmp(argv[a], "-l")) watch = 2;
        else { fprintf(stderr, "usage: fm1_diag [-l] [-w [ms]] [-o log.txt]\n"); return 2; }
    }
    if (every < 50) every = 50;
    if (MIDIClientCreate(CFSTR("fm1_diag"), NULL, NULL, &cl) != noErr) { fprintf(stderr, "fm1_diag: no CoreMIDI\n"); return 1; }
    if (!find(&src, &dst)) {
        fprintf(stderr, "fm1_diag: no \"Felucca\" among the MIDI devices (is the FM-1 on, by USB, on this Mac?)\n");
        return 1;
    }
    MIDIInputPortCreate(cl, CFSTR("in"), on_read, NULL, &in);
    MIDIPortConnectSource(in, src, NULL);
    if (logname && !(lg = fopen(logname, "a"))) { fprintf(stderr, "fm1_diag: cannot write %s\n", logname); return 1; }
    if (watch == 2) {                           /* the freeze log, the newest first */
        uint32_t r[128];
        int k, h, any = 0, off;
        for (k = 0; k < 16; k++) {
            for (h = off = 0; h < 2; h++) {
                if (!reading_log(dst, 1500, 2 * k + h)) { fprintf(stderr, "fm1_diag: no answer (an older firmware?)\n"); return 1; }
                if (nw < 2 || off + nw - 1 > 128) break;
                memcpy(r + off, w + 1, (size_t)(nw - 1) * 4u);
                off += nw - 1;
            }
            if (h < 2) break;
            log_print(stdout, k, r);
            if (lg) log_print(lg, k, r);
            any = 1;
        }
        if (!any) printf("The freeze log is empty.\n");
        if (lg) fclose(lg);
        return 0;
    }
    if (!watch) {
        if (!reading(dst, 1500)) { fprintf(stderr, "fm1_diag: no answer (an older firmware, or USB MIDI not through)\n"); return 1; }
        printf("Star Distroyer, the FM-1 now:\n");
        full(stdout);
        if (lg) full(lg);
        if (reading_p(dst, 1500, 2)) {
            printf("The second page (the watch, the records):\n");
            full2(stdout);
            if (lg) full2(lg);
        }
        if (lg) fclose(lg);
        return 0;
    }
    printf("time      up s   CPU   mode play  core2 on/gave/cut  jobs      rejected  zero-word misreads  core2 faults  RAM code unlike (KB bits)"
           "  worst  gaps  core2 wait\n");
    for (;;) {
        char line[256], ts[16];
        time_t now = time(NULL);
        strftime(ts, sizeof ts, "%H:%M:%S", localtime(&now));
        if (!reading(dst, every * 3 < 1000 ? 1000 : every * 3)) {
            snprintf(line, sizeof line, "%s  no answer (restarting, or USB gone)\n", ts);
            if (++miss >= 4) {                  /* (a restart takes USB away and back: look for it again) */
                MIDIEndpointRef s2, d2;
                miss = 0;
                if (find(&s2, &d2)) {
                    MIDIPortDisconnectSource(in, src);
                    src = s2;
                    dst = d2;
                    MIDIPortConnectSource(in, src, NULL);
                }
            }
        } else {
            miss = 0;
            uint32_t up = w[W_UP], c1 = w[W_C1];
            if (last_up && up < last_up) {
                printf("%s  RESTARTED. The crash it kept:\n", ts);
                full(stdout);
                if (lg) { fprintf(lg, "%s  RESTARTED. The crash it kept:\n", ts); full(lg); }
                if (reading_p(dst, 1500, 2)) {
                    printf("The second page (the watch, the records):\n");
                    full2(stdout);
                    if (lg) { fprintf(lg, "The second page (the watch, the records):\n"); full2(lg); }
                }
                last_up = up;
                if (!reading(dst, 1500)) continue;   /* (the first page again for the line below) */
                up = w[W_UP], c1 = w[W_C1];
            }
            last_up = up;
            snprintf(line, sizeof line, "%s  %6.1f  %4.1f%%  %u    %u     %u/%u/%u            %-9u %-9u %-8u (last %08X)  %-12u  %u (%08X)"
                     "  %4.1f%%  %-4u  %4.1f%%\n", ts,
                     up / 1000.0, w[W_CPU] == 0xFFFFFFFFu ? 0.0 : w[W_CPU] / 10.0, w[W_MODE] & 255u, (w[W_MODE] >> 8) & 255u,
                     c1 & 255u, (c1 >> 8) & 255u, (c1 >> 16) & 255u, w[W_JOBS], w[W_REJ], w[W_PBAD], w[W_PLAST], w[W_C1F],
                     nw > 56 ? w[53] : 0u, nw > 56 ? w[56] : 0u,   /* (2026-11-23: the worst since the start, the gaps,
                                                   the wait for core 2 in the last second: the dropouts' marks) */
                     nw > 69 ? w[67] / 10.0 : 0.0, nw > 69 ? w[68] : 0u, nw > 69 ? w[69] / 10.0 : 0.0);
        }
        fputs(line, stdout);
        fflush(stdout);
        if (lg) { fputs(line, lg); fflush(lg); }
        usleep((useconds_t)every * 1000u);
    }
}
