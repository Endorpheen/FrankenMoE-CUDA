"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-320-goose-kv-hadamard'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('corner starts negative', 'host',
     '  setat(out,0 as i64,1 as i64);', '  setat(out,0 as i64,(0 as i64)-1 as i64);'),
    ('bottom-right quadrant is not negated', 'host',
     '     setat(out,(i+s)*n+(j+s),(0 as i64)-val);', '     setat(out,(i+s)*n+(j+s),val);'),
    ('right quadrant is never written', 'host',
     '     setat(out,i*n+(j+s),val);', ''),
    ('doubling step advances by one', 'host',
     '   s=s*2 as i64;', '   s=s+1 as i64;'),
    ('rotation size test uses the size without the factor two', 'host',
     '    while embd_k%(2 as i64*n_rot_k)==(0 as i64){ n_rot_k=n_rot_k*2 as i64; }', '    while embd_k%n_rot_k==(0 as i64){ n_rot_k=n_rot_k*2 as i64; }'),
    ('rotation size starts at 32', 'host',
     '    n_rot_k=64 as i64;', '    n_rot_k=32 as i64;'),
    ('value rotation ignores the head-width multiple', 'host',
     '      if head_v%64 as i64==(0 as i64){', '      if true{'),
    ('indexer architectures rotate without the width match', 'host',
     '    if k_eq_idx!=(0 as i64){ rot_k=1 as i64; }', '    rot_k=1 as i64;'),
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
        t = subprocess.run(['python3', str(E/'test_local.py')], capture_output=True, text=True, timeout=3600)
        out = t.stdout + t.stderr
        fails = sum(int(x) for x in re.findall(r'failures=(\d+)', out)) + sum(int(x) for x in re.findall(r'launch_fail=(\d+)', out))
        # A mutation that breaks an invariant trips a validation code and aborts before the
        # summary prints; that is detection too, so the exit status counts as a channel.
        caught = fails > 0 or t.returncode != 0
        rows.append(dict(mutation=name, status='CAUGHT' if caught else 'MISSED', failures=fails, exit_code=t.returncode))
        print(name, 'failures=', fails, 'rc=', t.returncode)
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
