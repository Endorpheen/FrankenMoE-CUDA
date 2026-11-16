#!/usr/bin/env bash
# Run the S1.5 tokenizer gate: three decisions, 182 synthetic cases, no model.
#
# It was not in any build, so its expectation silently drifted from reality: the OFF arm once compared a
# working string_strip against "return the input" on the premise that the native body had been deleted.
# Running it from a script with the include roots spelled out is what keeps that from recurring.
#
# Usage: bash run.sh [--off|--on]
#   --off  default; the native body is what the library answers
#   --on   build with -DFG_GOOSE_TOKENIZER so the Goose bridge answers instead
set -euo pipefail
E="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$E/../../../.." && pwd)"
FL=${FG_FLINT:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
CXX=${FG_CXX:-g++-13}
MODE=${1:---off}
DEF=""
EXTRA=""
if [ "$MODE" = "--on" ]; then
    DEF="-DFG_GOOSE_TOKENIZER"
    # the ON arm needs the bridge implementation, and with it the composed Goose unit
    # the unit's validation hook lives in graph-bindings.o, which also pulls ggml - so the ON arm links it
    EXTRA="$ROOT/integration/m1/common/fg-goose-tokenizer-bridge.cpp $ROOT/builds/goose-standalone/main-gc.o $ROOT/builds/goose-standalone/graph-bindings.o $ROOT/builds/goose-standalone/host.o"
fi

OUT=$(mktemp -d /tmp/s15-gate.XXXXXX)
trap 'rm -rf "$OUT"' EXIT

# the bridge header lives with the M1 integration, not with flint
INC="-I$FL/include -I$FL/ggml/include -I$FL/common -I$FL/src -I$ROOT/integration/m1/common -I$ROOT/integration/m1/host -I$ROOT/builds/goose-standalone/inc"
B="$FL/build-cuda/bin"

"$CXX" -O1 -std=c++17 -w $DEF $INC \
    "$E/fg_goose_tokenizer_smoke.cpp" "$E"/native-*.cpp $EXTRA \
    -L"$B" -lllama-common -lllama -lggml -lggml-base -Wl,--gc-sections -lm -pthread -Wl,-rpath,"$B" \
    -o "$OUT/gate"

out=$("$OUT/gate" 2>&1 | tail -1)
printf '%s\n' "$out"
case "$out" in
    *failures=0*) exit 0 ;;
    *) echo "S1.5 tokenizer gate FAILED ($MODE)" >&2; exit 1 ;;
esac
