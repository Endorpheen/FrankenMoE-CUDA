"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-292-goose-kv-seq-cp-cross'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('restored position adds the shift', 'host',
     '    p=p-sh;', '    p=p+sh;'),
    ('copied cell is not marked used', 'host',
     '   st(c1,used_off(n,i),1 as i64);', '   st(c1,used_off(n,i),0 as i64);'),
    ('initial position is off by one', 'host',
     '   st(c1,pos_off(i),p);', '   st(c1,pos_off(i),p+(1 as i64));'),
    ('shift is not re-applied after the copy', 'host',
     '    let np:i64=p+sh;', '    let np:i64=p;'),
    ('reset forgets the sequence counts', 'host',
     ' while s<seq_n{ set_scnt(a,n,seq_n,s,0 as i64); s=s+1 as i64; }',
     ' while s<seq_n{ s=s+1 as i64; }'),
    ('reset keeps the ext block', 'host',
     '  st(a,ext_off(n,i,0 as i64),0 as i64);\n  st(a,ext_off(n,i,1 as i64),0 as i64);\n  st(a,ext_off(n,i,2 as i64),0 as i64);',
     ''),
    ('destination head is left alone', 'host',
     '  st(heads,s1*(8 as i64),ld(heads,s0*(8 as i64)));', ''),
    ('copy filters on the destination sequence', 'host',
     '  if seq_has(c0,n,i,wd,sid){', '  if seq_has(c0,n,i,wd,did){'),
    ('full-buffer test is off by one', 'host',
     '  if p0+(1 as i64)<size{ is_full=false; }', '  if p0<size{ is_full=false; }'),
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
