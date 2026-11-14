#!/usr/bin/env bash
# Gate build and run for P6-09 S5 / P7-02 S5 (llama_model_saver model path).
#
#   tests/gen.py -> source/model-saver.goose + tests/slots.h
#   goose --standalone -> build/model_saver.c/.h
#   tests/fixture.cpp links libllama (native llama_model_saver) and the Goose
#   object; it compares KV dumps and tensor descriptors on a synthetic model.
#
# Usage: bash tests/build.sh [final|mutate]
#   final  -O1, generated C cached by source SHA
#   mutate -O0, always regenerated
#
# Generated C, objects and the synthetic vocab files stay in build/.
set -euo pipefail

MODE=${1:-final}
SUB=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
L=${FG_LLAMA:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
SRC=${FG_SRC:-$SUB/source/model-saver.goose}
OUT=${FG_OUT:-$SUB/build}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}

if [ "$MODE" = "mutate" ]; then
    OPT=-O0
    BIN=$OUT/gate-mutate
else
    OPT=-O1
    BIN=$OUT/gate
fi
mkdir -p "$OUT"

# regenerate the goose source and the slot contract from the native tree
python3 "$SUB/tests/gen.py" >/dev/null

SHA=$(sha256sum "$SRC" | awk '{print $1}')
KEY="$SHA-$OPT"
if [ "$MODE" = "mutate" ] || [ ! -f "$OUT/model_saver.o" ] || \
   [ "$(cat "$OUT/.gen.key" 2>/dev/null || true)" != "$KEY" ]; then
    "$GOOSE" --standalone -o "$OUT/model_saver.c" --header "$OUT/model_saver.h" "$SRC" \
        >/dev/null
    "$CC" "$OPT" -std=c11 -c "$OUT/model_saver.c" -o "$OUT/model_saver.o"
    printf '%s\n' "$KEY" > "$OUT/.gen.key"
fi

"$CXX" "$OPT" -std=c++17 -I"$OUT" -I"$SUB/tests" -I"$L/include" -I"$L/src" -I"$L/ggml/include" \
    -c "$SUB/tests/fixture.cpp" -o "$OUT/fixture.o"
"$CXX" "$OUT/fixture.o" "$OUT/model_saver.o" \
    -L"$L/build-cuda/bin" -lllama -lggml -lggml-base \
    -Wl,-rpath,"$L/build-cuda/bin" -o "$BIN"

"$BIN" "$OUT"
