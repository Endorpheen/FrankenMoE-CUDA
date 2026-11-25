#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
out=${1:?Specify the component output directory}
base=${2:?Specify EXP-2086 build directory}
reference="$src/../../EXP-2026-11-25-2005-bonsai-decode-gqa/source/grouped.cl"
cuda=${CUDA_HOME:-/usr/local/cuda}
entry=bonsai_decode_gqa3_warp_tiles
mkdir -p "$out"

python3 - "$reference" "$out/component.cl" "$entry" <<'PY'
from pathlib import Path
import sys

reference, output = map(Path, sys.argv[1:3])
name = sys.argv[3]
text = reference.read_text()
original = '''    uint lane, job, tile;
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(job));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(tile));'''
candidate = '''    uint thread, job, block_y, threads;
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(job));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(block_y));
    __asm__("mov.u32 %0, %%ntid.x;" : "=r"(threads));
    const uint lane=thread%32;
    const uint tile=block_y*(threads/32)+thread/32;
    if (tile>=tiles) return;'''
assert text.count(original) == 1
text = text[:text.index('#define GROUPED(H)')].replace(original, candidate, 1)
output.write_text(text + f'''
__kernel void {name}(__global const float *q, __global const uchar *k,
    __global const uchar *v, __global float *scratch, uint keys, uint tiles, float scale) {{
    grouped_tiles(q, k, v, scratch, keys, tiles, scale, 3);
}}
''')
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
