"""Mutation gate for the IQ2 F16/BF16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-269-goose-cuda-dequant-iq2-f16bf16'
g = E/'source'/'device.goose'
orig = g.read_text()

MUTATIONS = [
    ('xxs scale factor 0.25 -> 0.5 (f16 body)', '(0.25 as f32)', '(0.5 as f32)'),
    ('xxs sign index 7*il -> 6*il (f16 body)', 'sh7:u32=(7*il) as u32', 'sh7:u32=(6*il) as u32'),
    ('xs grid mask 511 -> 255 (f16 body)', 'qv&(511 as u32)', 'qv&(255 as u32)'),
    ('bf16 store uses half conversion', 'st_bf16(yy,(((ybase+j)*(2 as i64)) as u64),d*((gb) as f32)*sgn(bit));', 'st_half(yy,(((ybase+j)*(2 as i64)) as u64),d*((gb) as f32)*sgn(bit));'),
    ('xs scales index ib -> 2*ib (f16 body)', 'blk+66 as i64+ib', 'blk+66 as i64+2*ib'),
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
