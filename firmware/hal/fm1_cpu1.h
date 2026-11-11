/* SPDX-License-Identifier: MPL-2.0
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio): from Melodee (github.com/keremimo/melodee): the start that works
 *   on the device, core 1's guards, the worker with its check word and its retirement, the bar, the fault path
 *   (fm1_multicore.h, audio_worker.h, fm1_core1_bar.h, fm1_irq.h), under MPL-2.0 by agreement
 *   (https://github.com/hugelton/felucca-bsp/issues/5)
 * Copyright (C) 2026 Charles Vestal (the PC limits around the start, the stack mark, the bar per build, the API)
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments (the first start, from the SDK)
 * Written against the public JieLi AC79 SDK (Apache-2.0).
 * Taken from felucca-bsp (hal/fm1_cpu1.h) for Star Distroyer, on Dinghy 0.5: the fallback always on (felucca-bsp's
 * FM1_CPU1_FALLBACK 1; its restart path left out), a fault beside a running CPU1 handled as below (Dinghy has no
 * per-core map of the debug unit's flags), the gate's hold (fm1_cpu1_stop.h) seen as a failure, a start that never
 * finished (a crash or a reset in it) barring the next, and Dinghy's timer and core number. */
/* FM-1 second core (CPU1): a worker that runs one job at a time for CPU0, a part of the audio render.
 *
 * Start (the SDK's EnableOtherCpu, as Melodee runs it): CPU1 held (fm1_cpu1_stop), its interrupt bank off
 * (0x1EEF300..), its entry address into the word at 0x01C7FFF8 (the protected top of RAM: unlocked for it), bit 3
 * of SYS_DIV (0x10008) set while it starts, C1_CON (0x1EEE004) bit 3 set and bit 1 cleared. It boots through the
 * chip's ROM, which the PC limits refuse (DBG bit 10, c1_pc_limit_err), so they are open until it has reported in.
 * Start it at power-on, before the audio and the timers run (Melodee's and X0X's finding: that is the start that
 * works), with interrupts off.
 *
 * Work: CPU0 hands a job over (fm1_cpu1_run: a function and an argument) and later waits for it (fm1_cpu1_wait).
 * One job at a time, a single writer for each word, csync around each (the cores share the caches): CPU0's words,
 * then CPU1's on a cache line of their own. CPU1's loop is in RAM (.c1_text, copied at boot), so an idle CPU1 never
 * fetches from the flash; the job itself runs from the flash, so CPU0 turns the flash off only with CPU1 idle (the
 * gate, fm1_cpu1_stop.h).
 *
 * Units that cannot run it (keremimo/melodee#17, charlesvestal/fm1-x0x#10; measured by Jangada,
 * github.com/zednaked/jangada): on some FM-1s CPU1 now and then reads shared RAM wrong (0x00200000 where it holds
 * 0), more often under load. So CPU1 runs only the next job, and only when its check word (the function, the
 * argument and the job's number, written before the job) matches; the jobs it finished stay in a register. A
 * misread, a fault on CPU1, a job not back in FM1_CPU1_TIMEOUT_US, a start it does not answer, or a hold at the
 * flash gate is a failure: CPU1 is retired (held, and CPU0 does the work alone: a job it had is dropped, never run
 * twice) and barred: one core at every start until power-off (fm1_cpu1_bar, .noinit; nothing written to the flash
 * on a unit that may be unstable). fm1_cpu1_bar_build gives a new build two cores again.
 *
 * Faults (fm1_cpu1_fault_c, first in fm1_fault_c): one taken on CPU1 itself (a misread pointer it followed, its
 * stack limit) is recorded and CPU1 parks (the crash screen and the reboot run from CPU1 would freeze the FM-1);
 * CPU0 sees it at its next look and retires it. One taken on CPU0 while CPU1 runs: CPU1 is held and barred, and
 * CPU0 reports and restarts as ever, so the FM-1 comes back on one core. (felucca-bsp resumes CPU0 when the debug
 * unit's flags are CPU1's alone; that needs its per-core map of DBG_MSG, which Dinghy does not have.)
 *
 *   fm1_cpu1_start()        0 = it runs and answered; -1 = held (barred, or it did not report in)
 *   fm1_cpu1_run(fn, arg)   1 = it has fn(arg); 0 = do it yourself (off, or a job outstanding)
 *   fm1_cpu1_wait()         0 = done; -1 = retired, the job dropped (do it yourself)
 *   fm1_cpu1_check()        retire it if it misread, faulted or was held at the gate while idle (run does it too)
 *   fm1_cpu1_stop()         hold it (fm1_cpu1_stop.h: registers only)
 *   fm1_cpu1_on, fm1_cpu1_gave_up (FM1_CPU1_*), fm1_cpu1_stack_used(), fm1_cpu1_barred(), fm1_cpu1_bar_build()
 *
 * The build defines FM1_CPU1=1 before the first hal header (fm1_irq.h then hands faults here first), assembles
 * fm1_cpu1.S (the entry and the stacks), and links .c1_text into the RAM code (.ram_text, between _c1_start and
 * _c1_end: build.py lets that range call, the job, and nothing else). */
