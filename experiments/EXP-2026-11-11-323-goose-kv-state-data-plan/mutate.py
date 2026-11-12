"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-323-goose-kv-state-data-plan'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('the V rotation size is not written', 'host',
     ' emit(out,cntp,0 as i64,3 as i64,n_rot_v,0 as i64);', ''),
    ('the layout flag comes second', 'host',
     ' emit(out,cntp,0 as i64,0 as i64,v_trans,0 as i64);\n emit(out,cntp,0 as i64,1 as i64,n_layer,0 as i64);',
     ' emit(out,cntp,0 as i64,1 as i64,v_trans,0 as i64);\n emit(out,cntp,0 as i64,0 as i64,n_layer,0 as i64);'),
    ('key offsets drop the row size', 'host',
     '   emit(out,cntp,2 as i64,lo*k_row,(hi-lo)*k_row,0 as i64);', '   emit(out,cntp,2 as i64,lo,(hi-lo)*k_row,0 as i64);'),
    ('key sizes drop the row size', 'host',
     '   emit(out,cntp,2 as i64,lo*k_row,(hi-lo)*k_row,0 as i64);', '   emit(out,cntp,2 as i64,lo*k_row,(hi-lo),0 as i64);'),
    ('plain V offsets use the key row size', 'host',
     '    emit(out,cntp,2 as i64,lo*v_row,(hi-lo)*v_row,0 as i64);', '    emit(out,cntp,2 as i64,lo*k_row,(hi-lo)*v_row,0 as i64);'),
    ('transposed V offsets forget the cache stride', 'host',
     '     emit(out,cntp,2 as i64,(lo+j*kv_size)*v_el,(hi-lo)*v_el,0 as i64);', '     emit(out,cntp,2 as i64,(lo+j)*v_el,(hi-lo)*v_el,0 as i64);'),
    ('transposed V sizes drop the element size', 'host',
     '     emit(out,cntp,2 as i64,(lo+j*kv_size)*v_el,(hi-lo)*v_el,0 as i64);', '     emit(out,cntp,2 as i64,(lo+j*kv_size)*v_el,(hi-lo),0 as i64);'),
    ('the GQA width is not written for a transposed V', 'host',
     '   emit(out,cntp,1 as i64,3 as i64,gqa,0 as i64);', ''),
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
