"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-327-goose-kv-seq-add-shift'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('a zero shift still walks the cells', 'host',
     ' if shift==(0 as i64){\n  put(packet,3,0,1 as u64); put(packet,3,1,0 as i64 as u64); return true;\n }',
     ''),
    ('a mirrored cache is shifted', 'host',
     ' if other!=(0 as i64){\n  put(packet,3,0,1 as u64); put(packet,3,1,0 as i64 as u64); return true;\n }',
     ''),
    ('the range end is inclusive', 'host',
     '  if p>=p0&&p<p1{ inside=true; }', '  if p>=p0&&p<=p1{ inside=true; }'),
    ('cells without the sequence are shifted', 'host',
     '  if ((at(seqbits,i)&(1 as i64))!=(0 as i64)){ has=true; }', '  has=true;'),
    ('a cell that lands on zero is freed', 'host',
     '   if np<(0 as i64){', '   if np<=(0 as i64){'),
    ('the head always resets to zero', 'host',
     ' var h:i64=0;\n if new_head!=n{ h=new_head; }', ' var h:i64=0;'),
    ('the head keeps the cache size when nothing is freed', 'host',
     ' var h:i64=0;\n if new_head!=n{ h=new_head; }', ' var h:i64=new_head;'),
    ('the per-sequence index is relinked before the shift is emitted', 'host',
     '   emit(out,cntp,2 as i64,i,0 as i64);\n   let np:i64=p+shift;\n   emit(out,cntp,0 as i64,i,np);',
     '   let np:i64=p+shift;\n   emit(out,cntp,0 as i64,i,np);\n   emit(out,cntp,2 as i64,i,0 as i64);'),
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
