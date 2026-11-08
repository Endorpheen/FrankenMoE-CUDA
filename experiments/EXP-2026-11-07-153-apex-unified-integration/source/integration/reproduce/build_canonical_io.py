from pathlib import Path
import json,hashlib,subprocess
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107')
S=A/'source/canonical-io';B=A/'build/canonical-io'
lock=json.loads((S/'source-lock.json').read_text())
assert hashlib.sha256((A/'compiler/stage3-f849233b999c').read_bytes()).hexdigest()==lock['compiler_sha256']
for rel,h in lock['files'].items():assert hashlib.sha256((S/rel).read_bytes()).hexdigest()==h,rel
for r in json.loads((B/'replay-commands.json').read_text()):
 p=subprocess.run(r['argv'],cwd=r['cwd'],capture_output=True,text=True,timeout=180)
 print(r['label'],p.returncode)
 if p.returncode:raise SystemExit((p.stdout+p.stderr)[-5000:])
