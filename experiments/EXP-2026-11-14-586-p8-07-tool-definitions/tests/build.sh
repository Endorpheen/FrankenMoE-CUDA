#!/bin/bash
# P8-07 S2 gate: tool names, flags, argument validation, result JSON.
#
#   goose --standalone (tools-pure + jsonparse) -> C -> gcc -O0
#   tools_fixture.cpp -> g++ -> 12 cases / 92 values, no filesystem, no model
set -euo pipefail

SUB=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
OPT=${OPT:--O0}
OUT=$SUB/build

mkdir -p "$OUT"
SHA=$(sha256sum "$SUB/source/tools-pure.goose" "$SUB/source/jsonparse.goose" | sha256sum | awk '{print $1}')

if [ ! -f "$OUT/g.o" ] || [ "$(cat "$OUT/.gen.sha" 2>/dev/null || true)" != "$SHA" ]; then
    "$GOOSE" --standalone -o "$OUT/g.c" --header "$OUT/g.h" "$SUB/source/tools-pure.goose"
    "$CC" "$OPT" -std=c11 -w -c "$OUT/g.c" -o "$OUT/g.o"
    printf '%s\n' "$SHA" > "$OUT/.gen.sha"
fi

"$CXX" -O2 -std=c++17 -I"$OUT" "$SUB/tests/tools_fixture.cpp" "$OUT/g.o" -o "$OUT/tools_fixture"
"$OUT/tools_fixture"
