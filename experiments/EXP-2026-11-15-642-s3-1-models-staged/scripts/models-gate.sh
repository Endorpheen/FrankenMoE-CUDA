#!/usr/bin/env bash
# S3.1 gate: the model-management decisions behind FG_GOOSE_MODELS.
#
# Reproduces the whole slice from the module sources: composition, typecheck, one
# standalone C unit, both builds of the staged tree, the differential gate, the
# OFF/ON transcript diff and the symbol counts. Writes evidence/ and prints the
# verdict lines. No model, no server, no child process, no GPU.
#
#   bash scripts/models-gate.sh
#
# The root is derived from the script location; nothing here uses an absolute
# scratch path.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GOOSE=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
JOBS="${JOBS:-32}"
EV="$ROOT/evidence"

cd "$ROOT"
mkdir -p "$EV" goose-gen

echo "== 1. composition, typecheck, standalone =="
{
    python3 scripts/compose-goose.py goose-gen/models.goose \
        goose/server/model-manager.goose \
        goose/server/hot-reload.goose \
        goose/server/connectors.goose
    "$GOOSE" check goose-gen/models.goose
    "$GOOSE" --standalone -o goose-gen/models.c --header goose-gen/models.h goose-gen/models.goose
} 2>&1 | tee "$EV/model-generate.log"

echo "== 2. build, flag OFF =="
rm -rf build-model-off
cmake -S . -B build-model-off -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DFG_GOOSE_MODELS=OFF > "$EV/model-off-configure.log" 2>&1
cmake --build build-model-off -j "$JOBS" > "$EV/model-off-build.log" 2>&1

echo "== 3. build, flag ON =="
rm -rf build-model-on
cmake -S . -B build-model-on -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DFG_GOOSE_MODELS=ON > "$EV/model-on-configure.log" 2>&1
cmake --build build-model-on -j "$JOBS" > "$EV/model-on-build.log" 2>&1

echo "== 4. the gate =="
./build-model-off/fg-model-smoke | tee "$EV/model-smoke-off.txt"
./build-model-on/fg-model-smoke | tee "$EV/model-smoke-on.txt"

./build-model-off/fg-model-smoke --dump > "$EV/model-dump-off.txt"
./build-model-on/fg-model-smoke --dump > "$EV/model-dump-on.txt"
# the only expected difference is the summary line (mode and goose_calls)
head -n -1 "$EV/model-dump-off.txt" > "$EV/model-dump-off.body"
head -n -1 "$EV/model-dump-on.txt" > "$EV/model-dump-on.body"
if diff -q "$EV/model-dump-off.body" "$EV/model-dump-on.body" > /dev/null; then
    echo "transcripts identical: $(wc -l < "$EV/model-dump-off.body") lines"
else
    echo "TRANSCRIPTS DIFFER"; diff "$EV/model-dump-off.body" "$EV/model-dump-on.body" | head -20; exit 1
fi

echo "== 5. symbols =="
{
    for s in mg_pred mg_unload_action mg_victim hr_preset_norm hr_unload_set \
             cn_find_binary cn_upstream_host; do
        on=$(nm -D --defined-only build-model-on/libfg-model-core.so | grep -c " T $s\$" || true)
        off=$(nm -D --defined-only build-model-off/libfg-model-core.so | grep -c " T $s\$" || true)
        echo "$s ON=$on OFF=$off"
    done
} | tee "$EV/model-nm-switched.txt"

{
    for s in pred_native unload_action_native victim_native preset_norm_native \
             unload_set_native connector_target_native; do
        on=$(nm -D --defined-only build-model-on/libfg-model-core.so | grep -c " T _ZN8fg_model.*${s}E" || true)
        off=$(nm -D --defined-only build-model-off/libfg-model-core.so | grep -c " T _ZN8fg_model.*${s}E" || true)
        echo "$s ON=$on OFF=$off"
    done
} | tee "$EV/model-nm-native.txt"

nm -D --defined-only build-model-on/libfg-model-core.so > "$EV/model-nm-on.txt"
nm -D --defined-only build-model-off/libfg-model-core.so > "$EV/model-nm-off.txt"
ldd build-model-on/fg-model-smoke > "$EV/model-ldd-smoke-on.txt"

echo "== 6. hashes and sizes =="
{
    sha256sum goose/server/model-manager.goose goose/server/hot-reload.goose \
              goose/server/connectors.goose goose-gen/models.goose goose-gen/models.c \
              goose-gen/models.h common/fg-model-decide.h common/fg-model-decide.cpp \
              common/fg-goose-model-bridge.h common/fg-goose-model-bridge.cpp \
              tests/fg_model_smoke.cpp scripts/models-gate.sh CMakeLists.txt
} | tee "$EV/model-sha-sources.txt"

{
    sha256sum build-model-off/libfg-model-core.so build-model-on/libfg-model-core.so \
              build-model-off/fg-model-smoke build-model-on/fg-model-smoke \
              build-model-on/CMakeFiles/fg-goose-models.dir/goose-gen/models.c.o \
              "$EV/model-dump-off.body" "$EV/model-dump-on.body"
} | tee "$EV/model-sha-artifacts.txt"

{
    stat -c '%s %n' goose/server/model-manager.goose goose/server/hot-reload.goose \
        goose/server/connectors.goose goose-gen/models.goose goose-gen/models.c \
        goose-gen/models.h common/fg-model-decide.cpp common/fg-goose-model-bridge.cpp \
        tests/fg_model_smoke.cpp build-model-off/libfg-model-core.so \
        build-model-on/libfg-model-core.so build-model-off/fg-model-smoke \
        build-model-on/fg-model-smoke build-model-on/CMakeFiles/fg-goose-models.dir/goose-gen/models.c.o
} | tee "$EV/model-sizes.txt"

echo "== verdict =="
grep -h '^FG643_MODEL mode=' "$EV/model-smoke-off.txt" "$EV/model-smoke-on.txt"
echo "done"
