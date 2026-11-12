"""Mutation gate for the snake/softcap module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-286-goose-cuda-snake-softcap'
files = {'device': E/'source'/'device.goose', 'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('snake drops one factor of s', 'device', 'xi+s*s*ld_f32(invb', 'xi+s*ld_f32(invb'),
    ('snake subtracts instead of adding', 'device', 'xi+s*s*ld_f32(invb', 'xi-s*s*ld_f32(invb'),
    ('snake channel index off by one', 'device', 'let c:i64=idx/tlen;', 'let c:i64=idx/tlen+1 as i64;'),
    ('softcap drops the cap factor', 'device', 'tanhf(scale*ld_f32(x,((idx*4 as i64) as u64)))*cap', 'tanhf(scale*ld_f32(x,((idx*4 as i64) as u64)))'),
    ('host grid uses 128 instead of 256', 'host', 'let nb:i64=(n+(256 as i64)-(1 as i64))/(256 as i64);', 'let nb:i64=(n+(128 as i64)-(1 as i64))/(128 as i64);'),
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
        fails = sum(int(x) for x in re.findall(r'failures=(\d+)', out)) + sum(int(x) for x in re.findall(r'launch_fail=(\d+)', out))
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
