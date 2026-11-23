#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Dinghy's host tests (no hardware). Run from the repo root after building, the SMK-37 first, the FM-1 last
# (both boards write the same build/ intermediates; the tests expect the FM-1 binaries):
#   BOARD=smk37 sh build.sh; BOARD=smk37 MODEL=elite sh build.sh; BOARD=smk37 MODEL=mke sh build.sh; sh build.sh
#   sh tests/run_tests.sh
# The FM-1 app on the host (tests/run_host.sh), MIDI and the SMK-37 on the host (tests/scripts), the flash map, the
# update path (ota.c, the loader, tools/fm1_install.py; from the fm1-to-smk37 kit's reference layer,
# genkigenki/sloop-fm1 smk37) and the packages.
set -e
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"   # (fm1pkg_make.py: the SDK files of a package)
cd "$(dirname "$0")/.."
OUT=build/host
mkdir -p "$OUT"
CC="${CC:-cc} -O1 -Wall -Wno-unused-function"
fail=0
run() { echo "== $1"; shift; "$@" || fail=1; }

[ -f build/stardistroyer.fwsc ] || { echo "run ./build.sh first"; exit 1; }

run "FM-1 app on the host: the default script plays, screens and sound" sh tests/run_host.sh
run "SMK-37 app on the host: the default script" env BOARD=smk37 sh tests/run_host.sh
run "FM-1 MIDI: USB, the TRS jack, a clock, the channel" sh tests/run_host.sh tests/scripts/midi.txt
run "FM-1 MIDI recorded: step by step, chords, live, KEEP from the ARP, over the tracks" sh tests/run_host.sh tests/scripts/m1.txt
run "SMK-37: velocity, a pad, an outer key, the pitch wheel" env BOARD=smk37 sh tests/run_host.sh tests/scripts/smk37.txt
$CC -DDINGHY_HOST -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "$OUT/app_key_test" tests/app_key_test.c
run "the key: SEQ B's RND in A's (the master's) root and scale" "$OUT/app_key_test" "$OUT/app_key_test" tests/scripts/k2.txt
$CC -DDINGHY_HOST -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "$OUT/app_home_test" tests/app_home_test.c
run "HOME's switches: the chain on black, the players on white, as the screen" "$OUT/app_home_test" "$OUT/app_home_test" tests/scripts/home1.txt
$CC -DDINGHY_HOST -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "$OUT/app_loop_test" tests/app_loop_test.c
run "the looper off keeps its loop; TOOLS' CLEAR clears it, asked" "$OUT/app_loop_test" "$OUT/app_loop_test" tests/scripts/lp1.txt
$CC -DDINGHY_HOST -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "$OUT/app_lane_test" tests/app_lane_test.c
run "ORBIT: a lane picks its destination beside it, then its amount" "$OUT/app_lane_test" "$OUT/app_lane_test" tests/scripts/ob2.txt
$CC -DDINGHY_HOST -DSD_DRUMS=1 -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "$OUT/app_layers_test" tests/app_layers_test.c   # (every page: the drums' too, SD_DRUMS=1)
run "ALGORITHM's layers: every page's every layer shown, every knob there does something" "$OUT/app_layers_test" "$OUT/app_layers_test" tests/scripts/lay1.txt
$CC -DDINGHY_HOST -DSD_DRUMS=1 -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "$OUT/app_drum4_test" tests/app_drum4_test.c   # (the drums' code kept: SD_DRUMS=1)
run "DRUM: ALGORITHM the layers, the pads the drum, KIT's K4 the fourth's ENGINE" "$OUT/app_drum4_test" "$OUT/app_drum4_test" tests/scripts/dr4.txt
$CC -DDINGHY_HOST -DSD_DRUMS=1 -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "$OUT/app_recoff_test" tests/app_recoff_test.c   # (REC on DRUM: SD_DRUMS=1)
run "SEQ's REC ends from any page; ENV and LFO go to their pages" "$OUT/app_recoff_test" "$OUT/app_recoff_test" tests/scripts/rec3.txt
$CC -DDINGHY_HOST -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "$OUT/app_recgate_test" tests/app_recgate_test.c
run "REC writes each note's GATE from how long it was held" "$OUT/app_recgate_test" "$OUT/app_recgate_test" tests/scripts/recg.txt
$CC -DDINGHY_HOST -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "$OUT/app_save_test" tests/app_save_test.c
run "SAVE while it plays: on, the flash at STOP; PRESETS at the one" "$OUT/app_save_test" "$OUT/app_save_test" tests/scripts/sv1.txt
run "SAVE while it plays, then STOP: in the flash" "$OUT/app_save_test" "$OUT/app_save_test" tests/scripts/sv2.txt
$CC -DDINGHY_HOST -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "$OUT/app_song_test" tests/app_song_test.c
run "the songs: the old ORBITs moved over, a song's pattern played" "$OUT/app_song_test" "$OUT/app_song_test" tests/scripts/sg1.txt
run "the songs: SAVE on ORBIT, in the flash" "$OUT/app_song_test" "$OUT/app_song_test" tests/scripts/sg2.txt
$CC -DDINGHY_HOST -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "$OUT/app_iter_test" tests/app_iter_test.c
run "2026-11-23's asks: TIME turns from what plays, SEQ A/B to a pitch exactly, SLIDE's TIME, RADAR's DIV on the beat, CLICK" "$OUT/app_iter_test" "$OUT/app_iter_test"
$CC -DDINGHY_HOST -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "$OUT/app_tail_test" tests/app_tail_test.c
run "ORBIT's LOOP and FREEZE pitch: packed, old songs, the engine" "$OUT/app_tail_test" "$OUT/app_tail_test" tests/scripts/ot1.txt
$CC -DDINGHY_HOST -DSD_DRUMS=1 -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "$OUT/app_drum_test" tests/app_drum_test.c   # (the drums' code kept: SD_DRUMS=1)
run "the drums: DRUM's steps, MIX, a beat loaded and saved; the saves before them kept" "$OUT/app_drum_test" "$OUT/app_drum_test" tests/scripts/dr1.txt
run "the drums: a song's step names a beat" "$OUT/app_drum_test" "$OUT/app_drum_test" tests/scripts/dr2.txt
run "the drums: a song's step turns them off" "$OUT/app_drum_test" "$OUT/app_drum_test" tests/scripts/dr3.txt
$CC -DDINGHY_HOST -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "$OUT/app_rec_test" tests/app_rec_test.c
run "REC while it plays: SEQ's keys on the nearest step as heard" "$OUT/app_rec_test" "$OUT/app_rec_test" tests/scripts/rec1.txt
$CC -DDINGHY_HOST -DSD_DRUMS=1 -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "$OUT/app_rec_test_dr" tests/app_rec_test.c
run "REC while it plays: DRUM's pads on the nearest step as heard (SD_DRUMS=1)" "$OUT/app_rec_test_dr" "$OUT/app_rec_test_dr" tests/scripts/rec2.txt
$CC -DDINGHY_HOST -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "$OUT/app_dub_test" tests/app_dub_test.c
run "SEQ's DUB: a key joins the step's chord, nothing taken" "$OUT/app_dub_test" "$OUT/app_dub_test" tests/scripts/dub1.txt
$CC -DDINGHY_HOST -DDUB_UNDO -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "$OUT/app_dub_undo_test" tests/app_dub_test.c
run "SEQ's TOOLS: UNDO takes a DUB back" "$OUT/app_dub_undo_test" "$OUT/app_dub_undo_test" tests/scripts/dub2.txt
$CC -DDINGHY_HOST -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "$OUT/app_frz_test" tests/app_frz_test.c
run "the freeze log: the inputs, 16 records, the newest first" "$OUT/app_frz_test" "$OUT/app_frz_test" tests/scripts/frz1.txt
$CC -fsanitize=address,undefined -fno-sanitize-recover=all -DDINGHY_HOST -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "$OUT/app_fuzz_test" tests/app_fuzz_test.c
for s in 1 2 3; do
    run "the button masher (seed $s): 120 s of fast random input under ASan: no hang, no stuck note, every page" \
        sh -c "HOST_NO_FILES=1 FUZZ_SEED=$s FUZZ_S=120 '$OUT/app_fuzz_test' '$OUT/app_fuzz_test' > /dev/null"
