from pathlib import Path
import json,subprocess,shutil,re,hashlib
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/cpu-backend';v=r/'variants/cpu-backend';build=v/'build';run=v/'run';run.mkdir(exist_ok=True);old=r/'variants/tiled-all-apex-perf';commands=[];rows=json.loads((old/'build/commands.json').read_text());corecmd=next(x['argv'] for x in rows if x['label']=='core');linkrow=next(x for x in rows if x['label']=='link');cwd=Path(linkrow.get('cwd','/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda'));shutil.copy2(s/'bridge.h',s/'fg143-bridge.h');public=set(json.loads((s/'native-ops-symbols.json').read_text()))-{'ggml_compute_forward_mul_mat'}
def run_cmd(label,cmd,work=r):
 p=subprocess.run(cmd,cwd=work,capture_output=True,text=True);(build/(label+'.log')).write_text(p.stdout+p.stderr);commands.append({'label':label,'cwd':str(work),'argv':cmd});print(label,p.returncode,(p.stdout+p.stderr)[-1800:],flush=True)
 if p.returncode:raise SystemExit(p.returncode)
 return p
core=(r/'source/cpu-tiled-all/ggml-cpu-apex-tiled.c').read_text();core=core.replace('#include "bridge.h"','#include "'+str(r/'source/cpu-tiled-all/bridge.h')+'"',1)
core='''struct ggml_tensor;struct ggml_compute_params;
#include <stddef.h>
#include <stdint.h>
extern uint8_t fg143_try(const struct ggml_compute_params*,struct ggml_tensor*);
extern size_t fg143_extra_workspace(const struct ggml_tensor*,int);
'''+core
core='#include <stdbool.h>\nstruct ggml_cgraph;struct ggml_cplan;struct ggml_threadpool;struct ggml_tensor;struct ggml_compute_params;\n#include "'+str(s/'control/declarations.h')+'"\n'+core
dispatch='static void ggml_compute_forward(struct ggml_compute_params * params, struct ggml_tensor * tensor) {'
assert core.count(dispatch)==1
core=core.replace(dispatch,dispatch+'\n    if(fg143_control_is_enabled()){fg143_control_dispatch(params,tensor);return;}',1)
tasks='static int ggml_get_n_tasks(struct ggml_tensor * node, int n_threads) {'
assert core.count(tasks)==1
core=core.replace(tasks,tasks+'\n    if(fg143_control_is_enabled())return fg143_control_tasks(node,n_threads);',1)
core,n=re.subn(r'(struct ggml_cplan ggml_graph_plan\(.*?\)\s*\{)',lambda m:m.group(1)+'\n    if(fg143_control_is_enabled())return fg143_control_plan(cgraph,n_threads,threadpool);',core,count=1,flags=re.S)
assert n==1
core=core.replace(tasks,(s/'control/services.c').read_text()+'\n'+tasks,1)
core,n=re.subn(r'(enum ggml_status ggml_graph_compute\(struct ggml_cgraph \* cgraph, struct ggml_cplan \* cplan\) \{)',lambda m:(s/'control/job-services.c').read_text()+'\n'+m.group(1),core,count=1)
assert n==1
job_needle='    ggml_cpu_init();\n\n    GGML_ASSERT(cplan);'
assert core.count(job_needle)==1
core=core.replace(job_needle,'    ggml_cpu_init();\n    if(fg143_control_is_enabled()&&fg143_control_supports_jobs())return fg143_control_graph_compute(cgraph,cplan);\n\n    GGML_ASSERT(cplan);',1)
pattern=r'(void ggml_compute_forward_mul_mat\(.*?\)\s*\{)';core,n=re.subn(pattern,lambda m:m.group(1)+'\n    if(fg143_try(params,dst))return;',core,count=1,flags=re.S);assert n==1
oldid='static void ggml_compute_forward_mul_mat_id(const struct ggml_compute_params*params,struct ggml_tensor*dst){';assert core.count(oldid)==1;core=core.replace(oldid,oldid+'if(fg143_try(params,dst))return;',1)
needle='        work_size = MAX(work_size, cur);';assert core.count(needle)==1;core=core.replace(needle,'        cur=MAX(cur,fg143_extra_workspace(node,n_tasks));\n'+needle,1);service=(s/'executor/services.c').read_text();marker='static thread_ret_t ggml_graph_compute_thread(void * data) {';assert core.count(marker)==1;core=core.replace(marker,service+'\n'+marker+'\n    if(fg143_executor_enabled)return fg143_run_executor(data);',1);(s/'ggml-cpu-integrated.c').write_text(core)
cmd=list(corecmd);at=cmd.index('-c')+1;cmd[at]=str(s/'ggml-cpu-integrated.c');cmd[cmd.index('-o')+1]=str(build/'core.o');run_cmd('core',cmd,cwd)
unitcmd=json.loads((build/'unit-command.json').read_text());cmd=[x for x in unitcmd if x!='-shared'];cmd.insert(cmd.index(str(s/'unit.c')),'-c');cmd[cmd.index('-o')+1]=str(build/'goose-cpu.o');run_cmd('goose-cpu',cmd)
cmd=['/usr/bin/g++-13','-O3','-march=native','-fPIC','-std=gnu++17','-I'+str(r/'source/ggml/include'),'-I'+str(r/'source/ggml/src'),'-I'+str(r/'source/ggml/src/ggml-cpu'),'-I'+str(s),'-c',str(s/'native-bridge.cpp'),'-o',str(build/'cpu-bridge.o')];run_cmd('cpu-bridge',cmd)
executor_cmd=json.loads((build/'executor-command.json').read_text());run_cmd('executor',executor_cmd)
stateful_cmd=json.loads((build/'stateful-command.json').read_text());run_cmd('stateful',stateful_cmd)
control_cmd=json.loads((build/'control-command.json').read_text());run_cmd('control',control_cmd)
for backend_label in ['backend-goose','backend-cpp']:
 backend_recipe=json.loads((build/(backend_label+'-command.json')).read_text());run_cmd(backend_label,backend_recipe['argv'],backend_recipe['cwd'])
