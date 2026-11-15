#!/bin/bash
# Staged S2.2 reproduction: compose, generate, build OFF and ON, gate, dumps, symbol check,
# hashes. Everything happens inside this staged copy; no repository path is written.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
J=${J:-32}

cd "$ROOT"
mkdir -p goose-gen evidence

# 1. Composition (one module, no imports), typecheck, one standalone C translation unit.
{
    echo "== compose =="
    python3 scripts/compose-goose.py goose-gen/spec-decide.goose \
        goose/misc/spec-decode-decisions.goose
    echo "== check =="
    "$GOOSE" check goose-gen/spec-decide.goose
    echo "== standalone =="
    "$GOOSE" --standalone -o goose-gen/spec-decide.c --header goose-gen/spec-decide.h \
        goose-gen/spec-decide.goose
} 2>&1 | tee evidence/generate.log

# 2. Anchors: the provenance of the transcription (reference copy, no repository path).
python3 scripts/spec-anchors.py | tee evidence/anchors.txt

# 3. Both builds of the whole staged tree; the flag selects the decision path.
for mode in OFF ON; do
    cmake -S . -B "build-spec-${mode,,}" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
          -DFG_GOOSE_SPEC="$mode" 2>&1 | tee "evidence/spec-${mode,,}-configure.log"
    cmake --build "build-spec-${mode,,}" -j "$J" 2>&1 | tee "evidence/spec-${mode,,}-build.log"
done

# 4. Gate: seven switched decisions plus the loop, against the native oracle.
./build-spec-off/fg-spec-smoke | tee evidence/smoke-off.txt
./build-spec-on/fg-spec-smoke  | tee evidence/smoke-on.txt

# 5. Whole-loop transcript parity: the two builds must print the same text.
./build-spec-off/fg-spec-smoke --dump > evidence/dump-off.txt
./build-spec-on/fg-spec-smoke  --dump > evidence/dump-on.txt
if diff -q evidence/dump-off.txt evidence/dump-on.txt > /dev/null; then
    echo "switch off/on loop transcripts identical ($(wc -l < evidence/dump-off.txt) lines)"
else
    echo "DUMP MISMATCH"; diff -u evidence/dump-off.txt evidence/dump-on.txt | head -40; exit 1
fi

# 6. Symbols: one definition per sdp_ export on ON, none on OFF; the native bodies on both.
{
    for s in sdp_ngram_draft sdp_accept sdp_emit_count sdp_mtp_width sdp_mtp_head_clamp \
             sdp_mtp_rewind sdp_mtp_catchup sdp_step_plan sdp_rollback sdp_step_advance \
             sdp_tail_trim; do
        on=$(nm -D --defined-only build-spec-on/libfg-spec-core.so | awk -v s="$s" '$3==s' | wc -l)
        off=$(nm -D --defined-only build-spec-off/libfg-spec-core.so | awk -v s="$s" '$3==s' | wc -l)
        printf "%-22s ON=%s OFF=%s\n" "$s" "$on" "$off"
    done
} | tee evidence/nm-sdp-counts.txt
{
    echo "== native bodies and the draft source, both libraries =="
    for s in ngram_draft_native accept_native emit_count_native step_plan_native \
             rollback_native advance_native tail_trim_native bmoe::ngram_draft; do
        on=$(nm -C -D --defined-only build-spec-on/libfg-spec-core.so | grep -c "$s")
        off=$(nm -C -D --defined-only build-spec-off/libfg-spec-core.so | grep -c "$s")
        printf "%-26s ON=%s OFF=%s\n" "$s" "$on" "$off"
    done
} | tee evidence/nm-native-counts.txt
nm -D --defined-only build-spec-on/libfg-spec-core.so  | sort > evidence/nm-spec-on.txt
nm -D --defined-only build-spec-off/libfg-spec-core.so | sort > evidence/nm-spec-off.txt
nm -D --defined-only build-spec-on/libfg-spec-core.so | grep -E " sdp_" > evidence/nm-spec-on-sdp.txt
ldd build-spec-on/fg-spec-smoke > evidence/ldd-smoke-on.txt 2>&1

# 7. Sizes and hashes.
{
    sha256sum goose/misc/spec-decode-decisions.goose goose-gen/spec-decide.goose \
              goose-gen/spec-decide.c goose-gen/spec-decide.h \
              common/fg-spec-decide.h common/fg-spec-decide.cpp \
              common/fg-goose-spec-bridge.h common/fg-goose-spec-bridge.cpp \
              runtime/core/src/engine/ngram_draft.cpp runtime/core/include/bmoe/ngram_draft.h \
              tests/fg_spec_smoke.cpp scripts/spec-gate.sh scripts/spec-anchors.py \
              CMakeLists.txt
    sha256sum build-spec-off/libfg-spec-core.so build-spec-on/libfg-spec-core.so \
              build-spec-off/fg-spec-smoke build-spec-on/fg-spec-smoke \
              build-spec-on/CMakeFiles/fg-goose-spec.dir/goose-gen/spec-decide.c.o
} | tee evidence/sha256.txt
stat -c '%s %n' goose-gen/spec-decide.c goose-gen/spec-decide.h \
    common/fg-spec-decide.cpp common/fg-goose-spec-bridge.cpp tests/fg_spec_smoke.cpp \
    build-spec-off/libfg-spec-core.so build-spec-on/libfg-spec-core.so \
    build-spec-off/fg-spec-smoke build-spec-on/fg-spec-smoke \
    build-spec-on/CMakeFiles/fg-goose-spec.dir/goose-gen/spec-decide.c.o | tee evidence/sizes.txt

echo "OK: S2.2 staged gate complete"
