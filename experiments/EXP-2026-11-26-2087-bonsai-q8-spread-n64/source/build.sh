#!/usr/bin/env bash
set -euo pipefail

out=${1:?Specify the component output directory}
base=${2:?Specify EXP-2086 build directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
entry=bonsai_pq2_mmq_int8_full_n64_spread
mkdir -p "$out"

python3 - "$base/component.cl" "$out/component.cl" "$entry" <<'PY'
from pathlib import Path
import sys

reference, output = map(Path, sys.argv[1:3])
name = 'bonsai_pq2_mmq_int8_full_n128_spread'
text = reference.read_text()
start = text.index(f'__kernel void {name}(')
end = text.index('\n}\n', start) + len('\n}\n')
kernel = text[start:end]
for old, new, count in (
    (name, sys.argv[3], 1),
    ('x[128][36]', 'x[64][36]', 1),
    ('token_scale[128]', 'token_scale[64]', 1),
    ('panel=flash_grid_y(),token_base=128*panel', 'tile=flash_grid_y(),token_base=64*tile', 1),
    ('(panel*blocks+block)*128', '((tile/2)*blocks+block)*128+(tile%2)*64', 1),
    ('warp_token=64*(warp%2)', 'warp_token=32*(warp%2)', 1),
    ('packed<128*32', 'packed<64*32', 1),
    ('if (thread<128)', 'if (thread<64)', 1),
    ('result[8]', 'result[4]', 1),
    ('i<8;++i', 'i<4;++i', 1),
    ('dot[8]', 'dot[4]', 1),
    ('f<8;++f', 'f<4;++f', 4),
):
    assert kernel.count(old) == count, old
    kernel = kernel.replace(old, new)
output.write_text(text + kernel)
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
