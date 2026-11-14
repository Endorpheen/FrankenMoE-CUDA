#!/bin/bash
# P9-06 S4 gate: serialized PEG arena load and build_parse_params.
#
#   goose --standalone on parse_params + jsonparse -> C -> gcc -O0
#   s4_fixture.cpp -> g++ -> 10 cases / 58 checks against the native shape
set -euo pipefail

SUB=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
OUT=$SUB/build

mkdir -p "$OUT"
SHA=$(sha256sum "$SUB/source/parse_params.goose" "$SUB/source/jsonparse.goose" | sha256sum | awk '{print $1}')

if [ ! -f "$OUT/g.o" ] || [ "$(cat "$OUT/.gen.sha" 2>/dev/null || true)" != "$SHA" ]; then
    "$GOOSE" --standalone -o "$OUT/g.c" --header "$OUT/g.h" \
        "$SUB/source/parse_params.goose"
    "$CC" -O0 -std=c11 -w -c "$OUT/g.c" -o "$OUT/g.o"
    printf '%s\n' "$SHA" > "$OUT/.gen.sha"
fi

"$CXX" -O2 -std=c++17 -I"$OUT" "$SUB/tests/s4_fixture.cpp" "$OUT/g.o" \
    -o "$OUT/s4_fixture"
"$OUT/s4_fixture"
