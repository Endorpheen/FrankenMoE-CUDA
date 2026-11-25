"""Use the established scoring and fixed-history harness for decode Q8."""
from pathlib import Path
import sys

mode = sys.argv.pop(1)
assert mode in ("full", "ffn", "vocab")
experiment = Path(__file__).resolve().parents[2]
harness = experiment / "EXP-2026-11-25-1994-bonsai-pq2-int8/source/check-quality.py"
src = harness.read_text()
anchor = "runtime_env={**env,'BONSAI_PQ2_MMQ_INT8':str(mode)}"
assert src.count(anchor) == 1
code = src.replace(anchor, f"runtime_env={{**env,'BONSAI_DECODE_Q8':{mode!r} if mode else 'off'}}")
exec(compile(code, str(harness), "exec"), {"__file__": str(harness), "__name__": "__main__"})
