"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-307-goose-dsv4-state-read-plan'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('version zero is accepted', 'host',
     ' if version_ref!=(1 as i64){', ' if version_ref<(1 as i64){'),
    ('embedding size is not compared', 'host',
     '  if ratio_ref!=ratio||state_size_ref!=state_size||n_embd_ref!=n_embd{', '  if ratio_ref!=ratio||state_size_ref!=state_size{'),
    ('smaller layer counts pass', 'host',
     '   if n_layer_ref!=n_layers{', '   if n_layer_ref>n_layers{'),
    ('destination accepts a stream count below one', 'host',
     '     if ns!=(1 as i64){ ok_range=false; }', '     if ns<(1 as i64){ ok_range=false; }'),
    ('destination accepts the first id past the range', 'host',
     '       if seq_id>=n_stream{ ok_range=false; }else{ s0=seq_id; }', '       if seq_id>n_stream{ ok_range=false; }else{ s0=seq_id; }'),
    ('layer ids below the expected one pass', 'host',
     '      if at(il_ref,il)!=il{ bad=true; }else{', '      if at(il_ref,il)<il{ bad=true; }else{'),
    ('each layer performs one read', 'host',
     '       reads=reads+2 as i64;', '       reads=reads+1 as i64;'),
    ('full-state restore accepts a partial stream count', 'host',
     '     if ns!=n_stream{ ok_range=false; }else{ s0=0; }', '     if ns<n_stream{ ok_range=false; }else{ s0=0; }'),
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
