#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
out=${1:?Specify a fresh candidate directory}
base=${2:?Specify EXP-2049 kernel source}
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir "$out"
python3 "$src/prepare.py" "$base" "$out/pipeline.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_89 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/pipeline.cl" -o "$out/pipeline.ptx"
python3 "$src/../../EXP-2026-11-25-2018-bonsai-flash-kv-panels/source/dynamic.py" "$out/pipeline.ptx"
python3 - "$out/pipeline.ptx" <<'PYPTX'
import re
from pathlib import Path
import sys
p = Path(sys.argv[1])
s = p.read_text()
s, version = re.subn(r"^\.version 8\.3$", ".version 8.4", s, flags=re.M)
assert version == 1
name = "bonsai_flash_kv_fused_ordered_$_panel_scratch"
s, count = re.subn(r"^\s*\.shared \.align 16 \.b8 " + re.escape(name) + r"\[50176\];$", "", s, flags=re.M)
assert count == 1
s = s.replace(name, "bonsai_flash_shared")
name = "bonsai_flash_fp8_qk_$_panel_scratch"
s, count = re.subn(r"^\s*\.shared \.align 16 \.b8 " + re.escape(name) + r"\[41472\];$", "", s, flags=re.M)
assert count == 1
s = s.replace(name, "bonsai_flash_shared")
name = "bonsai_flash_fp8_qkpv_$_panel_scratch"
s, count = re.subn(r"^\s*\.shared \.align 16 \.b8 " + re.escape(name) + r"\[41472\];$", "", s, flags=re.M)
assert count == 1
p.write_text(s.replace(name, "bonsai_flash_shared"))
PYPTX
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/pipeline.ptx" -o "$out/pipeline.cubin" 2>"$out/ptxas.txt"
