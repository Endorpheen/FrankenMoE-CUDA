#!/bin/bash
# EXP-2026-11-15-642 build: one gate binary for one module state.
#
#   tests/build.sh before        canon (1de2effc) + the accepted EXP641 qgrid patch
#   tests/build.sh fixed         the same + patches/iq1m-split.diff
#   tests/build.sh mut <id>      fixed + one mutation (tests/mutate.py)
#
# The oracle is the native libggml-base of the CPU-only tree at LLAMA.
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
case "$LABEL" in
    before|fixed) ;;
    mut) LABEL="mut-${2:?mutation id}" ;;
    *) echo "usage: $0 [before|fixed|mut <id>]" >&2; exit 2 ;;
esac

python3 "$S/mutate.py" materialize "$LABEL" > "$OUT/work/${LABEL}.materialize.log" 2>&1
SRC="$OUT/work/quant-rows2.${LABEL}.goose"

if [ ! -f "$OUT/native-tables.c" ]; then
    python3 "$S/make-tables.py" "$LLAMA/ggml/src/ggml-quants.c" "$OUT/native-tables.c"
fi
if [ ! -f "$OUT/native-tables.o" ] || [ "$OUT/native-tables.c" -nt "$OUT/native-tables.o" ]; then
    echo "[exp642] native tables"
    "$CC" -O2 -std=c11 -w -c "$OUT/native-tables.c" -o "$OUT/native-tables.o" \
          -I "$LLAMA/ggml/include" -I "$LLAMA/ggml/src"
fi

echo "[exp642] goose $LABEL"
"$GOOSE" --standalone -o "$OUT/gen/$LABEL.c" --header "$OUT/gen/$LABEL.h" "$SRC" \
    > "$OUT/gen/$LABEL.goose.log" 2>&1
"$CC" -O0 -std=c11 -w -c "$OUT/gen/$LABEL.c" -o "$OUT/gen/$LABEL.o"

echo "[exp642] fixture $LABEL"
"$CXX" -O1 -std=c++17 -w "$S/fixture.cpp" \
    "$OUT/native-tables.o" "$OUT/gen/$LABEL.o" \
    -o "$OUT/bin/$LABEL" \
    -I "$LLAMA/ggml/include" -I "$LLAMA/ggml/src" \
    -L "$LLAMA/build/bin" -lggml -lggml-base -lm \
    -Wl,-rpath,"$LLAMA/build/bin"

echo "[exp642] built $OUT/bin/$LABEL"
