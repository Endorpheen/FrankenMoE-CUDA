#!/usr/bin/env bash
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
BIN=${1:-$R/builds/flint-acc/bin}
gcc -O2 -o /tmp/fp_diff "$R/experiments/EXP-2026-11-19-1407-fit-line-compared/diff.c" -ldl || exit 1
out=$(LD_LIBRARY_PATH="$BIN" /tmp/fp_diff "$BIN/libllama.so.0")
echo "$out" | tail -1
echo "$out" | grep -q "disagreements=0"
