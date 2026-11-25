#!/usr/bin/env bash
set -euo pipefail

out=${1:?Specify the component output directory}
base=${2:?Specify EXP-2086 build directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
entry=bonsai_decode_direct_q4_mma
mkdir -p "$out"

python3 - "$base/component.cl" "$out/component.cl" "$entry" <<'PY'
from pathlib import Path
import sys

reference, output = map(Path, sys.argv[1:3])
name = 'bonsai_flash_direct_kv'
text = reference.read_text()
start = text.index(f'__kernel void {name}(')
end = text.index('\n}\n', start) + len('\n}\n')
kernel = text[start:end]
for old, new in (
    (name, sys.argv[3]),
    ('''uint queries, uint key_count, uint first_visible,
                                   uint tile_count, uint group_start, uint chunks)''',
     '''uint key_count, uint tile_count)'''),
    ('__local float score[FLASH_ROWS][FLASH_KEYS];', '__local float score[6][FLASH_KEYS];'),
    ('''    const uint work = work_group() + group_start;
    const uint groups_per_head = (queries*6 + FLASH_ROWS-1)/FLASH_ROWS;
    const uint kv_head = work / groups_per_head;
    const uint first_row = (work % groups_per_head)*FLASH_ROWS;
    const uint last_row = flash_min(first_row+FLASH_ROWS, queries*6)-1;
    const uint tile = flash_grid_y(), key_start = tile * FLASH_KEYS;
    if (key_start >= first_visible + last_row/6) return;''',
     '''    const uint kv_head = work_group();
    const uint tile = flash_grid_y(), key_start = tile * FLASH_KEYS;
    const uint live = flash_min(FLASH_KEYS,key_count-key_start);'''),
    ('''    __global float *out = partial + ((ulong)work_group()*tile_count + tile) * FLASH_ROWS * FLASH_PARTIAL;''',
     '''    const ulong head_stride = (ulong)tile_count*FLASH_PARTIAL;
    __global float *out = partial + kv_head*6*head_stride + tile*FLASH_PARTIAL;'''),
    ('''        const uint original = first_row+r, token = original/6, h = original%6;
        uint packed = 0;
        if (token < queries) {
            const __global float *q = query + ((ulong)token * 24 + kv_head*6 + h)*256 + col;''',
     '''        uint packed = 0;
        if (r < 6) {
            const __global float *q = query + (kv_head*6+r)*256 + col;'''),
    ('''        score[lane_row][key_col] = acc.x * 0.0625f;
        score[lane_row][key_col+1] = acc.y * 0.0625f;
        score[lane_row+8][key_col] = acc.z * 0.0625f;
        score[lane_row+8][key_col+1] = acc.w * 0.0625f;''',
     '''        if (lane_row < 6) {
            score[lane_row][key_col] = acc.x * 0.0625f;
            score[lane_row][key_col+1] = acc.y * 0.0625f;
        }'''),
    ('''        const uint token = (first_row+r)/6;
        const uint visible = flash_min(key_count, first_visible + token);
        const uint count = token < queries && visible > key_start ? flash_min(FLASH_KEYS, visible-key_start) : 0;''',
     '''        const uint count = r < 6 ? live : 0;'''),
    ('''        if (lane == 0) {
            out[r*FLASH_PARTIAL] = mx;
            out[r*FLASH_PARTIAL+1] = sum;
        }''',
     '''        if (lane == 0 && r < 6) {
            out[r*head_stride] = mx;
            out[r*head_stride+1] = sum;
        }'''),
    ('''        out[lane_row*FLASH_PARTIAL+column] = acc[result].x;
        out[lane_row*FLASH_PARTIAL+column+1] = acc[result].y;
        out[(lane_row+8)*FLASH_PARTIAL+column] = acc[result].z;
        out[(lane_row+8)*FLASH_PARTIAL+column+1] = acc[result].w;''',
     '''        if (lane_row < 6) {
            out[lane_row*head_stride+column] = acc[result].x;
            out[lane_row*head_stride+column+1] = acc[result].y;
        }'''),
):
    assert kernel.count(old) == 1, old
    kernel = kernel.replace(old, new)
output.write_text(text + kernel.replace('FLASH_KEYS','256u'))
PY
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/component.cl" -o "$out/component.ptx"

python3 - "$base/pipeline.ptx" "$out/component.ptx" "$out/pipeline.ptx" "$entry" <<'PY'
from pathlib import Path
import sys

base, component, output = map(Path, sys.argv[1:4])
anchor = f'.visible .entry {sys.argv[4]}('
target, donor = base.read_text(), component.read_text()
assert anchor not in target and donor.count(anchor) == 1
start = donor.index(anchor)
end = donor.index('\n}', start) + len('\n}')
output.write_text(target + '\n\n' + donor[start:end] + '\n')
PY
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/pipeline.ptx" -o "$out/pipeline.cubin" 2>"$out/ptxas.txt"
