"""Reuse the scoring harness while changing only one early projection."""
from pathlib import Path
import sys

mode=sys.argv.pop(1)
assert mode in ("gate","up")
exp=Path(__file__).resolve().parents[2]
src=exp/"EXP-2026-11-25-1994-bonsai-pq2-int8/source/check-quality.py"
s=src.read_text()
a="runtime_env={**env,'BONSAI_PQ2_MMQ_INT8':str(mode)}"
assert s.count(a)==1
s=s.replace(a,f"runtime_env={{**env,'BONSAI_FFN_Q8_EARLY':{mode!r} if mode else '0'}}")
exec(compile(s,str(src),"exec"),{"__file__":str(src),"__name__":"__main__"})
