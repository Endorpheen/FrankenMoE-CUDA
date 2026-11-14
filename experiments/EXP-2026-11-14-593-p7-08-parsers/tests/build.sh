#!/bin/bash
# Differential gate build for FG572 (nine parser families, 18 cases).
#
#   tests/build.sh            rebuild what changed and run the gate
#
# Layout:
#   draft.goose               the port, imports base/runtime.goose
#   tests/fixture.cpp         native session vs port, 18 cases
#   tests/build/              generated C, objects, executable (not committed)
#
# The canonical runtime goes through `goose --standalone`, then gcc -O0: the
# gate is a correctness run, not a performance measurement. gen.o is cached on
# the mtime of draft.goose and base/runtime.goose.
set -euo pipefail

S=$(cd "$(dirname "$0")/.." && pwd)
L=${LLAMA_DIR:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
G=${GOOSE_BIN:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
OUT=$S/build
BIN=$OUT/fg572-parsers

mkdir -p "$OUT"

need_gen=0
if [ ! -f "$OUT/gen.o" ] || [ ! -f "$OUT/runtime-gen.h" ]; then
    need_gen=1
elif [ "$S/source/runtime.goose" -nt "$OUT/gen.o" ] || [ false ]; then
    need_gen=1
fi

if [ "$need_gen" = 1 ]; then
    echo "[fg572] goose --standalone"
    timeout 600 "$G" --standalone -o "$OUT/runtime-gen.c" --header "$OUT/runtime-gen.h" "$S/source/runtime.goose"
    echo "[fg572] gcc -O0 runtime-gen.c"
    timeout 300 "$CC" -O0 -std=c11 -c "$OUT/runtime-gen.c" -o "$OUT/gen.o"
else
    echo "[fg572] gen.o cached"
fi

echo "[fg572] g++ fixture.cpp"
timeout 300 "$CXX" -O1 -std=c++17 \
    -I"$OUT" -I"$L/common" -I"$L/include" -I"$L/src" -I"$L/ggml/include" \
    -c "$S/tests/fixture.cpp" -o "$OUT/fixture.o"

echo "[fg572] link"
timeout 300 "$CXX" "$OUT/fixture.o" "$OUT/gen.o" \
    "$L/build-cuda/bin/libggml-base.so" \
    -L"$L/build-cuda/bin" -lllama-common \
    -Wl,-rpath,"$L/build-cuda/bin" -o "$BIN"

echo "[fg572] run"
"$BIN" "$L"
