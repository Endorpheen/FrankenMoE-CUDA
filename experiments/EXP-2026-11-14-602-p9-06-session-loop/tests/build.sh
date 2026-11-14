#!/bin/bash
# P9-06 S5 gate: the session loop's budget, cancellation and context decisions.
#
#   goose --standalone on session-loop.goose -> C -> gcc -O1
#   session_loop_fixture.cpp -> g++ -> 31 cases + 42 pinned values
#
# The experiment root is derived from this script's location; nothing here
# depends on the caller's working directory.
set -euo pipefail

SUB=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
OUT=$SUB/build

mkdir -p "$OUT"
SHA=$(sha256sum "$SUB/source/session-loop.goose" | awk '{print $1}')

if [ ! -f "$OUT/g.o" ] || [ "$(cat "$OUT/.gen.sha" 2>/dev/null || true)" != "$SHA" ]; then
    "$GOOSE" --standalone -o "$OUT/g.c" --header "$OUT/g.h" \
        "$SUB/source/session-loop.goose"
    "$CC" -O1 -std=c11 -w -c "$OUT/g.c" -o "$OUT/g.o"
    printf '%s\n' "$SHA" > "$OUT/.gen.sha"
fi

"$CXX" -O2 -std=c++20 -I"$OUT" "$SUB/tests/session_loop_fixture.cpp" "$OUT/g.o" \
    -o "$OUT/session_loop_gate"
"$OUT/session_loop_gate"
