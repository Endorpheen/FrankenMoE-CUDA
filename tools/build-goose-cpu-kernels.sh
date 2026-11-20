#!/bin/bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT=${1:?usage: build-goose-cpu-kernels.sh output-directory}
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
mkdir -p "$OUT"

python3 - "$ROOT" "$OUT" <<'PY'
import re
import shutil
import sys
from pathlib import Path

root, out = map(Path, sys.argv[1:])
cpu = root / "goose/cpu/goose-cpu-backend-api"
header = (root / "vendor/flint/ggml/include/ggml.h").read_text()
numbers = {}
for kind in ("op", "unary_op", "glu_op"):
    body = header.split(f"enum ggml_{kind} {{", 1)[1].split("};", 1)[0]
    body = re.sub(r"//[^\n]*", "", body)
    value = 0
    for field in body.split(","):
        field = field.strip()
        if not field:
            continue
        symbol, sep, explicit = field.partition("=")
        if sep:
            value = int(explicit.strip(), 0)
        numbers[symbol.strip().removeprefix("GGML_")] = str(value)
        value += 1

parts = (
    "tensor-base", "graph-residency", "cuda-vector", "cuda-position", "cuda-routing", "cuda-transform", "pointwise", "reductions", "attention", "transforms",
    "matrix", "recurrent", "rope", "indexed", "image", "codec-transform",
    "entry", "flash-attention", "stateful/scan-composition", "stateful/stateful", "graph-storage", "graph-dispatch", "standalone",
)
body = ["import.codec.decode;\n"]
body.extend(line for line in (root / "goose/cuda/standalone-vector.goose").read_text().splitlines()
            if line.startswith("const CV_"))
for name in parts:
    text = (cpu / f"{name}.goose").read_text().split("\nfn main()", 1)[0]
    if name == "recurrent":
        text = text[text.index("fn ssm_conv("):text.index("fn softplus_state(")]
    body.append(text.replace("import.codec.decode;", ""))
unit = "\n".join(body)
unit = re.sub(r"\b(?:OP_|UNARY_OP_|GLU_OP_)[A-Z0-9_]+\b",
              lambda match: numbers[match[0]], unit)
shutil.copytree(cpu / "codec", out / "codec", dirs_exist_ok=True)
(out / "cpu.goose").write_text(unit)
PY

"$GOOSE" --standalone -o "$OUT/cpu.c" --header "$OUT/cpu.h" "$OUT/cpu.goose"
"$CC" -O1 -std=c11 -ffunction-sections -fdata-sections \
    -Dgoose_init=fg143_cpu_init -c "$OUT/cpu.c" -o "$OUT/cpu.o"
