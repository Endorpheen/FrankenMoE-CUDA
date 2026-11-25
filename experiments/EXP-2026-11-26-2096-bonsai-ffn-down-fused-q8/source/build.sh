#!/usr/bin/env bash
set -euo pipefail

out=${1:?Specify the component output directory}
base=${2:?Specify EXP-2095 build directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
tail=bonsai_ffn_tail_q8_panel
mmq=bonsai_pq2_mmq_int8_down_n128_spread
mkdir -p "$out"

python3 - "$base/component.cl" "$out/component.cl" "$tail" "$mmq" <<'PY'
from pathlib import Path
import sys

reference, output = map(Path, sys.argv[1:3])
tail_name, mmq_name = sys.argv[3:]
text = reference.read_text()


def kernel(name):
    start = text.index(f'__kernel void {name}(')
    end = text.index('\n}\n', start) + len('\n}\n')
    return text[start:end]


def replace(source, old, new):
    assert source.count(old) == 1, old
    return source.replace(old, new)


tail = replace(kernel('bonsai_ffn_tail'), 'bonsai_ffn_tail', tail_name)
tail = replace(tail, '__global uint *packed,uint width)',
               '__global uint *packed,__global float *scales,uint width)')
tail = replace(tail, '''    for (uint i=lane;i<512;i+=256) {
        packed[(token+base)/2+i]=bonsai_half_pair(values[2*i]*0.03125f,values[2*i+1]*0.03125f);
    }''', '''    const uint warp=lane/32, in_warp=lane%32;
    const uint first=warp*128+in_warp*4;
    const float4 v=(float4)(values[first],values[first+1],values[first+2],values[first+3])*0.03125f;
    float largest=as_float(as_uint(v.x)&0x7fffffffu);
    largest=flash_max(largest,as_float(as_uint(v.y)&0x7fffffffu));
    largest=flash_max(largest,as_float(as_uint(v.z)&0x7fffffffu));
    largest=flash_max(largest,as_float(as_uint(v.w)&0x7fffffffu));
    for (uint bit=16;bit;bit/=2) largest=flash_max(largest,flash_shuffle(largest,bit));
    const float scale=largest/127.0f;
    const uint token_id=flash_grid_y(), block=work_group()*8+warp;
    const ulong row=((ulong)(token_id/128)*136+block)*128+token_id%128;
    packed[row*32+in_warp]=bonsai_quantize_q8_value(v.x,scale)
        | (bonsai_quantize_q8_value(v.y,scale)<<8)
        | (bonsai_quantize_q8_value(v.z,scale)<<16)
        | (bonsai_quantize_q8_value(v.w,scale)<<24);
    if (in_warp==0) scales[row]=scale;''')
old_mmq = 'bonsai_pq2_mmq_int8_full_n128_spread'
mmq = replace(kernel(old_mmq), old_mmq, mmq_name)
mmq = replace(mmq, 'rows=17408,blocks=40,', 'rows=5120,blocks=136,')
output.write_text(text + tail + mmq)
PY

clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/component.cl" -o "$out/component.ptx"

python3 - "$base/pipeline.ptx" "$out/component.ptx" "$out/pipeline.ptx" "$tail" "$mmq" <<'PY'
from pathlib import Path
import sys

base, component, output = map(Path, sys.argv[1:4])
target, donor = base.read_text(), component.read_text()
for entry in sys.argv[4:]:
    anchor = f'.visible .entry {entry}('
    assert anchor not in target and donor.count(anchor) == 1
    start = donor.index(anchor)
    end = donor.index('\n}', start) + len('\n}')
    target += '\n\n' + donor[start:end] + '\n'
output.write_text(target)
PY
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/pipeline.ptx" -o "$out/pipeline.cubin" 2>"$out/ptxas.txt"
