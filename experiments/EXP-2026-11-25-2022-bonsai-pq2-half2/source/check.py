"""Check every finite FP16 scale/code pair, then time real matrix tiles."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
src = root / "EXP-2026-11-25-2019-bonsai-mmq-pipeline/source/check.py"
text = src.read_text()
a=text.index("variants = ")
b=text.index("\n\ndevice",a)
text=text[:a]+'''variants = {
    "64x64_lut": "bonsai_pq2_lut_64x64_k64",
    "64x128_lut": "bonsai_pq2_lut_64x128_k64",
    "64x64_half2": "bonsai_pq2_half2_64x64_k64",
    "64x128_half2": "bonsai_pq2_half2_64x128_k64",
}'''+text[b:]
a='    common={"pack":bind(control,"bonsai_pack_half"),'
b='''    mismatches = gpu.allocate(4)
    try:
        gpu.call("cuMemsetD8_v2", mismatches, U(0), gpu.S(4))
        component.dispatch(bind(trial, "bonsai_pq2_validate_unpack"), (65536,1), 256, (mismatches,))
        value = U()
        gpu.call("cuMemcpyDtoH_v2",ct.byref(value),mismatches,gpu.S(4))
        assert value.value == 0, value.value
        print("checked 16252928 scale/code cases with no differences",file=sys.stderr)
    finally:
        gpu.call("cuMemFree_v2", mismatches)
    common={"pack":bind(control,"bonsai_pack_half"),'''
assert text.count(a)==1
text=text.replace(a,b).replace('f"bonsai_pq2_mmq_64x{baseline}_k64"', 'f"bonsai_pq2_cached_64x{baseline}_k64"')
exec(compile(text,str(src),"exec"),{"__file__":str(src),"__name__":"__main__"})