done
$CC -DDINGHY_HOST -DAPP_VER='"dev"' -Ifirmware/hal -Ifirmware/src -Ifirmware/gen -o "$OUT/app_demo_test" tests/app_demo_test.c
run "the demo from SETUP: it plays" "$OUT/app_demo_test" "$OUT/app_demo_test" tests/scripts/dm1.txt
run "the demo stopped: the player's sound back" "$OUT/app_demo_test" "$OUT/app_demo_test" tests/scripts/dm2.txt
run "the second demo from SETUP: ARMADA plays, the drums with it" "$OUT/app_demo_test" "$OUT/app_demo_test" tests/scripts/dm3.txt

$CC -DBOARD_SMK37=1 -Ifirmware/hal -o "$OUT/smk_flash_test" tests/smk_flash_test.c
run "SMK-37 flash map: settings, blobs, owner tag, slots in the store; never the head, BTIF/USRTRIM, USR, key_mac" "$OUT/smk_flash_test"
$CC -Ifirmware/hal -Ifirmware/src -o "$OUT/store_test" tests/store_test.c
run "the store's two copies (FM-1): the power cut at every step of a save, old saves, read back" "$OUT/store_test"
$CC -DBOARD_SMK37=1 -Ifirmware/hal -Ifirmware/src -o "$OUT/store_test_smk" tests/store_test.c
run "the store's two copies (SMK-37)" "$OUT/store_test_smk"
$CC -o "$OUT/smk_wheel_test" tests/smk_wheel_test.c
run "SMK-37 pitch wheel: the bend is back to 0 at rest, however slowly the wheel was moved" "$OUT/smk_wheel_test"

