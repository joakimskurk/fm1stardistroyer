# Working on Star Distroyer

- Bare-metal FM-1 firmware on Dinghy 0.5. The plans are in docs/sd/; the current step is the first one in the Sound
  Plan's §6 table that is not done (P0..V3, R1..R4, D1..D3, S1 and C1 are done; HW waits for the user's FM-1, so work goes on
  without it; the install always needs the user's go-ahead). The sequencer: docs/sd/ Sequencer Plan, its v0.3
  revisions first; `firmware/src/sd_seq.h`.
- After every change: `tests/run_sd.sh` (and `tests/run_host.sh tests/scripts/c1.txt`, `q1.txt`, `q2.txt` and `q3.txt` when the app changes). It
  stops at a failure (`set -e`): when it does, the renders in build/audio are the last run's, so check its end
  before measuring any of them. Every
  step needs both gates: hear it (build/audio/*.wav) and fit it (`./build.sh`, `tools/emu_cpu.sh`).
- `./build.sh` stops before the package when a check fails (its line says `FAIL`): never read only its `ok`
  lines, and never measure a `.fwsc` older than the source. A constant in 0xFFC00000..0xFFCFFFFF (e.g. `-(1 << 22)`)
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
  the Rider, §5.0 for DESTROY.
- Name no product or other firmware as a model or an inspiration, anywhere in the repository (code, docs, commit
  messages). Dinghy and its lineage stay credited: that is the code's licence, not inspiration.
- The look: the palette and feel of the built screens (`docs/readme/screen_*.png`, `tools/make_readme_art.sh`): near
  black, ice-white lines, red accents, the "///" marks.
  The logo comes only from `tools/gen_logo.py`.
- Never install on an FM-1, push, or tag a release without asking.
