#!/bin/bash
# P9-06 S2 gate: the CLI line protocol and the accepted JSON parser.
#
#   goose --standalone on the two-module pair -> gen.c -> gcc -O1
#   protocol_fixture.cpp -> g++ -> 17 cases against the carrier's format strings
set -euo pipefail

SUB=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
OPT=${OPT:--O1}
OUT=$SUB/build

mkdir -p "$OUT"
SHA=$(sha256sum "$SUB/source/cli_protocol.goose" "$SUB/source/jsonparse.goose" | sha256sum | awk '{print $1}')

if [ ! -f "$OUT/g.o" ] || [ "$(cat "$OUT/.gen.sha" 2>/dev/null || true)" != "$SHA" ]; then
    "$GOOSE" --standalone -o "$OUT/g.c" --header "$OUT/g.h" \
        "$SUB/source/cli_protocol.goose"
    "$CC" "$OPT" -std=c11 -c "$OUT/g.c" -o "$OUT/g.o"
    printf '%s\n' "$SHA" > "$OUT/.gen.sha"
fi

"$CXX" -O2 -std=c++17 -I"$OUT" "$SUB/tests/protocol_fixture.cpp" "$OUT/g.o" \
    -o "$OUT/protocol_fixture"
"$OUT/protocol_fixture"
