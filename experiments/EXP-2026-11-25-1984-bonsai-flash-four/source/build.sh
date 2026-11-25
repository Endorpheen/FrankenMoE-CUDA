#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
prev="$src/../../EXP-2026-11-25-1983-bonsai-flash-reuse/source"
out=${1:?Specify a new scratch directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
bash "$prev/build.sh" "$out"
cat "$out/flash-reuse.cl" "$src/flash-four.cl" > "$out/flash-four.cl"
python3 - "$out/flash-four.cl" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
body = path.read_text().split('__kernel void bonsai_flash_f32_merge(', 1)[1].split('\n}\n',1)[0]
body = body.replace('work_group()/12', 'work_group()/24').replace('work_group()%12', 'work_group()%24')
body = body.replace('(pair/4)*2', '(pair/4)*4').replace('FLASH_ROWS', 'FLASH_FOUR_ROWS')
with path.open('a') as out:
    out.write('\n__kernel void bonsai_flash_four_merge(' + body + '\n}\n')
PY
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/flash-four.cl" -o "$out/flash-four.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/flash-four.ptx" -o "$out/flash-four.cubin" 2> "$out/flash-four.ptxas.log"
patch -s "$out/flash-reuse-backend.c" -o "$out/flash-four-backend.c" < "$src/backend.c.patch"
"${CC:-gcc-13}" -O2 -Wall -Wextra -Werror -Wl,--gc-sections -I"$out" -I"$cuda/include" \
    "$out/flash-four-backend.c" "$out/bonsai.o" \
    "$src/../../EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -l:libcuda.so.1 -lm -pthread -o "$out/bonsai-flash-four"
