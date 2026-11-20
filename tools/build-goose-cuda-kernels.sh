#!/bin/bash
# Compile Goose device arithmetic through OpenCL C to driver-loadable PTX.
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT=${1:?usage: build-goose-cuda-kernels.sh output-directory}
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CLANG=${CLANG:-clang-18}
mkdir -p "$OUT"

sed '/^fn main()/,$d; s/^fn fg182d_q4_K(/export fn fg182d_q4_K(/' \
    "$ROOT/goose/cuda/goose-cuda-dequant-q4k/device.goose" > "$OUT/q4k.goose"
sed '/^fn main()/,$d; s/^fn q6_K(/export fn q6_K(/' \
    "$ROOT/goose/cuda/goose-cuda-dequant-q6k/device.goose" > "$OUT/q6k.goose"
cp "$ROOT/goose/cuda/standalone-matmul.goose" "$OUT/matmul.goose"
for module in q4k q6k matmul; do
    "$GOOSE" --standalone -o "$OUT/$module.c" --header "$OUT/$module.h" "$OUT/$module.goose"
    python3 - "$OUT" "$module" <<'PY'
from pathlib import Path
import sys

out, module = Path(sys.argv[1]), sys.argv[2]
source = (out / f"{module}.c").read_text()
fragment = source.split("/* ---- prototypes ---- */", 1)[1].split("static void gs_init_globals(void)", 1)[0]
if "gs_sp" in fragment:
    raise SystemExit(f"{module} uses the host stack ABI")
(out / f"{module}-device.h").write_text(fragment)
PY
done
"$CLANG" -target nvptx64-nvidia-cuda -Xclang -target-cpu -Xclang sm_70 \
    -nocudalib -O2 -x cl -cl-std=CL1.2 -I "$OUT" -S \
    "$ROOT/integration/goose-standalone/cuda-device.cl" -o "$OUT/goose-cuda.ptx"
python3 - "$OUT" <<'PY'
from pathlib import Path
import sys

out = Path(sys.argv[1])
ptx = (out / "goose-cuda.ptx").read_bytes() + b"\0"
lines = [", ".join(str(byte) for byte in ptx[i:i + 24]) for i in range(0, len(ptx), 24)]
(out / "goose-cuda-image.h").write_text(
    "static const unsigned char goose_cuda_image[] = {\n" + ",\n".join(lines) + "\n};\n"
)
PY
