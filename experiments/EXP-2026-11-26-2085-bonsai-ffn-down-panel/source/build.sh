#!/usr/bin/env bash
set -euo pipefail

out=${1:?Specify the component output directory}
base=${2:?Specify EXP-2084 build directory}
reference=${3:?Specify EXP-2071 pipeline.cl}
cuda=${CUDA_HOME:-/usr/local/cuda}
tail=bonsai_ffn_tail_panel128
mmq=bonsai_pq2_half_unpack_half2_17408_panel
mkdir -p "$out"

python3 - "$reference" "$out/component.cl" "$tail" "$mmq" <<'PY'
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
tail = replace(tail, '    const ulong token=(ulong)flash_grid_y()*width;',
               '''    const uint token_id=flash_grid_y();
    const ulong token=(ulong)token_id*width;''')
tail = replace(tail, '        packed[(token+base)/2+i]=',
               '''        const uint col=base/2+i;
        const ulong row=((ulong)(token_id/128)*272+col/32)*128+token_id%128;
        packed[row*32+col%32]=''')
old_mmq = 'bonsai_pq2_half_unpack_half2_17408'
mmq = replace(kernel(old_mmq), old_mmq, mmq_name)
mmq = replace(mmq, 'token_base=128*flash_grid_y();',
              'panel=flash_grid_y(), token_base=128*panel;')
mmq = replace(mmq, '''            const uint token=token_base+packed/32,col=packed%32;
            const ulong source=(ulong)token*8704+step*32+col;''',
               '''            const uint col=packed%32;
            const ulong source=((ulong)panel*272+step)*4096+packed;''')
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
