"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-345-goose-kv-state-write-ranges'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('a mirrored cache writes state too', 'host',
     ' if other!=(0 as i64){ done_marker(packet,0 as i64); return true; }', ''),
    ('a partial write of a non-SWA cache proceeds', 'host',
     ' if partial!=(0 as i64)&&swa_none!=(0 as i64){ done_marker(packet,0 as i64); return true; }', ''),
    ('empty cells are written', 'host',
     '   if p!=(0 as i64)-1 as i64{\n    if seq_id==(0 as i64)-1 as i64{ add=1 as i64; }else{',
     '   if true{\n    if seq_id==(0 as i64)-1 as i64{ add=1 as i64; }else{'),
    ('cells of other sequences are written', 'host',
     '     if (at(seqmask,base+i)&((1 as i64)<<seq_id))!=(0 as i64){ add=1 as i64; }', '     add=1 as i64;'),
    ('the whole-cache write applies the SWA filter', 'host',
     '    if seq_id!=(0 as i64)-1 as i64{\n     if masked(n_swa,swa_type,p,smax)!=(0 as i64){ add=0 as i64; }\n    }',
     '    if masked(n_swa,swa_type,p,smax)!=(0 as i64){ add=0 as i64; }'),
    ('the standard window is exclusive at the boundary', 'host',
     '  if p1-p0>=n_swa{ return 1 as i64; }', '  if p1-p0>n_swa{ return 1 as i64; }'),
    ('the chunk start uses p0', 'host',
     '  let start:i64=(p1/n_swa)*n_swa;', '  let start:i64=(p0/n_swa)*n_swa;'),
    ('the symmetric half is doubled', 'host',
     ' let half:i64=n_swa/(2 as i64);', ' let half:i64=n_swa;'),
    ('a stream with no cells is not skipped', 'host',
     '  if count==(0 as i64){\n   emit(out,cntp,6 as i64,s,0 as i64);\n  }else{', '  if false{\n  }else{'),
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
