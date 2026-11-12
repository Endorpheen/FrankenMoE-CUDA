"""Mutation gate for the cumsum device + host module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-279-goose-cuda-cumsum'
files = {'device': E/'source'/'device.goose', 'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('warp scan adds every step without the lane condition', 'device', 'if lane>=off{ v=v+t; }', 'v=v+t;'),
    ('warp scan ladder steps by 1', 'device', 'off=off*2 as i64;', 'off=off+1 as i64;'),
    ('final offset drops the carry', 'device', 'let fo:f32=(svi+swi+carry)-t3;', 'let fo:f32=(svi+swi)-t3;'),
    ('carry is replaced instead of accumulated', 'device', 'sstore(SC,sload(SC)+sload(ST));', 'sstore(SC,sload(ST));'),
    ('host block cap 256 -> 512', 'host', 'if blk>256 as i64{ blk=256 as i64; }', 'if blk>512 as i64{ blk=512 as i64; }'),
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
