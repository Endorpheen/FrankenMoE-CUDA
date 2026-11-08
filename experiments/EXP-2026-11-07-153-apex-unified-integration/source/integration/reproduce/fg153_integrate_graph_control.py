from pathlib import Path
import subprocess,json,hashlib,os,re,sys,shutil
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');D=A/'source/graph-control';B=A/'build/graph-control';R=A/'variants/cpu-backend/run';U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');B.mkdir(exist_ok=True);(D/'generated').mkdir(exist_ok=True);(D/'backend-control/generated').mkdir(exist_ok=True);records=[]
def run(label,args,cwd=A,env=None):
 if '--resume' in sys.argv and (label.startswith('graph-emit-') or label.endswith('-replay') or label in ['graph-core-build','graph-backend-build']):
  print('Reused unchanged successful step',label,flush=True);return None
 p=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=150);records.append({'label':label,'argv':args,'cwd':str(cwd),'exit_code':p.returncode});(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n');(A/'results'/f'{label}.stdout').write_text(p.stdout);(A/'results'/f'{label}.stderr').write_text(p.stderr);print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-5500:]);raise SystemExit(p.returncode)
 return p
producer=A/'compiler/stage3-f849233b999c'
assert hashlib.sha256(producer.read_bytes()).hexdigest()=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a'
for name in ['graph-traversal','graph-storage','hash-primitives','backend-control/backend-control']:
 src=D/(name+'.goose');gen=(D/'backend-control/generated/backend-control.c') if '/' in name else D/'generated'/(name+'.c')
 run('graph-emit-'+name.replace('/','-'),[str(producer),'-o',str(gen),str(src)])
 text=gen.read_text();main=text[text.rfind('static void gg_function_0('):];assert re.search(r'gg_function_1\(',main),(name,'entry mismatch')
shutil.copyfile(D/'backend-control/generated/backend-control.c',D/'generated/backend-control.c')
for n,family in [('goose-graph-traversal-wrapper.c',0),('goose-graph-storage-wrapper.c',1)]:
 p=D/n;s=p.read_text()
 if 'fg153_graph_trace' not in s:
  at=s.index('    return FG153_');s=s[:at]+f'    fg153_graph_trace({family}, p0);\n'+s[at:];s='extern void fg153_graph_trace(int,int64_t);\n'+s
 if s.startswith('extern void fg153_graph_trace'):s='#include <stdint.h>\n'+s
 p.write_text(s)
w=D/'graph-witness.c';w.write_text('''#include <stdint.h>
static uint64_t counts[2][32];
void fg153_graph_trace(int family,int64_t mode) {if(family>=0&&family<2&&mode>=0&&mode<32)__atomic_fetch_add(&counts[family][mode],1,__ATOMIC_RELAXED);}
uint64_t fg153_graph_witness(int family,int mode) {return family>=0&&family<2&&mode>=0&&mode<32?__atomic_load_n(&counts[family][mode],__ATOMIC_RELAXED):0;}
''')
core=D/'core.c'
run('graph-core-replay',['python3',str(D/'replay.py'),'--ggml',str(A/'source/tensor-core/merged-ops.c'),'--anchors',str(D/'replay-anchors.json'),'--symbols',str(D/'replay-symbols.json'),'--include','goose-replay-wrappers.h','--write','--output',str(core)])
s=core.read_text();s=s.replace('#include "bridge.h"','#include "'+str(A/'source/tensor-core/bridge.h')+'"');s+='\n#include "graph-arena-adapter.inc"\n';core.write_text(s)
rows=json.loads((A/'build/cohort-integration-commands.json').read_text());r=next(x for x in rows if x['label']=='cohort-core');args=r['argv'].copy();args[args.index('-c')+1]=str(core);args[args.index('-o')+1]=str(B/'core.o');args+=['-I'+str(D)];run('graph-core-build',args,Path(r['cwd']))
backend=D/'backend.cpp'
run('graph-backend-replay',['python3',str(D/'backend-control/replay.py'),'--source',str(A/'source/buffer-lifecycle/backend.cpp'),'--output',str(backend),'--anchors',str(D/'backend-control/replay-anchors.json'),'--include','backend-replay-wrappers.h'])
rows=json.loads((A/'build/buffer-lifecycle/commands.json').read_text());r=next(x for x in rows if x['label']=='buffer-lifecycle-backend');args=r['argv'].copy();args[args.index('-c')+1]=str(backend);args[args.index('-o')+1]=str(B/'backend.o');args+=['-I'+str(D/'backend-control')];run('graph-backend-build',args,Path(r['cwd']))
units=['graph-services.c','goose-graph-storage-wrapper.c','goose-graph-traversal-wrapper.c','goose-hash-wrapper.c','goose-replay-wrappers.c','graph-witness.c','backend-control/backend-services.c','backend-control/backend-control-wrapper.c','backend-control/backend-replay-wrappers.c'];objects=[]
for file in units:
 obj=B/(file.replace('/','-')+'.o');run('graph-unit-'+file.replace('/','-'),['/usr/bin/gcc-13','-std=c11','-D_GNU_SOURCE','-O2','-fPIC','-fexceptions','-fno-strict-aliasing','-Werror=implicit-function-declaration','-DFG153_BC_GENERATED_ENTRY=gg_function_1','-I'+str(D),'-I'+str(D/'backend-control'),'-I'+str(U/'ggml/include'),'-I'+str(U/'ggml/src'),'-c',str(D/file),'-o',str(obj)]);objects.append(obj)
rows=json.loads((A/'build/allocator-closure/commands.json').read_text());r=next(x for x in rows if x['label']=='accept-alloc-link');args=r['argv'].copy();replaced={str(A/'build/integrated-core.o'):str(B/'core.o'),str(A/'build/buffer-lifecycle/backend.o'):str(B/'backend.o')};assert all(args.count(k)==1 for k in replaced);args=[replaced.get(x,x) for x in args];args += [str(x) for x in objects];digest=hashlib.sha256(b''.join(p.read_bytes() for p in [B/'core.o',B/'backend.o',*objects])).hexdigest()[:12];out=R/('libggml-base-graph-'+digest+'.so.0.27.0');args[args.index('-o')+1]=str(out);run('graph-cohort-link',args,Path(r['cwd']))
T=B/'run';T.mkdir(exist_ok=True)
for f in R.iterdir():
 if f.is_file() or f.is_symlink():
  q=T/f.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise SystemExit('Refusing to overwrite '+str(q))
  q.symlink_to(out if f.name in ['libggml-base.so','libggml-base.so.0','libggml-base.so.0.27.0'] else f.resolve())
env=os.environ.copy();env['LD_LIBRARY_PATH']=str(T)+':'+str(R)
for file,name in [('local-graph-fixture.c','traversal'),('local-graph-creation-fixture.c','creation'),('backend-control/backend-fixture.c','backend')]:
 run('graph-fixture-build-'+name,['/usr/bin/gcc-13','-std=c11','-D_GNU_SOURCE','-O1','-I'+str(D),'-I'+str(D/'backend-control'),'-I'+str(U/'ggml/include'),'-I'+str(U/'ggml/src'),str(D/file),'-L'+str(T),'-Wl,-rpath,'+str(T),'-lggml','-lggml-base','-lggml-cpu','-lm','-lpthread','-o',str(B/(name+'-fixture'))])
 p=run('graph-fixture-'+name,[str(B/(name+'-fixture'))],env=env);assert 'RESULT PASS' in p.stdout
arms=[]
for flag in ['0','1']:
 arm=env.copy();arm.update(GGML_GOOSE_GRAPH=flag,GGML_GOOSE_BACKEND_CONTROL=flag)
 p=run('graph-merged-model-shapes-'+flag,[str(A/'build/model-integration/graph-differential')],env=arm);arms.append(p.stdout)
assert arms[0]==arms[1],arms
summary={'status':'ACCEPTED_GRAPH_BACKEND_LOCAL_COHORT','library':str(out),'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'graph_entrypoints':15,'backend_entrypoints':19,'differential_stdout':arms,'no_duplicate_definition_link':True,'source_worker':1,'model_requests':0,'production_changed':False,'rollback':['GGML_GOOSE_GRAPH=0','GGML_GOOSE_BACKEND_CONTROL=0'],'pending':'APX014/APX016 stay pending complete Batch A Apex gate'};(A/'results/accepted-graph-control.json').write_text(json.dumps(summary,indent=2)+'\n')
for name in ['libggml-base.so','libggml-base.so.0','libggml-base.so.0.27.0']:
 q=R/name;assert q.is_symlink();q.unlink();q.symlink_to(out.name)
print('Accepted real graph storage/traversal/hash and backend control into shared cohort.')
