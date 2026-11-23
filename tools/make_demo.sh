#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The FM-1's demos (SETUP's TOOLS: DEMO 1, DEMO 2): tools/sd_singularity.c and tools/sd_armada.c recorded as what
# they do to the engine (tools/sd_demo_rec.c), into firmware/src/sd_demo.h and sd_demo2.h. Run it after a performance
# or the engine changes; tests/run_sd.sh fails while a header is not its recording's.
#   tools/make_demo.sh
set -e
cd "$(dirname "$0")/.."
mkdir -p build/sd
cc -O2 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Ifirmware/src -Itools -o build/sd/sd_demo_rec tools/sd_demo_rec.c -lm  # (run_sd.sh's flags)
build/sd/sd_demo_rec firmware/src/sd_demo.h build/sd/demo_render.wav > build/sd/demo_rec.txt
tail -1 build/sd/demo_rec.txt
cc -O2 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Ifirmware/src -Itools -DDEMO_SRC='"sd_armada.c"' \
    -DDEMO_NAME='"ARMADA"' -DDEMO_PFX='"SD_DEMO2"' -o build/sd/sd_demo2_rec tools/sd_demo_rec.c -lm
build/sd/sd_demo2_rec firmware/src/sd_demo2.h build/sd/demo2_render.wav > build/sd/demo2_rec.txt
tail -1 build/sd/demo2_rec.txt
rm -f build/sd/demo_render.wav build/sd/demo2_render.wav
