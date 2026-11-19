#!/usr/bin/env bash
# Permanent gate: web_origin_is_localhost against the native origin decision.
#
# Oracle: the native body of origin_is_localhost from tools/server/server-http.cpp, transcribed in the
# fixture over the same parser production uses (common_http_parse_url).
# Port: web_origin_is_localhost of goose/server/prerouting.goose.
#
# Tier: fast - no model, no GPU, seconds.
#
# Usage: bash tests/server/origin-policy/run.sh
set -euo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
G=${FG_GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
SRC=$ROOT/goose/server/prerouting.goose
FIX=$D/fixture.cpp
OUT=$D/build
LOG=$OUT/run.log
OPT=${FG_OPT:--O1}
mkdir -p "$OUT"

GVER=$("$G" --version 2>/dev/null | head -1 | tr ' ' '_' || true)
KEY=$(sha256sum "$SRC" | cut -d' ' -f1)"-$OPT-${GVER:-unknown}"

{
    printf '$ %s %s\n' "$G" "--standalone $SRC"
    if [ ! -f "$OUT/gen.o" ] || [ "$(cat "$OUT/.key" 2>/dev/null)" != "$KEY" ]; then
        "$G" --standalone -o "$OUT/runtime-gen.c" --header "$OUT/runtime-gen.h" "$SRC"
        gcc-13 $OPT -std=c11 -c "$OUT/runtime-gen.c" -o "$OUT/gen.o"
        printf '%s' "$KEY" > "$OUT/.key"
    else
        echo "gen.o reused: source, optimisation and compiler unchanged"
    fi
    printf '$ %s\n' "g++-13 -O2 -std=c++17 fixture.cpp"
    if [ ! -f "$OUT/fixture.o" ] || [ "$FIX" -nt "$OUT/fixture.o" ]; then
        g++-13 -O2 -std=c++17 -I"$OUT" \
            -I"$ROOT/vendor/flint/include" -I"$ROOT/vendor/flint/src" \
            -I"$ROOT/vendor/flint/common" -I"$ROOT/vendor/flint/vendor" \
            -I"$ROOT/vendor/flint/ggml/include" \
            -c "$FIX" -o "$OUT/fixture.o"
    else
        echo "fixture.o reused: fixture unchanged"
    fi
    printf '$ %s\n' "g++-13 fixture.o gen.o -o gate"
    g++-13 "$OUT/fixture.o" "$OUT/gen.o" -o "$OUT/gate"
    printf '$ %s\n' "$OUT/gate"
    "$OUT/gate"
} 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
printf '[exit %d]\n' "$rc" | tee -a "$LOG"
exit "$rc"
