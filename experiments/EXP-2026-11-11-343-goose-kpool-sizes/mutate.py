"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-343-goose-kpool-sizes'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('the full context reports the worst case', 'host',
     '  emit(out,cntp,0 as i64,0 as i64,lay);\n  ret=lay;', '  emit(out,cntp,0 as i64,0 as i64,lay);\n  ret=lay;\n  emit(out,cntp,1 as i64,0 as i64,0 as i64);'),
    ('an untracked context lays out anyway', 'host',
     '   if track==(0 as i64){', '   if false{'),
    ('the worst case is not floored by the layout', 'host',
     '    if worst>npr{ npr=worst; }', ''),
    ('the graph bound may be zero', 'host',
     '    if g<(1 as i64){ g=1 as i64; }', ''),
    ('n_new is not set to the raised count', 'host',
     '    emit(out,cntp,0 as i64,1 as i64,npr);', '    emit(out,cntp,0 as i64,1 as i64,0 as i64);'),
    ('the worst case drops the sequence factor', 'host',
     '    let worst:i64=(idx_size/kpool)*n_seq_max;', '    let worst:i64=idx_size/kpool;'),
    ('the padded count does not reserve the trailing slot', 'host',
     ' var v:i64=((n+(1 as i64))+(63 as i64))/(64 as i64)*(64 as i64);', ' var v:i64=(n+(63 as i64))/(64 as i64)*(64 as i64);'),
    ('the new-count accessor returns the real count', 'host',
     '    emit(out,cntp,3 as i64,1 as i64,n_new_g);', '    emit(out,cntp,3 as i64,1 as i64,n_new);'),
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
