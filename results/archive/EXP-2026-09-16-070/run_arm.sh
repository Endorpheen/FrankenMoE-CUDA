#!/bin/bash
# EXP-2026-09-16-070: one manual measurement of one arm. Run arms strictly one at a time.
# usage: run_arm.sh A|B|C <label> [--slots N] [--max-range-bytes N]
set -euo pipefail
cd "$(dirname "$0")"
MODE="${1:?usage: run_arm.sh A|B|C <label>}"
LABEL="${2:?usage: run_arm.sh A|B|C <label>}"
shift 2
ROOT=../../..
D="$ROOT/models/qwen38/UD-IQ3_XXS"
BIN="$ROOT/build/exp070-transfer3/exp070_transfer3"
"$BIN" \
    --ranges exp070_ranges.txt \
    --shard "$D/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf" \
    --shard "$D/Qwen3.8-Flash-Next-UD-IQ3_XXS-00002-of-00003.gguf" \
    --shard "$D/Qwen3.8-Flash-Next-UD-IQ3_XXS-00003-of-00003.gguf" \
    --arena 64 \
    --mode "$MODE" \
    --slots 3 \
    --two-thread 1 \
    --pass "$LABEL" \
    --warmup 1 \
    --passes 4 "$@" | tee "run-$LABEL.json"
