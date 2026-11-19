#!/usr/bin/env bash
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
BIN=${1:-$R/builds/flint-acc/bin}
gcc -O2 -o /tmp/lb_probe "$R/experiments/EXP-2026-11-19-1408-bench-doors/probe.c" -ldl || exit 1
LD_LIBRARY_PATH="$BIN" /tmp/lb_probe "$BIN/libllama.so.0"
