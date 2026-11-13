from pathlib import Path
import re,json,hashlib,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E=F/'experiments/EXP-2026-11-09-208-goose-mmq-typed-x'
S=E/'source'
# EXP208 uses the typed-reference subset build explicitly; the runtime pin stays where it is.
C=Path('/home/end0/AI/Fried-Goose/.cache/selfhosted-builds/versions/subset-acc916a43628499a/compiler')
pin=dict(compiler=str(C), sha256=hashlib.sha256(C.read_bytes()).hexdigest())
assert pin['sha256'].startswith('acc916a43628499a'), pin['sha256']

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
    prefix='fg208qs_' if name=='host' else 'fg208qd_'
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
        ffi+=['uint8_t fg180qh_run(int64_t);','#ifdef __cplusplus','}','#endif']
        (S/'host-ffi.h').write_text('\n'.join(ffi)+'\n')
        (S/'host-qualified.c').write_text(qual(g))
        (S/'host-unit.c').write_text(
            '#include "host-ffi.h"\n#define main fg180qs_generated_main\n#include "host-qualified.c"\n'
            '#undef main\nuint8_t fg180qh_run(int64_t p){return '+prefix+entries[0]+'(p,1);}\n')
    else:
        # Device wrappers: main calls all eight in the order the Goose source
        # writes them, so this list is the parent's dispatch mapping. Every
        # callee reachable from those wrappers is extracted too, because the C
        # backend numbers functions by emission and does not keep their names.
        # fg180q_mmq_ds4_direct then fg180q_mmq_ds4_scatter
        # two wrappers: ds4_direct and ds4_scatter
        assert len(entries)==2,entries
        assert len(set(entries))==2,entries
        funcs=['static void gg_fail(const char*){asm("trap;");}']
        for m in re.finditer(r'(?m)^static [^\n{]+?\b(gg_function_\d+|gg_div|gg_mod|gg_udiv|gg_umod|gg_double_bits)\([^;{}]*\)\s*\{',g):
            if m[1]=='gg_function_0':
                continue
            funcs.append(body(g,m))
        text=qual('\n'.join(x[:x.index('{')].rstrip()+';' for x in funcs)+'\n'+'\n'.join(funcs))
        text=re.sub(r'(?m)^static ','static __device__ __forceinline__ ',text)
        # _Alignof is C11; the device build is C++, so use the C++ spelling. The generated
        # alignment contract itself is unchanged -- only how the compiler is told to express it.
        text=text.replace('_Alignof(', 'alignof(')
        # gg_fail is the emitted runtime trap. It stays inlined: an out-of-line
        # call made the compiler materialise a frame at every ensure() site.
        # Internally generated helper names (gg_div/gg_fail/gg_function_N) must not
        # collide with other Goose modules linked into the same DSO. The C backend
        # emits gg_* names for both user externs and its own runtime helpers, so
        # rename every remaining gg_ identifier with this module's own prefix.
        text=re.sub(r'\bgg_(?!fail_)','fg180qdx_',text)
        assert not any(x in text for x in ['gg_get_stack(','gg_allocate(','gg_append(','gg_ref(']),'stack allocator reachable'
        # The typed address contract: the generated const-reference typedefs, the scalar
        # typedefs they point at, and the alignment macros. Without these the emitted bodies
        # do not compile and, worse, a hand-written pointer type would silently disagree.
        macro_start=g.rindex('#if defined(_MSC_VER)')
        macro_end=g.index('/* Fried Goose self-hosted C backend. */')
        macros=g[macro_start:macro_end]
        types=[l for l in g.splitlines()
               if l.startswith('typedef ') and ('gg_unaligned_' in l or 'gg_type_' in l)]
        assert any('const GG_UNALIGNED' in x for x in types),'no const reference typedef'
        # typedefs must carry the same module prefix as the bodies that use them
        types_p=[qual(x) for x in types]
        # Two files on purpose: the alignment macros and the typed address typedefs must be
        # visible before the loader primitives that take those types, while the function bodies
        # must come after the primitives they call. Both are the emitted text, unchanged.
        (S/'device-types-generated.h').write_text(
            '#pragma once\n'+macros+'\n'+'\n'.join(types_p)+'\n')
        (S/'device-functions-generated.cuh').write_text(
            '#pragma once\n#include "device-types-generated.h"\n'+text+'\n')
    rows.append(dict(module=name,entries=entries,
                     generated_c_sha256=hashlib.sha256(g.encode()).hexdigest()))
(E/'emission.json').write_text(json.dumps(dict(
    status='PASS_CURRENT_SELFHOSTED_SOURCE_AND_MECHANICAL_EXTRACTION',
    compiler=pin,modules=rows),indent=2)+'\n')
print('PASS host/device actual selfhosted emission and mechanical device extraction')
