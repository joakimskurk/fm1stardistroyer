#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The lab (web/lab/) on this computer, from the source as it is: the player built (web/app/build.sh), the lab made
# one file (web/make_site.py --lab-html: everything inlined, nothing fetched), and opened in the browser. The file
# runs from the disk (Chrome, Edge or Brave); copy it anywhere. It is not on the site.
#   tools/lab.sh                 build build/star_distroyer_lab.html and open it
#   LAB_NOBUILD=1 tools/lab.sh   from the player last built (build/web/stardistroyer.wasm)
#   LAB_NOOPEN=1 tools/lab.sh    do not open it
# The build needs a clang that has the wasm32 target, and wasm-ld: Homebrew's (brew install llvm lld), a Linux's
# (apt install clang lld), or Docker with an image that has both (LAB_DOCKER=IMAGE; roboseq-dev:latest if it is there).
set -e
cd "$(dirname "$0")/.."
WASM=build/web/stardistroyer.wasm
OUT=build/star_distroyer_lab.html
wasm_cc() {                                        # a clang that can build for wasm32
    t=build/web/cc-probe.c; mkdir -p build/web; echo 'int f(void){return 1;}' > "$t"
    for c in "$CC_WASM" /opt/homebrew/opt/llvm/bin/clang /usr/local/opt/llvm/bin/clang clang; do
        [ -n "$c" ] && command -v "$c" >/dev/null 2>&1 || continue
        if "$c" --target=wasm32 -c -o /dev/null "$t" 2>/dev/null; then rm -f "$t"; echo "$c"; return 0; fi
    done
    rm -f "$t"; return 1
}
if [ -z "$LAB_NOBUILD" ]; then
    if CC=$(wasm_cc); then
        for d in /opt/homebrew/opt/lld/bin /usr/local/opt/lld/bin "$(dirname "$CC")"; do
            [ -x "$d/wasm-ld" ] && PATH="$d:$PATH"
        done
        CC_WASM="$CC" sh web/app/build.sh "$WASM"
    else
        IMG="${LAB_DOCKER:-roboseq-dev:latest}"
        if command -v docker >/dev/null 2>&1 && docker image inspect "$IMG" >/dev/null 2>&1; then
            docker run --rm -v "$PWD":/w -w /w "$IMG" sh web/app/build.sh "$WASM"
        else
            echo "tools/lab.sh: no clang with the wasm32 target here (brew install llvm lld, or LAB_DOCKER=IMAGE)" >&2
            [ -f "$WASM" ] || exit 1
            echo "tools/lab.sh: from the player last built ($WASM), which may be older than the source" >&2
        fi
    fi
fi
[ -f "$WASM" ] || { echo "tools/lab.sh: no $WASM" >&2; exit 1; }
if [ -n "$(find firmware/src firmware/hal web/app -newer "$WASM" \( -name '*.c' -o -name '*.h' \) 2>/dev/null | head -1)" ]; then
    echo "tools/lab.sh: note: the source has changed since $WASM was built" >&2
fi
python3 web/make_site.py --lab-html "$OUT"
if [ -z "$LAB_NOOPEN" ]; then
    if command -v open >/dev/null 2>&1; then open "$OUT"; elif command -v xdg-open >/dev/null 2>&1; then xdg-open "$OUT"; fi
fi
