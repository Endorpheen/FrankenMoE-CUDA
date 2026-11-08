from pathlib import Path
import subprocess,json,os,hashlib
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');D=A/'source/hybrid-memory';B=A/'build/hybrid-memory';R=A/'variants/cpu-backend/run';T=B/'run';T.mkdir(exist_ok=True)
candidate=json.loads((B/'candidate.json').read_text());lib=Path(candidate['library'])
for f in R.iterdir():
    if f.is_file() or f.is_symlink():
        to=T/f.name
        if to.is_symlink():to.unlink()
        elif to.exists():raise SystemExit('Refusing to overwrite '+str(to))
        to.symlink_to(lib if f.name in ['libllama.so','libllama.so.0','libllama.so.0.7.0'] else f.resolve())
inc=['-I'+str(D),'-I'+str(U/'src'),'-I'+str(U/'src/models'),'-I'+str(U/'include'),'-I'+str(U/'ggml/include')]
args=['/usr/bin/g++-13','-std=c++17','-O1',*inc,str(D/'fixture.cpp'),'-L'+str(T),'-Wl,-rpath,'+str(T),'-lllama','-lggml','-lggml-base','-lm','-o',str(B/'fixture')]
records=[]
for label,cmd in [('hybrid-fixture-build',args),('hybrid-fixture',[str(B/'fixture')])]:
    env=os.environ.copy();env['LD_LIBRARY_PATH']=str(T)+':'+str(R)
    p=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=90)
    (A/'results'/f'{label}.stdout').write_text(p.stdout);(A/'results'/f'{label}.stderr').write_text(p.stderr)
    records.append({'label':label,'argv':cmd,'exit_code':p.returncode});(B/'fixture-commands.json').write_text(json.dumps(records,indent=2)+'\n');print(label,p.returncode,flush=True)
    if p.returncode:print((p.stdout+p.stderr)[-5000:]);raise SystemExit(p.returncode)
summary={'status':'PASS_REAL_CPU_HYBRID_NATIVE_GOOSE_DIFFERENTIAL','library':str(lib),'sha256':hashlib.sha256(lib.read_bytes()).hexdigest(),'fixture_stdout':p.stdout,'model_requests':0,'coverage':['real CPU KV/recurrent allocation','native/Goose equal and sequence splits','context next/apply ordering','sequence cp/rm/keep/add/div','min/max and buffer breakdown','checkpoint write/read','recurrent read failure clears attention state','all status combinations'],'limits':['small synthetic model metadata, no GGUF weights or inference','child KV/recurrent policies still separately pending integration','native container and constructor ownership still open'],'rollback':'GGML_GOOSE_HYBRID=0','production_changed':False}
(A/'results/hybrid-memory-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
for name in ['libllama.so','libllama.so.0','libllama.so.0.7.0']:
    f=R/name;assert f.is_symlink();f.unlink();f.symlink_to(lib.name)
print('PASS real CPU hybrid differential; candidate aliases activated only after local correctness gate.')
