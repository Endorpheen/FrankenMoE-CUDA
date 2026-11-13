"""Mutation gate for the arange device + host module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-274-goose-cuda-arange'
files = {'device': E/'source'/'device.goose', 'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('device value uses nidx+1', 'device', 'start+step*(nidx as f32)', 'start+step*((nidx+1 as i64) as f32)'),
    ('device value subtracts the step term', 'device', 'start+step*(nidx as f32)', 'start-step*(nidx as f32)'),
    ('device bound skips the last element', 'device', 'if nidx>=ne0 as i64{ return; }', 'if nidx>=ne0-1 as i64{ return; }'),
    ('host steps uses truncation instead of ceil', 'host', 'let steps:i64=ceilf(cx) as i64;', 'let steps:i64=cx as i64;'),
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
