"""Append the established exact PQ2-to-FP16 expansion for diagnostic GEMM."""

from pathlib import Path
import sys


base, out = map(Path, sys.argv[1:])
experiments = Path(__file__).resolve().parents[2]
source = (experiments / "EXP-2026-11-25-1993-bonsai-mmq-expand/source/expand.cl").read_text()
start = source.index("__kernel void bonsai_pq2_expand_half(")
end = source.index("\n}\n", start) + 3
out.write_text(base.read_text() + "\n" + source[start:end])