newlink=[];found=set();renames=[]
for arg in linkrow['argv']:
 if arg==str(old/'run/libggml-cpu.so.0.27.0'):arg=str(run/'libggml-cpu.so.0.27.0')
 elif arg==str(old/'build/core.o'):arg=str(build/'core.o')
 elif arg.endswith('/ggml-cpu/ggml-cpu.cpp.o'):arg=str(build/'backend-cpp.o')
 elif arg.endswith('.o'):
  path=Path(arg);path=path if path.is_absolute() else cwd/path
  nm=subprocess.run(['/usr/bin/nm','--defined-only','-g',str(path)],capture_output=True,text=True,check=True);defined={line.split()[-1] for line in nm.stdout.splitlines() if len(line.split())>=3};match=public&defined
  if match:
   destination=build/('preserved-native-'+str(len(renames))+'.o');mapping=build/('rename-'+str(len(renames))+'.txt');mapping.write_text(''.join(name+' fg143_native_'+name+'\n' for name in sorted(match)));run_cmd('rename-'+str(len(renames)),['/usr/bin/objcopy','--redefine-syms='+str(mapping),str(path),str(destination)]);renames.append({'source':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'symbols':sorted(match),'output':str(destination)});found.update(match);arg=str(destination)
 if arg.startswith('-Wl,-rpath,'):arg='-Wl,-rpath,$ORIGIN'
 newlink.append(arg)
assert found==public,('unresolved declared entrypoints',sorted(public-found));newlink += [str(build/'goose-cpu.o'),str(build/'cpu-bridge.o'),str(build/'executor.o'),str(build/'stateful.o'),str(build/'control.o'),str(build/'backend-goose.o')];run_cmd('link',newlink,cwd)
for p in (old/'run').iterdir():
 if p.name.startswith('libggml-cpu.so'):continue
 q=run/p.name
 if not q.exists() and not q.is_symlink():q.symlink_to(p)
for suffix in ['.so','.so.0']:
 p=run/('libggml-cpu'+suffix)
 if not p.exists() and not p.is_symlink():p.symlink_to('libggml-cpu.so.0.27.0')
(build/'integration-commands.json').write_text(json.dumps(commands,indent=2)+'\n');(build/'renamed-native-symbols.json').write_text(json.dumps(renames,indent=2)+'\n');(build/'integration-manifest.json').write_text(json.dumps({'status':'INTEGRATED_BUILD_CORRECTNESS_AND_SAFETY_GATE_PENDING','cpu_sha256':hashlib.sha256((run/'libggml-cpu.so.0.27.0').read_bytes()).hexdigest(),'wrapped_operator_entries':len(found),'native_fallback':'Present,explicitly counted; not qualified as complete B1','producer':json.loads((s/'producer.json').read_text()),'working_runtime_changed':False,'model_loaded':False,'microbenchmarks':False,'requirements_closed':0},indent=2)+'\n');print('One isolated common CPU candidate linked; full B1 qualification still pending',flush=True)
