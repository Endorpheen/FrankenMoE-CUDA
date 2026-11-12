from pathlib import Path
import re,json,hashlib,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E=F/'experiments/EXP-2026-11-11-252-goose-cuda-dequant-iq4nl'
S=E/'source'
pin=json.loads((F/'runtime/compiler/current.json').read_text())
C=Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest()==pin['sha256'],'compiler pin mismatch'

def body(g,m):
    at=m.end();depth=1
    while depth:
        depth+=(g[at]=='{')-(g[at]=='}');at+=1
    return g[m.start():at]

rows=[]
for name in ['host','device']:
    out=S/(name+'-generated.c')
    q=subprocess.run([str(C),'-o',str(out),str(S/(name+'.goose'))],capture_output=True,text=True)
    assert q.returncode==0,q.stderr
    g=out.read_text()
    prefix='fg252ds_' if name=='host' else 'fg252dd_'
    qual=lambda s:re.sub(r'\b(gg_)',lambda m:prefix+m[1],s)
    main=g[g.rfind('static void gg_function_0('):]
    entries=[x for x in re.findall(r'\b(gg_function_\d+)\(',main) if x!='gg_function_0']
    if name=='host':
        assert len(entries)==1,entries
        ffi=['#pragma once','#include <stdint.h>','#ifdef __cplusplus','extern "C" {','#endif']
        ct={'i64':'int64_t','u64':'uint64_t','u32':'uint32_t','i32':'int32_t','bool':'uint8_t','f32':'float'}
        for m in re.finditer(r'extern "([^"]+)" fn \w+\(([^)]*)\)(?:->(\w+))?;',(S/'host.goose').read_text()):
            ps=[]
            for p in m[2].split(','):
                if p.strip():
                    n,t=p.split(':');ps.append(ct[t.strip()])
            ffi.append((ct[m[3]] if m[3] else 'void')+' '+m[1]+'('+','.join(ps)+');')
        ffi+=['uint8_t fg252dh_run(int64_t);','#ifdef __cplusplus','}','#endif']
        (S/'host-ffi.h').write_text('\n'.join(ffi)+'\n')
        (S/'host-qualified.c').write_text(qual(g))
        (S/'host-unit.c').write_text(
            '#include "host-ffi.h"\n#define main fg252ds_generated_main\n#include "host-qualified.c"\n'
            '#undef main\nuint8_t fg252dh_run(int64_t p){return '+prefix+entries[0]+'(p,1);}\n')
    else:
        # Device entry: main calls q6_K, which is the single entry point.
        assert len(entries)==1,entries
        assert len(set(entries))==1,entries
        funcs=['static void gg_fail(const char*){asm("trap;");}']
        for m in re.finditer(r'(?m)^static [^\n{]+?\b(gg_function_\d+|gg_div|gg_mod|gg_udiv|gg_umod|gg_double_bits)\([^;{}]*\)\s*\{',g):
            if m[1]=='gg_function_0':
                continue
            funcs.append(body(g,m))
        text=qual('\n'.join(x[:x.index('{')].rstrip()+';' for x in funcs)+'\n'+'\n'.join(funcs))
        text=re.sub(r'(?m)^static ','static __device__ __forceinline__ ',text)
        # gg_fail is the emitted runtime trap. It stays inlined: an out-of-line
        # call made the compiler materialise a frame at every ensure() site.
        # Internally generated helper names (gg_div/gg_fail/gg_function_N) must not
        # collide with other Goose modules linked into the same DSO. The C backend
        # emits gg_* names for both user externs and its own runtime helpers, so
        # rename every remaining gg_ identifier with this module's own prefix.
        text=re.sub(r'\bgg_(?!fail_)','fg252ddx_',text)
        assert not any(x in text for x in ['gg_get_stack(','gg_allocate(','gg_append(','gg_ref(']),'stack allocator reachable'
        (S/'device-functions-generated.cuh').write_text('#pragma once\n'+text+'\n')
    rows.append(dict(module=name,entries=entries,
                     generated_c_sha256=hashlib.sha256(g.encode()).hexdigest()))
(E/'emission.json').write_text(json.dumps(dict(
    status='PASS_CURRENT_SELFHOSTED_SOURCE_AND_MECHANICAL_EXTRACTION',
    compiler=pin,modules=rows),indent=2)+'\n')
print('PASS host/device actual selfhosted emission and mechanical device extraction')
