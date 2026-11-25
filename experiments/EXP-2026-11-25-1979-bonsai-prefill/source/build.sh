#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
trunk="$src/../../EXP-2026-11-25-1976-bonsai-trunk/source"
out=${1:?Specify a new output directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
bash "$trunk/build.sh" "$out"
patch -s "$trunk/backend.c" -o "$out/backend.c" < "$src/backend.c.patch"
"${CC:-gcc-13}" -O2 -Wall -Wextra -Werror -Wl,--gc-sections -I"$out" -I"$cuda/include" \
    "$out/backend.c" "$out/bonsai.o" \
    "$src/../../EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -l:libcuda.so.1 -lm -pthread -o "$out/bonsai-batch-infer"
python3 "$src/compose-device.py" > "$out/prefill.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/prefill.cl" -o "$out/prefill.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/prefill.ptx" -o "$out/prefill.cubin" 2> "$out/ptxas.log"