#pragma once
#include <stdint.h>
#include "fm1_cc.h"
#ifndef FM1_CPU1
#define FM1_CPU1 1
#endif
#include "fm1_time.h"
#include "fm1_irq.h"
#include "fm1_guard.h"
#include "fm1_cpu1_stop.h"

#ifndef FM1_CPU1_TIMEOUT_US
#define FM1_CPU1_TIMEOUT_US 8000u               /* a job not back by then: far past any audio block, or it faulted */
#endif
#define FM1_CPU1_SYNC() __asm__ volatile("csync" ::: "memory")
#define FM1_C1_CON (*(volatile uint32_t *)0x1EEE004u)
#define FM1_C1_SYS_DIV (*(volatile uint32_t *)0x10008u)
#define FM1_C1_ENTRY_WORD (*(volatile uint32_t *)0x01C7FFF8u)
#define FM1_C1_ICFG(i) (*(volatile uint32_t *)(0x1EEF300u + 4u * (i)))   /* CPU1's interrupt bank (CPU0's: 0x1EEF100) */
#define FM1_C1_EMU_CON (*(volatile uint32_t *)0x1EEF2D0u)                 /* CPU1's EMU bank (CPU0's: 0x1EEF0D0) */
#define FM1_C1_EMU_MSG (*(volatile uint32_t *)0x1EEF2D4u)

enum { FM1_CPU1_MISREAD = 1, FM1_CPU1_LATE = 2, FM1_CPU1_BARRED = 3, FM1_CPU1_FAULT = 4, FM1_CPU1_SILENT = 5,
       FM1_CPU1_CUT = 6 };

static inline uint32_t fm1_cnum(void) { uint32_t v; __asm__ volatile("%0 = cnum" : "=r"(v)); return v; }

extern void fm1_cpu1_entry(void);                       /* fm1_cpu1.S */
extern uint32_t fm1_cpu1_ustack[], fm1_cpu1_sstack_top[];
#ifndef FM1_CPU1_USTACK_BYTES
#define FM1_CPU1_USTACK_BYTES 2816                      /* the jobs' stack, bytes (fm1_cpu1.S reads the same macro):
                                                         * a multiple of 8, the lowest 256 B a guard */
#endif
#define FM1_CPU1_USTACK_WORDS (FM1_CPU1_USTACK_BYTES / 4u)
_Static_assert(FM1_CPU1_USTACK_BYTES % 8 == 0 && FM1_CPU1_USTACK_BYTES > 256,
               "FM1_CPU1_USTACK_BYTES: a multiple of 8, above the 256 B guard");
#define FM1_CPU1_MARK 0x43314D4Bu                       /* "C1MK": how deep its stack has gone */

