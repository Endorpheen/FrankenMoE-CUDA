import json, pathlib, subprocess, os, hashlib, sys
r=pathlib.Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107')
src=r/'source/apex-ops'; build=r/'build/apex-ops'; out=r/'results/apex-ops'
records=[]
def run(label,args,env=None):
    p=subprocess.run([str(a) for a in args],capture_output=True,text=True,env=env)
    records.append({'label':label,'argv':[str(a) for a in args],'returncode':p.returncode})
    (out/(label+'.stdout')).write_text(p.stdout);(out/(label+'.stderr')).write_text(p.stderr)
    (out/'commands.json').write_text(json.dumps(records,indent=2)+'\n')
    print(label,p.returncode,flush=True)
    if p.returncode:print(p.stderr[-8000:]);raise SystemExit(p.returncode)
compiler=r/'compiler/stage3-f849233b999c'
assert hashlib.sha256(compiler.read_bytes()).hexdigest()=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a'
run('generate',[compiler,'-o',r/'generated/apex-ops/kernel.c',src/'kernel.goose'])
run('emit-unit',[sys.executable,src/'emit-unit.py'])
run('replay-snapshot',[sys.executable,src/'replay.py','--input',src/'parent-ggml-core.c','--output',src/'ggml-core.c'])
base=json.loads((r/'build/local-commands.json').read_text())
cmd=next(x['argv'] for x in base if x['label']=='compile-core').copy()
cmd=[str(src/'ggml-core.c') if a==str(r/'source/tensor-core/ggml-core.c') else str(build/'ggml-core.o') if a==str(r/'build/ggml-core.o') else a for a in cmd]
run('compile-core',cmd)
for module,source in [('ops',src/'unit.c'),('shapes',r/'source/shapes/unit.c')]:
    run('compile-'+module,['/usr/bin/gcc-13','-std=gnu11','-O2','-fPIC','-fno-strict-aliasing','-Werror=implicit-function-declaration','-Werror=incompatible-pointer-types','-ffunction-sections','-fdata-sections','-c',source,'-o',build/(module+'.o')])
cmd=next(x['argv'] for x in base if x['label']=='link-fixture').copy()
cmd=[str(src/'fixture.c') if a==str(r/'source/tensor-core/fixture.c') else str(build/'ggml-core.o') if a==str(r/'build/ggml-core.o') else str(build/'fixture') if a==str(r/'build/tensor-fixture') else a for a in cmd]
cmd[1:1]=['-Werror=implicit-function-declaration','-Werror=incompatible-pointer-types']
cmd[cmd.index('-Wl,--gc-sections'):cmd.index('-Wl,--gc-sections')]=[str(build/'ops.o'),str(build/'shapes.o')]
run('link-fixture',cmd)
run('differential-fixture',[build/'fixture'])
numeric_cmd=[str(src/'numeric.c') if a==str(src/'fixture.c') else str(build/'numeric') if a==str(build/'fixture') else a for a in cmd]
numeric_cmd.insert(1,'-I/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/src')
run('link-numeric',numeric_cmd)
numeric_env=os.environ.copy();numeric_env['GGML_NO_BACKTRACE']='1'
run('numeric-fixture',[build/'numeric'],numeric_env)
