"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-338-goose-recurrent-state-read'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('a single-sequence record may carry one id', 'host',
     '   if nseq!=(0 as i64){ done(packet,cntp,2 as i64); return true; }', '   if nseq!=(1 as i64){ done(packet,cntp,2 as i64); return true; }'),
    ('an empty single-sequence section still reads records', 'host',
     '  if cell_count==(0 as i64){ done(packet,cntp,0 as i64); return true; }', ''),
    ('a failed slot search is ignored', 'host',
     '  if find_ok==(0 as i64){ done(packet,cntp,3 as i64); return true; }', ''),
    ('the whole cache is not cleared before reading', 'host',
     '   emit(out,cntp,2 as i64,0 as i64,0 as i64);', ''),
    ('a second tail is accepted', 'host',
     '     if at(tail,id)!=(0 as i64)-1 as i64{ done(packet,cntp,5 as i64); return true; }', ''),
    ('an id above the range is accepted', 'host',
     '     if id<(0 as i64)||id>=n_seq_max{ done(packet,cntp,4 as i64); return true; }', '     if id<(0 as i64)||id>n_seq_max{ done(packet,cntp,4 as i64); return true; }'),
    ('a transposed state is accepted', 'host',
     '    if s_trans!=(0 as i64){ done(packet,cntp,3 as i64); return true; }', ''),
    ('null layers are read anyway', 'host',
     '     if at(r_present,il)!=(0 as i64){', '     if true{'),
    ('a zero-length section still reads tensors', 'host',
     '      if cell_count>(0 as i64){ emit(out,cntp,8 as i64,h2*rr,cell_count*rr); }', '      emit(out,cntp,8 as i64,h2*rr,cell_count*rr);'),
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
