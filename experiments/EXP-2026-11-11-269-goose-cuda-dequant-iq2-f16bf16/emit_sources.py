from pathlib import Path
import re,json,hashlib,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E=F/'experiments/EXP-2026-11-11-269-goose-cuda-dequant-iq2-f16bf16'
S=E/'source'
pin=json.loads((F/'runtime/compiler/current.json').read_text())
C=Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest()==pin['sha256'],'compiler pin mismatch'

def body(g,m):
    at=m.end();depth=1
    while depth:
        depth+=(g[at]=='{')-(g[at]=='}');at+=1
    return g[m.start():at]

out=S/'device-generated.c'
q=subprocess.run([str(C),'-o',str(out),str(S/'device.goose')],capture_output=True,text=True)
assert q.returncode==0,q.stderr
g=out.read_text()
prefix='fg269dd_'
qual=lambda s:re.sub(r'\b(gg_)',lambda m:prefix+m[1],s)
main=g[g.rfind('static void gg_function_0('):]
entries=[x for x in re.findall(r'\b(gg_function_\d+)\(',main) if x!='gg_function_0']
assert len(entries)==4,entries
names=["q2_xxs_f16","q2_xxs_bf16","q2_xs_f16","q2_xs_bf16"]
funcs=['static void gg_fail(const char*){asm("trap;");}']
for m in re.finditer(r'(?m)^static [^\n{]+?\b(gg_function_\d+|gg_div|gg_mod|gg_udiv|gg_umod|gg_double_bits)\([^;{}]*\)\s*\{',g):
    if m[1]=='gg_function_0':
        continue
    funcs.append(body(g,m))
text=qual('\n'.join(x[:x.index('{')].rstrip()+';' for x in funcs)+'\n'+'\n'.join(funcs))
text=re.sub(r'(?m)^static ','static __device__ __forceinline__ ',text)
text=re.sub(r'\bgg_(?!fail_)','fg269ddx_',text)
assert not any(x in text for x in ['gg_get_stack(','gg_allocate(','gg_append(','gg_ref(']),'stack allocator reachable'
(S/'device-functions-generated.cuh').write_text('#pragma once\n'+text+'\n')
(S/'entries.h').write_text('#pragma once\n'+''.join(f'#define FG269D_CB_{n} {prefix}{e}\n' for n,e in zip(names,entries)))
(E/'emission.json').write_text(json.dumps(dict(
    status='PASS_CURRENT_SELFHOSTED_SOURCE_AND_MECHANICAL_EXTRACTION',
    compiler=pin,entries=dict(zip(names,entries)),
    generated_c_sha256=hashlib.sha256(g.encode()).hexdigest()),indent=2)+'\n')
print('PASS iq2 f16/bf16 emission',entries)