typedef void (*fm1_cpu1_fn)(uint32_t);
/* CPU0's words, then CPU1's on a cache line of their own */
static struct {
    volatile uint32_t request;                  /* CPU0: the job's number (request + 1 hands fn(arg) over) */
    fm1_cpu1_fn volatile fn;
    volatile uint32_t arg, check;
    volatile uint32_t ready __attribute__((aligned(32)));   /* CPU1: it started */
    volatile uint32_t complete, rejected;       /* the last job it finished; the jobs it would not run */
    volatile uint32_t misread, jobs;            /* the last refused, as it read it; how many it ran */
} fm1_cpu1_mb __attribute__((aligned(32)));
#define FM1_CPU1_CHECK(fn, arg, request) ((uint32_t)(uintptr_t)(fn) ^ (arg) ^ (request) * 0x9E3779B1u)

static uint8_t fm1_cpu1_on;                     /* started and answering: CPU0 may hand it work */
static uint8_t fm1_cpu1_gave_up;                /* why it is off (FM1_CPU1_*), 0 = it is not */

/* its faults (fm1_cpu1_fault_c): CPU0 reads count */
static struct { volatile uint32_t count; uint32_t dbg, emu, pc, rets; } fm1_cpu1_fault;

/* one core until power-off. Not static, as fm1_crash: the compiler must not assume a zero start */
#define FM1_CPU1_BAR_MAGIC 0x52414231u          /* "1BAR" */
struct { uint32_t magic, barred, build; } fm1_cpu1_bar __attribute__((section(".noinit")));
static inline int fm1_cpu1_barred(void)         /* (barred 2: a start that never finished, a reset or a crash in it) */
{
    return fm1_cpu1_bar.magic == FM1_CPU1_BAR_MAGIC && (fm1_cpu1_bar.barred == 1u || fm1_cpu1_bar.barred == 2u);
}
static inline void fm1_cpu1_bar_set(void)
{
    fm1_cpu1_bar.magic = FM1_CPU1_BAR_MAGIC;
    fm1_cpu1_bar.barred = 1;
}
/* at boot: a bar set by another build (another id) is lifted, as it was for that build's sake */
static void fm1_cpu1_bar_build(uint32_t build)
{
    if (fm1_cpu1_bar.magic != FM1_CPU1_BAR_MAGIC || fm1_cpu1_bar.build != build)
        fm1_cpu1_bar.barred = 0;
    fm1_cpu1_bar.magic = FM1_CPU1_BAR_MAGIC;
    fm1_cpu1_bar.build = build;
}

/* CPU1's loop (in RAM): its guards, then one look at the mailbox after another. Only request, fn, arg and check
 * come from the RAM; a misread of any of them fails the check */
void fm1_cpu1_main(void);
void __attribute__((section(".c1_text"), noreturn, used)) fm1_cpu1_main(void)
{
    /* the stack limit at CPU1's EMU bank, over both its stacks less the lowest 256 bytes (a stack that runs
     * down traps before it reaches what lies below), and EMU_CON bit 2 and bits 16..20 off, bit 3 on */
    uint32_t lo = (uint32_t)(uintptr_t)fm1_cpu1_ustack + 256u, hi = (uint32_t)(uintptr_t)fm1_cpu1_sstack_top - 1u;
    uint32_t done;
    *(volatile uint32_t *)0x1EEF2D8u = hi;
    *(volatile uint32_t *)0x1EEF2DCu = lo;
    *(volatile uint32_t *)0x1EEF2E0u = hi;
    *(volatile uint32_t *)0x1EEF2E4u = lo;
    FM1_C1_EMU_CON = (FM1_C1_EMU_CON & ~((1u << 2) | (0x1Fu << 16))) | (1u << 3);
    done = fm1_cpu1_mb.complete;
    FM1_CPU1_SYNC();
    fm1_flash_cpu1_idle = 1;
    FM1_CPU1_SYNC();
    fm1_cpu1_mb.ready = 1;
    FM1_CPU1_SYNC();
    for (;;) {
        uint32_t request = fm1_cpu1_mb.request;
        FM1_CPU1_SYNC();
        if (request == done)
            continue;
        if (request == done + 1u) {
            fm1_cpu1_fn fn = fm1_cpu1_mb.fn;
            uint32_t arg = fm1_cpu1_mb.arg;
            if (fm1_cpu1_mb.check == FM1_CPU1_CHECK(fn, arg, request)) {
                fn(arg);                        /* the work itself runs from the flash */
                fm1_cpu1_mb.jobs++;
                done = request;
                FM1_CPU1_SYNC();
                fm1_flash_cpu1_idle = 1;
                FM1_CPU1_SYNC();
                fm1_cpu1_mb.complete = request;
                FM1_CPU1_SYNC();
                continue;
            }
        }
        fm1_cpu1_mb.misread = request;
        FM1_CPU1_SYNC();
        fm1_cpu1_mb.rejected++;
        FM1_CPU1_SYNC();
    }
}

