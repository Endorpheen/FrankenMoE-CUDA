"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-324-goose-kv-read-meta-plan'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('an exactly full cache is rejected', 'host',
     ' if cell_count>size{ done(packet,1 as i64); return true; }', ' if cell_count>=size{ done(packet,1 as i64); return true; }'),
    ('a record with no ids is accepted', 'host',
     '   if nseq!=(1 as i64){ done(packet,2 as i64); return true; }', '   if nseq<(1 as i64){ done(packet,2 as i64); return true; }'),
    ('a short mirrored layout is accepted', 'host',
     '   if sin_cells!=cell_count{ bad=true; }\n   if bad{ done(packet,3 as i64); return true; }\n   var q:i64=0;',
     '   if sin_cells<cell_count{ bad=true; }\n   if bad{ done(packet,3 as i64); return true; }\n   var q:i64=0;'),
    ('a mirrored cell past the cache is accepted', 'host',
     '    if idx>=size{ done(packet,4 as i64); return true; }', '    if idx>size{ done(packet,4 as i64); return true; }'),
    ('a failed search is accepted', 'host',
     '   if find_ok==(0 as i64){ done(packet,5 as i64); return true; }', '   if find_ok<(0 as i64){ done(packet,5 as i64); return true; }'),
    ('an id past the range is accepted', 'host',
     '    if id>=n_seq_max{ done(packet,6 as i64); return true; }', '    if id>n_seq_max{ done(packet,6 as i64); return true; }'),
    ('the destination sequence is not removed', 'host',
     '  emit(out,cntp,0 as i64,0 as i64,0 as i64);', ''),
    ('the extension is never restored', 'host',
     '  if has_ext!=(0 as i64){', '  if false{'),
    ('the head is not reset on a whole-cache restore', 'host',
     '  emit(out,cntp,7 as i64,0 as i64,0 as i64);', ''),
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
