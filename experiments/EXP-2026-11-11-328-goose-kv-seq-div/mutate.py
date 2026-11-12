"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-328-goose-kv-seq-div'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('a division by one still walks the cells', 'host',
     '  if d==(1 as i64){ put(packet,3,0,1 as u64); put(packet,3,1,0 as i64 as u64); return true; }', ''),
    ('a mirrored cache is divided', 'host',
     '  if other!=(0 as i64){ put(packet,3,0,1 as u64); put(packet,3,1,0 as i64 as u64); return true; }\n  if d==(1 as i64){',
     '  if d==(1 as i64){'),
    ('the shift gains the new position instead of the difference', 'host',
     '    emit(out,cntp,3 as i64,i,p-np);', '    emit(out,cntp,3 as i64,i,np);'),
    ('the index is relinked before the division', 'host',
     '    let np:i64=p/d;\n    emit(out,cntp,1 as i64,i,np);\n    emit(out,cntp,3 as i64,i,p-np);\n    emit(out,cntp,2 as i64,i,0 as i64);',
     '    let np:i64=p/d;\n    emit(out,cntp,2 as i64,i,0 as i64);\n    emit(out,cntp,1 as i64,i,np);\n    emit(out,cntp,3 as i64,i,p-np);'),
    ('the minimum is computed as a maximum', 'host',
     '       if p<m{ m=p; }', '       if p>m{ m=p; }'),
    ('an empty sequence reports zero instead of nothing', 'host',
     '    if found==(0 as i64){ m=(0 as i64)-1 as i64; }\n    ret=m;', '    if found==(0 as i64){ m=0 as i64; }\n    ret=m;'),
    ('the maximum ignores the sequence bits', 'host',
     '       if ((at(seqbits,i)&(1 as i64))!=(0 as i64)){\n        if p>m{ m=p; }',
     '       if true{\n        if p>m{ m=p; }'),
    ('a mirrored cache no longer delegates for the extremes', 'host',
     '   if other!=(0 as i64){ ret=other_min; }else{', '   if false{ ret=other_min; }else{'),
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
