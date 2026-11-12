"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-339-goose-recurrent-state-clear'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('a whole-cache clear also walks the tensors', 'host',
     '  if seq_id==(0 as i64)-1 as i64{\n   emit(out,cntp,0 as i64,0 as i64,0 as i64);\n   ret=0 as i64;\n  }else{',
     '  if false{\n  }else{'),
    ('the sequence is not removed before clearing', 'host',
     '   emit(out,cntp,1 as i64,seq_id,0 as i64);', ''),
    ('a zero-length section clears the whole row range', 'host',
     '   if cell_count==(0 as i64){\n    ret=0 as i64;\n   }else{', '   if false{\n   }else{'),
    ('the state tensor is cleared before the recurrent one', 'host',
     '     if at(r_present,il)!=(0 as i64){\n      let rr:i64=at(r_rows,il);\n      emit(out,cntp,2 as i64,head*rr,cell_count*rr);\n     }\n     if at(s_present,il)!=(0 as i64){',
     '     if at(s_present,il)!=(0 as i64){\n      let sr0:i64=at(s_rows,il);\n      emit(out,cntp,2 as i64,head*sr0,cell_count*sr0);\n     }\n     if at(r_present,il)!=(0 as i64){\n      let rr:i64=at(r_rows,il);\n      emit(out,cntp,2 as i64,head*rr,cell_count*rr);\n     }\n     if false{'),
    ('null tensors are cleared anyway', 'host',
     '     if at(p_present,il)!=(0 as i64){', '     if true{'),
    ('the cursor does not advance', 'host',
     '   let ni:i64=i_next+(1 as i64);', '   let ni:i64=i_next;'),
    ('the end of the ubatches is not reported', 'host',
     '   if ni>=n_ub{\n    emit(out,cntp,4 as i64,0 as i64,0 as i64);', '   if false{\n    emit(out,cntp,4 as i64,0 as i64,0 as i64);'),
    ('an update is treated as a batch', 'host',
     '    if n_ub==(0 as i64){', '    if false{'),
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
