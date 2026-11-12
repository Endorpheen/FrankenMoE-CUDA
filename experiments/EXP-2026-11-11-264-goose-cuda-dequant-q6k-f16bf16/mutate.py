"""Mutation gate for the Q6_K F16/BF16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-264-goose-cuda-dequant-q6k-f16bf16'
g = E/'source'/'device.goose'
orig = g.read_text()

MUTATIONS = [
    ('qh high-bit shift 2 -> 1 (f16 body)', '(((qh>>2 as u32)&3)<<4)', '(((qh>>1 as u32)&3)<<4)'),
    ('scale index +2 -> +1 (f16 body)', 'scl+2 as i64', 'scl+1 as i64'),
    ('third value offset +64 -> +32 (f16 body)', '(ybase+64 as i64)*(2 as i64)', '(ybase+32 as i64)*(2 as i64)'),
    ('bf16 store uses half conversion', 'st_bf16(yy,(((ybase)*(2 as i64)) as u64),d*((s0) as f32)*((v0) as f32));', 'st_half(yy,(((ybase)*(2 as i64)) as u64),d*((s0) as f32)*((v0) as f32));'),
    ('offset constant -32 -> -31 (f16 body)', '(((qh>>0 as u32)&3)<<4)) as i64)-32;', '(((qh>>0 as u32)&3)<<4)) as i64)-31;'),
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
