#!/bin/bash
# P9-07 gate: metrics/traces text formats and deterministic workload validation.
#
#   goose --standalone on source/metrics-traces.goose -> C -> gcc -O1
#   tests/metrics_traces_fixture.cpp + the carrier's own sinks and DeterministicWorkload
#     -> g++ -> S1 field cases, S2 trace rows, S3 workload scenarios
#
# The experiment root is derived from this script's location; the carrier path has a
# default and an override, nothing else depends on the caller's working directory.
set -euo pipefail

SUB=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
CARRIER=${CARRIER:-/home/end0/AI/FrankenMoE-CUDA}
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
OUT=${FG_OUT:-$SUB/build}

mkdir -p "$OUT"
SHA=$(sha256sum "$SUB/source/metrics-traces.goose" | awk '{print $1}')

if [ ! -f "$OUT/g.o" ] || [ "$(cat "$OUT/.gen.sha" 2>/dev/null || true)" != "$SHA" ]; then
    "$GOOSE" --standalone -o "$OUT/g.c" --header "$OUT/g.h" \
        "$SUB/source/metrics-traces.goose"
    "$CC" -O1 -std=c11 -w -c "$OUT/g.c" -o "$OUT/g.o"
    printf '%s\n' "$SHA" > "$OUT/.gen.sha"
fi

"$CXX" -O2 -std=c++20 -I"$OUT" -I"$CARRIER/runtime/core/include" -I"$CARRIER/runtime/core/src/moe" \
    "$SUB/tests/metrics_traces_fixture.cpp" \
    "$CARRIER/runtime/core/src/metrics/decode_trace_sink.cpp" \
    "$CARRIER/runtime/core/src/metrics/route_trace_sink.cpp" \
    "$CARRIER/runtime/core/src/metrics/csv_metrics_sink.cpp" \
    "$CARRIER/runtime/core/src/moe/deterministic_workload.cpp" \
    "$OUT/g.o" -o "$OUT/metrics_traces_gate"
"$OUT/metrics_traces_gate" "$OUT"
