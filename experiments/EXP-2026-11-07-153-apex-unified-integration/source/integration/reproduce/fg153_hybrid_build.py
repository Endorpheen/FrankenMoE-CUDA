from pathlib import Path
import subprocess,json,shlex,re,os,hashlib
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');D=A/'source/hybrid-memory';B=A/'build/hybrid-memory';B.mkdir(exist_ok=True);records=[]
def run(label,args,cwd=A):
    p=subprocess.run(args,cwd=cwd,capture_output=True,text=True,timeout=120)
    records.append({'label':label,'argv':args,'cwd':str(cwd),'exit_code':p.returncode});(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n')
    (A/'results'/f'{label}.stdout').write_text(p.stdout);(A/'results'/f'{label}.stderr').write_text(p.stderr);print(label,p.returncode,flush=True)
    if p.returncode:print((p.stdout+p.stderr)[-5000:]);raise SystemExit(p.returncode)
    return p
compiler=A/'compiler/stage3-f849233b999c';assert hashlib.sha256(compiler.read_bytes()).hexdigest()=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a'
generated=A/'generated/hybrid-memory.c';run('hybrid-emit',[str(compiler),'-o',str(generated),str(D/'kernel.goose')])
text=generated.read_text();main_body=text[text.rfind('static void gg_function_0('):]
matches=re.findall(r'(gg_function_\d+)\(t\d+,\s*t\d+,\s*t\d+,\s*t\d+,\s*t\d+,\s*t\d+,\s*base \+ \d+\)',main_body)
assert len(set(matches))==1,('entry extraction',matches)
entry=matches[0]
(D/'unit.c').write_text('''#include "bridge.h"
#include <stdatomic.h>
#include <stdio.h>
static atomic_uint_fast64_t calls[21];
void fg153_hm_witness(int64_t mode){if(mode<0||mode>=21)__builtin_trap();atomic_fetch_add_explicit(calls+mode,1,memory_order_relaxed);}
#define main fg153_hybrid_unused_main
#include "../../generated/hybrid-memory.c"
#undef main
int64_t fg153_hybrid(int64_t mode,int64_t p,int64_t a,int64_t b,int64_t c,int64_t d){return ENTRY(mode,p,a,b,c,d,0);}
__attribute__((destructor))static void report(void){for(int i=0;i<21;i++){uint64_t n=atomic_load_explicit(calls+i,memory_order_relaxed);if(n)fprintf(stderr,"FG153_HYBRID mode=%d calls=%llu\\n",i,(unsigned long long)n);}}
'''.replace('ENTRY',entry))
inc=['-I'+str(D),'-I'+str(U/'src'),'-I'+str(U/'src/models'),'-I'+str(U/'include'),'-I'+str(U/'ggml/include')]
run('hybrid-unit',['/usr/bin/gcc-13','-std=c11','-D_GNU_SOURCE','-O3','-DNDEBUG','-fPIC','-fexceptions','-fno-strict-aliasing','-Werror=implicit-function-declaration',*inc,'-c',str(D/'unit.c'),'-o',str(B/'unit.o')])
recipe=next(r for r in json.loads((U/'build-cuda/compile_commands.json').read_text()) if r['file']==str(U/'src/llama-memory-hybrid.cpp'))
for module,file in [('hybrid',D/'hybrid.cpp'),('services',D/'services.cpp')]:
    args=shlex.split(recipe['command']);args[args.index('-c')+1]=str(file);args[args.index('-o')+1]=str(B/(module+'.o'));args+=inc
    run('hybrid-'+module,args,cwd=Path(recipe['directory']))
parent=json.loads((A/'build/model-integration/commands.json').read_text());recipe=next(r for r in parent if r['label']=='model-cohort-link');args=recipe['argv'].copy();R=A/'variants/cpu-backend/run';out=R/'libllama-apex-hybrid-goose.so.0.7.0';args[args.index('-o')+1]=str(out)
assert args.count('src/CMakeFiles/llama.dir/llama-memory-hybrid.cpp.o')==1
args=[str(B/'hybrid.o') if a=='src/CMakeFiles/llama.dir/llama-memory-hybrid.cpp.o' else a for a in args];args+=[str(B/'unit.o'),str(B/'services.o')]
run('hybrid-link',args,cwd=Path(recipe['cwd']))
(B/'candidate.json').write_text(json.dumps({'library':str(out),'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'entry':entry,'model_gate':'PENDING_COMPLETE_BATCH_A','rollback':'GGML_GOOSE_HYBRID=0'},indent=2)+'\n')
print('Compiled and linked isolated Goose hybrid policy; local correctness fixture pending.')
