#!/bin/bash
# Build entry points from one Goose unit and the C system adapters.
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT=${1:-$ROOT/build/goose-standalone}
CC=${CC:-gcc-13}
UNIT=$OUT/unit
mkdir -p "$UNIT"

bash "$ROOT/tools/build-main-runtime.sh" "$UNIT"
printf '#include "main.h"\n' > "$UNIT/goose-unit.h"
COMMON_FLAGS=(-O1 -D_GNU_SOURCE -std=c11 -ffunction-sections -fdata-sections -I "$UNIT")
"$CC" "${COMMON_FLAGS[@]}" -I "$ROOT/integration/m1/host" \
    -c "$ROOT/integration/m1/host/goose_host.c" -o "$OUT/host.o"
"$CC" "${COMMON_FLAGS[@]}" -c "$ROOT/integration/goose-standalone/libc-forwards.c" \
    -o "$OUT/libc-forwards.o"

entries=(
    "$ROOT/integration/goose-standalone/cli/tokenize-main.c"
    "$ROOT/integration/goose-standalone/cli/serve-main.c"
)
for source in "${entries[@]}"; do
    entry=${source##*/}
    entry=${entry%-main.c}
    "$CC" "${COMMON_FLAGS[@]}" -c "$source" -o "$OUT/$entry.o"
    "$CC" -Wl,--gc-sections "$OUT/$entry.o" "$UNIT/main.o" "$OUT/host.o" \
        "$OUT/libc-forwards.o" -lm -pthread -o "$OUT/gooseinfer-$entry"
    if readelf -d "$OUT/gooseinfer-$entry" | grep -Eq 'NEEDED.*(libstdc\+\+|libllama|libggml)'; then
        printf 'Unexpected native runtime dependency in gooseinfer-%s\n' "$entry" >&2
        exit 1
    fi
    ln -sfn "gooseinfer-$entry" "$OUT/frankenmoe-$entry"
    printf 'Built %s/gooseinfer-%s\n' "$OUT" "$entry"
done
