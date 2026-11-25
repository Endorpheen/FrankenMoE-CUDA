"""Retain EXP-2065 and specialize the width-6144 full tile."""

import importlib.util
from pathlib import Path
import sys


experiments = Path(__file__).resolve().parents[2]
prior = experiments / "EXP-2026-11-25-2048-bonsai-mmq-full-tile/source/prepare.py"
spec = importlib.util.spec_from_file_location("full_tiles", prior)
specialization = importlib.util.module_from_spec(spec)
spec.loader.exec_module(specialization)

base, out = map(Path, sys.argv[1:])
source = base.read_text()
entry = "bonsai_pq2_cached_fp16_acc_k32"
start = source.index(f"__kernel void {entry}(")
end = source.index("\n}\n", start) + 3
control = source[start:end].replace(entry, "bonsai_pq2_cached_64x128_k64")
variant = specialization.kernel_variant(control, 6144).replace(
    "bonsai_pq2_full_64x128_k64_6144", "bonsai_pq2_half_full_6144")
out.write_text(source + "\n" + variant)
