"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-344-goose-kv-state-read-sinfo'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('a mirrored cache reads state too', 'host',
     '  if other!=(0 as i64){ done(packet,cntp,0 as i64); return true; }', ''),
    ('a partial-only restore of a non-SWA cache proceeds', 'host',
     '  if partial!=(0 as i64)&&swa_none!=(0 as i64){ done(packet,cntp,0 as i64); return true; }', ''),
    ('a mirrored layout with the wrong stream count is accepted', 'host',
     '  if mirror!=(0 as i64)&&mirror_size!=n_stream{ done(packet,cntp,1 as i64); return true; }', ''),
    ('a stream count mismatch is accepted', 'host',
     '  if n_stream_cur!=n_stream{ done(packet,cntp,2 as i64); return true; }', ''),
    ('the whole cache is not cleared up front', 'host',
     '   emit(out,cntp,0 as i64,0 as i64,0 as i64);', ''),
    ('a zero-cell stream ignores its mirrored layout', 'host',
     '    if mirror!=(0 as i64){\n     if at(mirror_empty,s)==(0 as i64){', '    if false{\n     if at(mirror_empty,s)==(0 as i64){'),
    ('a failed restore leaves the cells in place', 'host',
     '     emit(out,cntp,4 as i64,strm,0 as i64);\n     done(packet,cntp,4 as i64);', '     done(packet,cntp,4 as i64);'),
    ('a single-sequence restore uses the per-stream index', 'host',
     '    if seq_id!=(0 as i64)-1 as i64{ strm=at(s2s,seq_id); }', ''),
    ('the transposed clear forgets the cache stride', 'host',
     '         emit(out,cntp,6 as i64,(head+j*cells_size)*v_el,n_idx*v_el);', '         emit(out,cntp,6 as i64,(head+j)*v_el,n_idx*v_el);'),
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
