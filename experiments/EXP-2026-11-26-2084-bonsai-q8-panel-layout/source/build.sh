#!/usr/bin/env bash
set -euo pipefail

out=${1:?Specify the component output directory}
base=${2:?Specify EXP-2081 build directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
pack=bonsai_pack_q8_panel128
mmq=bonsai_pq2_mmq_int8_full_n128_panel
mkdir -p "$out"

python3 - "$base/component.cl" "$out/component.cl" "$pack" "$mmq" <<'PY'
from pathlib import Path
import sys

reference, output = map(Path, sys.argv[1:3])
pack_name, mmq_name = sys.argv[3:]
text = reference.read_text()


def kernel(name):
    start = text.index(f'__kernel void {name}(')
    end = text.index('\n}\n', start) + len('\n}\n')
    return text[start:end]


def replace(source, old, new):
    assert source.count(old) == 1, old
    return source.replace(old, new)


pack = replace(kernel('bonsai_pack_q8_128'), 'bonsai_pack_q8_128', pack_name)
pack = replace(pack, '    packed[(ulong)row*32+lane]',
               '''    const uint token=row/40, block=row%40;
    const uint panel_row=((token/128)*40+block)*128+token%128;
    packed[(ulong)panel_row*32+lane]''')
pack = replace(pack, 'scales[row]=scale', 'scales[panel_row]=scale')

old_mmq = 'bonsai_pq2_mmq_int8_full_n128_pairload'
mmq = replace(kernel(old_mmq), old_mmq, mmq_name)
mmq = replace(mmq, 'token_base=128*flash_grid_y();',
              'panel=flash_grid_y(),token_base=128*panel;')
mmq = replace(mmq, '''        for (uint packed=4*thread;packed<128*32;packed+=1024) {
            const uint t=token_base+packed/32;
            const ulong from=((ulong)t*blocks+block)*32+packed%32;
            bonsai_copy16(&x[packed/32][packed%32],input+from,16);
        }
        if (thread<128) {
            const uint token=token_base+thread;
            token_scale[thread]=scales[(ulong)token*blocks+block];
        }''',
               '''        const uint panel_row=(panel*blocks+block)*128;
        for (uint packed=4*thread;packed<128*32;packed+=1024) {
            const ulong from=(ulong)panel_row*32+packed;
            bonsai_copy16(&x[packed/32][packed%32],input+from,16);
        }
        if (thread<128) {
            token_scale[thread]=scales[panel_row+thread];
        }''')
output.write_text(text + pack + mmq)
PY

clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/component.cl" -o "$out/component.ptx"

python3 - "$base/pipeline.ptx" "$out/component.ptx" "$out/pipeline.ptx" "$pack" "$mmq" <<'PY'
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
