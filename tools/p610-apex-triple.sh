#!/usr/bin/env bash
# Three paired runs on the acceptance profile through the rule-30 entry point.
# Each pair is one ON run and one OFF run of the same binary, so drift cancels between them.
set -uo pipefail
R=/home/end0/AI/FrankenMoE-CUDA
PAIRS=${1:-3}
for p in $(seq 1 "$PAIRS"); do
  GGML_GOOSE_SAMPLER_CHAIN=1 APEX_SERVER_DIR="$R/builds/flint-acc/bin" \
      bash "$R/tools/apex-test.sh" "pair${p}-on" 512 4096 4096 4992 2>&1 | grep -E "decode \[|sha"
  GGML_GOOSE_SAMPLER_CHAIN=0 APEX_SERVER_DIR="$R/builds/flint-acc/bin" \
      bash "$R/tools/apex-test.sh" "pair${p}-off" 512 4096 4096 4992 2>&1 | grep -E "decode \[|sha"
done
