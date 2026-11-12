import re,sys,subprocess
from pathlib import Path
S=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments/EXP-2026-11-08-180-goose-cuda-mmq-q8-1/source')
def body(g,m):
    at=m.end();depth=1
    while depth:
        depth+=(g[at]=='{')-(g[at]=='}');at+=1
    return g[m.start():at]
def extract(g,prefix):
    qual=lambda s:re.sub(r'\b(gg_)',lambda m:prefix+m[1],s)
    funcs=['static void gg_fail(const char*){asm("trap;");}']
    for m in re.finditer(r'(?m)^static [^\n{]+?\b(gg_function_\d+|gg_div|gg_mod|gg_udiv|gg_umod|gg_double_bits)\([^;{}]*\)\s*\{',g):
        if m[1]=='gg_function_0': continue
        funcs.append(body(g,m))
    text=qual('\n'.join(x[:x.index('{')].rstrip()+';' for x in funcs)+'\n'+'\n'.join(funcs))
    text=re.sub(r'(?m)^static ','static __device__ __forceinline__ ',text)
    text=re.sub(r'\bgg_(?!fail_)','fg180qdx_',text)
    return '#pragma once\n'+text+'\n'
src=Path(sys.argv[1]).read_text()
outdir=Path(sys.argv[2]); outdir.mkdir(exist_ok=True)
(outdir/'device-functions-generated.cuh').write_text(extract(src,'fg180qd_'))
print('wrote',outdir/'device-functions-generated.cuh')
