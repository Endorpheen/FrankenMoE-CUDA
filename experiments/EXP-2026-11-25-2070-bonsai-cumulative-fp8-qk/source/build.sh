#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
out=${1:?Specify a fresh candidate directory}
base=${2:?Specify EXP-2067 build directory}
donor=${3:?Specify EXP-2056 build directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir "$out"
python3 "$src/merge-ptx.py" "$base/pipeline.ptx" "$donor/pipeline.ptx" "$out/pipeline.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/pipeline.ptx" -o "$out/pipeline.cubin" 2>"$out/ptxas.txt"
python3 "$src/verify-code.py" "$base/pipeline.cubin" "$out/pipeline.cubin" > "$out/device-code.json"
bash "$src/build-http.sh" "$out" "$base"
