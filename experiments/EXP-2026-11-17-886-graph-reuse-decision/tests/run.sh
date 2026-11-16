#!/usr/bin/env bash
# Differential gate for goose/cpu/graph-reuse.goose against the native conjunction it replaces:
#   res &= (!present) || (want == have);   repeated per field, starting from true.
#
# 4000 randomised cases: 1-8 fields, present 0/1, wants 0..3, and a have that matches the want in two cases
# out of three so agreement is not reached by everything differing.
set -euo pipefail
E="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$E/../../.." && pwd)"
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
RT=${GOOSE_RUNTIME:-/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/build/slru-runtime.o}
OUT=$(mktemp -d /tmp/gr.XXXXXX); trap 'rm -rf "$OUT"' EXIT

"$GOOSE" -o "$OUT/gr.c" --header "$OUT/gr.h" "$ROOT/goose/cpu/graph-reuse.goose" >/dev/null
gcc-13 -O2 -std=c11 -w -I"$OUT" -c "$E/bridge.c" -o "$OUT/bridge.o"
gcc-13 -O2 -std=c11 -w "$E/gate.c" "$OUT/bridge.o" "$RT" -o "$OUT/gate"
"$OUT/gate"
