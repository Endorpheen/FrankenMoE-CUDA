#!/bin/bash
# Build and run the two tool-builder gates.
#
#   structural: native standard_* builders vs the port, compared through the
#               textual arena dump plus a rebuild identity check (13 cases);
#   parse:      native chat parser driven by the built grammar (9 cases).
#
# Native side: libllama-common from the flint build. No weights, no model.
set -euo pipefail

SUB=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
L=${FG_LLAMA:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
BIN=${FG_BIN:-$L/build-cuda/bin}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
OUT=$SUB/build

mkdir -p "$OUT"
SHA=$(sha256sum "$SUB/source/runtime.goose" | awk '{print $1}')

if [ ! -f "$OUT/gen.o" ] || [ "$(cat "$OUT/.gen.sha" 2>/dev/null || true)" != "$SHA" ]; then
    "$GOOSE" --standalone -o "$OUT/runtime-gen.c" --header "$OUT/runtime-gen.h" \
        "$SUB/source/runtime.goose"
    "$CC" -O0 -std=c11 -w -c "$OUT/runtime-gen.c" -o "$OUT/gen.o"
    printf '%s\n' "$SHA" > "$OUT/.gen.sha"
fi

for gate in fixture parse_fixture; do
    "$CXX" -O1 -std=c++17 -I"$OUT" -I"$SUB/tests" \
        -I"$L/common" -I"$L/include" -I"$L/src" -I"$L/ggml/include" \
        -c "$SUB/tests/$gate.cpp" -o "$OUT/$gate.o"
    "$CXX" "$OUT/$gate.o" "$OUT/gen.o" -L"$BIN" -lllama-common \
        -Wl,-rpath,"$BIN" -o "$OUT/$gate"
done

"$OUT/fixture"
"$OUT/parse_fixture"
