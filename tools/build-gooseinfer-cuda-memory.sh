#!/usr/bin/env bash
set -euo pipefail

out=${1:?Missing CUDA device include directory}
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler=${GOOSE_CUDA_DEVICE_BIN:-${GOOSE_CUDA_DRIVER_BIN:?Set GOOSE_CUDA_DRIVER_BIN to KeelWing dev.5 or set GOOSE_CUDA_DEVICE_BIN}}
"$compiler" --emit-cuda-device "$out/typed-memory.h" "$root/goose/cuda/typed-memory.goose"
"$compiler" --emit-cuda-device "$out/checked-device-arithmetic.h" "$root/goose/cuda/checked-device-arithmetic.goose"
"${GOOSE_CUDA_DRIVER_BIN:-$compiler}" --standalone --emit-provenance "$out/cuda-device-scalar.c" "$root/goose/cuda/cuda-device-scalar.goose"
python3 "$root/integration/goose-standalone/universal/bonsai-frozen-cuda/frankenmoe-fragment.py" "$out" cuda-device-scalar
python3 "$root/tools/emit-gooseinfer-cuda-memory-bridge.py" \
    "$root/integration/goose-standalone/cuda-typed-memory-bridge.json" "$out/typed-memory-bridge.h"
python3 "$root/tools/emit-gooseinfer-cuda-compat.py" "$out"
python3 "$root/tools/emit-gooseinfer-cuda-modules.py" "$root/integration/goose-standalone/cuda-device-modules.json" "$out"
