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
cc $CFLAGS -DSD_DRUMS=1 -o build/sd/sd_test tests/sd_test.c -lm   # (the drums' code kept, tested: SD_DRUMS=1)
cc $CFLAGS -o build/sd/sd_render tools/sd_render.c -lm
cc $CFLAGS -o build/sd/sd_demo tools/sd_demo.c -lm
cc $CFLAGS -o build/sd/sd_cine tools/sd_cine.c -lm
cc $CFLAGS -DSD_DRUMS=1 -o build/sd/sd_contact tools/sd_contact.c -lm   # (FIRST CONTACT's drums: the FM-1's drum engine, set aside in the firmware)
cc $CFLAGS -o build/sd/sd_strings tools/sd_strings.c -lm
cc $CFLAGS -o build/sd/sd_epic tools/sd_epic.c -lm
cc $CFLAGS -o build/sd/sd_range tools/sd_range.c -lm
cc $CFLAGS -o build/sd/sd_song tools/sd_song.c -lm
cc $CFLAGS -o build/sd/sd_tapestry tools/sd_tapestry.c -lm
cc $CFLAGS -o build/sd/sd_derelict tools/sd_derelict.c -lm
cc $CFLAGS -o build/sd/sd_heliopause tools/sd_heliopause.c -lm
cc $CFLAGS -o build/sd/sd_singularity tools/sd_singularity.c -lm
cc $CFLAGS -o build/sd/sd_armada tools/sd_armada.c -lm
build/sd/sd_test
if [ "${SD_ASAN:-1}" != 0 ]; then               # again with the sanitizers: overflow, a shift of a negative, memory
    cc -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -Wno-unused-function -Ifirmware/src \
        -DSD_DRUMS=1 -o build/sd/sd_test_asan tests/sd_test.c -lm
    build/sd/sd_test_asan > build/sd/sd_test_asan.txt 2>&1 || { tail -20 build/sd/sd_test_asan.txt; exit 1; }
    echo "sd_test: clean under the sanitizers"
    cc -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -Wno-unused-function -Ifirmware/src \
        -o build/sd/sd_derelict_asan tools/sd_derelict.c -lm   # its patch changes, under them too (a bus out of
    mkdir -p build/sd/derelict_asan                            # range was found so)
    build/sd/sd_derelict_asan build/sd/derelict_asan/x.wav > build/sd/derelict_asan.txt 2>&1 ||
        { tail -20 build/sd/derelict_asan.txt; exit 1; }
    rm -rf build/sd/derelict_asan
    echo "sd_derelict: clean under the sanitizers"
