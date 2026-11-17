/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 joakimskurk */
/* Star Distroyer's diagnosis over USB, on a Mac: reads what the FM-1 is doing while it plays, and what it kept from its
 * last crash. Read only: it sends the FM-1 one question, F0 7D 53 44 01 F7 (the app's sd_diag_service answers), and
 * nothing else, never the installer's messages; the FM-1 writes nothing for it, and no flash.
 *   cc -O2 -o build/fm1_diag tools/fm1_diag.c -framework CoreMIDI -framework CoreFoundation
 *   build/fm1_diag                       one reading, every field named
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
    "RAM code: the KB with one (bits)"};
#define NNAME ((int)(sizeof NAME / sizeof NAME[0]))
enum { W_UP = 2, W_CPU = 3, W_MODE = 4, W_C1 = 5, W_REJ = 10, W_MIS = 11, W_JOBS = 12, W_C1F = 13, W_STACK = 18,
       W_PBAD = 22, W_PLAST = 23, W_CRASH = 26, W_C1L = 42 };

static uint8_t rx[4096];
static size_t rxn;
static int in_sx;
static uint32_t w[128];
static volatile int nw, got;

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
            if (rxn >= 7 && rx[1] == 0x7D && rx[2] == 0x53 && rx[3] == 0x44 && rx[4] == 0x02) {
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

static int reading(MIDIEndpointRef dst, int ms)   /* ask once; 1 when the answer came in ms */
{
    int t;
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
    for (i = 0; i < nw && i < NNAME; i++) fprintf(f, "  %-38s %08X  (%u)\n", NAME[i], w[i], w[i]);
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
        else { fprintf(stderr, "usage: fm1_diag [-w [ms]] [-o log.txt]\n"); return 2; }
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
    if (!watch) {
        if (!reading(dst, 1500)) { fprintf(stderr, "fm1_diag: no answer (an older firmware, or USB MIDI not through)\n"); return 1; }
        printf("Star Distroyer, the FM-1 now:\n");
        full(stdout);
        if (lg) { full(lg); fclose(lg); }
        return 0;
    }
    printf("time      up s   CPU   mode play  core2 on/gave/cut  jobs      rejected  zero-word misreads  core2 faults  RAM code unlike (KB bits)\n");
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
            }
            last_up = up;
            snprintf(line, sizeof line, "%s  %6.1f  %4.1f%%  %u    %u     %u/%u/%u            %-9u %-9u %-8u (last %08X)  %-12u  %u (%08X)\n", ts,
                     up / 1000.0, w[W_CPU] == 0xFFFFFFFFu ? 0.0 : w[W_CPU] / 10.0, w[W_MODE] & 255u, (w[W_MODE] >> 8) & 255u,
                     c1 & 255u, (c1 >> 8) & 255u, (c1 >> 16) & 255u, w[W_JOBS], w[W_REJ], w[W_PBAD], w[W_PLAST], w[W_C1F],
                     nw > 56 ? w[53] : 0u, nw > 56 ? w[56] : 0u);
        }
        fputs(line, stdout);
        fflush(stdout);
        if (lg) { fputs(line, lg); fflush(lg); }
        usleep((useconds_t)every * 1000u);
    }
}
