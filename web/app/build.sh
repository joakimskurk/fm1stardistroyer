#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Joakim (joakimskurk)
# Star Distroyer for the browser: firmware/src/stardistroyer.c compiled to WebAssembly on web/app/web_host.c (the
# page's FM-1) instead of the chip: build/web/stardistroyer.wasm, no imports, no C library. clang with the wasm32
# target and wasm-ld (lld): apt install clang lld. (No such clang here? Any Docker image with both will do:
# docker run --rm -v "$PWD":/w -w /w IMAGE sh web/app/build.sh)
#
#   sh web/app/build.sh [OUT]
set -e
OUT="${1:-build/web/stardistroyer.wasm}"
CC="${CC_WASM:-clang}"
mkdir -p "$(dirname "$OUT")"
if ! command -v wasm-ld >/dev/null; then           # (clang runs wasm-ld by that name: a versioned one, linked as it)
    LD=$(command -v wasm-ld-18 || command -v wasm-ld-19 || command -v wasm-ld-20 || command -v wasm-ld-17 || \
         command -v wasm-ld-16 || command -v wasm-ld-15 || command -v wasm-ld-14 || true)
    [ -n "$LD" ] || { echo "web/app/build.sh: no wasm-ld (apt install lld)" >&2; exit 1; }
    mkdir -p build/web/bin && ln -sf "$LD" build/web/bin/wasm-ld && PATH="$PWD/build/web/bin:$PATH"
fi
$CC --target=wasm32 -O2 -ffreestanding -nostdlib -mbulk-memory -Wall -Werror -Wno-unused-function \
    -DDINGHY_HOST -DDINGHY_WEB -DAPP_VER="\"${APP_VER:-web}\"" -Ifirmware/hal -Ifirmware/src -Ifirmware/gen \
    -Wl,--no-entry -Wl,--strip-all -Wl,-z,stack-size=262144 -o "$OUT" firmware/src/stardistroyer.c
echo "web/app/build.sh: $OUT ($(wc -c < "$OUT") B)"
