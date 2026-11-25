"""Measure exact staged products beside the EXP-1998 warp-pack fixture."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
source = root / "EXP-2026-11-25-1998-bonsai-decay-reuse/source/check-decay.py"
text = source.read_text()
a="MODES=('original','warp_pack','reuse2','reuse4','reuse8')"
b="MODES=('original','warp_pack','stage_128_0','stage_256_0','stage_128_1','stage_256_1')"
assert text.count(a)==1
text=text.replace(a,b)
a="""            else:
                tile={'warp_pack':1,'reuse2':2,'reuse4':4,'reuse8':8}[name]"""
b="""            elif name.startswith('stage'):
                _,threads,split=name.split('_')
                component.dispatch(k[name],(count*48,1+int(split)),int(threads),args)
            else:
                tile=1"""
assert text.count(a)==1
text=text.replace(a,b).replace("[(0,(1,33,273,1024)),(42,(1023,1024))]","[(0,(1,3,17,31)),(42,(1,17,31))]")
exec(compile(text,str(source),"exec"),{"__file__":str(source),"__name__":"__main__"})
