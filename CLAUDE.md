# Working on Star Distroyer

- Bare-metal FM-1 firmware on Dinghy 0.5. The plans are in docs/sd/; the current step is the first one in the Sound
  Plan's §6 table that is not done (P0..V3, R1..R4, D1..D3, S1, C1, S2, S3, S4 and L1, the lab, are done; HW: the user has an FM-1 since 2026-11-16
  and played it: the CPU read 90 to 100 %, the work now (2026-11-25: SPACE waits only for the job's looper part, the plate heard a block later: 98-105 % -> 85-93 % on the device, Sound Plan 5.5.2); W1, the console page, built 2026-11-23: `sd_console.h`, `web/console/`, `tests/console_test.mjs`; the install always needs the user's go-ahead).
  Two cores again since 2026-11-19 (the plate, the drums and half of each sub-block on the second, `SD_C1_HALVES`:
  an hour on the user's FM-1 at 63 %, no misread): the crash at the boot loader's core supplies is gone
  with M-VAVE's own raised at the boot (hal/fm1_sys.h `fm1_supply_stock`; Sound Plan 6.8); SETUP's CORES (K2 on its MORE layer)
  one from the next start; a one-core build `DINGHY_DEFS=-DSD_TWO_CORES=0`; the SMK-37 one core. The
  diagnosis over USB (`build/fm1_diag -w 250 -o build/diag.txt`, tools/fm1_diag.c) reads the device as it plays. The sequencer: docs/sd/ Sequencer Plan (the
  reference, v0.3); `firmware/src/sd_seq.h`. The plans hold the design and what each step decided, not a log: the
  history is git's.
- After every change: `tests/run_sd.sh` (and `tests/run_host.sh tests/scripts/c1.txt`, `q1.txt`, `q2.txt` and `q3.txt` when the app changes). It
  stops at a failure (`set -e`): when it does, the renders in build/audio are the last run's, so check its end
  before measuring any of them. Every
  step needs both gates: hear it (build/audio/*.wav) and fit it (`./build.sh`, `tools/emu_strict.sh`: fm1-live's
  realistic mode, each instruction its cycles and the flash cache its misses, read as the app's own CPU meter; it
  matched the user's FM-1 on 2026-11-16, where `tools/emu_cpu.sh`'s instruction count read half of it). The audio's
  hot code runs from RAM (`SD_RAM` in sd_engine.h, app.ld's `.ram_audio`): the RAM code region is full (32 B left
  on 2026-11-21, after SUB: its frames are made at the tick, the render only adds them), so a function there that
  grows fails the link; a call between it and the flash goes through `SD_FAR`. The audio's
  code that stays in the flash is `SD_HOT` (sd_engine.h): one section, first in the image after memcpy/memset, then
  the tables, then the rest (app.ld), so the flash cache sees it at one place; a new function the control tick or the
  plate's job calls gets `SD_HOT` too, and a new flash function an interrupt calls goes in app.ld's list after it
  (the ISR stubs, key scan, USB service are there). Flash code the audio runs every sub-block costs far more than its
  math: the drums did that first and took DISTROY's loop from 82 % to 111 %, two thirds of it the shared cache (their
  code pushed the synth's out eight times a block); now they are a batch a block ahead on the second core
  (sd_drums.h, the plate's job). New audio work outside the RAM code: once a block, or the second core's. On the
  user's FM-1 (2026-11-23, `fm1_diag -w`) the drums still cost the first core ~16 points that fm1-live does not show
  (95-104 % with SEQ A and B in POLY, 66 % in strict): they are out (`SD_DRUMS` 0, sd_engine.h), their code kept and
  tested (`-DSD_DRUMS=1`: sd_test and the DRUM app tests). Core 2's flash work is under-read by the emulator: a change
  there needs a device reading. The SD_HOT section has 42 KB of its own (app.ld; if it outgrows that the link fails: add 8 KB,
  0xC800 next, the same cache sets), so what follows never moves with it. Compare CPU readings only at a fixed layout
  (unpinned, a few lines of UI moved it 2-4 points), and like with like (`STRICT_PROF` itself reads ~0.8 lower).
  Past 100 % the main loop starves and the watchdog restarts the FM-1 8 s later (2026-11-20, the user's freeze; one
  core at ARMADA's climax reads 139 %): app_audio's brake skips a block after three interrupts over in a row (aud_held,
  CPU FULL), and `last_run` (.noinit) says RESTART ... at the next start. A demo's cpu_log over 100 % means skipped
  blocks, not a freeze.
- Levels (2026-11-21, the gain staging): a knob that shapes the sound must not move the level by much. A new stage
  that adds gain gets a make-up as DRIVE has (sd_engine.h's make-ups, at control rate), and `tools/sd_gain.c` (every
  knob x every patch, `tools/sd_loud.h`'s meter) shows it; `test_gain_staging` guards DRIVE, the factory's loudness and
  the limiters. A change to a make-up's law changes what an older patch is moved to: `sd_patch_levels` (lv 0 -> 1) must
  then give back what it takes, or the factory patches change (check old vs new, TRIM out). A new factory patch gets a
  TRIM (its loudest 400 ms at -11 LUFS).
- The flash keeps each save twice (`firmware/src/store_io.h`, store.h's places; 2026-11-20): a struct that grows must
  leave `STORE_FOOT` bytes (the app's `_Static_assert`s), slots 3 and 4 hold one sector, slots 5..7 are copies. Never
  write a blob on the FM-1 (the stock firmware's patch settings lie there). `tests/store_test.c` cuts the power at
  every step of a save; `tests/app_fuzz_test.c` (the button masher, ASan) must stay green when the UI changes.
- `./build.sh` stops before the package when a check fails (its line says `FAIL`): never read only its `ok`
  lines, and never measure a `.fwsc` older than the source. CI builds the SMK-37's three models first (`BOARD=smk37 MODEL=pro|elite|mke
  sh build.sh`), whose RAM is tighter (3.2 KB left on 2026-11-17, core 2's stack moved to the pool; the pool has 2.9 KB: the next thing in .bss goes in the pool, or a field smaller): build them too when RAM grows. A constant in 0xFFC00000..0xFFCFFFFF (e.g. `-(1 << 22)`)
  fails the ROM check: clamp in another range, or shift after the clamp.
- `tests/run_sd.sh` runs the tests twice, the second time under ASan/UBSan: a signed overflow there is a bug on
  the FM-1 too (it found MIX's), so keep it clean.
- In a feedback loop, round toward zero (`t < 0 ? -((-t) >> n) : t >> n`), never a bare `>>`: its floor holds a
  loop at -1 or a small DC for ever (R2 found two).
- Only edit Dinghy's own files (`platform.c`, `gfx.c`, `hal/`, `loader/`, `tools/build.py`) when the task needs it,
  and list the departure in README.md.
- Audio (`sd_engine_render`): no float, no 64-bit division, no flash, no loop whose length a parameter sets; fixed
  point stays in range (a `q15mul` of two full-scale values overflows).
- The oscillator's laws (warp, fold, envelope times, cents) live in `tools/gen_tables.py`; change them there and rerun it,
  never edit `sd_tables.h`.
- When a sound choice is open, follow the design the Sound Plan sets out (the user's rule): §3.0 for VOICE, §4.0 for
  the Reactor, §5.0 for DISTROY.
- Name no product or other firmware as a model or an inspiration, anywhere in the repository (code, docs, commit
  messages). Dinghy and its lineage stay credited: that is the code's licence, not inspiration.
- The look: the palette and feel of the built screens (`docs/readme/screen_*.png`, `tools/make_readme_art.sh`): near
  black, ice-white lines, red accents, the "///" marks.
  The logo comes only from `tools/gen_logo.py`.
- The page: `web/` (`web/make_site.py` builds it from the package and `tests/run_sd.sh`'s demos; CI publishes it from
  `main`). A demo is one FM-1 played by one person (`tools/sd_play.h`'s rules); DERELICT layers four such passes.
  Its player (`web/app/`) is the firmware as WebAssembly: when the app changes, also `sh web/app/build.sh` (here in
  Docker: an image with clang and lld) and `node tests/web_test.mjs build/web/stardistroyer.wasm`; the lab
  (`web/lab/`, its fields from `web/app/web_lab.h`'s table of SdPatch) also `tests/run_host.sh tests/scripts/lab.txt`
  and `node tests/lab_test.mjs build/web/stardistroyer.wasm build/host/stardistroyer.wav` (the same sound, sample for
  sample; a field added to SdPatch goes in that table, or the test fails). In
  `stardistroyer.c`, `DINGHY_WEB` makes `app_main` one pass a call (`APP_STATE`); keep its loop free of `continue`
  and `return`.
- The FM-1's demos (SETUP's TOOLS: DEMO 1, DEMO 2) are SINGULARITY and ARMADA recorded as what they do to the
  engine (`tools/sd_demo_rec.c` -> `firmware/src/sd_demo.h`, `sd_demo2.h`, never edited by hand): `tests/run_sd.sh`
  fails while a header is not its performance's recording (after a change to `tools/sd_singularity.c`,
  `tools/sd_armada.c`, `tools/sd_play.h`, the structs or `sd_demo_fmt.h`'s scalars): run `tools/make_demo.sh`.
  `tests/demo_test.c` plays each back against its render, sample for sample (`-DDEMO2`: the second). A demo's CPU on
  the strict emulator: `DINGHY_DEFS=-DSD_CPU_LOG=1 ./build.sh`, then `cpu_log` (the worst of each half second, %).
- Never install on an FM-1, push, or tag a release without asking.
