#!/usr/bin/env bash
# Gate build for EXP-2026-11-19-1491. Derives every path from its own location.
# Usage: bash tests/build.sh
set -euo pipefail
E="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
G=${FG_GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
OPT=${FG_OPT:--O1}
OUT=$E/build
SRC=$E/source/runtime.goose
FIX=$E/tests/fixture.cpp
mkdir -p "$OUT"

GVER=$("$G" --version 2>/dev/null | head -1 | tr ' ' '_' || true)
KEY=$(sha256sum "$SRC" | cut -d' ' -f1)"-$OPT-${GVER:-unknown}"
if [ ! -f "$OUT/gen.o" ] || [ "$(cat "$OUT/.key" 2>/dev/null)" != "$KEY" ]; then
    "$G" --standalone -o "$E/source/runtime-gen.c" --header "$E/source/runtime-gen.h" "$SRC" >/dev/null
    gcc-13 $OPT -std=c11 -c "$E/source/runtime-gen.c" -o "$OUT/gen.o"
    printf '%s' "$KEY" > "$OUT/.key"
fi
if [ ! -f "$OUT/fixture.o" ] || [ "$FIX" -nt "$OUT/fixture.o" ]; then
    g++-13 -O2 -std=c++17 -I"$E/source" \
        -I"$E/../../vendor/flint/include" -I"$E/../../vendor/flint/src" \
        -I"$E/../../vendor/flint/ggml/include" \
        -c "$FIX" -o "$OUT/fixture.o"
fi
g++-13 "$OUT/fixture.o" "$OUT/gen.o" -o "$OUT/gate"
set +e
"$OUT/gate"
RC=$?
set -e
echo "gate exit=$RC"
exit $RC
