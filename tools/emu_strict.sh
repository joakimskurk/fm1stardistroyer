#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 joakimskurk
# The chip's load as the FM-1 has it (the "fit it" gate): fm1-live's realistic mode (FM1_STRICT=1) gives each
# instruction its measured cycles and the flash cache its misses (32 KB shared by both cores, ~300 cycles a line),
# and the app's own meter (cpu_permille: the worst audio interrupt of the last second, in % of its period) is read
# from the RAM at the end. tools/emu_cpu.sh counts instructions instead: it read about half of what a real FM-1
# showed (2026-11-16: 44 % there, 90 to 100 % on the device; this reads 98 % for that build).
# The run: the splash, PLAY at 1.6 s, the meter's window 4.2 .. 5.2 s.
#   tools/emu_strict.sh build/stardistroyer.fwsc [label]
#   FM1_LIVE     the fm1-live binary (default: ~/fm1-emu/target/release/fm1-live; cargo build --release -p fm1-live)
#   EMU_ELF      the package's ELF, for the meter's address (default: build/stardistroyer.elf)
#   STRICT_KEYS  more input, in fm1-emu's --keys form as tools/emu_cpu.sh's EMU_KEYS (1700-5200:n0 holds a key,
#                2800:enc5:+79 turns K4), turned into fm1-live's here
#   STRICT_ARGS  more options (e.g. --flaky-core1 1e-5 --seed 7)
#   STRICT_PROF  a file for both cores' time-weighted PCs and the flash cache's misses, 3 .. 5 s (FM1_TPROF)
# An interpreter runs it (no JIT in this mode): about 75 s for the 5.2 s
set -e
PKG="$1"
[ -f "$PKG" ] || { echo "usage: tools/emu_strict.sh PACKAGE.fwsc [label]"; exit 2; }
LABEL="${2:-$(basename "$PKG" .fwsc)}"
LIVE="${FM1_LIVE:-$HOME/fm1-emu/target/release/fm1-live}"
ELF="${EMU_ELF:-$(dirname "$0")/../build/stardistroyer.elf}"
[ -x "$LIVE" ] || { echo "emu_strict: no fm1-live at $LIVE"; exit 2; }
[ -f "$ELF" ] || { echo "emu_strict: no ELF at $ELF (EMU_ELF)"; exit 2; }
WORK="$(mktemp -d "${TMPDIR:-/tmp}/sd-strict.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/work"
cp "$PKG" "$WORK/pkg.fwsc"
ADDR=$(python3 - "$ELF" <<'EOF'
import struct, sys
b = open(sys.argv[1], "rb").read()
shoff, = struct.unpack_from("<I", b, 0x20)
shentsize, shnum = struct.unpack_from("<HH", b, 0x2E)
secs = [struct.unpack_from("<IIIIIIIIII", b, shoff + i * shentsize) for i in range(shnum)]
for s in secs:
    if s[1] == 2:                                   # SHT_SYMTAB, its strings in sh_link
        st = secs[s[6]]
        for o in range(s[4], s[4] + s[5], 16):
            name, value = struct.unpack_from("<II", b, o)
            n = b[st[4] + name:b.index(b"\0", st[4] + name)]
            if n == b"cpu_permille":
                print(value)
                sys.exit(0)
sys.exit("emu_strict: no cpu_permille in the ELF")
EOF
)
KEYS=$(python3 - "1600:PLAY${STRICT_KEYS:+,$STRICT_KEYS}" <<'EOF'
import re, sys                                  # fm1-emu's --keys form -> fm1-live's (no ranges; eN:clicks)
out = []
for k in sys.argv[1].split(","):
    m = re.fullmatch(r"(\d+)-(\d+):(\w+)", k.strip())
    if m:
        out += [f"{m.group(1)}:{m.group(3)}", f"{m.group(2)}:{m.group(3)}^"]
        continue
    m = re.fullmatch(r"(\d+):enc(\d+):([+-]?\d+)", k.strip())
    if m:
        out.append(f"{m.group(1)}:e{m.group(2)}:{int(m.group(3))}")
        continue
    if not re.fullmatch(r"\d+:\w+\^?", k.strip()):
        sys.exit(f"emu_strict: what is {k!r}?")
    out.append(k.strip())
print(",".join(out))
EOF
)
PROF=""
[ -n "$STRICT_PROF" ] && PROF="$WORK/tprof.txt,3000,5000"
(cd "$WORK" && env FM1_STRICT=1 FM1_BENCH_KEYS="$KEYS" FM1_DUMPRAM="$WORK/ram.bin" \
    ${PROF:+FM1_TPROF=$PROF} "$LIVE" --fwsc pkg.fwsc --shot 5200 $STRICT_ARGS > "$WORK/run.txt" 2>&1) || true
[ -f "$WORK/ram.bin" ] || { cat "$WORK/run.txt"; echo "emu_strict: no RAM dump"; exit 1; }
[ -n "$STRICT_PROF" ] && [ -f "$WORK/tprof.txt" ] && cp "$WORK/tprof.txt" "$STRICT_PROF"
python3 - "$WORK/ram.bin" "$ADDR" "$LABEL" <<'EOF'
import struct, sys
ram, addr, label = open(sys.argv[1], "rb").read(), int(sys.argv[2]), sys.argv[3]
v, = struct.unpack_from("<I", ram, addr - 0x01C00000)
if v == 0xFFFFFFFF:
    sys.exit(f"{label}: the meter has no reading yet")
print(f"{label}: the worst audio interrupt {v / 10:.1f}% of its period (the chip's timing, FM1_STRICT){' OVER' if v >= 1000 else ''}")
EOF
