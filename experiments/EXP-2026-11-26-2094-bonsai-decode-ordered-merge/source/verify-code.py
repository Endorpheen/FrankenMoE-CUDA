"""Check that the cumulative CUBIN retains its original machine code sections."""

import json
from pathlib import Path
import re
import subprocess
import sys


SECTION = re.compile(
    r"^\s*\[\s*\d+\]\s+(\.text\.[^ ]+)\s+PROGBITS\s+[0-9a-f]+\s+"
    r"([0-9a-f]+)\s+([0-9a-f]+)",
    re.M,
)
ADDED = {".text.bonsai_verify_merge_parallel"}


def sections(path):
    binary = path.read_bytes()
    result = subprocess.run(["readelf", "-W", "-S", str(path)], text=True,
                            check=True, capture_output=True)
    found = {}
    for name, offset, size in SECTION.findall(result.stdout):
        begin = int(offset, 16)
        found[name] = binary[begin:begin + int(size, 16)]
    if not found:
        raise ValueError(f"{path}: no CUDA code sections")
    return found


base, cumulative = map(Path, sys.argv[1:])
control, combined = sections(base), sections(cumulative)
changed = [name for name, binary in control.items() if combined.get(name) != binary]
added = combined.keys() - control.keys()
if changed or added != ADDED:
    raise ValueError(f"Unexpected CUDA code changes: {changed}; added {sorted(added)}")
print(json.dumps({"retained_sections_bit_exact": len(control), "added": sorted(added)}, indent=2))
