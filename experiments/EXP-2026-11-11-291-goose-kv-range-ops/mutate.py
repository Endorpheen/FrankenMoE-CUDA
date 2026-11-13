"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-291-goose-kv-range-ops'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('range test includes the upper bound', 'host',
     ' return p>=p0&&p<p1;', ' return p>=p0&&p<=p1;'),
    ('seq_rm forgets to move the head', 'host',
     '  head_update(heads,stream,new_head,n);\n }else{', ' }else{'),
    ('any-sequence rm skips the last stream', 'host',
     '   while stream<n_stream{', '   while stream<n_stream-(1 as i64){'),
    ('seq_keep leaves the old sequence pairs behind', 'host',
     '  sp_rm_cell(a,n,seq_n,i);\n  clear_seq(a,n,i);\n  st(a,seq_off(n,i,s/64 as i64),bit_of(s));',
     '  clear_seq(a,n,i);\n  st(a,seq_off(n,i,s/64 as i64),bit_of(s));'),
    ('seq_keep keeps the ext block on release', 'host',
     '  clear_seq(a,n,i);\n  st(a,pos_off(i),(0 as i64)-1 as i64);\n  clear_ext(a,n,i);',
     '  clear_seq(a,n,i);\n  st(a,pos_off(i),(0 as i64)-1 as i64);'),
    ('seq_cp no longer clamps a negative p0', 'host',
     ' if p0<0 as i64{ p0=0 as i64; }', ''),
    ('seq_cp no longer opens a negative p1', 'host',
     ' if p1<0 as i64{ p1=posmax(); }', ''),
    ('head update moves backwards', 'host',
     '  if new_head<h{ st(hp,stream*(8 as i64),new_head); }', '  if new_head>h{ st(hp,stream*(8 as i64),new_head); }'),
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