/* CPU0: a failure: held for good (until power-off), and CPU0 works alone (a job it may have been running is
 * dropped) */
static void fm1_cpu1_retire(uint32_t why)
{
    fm1_cpu1_stop();
    fm1_cpu1_bar_set();
    fm1_cpu1_on = 0;
    fm1_cpu1_gave_up = (uint8_t)why;
    fm1_cpu1_mb.complete = fm1_cpu1_mb.request;
    FM1_CPU1_SYNC();
}

static int fm1_cpu1_start(void)
{
    uint32_t div, t0, i, pc[4], top;
    if (fm1_cpu1_on)
        return 0;
    fm1_cpu1_stop();
    if (fm1_cpu1_barred()) {
        fm1_cpu1_gave_up = FM1_CPU1_BARRED;
        return -1;
    }
    for (i = 0; i < FM1_CPU1_USTACK_WORDS; i++)
        fm1_cpu1_ustack[i] = FM1_CPU1_MARK;
    fm1_flash_cpu1_idle = 0;
    fm1_flash_cpu1_cut = 0;
    fm1_cpu1_mb.ready = 0;
    fm1_cpu1_mb.request = 0;
    fm1_cpu1_mb.complete = 0;
    fm1_cpu1_mb.rejected = 0;
    fm1_cpu1_mb.fn = 0;
    fm1_cpu1_fault.count = 0;
    FM1_CPU1_SYNC();
    for (i = 0; i < 32u; i++)                   /* its interrupt bank: all off (it runs polled) */
        FM1_C1_ICFG(i) = 0;
    fm1__dbg_unlock();                          /* the PC limits open while it boots through the ROM, and the */
    pc[0] = FM1_PC_LIMIT0_L, pc[1] = FM1_PC_LIMIT0_H;     /* protected top of RAM for its entry word */
    pc[2] = FM1_PC_LIMIT1_L, pc[3] = FM1_PC_LIMIT1_H;
    FM1_PC_LIMIT0_L = 0u;
    FM1_PC_LIMIT0_H = 0xFFFFFFFFu;
    FM1_PC_LIMIT1_L = 0u;
    FM1_PC_LIMIT1_H = 0xFFFFFFFFu;
    top = FM1_C0_WR_LIMIT_EN & 4u;
    FM1_C0_WR_LIMIT_EN &= ~4u;
    fm1__dbg_lock();
    fm1_cpu1_bar.magic = FM1_CPU1_BAR_MAGIC;    /* (a crash or a reset before it is over: one core at the next start) */
    fm1_cpu1_bar.barred = 2;
    FM1_CPU1_SYNC();
    FM1_C1_ENTRY_WORD = (uint32_t)(uintptr_t)fm1_cpu1_entry;
    div = FM1_C1_SYS_DIV;
    FM1_C1_SYS_DIV = div | 8u;
    FM1_CPU1_SYNC();
    FM1_C1_CON |= 8u;
    FM1_C1_CON &= ~2u;
    t0 = fm1_ticks();
    while (!fm1_cpu1_mb.ready && (uint32_t)(fm1_ticks() - t0) < 20000u * FM1_TICKS_PER_US)   /* 20 ms */
        FM1_CPU1_SYNC();
    if (!fm1_cpu1_mb.ready)
        fm1_cpu1_stop();
    FM1_C1_SYS_DIV = div;
    fm1__dbg_unlock();
    FM1_DBG_MSG_CLR = 0xFFFFFFFFu;
    FM1_PC_LIMIT0_L = pc[0], FM1_PC_LIMIT0_H = pc[1];
    FM1_PC_LIMIT1_L = pc[2], FM1_PC_LIMIT1_H = pc[3];
    FM1_C0_WR_LIMIT_EN |= top;
    fm1__dbg_lock();
    fm1_cpu1_bar.barred = 0;
    if (!fm1_cpu1_mb.ready) {
        fm1_cpu1_gave_up = FM1_CPU1_SILENT;
        return -1;
    }
    fm1_cpu1_on = 1;
    fm1_cpu1_gave_up = 0;
    return 0;
}

