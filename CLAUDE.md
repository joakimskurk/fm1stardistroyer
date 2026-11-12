# Working on Star Distroyer

- Bare-metal FM-1 firmware on Dinghy 0.5. The plans are in docs/sd/; the current step is the first one in the Sound
  Plan's §6 table that is not done (P0..V3, R1..R4, D1..D3, S1, C1, S2, S3, S4 and L1, the lab, are done; HW waits for the user's FM-1, so work
  goes on without it, next W1, the console page; the install always needs the user's go-ahead). The sequencer: docs/sd/ Sequencer Plan (the
  reference, v0.3); `firmware/src/sd_seq.h`. The plans hold the design and what each step decided, not a log: the
  history is git's.
- After every change: `tests/run_sd.sh` (and `tests/run_host.sh tests/scripts/c1.txt`, `q1.txt`, `q2.txt` and `q3.txt` when the app changes). It
  stops at a failure (`set -e`): when it does, the renders in build/audio are the last run's, so check its end
  before measuring any of them. Every
  step needs both gates: hear it (build/audio/*.wav) and fit it (`./build.sh`, `tools/emu_cpu.sh`).
- `./build.sh` stops before the package when a check fails (its line says `FAIL`): never read only its `ok`
  lines, and never measure a `.fwsc` older than the source. CI builds the SMK-37's three models first (`BOARD=smk37 MODEL=pro|elite|mke
  sh build.sh`), whose RAM is tighter (884 B left on 2026-11-11): build them too when RAM grows. A constant in 0xFFC00000..0xFFCFFFFF (e.g. `-(1 << 22)`)
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
- Never install on an FM-1, push, or tag a release without asking.
