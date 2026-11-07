from pathlib import Path
import json,os,subprocess,sys
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');a=Path('/home/end0/AI/LlamaServer/franken-goose-exp148-quant-20261107')
sanitize='--sanitize' in sys.argv
v=a/'variants'/('sanitized' if sanitize else 'cpu-backend');b=v/'build';run=v/'run';b.mkdir(parents=True,exist_ok=True);run.mkdir(exist_ok=True)
for path in (a/'variants/cpu-backend/run').iterdir():
 if path.is_file() and not path.name.startswith(('libggml-cpu.so','libggml-base.so')) and not (run/path.name).exists():os.symlink(path,run/path.name)
def mapped(arg):
 for old,new in [(r/'source/cpu-backend',a/'source/cpu-backend'),(r/'generated',a/'generated'),(r/'variants/cpu-backend/build',b),(r/'variants/cpu-backend/run',run)]:arg=arg.replace(str(old),str(new))
 return arg

for path in (a/'variants/cpu-backend/build').glob('rename-*.txt'):
 if b!=path.parent:(b/path.name).write_bytes(path.read_bytes())
records=[]
def execute(label,argv,cwd):
 q=subprocess.run(argv,cwd=cwd,capture_output=True,text=True,timeout=180);(b/(label+'.log')).write_text(q.stdout+q.stderr);records.append({'label':label,'argv':argv,'cwd':str(cwd),'exit_code':q.returncode});print(label,q.returncode,flush=True)
 if q.returncode:print((q.stdout+q.stderr)[-2500:]);raise SystemExit(q.returncode)
def inst(argv):
 if not sanitize:return argv
 return [('-O1' if x in ['-O2','-O3'] else x) for x in argv]+['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g']
execute('generate-backend',[str(a/'compiler/stage3-f849233b999c'),'-o',str(a/'generated/cpu-backend-api.c'),str(a/'source/cpu-backend/backend/kernel.goose')],a)
for output,source in [('cpu-backend.c','kernel.goose'),('cpu-executor.c','executor/kernel.goose'),('cpu-stateful.c','stateful/kernel.goose'),('cpu-control.c','control/kernel.goose'),('cpu-quant.c','quant/kernel.goose')]:
 execute('generate-'+output,[str(a/'compiler/stage3-f849233b999c'),'-o',str(a/'generated'/output),str(a/'source/cpu-backend'/source)],a)
execute('quant-unit',inst(['/usr/bin/gcc-13','-O3','-march=native','-fPIC','-ffp-contract=off','-fno-strict-aliasing','-Werror=implicit-function-declaration','-c',str(a/'source/cpu-backend/quant/unit.c'),'-o',str(b/'quant.o')]),a)
recipes=json.loads((r/'variants/cpu-backend/build/integration-commands.json').read_text())
for row in recipes:
 if row['label']=='link':continue
 argv=[mapped(x) for x in row['argv']]
 if not row['label'].startswith('rename-'):argv=inst(argv)
 if argv[0].endswith('gcc-13'):argv+=['-Werror=implicit-function-declaration']
 execute(row['label'],argv,row['cwd'])
for label in ['backend-base-goose','backend-base','backend-traits','base-ggml','base-ggmlcpp','base-ggmlalloc','base-backendmeta','base-opt','base-threading','base-quants','base-gguf']:
 row=json.loads((r/'variants/cpu-backend/build'/(label+'-command.json')).read_text());execute(label,inst([mapped(x) for x in row['argv']])+(['-Werror=implicit-function-declaration'] if row['argv'][0].endswith('gcc-13') else []),row['cwd'])
for row in [json.loads((r/'variants/cpu-backend/build/base-link-command.json').read_text()),next(x for x in recipes if x['label']=='link')]:
 argv=[mapped(x) for x in row['argv']];argv+=([str(b/'quant.o')] if row.get('label')=='link' else []);execute('link-'+('base' if 'base-' in ' '.join(argv) else 'cpu'),inst(argv),row['cwd'])
for stem in ['libggml-cpu','libggml-base']:
 for suffix in ['.so','.so.0']:
  dest=run/(stem+suffix)
  if dest.exists() or dest.is_symlink():dest.unlink()
  dest.symlink_to(stem+'.so.0.27.0')
(b/'commands.json').write_text(json.dumps(records,indent=2)+'\n')
print(run,flush=True)
