# Licensing

Star Distroyer, like Dinghy under it, is free software under the GNU General Public License, version 3 only (`GPL-3.0-only`, full text in
`LICENSE`). You may use, study, change and share it. If you give someone a firmware built from it, give them its
complete source under the same licence too.

## Who made what

| | |
| --- | --- |
| The platform: `firmware/hal/`, `firmware/loader/`, `crt0.S`, `app.ld`, `firmware/src/` (`usb.c`, `ota.c`, `recovery.c`, `lcd.c`, `gfx.c`, `libc.c`, `bootguard.h`, `uac_fir.h`), `tools/` | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments ([Felucca](https://github.com/hugelton/Felucca)), with the changes of [SLOOP](https://github.com/isod89/sloop-fm1) (isod89) |
| The SMK-37 board layer: `firmware/hal/smk_input.h`, `fm1_flash_map.h`, the SMK-37 parts of `fm1_cc.h`, `fm1_flash.h`, `fm1_lcd_hw.h`, `lcd.c`, `ota.c`, `bootguard.h`, `firmware/loader/`, `tools/fm1pkg_make.py`, `tools/fm1_install.py`, `tools/build.py`; `firmware/src/smk_wheel.h`; `tests/ota_test.c`, `ldr_test.c`, `install_test.py`, `smk_wheel_test.c`, `smk_pkg_test.py` | Copyright (C) 2026 M. Geisler (genkigenki/makkuro), from [genkigenki/sloop-fm1](https://github.com/genkigenki/sloop-fm1) branch `smk37` (the reference port of the [fm1-to-smk37](https://github.com/genkigenki/fm1-to-smk37) kit) |
| The TRS MIDI IN `firmware/src/midi_uart.c` | Felucca's, as sloopDX carries it, with the ring read by content from Felucca [Salt] 0.9-salt14 by ChanceTheMaker (found by keremimo) |
| The update loader's screen: `firmware/loader/ldr_screen.c`, `tests/ldr_screen_test.c` | Copyright (C) 2026 zvenson (from OberVave, the Firmware Hub's look); the idea, a loader that draws, its 5 x 7 letters and the order of things from [Skiff](https://github.com/illuminus5/skiff) by illuminus5, GPL-3.0 |
| From OberVave: the storage (blobs, slots), `LCD_IDLE`, the SMK-37 wiring of `platform.c`, `store.h`, the pad event queue in `smk_input.h`, `tests/smk_flash_test.c`, `tests/run_tests.sh`, the CI workflow | Copyright (C) 2026 zvenson |
| The wiring `platform.c`, the host mode `tests/host.c` and `tests/run_host.sh`, Dinghy's docs in `docs/` (`API.md`, `RULES.md`, `MAKE-IT-YOURS.md`, `SMK37.md`, `DINGHY.md`) | zvenson |
| The installer in the browser: `web/fm1pkg.js`, `web/fm1ota.js` (unchanged), and the install flow of `web/index.html` and `web/make_site.py` | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, from [SLOOP](https://github.com/isod89/sloop-fm1) (isod89) |
| Star Distroyer: the app `firmware/src/stardistroyer.c` (in the place of Dinghy's `dinghy.c`) and its `sd_*.h`, `firmware/hal/fm1_ram2.h`, `tools/gen_tables.py`, `tools/gen_logo.py`, `tools/sd_*.c`, `tools/sd_*.h`, `tools/emu_cpu.sh`, `tools/lab.sh`, `tools/make_card.sh`, `tools/make_readme_art.sh`, `tests/sd_test.c`, `tests/run_sd.sh`, `tests/scripts/`, `tests/web_test.mjs`, `tests/lab_test.mjs`, Star Distroyer in the browser `web/app/` (but the Orbitron font, below) and its lab `web/lab/`, the page `web/index.html` and `web/make_site.py` (but for what is SLOOP's in them, above), `web/make_og.py`, the pictures in `docs/readme/`, `docs/card.png`, `web/og.png`, `web/icon.png`, `fmhub.json`, the plans in `docs/sd/`, `README.md` | Copyright (C) 2026 Joakim (joakimskurk) |
| The changes to Dinghy's own files, listed in README.md: each of those files says so at its top ("Changed for Star Distroyer") | Copyright (C) 2026 Joakim (joakimskurk) |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Terminus font 8x16 (ter-u16n), rendered into `firmware/gen/felucca_font.h` | SIL OFL 1.1 | `assets/fonts/Terminus-LICENSE.txt` |
| Orbitron (The Orbitron Project Authors, The League of Moveable Type), unchanged in `assets/fonts/Orbitron-VariableFont_wght.ttf`: the logo's letters, rendered by `tools/gen_logo.py` into `firmware/src/sd_logo.h` and the pictures in `docs/readme/` and `web/`; on the page, the player panel's letters (served with its licence, `app/orbitron-OFL.txt`) | SIL OFL 1.1 | `assets/fonts/Orbitron-OFL.txt` |
| The second core: `firmware/hal/fm1_cpu1.h`, `fm1_cpu1.S`, `fm1_cpu1_stop.h`, from [felucca-bsp](https://github.com/hugelton/felucca-bsp) (`hal/fm1_cpu1.h`, `fm1_cpu1.S`, `fm1_cpu1_stop.h`, `fm1_flash_access.h`), adapted to Dinghy (each file says how). Copyright (C) 2026 Kerem Kilic (Ellic Studio), from [Melodee](https://github.com/keremimo/melodee) under MPL-2.0 by agreement; Charles Vestal; Leo Kuroshita (@kurogedelic), Hügelton Instruments. The files stay under the MPL; Star Distroyer as a whole is GPL-3.0-only (the MPL allows it as a secondary licence) | MPL-2.0 | <https://mozilla.org/MPL/2.0/> |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## The logo

`assets/logo.svg` / `logo.png` carry the designburg signet: Copyright (C) designburg, all rights reserved, not under
the GPL. It marks Dinghy itself; a fork uses its own logo.

Star Distroyer's own logo is drawn by `tools/gen_logo.py` (into `firmware/src/sd_logo.h`, the pictures in
`docs/readme/` and `web/`), GPL-3.0-only as the rest of Star Distroyer; its letters are Orbitron (above).

## The name

"Star Distroyer" is the name of this firmware, and it is its author's (Joakim, joakimskurk). The GPL covers the code,
not the name: under section 7(e) of the GPL, this licence gives no right to use "Star Distroyer", or a name that can
be taken for it, as the name of another work. A changed version, a fork that is passed on, or a firmware built on this
one takes a name of its own, and does not call itself Star Distroyer on its screens, in its package or on its page.
Saying where it comes from ("based on Star Distroyer by joakimskurk") is welcome, and is what the notices above ask
for anyway. A copy passed on unchanged keeps the name.

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments; "M-VAVE", "FM-1", "SMK-37" and "MKE-P37"
belong to their owners. Dinghy and Star Distroyer are not made or endorsed by them.
