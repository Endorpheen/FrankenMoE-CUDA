from pathlib import Path
import json,subprocess,os
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-expert-registry-20261107');b=A/'variants/hbm-mock/build';run=A/'variants/hbm-mock/run';b.mkdir(parents=True,exist_ok=True);run.mkdir(exist_ok=True);out=A/'results/hbm-mock';out.mkdir(exist_ok=True)
(b/'hbwmalloc.h').write_text('''#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
int hbw_posix_memalign(void**,size_t,size_t);
void hbw_free(void*);
#ifdef __cplusplus
}
#endif
''')
(b/'stub.cpp').write_text('''#include <cstdlib>
#include <cstddef>
#include <atomic>
static std::atomic<int> live{0};static int fail=0;
extern "C" void fg150_hbm_fail(int value){fail=value;}
extern "C" int fg150_hbm_live(){return live.load();}
extern "C" int hbw_posix_memalign(void**p,size_t align,size_t size){if(fail)return 12;int status=posix_memalign(p,align,size?size:1);if(!status)live.fetch_add(1);return status;}
extern "C" void hbw_free(void*p){if(p){live.fetch_sub(1);free(p);}}
''')
(b/'fixture.cpp').write_text('''#include "ggml.h"
#include "ggml-backend.h"
#include "hbm.h"
#include <cstdio>
#include <cstdlib>
extern "C" void fg150_hbm_fail(int);
extern "C" int fg150_hbm_live();
#define CHECK(x) do{if(!(x))abort();}while(0)
int main(){auto type=ggml_backend_cpu_hbm_buffer_type();CHECK(type&&ggml_backend_buft_is_host(type));int cases=0;for(size_t bytes:{size_t(1),size_t(64),size_t(1024),size_t(8192)}){auto*b=ggml_backend_buft_alloc_buffer(type,bytes);CHECK(b&&fg150_hbm_live()==1);ggml_backend_buffer_clear(b,123);auto*p=(unsigned char*)ggml_backend_buffer_get_base(b);for(size_t i=0;i<bytes;i++)CHECK(p[i]==123);ggml_backend_buffer_free(b);CHECK(fg150_hbm_live()==0);cases++;}fg150_hbm_fail(1);CHECK(ggml_backend_buft_alloc_buffer(type,512)==nullptr&&fg150_hbm_live()==0);cases++;printf("{\\"status\\":\\"PASS_COMPILED_HBM_GOOSE_WITH_ALLOCATOR_STUB\\",\\"cases\\":%d,\\"hardware_hbm\\":false}\\n",cases);}
'''.replace('#include <cstdio>','#include <cstdio>\n#include <initializer_list>'))
cmds=[]
def execute(label,argv,cwd=A,env=None):
 p=subprocess.run(argv,cwd=cwd,env=env,capture_output=True,text=True,timeout=180);(out/(label+'.log')).write_text(p.stdout+p.stderr);cmds.append(dict(label=label,argv=argv,cwd=str(cwd),exit_code=p.returncode));(out/'commands.json').write_text(json.dumps(cmds,indent=2)+'\n');print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-2000:]);raise SystemExit(p.returncode)
 return p
recipes=json.loads((A/'variants/cpu-backend/build/commands.json').read_text());row=next(x for x in recipes if x['label']=='backend-cpp');argv=list(row['argv']);argv[argv.index('-o')+1]=str(b/'backend-cpp.o');argv+=['-DGGML_USE_CPU_HBM','-I'+str(b)];execute('compile-hbm-backend',argv,row['cwd'])
common=['/usr/bin/g++-13','-O2','-fPIC','-DGGML_USE_CPU_HBM','-I'+str(b),'-I'+str(A/'source/ggml/include'),'-I'+str(A/'source/ggml/src'),'-I'+str(A/'source/ggml/src/ggml-cpu')]
for name,source in [('hbm',A/'source/cpu-backend/registry/hbm.cpp'),('stub',b/'stub.cpp')]:execute('compile-'+name,common+['-c',str(source),'-o',str(b/(name+'.o'))])
row=next(x for x in recipes if x['label']=='link-cpu');argv=list(row['argv']);argv[argv.index('-o')+1]=str(run/'libggml-cpu.so.0.27.0');argv=[str(b/'backend-cpp.o') if x==str(A/'variants/cpu-backend/build/backend-cpp.o') else x for x in argv];argv+=[str(b/'hbm.o'),str(b/'stub.o')];execute('link-hbm-mock',argv,row['cwd'])
for suffix in ['.so','.so.0']:
 p=run/('libggml-cpu'+suffix)
 if not p.exists():p.symlink_to('libggml-cpu.so.0.27.0')
execute('compile-fixture',common+[str(b/'fixture.cpp'),'-L'+str(run),'-L'+str(A/'variants/cpu-backend/run'),'-lggml-cpu','-lggml-base','-o',str(b/'fixture')])
p=execute('hbm-model-api',[str(b/'fixture')],env=dict(os.environ,LD_LIBRARY_PATH=str(run)+':'+str(A/'variants/cpu-backend/run')));(out/'summary.json').write_text(json.dumps(json.loads(p.stdout),indent=2)+'\n')
