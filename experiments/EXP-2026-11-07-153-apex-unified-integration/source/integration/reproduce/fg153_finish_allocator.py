from pathlib import Path
import json,subprocess,os,hashlib
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');D=A/'source/allocator-closure';B=A/'build/allocator-closure';T=B/'run';R=A/'variants/cpu-backend/run';U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');records=[]
def run(label,args,env=None):
 p=subprocess.run(args,env=env,capture_output=True,text=True,timeout=90)
 records.append({'label':label,'argv':args,'exit_code':p.returncode,'stdout':p.stdout,'stderr':p.stderr});(B/'acceptance-corpus.json').write_text(json.dumps(records,indent=2)+'\n');print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-4500:]);raise SystemExit(p.returncode)
 return p
incs=['-I'+str(D),'-I'+str(U/'ggml/include'),'-I'+str(U/'ggml/src')];link=['-L'+str(T),'-Wl,-rpath,'+str(T),'-lggml-base','-lggml-cpu']
run('actual-closure-failure-corpus-build',['/usr/bin/gcc-13','-std=c11','-D_GNU_SOURCE','-O2',*incs,str(D/'allocator-closure-fixture.c'),*link,'-o',str(B/'full-fixture')])
probe=D/'public-reserve-fixture.c';probe.write_text('''#include "ggml.h"
#include "ggml-alloc.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
#include <stdio.h>
int main(void) {
 struct ggml_context * c=ggml_init((struct ggml_init_params){.mem_size=4*1024*1024,.no_alloc=true});
 if(!c)return 1;
 struct ggml_tensor *x=ggml_new_tensor_2d(c,GGML_TYPE_F32,32,8);
 ggml_set_input(x);
 struct ggml_tensor *z=ggml_scale(c,x,0.5f);ggml_set_output(z);
 struct ggml_cgraph *g=ggml_new_graph(c);ggml_build_forward_expand(g,z);
 ggml_gallocr_t a=ggml_gallocr_new(ggml_backend_cpu_buffer_type());size_t size=0;
 ggml_gallocr_reserve_n_size(a,g,NULL,NULL,&size);
 printf("PUBLIC_RESERVE bytes=%zu\\n",size);
 ggml_gallocr_free(a);ggml_free(c);return size?0:2;
}
''')
run('public-reserve-witness-build',['/usr/bin/gcc-13','-std=c11','-O2',*incs,str(probe),*link,'-o',str(B/'public-reserve-fixture')])
arms=[]
for flag in ['0','1']:
 env=os.environ.copy();env.update(LD_LIBRARY_PATH=str(T)+':'+str(R),GGML_GOOSE_ALLOC_CLOSURE=flag)
 failure=run('actual-closure-corpus-'+flag,[str(B/'full-fixture')],env)
 public=run('public-reserve-witness-'+flag,[str(B/'public-reserve-fixture')],env)
 arms.append({'flag':flag,'failure_stdout':failure.stdout,'public_stdout':public.stdout,'public_witness':public.stderr})
assert arms[0]['failure_stdout']==arms[1]['failure_stdout']
assert arms[0]['public_stdout']==arms[1]['public_stdout']
assert 'FG153_ALLOC_CLOSURE mode=0' not in arms[0]['public_witness']
assert 'FG153_ALLOC_CLOSURE mode=0 calls=1' in arms[1]['public_witness']
out=(T/'libggml-base.so').resolve();summary={'status':'ACCEPTED_ALLOCATOR_CLOSURE_LOCAL_COHORT','library':str(out),'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'source_worker':8,'source_scope':'remaining native ggml_gallocr_reserve_n_size aggregation','actual_abi_static_asserts':10,'results':arms,'public_api_liveness':'new Goose entry witnessed exactly once only in default-on public call','model_requests':0,'production_changed':False,'target_requirement':'APX015 remains pending complete first-batch gate','rollback':'GGML_GOOSE_ALLOC_CLOSURE=0'}
(A/'results/accepted-allocator-closure.json').write_text(json.dumps(summary,indent=2)+'\n')
for name in ['libggml-base.so','libggml-base.so.0','libggml-base.so.0.27.0']:
 p=R/name;assert p.is_symlink();p.unlink();p.symlink_to(out.name)
print('Accepted worker8 closure: real public hook, differential failure corpus, ABI; no model run.')
