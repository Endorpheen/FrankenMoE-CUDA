"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-325-goose-kv-read-data-plan'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('keys are split into single-cell runs', 'host',
     '    if at(idxs,i1)==at(idxs,i1-(1 as i64))+(1 as i64){ i1=i1+1 as i64; }else{ stop1=true; }',
     '    stop1=true;'),
    ('plain values are split into single-cell runs', 'host',
     '     if at(idxs,i3)==at(idxs,i3-(1 as i64))+(1 as i64){ i3=i3+1 as i64; }else{ stop2=true; }',
     '     stop2=true;'),
    ('key offsets drop the row size', 'host',
     '   emit(out,cntp,from*k_row,(to-from)*k_row,0 as i64);', '   emit(out,cntp,from,(to-from)*k_row,0 as i64);'),
    ('plain value offsets use the key row size', 'host',
     '    emit(out,cntp,f2*v_row,(t2-f2)*v_row,0 as i64);', '    emit(out,cntp,f2*k_row,(t2-f2)*v_row,0 as i64);'),
    ('transposed offsets forget the cache stride', 'host',
     '     emit(out,cntp,(f4+j*cells_size)*v_el,(t4-f4)*v_el,j);', '     emit(out,cntp,(f4+j)*v_el,(t4-f4)*v_el,j);'),
    ('transposed sizes drop the element size', 'host',
     '     emit(out,cntp,(f4+j*cells_size)*v_el,(t4-f4)*v_el,j);', '     emit(out,cntp,(f4+j*cells_size)*v_el,(t4-f4),j);'),
    ('the layer count is not validated', 'host',
     ' if nl_ref!=nl{ reject(packet,1 as i64); return true; }', ''),
    ('the V layout is not validated', 'host',
     ' if vtr_ref!=vtr{ reject(packet,3 as i64); return true; }', ''),
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