$CC -o "$OUT/ota_test" tests/ota_test.c
run "M-UPGRADE entry (FM-1)" "$OUT/ota_test" build/stardistroyer.fwsc
head -c 60000 build/stardistroyer.bin > "$OUT/old_app.bin"
python3 tools/fm1pkg_make.py "$OUT/old_app.bin" build/loader/ota.bin "$OUT/old.fwsc" >/dev/null
$CC -o "$OUT/ldr_screen_test" tests/ldr_screen_test.c
run "update loader's screen (from Skiff): UPDATE MODE, WRITING and its bar, UPDATE FAILED" "$OUT/ldr_screen_test" "$OUT"
$CC -o "$OUT/ldr_test" tests/ldr_test.c
run "update loader (FM-1): other app -> this build" "$OUT/ldr_test" "$OUT/old.fwsc" build/stardistroyer.fwsc

# SMK-37: the OTA staging and the loader windows on SMK-layout packages (made here from the FM-1 binaries: only the
# layout and the protocol are under test), then the real SMK packages
python3 tools/fm1pkg_make.py --board smk37 build/stardistroyer.bin build/loader/ota.bin "$OUT/smk_new.fwsc" >/dev/null
python3 tools/fm1pkg_make.py --board smk37 "$OUT/old_app.bin" build/loader/ota.bin "$OUT/smk_old.fwsc" >/dev/null
run "SMK-37 package layout (fm1pkg_make --board smk37): PRCT 0x9D000, stock region table" \
    python3 tests/smk_pkg_test.py "$OUT/smk_new.fwsc" "SMK-37 Pro_900"
$CC -DBOARD_SMK37=1 -o "$OUT/ota_test_smk" tests/ota_test.c
run "M-UPGRADE entry, SMK-37 Pro (staging 0xBB000, parm ota-SMK-37 Pro_016)" "$OUT/ota_test_smk" "$OUT/smk_new.fwsc"
for m in 1 2; do
    $CC -DBOARD_SMK37=1 -DT_MODEL=$m -o "$OUT/ota_test_smk_m$m" tests/ota_test.c
    run "M-UPGRADE entry, SMK-37 model $m (1 Elite, 2 MKE-P37): identity and parm under its name" "$OUT/ota_test_smk_m$m" "$OUT/smk_new.fwsc"
done
$CC -DBOARD_SMK37=1 -o "$OUT/ldr_test_smk" tests/ldr_test.c
run "update loader, SMK-37: app area 0x4000-0x9CFFF, protected ranges untouched" "$OUT/ldr_test_smk" "$OUT/smk_old.fwsc" "$OUT/smk_new.fwsc"
found=0
for m in ":SMK-37 Pro_962" "-elite:SMK-37 Elite_962" "-mke:MKE-P37_962"; do
    for f in build/stardistroyer*-smk37${m%%:*}.fwsc; do
        [ -f "$f" ] || continue
        found=1
        v=$(echo "$f" | sed -n 's/.*stardistroyer-\([0-9]\)\.\([0-9]\).*/\1\2/p'); v=${v:-00}   # 962XY, 96200 a dev build
        run "SMK-37 package $f: layout, identity ${m#*:}$v, the loader marker" python3 tests/smk_pkg_test.py "$f" "${m#*:}$v"
    done
done
[ "$found" = 1 ] || echo "(no SMK-37 packages in build/: BOARD=smk37 sh build.sh first)"

run "installer (tools/fm1_install.py): FM-1 and SMK-37, install and the update-mode resume check the model" python3 tests/install_test.py

[ "$fail" = 0 ] && echo "ALL TESTS PASSED" || echo "SOME TESTS FAILED"
exit "$fail"
