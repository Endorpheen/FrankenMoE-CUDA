#!/usr/bin/env bash
# S1.6 staged build: the P9-03/P9-04 stream decisions behind FG_GOOSE_STREAM.
#
# Generates the standalone Goose module, builds the staged project twice (flag
# OFF and ON), runs the differential gate in both configurations and dumps the
# nm evidence. The root is derived from this script's location; nothing here
# touches the repository, a model, the GPU or the production build.
#
# Usage: bash scripts/stage-build-stream.sh
#   FG_GOOSE overrides the toolchain path, FG_JOBS the build parallelism.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
G=${FG_GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
JOBS=${FG_JOBS:-32}
EV=$ROOT/evidence
mkdir -p "$EV" "$ROOT/goose-gen"

cd "$ROOT"

echo "== 1. Goose module -> one standalone C translation unit"
# The composition unit imports the six canon slices; every slice typechecks on
# its own first, then as the combined unit that is actually built.
for f in goose/stream/expert_lru.goose goose/stream/dense_place.goose \
         goose/stream/stream_plan.goose goose/stream/hook_rules.goose \
         goose/stream/overlap_rules.goose goose/stream/row_select.goose; do
    "$G" check "$f" >/dev/null
done
"$G" check goose/stream/stream_policy_all.goose >/dev/null
"$G" --standalone -o goose-gen/stream-policy.c --header goose-gen/stream-policy.h \
    goose/stream/stream_policy_all.goose | tee "$EV/goose-generate.log"

echo "== 2. staged build, FG_GOOSE_STREAM=OFF"
cmake -S . -B build-stream-off -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DFG_GOOSE_JSON_SCHEMA=OFF -DFG_GOOSE_STREAM=OFF >"$EV/stream-off-configure.log" 2>&1
cmake --build build-stream-off -j "$JOBS" --target fg-goose-stream-gate \
      >"$EV/stream-off-build.log" 2>&1

echo "== 3. staged build, FG_GOOSE_STREAM=ON"
cmake -S . -B build-stream-on -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DFG_GOOSE_JSON_SCHEMA=OFF -DFG_GOOSE_STREAM=ON >"$EV/stream-on-configure.log" 2>&1
cmake --build build-stream-on -j "$JOBS" --target fg-goose-stream-gate \
      >"$EV/stream-on-build.log" 2>&1

echo "== 4. gate, both configurations"
./build-stream-off/fg-goose-stream-gate >"$EV/gate-off.txt" 2>&1
./build-stream-on/fg-goose-stream-gate >"$EV/gate-on.txt" 2>&1
tail -1 "$EV/gate-off.txt"
tail -1 "$EV/gate-on.txt"

# Every decision line must be identical between the two builds; only the summary
# (mode, goose_calls) may differ.
diff <(grep -v '^FG641_SMOKE' "$EV/gate-off.txt") \
     <(grep -v '^FG641_SMOKE' "$EV/gate-on.txt") >"$EV/gate-off-on-detail.diff"
test ! -s "$EV/gate-off-on-detail.diff"

echo "== 5. nm: one definition of each switched export on ON, none on OFF"
{
    echo "# the six switched exports, defined text symbols"
    for b in off on; do
        echo "## build-stream-$b"
        nm --defined-only "build-stream-$b/bin/libllama-common.so" |
            grep -wE 'xl_touch|xl_evict_tail|xl_set_budget|dw_subtract|ov_wave_list|rs_table_verdict' ||
            echo "(none)"
    done
    echo "# the native transcription must exist in both"
    for b in off on; do
        echo -n "## build-stream-$b fg_native_ defined: "
        nm --defined-only "build-stream-$b/bin/libllama-common.so" |
            grep -cE ' T fg_native_' || true
    done
} >"$EV/nm-switched.txt"
cat "$EV/nm-switched.txt"

echo "== 6. SHA-256"
{
    sha256sum goose/stream/*.goose goose-gen/stream-policy.c goose-gen/stream-policy.h \
              common/fg-stream-policy.h common/fg-stream-policy.cpp \
              tests/fg_goose_stream_gate.cpp tests/mutate-stream.sh \
              CMakeLists.txt scripts/stage-build-stream.sh
    sha256sum build-stream-off/bin/libllama-common.so build-stream-on/bin/libllama-common.so \
              build-stream-off/fg-goose-stream-gate build-stream-on/fg-goose-stream-gate
    sha256sum build-stream-on/CMakeFiles/fg-goose-stream.dir/goose-gen/stream-policy.c.o
} >"$EV/sha-artifacts.txt"
cat "$EV/sha-artifacts.txt"
