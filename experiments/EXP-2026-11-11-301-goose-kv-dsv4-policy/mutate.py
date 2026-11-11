"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-301-goose-kv-dsv4-policy'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('comp_size rounds down', 'host',
     ' let r:i64=(kv+ratio-(1 as i64))/ratio;', ' let r:i64=kv/ratio;'),
    ('comp_size may be zero', 'host',
     ' if r < (1 as i64){ return 1 as i64; }', ' if r < (0 as i64){ return 1 as i64; }'),
    ('stream offset accepts the first id past the range', 'host',
     ' if seq_id>=n_stream{ return (0 as i64)-1 as i64; }', ' if seq_id>n_stream{ return (0 as i64)-1 as i64; }'),
    ('coupled means any sequence id at all', 'host',
     '  if at(nsid,i) > 1 as i64{ return 1 as i64; }', '  if at(nsid,i) > 0 as i64{ return 1 as i64; }'),
    ('token membership skips the last id', 'host',
     ' while j<k{\n  if at(sid,i*width+j)==seq_id{ return 1 as i64; }', ' while j<k-(1 as i64){\n  if at(sid,i*width+j)==seq_id{ return 1 as i64; }'),
    ('single-unique-sequence fan-out is allowed', 'host',
     ' if n_seqs_unq <= (1 as i64){ return 1 as i64; }', ' if n_seqs_unq < (0 as i64){ return 1 as i64; }'),
    ('K-only tweak leaves the value width alone', 'host',
     '  st(out,(1 as i64)*(8 as i64),a);', '  st(out,(1 as i64)*(8 as i64),a+(1 as i64));'),
    ('used rows ignore the cache size clamp', 'host',
     ' if rows<kv{ return rows; }\n return kv;', ' return rows;'),
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
