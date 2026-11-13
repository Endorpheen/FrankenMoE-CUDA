"""Mutation gate for the diag device + host module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-275-goose-cuda-diag'
files = {'device': E/'source'/'device.goose', 'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('device diagonal test inverted (f32 body)', 'device', 'if i0==i1{\n  let bi:i64=i3*ne2+i2;\n  let si:i64=bi*ne0+i0;\n  st_f32', 'if i0!=i1{\n  let bi:i64=i3*ne2+i2;\n  let si:i64=bi*ne0+i0;\n  st_f32'),
    ('device i1 divisor ne0 -> ne1 (f32 body)', 'device', 'let i1:i64=(gidx/ne0)%ne1;', 'let i1:i64=(gidx/ne1)%ne1;'),
    ('device i2 divisor drops ne1 (f32 body)', 'device', 'let i2:i64=(gidx/(ne0*ne1))%ne2;', 'let i2:i64=(gidx/ne0)%ne2;'),
    ('host product check weakened to >=', 'host', 'ensure(ne==ne0*ne1*ne2*ne3,22);', 'ensure(ne>=ne0*ne1*ne2*ne3,22);'),
    ('host block size 256 -> 128', 'host', 'put(p,1,1,256 as u64);', 'put(p,1,1,128 as u64);'),
]

rows = []
try:
    for name, mod, old, new in MUTATIONS:
        src = orig[mod]
        assert old in src, ('anchor missing', name)
        files[mod].write_text(src.replace(old, new, 1))
        e = subprocess.run(['python3', str(E/'emit_sources.py')], capture_output=True, text=True)
        if e.returncode:
            rows.append(dict(mutation=name, status='EMIT_FAILED')); print(name,'EMIT_FAILED')
            files[mod].write_text(src); continue
        t = subprocess.run(['python3', str(E/'test_local.py')], capture_output=True, text=True, timeout=1800)
        out = t.stdout + t.stderr
        fails = sum(int(x) for x in re.findall(r'failures=(\d+)', out))
        rows.append(dict(mutation=name, status='CAUGHT' if fails>0 else 'MISSED', failures=fails))
        print(name, 'failures=', fails)
        files[mod].write_text(src)
finally:
    for k, v in files.items():
        v.write_text(orig[k])
    subprocess.run(['python3', str(E/'emit_sources.py')], capture_output=True)
    subprocess.run(['python3', str(E/'test_local.py')], capture_output=True)

(E/'mutations.json').write_text(json.dumps(rows, indent=2)+'\n')
missed = [x for x in rows if x['status'] != 'CAUGHT']
print('MISSED' if missed else 'ALL_MUTATIONS_CAUGHT', len(rows))
raise SystemExit(1 if missed else 0)
