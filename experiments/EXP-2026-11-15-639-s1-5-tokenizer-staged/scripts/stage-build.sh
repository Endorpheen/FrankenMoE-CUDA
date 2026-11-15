#!/bin/bash
# Staged S1.5 reproduction: compose the Goose tokenizer unit, build OFF and ON,
# prove the native bodies are untouched, run the gate, check the symbols.
#
# Everything happens inside this staged copy; no repository path is written.
#
#   scripts/stage-build.sh          full run, logs into evidence/
#   J=16 scripts/stage-build.sh     fewer build jobs
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
J=${J:-32}
EV=${EV:-$ROOT/evidence/s1.5}

cd "$ROOT"
mkdir -p "$EV"

echo "=== 1. compose goose/vocab-type + special-partition + common-strings, --standalone"
./scripts/compose-goose.sh 2>&1 | tee "$EV/compose.log"

echo "=== 2. native bodies are the pristine ones (moved, not rewritten)"
python3 tests/check-native-parity.py reference . 2>&1 | tee "$EV/native-parity.txt"

echo "=== 3. staged build, FG_GOOSE_TOKENIZER=OFF"
cmake -S . -B build-off -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DFG_GOOSE_JSON_SCHEMA=OFF -DFG_GOOSE_TOKENIZER=OFF 2>&1 | tee "$EV/off-configure.log" | tail -3
cmake --build build-off -j "$J" --target llama llama-common 2>&1 | tee "$EV/off-build.log" | tail -3

echo "=== 4. staged build, FG_GOOSE_TOKENIZER=ON"
cmake -S . -B build-on -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DFG_GOOSE_JSON_SCHEMA=OFF -DFG_GOOSE_TOKENIZER=ON 2>&1 | tee "$EV/on-configure.log" | tail -3
cmake --build build-on -j "$J" --target llama llama-common 2>&1 | tee "$EV/on-build.log" | tail -3

echo "=== 5. gate: 182 cases per build"
OUT="$ROOT/build-gate" ./scripts/stage-gate.sh 2>&1 | tee "$EV/gate.log" | grep -E 'FG639|PARITY|MISMATCH|built|wrote'
cp "$ROOT/build-gate/cases-off.txt" "$EV/cases-off.txt"
cp "$ROOT/build-gate/cases-on.txt"  "$EV/cases-on.txt"

echo "=== 6. symbols and ABI"
./scripts/check-symbols.sh 2>&1 | tee "$EV/symbols.txt"

echo "=== 7. hashes"
{
    sha256sum goose/vocab-type.goose goose/special-partition.goose goose/common-strings.goose
    sha256sum goose-gen/tokenizer-decisions.goose goose-gen/tokenizer-decisions.c \
              goose-gen/tokenizer-decisions.h
    sha256sum llama.cpp/src/fg-goose-tokenizer-bridge.cpp llama.cpp/src/fg-goose-tokenizer-bridge.h \
              llama.cpp/src/llama-vocab.cpp llama.cpp/common/common.cpp
    sha256sum tests/fg_goose_tokenizer_smoke.cpp tests/make-native.py tests/gen-cases.py \
              tests/check-native-parity.py
    sha256sum CMakeLists.txt scripts/compose-goose.sh scripts/apply-tokenizer-hooks.py \
              scripts/stage-build.sh scripts/stage-gate.sh scripts/check-symbols.sh
} 2>&1 | tee "$EV/sha-sources.txt"

{
    sha256sum build-off/bin/libllama.so.0.3.0 build-off/bin/libllama-common.so.0.3.0
    sha256sum build-on/bin/libllama.so.0.3.0  build-on/bin/libllama-common.so.0.3.0
    sha256sum build-on/CMakeFiles/fg-goose-tokenizer.dir/goose-gen/tokenizer-decisions.c.o
    sha256sum build-gate/fg-goose-tokenizer-smoke-off build-gate/fg-goose-tokenizer-smoke-on
} 2>&1 | tee "$EV/sha-artifacts.txt"
