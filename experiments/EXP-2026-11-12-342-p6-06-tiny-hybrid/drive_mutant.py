import json, re, subprocess, sys
from pathlib import Path
E=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments/EXP-2026-11-12-342-p6-06-tiny-hybrid')
BIN='/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin'
def run():
    p=subprocess.run(['env','-i','PATH=/usr/bin:/bin',f'LD_LIBRARY_PATH={BIN}','HOME=/tmp','timeout','40',
                      f'{BIN}/llama-server','-m',str(E/'tiny-mutant.gguf'),'-c','256','-ngl','0','--port','8053'],
                     capture_output=True,text=True)
    out=p.stdout+p.stderr
    for l in out.splitlines():
        if 'listening on' in l: return ('ok',None,None)
        m=re.search(r"tensor '([^']+)' has wrong shape; expected\s+([\d,\s]+?)got", l)
        if m: return ('shape', m.group(1), [int(x) for x in m.group(2).replace(' ','').strip(',').split(',') if x.strip()])
        m=re.search(r"tensor '([^']+)' not found", l)
        if m: return ('missing', m.group(1), None)
    return ('other',None,None)
for r in range(1,30):
    k,n,d=run(); print(f'round {r}: {k} {n} {d}')
    if k=='ok': print('MUTANT LOADS'); break
    names=[l.strip() for l in (E/'names-mutant.txt').read_text().splitlines() if l.strip()]
    sh=json.loads((E/'shapes-mutant.json').read_text())
    if k=='missing':
        if n not in names: names.append(n)
        sh.setdefault(n, [32])
    elif k=='shape':
        sh[n]=d
    else:
        break
    (E/'names-mutant.txt').write_text('\n'.join(names)+'\n')
    (E/'shapes-mutant.json').write_text(json.dumps(sh,indent=1)+'\n')
    subprocess.run([sys.executable, str(E/'gen_mutant.py')],capture_output=True,env={**__import__('os').environ,'TINY_RECUR':'00'})
