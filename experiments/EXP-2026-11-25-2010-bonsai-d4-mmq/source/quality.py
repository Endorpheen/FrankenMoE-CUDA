"""Use the shared quality harness with the scoped D4 candidate."""
from pathlib import Path
import sys

experiment = Path(__file__).resolve().parents[2]
harness = experiment / "EXP-2026-11-25-1994-bonsai-pq2-int8/source/check-quality.py"
text = harness.read_text()
anchor = "runtime_env={**env,'BONSAI_PQ2_MMQ_INT8':str(mode)}"
assert text.count(anchor) == 1
text = text.replace(anchor, "runtime_env={**env,'BONSAI_FFN_QUANT':'d4' if mode else 'off'}")
exec(compile(text, str(harness), "exec"), {"__file__": str(harness), "__name__": "__main__"})
