from pathlib import Path
import json,subprocess,os,hashlib
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');R=A/'variants/cpu-backend/run';records=[]
def run(label,args,cwd=A,env=None):
    p=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=120)
    records.append({'label':label,'argv':args,'cwd':str(cwd),'exit_code':p.returncode})
    (A/'build/model-integration/recheck-commands.json').write_text(json.dumps(records,indent=2)+'\n')
    (A/'results'/f'{label}.stdout').write_text(p.stdout);(A/'results'/f'{label}.stderr').write_text(p.stderr)
    print(label,p.returncode,flush=True)
    if p.returncode:print((p.stdout+p.stderr)[-4500:]);raise SystemExit(p.returncode)
    return p
rows=json.loads((A/'build/cohort-integration-commands.json').read_text())
row=next(r for r in rows if r['label']=='cohort-core');run('model-shape-fix-core',row['argv'],Path(row['cwd']))
row=next(r for r in rows if r['label']=='cohort-link');args=row['argv'].copy();out=R/'libggml-base-unified-v2.so.0.27.0';args[args.index('-o')+1]=str(out);run('model-shape-fix-link',args,Path(row['cwd']))
for name in ['libggml-base.so','libggml-base.so.0','libggml-base.so.0.27.0']:
    p=R/name;assert p.is_symlink();p.unlink();p.symlink_to(out.name)
env=os.environ.copy();env['LD_LIBRARY_PATH']=str(R)
run('model-shape-fix-dlopen',['python3','-c','import ctypes,os;ctypes.CDLL('+repr(str(R/'libllama.so'))+',mode=os.RTLD_NOW);print("PASS merged DSO symbol resolution")'],env=env)
p=run('model-shape-fix-graph',[str(A/'build/model-integration/graph-differential')],env=env)
summary={'status':'PASS_MERGED_APEX_MODEL_GRAPH_LOCAL','llama_library':str((R/'libllama.so').resolve()),'ggml_library':str(out),'llama_sha256':hashlib.sha256((R/'libllama.so').read_bytes()).hexdigest(),'ggml_sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'graph_fixture_stdout':p.stdout,'model_requests':0,'inference_status':'PENDING_COMPLETE_BATCH_A','correctness_fix':'unary shape projection now permits absent second tensor and uses the common typed shape ABI','residual_boundaries':['hybrid KV/recurrent context lifecycle','owned native storage/container imports','CUDA numerical backend deferred to batch B'],'production_changed':False,'compiler_changed':False,'rollback_env':'FG153_APEX_MODEL=0','parent_llama_library':'/home/end0/AI/LlamaServer/franken-goose-exp151-cache-lifecycle-20261107/variants/cpu-backend/run/libllama-control.so.0.7.0'}
(A/'results/model-cohort-integration.json').write_text(json.dumps(summary,indent=2)+'\n')
print('PASS model graph subsystem merged with Goose GGML core; no model execution.',flush=True)
