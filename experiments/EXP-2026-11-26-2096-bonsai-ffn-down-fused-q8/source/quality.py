"""Re-use the established Bonsai quality suite with the cumulative presets."""

from pathlib import Path


experiments = Path(__file__).resolve().parents[2]
harness = experiments / "EXP-2026-11-25-1994-bonsai-pq2-int8/source/check-quality.py"
source = harness.read_text()
anchor = "runtime_env={**env,'BONSAI_PQ2_MMQ_INT8':str(mode)}"
assert source.count(anchor) == 1
source = source.replace(
    anchor,
    "runtime_env={**env,'BONSAI_MMQ_HALF_ACC':'1','BONSAI_FLASH_FP8_QK':'1',"
    "'BONSAI_FLASH_FP8_PV':'1','BONSAI_FFN_Q8_PREFILL':'1','BONSAI_UBATCH':'128','BONSAI_DECODE_DIRECT_MMA':'1','BONSAI_DECODE_ORDERED_MERGE':'1','BONSAI_DECODE_DIRECT_HALF2':'1','BONSAI_FFN_DOWN_Q8_FUSED':str(mode),"
    "'BONSAI_FFN_Q8_PAIRLOAD':'1','BONSAI_FFN_Q8_PANEL':'1','BONSAI_FFN_Q8_SPREAD':'1'}",
)
exec(compile(source, str(harness), "exec"), {"__file__": str(harness), "__name__": "__main__"})
