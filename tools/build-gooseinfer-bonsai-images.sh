#!/usr/bin/env bash
set -euo pipefail

output=${1:?usage: build-gooseinfer-bonsai-images.sh OUTPUT PIPELINE_CUBIN MTP_CUBIN FROZEN_PTX}
pipeline=${2:?Missing Bonsai pipeline image}
mtp=${3:?Missing Bonsai MTP image}
frozen=${4:?Missing frozen Goose PTX}
mkdir -p -- "$(dirname -- "$output")"
assembly=$(mktemp "$(dirname -- "$output")/bonsai-images.XXXXXX.S")
trap 'rm -f -- "$assembly"' EXIT

python3 - "$assembly" "$pipeline" "$mtp" "$frozen" <<'PY'
import json
from pathlib import Path
import sys

assembly = Path(sys.argv[1])
lines = ['#ifdef _WIN32', '.section .rdata,"dr"', '#else', '.section .rodata,"a",@progbits', '#endif']
for name, filename in zip(("pipeline", "mtp", "frozen"), sys.argv[2:], strict=True):
    path = Path(filename).resolve(strict=True)
    if not path.is_file() or path.stat().st_size == 0:
        raise SystemExit(f"Empty or invalid Bonsai image: {path}")
    symbol = f"gooseinfer_bonsai_{name}_image"
    lines.extend([".balign 16", f".globl {symbol}", f"{symbol}:", f".incbin {json.dumps(str(path), ensure_ascii=False)}"])
    if name == "frozen":
        lines.append(".byte 0")
# Image storage and address accessors stay in the same object on both ABIs.
lines.append(".text")
for name in ("pipeline", "mtp", "frozen"):
    accessor = f"gooseinfer_bonsai_{name}_address"
    image = f"gooseinfer_bonsai_{name}_image"
    lines.extend([".balign 16", f".globl {accessor}", f"{accessor}:", f"lea {image}(%rip), %rax", "ret"])
lines.extend(['#ifndef _WIN32', '.section .note.GNU-stack,"",@progbits', '#endif'])
assembly.write_text("\n".join(lines) + "\n")
PY

"${CC:-gcc-13}" -c "$assembly" -o "$output"
