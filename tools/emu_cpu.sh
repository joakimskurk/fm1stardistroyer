#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 joakimskurk
# The chip's load for a package, on fm1-emu (Sound Plan v0.1 §6, the "fit it" gate): the audio interrupt's share of the chip while it
# plays, in instructions (the emulator counts instructions, not cycles: a divide counts as one). Two runs: 1.6 s of
# the splash and silence, then 5.6 s with PLAY pressed at 1.6 s; the difference is 4 s of playing. The main loop
# spins in the emulator (its idle instruction does not sleep there), so its count says nothing about the chip.
#   tools/emu_cpu.sh build/stardistroyer.fwsc [label]
#   FM1_EMU   the fm1-emu directory (default: ~/fm1-emu-0.3.0/fm1-emu-0.3.0-linux-x86_64); a Linux binary, so
#             on a Mac it runs in Docker (ubuntu:24.04).
#   EMU_LOG   a file to keep the emulator's reports in (the playing run's, then the silent run's)
#   EMU_KEYS  more input before PLAY, in fm1-emu's --keys form (e.g. a mode, a page, held keys): a scenario.
#             Both runs get it, so what it costs while silent is taken off too
set -e
PKG="$1"
[ -f "$PKG" ] || { echo "usage: tools/emu_cpu.sh PACKAGE.fwsc [label]"; exit 2; }
LABEL="${2:-$(basename "$PKG" .fwsc)}"
EMU="${FM1_EMU:-$HOME/fm1-emu-0.3.0/fm1-emu-0.3.0-linux-x86_64}"
SILENT_MS=1600
PLAY_MS=4000
WORK="$(mktemp -d "$HOME/.fm1-emu-run.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
cp "$PKG" "$WORK/pkg.fwsc"
emu() {    # ms, keys, log
    if [ "$(uname -s)" = Linux ]; then
        FM1_HIST_TOP=400 "$EMU/fm1-emu" --fwsc "$WORK/pkg.fwsc" --ms "$1" --keys "$2" >"$3" 2>&1 || true
    else
        docker run --rm -e FM1_HIST_TOP=400 -v "$EMU":/e -v "$WORK":/w -w /w ubuntu:24.04 \
            /e/fm1-emu --fwsc /w/pkg.fwsc --ms "$1" --keys "$2" >"$3" 2>&1 || true
    fi
    grep -q "in an ISR" "$3" || { cat "$3"; echo "emu_cpu: no report"; exit 1; }
}
emu $((SILENT_MS + PLAY_MS)) "${EMU_KEYS:+$EMU_KEYS,}$SILENT_MS:PLAY" "$WORK/play.txt"
emu $SILENT_MS "${EMU_KEYS:-}" "$WORK/silent.txt"
[ -n "$EMU_LOG" ] && cat "$WORK/play.txt" "$WORK/silent.txt" >"$EMU_LOG"
isr_total() {    # M ISR instructions in the whole run
    awk -v ms="$2" '/in an ISR/ { for (i = 1; i <= NF; i++) if ($i == "M" && $(i+1) == "ISR") { m = $(i-1); sub(/\(/, "", m); printf "%.3f", m * ms / 1000.0 } }' "$1"
}
PLAY_TOTAL=$(isr_total "$WORK/play.txt" $((SILENT_MS + PLAY_MS)))
SILENT_TOTAL=$(isr_total "$WORK/silent.txt" $SILENT_MS)
HALVES=$(awk '/^time / { print $(NF-4) }' "$WORK/play.txt")
awk -v label="$LABEL" -v play="$PLAY_TOTAL" -v silent="$SILENT_TOTAL" -v pms=$PLAY_MS -v sms=$SILENT_MS -v halves="$HALVES" 'BEGIN {
    per_s = (play - silent) / (pms / 1000.0)
    printf "%s: playing, the audio ISR runs %.1f M instructions/s = %.1f%% of the chip (silent: %.1f M/s); %s audio halves in %d ms\n",
        label, per_s, 100.0 * per_s / 240.0, silent / (sms / 1000.0), halves, pms + sms
}'
