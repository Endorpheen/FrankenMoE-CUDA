#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a diagnostic output directory}
previous="$root/build/bonsai-mtp-head-2025"
target="$root/build/bonsai-mtp-exact-2029"
image="$root/build/bonsai-mtp-q8-2028"
experiments="$src/../.."
cuda=${CUDA_HOME:-/usr/local/cuda}
compiler=${GOOSE_BIN:-/home/end0/AI/Fried-Goose/releases/fried-goose-v0.3.0-preview-keelwing/bin/goose}
mkdir -p "$out"
python3 "$src/prepare.py" "$target/backend.c" "$previous/stem.goose" "$out"
python3 - "$experiments/EXP-2026-11-25-2027-bonsai-mtp-target-verify/source/target-probe.inc" "$src/commit-test.inc" "$out/commit-probe.inc" <<'PYCODE'
from pathlib import Path
import sys
base, test, out = map(Path, sys.argv[1:])
helpers, marker, _ = base.read_text().partition("static int probe_target(")
assert marker
out.write_text(helpers + test.read_text())
PYCODE

"$compiler" --standalone -o "$out/target.c" --header "$out/bonsai.h" "$out/target.goose"
gcc-13 -O2 -fPIC -ffunction-sections -fdata-sections -w -c "$out/target.c" -o "$out/target.o"
cat "$image/verify.cl" "$src/conv-prefix.cl" > "$out/commit.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/commit.cl" -o "$out/commit.ptx"
python3 "$experiments/EXP-2026-11-25-2018-bonsai-flash-kv-panels/source/dynamic.py" "$out/commit.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/commit.ptx" -o "$out/commit.cubin" 2>"$out/ptxas.txt"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$out" -I"$src" -I"$experiments/EXP-2026-11-25-2029-bonsai-mtp-exact-verify/source" \
    -I"$experiments/EXP-2026-11-25-2025-bonsai-mtp-head/source" \
    -I"$experiments/EXP-2026-11-25-2002-bonsai-session/source" -I"$cuda/include" \
    "$out/backend.c" "$out/target.o" \
    "$experiments/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -lm -ldl -pthread -o "$out/target-commit"
