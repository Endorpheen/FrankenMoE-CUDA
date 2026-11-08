from pathlib import Path
import subprocess,json,shutil,hashlib,os,shlex
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');O=A/'helpers/omo5';S=A/'source/cpu-repack';B=A/'build/cpu-repack';R=A/'variants/cpu-backend/run';U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');B.mkdir(exist_ok=True);records=[]
if not S.exists():shutil.copytree(O/'worker-6/cpu-final',S)
def run(label,args,cwd=S,env=None):
 p=subprocess.run([str(x) for x in args],cwd=cwd,env=env,capture_output=True,text=True,timeout=180);records.append({'label':label,'argv':[str(x) for x in args],'cwd':str(cwd),'exit_code':p.returncode});(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n');(B/(label+'.stdout')).write_text(p.stdout);(B/(label+'.stderr')).write_text(p.stderr);print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-6000:]);raise SystemExit(p.returncode)
 return p
records=json.loads((B/'commands.json').read_text())
T=B/'run'
row=next(x for x in records if x['label']=='repack-cohort-link');out=Path(row['argv'][row['argv'].index('-o')+1])
p=S/'normal-backend-fixture.c';t=p.read_text().replace('#include "ggml-cpu/repack.h"','extern ggml_backend_buffer_type_t fg153_repack_buft(void);').replace('ggml_backend_cpu_repack_buffer_type()','fg153_repack_buft()');p.write_text(t)
(S/'fixture-buft.cpp').write_text('#include "ggml-cpu/repack.h"\nextern "C" ggml_backend_buffer_type_t fg153_repack_buft(void) {return ggml_backend_cpu_repack_buffer_type();}\n')
run('repack-buft-bridge',['/usr/bin/g++-13','-std=c++17','-O1','-I'+str(U/'ggml/include'),'-I'+str(U/'ggml/src'),'-c',S/'fixture-buft.cpp','-o',B/'fixture-buft.o'])
run('repack-fixture-build',['/usr/bin/gcc-13','-std=c11','-D_GNU_SOURCE','-O1','-I'+str(U/'ggml/include'),'-I'+str(U/'ggml/src'),p,B/'fixture-buft.o','-L'+str(T),'-Wl,-rpath,'+str(T),'-lggml-cpu','-lggml-base','-lm','-lpthread','-o',B/'normal-backend-fixture'])
arms=[]
for flag in ['0','1']:
 env=os.environ.copy();env.update(LD_LIBRARY_PATH=str(T)+':'+str(R),GGML_GOOSE_CPU_REPACK=flag);env.pop('FG006_REPACK_ROLLBACK',None);dest=B/('backend-'+flag+'.jsonl');p=run('repack-backend-'+flag,[B/'normal-backend-fixture',dest],env=env);items=[json.loads(x) for x in dest.read_text().splitlines()];assert items[-1]['failures']==0;arms.append({x['name']:x['hash'] for x in items if 'name' in x})
assert arms[0]==arms[1],[(k,arms[0].get(k),v) for k,v in arms[1].items() if arms[0].get(k)!=v]
err=(B/'repack-backend-1.stderr').read_text();assert all('FG006_REPACK mode='+str(i) in err for i in [0,1,2,3]),err[-3500:]
summary={'status':'ACCEPTED_SOURCE_LINKED_Q4K_REPACK_NUMERICS','library':str(out),'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'normal_backend_cases':len(arms[0]),'bit_exact':True,'source':str(S),'default_on':True,'rollback':'GGML_GOOSE_CPU_REPACK=0','provider_control_disabled_for_test':False,'preload':False,'production_changed':False,'model_loads':0,'remaining':'Repack provider/traits control closure and full Batch A Apex gate'};(A/'results/accepted-cpu-repack.json').write_text(json.dumps(summary,indent=2)+'\n')
for name in ['libggml-cpu.so','libggml-cpu.so.0','libggml-cpu.so.0.27.0']:
 q=R/name;assert q.is_symlink();q.unlink();q.symlink_to(out.name)
print('PASS real CPU_REPACK source integration;normal graph exact native/Goose;full backend control retained.')
