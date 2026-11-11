"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-341-goose-hybrid-idx-ops'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('a refusing recurrent cache still touches the indexer', 'host',
     '  if recr_ok==(0 as i64){\n   ret=0 as i64;\n  }else{', '  if false{\n   ret=0 as i64;\n  }else{'),
    ('the indexer is removed before being told the position', 'host',
     '    let st:i64=stale_pos(seq_id,p0,seq_min);\n    emit(out,cntp,0 as i64,1 as i64,0 as i64);\n    stale_set(out,cntp,seq_id,st,stale,n_seq);',
     '    let st:i64=stale_pos(seq_id,p0,seq_min);\n    stale_set(out,cntp,seq_id,st,stale,n_seq);\n    emit(out,cntp,0 as i64,1 as i64,0 as i64);'),
    ('stale_pos never zeroes at the first position', 'host',
     ' if seq_id<(0 as i64)||p0<=seq_min{ return 0 as i64; }', ' if seq_id<(0 as i64){ return 0 as i64; }'),
    ('a negative shift is not accounted for', 'host',
     '      if shift<(0 as i64){ src=p0+shift; }', ''),
    ('seq_keep marks only the kept sequence', 'host',
     '     stale_set(out,cntp,(0 as i64)-1 as i64,0 as i64,stale,n_seq);', '     stale_set(out,cntp,seq_id,0 as i64,stale,n_seq);'),
    ('the minimum is overwritten instead of kept', 'host',
     '  if v<at(stale,seq_id){ setat(stale,seq_id,v); }', '  setat(stale,seq_id,v);'),
    ('the indexer section is written first', 'host',
     '       emit(out,cntp,0 as i64,2 as i64,0 as i64);\n       emit(out,cntp,0 as i64,0 as i64,0 as i64);\n       if has_idx!=(0 as i64){ emit(out,cntp,0 as i64,1 as i64,0 as i64); }',
     '       if has_idx!=(0 as i64){ emit(out,cntp,0 as i64,1 as i64,0 as i64); }\n       emit(out,cntp,0 as i64,2 as i64,0 as i64);\n       emit(out,cntp,0 as i64,0 as i64,0 as i64);'),
    ('a half-failed restore is left as it is', 'host',
     '         emit(out,cntp,3 as i64,2 as i64,0 as i64);\n         emit(out,cntp,3 as i64,0 as i64,0 as i64);',
     '         emit(out,cntp,3 as i64,2 as i64,0 as i64);'),
    ('an empty copied layout still invalidates the pools', 'host',
     '          if sinfos_empty==(0 as i64){ stale_set(out,cntp,seq_id,0 as i64,stale,n_seq); }', '          stale_set(out,cntp,seq_id,0 as i64,stale,n_seq);'),
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
