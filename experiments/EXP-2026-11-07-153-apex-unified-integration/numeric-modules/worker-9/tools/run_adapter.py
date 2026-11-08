#!/usr/bin/env python3
import json, subprocess, sys
from pathlib import Path
root = Path(__file__).resolve().parent.parent
man = json.loads((root / 'kernels.json').read_text())
summary = []
for k in man['kernels']:
    base = Path(k['file']).stem
    gen = root / 'gen' / (base + '.c')
    cmd = [sys.executable, str(root / 'tools' / 'qualify_cuda.py'),
           '--generated', str(gen), '--alias', k['alias'],
           '--device-out', str(root / 'gen' / 'device' / (base + '.cu')),
           '--host-out', str(root / 'gen' / 'host' / (base + '.c')),
           '--summary', str(root / 'gen' / (base + '.emit.json')),
           '--wrapper', str(root / 'device' / 'wrappers' / (k['alias'] + '.inc'))]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        print(r.stdout); print(r.stderr, file=sys.stderr); sys.exit(r.returncode)
    summary.append(json.loads(r.stdout))
(root / 'gen' / 'adapter-summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print('adapted', len(summary), 'kernels')
