#!/bin/bash
# EXP-2026-11-15-636 build: one gate binary for one module state.
#
#   tests/build.sh canon          the canon module (4930d479), unmodified
#   tests/build.sh fixed          the canon module + patches/quant-rows2-iq-qgrid.diff
#   tests/build.sh mut <id>       the fixed module + one mutation (tests/mutate.py)
#
# The oracle is the native libggml-base of the CPU-only tree at LLAMA (override
# with LLAMA=...). Every path is derived from this script's location.
set -euo pipefail
S="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
E="$(cd "$S/.." && pwd)"
LLAMA="${LLAMA:-$E/../../tree/llama.cpp}"
GOOSE="${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}"
CC="${CC:-gcc}"
CXX="${CXX:-g++}"
OUT="$E/results"

mkdir -p "$OUT/work" "$OUT/bin" "$OUT/gen"

LABEL="${1:-fixed}"
[ "$LABEL" = "fixed" ] || [ "$LABEL" = "canon" ] || LABEL="mut-${2:-}"

# 1. materialise the module state for this label
python3 "$S/mutate.py" materialize "$LABEL" > "$OUT/work/${LABEL}.materialize.log" 2>&1
NAME="${LABEL/#mut-/mut-}"
if [ "$LABEL" = "canon" ]; then
    SRC="$E/source/quant-rows2.goose"
else
    SRC="$OUT/work/quant-rows2.${LABEL}.goose"
fi

# 2. tables built by the native init code (fixed test data, generated once)
if [ ! -f "$OUT/native-tables.c" ]; then
    python3 "$S/make-tables.py" "$LLAMA/ggml/src/ggml-quants.c" "$OUT/native-tables.c"
fi
if [ ! -f "$OUT/native-tables.o" ] || [ "$OUT/native-tables.c" -nt "$OUT/native-tables.o" ]; then
    echo "[exp636] native tables"
    "$CC" -O2 -std=c11 -w -c "$OUT/native-tables.c" -o "$OUT/native-tables.o" \
          -I "$LLAMA/ggml/include" -I "$LLAMA/ggml/src"
fi

# 3. the module: one standalone C unit, -O0 as the repository requires
echo "[exp636] goose $LABEL"
"$GOOSE" --standalone -o "$OUT/gen/$LABEL.c" --header "$OUT/gen/$LABEL.h" "$SRC" \
    > "$OUT/gen/$LABEL.goose.log" 2>&1
"$CC" -O0 -std=c11 -w -c "$OUT/gen/$LABEL.c" -o "$OUT/gen/$LABEL.o"

# 4. the fixture, -O1 (test infrastructure, not the mutated port)
echo "[exp636] fixture $LABEL"
"$CXX" -O1 -std=c++17 -w "$S/fixture.cpp" \
    "$OUT/native-tables.o" "$OUT/gen/$LABEL.o" \
    -o "$OUT/bin/$LABEL" \
    -I "$LLAMA/ggml/include" -I "$LLAMA/ggml/src" \
    -L "$LLAMA/build/bin" -lggml -lggml-base -lm \
    -Wl,-rpath,"$LLAMA/build/bin"

echo "[exp636] built $OUT/bin/$LABEL from $(basename "$SRC")"
