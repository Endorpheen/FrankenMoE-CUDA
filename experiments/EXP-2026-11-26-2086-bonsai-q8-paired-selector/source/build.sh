#!/usr/bin/env bash
set -euo pipefail

out=${1:?Specify the component output directory}
base=${2:?Specify EXP-2084 build directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
entry=bonsai_pq2_mmq_int8_full_n128_spread
mkdir -p "$out"

python3 - "$base/component.cl" "$out/component.cl" "$entry" <<'PY'
from pathlib import Path
import sys

reference, output = map(Path, sys.argv[1:3])
name = 'bonsai_pq2_mmq_int8_full_n128_panel'
text = reference.read_text()
start = text.index(f'__kernel void {name}(')
end = text.index('\n}\n', start) + len('\n}\n')
kernel = text[start:end]
original = """            const uint2 unpacked=(uint2)(bonsai_expand_pq2_byte(pair&255u),
                                         bonsai_expand_pq2_byte(pair>>8));"""
candidate = """            uint selector=(pair|(pair<<8))&0x00ff00ffu;
            selector=(selector|(selector<<4))&0x0f0f0f0fu;
            selector=(selector|(selector<<2))&0x33333333u;
            uint2 unpacked;
            uint low,high;
            __asm__("prmt.b32 %0, %1, %1, %2;" : "=r"(low) : "r"(0x020100ffu), "r"(selector&65535u));
            __asm__("prmt.b32 %0, %1, %1, %2;" : "=r"(high) : "r"(0x020100ffu), "r"(selector>>16));
            unpacked=(uint2)(low,high);"""
assert kernel.count(original) == 1
for pair in range(65536):
    def reference_selector(code):
        return sum(((code >> (2*i)) & 3) << (4*i) for i in range(4))
    spread = (pair | (pair << 8)) & 0x00ff00ff
    spread = (spread | (spread << 4)) & 0x0f0f0f0f
    spread = (spread | (spread << 2)) & 0x33333333
    assert spread == (reference_selector(pair & 255) | (reference_selector(pair >> 8) << 16))
output.write_text(text + kernel.replace(name, sys.argv[3], 1).replace(original, candidate))
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
