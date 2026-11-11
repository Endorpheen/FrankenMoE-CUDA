"""Mutation gate for the 32-element legacy dequantization callbacks."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-262-goose-cuda-dequant-legacy32'
g = E/'source'/'device.goose'
orig = g.read_text()

MUTATIONS = [
    ('q1_0 sign drops the -1', '(((2*(k0 as i64))-(1 as i64)) as f32)', '(((2*(k0 as i64))+(0 as i64)) as f32)'),
    ('q2_0 code offset -1 removed', '(((c0 as i64)-(1 as i64)) as f32)', '((c0 as i64) as f32)'),
    ('q4_0 bias 8 -> 7', '(((lo) as f32)-(8.0 as f32))', '(((lo) as f32)-(7.0 as f32))'),
    ('q5_0 qs offset 6 -> 7', 'blk+6 as i64+iqs', 'blk+7 as i64+iqs'),
    ('q8_0 signed byte read as unsigned', 'ld_i8(vx,((blk+2 as i64+iqs) as u64))', 'ld_u8(vx,((blk+2 as i64+iqs) as u64))'),
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
