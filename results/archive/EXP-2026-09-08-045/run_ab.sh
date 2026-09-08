#!/bin/bash
# EXP-2026-09-08-045: one manual measurement run of one arm. Run arms strictly one at a time.
# usage: run_ab.sh A|B <label>      e.g. ./run_ab.sh A pair1-A
set -euo pipefail
cd "$(dirname "$0")"
MODE="${1:?usage: run_ab.sh A|B <label>}"
LABEL="${2:?usage: run_ab.sh A|B <label>}"
ROOT=../../..
D="$ROOT/models/qwen38/UD-IQ3_XXS"
BIN="$ROOT/build/exp045-gather-2t/exp045_gather2t"
"$BIN" \
    --ranges exp045_ranges.txt \
    --shard "$D/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf" \
    --shard "$D/Qwen3.8-Flash-Next-UD-IQ3_XXS-00002-of-00003.gguf" \
    --shard "$D/Qwen3.8-Flash-Next-UD-IQ3_XXS-00003-of-00003.gguf" \
    --arena 64 \
    --mode "$MODE" \
    --pass "$LABEL" \
    --warmup 1 \
    --passes 1 | tee "run-$LABEL.json"
