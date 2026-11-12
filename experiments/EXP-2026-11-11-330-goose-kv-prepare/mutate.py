"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-330-goose-kv-prepare'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('the undo never runs', 'host',
     '  var ru:i64=done_ub-(1 as i64);\n  while ru>=(0 as i64){',
     '  var ru:i64=done_ub-(1 as i64);\n  while ru>=0 as i64&&false{'),
    ('a failed prepare returns a partial result', 'host',
     '  put(packet,3,1,((0 as i64)-1 as i64) as u64);', '  put(packet,3,1,done_ub as u64);'),
    ('the head is not restored', 'host',
     '   var s:i64=0;\n   while s<n_stream{\n    emit(out,cntp,4 as i64,s,at(heads,ru*hs+s));',
     '   var s:i64=0;\n   while s<0 as i64{\n    emit(out,cntp,4 as i64,s,at(heads,ru*hs+s));'),
    ('every ubatch restores the last head snapshot', 'host',
     '    emit(out,cntp,4 as i64,s,at(heads,ru*hs+s));', '    emit(out,cntp,4 as i64,s,at(heads,s));'),
    ('cells are not snapshotted', 'host',
     '    emit(out,cntp,1 as i64,off+c,0 as i64);', '    setat(ub_ncells,0 as i64,at(ub_ncells,0 as i64));'),
    ('the ubatch is never applied', 'host',
     '   emit(out,cntp,2 as i64,u,0 as i64);', ''),
    ('the loop continues past a failed search', 'host',
     '  if at(ub_find,u)==(0 as i64){\n   failed=1 as i64;\n   u=n_ub;\n  }else{',
     '  if at(ub_find,u)==(0 as i64){\n   failed=1 as i64;\n   u=u+1 as i64;\n  }else{'),
    ('the undo restores the newest cells first', 'host',
     '   let off:i64=at(cell_off,ru);', '   let off:i64=at(cell_off,done_ub-(1 as i64)-ru);'),
    ('a successful search is not recorded', 'host',
     '   emit(out,cntp,0 as i64,u,0 as i64);', ''),
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
