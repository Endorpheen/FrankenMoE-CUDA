#!/usr/bin/env python3
import re, subprocess, sys, json
from pathlib import Path
E = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments/EXP-2026-11-12-342-p6-06-tiny-hybrid')
BIN='/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin'
def run():
    p=subprocess.run(['env','-i','PATH=/usr/bin:/bin',f'LD_LIBRARY_PATH={BIN}','HOME=/tmp','timeout','40',
                      f'{BIN}/llama-server','-m',str(E/'tiny-hybrid.gguf'),'-c','256','-ngl','0','--port','8051'],
                     capture_output=True,text=True)
    out=p.stdout+p.stderr
    for l in out.splitlines():
        if 'listening on' in l: return ('ok',None,None)
        m=re.search(r"tensor '([^']+)' has wrong shape; expected\s+([\d,\s]+?)got", l)
        if m: return ('shape', m.group(1), [int(x) for x in m.group(2).replace(' ','').strip(',').split(',') if x.strip()])
        m=re.search(r"tensor '([^']+)' not found", l)
        if m: return ('missing', m.group(1), None)
        m=re.search(r"wrong number of tensors; expected (\d+), got (\d+)", l)
        if m: return ('count', None, (int(m.group(1)), int(m.group(2))))
    return ('other',None,None)
for r in range(1,120):
    k,n,d=run(); print(f'round {r}: {k} {n} {d}')
    if k=='ok': print('MODEL LOADS'); break
    if k=='shape':
        s=json.loads((E/'shapes.json').read_text()); s[n]=d
        (E/'shapes.json').write_text(json.dumps(s,indent=1)+'\n')
        subprocess.run([sys.executable, str(E/'gen.py')],capture_output=True); continue
    break
