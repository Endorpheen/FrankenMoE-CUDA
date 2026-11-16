#!/usr/bin/env python3
"""Rebuild one CUDA unit and relink an isolated host-registration candidate."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--support', type=Path, required=True)
ap.add_argument('--control', type=Path, required=True)
ap.add_argument('--out', type=Path, required=True)
a = ap.parse_args()
a.support = a.support.resolve()
a.control = a.control.resolve()
a.out = a.out.resolve()
if not a.out.is_relative_to(Path('/home/end0/AI/LlamaServer')):
    raise SystemExit('candidate output must be inside LlamaServer')
a.out.mkdir(exist_ok=False)

manifest = []

def digest(p):
    with Path(p).open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def run(cmd, **kw):
    with (a.out/'commands.jsonl').open('a') as f:
        f.write(json.dumps([str(x) for x in cmd])+'\n')
    print('command:', shlex.join([str(x) for x in cmd]), flush=True)
    q = subprocess.run(cmd, **kw)
    print('exit:', q.returncode, flush=True)
    q.check_returncode()
    return q

entries = json.loads((a.support/'compile_commands.json').read_text())
entry = next(e for e in entries if e['file'].endswith('/ggml-cuda/ggml-cuda.cu'))
cmd = shlex.split(entry['command'])
old_object = (a.support/entry['output']).resolve()
new_object = a.out/'ggml-cuda.cu.o'
cmd[cmd.index('-o')+1] = str(new_object)
source = Path(entry['file'])
(a.out/'source.cu').write_bytes(source.read_bytes())
run(cmd, cwd=entry['directory'])

raw = subprocess.check_output(['ninja', '-C', str(a.support), '-t', 'commands',
                               'bin/libggml-cuda.so.0.27.0'], text=True).splitlines()[-1]
link = shlex.split(raw)
assert link[:2] == [':', '&&'] and link[-2:] == ['&&', ':']
link = link[2:-2]
for i, arg in enumerate(link):
    if arg.endswith('.o'):
        p = (a.support/arg).resolve()
        manifest.append({'path': str(p), 'sha256': digest(p)})
        if p == old_object:
            link[i] = str(new_object)
    elif arg.startswith('-Wl,--dependency-file='):
        link[i] = '-Wl,--dependency-file='+str(a.out/'link.d')
    elif arg.startswith('-Wl,-rpath,'):
        link[i] = '-Wl,-rpath,$ORIGIN'
link[link.index('-o')+1] = str(a.out/'libggml-cuda.so.0')
link.insert(1, '-Wl,-z,defs')
run(link, cwd=a.support)

runtime = a.out/'run'
runtime.mkdir()
control_manifest = {}
for p in sorted(a.control.iterdir()):
    if p.name != 'llama-server' and '.so' not in p.name:
        continue
    control_manifest[p.name] = {'sha256': digest(p), 'bytes': p.stat().st_size}
    candidate = a.out/'libggml-cuda.so.0' if p.name == 'libggml-cuda.so.0' else p
    shutil.copy2(candidate, runtime/p.name)
changed = [n for n, v in control_manifest.items() if digest(runtime/n) != v['sha256']]
assert changed == ['libggml-cuda.so.0'], changed
with (runtime/'SHA256SUMS').open('w') as f:
    for p in sorted(runtime.iterdir()):
        if p.name != 'SHA256SUMS':
            f.write(digest(p)+'  '+p.name+'\n')
q = run(['ldd', '-r', str(runtime/'llama-server')],
        env={**os.environ, 'LD_LIBRARY_PATH': str(runtime)}, capture_output=True, text=True)
(a.out/'dependencies.txt').write_text(q.stdout+q.stderr)
assert 'undefined symbol' not in q.stdout+q.stderr and 'not found' not in q.stdout+q.stderr
(a.out/'provenance.json').write_text(json.dumps({
    'source_commit': subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(),
    'source': str(source), 'source_sha256': digest(source),
    'control': str(a.control), 'control_components': control_manifest,
    'reused_source_build_objects': manifest, 'changed_components': changed,
    'candidate_cuda_sha256': digest(runtime/'libggml-cuda.so.0'),
}, indent=2)+'\n')
print('candidate link/dependency/component gates passed:', runtime)
