from pathlib import Path
import os,json,sys,hashlib,socket
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');mode=sys.argv[1];assert mode in ['plan-apex-native','plan-apex-exp141'];a=json.loads(Path('/home/end0/AI/FrankenMoE-CUDA/results/exp107-expert-overlap/launch-profiles.json').read_text())['profiles']['apex'];a[a.index('-m')+1]='/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf';bundle=r/('run' if mode.endswith('native') else 'variants/tiled-all-apex-perf/run');a[0]=str(bundle/'llama-server');env=dict(os.environ);env.pop('LD_PRELOAD',None);env['LD_LIBRARY_PATH']=str(bundle)
for k in list(env):
 if k.startswith('GGML_GOOSE_'):env.pop(k)
flags=['GGML_GOOSE_ALLOC','GGML_GOOSE_COALESCE','GGML_GOOSE_LIFETIME','GGML_GOOSE_GRAPH_PLAN','GGML_GOOSE_MOE','GGML_GOOSE_TILED','GGML_GOOSE_TILED_GENERAL','GGML_GOOSE_ELEMENTWISE','GGML_GOOSE_MOE_QUANT','GGML_GOOSE_MOE_CONTROL','GGML_GOOSE_RMS']
for k in flags:env[k]='0' if mode.endswith('native') else '1'
env['GGML_GOOSE_ALLOC_VERIFY']='0';env['GGML_EXPERT_HOTSTORE']='0'
if not mode.endswith('native'):env['LD_PRELOAD']=' '.join(str(r/'build'/x) for x in ['libfranken-goose-silu.so','libfranken-goose-scale.so','libfranken-goose-rms-block.so'])
with socket.socket() as s:assert s.connect_ex(('127.0.0.1',8081))!=0,'Port8081 occupied; do not stop another workload'
(r/'results/current-mode.txt').write_text(mode);(r/'results/current-pid.txt').write_text(str(os.getpid()));(r/'results'/f'{mode}-argv.json').write_text(json.dumps(a,indent=2)+'\n');e={k:v for k,v in env.items() if k.startswith('GGML_') or k in ['LD_LIBRARY_PATH','LD_PRELOAD']};(r/'results'/f'{mode}-environment.json').write_text(json.dumps(e,indent=2)+'\n');files={str(p.resolve()):hashlib.sha256(p.read_bytes()).hexdigest() for p in list(bundle.glob('*.so*'))+[bundle/'llama-server'] if p.is_file()};files.update({str(Path(p).resolve()):hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in env.get('LD_PRELOAD','').split()});(r/'results'/f'{mode}-libraries.json').write_text(json.dumps(files,indent=2)+'\n');log=os.open(r/'results'/f'{mode}-server.log',os.O_CREAT|os.O_WRONLY|os.O_TRUNC,0o600);os.dup2(log,1);os.dup2(log,2);os.execvpe(a[0],a,env)
