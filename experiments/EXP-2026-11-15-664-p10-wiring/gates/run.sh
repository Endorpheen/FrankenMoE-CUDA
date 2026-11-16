#!/usr/bin/env bash
# P10 wiring gate: build the hosts, then run every wiring gate.
#
#   bash gates/run.sh                 # -O1 hosts, all four wirings
#   bash gates/run.sh --mutate        # -O0 hosts (the mutation runner rewrites
#                                     # hosts itself; this is for a manual check)
#
# No model, no GPU, no CUDA, no engine build: the gates drive scripts, cmake -P,
# CTest-free bash, gguf/numpy and the Goose toolchain only. The two packaging
# runs inside w3_pkg.py do build the composed 66-module unit twice; pass
# --skip-e2e through w3_pkg.py to skip that arm.
set -euo pipefail

EXP="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ROOT="$(cd "${EXP}/../.." && pwd)"
cd "$EXP"

GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
bash "${ROOT}/tools/goose-hosts/build.sh" ${1:-}

mkdir -p results
for gate in w1_art w2_env w3_pkg w3_release w4_fx; do
    echo "== ${gate}"
    python3 "gates/${gate}.py" "${2:-}" | tee "results/${gate}.log"
done
