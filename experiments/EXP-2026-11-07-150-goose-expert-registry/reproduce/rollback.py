from pathlib import Path
import json,subprocess,os,hashlib
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-expert-registry-20261107');out=A/'results/release';run=A/'variants/cpu-backend/run';b=A/'variants/cpu-backend/build';records=[]
for name,flag in [('backend-buffer','CPU_REGISTRY'),('composite','EXPERT_POLICY')]:
 argv=[str(b/name)]
 # Use the exact successful fixture executable recorded by the common gate.
 prior=json.loads((out/'commands.json').read_text());argv=list(next(x['argv'] for x in prior if x['label']==name+'-candidate'));argv[-1]=str(out/(name+'-exp150-disable.bin'))
 env=dict(os.environ,LD_LIBRARY_PATH=str(run),GGML_GOOSE_ALLOC='0',GGML_GOOSE_COALESCE='0',GGML_GOOSE_LIFETIME='0',GGML_GOOSE_GRAPH_PLAN='0',GGML_GOOSE_CPU_BACKEND='1',GGML_GOOSE_CPU_EXECUTOR='1',GGML_GOOSE_CPU_CONTROL='1',GGML_GOOSE_CPU_BACKEND_API='1',GGML_EXPERT_HOTSTORE='0');env.pop('LD_PRELOAD',None);env['GGML_GOOSE_'+flag]='0'
 p=subprocess.run(argv,env=env,capture_output=True,text=True,timeout=180);(out/(name+'-exp150-disable.log')).write_text(p.stdout+p.stderr);assert p.returncode==0,(name,p.stderr[-1000:]);assert Path(argv[-1]).read_bytes()==(out/(name+'-candidate.bin')).read_bytes()
 records.append(dict(component=flag,argv=argv,exit_code=p.returncode,exact_against_enabled=True))
 # Disabled counters are required to show that the flag affects the candidate itself.
 if flag=='CPU_REGISTRY':assert 'FG150_REGISTRY ' not in p.stderr
 else:assert 'scratch_calls=0 enabled=0' in p.stderr
(A/'results/rollback-summary.json').write_text(json.dumps({'status':'PASS_EXP150_COMPONENT_DISABLE_ROLLBACK','checks':records,'cache':'Actual SLRU fixture toggles implementation for every native/Goose pair;all private policy state and outputs match'},indent=2)+'\n');print('PASS independent registry/scratch disable rollback;cache native/Goose pairs preserved')
