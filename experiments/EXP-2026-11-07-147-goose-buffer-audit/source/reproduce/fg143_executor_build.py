from pathlib import Path
import json,subprocess,hashlib
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/cpu-backend/executor';b=r/'variants/cpu-backend/build';producer=json.loads((s.parent/'producer.json').read_text());cmd=[producer['frozen'],'-o',str(r/'generated/cpu-executor.c'),str(s/'kernel.goose')];p=subprocess.run(cmd,capture_output=True,text=True,timeout=90);(s/'generation.log').write_text(p.stdout+p.stderr);print('CPU executor generation',p.returncode,(p.stdout+p.stderr)[-1000:],flush=True)
if p.returncode:raise SystemExit(p.returncode)
unit='''#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
int64_t fg143_ex_meta(int64_t,int64_t,int64_t);
int64_t fg143_ex_abort_value(int64_t);
void fg143_ex_compute(int64_t,int64_t);
void fg143_ex_fused(int64_t,int64_t,int64_t);
uint8_t fg143_ex_abort_requested(int64_t);
void fg143_ex_abort_at(int64_t,int64_t);
void fg143_ex_barrier(int64_t);
static atomic_uint_fast64_t steps,fusions,graphs;
static void fg143_ex_witness(int64_t n,int64_t f){atomic_fetch_add_explicit(&steps,n,memory_order_relaxed);atomic_fetch_add_explicit(&fusions,f,memory_order_relaxed);atomic_fetch_add_explicit(&graphs,1,memory_order_relaxed);}
#define main fg143_executor_unused_main
#include "../../../generated/cpu-executor.c"
#undef main
int64_t fg143_execute_graph(int64_t c,int64_t n,int64_t ith,int64_t nth,int64_t ref,int64_t fuse,int64_t compute,int64_t output){return gg_function_1(c,n,ith,nth,ref,fuse,compute,output,0);}
__attribute__((destructor))static void fg143_executor_report(void){fprintf(stderr,"FG143_EXECUTOR worker_graphs=%llu worker_steps=%llu worker_fusions=%llu\\n",(unsigned long long)atomic_load(&graphs),(unsigned long long)atomic_load(&steps),(unsigned long long)atomic_load(&fusions));}
''';(s/'unit.c').write_text(unit);cmd=['/usr/bin/gcc-13','-O3','-march=native','-fPIC','-fno-strict-aliasing','-ffp-contract=off','-c',str(s/'unit.c'),'-o',str(b/'executor.o')];p=subprocess.run(cmd,capture_output=True,text=True);(b/'executor-build.log').write_text(p.stdout+p.stderr);(b/'executor-command.json').write_text(json.dumps(cmd,indent=2)+'\n');print('CPU executor build',p.returncode,(p.stdout+p.stderr)[-1200:],flush=True)
if p.returncode:raise SystemExit(p.returncode)
(s/'manifest.json').write_text(json.dumps({'status':'GENERATED_BUILT_INTEGRATION_PENDING','producer':producer,'goose_sha256':hashlib.sha256((s/'kernel.goose').read_bytes()).hexdigest(),'scope':'Whole per-worker graph traversal,skip/fusion selection,abort callback order and barriers. Fused arithmetic also Goose. Native OS pool waits/atomic primitives,metadata use-count access and original node dispatcher remain explicit services; not full scheduler/backend closure.','production_changed':False,'compiler_changed':False},indent=2)+'\n')
