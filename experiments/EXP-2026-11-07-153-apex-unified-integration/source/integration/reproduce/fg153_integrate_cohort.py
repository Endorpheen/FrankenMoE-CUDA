from pathlib import Path
import subprocess,json,os,hashlib
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');S=A/'source/integration';S.mkdir(exist_ok=True);records=[]
def run(label,args,cwd=A,env=None):
 p=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=120);(A/'results'/f'{label}.stdout').write_text(p.stdout);(A/'results'/f'{label}.stderr').write_text(p.stderr);records.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=p.returncode));(A/'build/cohort-integration-commands.json').write_text(json.dumps(records,indent=2)+'\n');print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-2200:]);raise SystemExit(p.returncode)
 return p
core=A/'source/tensor-core/merged-ops.c';run('cohort-ops-replay',['python3',str(A/'source/apex-ops/replay.py'),'--input',str(A/'source/tensor-core/ggml-core.c'),'--output',str(core)])
s=core.read_text();hooks=[('bool ggml_is_scalar(const struct ggml_tensor * tensor) {',0,'tensor','NULL',0,4),('bool ggml_is_vector(const struct ggml_tensor * tensor) {',1,'tensor','NULL',0,4),('bool ggml_is_matrix(const struct ggml_tensor * tensor) {',2,'tensor','NULL',0,4),('bool ggml_is_3d(const struct ggml_tensor * tensor) {',3,'tensor','NULL',0,4),('int ggml_n_dims(const struct ggml_tensor * tensor) {',4,'tensor','NULL',0,4),('bool ggml_is_transposed(const struct ggml_tensor * tensor) {',5,'tensor','NULL',0,4),('bool ggml_is_permuted(const struct ggml_tensor * tensor) {',6,'tensor','NULL',0,4),('bool ggml_are_same_shape(const struct ggml_tensor * t0, const struct ggml_tensor * t1) {',7,'t0','t1',0,4),('bool ggml_are_same_stride(const struct ggml_tensor * t0, const struct ggml_tensor * t1) {',8,'t0','t1',0,4),('bool ggml_can_repeat(const struct ggml_tensor * t0, const struct ggml_tensor * t1) {',9,'t0','t1',0,4),('static bool ggml_is_contiguous_m_n(const struct ggml_tensor * tensor, int m, int n) {',10,'tensor','NULL','m','n')]
for sig,mode,a,b,m,n in hooks:
 assert s.count(sig)==1,sig;s=s.replace(sig,sig+f'\n    if(fg_apex_ops_enabled())return fg_apex_ops_shape({mode},(intptr_t){a},(intptr_t){b},{m},{n});',1)
core.write_text(s)
run('cohort-scheduler-replay',['python3',str(A/'source/scheduler/replay.py'),'--base',str(A/'source/buffers/backend.cpp'),'--output',str(S/'backend.cpp')])
rows=json.loads((A/'build/local-commands.json').read_text());row=next(x for x in rows if x['label']=='compile-core');args=row['argv'].copy();args[args.index('-c')+1]=str(core);args[args.index('-o')+1]=str(A/'build/integrated-core.o');run('cohort-core',args)
run('cohort-shapes',['/usr/bin/gcc-13','-O2','-fPIC','-fno-strict-aliasing','-Werror=implicit-function-declaration','-c',str(A/'source/shapes/unit.c'),'-o',str(A/'build/shapes.o')])
rows=json.loads((A/'build/buffer-commands.json').read_text());row=next(x for x in rows if x['label']=='buffers-backend');args=row['argv'].copy();args[args.index('-c')+1]=str(S/'backend.cpp');args[args.index('-o')+1]=str(A/'build/integrated-backend.o');run('cohort-backend',args)
rows=json.loads((A/'build/memory-commands.json').read_text());row=next(x for x in rows if x['label']=='memory-link-base');args=row['argv'].copy();args=[x.replace(str(A/'build/ggml-core.o'),str(A/'build/integrated-core.o')).replace(str(A/'build/backend.o'),str(A/'build/integrated-backend.o')) for x in args];R=A/'variants/cpu-backend/run';out=R/'libggml-base-unified.so.0.27.0';args[args.index('-o')+1]=str(out);args+=[str(A/'build/shapes.o'),str(A/'build/apex-ops/ops.o'),str(A/'build/scheduler/scheduler.o')];run('cohort-link',args,cwd=row['cwd'])
for name in ['libggml-base.so','libggml-base.so.0','libggml-base.so.0.27.0']:
 q=R/name;assert q.is_symlink();q.unlink();q.symlink_to(out.name)
# Combined existing CPU graph/memory corpus exercises merged constructors,scheduler and allocation without loading a model.
baseenv=os.environ.copy();baseenv['LD_LIBRARY_PATH']=str(R);results=[]
for mode in ['0','1']:
 env=baseenv.copy();env.update(GGML_GOOSE_APEX_OPS=mode,GGML_GOOSE_SCHEDULER=mode,GGML_GOOSE_GRAPH_ALLOCATOR=mode,GGML_GOOSE_MEMORY=mode,GGML_GOOSE_BUFFERS=mode,GGML_GOOSE_TENSOR_CORE=mode,GGML_GOOSE_SCHEDULER_TRACE='1')
 for label,exe in [('memory',A/'build/memory-fixture'),('cpu',A/'build/scheduler/cpu-fixture')]:
  p=run(f'cohort-{label}-{mode}',[str(exe)],env=env);results.append(dict(corpus=label,mode=mode,stdout=p.stdout))
for label in ['memory','cpu']:
 a=next(x['stdout'] for x in results if x['corpus']==label and x['mode']=='0');b=next(x['stdout'] for x in results if x['corpus']==label and x['mode']=='1');assert a==b,(label,a,b)
summary=dict(status='PASS_MERGED_GGML_CPU_GRAPH_COHORT_LOCAL',library=str(out),library_sha256=hashlib.sha256(out.read_bytes()).hexdigest(),components=['Goose tensor/context core','reviewed Crouton shape predicates','67Goose GGML constructor/attribute entries','Goose buffer/chunk/graph allocator','Goose CPU/GPU scheduler control'],model_requests=0,gpu_model_gate='PENDING_BATCH_A',apex_model_module='qualified separately;not linked into llama yet',cpu_and_memory_corpora=results,production_changed=False)
(A/'results/cohort-integration-summary.json').write_text(json.dumps(summary,indent=2)+'\n');print('Merged GGML constructor/scheduler/allocator cohort passed local differential checks;Apex batch remains incomplete.')
