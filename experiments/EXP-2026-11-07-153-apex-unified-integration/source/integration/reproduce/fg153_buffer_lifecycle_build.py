from pathlib import Path
import subprocess,json,os,re,hashlib
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');D=A/'source/buffer-lifecycle';B=A/'build/buffer-lifecycle';B.mkdir(exist_ok=True);R=A/'variants/cpu-backend/run';records=[]
def run(label,args,cwd=A,env=None):
 p=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=120);records.append({'label':label,'argv':args,'cwd':str(cwd),'exit_code':p.returncode});(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n');(A/'results'/f'{label}.stdout').write_text(p.stdout);(A/'results'/f'{label}.stderr').write_text(p.stderr);print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-5000:]);raise SystemExit(p.returncode)
 return p
producer=A/'compiler/stage3-f849233b999c';assert hashlib.sha256(producer.read_bytes()).hexdigest()=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a'
g=A/'generated/buffer-lifecycle.c';run('buffer-lifecycle-emit',[str(producer),'-o',str(g),str(D/'kernel.goose')]);text=g.read_text();main=text[text.rfind('static void gg_function_0('):];matches=re.findall(r'(gg_function_\d+)\(t\d+,\s*t\d+,\s*t\d+,\s*base \+ \d+\)',main);assert len(set(matches))==1,matches
(D/'unit.c').write_text('''#include "bridge.h"
#include <stdatomic.h>
#include <stdio.h>
static atomic_uint_fast64_t calls[10];
void fg153_bl_witness(int64_t mode){if(mode<0||mode>=10)__builtin_trap();atomic_fetch_add_explicit(calls+mode,1,memory_order_relaxed);}
#define main fg153_buffer_lifecycle_unused_main
#include "../../generated/buffer-lifecycle.c"
#undef main
int64_t fg153_buffer_lifecycle(int64_t mode,int64_t a,int64_t b){return ENTRY(mode,a,b,0);}
__attribute__((destructor))static void report(void){for(int i=0;i<10;i++){uint64_t n=atomic_load_explicit(calls+i,memory_order_relaxed);if(n)fprintf(stderr,"FG153_BUFFER_LIFECYCLE mode=%d calls=%llu\\n",i,(unsigned long long)n);}}
'''.replace('ENTRY',matches[0]))
run('buffer-lifecycle-unit',['/usr/bin/gcc-13','-std=c11','-D_GNU_SOURCE','-O3','-DNDEBUG','-fPIC','-fexceptions','-fno-strict-aliasing','-Werror=implicit-function-declaration','-c',str(D/'unit.c'),'-o',str(B/'unit.o')])
rows=json.loads((A/'build/cohort-integration-commands.json').read_text());row=next(r for r in rows if r['label']=='cohort-backend');args=row['argv'].copy();args[args.index('-c')+1]=str(D/'backend.cpp');args[args.index('-o')+1]=str(B/'backend.o');args+=['-I'+str(D),'-fno-strict-aliasing'];run('buffer-lifecycle-backend',args,Path(row['cwd']))
row=next(r for r in rows if r['label']=='cohort-link');args=row['argv'].copy();out=R/'libggml-base-lifecycle.so.0.27.0';args[args.index('-o')+1]=str(out);args=[str(B/'backend.o') if x==str(A/'build/integrated-backend.o') else x for x in args];args+=[str(B/'unit.o'),'-Wl,-z,defs'];run('buffer-lifecycle-link',args,Path(row['cwd']))
T=B/'run';T.mkdir(exist_ok=True)
for f in R.iterdir():
 if f.is_file() or f.is_symlink():
  q=T/f.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise SystemExit('Refusing to replace '+str(q))
  q.symlink_to(out if f.name in ['libggml-base.so','libggml-base.so.0','libggml-base.so.0.27.0'] else f.resolve())
fixture=(A/'source/buffers/fixture.c').read_text();old='if(b){CHECK(measured==ggml_backend_buffer_get_size(b));';new='if(b){ggml_backend_buffer_set_usage(b,GGML_BACKEND_BUFFER_USAGE_WEIGHTS);for(struct ggml_tensor *t=ggml_get_first_tensor(c);t;t=ggml_get_next_tensor(c,t))CHECK(ggml_backend_buffer_get_usage(t->buffer)==GGML_BACKEND_BUFFER_USAGE_WEIGHTS);ggml_backend_buffer_reset(b);CHECK(measured==ggml_backend_buffer_get_size(b));';assert fixture.count(old)==1;fixture=fixture.replace(old,new);(D/'fixture.c').write_text(fixture)
U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');run('buffer-lifecycle-fixture-build',['/usr/bin/gcc-13','-std=c11','-D_GNU_SOURCE','-O1','-I'+str(U/'ggml/include'),'-I'+str(U/'ggml/src'),str(D/'fixture.c'),'-L'+str(T),'-Wl,-rpath,'+str(T),'-lggml-base','-lm','-o',str(B/'fixture')])
results=[]
for mode in ['0','1']:
 env=os.environ.copy();env.update(LD_LIBRARY_PATH=str(T)+':'+str(R),GGML_GOOSE_BUFFERS=mode)
 p=run('buffer-lifecycle-fixture-'+mode,[str(B/'fixture')],env=env);results.append({'mode':mode,'stdout':p.stdout,'witness':p.stderr})
assert results[0]['stdout']==results[1]['stdout'],results
assert 'FG153_BUFFER_LIFECYCLE mode=0' in results[1]['witness']
for mode in [1,2,3,4,5,6,7,8]:assert 'FG153_BUFFER_LIFECYCLE mode='+str(mode) in results[1]['witness'],mode
summary={'status':'PASS_GENERIC_AND_MULTI_BUFFER_GOOSE_LIFECYCLE','library':str(out),'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'results':results,'coverage':['multi-buffer create/clear/free and child ownership','generic free/clear/reset/base','usage propagation to real child allocations','existing OOM/init failure cleanup/retry corpus'],'model_requests':0,'production_changed':False,'rollback':'GGML_GOOSE_BUFFERS=0','residuals':['typed POD buffer header construction','OS/provider callback primitives and native ABI storage']};(A/'results/buffer-lifecycle-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
for name in ['libggml-base.so','libggml-base.so.0','libggml-base.so.0.27.0']:
 p=R/name;assert p.is_symlink();p.unlink();p.symlink_to(out.name)
print('PASS buffer ownership lifecycle integrated; all changed branches witnessed, no model run.')
