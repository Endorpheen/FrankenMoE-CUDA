#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
root=$(git -C "$src" rev-parse --show-toplevel)
out=${1:?Specify a fresh candidate directory}
base=${2:?Specify an EXP-2019 release rebuild directory}
checkpoints=${FRANKENMOE_CHECKPOINTS:-$root/build}
cuda=${CUDA_HOME:-/usr/local/cuda}
session="$root/experiments/EXP-2026-11-25-2002-bonsai-session/source"
host="$checkpoints/bonsai-mmq-k64-2000-default"
http="$checkpoints/bonsai-http-2003-link"
apex="$checkpoints/checkpoint-apex-exp1880-before-inkling"
inkling="$checkpoints/checkpoint-inkling-exp1969-before-bonsai"
device="$checkpoints/inkling-exp-baseline/device-attention"
archive="$checkpoints/bonsai-dp4a-2012-default/libfranken-inference.a"

mkdir "$out"
python3 "$src/prepare.py" "$base/device/pipeline.cl" "$base/http/backend.c" "$out"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/pipeline.cl" -o "$out/pipeline.ptx"
python3 "$root/experiments/EXP-2026-11-25-2018-bonsai-flash-kv-panels/source/dynamic.py" "$out/pipeline.ptx"
python3 - "$out/pipeline.ptx" <<'PYPTX'
import re
from pathlib import Path
import sys
p = Path(sys.argv[1])
s = p.read_text()
name = "bonsai_flash_kv_fused_ordered_$_panel_scratch"
s, count = re.subn(r"^\s*\.shared \.align 16 \.b8 " + re.escape(name) + r"\[50176\];$", "", s, flags=re.M)
assert count == 1
p.write_text(s.replace(name, "bonsai_flash_shared"))
PYPTX
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/pipeline.ptx" -o "$out/pipeline.cubin" 2>"$out/ptxas.txt"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -I"$host" -I"$session" -I"$cuda/include" -Dmain=bonsai_diagnostic_main \
    -c "$out/backend.c" -o "$out/session-unlinked.o"
objcopy --redefine-syms="$http/rename.txt" "$out/session-unlinked.o" "$out/bonsai-session.o"
gcc-13 -O2 -std=gnu11 -D_GNU_SOURCE -DI2_NUMERIC_MODULE -Werror=implicit-function-declaration \
    -ffunction-sections -fdata-sections -I"$apex/build-inputs" -I"$apex/checkpoint" \
    -I"$root/vendor/flint/ggml/include" -I"$root/vendor/flint/ggml/src" \
    -I"$root/integration/goose-standalone" -I"$root/integration/m1/host" \
    -Wl,--gc-sections "$inkling/source/infer-main.c" \
    "$device/inkling-moe-tile32/numeric.o" "$out/bonsai-session.o" \
    "$http/bonsai-goose.o" "$archive" -lm -ldl -pthread -o "$out/frankenmoe-infer"
