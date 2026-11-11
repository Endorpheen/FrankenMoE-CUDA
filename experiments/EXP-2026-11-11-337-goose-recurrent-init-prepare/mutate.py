"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-337-goose-recurrent-init-prepare'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('an all-output batch splits equally', 'host',
     '  if embd_all!=(0 as i64){ mode=0 as i64; }', '  if false{ mode=0 as i64; }'),
    ('the split is not sequential', 'host',
     '  emit(out,cntp,0 as i64,mode,1 as i64);', '  emit(out,cntp,0 as i64,mode,0 as i64);'),
    ('the rollback window is missing', 'host',
     '  if n_rs_seq>(0 as i64){ window=n_rs_seq+(1 as i64); }', ''),
    ('unplaced tokens are accepted', 'host',
     '  if n_used<n_tokens{', '  if false{'),
    ('the dry run commits nothing but reports nothing either', 'host',
     '  emit(out,cntp,4 as i64,0 as i64,0 as i64);', ''),
    ('the walk continues past a refused slot', 'host',
     '    ok=0 as i64;\n    j=n_find;', '    ok=0 as i64;\n    j=j+1 as i64;'),
    ('the state is only restored on failure', 'host',
     '  emit(out,cntp,7 as i64,0 as i64,0 as i64);', '  if ok==(0 as i64){ emit(out,cntp,7 as i64,0 as i64,0 as i64); }'),
    ('init_update returns a working context', 'host',
     '   emit(out,cntp,8 as i64,0 as i64,0 as i64);', ''),
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
