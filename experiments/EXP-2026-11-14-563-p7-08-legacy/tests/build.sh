#!/usr/bin/env bash
# Builds the native legacy oracle, the Goose draft and the C host, then diffs.
set -euo pipefail
cd "$(dirname "$0")"

ROOT=/home/end0/AI/FrankenMoE-CUDA
FLINT=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda
BUNDLE=$ROOT/builds/goose-2026-11-13-stage-run
GOOSE=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose

# 1. Raw legacy oracle: the real src/llama-chat.cpp, no model or weights.
g++ -std=c++17 -O2 \
    -I"$ROOT/upstream/llama.cpp/include" \
    -I"$ROOT/upstream/llama.cpp/src" \
    -I"$ROOT/upstream/llama.cpp/ggml/include" \
    probe.cpp "$ROOT/upstream/llama.cpp/src/llama-chat.cpp" -o probe

# 2. Full common_chat_params oracle: links the flint build bundle. The flint
#    headers must be used, otherwise common_chat_params has a different layout
#    and the probe segfaults (see REVIEW.md).
g++ -std=c++17 -O0 \
    -I"$FLINT/include" -I"$FLINT/common" -I"$FLINT/ggml/include" \
    -I"$FLINT/build-cuda/src" -I"$FLINT/vendor" \
    probe2.cpp -L"$BUNDLE" -lllama-common -lllama -lggml -lggml-base \
    -Wl,-rpath,"$BUNDLE" -o probe2

# 3. Draft port and its host.
"$GOOSE" -o draft.c --standalone --header draft.h ../source/runtime.goose >/dev/null
gcc -O1 -c draft.c -o draft.o
g++ -O1 -I. host.cpp draft.o -o host -lm

# 4. Fixtures.
./probe --tsv > native.tsv
./probe --cases-tsv > native_cases.tsv
./probe --names-tsv > native_names.tsv
LD_LIBRARY_PATH="$BUNDLE" ./probe2 --parts-tsv 2>/dev/null | grep -v '^FG' > native_parts.tsv
./host > goose.tsv
LD_LIBRARY_PATH="$BUNDLE" ./probe2 2>/dev/null | grep -v '^FG' > params.txt || true

python3 compare.py
