"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-349-goose-batch-ubatch-add'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('a fully embedding-marked ubatch is not mixed', 'host',
     '  if n_embd_rows>(0 as i64)&&n_embd_rows<n{ mixed=1 as i64; }', '  if n_embd_rows>(0 as i64)&&n_embd_rows<=n{ mixed=1 as i64; }'),
    ('tokens are used for an all-embedding batch', 'host',
     '  if mixed_batch!=(0 as i64)&&n_embd_rows==n{ use_token=0 as i64; }', ''),
    ('embeddings are used for a token-only batch', 'host',
     '  if mixed_batch!=(0 as i64)&&n_embd_rows==(0 as i64){ use_embd=0 as i64; }', ''),
    ('the state payload is sized by the token count only', 'host',
     ' if has_state!=(0 as i64){ s_all=n*n_embd; }', ' if has_state!=(0 as i64){ s_all=n; }'),
    ('out ids are recorded for every row', 'host',
     '  if lg!=(0 as i64){', '  if true{'),
    ('unique sequences are listed in row order', 'host',
     '  var present:i64=0 as i64;\n  var r:i64=0;', '  var present:i64=0 as i64;\n  var r:i64=n-(1 as i64);'),
    ('the sequence index is not assigned', 'host',
     '   emit(out,cntp,5 as i64,s,u);', ''),
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
