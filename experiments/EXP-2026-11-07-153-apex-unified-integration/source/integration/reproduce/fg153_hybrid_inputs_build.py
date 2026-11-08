from pathlib import Path
import subprocess,json,shlex,re,hashlib
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');D=A/'source/hybrid-inputs';B=A/'build/hybrid-inputs';B.mkdir(exist_ok=True);records=[]
def run(label,args,cwd=A):
 p=subprocess.run(args,cwd=cwd,capture_output=True,text=True,timeout=120);records.append({'label':label,'argv':args,'cwd':str(cwd),'exit_code':p.returncode});(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n');(A/'results'/f'{label}.stdout').write_text(p.stdout);(A/'results'/f'{label}.stderr').write_text(p.stderr);print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-5000:]);raise SystemExit(p.returncode)
 return p
producer=A/'compiler/stage3-f849233b999c';assert hashlib.sha256(producer.read_bytes()).hexdigest()=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a'
g=A/'generated/hybrid-inputs.c';run('hybrid-input-emit',[str(producer),'-o',str(g),str(D/'kernel.goose')]);text=g.read_text();main=text[text.rfind('static void gg_function_0('):];matches=re.findall(r'(gg_function_\d+)\(t\d+,\s*t\d+,\s*base \+ \d+\)',main);assert len(set(matches))==1,matches
(D/'unit.c').write_text('''#include "bridge.h"
#include <stdatomic.h>
#include <stdio.h>
static atomic_uint_fast64_t calls[4];
void fg153_hi_witness(int64_t mode){if(mode<0||mode>=4)__builtin_trap();atomic_fetch_add_explicit(calls+mode,1,memory_order_relaxed);}
#define main fg153_hybrid_inputs_unused_main
#include "../../generated/hybrid-inputs.c"
#undef main
int64_t fg153_hybrid_inputs(int64_t mode,int64_t p){return ENTRY(mode,p,0);}
__attribute__((destructor))static void report(void){for(int i=0;i<4;i++){uint64_t n=atomic_load_explicit(calls+i,memory_order_relaxed);if(n)fprintf(stderr,"FG153_HYBRID_INPUT mode=%d calls=%llu\\n",i,(unsigned long long)n);}}
'''.replace('ENTRY',matches[0]))
inc=['-I'+str(D),'-I'+str(U/'src'),'-I'+str(U/'src/models'),'-I'+str(U/'include'),'-I'+str(U/'ggml/include')]
run('hybrid-input-unit',['/usr/bin/gcc-13','-std=c11','-D_GNU_SOURCE','-O3','-DNDEBUG','-fPIC','-fexceptions','-fno-strict-aliasing','-Werror=implicit-function-declaration',*inc,'-c',str(D/'unit.c'),'-o',str(B/'unit.o')])
recipe=next(r for r in json.loads((U/'build-cuda/compile_commands.json').read_text()) if r['file']==str(U/'src/llama-graph.cpp'))
for module in ['graph','services']:
 args=shlex.split(recipe['command']);args[args.index('-c')+1]=str(D/(module+'.cpp'));args[args.index('-o')+1]=str(B/(module+'.o'));args+=inc;run('hybrid-input-'+module,args,Path(recipe['directory']))
rows=json.loads((A/'build/hybrid-memory/commands.json').read_text());recipe=next(r for r in rows if r['label']=='hybrid-link');args=recipe['argv'].copy();out=A/'variants/cpu-backend/run/libllama-apex-hybrid-inputs-goose.so.0.7.0';args[args.index('-o')+1]=str(out);assert args.count('src/CMakeFiles/llama.dir/llama-graph.cpp.o')==1;args=[str(B/'graph.o') if x=='src/CMakeFiles/llama.dir/llama-graph.cpp.o' else x for x in args];args += [str(B/'unit.o'),str(B/'services.o')];run('hybrid-input-link',args,Path(recipe['cwd']))
(B/'candidate.json').write_text(json.dumps({'library':str(out),'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'entry':matches[0],'model_gate':'PENDING_COMPLETE_BATCH_A','rollback':'GGML_GOOSE_HYBRID_INPUT=0'},indent=2)+'\n');print('New hybrid graph-input module linked; local correctness gate pending.')
