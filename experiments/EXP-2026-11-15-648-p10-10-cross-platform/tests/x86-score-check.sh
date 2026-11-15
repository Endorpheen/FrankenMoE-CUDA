#!/usr/bin/env bash
# Cross-check the transcribed x86 score chain against the real native function.
#
# The real ggml_backend_cpu_x86_score() reads CPUID and a compile-time flag word,
# so one build exercises one flag word. This script builds the check once per
# flag combination and requires port == real every time; the host's actual
# feature word is the input, so a wrong weight or a wrong requirement shows up
# immediately.
#
#   bash tests/x86-score-check.sh
set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SUB=$(cd "$HERE/.." && pwd)
GOOSE=${GOOSE_BIN:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CXX=${CXX:-g++-13}
UP=${FG_UPSTREAM:-/home/end0/AI/FrankenMoE-CUDA/upstream/llama.cpp}
OUT=${OUT_DIR:-$SUB/build}/x86-score

mkdir -p "$OUT"
"$GOOSE" --standalone -o "$OUT/feat.c" --header "$OUT/feat.h" "$SUB/source/feat-cpu-features.goose" >/dev/null
gcc-13 -O1 -std=c11 -w -c "$OUT/feat.c" -o "$OUT/feat.o"

WORDS=(
    ""
    "-DGGML_FMA -DGGML_F16C"
    "-DGGML_AVX -DGGML_AVX2 -DGGML_FMA -DGGML_F16C -DGGML_SSE42 -DGGML_BMI2"
    "-DGGML_AVX512 -DGGML_AVX2 -DGGML_FMA -DGGML_F16C -DGGML_SSE42 -DGGML_BMI2"
    "-DGGML_AVX512 -DGGML_AVX512_VBMI -DGGML_AVX512_BF16 -DGGML_AVX512_VNNI"
    "-DGGML_AVX_VNNI -DGGML_AVX2 -DGGML_AVX"
    "-DGGML_AMX_INT8 -DGGML_AVX512 -DGGML_AVX512_VNNI"
    "-DGGML_FMA -DGGML_F16C -DGGML_SSE42 -DGGML_BMI2 -DGGML_AVX -DGGML_AVX2 -DGGML_AVX_VNNI -DGGML_AVX512 -DGGML_AVX512_VBMI -DGGML_AVX512_BF16 -DGGML_AVX512_VNNI -DGGML_AMX_INT8"
)

rc=0
i=0
for w in "${WORDS[@]}"; do
    i=$((i + 1))
    # shellcheck disable=SC2086
    if ! $CXX -O2 -std=c++17 -DGGML_BACKEND_DL -I "$OUT" -I "$UP/ggml/include" -I "$UP/ggml/src" $w \
            "$HERE/x86-score-check.cpp" "$OUT/feat.o" -o "$OUT/check-$i"; then
        echo "[x86-score] word $i failed to build"
        rc=1
        continue
    fi
    "$OUT/check-$i" || rc=1
done
exit $rc
