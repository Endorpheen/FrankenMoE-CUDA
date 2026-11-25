#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
repo=$(git rev-parse --show-toplevel)
out=${1:?Specify a new output directory}
compiler=${GOOSE_BIN:-/home/end0/AI/Fried-Goose/releases/fried-goose-v0.3.0-preview-keelwing/bin/goose}
gcc=${CC:-gcc-13}
cuda=${CUDA_HOME:-/usr/local/cuda}
bootstrap=${BONSAI_BOOTSTRAP:-"$repo/build/bonsai-integration"}
header=${GOOSE_FROZEN_IMAGE:-"$repo/build/checkpoint-apex-exp1880-before-inkling/build-inputs/goose-cuda-image.h"}
mkdir "$out"
sed -n '/^fn bt_eog_text(/,/^fn bt_has_unused(/{ /^fn bt_has_unused(/d;p; }
        /^export fn bt_eog_ids(/,/^\/\/ Tokenises/{ /^\/\/ Tokenises/d;p; }' \
    "$repo/goose/model/bpe-tokenize.goose" > "$out/eog.goose"
cat "$bootstrap/core.goose" "$out/eog.goose" "$src/trunk.goose" > "$out/bonsai.goose"
"$compiler" --standalone -o "$out/bonsai.c" --header "$out/bonsai.h" "$out/bonsai.goose"
"$gcc" -O2 -fPIC -ffunction-sections -fdata-sections -w -c "$out/bonsai.c" -o "$out/bonsai.o"
"$gcc" -O2 -Wall -Wextra -Werror -Wl,--gc-sections -I"$out" -I"$cuda/include" \
    "$src/backend.c" "$out/bonsai.o" \
    "$src/../../EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -l:libcuda.so.1 -lm -pthread -o "$out/bonsai-infer"
python3 - "$header" "$out/frozen-goose.ptx" <<'PY'
from pathlib import Path
import re
import sys

content = Path(sys.argv[1]).read_bytes()
array = re.search(rb'goose_cuda_mmq_image\[\] = \{(.*?)\};', content, re.S)
assert array
image = bytes(int(item, 0) for item in array[1].split(b',') if item.strip())
assert b'.entry goose_gdn_decode' in image
Path(sys.argv[2]).write_bytes(image.rstrip(b'\0'))
PY
