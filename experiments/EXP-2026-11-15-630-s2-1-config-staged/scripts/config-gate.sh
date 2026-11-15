#!/bin/bash
# S2.1 reproduction: the Goose config validator inside the staged runtime build,
# behind FG_GOOSE_CONFIG, with the native validate() as the oracle.
#
#   OFF (default): validate() -> validate_native(), the pre-integration body.
#   ON           : validate() -> fg_config_to_flat() -> cfg_validate() -> the
#                  carrier's English texts.
#
# Everything happens inside this staged copy; no repository path is written and
# no model, server or GPU is involved. Generated C is not kept in evidence.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
J=${J:-32}
EV="$ROOT/evidence"
mkdir -p "$EV" "$ROOT/goose-gen"
cd "$ROOT"

echo "[1/6] compose the P9-01 set and generate one standalone C unit"
python3 scripts/compose-goose.py goose-gen/config-slice.goose \
    goose/model/arch-registry.goose \
    goose/cli/config-validate.goose \
    goose/cli/config-resolve.goose | tee "$EV/compose.log"
"$GOOSE" check goose-gen/config-slice.goose 2>&1 | tee "$EV/check.log"
"$GOOSE" --standalone -o goose-gen/config-slice.c --header goose-gen/config-slice.h \
    goose-gen/config-slice.goose 2>&1 | tee "$EV/standalone.log"
# The generator is deterministic; make the generated file newer than any object
# so a regenerated unit can never be skipped as "up to date".
touch goose-gen/config-slice.c goose-gen/config-slice.h

echo "[2/6] staged builds, flag OFF and ON (whole staged tree)"
for mode in OFF ON; do
    lower="${mode,,}"
    cmake -S . -B "build-config-$lower" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
          -DFG_GOOSE_CONFIG="$mode" > "$EV/config-$lower-configure.log" 2>&1
    cmake --build "build-config-$lower" -j "$J" > "$EV/config-$lower-build.log" 2>&1
done

echo "[3/6] gate"
./build-config-off/fg-config-smoke | tee "$EV/smoke-off.txt"
./build-config-on/fg-config-smoke  | tee "$EV/smoke-on.txt"

echo "[4/6] the gate at -O1, and no llama.cpp on the link line"
grep -h -o -- "-O[0-9]" build-config-on/CMakeFiles/fg-config-core.dir/flags.make | sort -u | tee "$EV/opt-level.txt"
ldd build-config-on/fg-config-smoke | tee "$EV/ldd-smoke-on.txt"

echo "[5/6] symbols"
{
    echo "ON  cfg_validate definitions : $(nm build-config-on/libfg-config-core.so  | grep -cE ' [TtWw] cfg_validate$')"
    echo "OFF cfg_validate definitions : $(nm build-config-off/libfg-config-core.so | grep -cE ' [TtWw] cfg_validate$')"
    echo "OFF cfg_validate occurrences : $(nm build-config-off/libfg-config-core.so | grep -c 'cfg_validate' || true)"
    echo "ON  validate_native          : $(nm build-config-on/libfg-config-core.so  | grep -cE ' [TtWw] _ZN4bmoe15validate_native')"
    echo "OFF validate_native          : $(nm build-config-off/libfg-config-core.so | grep -cE ' [TtWw] _ZN4bmoe15validate_native')"
} | tee "$EV/nm-counts.txt"
nm -D --defined-only build-config-on/libfg-config-core.so  | grep -E " (cfg_|ar_|cc_)" | tee "$EV/nm-on.txt"
nm -D --defined-only build-config-off/libfg-config-core.so | grep -E " (cfg_|ar_|cc_)" > "$EV/nm-off.txt" || true

echo "[6/6] SHA-256 of inputs, stage files and artefacts"
{
    sha256sum goose/cli/config-validate.goose goose/cli/config-resolve.goose \
              goose/model/arch-registry.goose goose-gen/config-slice.goose \
              goose-gen/config-slice.c goose-gen/config-slice.h \
              runtime/core/src/config.cpp runtime/core/include/bmoe/config.h \
              common/fg-config-native.h common/fg-goose-config-bridge.h \
              common/fg-goose-config-bridge.cpp tests/fg_config_smoke.cpp \
              tests/config_mutate.py CMakeLists.txt scripts/config-gate.sh
    sha256sum build-config-off/libfg-config-core.so build-config-on/libfg-config-core.so \
              build-config-off/fg-config-smoke build-config-on/fg-config-smoke \
              build-config-on/CMakeFiles/fg-goose-config.dir/goose-gen/config-slice.c.o
} | tee "$EV/sha256.txt"
