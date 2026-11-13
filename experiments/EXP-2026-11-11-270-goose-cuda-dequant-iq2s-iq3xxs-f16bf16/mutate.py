"""Mutation gate for the IQ2_S / IQ3_XXS F16/BF16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-270-goose-cuda-dequant-iq2s-iq3xxs-f16bf16'
g = E/'source'/'device.goose'
orig = g.read_text()

MUTATIONS = [
    ('q2_s grid byte drops il (f16 body)', 'qs+4*ib+il', 'qs+4*ib'),
    ('q2_s qh shift 8-2*il -> 6-2*il (f16 body)', '(8-2*il) as u32', '(6-2*il) as u32'),
    ('q3_xxs exponent 28 -> 27 (f16 body)', '(28 as u32)', '(27 as u32)'),
    ('q3_xxs second group +4 -> +8 (f16 body)', 'ybase+j+4 as i64', 'ybase+j+8 as i64'),
    ('bf16 store uses half conversion (q3_xxs body)', 'st_bf16(yy,(((ybase+j)*(2 as i64)) as u64),d*((b1) as f32)*sgn(q1));', 'st_half(yy,(((ybase+j)*(2 as i64)) as u64),d*((b1) as f32)*sgn(q1));'),
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
