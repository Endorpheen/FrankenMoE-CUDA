from pathlib import Path
import json,hashlib,subprocess,os
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');S=A/'source/cpu-repack';B=A/'build/cpu-repack'
lock=json.loads((S/'source-lock.json').read_text())
assert hashlib.sha256((A/'compiler/stage3-f849233b999c').read_bytes()).hexdigest()==lock['compiler_sha256']
for rel,h in lock['files'].items():assert hashlib.sha256((S/rel).read_bytes()).hexdigest()==h,rel
for rel,h in lock['link_input_objects'].items():assert hashlib.sha256(Path(rel).read_bytes()).hexdigest()==h,rel
for r in json.loads((B/'replay-commands.json').read_text()):
 args=r['argv'].copy();dest=None;temp=None
 if r['label']=='repack-cohort-link':
  pos=args.index('-o')+1;dest=Path(args[pos]);temp=B/'replay-library.tmp';args[pos]=str(temp)
 env=os.environ.copy();env.update(r.get('env_overrides',{}))
 for k in r.get('unset_env',[]):env.pop(k,None)
 p=subprocess.run(args,cwd=r['cwd'],env=env,capture_output=True,text=True,timeout=180)
 print(r['label'],p.returncode)
 if p.returncode:raise SystemExit((p.stdout+p.stderr)[-5000:])
 if dest is not None:
  if dest.exists():
   assert hashlib.sha256(dest.read_bytes()).digest()==hashlib.sha256(temp.read_bytes()).digest(),'Immutable library drift;old candidate preserved'
   temp.unlink()
  else:temp.rename(dest)