fi
rm -f build/audio/*.wav
NAMES="growl voyage distroy vowel chords plucks flange phaser fusion split aurora"   # sd_render takes the FM-1's names,
NAMES="$NAMES gravity chrome abyss neon core nova solar laser signal mass overtone horizon choir stardust circuit"
                                            # then the demos' (sd_demo_patches.h); the collection's sixteen (2026-11-22)
for n in $NAMES; do                         # each patch in the loop, and one note held 2 s then let ring
    build/sd/sd_render render build/audio/loop_$n.wav 8 120 $n
    build/sd/sd_render hold build/audio/hold_$n.wav $n 57
done
build/sd/sd_render hold build/audio/hold_growl_low.wav growl 33
build/sd/sd_render field build/audio/v3_field_voyage.wav 15            # Gate D VOICE: the XY field
build/sd/sd_render mode poly build/audio/v3_poly_nebula.wav nebula    # four voices: a chord
build/sd/sd_render mode poly build/audio/v3_poly_aurora.wav aurora
for n in mass overtone horizon choir nova; do build/sd/sd_render mode poly build/audio/pad_poly_$n.wav $n; done   # the
                                            # collection's pads and its chord lead (2026-11-22), a chord each
build/sd/sd_render mode duo build/audio/v3_duo_voyage.wav voyage             # two voices, A <-> B by X
build/sd/sd_render hold build/audio/d1_split_lr4.wav split 40                # D1's A/B: SPLIT's crossover, LR4
build/sd/sd_render hold build/audio/d1_split_cmp.wav split 40 cmp            #   and complementary
build/sd/sd_render hold build/audio/d2_split_phat.wav split 33                # D2's A/B: PHAT on SPLIT's low band
SD_NODYN=1 build/sd/sd_render hold build/audio/d2_split_nodyn.wav split 33    #   and without
build/sd/sd_render render build/audio/d2_distroy_dyn.wav 8 120 distroy           # DISTROY: PHAT low, SHARP crush + hard
SD_NODYN=1 build/sd/sd_render render build/audio/d2_distroy_nodyn.wav 8 120 distroy   #   and without
build/sd/sd_render render build/audio/d3_distroy_fb.wav 8 120 distroy            # D3's A/B: DISTROY's feedback + TAME
SD_NOFB=1 build/sd/sd_render render build/audio/d3_distroy_nofb.wav 8 120 distroy     #   and without
build/sd/sd_render hold build/audio/d3_distroy_hold_fb.wav distroy 45
mkdir -p build/audio/evolve
mkdir -p build/audio/tour
build/sd/sd_render tour build/audio/tour                            # ten patches, a phrase each, ~3.7 min
build/sd/sd_render evolve build/audio/evolve/evolve_voyage.wav voyage 90             # the essence: slow lines, the macros drifting
build/sd/sd_render evolve build/audio/evolve/evolve_distroy.wav distroy 90
build/sd/sd_render pattern build/audio/q1_pattern_growl.wav growl                     # the sequencer: segments, ratchets, PROB
build/sd/sd_render pattern build/audio/q1_pattern_distroy_radar.wav distroy 25   # ... RADAR's NOTES on it
build/sd/sd_render macros build/audio/c1_macros_aurora.wav aurora                      # C1: each macro in turn, ~29 s
build/sd/sd_render macros build/audio/c1_macros_growl.wav growl
build/sd/sd_render macros build/audio/c1_macros_voyage.wav voyage
build/sd/sd_render macros build/audio/c1_macros_distroy.wav distroy
build/sd/sd_render macros build/audio/c1_macros_vowel.wav vowel
build/sd/sd_render macros build/audio/c1_macros_plucks.wav plucks
build/sd/sd_render macros build/audio/c1_macros_split.wav split
build/sd/sd_render sweep pos build/audio/r1_sweep_pos.wav 12 45      # the Reactor: VOWEL, POS 1 -> 6, 2 s a stage
build/sd/sd_render sweep wp build/audio/q_sweep_wp.wav 10    # sine, triangle (2 s), saw (4 s), pulse (10 s)
build/sd/sd_render sweep fd build/audio/q_sweep_fd.wav 10    # fold regions at 3, 6.5, 8.5 s
build/sd/sd_render sweep xm build/audio/q_sweep_xm.wav 10    # a 1:1 pair, FM 0 -> 4 pi
build/sd/sd_render sweep dt build/audio/q_sweep_dt.wav 8     # four unison carriers spread 0 -> +-21 cents
mkdir -p build/audio/cine
mkdir -p build/audio/strings
build/sd/sd_strings build/audio/strings/strings                  # STRINGS: the cello's and the violin's scales (V4)
build/sd/sd_cine build/audio/cine/star_distroyer_cine.wav build/audio/cine   # CINEMATIC, ~69 s, and its stems
mkdir -p build/audio/contact
build/sd/sd_contact build/audio/contact/star_distroyer_contact.wav   # FIRST CONTACT, ~3:35 (the site's)
mkdir -p build/audio/epic
build/sd/sd_epic build/audio/epic/star_distroyer_epic.wav   # the second cue, ~127 s (its stems: a second argument, a folder)
build/sd/sd_strings piece build/audio/strings/cello_duo.wav     # the cello duo on STRINGS, ~32 s
build/sd/sd_strings piece build/audio/strings/cello_v4.wav tools/cello_v4.txt   # STRINGS is tools/cello_v4.txt:
cmp build/audio/strings/cello_duo.wav build/audio/strings/cello_v4.wav && rm build/audio/strings/cello_v4.wav   # bit for bit
build/sd/sd_demo build/audio/demo_star_distroyer.wav               # ~105 s of music: five parts layered
mkdir -p build/audio/range                                          # RANGE: one FM-1 played by one person, ~2:02;
build/sd/sd_range build/audio/range/star_distroyer_range.wav > build/audio/range/performance.txt ||   # every gesture
    { tail -5 build/audio/range/performance.txt; exit 1; }
tail -1 build/audio/range/performance.txt                           # (it stops at one the FM-1 could not play)
mkdir -p build/audio/song                                           # the song, the same rules: one patch at a time,
build/sd/sd_song build/audio/song/star_distroyer_song.wav > build/audio/song/performance.txt ||   # no layering, ~3:25
    { tail -5 build/audio/song/performance.txt; exit 1; }
tail -1 build/audio/song/performance.txt
mkdir -p build/audio/tapestry                                       # TAPESTRY: the same rules, the looper building
build/sd/sd_tapestry build/audio/tapestry/star_distroyer_tapestry.wav > build/audio/tapestry/performance.txt ||   # the bed
    { tail -5 build/audio/tapestry/performance.txt; exit 1; }       # (its memory the RAM2 build's), ~88 s
tail -1 build/audio/tapestry/performance.txt
mkdir -p build/audio/derelict                                       # DERELICT: four such passes layered, ~2:06,
build/sd/sd_derelict build/audio/derelict/star_distroyer_derelict.wav > build/audio/derelict/performance.txt ||   # stems too
    { tail -5 build/audio/derelict/performance.txt; exit 1; }
tail -1 build/audio/derelict/performance.txt
mkdir -p build/audio/heliopause                                     # HELIOPAUSE: a cue, four passes layered, ~1:47,
build/sd/sd_heliopause build/audio/heliopause/star_distroyer_heliopause.wav > build/audio/heliopause/performance.txt ||
    { tail -5 build/audio/heliopause/performance.txt; exit 1; }     # everything RANGE plays, the looper as texture
tail -1 build/audio/heliopause/performance.txt
mkdir -p build/audio/singularity                                    # SINGULARITY: one take, one stem, ~1:57: one
build/sd/sd_singularity build/audio/singularity/star_distroyer_singularity.wav > build/audio/singularity/performance.txt ||
    { tail -5 build/audio/singularity/performance.txt; exit 1; }    # FM-1 at full (A, B, ARP, ORBIT, the looper)
tail -1 build/audio/singularity/performance.txt
mkdir -p build/audio/armada                                         # ARMADA: one take with the drums, ~2:19: the
build/sd/sd_armada build/audio/armada/star_distroyer_armada.wav > build/audio/armada/performance.txt ||
    { tail -5 build/audio/armada/performance.txt; exit 1; }         # theme three times, ORBIT's lift a whole step
tail -1 build/audio/armada/performance.txt
cc $CFLAGS -Itools -o build/sd/sd_demo_rec tools/sd_demo_rec.c -lm  # the FM-1's demo (SETUP): SINGULARITY recorded as
build/sd/sd_demo_rec build/sd/sd_demo.h build/sd/demo_render.wav > build/sd/demo_rec.txt   # what it does to the
cmp -s build/sd/sd_demo.h firmware/src/sd_demo.h ||                 # engine; the header the firmware plays must be
    { echo "FAIL  firmware/src/sd_demo.h is not the performance's recording: run tools/make_demo.sh"; exit 1; }   # it
cc $CFLAGS -o build/sd/demo_test tests/demo_test.c -lm              # and played back: the render, sample for sample
build/sd/demo_test build/audio/singularity/star_distroyer_singularity.wav
cc $CFLAGS -Itools -DDEMO_SRC='"sd_armada.c"' -DDEMO_NAME='"ARMADA"' -DDEMO_PFX='"SD_DEMO2"' -o build/sd/sd_demo2_rec \
    tools/sd_demo_rec.c -lm                                         # the second (SETUP's ALGORITHM: DEMO 2): ARMADA
build/sd/sd_demo2_rec build/sd/sd_demo2.h build/sd/demo2_render.wav > build/sd/demo2_rec.txt
cmp -s build/sd/sd_demo2.h firmware/src/sd_demo2.h ||
    { echo "FAIL  firmware/src/sd_demo2.h is not the performance's recording: run tools/make_demo.sh"; exit 1; }
cc $CFLAGS -DDEMO2 -o build/sd/demo2_test tests/demo_test.c -lm
build/sd/demo2_test build/audio/armada/star_distroyer_armada.wav
rm -f build/sd/demo_render.wav build/sd/demo2_render.wav            # (the recorders' renders: the disk's)
build/sd/sd_render bench "${SD_BENCH_S:-60}"
