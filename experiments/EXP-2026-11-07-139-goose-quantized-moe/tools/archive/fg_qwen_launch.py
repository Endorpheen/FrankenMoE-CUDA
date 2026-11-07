from pathlib import Path
import os,sys,json
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');mode=sys.argv[1];profile=json.loads(Path('/home/end0/AI/LlamaServer/franken-qwen38-v31/launch-profile.json').read_text());a=profile['argv'];remove={'-md':1,'--spec-type':1,'--spec-draft-n-max':1,'-ngld':1,'--spec-draft-cpu-moe':0,'-ctkd':1,'-ctvd':1};out=[];i=0
while i<len(a):
 if a[i] in remove:i+=1+remove[a[i]]
 else:out.append(a[i]);i+=1
a=out;a[a.index('-c')+1]='4096';a[a.index('--ctx-checkpoints')+1]='0';a+=['--spec-type','none','--no-warmup'];env=dict(os.environ);env.pop('LD_PRELOAD',None)
if mode.startswith('qwen-goose'):
 bundle=r/'variants/qwen839a-goose/run';a[0]=str(bundle/'llama-server');env['LD_LIBRARY_PATH']=str(bundle)
 env['GGML_GOOSE_ALLOC']='1';env['GGML_GOOSE_COALESCE']='1';env['GGML_GOOSE_LIFETIME']='0';env['GGML_GOOSE_GRAPH_PLAN']='0' if mode.endswith('-off') else '1';env['GGML_GOOSE_ALLOC_VERIFY']='1' if 'trace' in mode else '0'
elif mode.startswith('qwen-shared'):
 bundle=r/'variants/qwen839a-shared/build/bin';a[0]=str(bundle/'llama-server');env['LD_LIBRARY_PATH']=str(bundle)
else:env.pop('LD_LIBRARY_PATH',None)
(r/'results/current-pid.txt').write_text(str(os.getpid()));(r/'results/current-mode.txt').write_text(mode);(r/'results'/f'{mode}-argv.json').write_text(json.dumps(a,indent=2));(r/'results'/f'{mode}-environment.json').write_text(json.dumps({'LD_LIBRARY_PATH':env.get('LD_LIBRARY_PATH'),'LD_PRELOAD':env.get('LD_PRELOAD'),'GGML_GOOSE_GRAPH_PLAN':env.get('GGML_GOOSE_GRAPH_PLAN'),'GGML_GOOSE_ALLOC_VERIFY':env.get('GGML_GOOSE_ALLOC_VERIFY'),'pid':os.getpid()},indent=2));log=os.open(r/'results'/f'{mode}-server.log',os.O_CREAT|os.O_WRONLY|os.O_TRUNC,0o600);os.dup2(log,1);os.dup2(log,2);os.execvpe(a[0],a,env)
