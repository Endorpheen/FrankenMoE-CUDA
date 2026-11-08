from pathlib import Path
import json,subprocess,os,hashlib
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');B=A/'build/common-memory-io';T=B/'run';R=A/'variants/cpu-backend/run';out=(T/'libllama.so').resolve()
assert 'FG153_RESULT failures=0' in (A/'results/common-fixture.stdout').read_text()
env=os.environ.copy();env['LD_LIBRARY_PATH']=str(T)+':'+str(R)
args=['python3','-c','import ctypes;L=ctypes.CDLL('+repr(str(out))+');getattr(L,"_Z27llama_memory_status_combine19llama_memory_statusS_")(0,1);assert L.fg153_common_witness(0)>0;print("PASS actual public status hook reaches Goose")']
p=subprocess.run(args,env=env,capture_output=True,text=True);(A/'results/common-witness.stdout').write_text(p.stdout);(A/'results/common-witness.stderr').write_text(p.stderr);print(p.stdout)
if p.returncode:print(p.stderr);raise SystemExit(p.returncode)
summary={'status':'ACCEPTED_COMMON_MEMORY_IO_LOCAL_COHORT','library':str(out),'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'source_worker':3,'minor_parent_fixes':['RAII visitor/result on vector exceptions','preallocated typed output storage avoids allocation exceptions inside Goose loops','removed shared gg_stacks reset'],'model_requests':0,'production_changed':False,'rollback':'FG153_GOOSE_COMMON=0','pending':'Batch A full Apex gate'};(A/'results/accepted-common-memory-io.json').write_text(json.dumps(summary,indent=2)+'\n')
for name in ['libllama.so','libllama.so.0','libllama.so.0.7.0']:
 q=R/name;assert q.is_symlink();q.unlink();q.symlink_to(out.name)
print('Accepted common IO/status and buffer collection; no rebuild or repeated corpus.')
