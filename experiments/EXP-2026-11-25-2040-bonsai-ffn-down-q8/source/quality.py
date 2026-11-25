"""Run the established independent quality harness with the isolated Q8 down scope."""
from pathlib import Path
import sys


layers = int(sys.argv.pop(1))
assert 1 <= layers <= 16
experiments = Path(__file__).resolve().parents[2]
harness = experiments / "EXP-2026-11-25-1994-bonsai-pq2-int8/source/check-quality.py"
text = harness.read_text()
old = "runtime_env={**env,'BONSAI_PQ2_MMQ_INT8':str(mode)}"
assert text.count(old) == 1
text = text.replace(old, f"runtime_env={{**env,'BONSAI_FFN_DOWN_Q8_LAST':str({layers} if mode else 0)}}")
exec(compile(text, str(harness), "exec"), {"__file__": str(harness), "__name__": "__main__"})
