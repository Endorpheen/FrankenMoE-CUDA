#!/usr/bin/env bash
# Differential gate for fg377s_plan (EXP-377): the Goose aligned-read geometry against the native
# expressions it replaced in llama-model-loader.cpp.
#
# Distinct from exercising the loader branch: the pinned server uses mmap and this build exposes no mmap
# flag, so the branch inside the loader is unreachable from the server. This checks the DECISION, which is
# what the C++ arithmetic was.
set -euo pipefail
E="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$E/../../.." && pwd)"
LIB=${FG_LIB:-$ROOT/runtime/goose-stage/lib}
SUB=${FG_SUBSTRATE:-/tmp/opencode/sub-661/tree/build-off/llama.cpp/ggml/src}
OUT=$(mktemp -d /tmp/align-eq.XXXXXX); trap 'rm -rf "$OUT"' EXIT

gcc-13 -O1 -c "$E/align_eq.c" -o "$OUT/align_eq.o"
g++-13 -O1 -fopenmp "$OUT/align_eq.o" "$LIB/libllama.so" -Wl,-rpath,"$LIB" \
    -L"$SUB" -lggml-base -lggml -lggml-cpu -lm -pthread -o "$OUT/align_eq"
"$OUT/align_eq"
