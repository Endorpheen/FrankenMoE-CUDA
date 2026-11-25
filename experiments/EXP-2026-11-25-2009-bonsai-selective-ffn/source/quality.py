"""Run the established multi-request quality harness with selective FFN modes."""
from pathlib import Path
import sys

experiment = Path(__file__).resolve().parents[2]
harness = experiment / "EXP-2026-11-25-1994-bonsai-pq2-int8/source/check-quality.py"
src = harness.read_text()
mode = sys.argv.pop(1)
assert mode in ("plain", "residual", "last16")
anchor = "runtime_env={**env,'BONSAI_PQ2_MMQ_INT8':str(mode)}"
assert src.count(anchor) == 1
src = src.replace(anchor, f"runtime_env={{**env,'BONSAI_FFN_QUANT':{mode!r} if mode else 'off'}}")
exec(compile(src, str(harness), "exec"), {"__file__": str(harness), "__name__": "__main__"})
