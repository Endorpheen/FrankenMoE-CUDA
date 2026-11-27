#!/usr/bin/env bash
set -euo pipefail

output=$(realpath -m -- "${1:?Usage: build-gooseinfer-shared-cuda-image.sh OUTPUT_HEADER}")
tools=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT
mkdir -p -- "$(dirname -- "$output")"

bash "$tools/build-gooseinfer-bonsai-ptx.sh" "$scratch/plain.ptx" plain
bash "$tools/build-gooseinfer-bonsai-ptx.sh" "$scratch/mmq.ptx" mmq
python3 - "$output" "$scratch" <<'PY'
from pathlib import Path
import re
import sys

output, scratch = map(Path, sys.argv[1:])
argument_counts = {}
with output.open("w") as header:
    for variant, name in (("plain", "goose_cuda_image"), ("mmq", "goose_cuda_mmq_image")):
        data = (scratch / f"{variant}.ptx").read_bytes() + b"\0"
        signatures = re.findall(rb"(?m)^\s*(?:\.visible\s+)?\.entry\s+(\w+)\s*\(([^)]*)\)", data)
        if not signatures:
            raise ValueError(f"No kernels found in {variant} PTX")
        for kernel, parameters in signatures:
            count = len(re.findall(rb"\.param\b", parameters))
            if argument_counts.setdefault(kernel.decode("ascii"), count) != count:
                raise ValueError(f"Inconsistent signature for {kernel!r}")
        header.write(f"static const unsigned char {name}[] = {{\n")
        for offset in range(0, len(data), 24):
            header.write(",".join(map(str, data[offset:offset + 24])) + ",\n")
        header.write("};\n")
    header.write("#define GOOSE_CUDA_HAS_ARG_COUNTS 1\n"
                 "static const struct { const char *name; unsigned count; } goose_cuda_arg_counts[] = {\n")
    for kernel, count in sorted(argument_counts.items()):
        header.write(f'    {{"{kernel}", {count}}},\n')
    header.write("};\n")
PY
