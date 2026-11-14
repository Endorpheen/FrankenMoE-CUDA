#!/bin/bash
# Build the composed server unit, the fake MCP server and the gate, then run it.
# The unit is generated: build/ is not part of the experiment (the composed C is
# larger than the 1 MiB generated-file limit). FG_OPT is the C optimisation
# level: -O1 for the gate, -O0 for the mutation builds.
set -euo pipefail
S=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
L=${FG_LLAMA:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
REPO=${FG_REPO:-/home/end0/AI/FrankenMoE-CUDA}
OUT=$S/build
mkdir -p "$OUT"

# one unit: the accepted server modules plus the integration glue
python3 "$REPO/tools/compose-goose.py" "$OUT/unit.goose" \
    "$S/source/mcp-core.goose" \
    "$S/source/mcp-stdio.goose" \
    "$S/source/decision-core.goose" \
    "$S/source/server-schema.goose" \
    "$S/source/tools-pure.goose" \
    "$S/source/mcp-integration.goose" >/dev/null

"$GOOSE" --standalone -o "$OUT/g.c" --header "$OUT/g.h" "$OUT/unit.goose" 2>/dev/null
gcc-13 ${FG_OPT:--O1} -std=c11 -w -c "$OUT/g.c" -o "$OUT/g.o"

g++-13 ${FG_OPT:--O1} -std=c++17 -I"$L/vendor" "$S/tests/fake_server.cpp" -o "$OUT/fake_server_619"
g++-13 ${FG_OPT:--O1} -std=c++17 -I"$OUT" -I"$L/vendor" "$S/tests/fixture.cpp" "$OUT/g.o" -o "$OUT/gate"

PATH="$OUT:$PATH" "$OUT/gate"
