#!/usr/bin/env bash
set -euo pipefail

out=${1:?Missing CUDA device include directory}
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler=${GOOSE_CUDA_DEVICE_BIN:-${GOOSE_CUDA_DRIVER_BIN:?Set GOOSE_CUDA_DRIVER_BIN to KeelWing dev.5 or set GOOSE_CUDA_DEVICE_BIN}}
"$compiler" --emit-cuda-device "$out/typed-memory.h" "$root/goose/cuda/typed-memory.goose"
python3 "$root/tools/emit-gooseinfer-cuda-memory-bridge.py" \
    "$root/integration/goose-standalone/cuda-typed-memory-bridge.json" "$out/typed-memory-bridge.h"
