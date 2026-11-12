"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-306-goose-dsv4-state-io-plan'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('stream stride uses the live row count', 'host',
     ' let stream_stride:i64=tensor_rows*row_size;', ' let stream_stride:i64=n_rows*row_size;'),
    ('record size uses the storage row count', 'host',
     ' let size:i64=n_rows*row_size;', ' let size:i64=tensor_rows*row_size;'),
    ('row count check is off by one', 'host',
     '  if n_rows>tensor_rows{ ret=(0 as i64)-1 as i64; }else{', '  if n_rows>=tensor_rows{ ret=(0 as i64)-1 as i64; }else{'),
    ('stream map accepts fewer ids than streams', 'host',
     '     if ids_len!=ns{ ret=(0 as i64)-1 as i64; }', '     if ids_len<ns{ ret=(0 as i64)-1 as i64; }'),
    ('stream bound check is off by one', 'host',
     '      if stream>=ne2{ bad=true; }else{', '      if stream>ne2{ bad=true; }else{'),
    ('write offset drops the stride', 'host',
     '       setat(out,(2 as i64*s),stream*stream_stride);', '       setat(out,(2 as i64*s),stream);'),
    ('read offset drops the stride', 'host',
     '          setat(out,(2 as i64*s),(s0+s)*stream_stride);', '          setat(out,(2 as i64*s),(s0+s));'),
    ('read ignores the tensor type', 'host',
     '   if type_i!=rt{ ret=(0 as i64)-1 as i64; }else{', '   if type_i<rt{ ret=(0 as i64)-1 as i64; }else{'),
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
