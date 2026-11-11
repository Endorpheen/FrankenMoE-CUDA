"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-326-goose-kv-lazy-vmm'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('the lazy buffer is used without offload', 'host',
     '  if offload!=(0 as i64){', '  if true{'),
    ('the lazy buffer is used for a transposed V too', 'host',
     '   if v_trans==(0 as i64){', '   if true{'),
    ('the lazy buffer is used when the backend has none', 'host',
     '    if lazy_avail!=(0 as i64){ use_lazy=1 as i64; }', '    use_lazy=1 as i64;'),
    ('commits happen on a plain buffer', 'host',
     '   if vmm_lazy==(0 as i64){ ret=0 as i64; }else{', '   if false{ ret=0 as i64; }else{'),
    ('a zero request still commits', 'host',
     '    if n_kv==(0 as i64){ ret=0 as i64; }else{', '    if false{ ret=0 as i64; }else{'),
    ('absent tensors are committed too', 'host',
     '      if present!=(0 as i64){', '      if true{'),
    ('the highest cell is taken from the range start', 'host',
     '     let hi:i64=at(ranges,(2 as i64*r+1 as i64));', '     let hi:i64=at(ranges,(2 as i64*r));'),
    ('the highest cell becomes the lowest', 'host',
     '     if hi>mx{ mx=hi; }', '     if hi<mx{ mx=hi; }'),
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
