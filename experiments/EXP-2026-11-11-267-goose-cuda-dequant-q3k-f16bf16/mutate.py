"""Mutation gate for the Q3_K F16/BF16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-267-goose-cuda-dequant-q3k-f16bf16'
g = E/'source'/'device.goose'
orig = g.read_text()

MUTATIONS = [
    ('d offset 108 -> 106', 'blk+108 as i64', 'blk+106 as i64'),
    ('first-branch high bits is+8 -> is+4', 'base+is+8 as i64', 'base+is+4 as i64'),
    ('shift 2*j -> j (f16 body)', 'let shift:i64=2*j;', 'let shift:i64=j;'),
    ('bf16 store uses half conversion', 'st_bf16(yy,(((ybase+idx)*(2 as i64)) as u64),dl*((v) as f32));', 'st_half(yy,(((ybase+idx)*(2 as i64)) as u64),dl*((v) as f32));'),
    ('scale bias 32 -> 31', 'us-32 as i64', 'us-31 as i64'),
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
