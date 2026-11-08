from pathlib import Path
import subprocess,json,os,shlex,hashlib

A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107')
U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
S=A/'source/apex-model';I=A/'source/integration';B=A/'build/model-integration';R=A/'variants/cpu-backend/run'
B.mkdir(exist_ok=True);I.mkdir(exist_ok=True)
records=[]
def run(label,args,cwd=A,env=None):
    p=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=180)
    (A/'results'/f'{label}.stdout').write_text(p.stdout)
    (A/'results'/f'{label}.stderr').write_text(p.stderr)
    records.append({'label':label,'argv':args,'cwd':str(cwd),'exit_code':p.returncode})
    (B/'commands.json').write_text(json.dumps(records,indent=2)+'\n')
    print(label,p.returncode,flush=True)
    if p.returncode:
        print((p.stdout+p.stderr)[-6500:]);raise SystemExit(p.returncode)
    return p
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
source=U/'build-cuda/src/CMakeFiles/llama.dir/Unity/unity_8_cxx.cxx'
old=str(U/'src/models/qwen35moe.cpp');new=str(S/'qwen35moe.cpp')
assert sha(U/'src/models/qwen35moe.cpp')=='579426f57c8691279ab9231264b5a0bceb214e57b3876d58020a1f40a98647a4'
text=source.read_text();assert text.count(old)==1
unity=I/'apex-unity8.cxx';unity.write_text(text.replace(old,new))
recipe=next(r for r in json.loads((U/'build-cuda/compile_commands.json').read_text()) if r['file']==str(source))
args=shlex.split(recipe['command']);args[args.index('-c')+1]=str(unity);args[args.index('-o')+1]=str(B/'apex-unity8.o')
args+=['-I'+str(U/'src/models'),'-I'+str(S)]
run('model-cohort-unity',args,cwd=Path(recipe['directory']))
inc=['-I'+str(S),'-I'+str(U/'src'),'-I'+str(U/'src/models'),'-I'+str(U/'include'),'-I'+str(U/'ggml/include')]
objects=[]
for module in ['unit','primitives','services','binding-services']:
    c=module=='unit';out=B/(module+'.o');objects.append(str(out))
    args=['/usr/bin/gcc-13' if c else '/usr/bin/g++-13','-std=c11' if c else '-std=c++17','-O3','-DNDEBUG','-fPIC','-D_GNU_SOURCE','-DGGML_SHARED','-DGGML_BACKEND_SHARED','-DLLAMA_SHARED','-DLLAMA_BUILD']
    if c:args+=['-Werror=implicit-function-declaration','-fno-strict-aliasing']
    args+=inc+['-c',str(S/(module+('.c' if c else '.cpp'))),'-o',str(out)]
    run('model-cohort-'+module,args)
parent=Path('/home/end0/AI/LlamaServer/franken-goose-exp151-cache-lifecycle-20261107/build/link-cache-controller-command.json')
recipe=json.loads(parent.read_text());args=recipe['argv'].copy();out=R/'libllama-apex-goose.so.0.7.0';args[args.index('-o')+1]=str(out)
args=[str(B/'apex-unity8.o') if p=='src/CMakeFiles/llama.dir/Unity/unity_8_cxx.cxx.o' else p for p in args]
for k,p in enumerate(args):
    if p.startswith('-Wl,-rpath,'):args[k]='-Wl,-rpath,$ORIGIN'
    elif p.endswith('.so.0.27.0') and Path(p).name.startswith('libggml'):args[k]=str(R/Path(p).name)
args+=objects
run('model-cohort-link',args,cwd=Path(recipe['cwd']))
env=os.environ.copy();env['LD_LIBRARY_PATH']=str(R)
run('model-cohort-dlopen',['python3','-c','import ctypes,os;ctypes.CDLL('+repr(str(out))+',mode=os.RTLD_NOW);print("PASS isolated merged llama DSO resolves all symbols")'],env=env)
for name in ['libllama.so','libllama.so.0','libllama.so.0.7.0']:
    f=R/name
    if f.exists() or f.is_symlink():
        assert f.is_symlink(),f
        f.unlink()
    f.symlink_to(out.name)
fixture=(S/'fixture.cpp').read_text();assert fixture.count('mode<=7')==1
fixture=fixture.replace('mode<=7','mode<=6')
file=I/'apex-model-merged-fixture.cpp';file.write_text(fixture)
run('model-cohort-fixture-compile',['/usr/bin/g++-13','-std=c++17','-O1',*inc,'-c',str(file),'-o',str(B/'fixture.o')])
run('model-cohort-fixture-link',['/usr/bin/g++-13',str(B/'fixture.o'),str(A/'build/apex-model/native-reference.o'),str(A/'build/apex-model/native-delta.o'),'-L'+str(R),'-Wl,-rpath,'+str(R),'-lllama','-lggml','-lggml-base','-lm','-o',str(B/'graph-differential')])
p=run('model-cohort-fixture',[str(B/'graph-differential')],env=env)
summary={'status':'PASS_MERGED_APEX_MODEL_GRAPH_LOCAL','llama_library':str(out),'llama_sha256':sha(out),'ggml_library':str((R/'libggml-base.so').resolve()),'graph_fixture_stdout':p.stdout,'model_requests':0,'model_inference':'PENDING_COMPLETE_BATCH_A','changed_components':['Apex graph loop and tensor binding','Goose GGML constructors','Goose allocation','Goose scheduler'],'residual_boundaries':['hybrid KV/recurrent storage and context lifecycle','owned native helpers/containers still documented per component','CUDA numerical backend deferred to batch B'],'production_changed':False,'compiler_changed':False,'rollback_env':'FG153_APEX_MODEL=0','parent_llama_library':str(Path('/home/end0/AI/LlamaServer/franken-goose-exp151-cache-lifecycle-20261107/variants/cpu-backend/run/libllama-control.so.0.7.0'))}
(A/'results/model-cohort-integration.json').write_text(json.dumps(summary,indent=2)+'\n')
print('Merged Apex model subsystem linked and graph differential verified; real inference remains pending complete batch A.',flush=True)
