#!/usr/bin/env bash
# Build the SLRU policy module into an object that can be linked into libllama.
#
# Three inputs: the module's generated C, the Goose runtime (emitted once, deterministic for a pinned
# compiler), and the host shim. The runtime is needed because the emitted C calls gs_rt_init and the
# three failure hooks; unlike the older fgNNN bridges, this module is not post-processed to strip them.
#
# Usage: bash build.sh [outdir]
set -euo pipefail
E="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$E/../.." && pwd)"
G=${FG_GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${FG_CC:-gcc-13}
OUT=${1:-$E/build}

mkdir -p "$OUT"
"$G" -o "$OUT/slru-policy.c" --header "$OUT/slru-policy.h" "$ROOT/goose/cpu/goose-moe-cache-slru.goose" >/dev/null
"$G" --emit-runtime "$OUT/slru-runtime.c" >/dev/null

"$CC" -O2 -fPIC -std=c11 -w -c "$OUT/slru-policy.c" -o "$OUT/slru-policy.o"
"$CC" -O2 -fPIC -std=c11 -w -c "$OUT/slru-runtime.c" -o "$OUT/slru-runtime.o"
"$CC" -O2 -fPIC -std=c11 -w -c "$ROOT/integration/goose-standalone/slru-runtime-shim.c" -o "$OUT/slru-shim.o"

echo "slru-policy.o  $(stat -c%s "$OUT/slru-policy.o") B"
echo "slru-runtime.o $(stat -c%s "$OUT/slru-runtime.o") B"
echo "slru-shim.o    $(stat -c%s "$OUT/slru-shim.o") B"
echo "exports:       $($CC -print-file-name=  >/dev/null; nm "$OUT/slru-policy.o" | awk '$2=="T"{print $3}' | tr '\n' ' ')"
