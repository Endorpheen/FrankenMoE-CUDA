"""Mutation gate for the count_equal device module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-273-goose-cuda-count-equal'
g = E/'source'/'device.goose'
orig = g.read_text()

MUTATIONS = [
    ('loop stride 32 -> 16', 'i=i+32 as i64;', 'i=i+16 as i64;'),
    ('chunk end drops the last element', 'if i1>k as i64{ i1=k; }', 'if i1>k as i64{ i1=k-1 as i64; }'),
    ('equality inverted', 'if xi==yi{', 'if xi!=yi{'),
    ('reduction ladder starts at 8', 'off:i64=16 as i64;', 'off:i64=8 as i64;'),
    ('atomic adds twice the count', 'atomic_add(dst,ne as i32);', 'atomic_add(dst,(ne*2 as i64) as i32);'),
]

rows = []
try:
    for name, old, new in MUTATIONS:
        assert old in orig, ('anchor missing', name)
        g.write_text(orig.replace(old, new, 1))
        e = subprocess.run(['python3', str(E/'emit_sources.py')], capture_output=True, text=True)
        if e.returncode:
            rows.append(dict(mutation=name, status='EMIT_FAILED')); print(name,'EMIT_FAILED'); continue
        t = subprocess.run(['python3', str(E/'test_local.py')], capture_output=True, text=True, timeout=1800)
        out = t.stdout + t.stderr
        m = re.search(r'failures=(\d+) launch_fail=(\d+)', out)
        fails = int(m.group(1)) + int(m.group(2)) if m else -1
        rows.append(dict(mutation=name, status='CAUGHT' if fails>0 else 'MISSED', failures=fails))
        print(name, 'failures=', fails)
finally:
    g.write_text(orig)
    subprocess.run(['python3', str(E/'emit_sources.py')], capture_output=True)
    subprocess.run(['python3', str(E/'test_local.py')], capture_output=True)

(E/'mutations.json').write_text(json.dumps(rows, indent=2)+'\n')
missed = [x for x in rows if x['status'] != 'CAUGHT']
print('MISSED' if missed else 'ALL_MUTATIONS_CAUGHT', len(rows))
raise SystemExit(1 if missed else 0)
