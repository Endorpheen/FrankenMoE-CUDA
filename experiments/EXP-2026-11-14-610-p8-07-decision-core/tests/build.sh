#!/usr/bin/env bash
# Differential gate for P8-07 S4 (decision core), no server and no model: the
# fixture carries a transcription of tools/server/server-decision.cpp on
# nlohmann::ordered_json, the port is the Goose module, both emit the same
# lines. Usage: bash tests/build.sh
# FG_GOOSE, FG_CXX, FG_VENDOR, FG_OUT override the defaults.
set -euo pipefail
E="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
G=${FG_GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CXX=${FG_CXX:-g++-13}
V=${FG_VENDOR:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/vendor}
OUT=${FG_OUT:-$E/build}
mkdir -p "$OUT"

"$G" --standalone -o "$OUT/g.c" --header "$OUT/g.h" "$E/source/decision-core.goose"
gcc-13 -O0 -std=c11 -w -c "$OUT/g.c" -o "$OUT/g.o"
"$CXX" -O1 -std=c++17 -I"$OUT" -I"$V" "$E/tests/fixture.cpp" "$OUT/g.o" -o "$OUT/gate" -lm
"$OUT/gate"
