"""Use the established independently scored Bonsai quality fixture."""

from pathlib import Path


experiments = Path(__file__).resolve().parents[2]
harness = experiments / "EXP-2026-11-25-1994-bonsai-pq2-int8/source/check-quality.py"
source = harness.read_text()
anchor = "runtime_env={**env,'BONSAI_PQ2_MMQ_INT8':str(mode)}"
assert source.count(anchor) == 1
source = source.replace(anchor, "runtime_env={**env,'BONSAI_MMQ_HALF_ACC':str(mode)}")
exec(compile(source, str(harness), "exec"), {"__file__": str(harness), "__name__": "__main__"})
