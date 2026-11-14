#!/usr/bin/env bash
# The accepted P6-09 S4 gate against the extended source (S1-S4 + S5).
# Proves the carried writer path is unchanged by the S5 addition.
#
# Usage: bash tests/regress-s4.sh
set -euo pipefail

SUB=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
L=${FG_LLAMA:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
S4=${FG_S4:-/home/end0/AI/FrankenMoE-CUDA/experiments/EXP-2026-11-14-606-p6-09-saver-writer}
OUT=$SUB/build/s4regress
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}

mkdir -p "$OUT"

# the S4 fixture does not know the S5 f32 accessor
cat > "$OUT/shim.c" <<'EOF'
#include <stdint.h>
#include <string.h>
int64_t ms_bits(float v) { uint32_t b; memcpy(&b, &v, 4); return (int64_t) b; }
EOF

"$CC" -O1 -std=c11 -c "$OUT/shim.c" -o "$OUT/shim.o"
"$CXX" -O1 -std=c++17 -I"$SUB/build" -I"$S4/tests" \
    -I"$L/include" -I"$L/src" -I"$L/ggml/include" \
    -c "$S4/tests/fixture.cpp" -o "$OUT/fixture.o"
"$CXX" "$OUT/fixture.o" "$SUB/build/model_saver.o" "$OUT/shim.o" \
    -L"$L/build-cuda/bin" -lllama -lggml -lggml-base \
    -Wl,-rpath,"$L/build-cuda/bin" -o "$OUT/gate"
"$OUT/gate" "$OUT/tmp.gguf"
