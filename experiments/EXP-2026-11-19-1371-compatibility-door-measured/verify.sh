#!/usr/bin/env bash
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
BIN=${1:-$R/builds/flint-acc/bin}
gcc -O2 -o /tmp/cmp_diff "$R/experiments/EXP-2026-11-19-1371-compatibility-door-measured/diff.c" -ldl || exit 1
out=$(LD_LIBRARY_PATH="$BIN" /tmp/cmp_diff "$BIN/libllama.so.0")
echo "$out" | tail -1
echo "$out" | grep -q "disagreements=0"
