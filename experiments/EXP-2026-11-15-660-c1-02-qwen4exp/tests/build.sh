#!/usr/bin/env bash
# C1-02 gate build: one slice per Goose module, one fixture per slice.
#
#   bash tests/build.sh q4m            # -O1, cached
#   bash tests/build.sh q4t
#   bash tests/build.sh q4e
#   bash tests/build.sh q4m mutate     # -O0, always regenerates
#
# No absolute paths are stored: the root is derived from this script's
# location, GOOSE_BIN/CC/CXX/OUT_DIR override the tools.
set -euo pipefail

SLICE=${1:-q4m}
MODE=${2:-final}
E=$(cd "$(dirname "$0")/.." && pwd)
G=${GOOSE_BIN:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}

case "$SLICE" in
    q4m) SRC=$E/source/q4m_metadata.goose;    FIX=$E/tests/fixture_q4m.cpp ;;
    q4t) SRC=$E/source/q4t_tensor_map.goose;  FIX=$E/tests/fixture_q4t.cpp ;;
    q4e) SRC=$E/source/q4e_expert_layout.goose; FIX=$E/tests/fixture_q4e.cpp ;;
    join) SRC=$E/source/join.goose;          FIX=$E/tests/join_check.cpp ;;
    *) echo "unknown slice: $SLICE" >&2; exit 2 ;;
esac

if [ "$MODE" = mutate ]; then OPT=-O0; else OPT=-O1; fi
OUT=${OUT_DIR:-$E/build/$SLICE}
BIN=$OUT/gate
mkdir -p "$OUT"

# The cache key covers the generated source, every imported module, the
# optimisation level and the compiler version.
KEY=$( (cat "$E"/source/*.goose "$E"/source/base/*.goose 2>/dev/null | sha256sum; echo "$OPT"; "$CC" --version | head -1) \
       | sha256sum | cut -d' ' -f1)

if [ "$MODE" = mutate ] || [ ! -f "$OUT/gen.o" ] || [ "$(cat "$OUT/.key" 2>/dev/null || true)" != "$KEY" ]; then
    echo "[c1-02:$SLICE] goose --standalone $OPT"
    "$G" --standalone -o "$OUT/gen.c" --header "$OUT/gen.h" "$SRC" >/dev/null
    echo "[c1-02:$SLICE] cc $OPT"
    "$CC" "$OPT" -std=c11 -c "$OUT/gen.c" -o "$OUT/gen.o"
    printf '%s' "$KEY" > "$OUT/.key"
else
    echo "[c1-02:$SLICE] gen.o cached"
fi

echo "[c1-02:$SLICE] cxx $OPT"
"$CXX" "$OPT" -std=c++17 -I"$OUT" -c "$FIX" -o "$OUT/fixture.o"

echo "[c1-02:$SLICE] link"
"$CXX" "$OUT/fixture.o" "$OUT/gen.o" -o "$BIN"

echo "[c1-02:$SLICE] run"
"$BIN"
