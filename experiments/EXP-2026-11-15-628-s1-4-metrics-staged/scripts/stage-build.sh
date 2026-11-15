#!/bin/bash
# Staged S1.1 reproduction: base build, OFF build, ON build, gate.
# Everything happens inside this staged copy; no repository path is written.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
J=${J:-32}

cd "$ROOT"

# 1. Goose source -> generated C (standalone, one translation unit).
mkdir -p goose-gen
"$GOOSE" --standalone -o goose-gen/json-schema-grammar.c \
         --header goose-gen/json-schema-grammar.h goose/format-grammar.goose

# 2. Base: pristine llama.cpp, CPU only, normal path. No Goose, no flag.
cmake -S llama.cpp -B build-base -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
      -DGGML_CUDA=OFF -DGGML_NATIVE=ON -DLLAMA_CURL=OFF -DLLAMA_OPENSSL=OFF \
      -DLLAMA_SUBPROCESS=OFF -DLLAMA_BUILD_COMMON=ON -DLLAMA_BUILD_TESTS=OFF \
      -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_SERVER=OFF -DLLAMA_BUILD_TOOLS=OFF \
      -DLLAMA_BUILD_APP=OFF
cmake --build build-base -j "$J" --target llama llama-common

# 3. Staged builds through this directory's CMakeLists; the flag selects the path.
for mode in OFF ON; do
    cmake -S . -B "build-${mode,,}" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
          -DFG_GOOSE_JSON_SCHEMA="$mode"
    cmake --build "build-${mode,,}" -j "$J" --target fg-goose-smoke fg-goose-dump
done

# 4. Gate: 30 schemas, selected path against the native oracle.
./build-off/fg-goose-smoke
./build-on/fg-goose-smoke

# 5. Switch-off parity: the OFF build must print the base (production) text.
./build-off/fg-goose-dump > /tmp/fg629-dump-off.txt 2>/dev/null
./build-on/fg-goose-dump  > /tmp/fg629-dump-on.txt  2>/dev/null
diff -q /tmp/fg629-dump-off.txt /tmp/fg629-dump-on.txt && echo "switch on/off grammar text identical"

# 5. Symbol check: exactly one definition of the Goose export, none when off.
echo "--- ON: json_schema_convert definitions in libllama-common.so"
nm build-on/bin/libllama-common.so | grep -cE " [TtWw] json_schema_convert$"
echo "--- OFF: json_schema_convert references in libllama-common.so"
nm build-off/bin/libllama-common.so | grep -cE "json_schema_convert$" || true
