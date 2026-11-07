from pathlib import Path
import subprocess,json,shlex,sys
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp151-cache-lifecycle-20261107');D=A/'source/expert-cache';b=A/'build';records=[]
def run(label,argv,cwd=A):
 p=subprocess.run(argv,cwd=cwd,capture_output=True,text=True,timeout=180);(b/(label+'.log')).write_text(p.stdout+p.stderr);records.append(dict(label=label,argv=argv,cwd=str(cwd),exit_code=p.returncode));(b/'local-commands.json').write_text(json.dumps(records,indent=2)+'\n')
 if p.returncode:print((p.stdout+p.stderr)[-2500:]);raise SystemExit(p.returncode)
 print(label+' passed',flush=True)
run('emit-lifecycle',[str(A/'compiler/stage3-f849233b999c'),'-o',str(A/'generated/cache-lifecycle.c'),str(D/'lifecycle.goose')])
run('compile-lifecycle',['/usr/bin/gcc-13','-O2','-fPIC','-fno-strict-aliasing','-Werror=implicit-function-declaration','-c',str(D/'lifecycle-unit.c'),'-o',str(b/'lifecycle.o')])
rows=json.loads(Path('/home/end0/AI/LlamaServer/franken-goose-exp150-expert-registry-20261107/variants/cpu-backend/build/commands.json').read_text());row=next(x for x in rows if x['label']=='cache-cpp');argv=[x.replace('/home/end0/AI/LlamaServer/franken-goose-exp150-expert-registry-20261107/source/expert-cache',str(D)) for x in row['argv']];argv[argv.index('-o')+1]=str(b/'cache-cpp.o');run('compile-cache-adapter',argv,row['cwd'])
print('Local changed-module compilation complete;no runtime relink,sanitizer suite or model launch')
