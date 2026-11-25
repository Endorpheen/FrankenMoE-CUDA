#!/usr/bin/env bash
set -euo pipefail
worktree=$(git rev-parse --show-toplevel)
source_dir=$(cd "$(dirname "$0")" && pwd)
out=${1:?Specify a new scratch directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
reference="$worktree/build/inkling-exp-baseline/device-attention/inkling-prefill-team/numeric.c"
mkdir "$out"
cp "$source_dir/check-bridge.py" "$out/"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$source_dir/attention-bridge.cl" \
    -o "$out/attention-bridge.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/attention-bridge.ptx" -o "$out/attention-bridge.cubin"
gcc-13 -fPIC -O1 -std=c11 -ffunction-sections -fdata-sections -Dgoose_init=i2_candidate_init \
    -Dgg_init_globals=i2_candidate_globals -w -c "$reference" -o "$out/numeric-pic.o"
objcopy --keep-global-symbol=i2_candidate_init --keep-global-symbol=i2_depth_mix_cached \
    --keep-global-symbol=inkling_row_major_i2_postconv_token \
    --keep-global-symbol=inkling_row_major_i2_rms_gain "$out/numeric-pic.o" "$out/reference.o"
gcc-13 -shared -Wl,--gc-sections,-z,defs "$out/reference.o" -lm -pthread -o "$out/reference.so"
