#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Star Distroyer's sound on this computer: the engine's tests, the renders to listen to, the bench.
#   tests/run_sd.sh          -> build/sd/sd_test (run), build/audio/*.wav, the bench's line
#   SD_BENCH_S=10            a shorter bench
#   SD_ASAN=0                skip the sanitizers' second run (~15 s)
set -e
cd "$(dirname "$0")/.."
mkdir -p build/sd build/audio
CFLAGS="-O2 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Ifirmware/src"
cc $CFLAGS -o build/sd/sd_test tests/sd_test.c -lm
cc $CFLAGS -o build/sd/sd_render tools/sd_render.c -lm
cc $CFLAGS -o build/sd/sd_demo tools/sd_demo.c -lm
cc $CFLAGS -o build/sd/sd_cine tools/sd_cine.c -lm
cc $CFLAGS -o build/sd/sd_strings tools/sd_strings.c -lm
cc $CFLAGS -o build/sd/sd_epic tools/sd_epic.c -lm
cc $CFLAGS -o build/sd/sd_range tools/sd_range.c -lm
cc $CFLAGS -o build/sd/sd_song tools/sd_song.c -lm
build/sd/sd_test
if [ "${SD_ASAN:-1}" != 0 ]; then               # again with the sanitizers: overflow, a shift of a negative, memory
    cc -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -Wno-unused-function -Ifirmware/src \
        -o build/sd/sd_test_asan tests/sd_test.c -lm
    build/sd/sd_test_asan > build/sd/sd_test_asan.txt 2>&1 || { tail -20 build/sd/sd_test_asan.txt; exit 1; }
    echo "sd_test: clean under the sanitizers"
fi
rm -f build/audio/*.wav
NAMES="glass bell epiano growl organ tour distroy vowel chords plucks flange phaser ride split"   # SD_PATCH_NAME
p=0
for n in $NAMES; do                         # each patch in the loop, and one note held 2 s then let ring
    build/sd/sd_render render build/audio/loop_$n.wav 8 120 $p
    build/sd/sd_render hold build/audio/hold_$n.wav $p 57
    p=$((p + 1))
done
build/sd/sd_render hold build/audio/hold_growl_low.wav 3 33
build/sd/sd_render field build/audio/v3_field_tour.wav 15            # Gate D VOICE: the XY field
build/sd/sd_render mode poly build/audio/v3_poly_bell.wav 1           # four voices: a chord
build/sd/sd_render mode poly build/audio/v3_poly_epiano.wav 2
build/sd/sd_render mode duo build/audio/v3_duo_tour.wav 5             # two voices, A <-> B by X
build/sd/sd_render hold build/audio/d1_split_lr4.wav 13 40                # D1's A/B: SPLIT's crossover, LR4
build/sd/sd_render hold build/audio/d1_split_cmp.wav 13 40 cmp            #   and complementary
build/sd/sd_render hold build/audio/d2_split_phat.wav 13 33                # D2's A/B: PHAT on SPLIT's low band
SD_NODYN=1 build/sd/sd_render hold build/audio/d2_split_nodyn.wav 13 33    #   and without
build/sd/sd_render render build/audio/d2_distroy_dyn.wav 8 120 6           # DISTROY: PHAT low, SHARP crush + hard
SD_NODYN=1 build/sd/sd_render render build/audio/d2_distroy_nodyn.wav 8 120 6   #   and without
build/sd/sd_render render build/audio/d3_distroy_fb.wav 8 120 6            # D3's A/B: DISTROY's feedback + TAME
SD_NOFB=1 build/sd/sd_render render build/audio/d3_distroy_nofb.wav 8 120 6     #   and without
build/sd/sd_render hold build/audio/d3_distroy_hold_fb.wav 6 45
mkdir -p build/audio/evolve
mkdir -p build/audio/tour
build/sd/sd_render tour build/audio/tour                            # ten patches, a phrase each, ~3.7 min
build/sd/sd_render evolve build/audio/evolve/evolve_tour.wav 5 90             # the essence: slow lines, the macros drifting
build/sd/sd_render evolve build/audio/evolve/evolve_distroy.wav 6 90
build/sd/sd_render pattern build/audio/q1_pattern_growl.wav 3                     # the sequencer: segments, ratchets, PROB
build/sd/sd_render pattern build/audio/q1_pattern_distroy_mutate.wav 6 25
build/sd/sd_render macros build/audio/c1_macros_glass.wav 0                      # C1: each macro in turn, ~29 s
build/sd/sd_render macros build/audio/c1_macros_growl.wav 3
build/sd/sd_render macros build/audio/c1_macros_tour.wav 5
build/sd/sd_render macros build/audio/c1_macros_distroy.wav 6
build/sd/sd_render macros build/audio/c1_macros_vowel.wav 7
build/sd/sd_render macros build/audio/c1_macros_plucks.wav 9
build/sd/sd_render macros build/audio/c1_macros_split.wav 13
build/sd/sd_render sweep pos build/audio/r1_sweep_pos.wav 12 45      # the Rider: VOWEL, POS 1 -> 6, 2 s a stage
build/sd/sd_render sweep wp build/audio/q_sweep_wp.wav 10    # sine, triangle (2 s), saw (4 s), pulse (10 s)
build/sd/sd_render sweep fd build/audio/q_sweep_fd.wav 10    # fold regions at 3, 6.5, 8.5 s
build/sd/sd_render sweep xm build/audio/q_sweep_xm.wav 10    # a 1:1 pair, FM 0 -> 4 pi
build/sd/sd_render sweep dt build/audio/q_sweep_dt.wav 8     # four unison carriers spread 0 -> +-21 cents
mkdir -p build/audio/cine
mkdir -p build/audio/strings
build/sd/sd_strings build/audio/strings/strings                  # STRINGS: the cello's and the violin's scales (V4)
build/sd/sd_cine build/audio/cine/star_distroyer_cine.wav build/audio/cine   # the cinematic demo, ~69 s, and its stems
mkdir -p build/audio/epic
build/sd/sd_epic build/audio/epic/star_distroyer_epic.wav   # the second cue, ~127 s (its stems: a second argument, a folder)
build/sd/sd_strings piece build/audio/strings/cello_duo.wav     # the cello duo on STRINGS, ~32 s
build/sd/sd_strings piece build/audio/strings/cello_v4.wav tools/cello_v4.txt   # STRINGS is tools/cello_v4.txt:
cmp build/audio/strings/cello_duo.wav build/audio/strings/cello_v4.wav && rm build/audio/strings/cello_v4.wav   # bit for bit
build/sd/sd_demo build/audio/demo_star_distroyer.wav               # ~105 s of music: five parts layered
mkdir -p build/audio/range                                          # RANGE: one FM-1 played by one person, ~135 s;
build/sd/sd_range build/audio/range/star_distroyer_range.wav > build/audio/range/performance.txt   # every gesture
tail -1 build/audio/range/performance.txt                           # (it stops at one the FM-1 could not play)
mkdir -p build/audio/song                                           # the song, the same rules: one patch at a time,
build/sd/sd_song build/audio/song/star_distroyer_song.wav > build/audio/song/performance.txt   # no layering, ~3:25
tail -1 build/audio/song/performance.txt
build/sd/sd_render bench "${SD_BENCH_S:-60}"
