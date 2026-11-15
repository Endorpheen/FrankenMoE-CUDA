#!/bin/bash
# Staged S3.2 reproduction: compose the P9-05 modules, build the staged tree twice
# (FG_GOOSE_SESSION=OFF and ON), run the differential gate, and write evidence/.
#
# Nothing depends on the caller's working directory: the experiment root is derived from this
# script's location. Overrides, all optional:
#   GOOSE_BIN, CC, CXX, OPT (gate at -O1), SUB
set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SUB=$(cd "$HERE/.." && pwd)
GOOSE=${GOOSE_BIN:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
OPT=${OPT:--O1}
E=$SUB/evidence
mkdir -p "$E"

cd "$SUB"

echo "== 1. composition =="
python3 scripts/compose-goose.py goose-gen/session.goose \
    goose/misc/session-lifecycle.goose \
    goose/misc/runtime-facade.goose \
    goose/misc/draft-source.goose | tee "$E/compose.log"
"$GOOSE" check goose-gen/session.goose 2>&1 | tee "$E/goose-check.log"
"$GOOSE" --standalone -o goose-gen/session.c --header goose-gen/session.h \
    goose-gen/session.goose 2>&1 | tee "$E/generate.log"
ls -l goose-gen/session.c goose-gen/session.h >> "$E/generate.log"

echo "== 2. provenance of the transcription =="
python3 scripts/session-anchors.py | tee "$E/anchors.txt"

echo "== 3. builds, both modes =="
for mode in off on; do
    flag=$(echo "$mode" | tr 'a-z' 'A-Z')
    cmake -S . -B "build-sess-$mode" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
          -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
          -DFG_GOOSE_JSON_SCHEMA=OFF -DFG_GOOSE_SESSION="$flag" \
          -DFG_GATE_OPT="$OPT" > "$E/$mode-configure.log" 2>&1
    cmake --build "build-sess-$mode" -j "$(nproc)" --target fg-session-smoke \
          > "$E/$mode-build.log" 2>&1
    echo "built $mode ($(wc -l < "$E/$mode-build.log") log lines)"
done

echo "== 4. gate =="
./build-sess-off/fg-session-smoke | tee "$E/smoke-off.txt"
./build-sess-on/fg-session-smoke | tee "$E/smoke-on.txt"
./build-sess-off/fg-session-smoke --dump 2>/dev/null > "$E/dump-off.txt"
./build-sess-on/fg-session-smoke --dump 2>/dev/null > "$E/dump-on.txt"
diff -q "$E/dump-off.txt" "$E/dump-on.txt" && echo "dump: identical" | tee "$E/dump-diff.txt"

echo "== 5. symbols =="
SYMS="ss_sentinels ss_hot_slots ss_hot_reason ss_sentinel_mib ss_vram_clamp ss_probe_margin
      ss_probe_budget ss_resize_plan rts_n_batch rts_ubatch rts_plan nd_draft_ubatch
      nd_verify_batch"
: > "$E/nm-export-counts.txt"
for s in $SYMS; do
    on=$(nm -D --defined-only build-sess-on/libfg-session-core.so | awk -v s="$s" '$3==s' | wc -l)
    off=$(nm -D --defined-only build-sess-off/libfg-session-core.so | awk -v s="$s" '$3==s' | wc -l)
    printf '%-18s ON=%s OFF=%s\n' "$s" "$on" "$off" | tee -a "$E/nm-export-counts.txt"
done
nm -D --defined-only build-sess-on/libfg-session-core.so | grep -c ' goose_init$' \
    > "$E/nm-goose-init.txt" || true
nm -D --defined-only -C build-sess-on/libfg-session-core.so > "$E/nm-on.txt"
nm -D --defined-only -C build-sess-off/libfg-session-core.so > "$E/nm-off.txt"
ldd build-sess-on/fg-session-smoke > "$E/ldd-smoke-on.txt"

echo "== 6. same gate at -O0 =="
for mode in off on; do
    flag=$(echo "$mode" | tr 'a-z' 'A-Z')
    cmake -S . -B "build-sess-o0-$mode" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
          -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
          -DFG_GOOSE_JSON_SCHEMA=OFF -DFG_GOOSE_SESSION="$flag" \
          -DFG_GATE_OPT=-O0 > "$E/o0-$mode-configure.log" 2>&1
    cmake --build "build-sess-o0-$mode" -j "$(nproc)" --target fg-session-smoke \
          > "$E/o0-$mode-build.log" 2>&1
done
{
    ./build-sess-o0-off/fg-session-smoke
    ./build-sess-o0-on/fg-session-smoke
} | tee "$E/smoke-o0.txt"

echo "== 7. flags (last -O wins on the command line) =="
{
    echo "== -O1 core CXX_FLAGS"; grep CXX_FLAGS build-sess-on/CMakeFiles/fg-session-core.dir/flags.make
    echo "== -O1 goose C_FLAGS"; grep C_FLAGS build-sess-on/CMakeFiles/fg-goose-session.dir/flags.make
    echo "== -O0 core CXX_FLAGS"; grep CXX_FLAGS build-sess-o0-on/CMakeFiles/fg-session-core.dir/flags.make
} > "$E/flags.txt"

echo "== 8. sizes and hashes =="
{
    ls -l goose-gen/session.goose goose-gen/session.c goose-gen/session.h \
          common/fg-session-decide.h common/fg-session-decide.cpp \
          common/fg-goose-session-bridge.h common/fg-goose-session-bridge.cpp \
          tests/fg_session_smoke.cpp \
          build-sess-off/libfg-session-core.so build-sess-on/libfg-session-core.so \
          build-sess-off/fg-session-smoke build-sess-on/fg-session-smoke \
          build-sess-on/CMakeFiles/fg-goose-session.dir/goose-gen/session.c.o
} > "$E/sizes.txt"
{
    sha256sum goose/misc/*.goose goose-gen/*.goose common/fg-*.[ch]* tests/fg_session_smoke.cpp \
              scripts/session-gate.sh scripts/session-anchors.py CMakeLists.txt
    echo
    sha256sum build-sess-off/libfg-session-core.so build-sess-on/libfg-session-core.so \
              build-sess-off/fg-session-smoke build-sess-on/fg-session-smoke \
              build-sess-on/CMakeFiles/fg-goose-session.dir/goose-gen/session.c.o \
              "$E/dump-off.txt" "$E/dump-on.txt"
} | tee "$E/sha256.txt"

echo "done"