static void fm1_cpu1_check(void)
{
    FM1_CPU1_SYNC();
    if (!fm1_cpu1_on)
        return;
    if (fm1_cpu1_fault.count)
        fm1_cpu1_retire(FM1_CPU1_FAULT);
    else if (fm1_cpu1_mb.rejected)
        fm1_cpu1_retire(FM1_CPU1_MISREAD);
    else if (fm1_flash_cpu1_cut)
        fm1_cpu1_retire(FM1_CPU1_CUT);
}

static inline int fm1_cpu1_run(fm1_cpu1_fn fn, uint32_t arg)
{
    uint32_t request;
    fm1_cpu1_check();
    if (!fm1_cpu1_on || (request = fm1_cpu1_mb.request) != fm1_cpu1_mb.complete)
        return 0;
    request++;
    fm1_flash_cpu1_idle = 0;
    FM1_CPU1_SYNC();
    fm1_cpu1_mb.fn = fn;
    fm1_cpu1_mb.arg = arg;
    fm1_cpu1_mb.check = FM1_CPU1_CHECK(fn, arg, request);
    FM1_CPU1_SYNC();
    fm1_cpu1_mb.request = request;
    FM1_CPU1_SYNC();
    return 1;
}

static int fm1_cpu1_wait(void)
{
    uint32_t t0 = fm1_ticks();
    for (;;) {
        FM1_CPU1_SYNC();
        if (!fm1_cpu1_on)
            return -1;
        if (fm1_cpu1_fault.count) {
            fm1_cpu1_retire(FM1_CPU1_FAULT);
            return -1;
        }
        if (fm1_cpu1_mb.rejected) {
            fm1_cpu1_retire(FM1_CPU1_MISREAD);
            return -1;
        }
        if (fm1_flash_cpu1_cut) {
            fm1_cpu1_retire(FM1_CPU1_CUT);
            return -1;
        }
        if (fm1_cpu1_mb.complete == fm1_cpu1_mb.request)
            return 0;
        if ((uint32_t)(fm1_ticks() - t0) >= FM1_CPU1_TIMEOUT_US * FM1_TICKS_PER_US) {
            fm1_cpu1_retire(FM1_CPU1_LATE);
            return -1;
        }
    }
}

/* how deep its stack has gone, in bytes (the mark left untouched since the start) */
static uint32_t fm1_cpu1_stack_used(void)
{
    uint32_t i = 0;
    while (i < FM1_CPU1_USTACK_WORDS && fm1_cpu1_ustack[i] == FM1_CPU1_MARK)
        i++;
    return (FM1_CPU1_USTACK_WORDS - i) * 4u;
}

/* fm1_fault_c (fm1_irq.h), first (see the top): on CPU1 it records the fault and parks; on CPU0 it holds and bars a
 * running CPU1 and returns, and CPU0 reports its crash and restarts */
static void fm1_cpu1_fault_c(const uint32_t *f)
{
    if (fm1_cnum()) {
        fm1_cpu1_fault.dbg = FM1_DBG_MSG;
        fm1_cpu1_fault.emu = FM1_C1_EMU_MSG;
        fm1_cpu1_fault.pc = f[16];
        fm1_cpu1_fault.rets = f[25];
        FM1_CPU1_SYNC();
        fm1_cpu1_fault.count++;
        FM1_CPU1_SYNC();                        /* CPU0 sees it and holds us */
        for (;;)
            ;
    }
    if (fm1_cpu1_on) {
        fm1_cpu1_stop();
        fm1_cpu1_bar_set();
        fm1_cpu1_on = 0;
    }
}
