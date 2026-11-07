from pathlib import Path
import json,subprocess,hashlib
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp151-cache-lifecycle-20261107');P=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-expert-registry-20261107');run=A/'variants/cpu-backend/run';out=run/'libllama-control.so.0.7.0'
row=next(x for x in reversed(json.loads((P/'variants/cpu-backend/build/commands.json').read_text())) if x['label']=='link-llama' and x['exit_code']==0);argv=list(row['argv']);argv[argv.index('-o')+1]=str(out);argv=[str(A/'build/cache-cpp.o') if x==str(P/'variants/cpu-backend/build/cache-cpp.o') else x for x in argv];argv+=[str(A/'build/lifecycle.o'),'-Wl,-z,defs']
argv=[str((run/Path(x).name).resolve()) if Path(x).name in ['libggml.so.0.27.0','libggml-base.so.0.27.0','libggml-cpu.so.0.27.0','libggml-cuda.so.0.27.0'] else x for x in argv]
assert str(A/'build/cache-cpp.o') in argv
if str(P/'variants/cpu-backend/build/budget-services.o') not in argv:argv.append(str(P/'variants/cpu-backend/build/budget-services.o'))
if out.is_symlink():out.unlink()
assert not out.exists() or out.parent==run
p=subprocess.run(argv,cwd=row['cwd'],capture_output=True,text=True,timeout=180);(A/'build/link-cache-controller.log').write_text(p.stdout+p.stderr);(A/'build/link-cache-controller-command.json').write_text(json.dumps(dict(argv=argv,cwd=row['cwd'],exit_code=p.returncode),indent=2)+'\n')
if p.returncode:print((p.stdout+p.stderr)[-1800:]);raise SystemExit(p.returncode)
for name in ['libllama.so','libllama.so.0','libllama.so.0.7.0']:
 alias=run/name
 if alias.exists() or alias.is_symlink():alias.unlink()
 alias.symlink_to(out.name)
(A/'results/integration-state.json').write_text(json.dumps({'status':'LOCAL_CONTROLLER_LINKED_WHOLE_CYCLE_GATE_PENDING','source':'Actual expert-cache source object and self-hosted Goose lifecycle object linked;all other qualified runtime objects reused unchanged','library':str(out),'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'original_cpp_controller_preserved':'GGML_GOOSE_EXPERT_LIFECYCLE=0;GGML_GOOSE_EXPERT_CACHE=0 additionally restores old SLRU/budget helpers','model_requests':0,'broad_build':False,'sanitizer_campaign':False,'whole_runtime_performance':'Not measured;end-of-cycle shared gate pending','production_changed':False},indent=2)+'\n');print('Changed library linked in isolation;all other runtime components reused;model gate remains deferred')
