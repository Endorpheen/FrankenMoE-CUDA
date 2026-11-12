"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-294-goose-recurrent-seq-ops'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('seq_cp ignores cell zero', 'host',
     ' if td>=0 as i64{', ' if td>0 as i64{'),
    ('seq_cp keeps the destination bit', 'host',
     '  seq_del(a,n,td,sw,did);', ''),
    ('seq_cp does not move the destination tail', 'host',
     '  st(a,tail_off(n,did),ts);', ''),
    ('seq_cp forgets to drop the used count', 'host',
     '   st(a,used_off(n,sw),ld(a,used_off(n,sw))-(1 as i64));', ''),
    ('seq_keep leaves the other tails in place', 'host',
     '  if i!=sid{ st(a,tail_off(n,i),(0 as i64)-1 as i64); }', ''),
    ('seq_keep does not move the head', 'host',
     '  if new_head<ld(a,head_off(n,sw)){ st(a,head_off(n,sw),new_head); }', ''),
    ('range test includes p0', 'host',
     ' if p<p0{ return (0 as i64)-1 as i64; }', ' if p<=p0{ return (0 as i64)-1 as i64; }'),
    ('seq_div divides by one more', 'host',
     '        st(a,pos_off(c),ld(a,pos_off(c))/d);', '        st(a,pos_off(c),ld(a,pos_off(c))/(d+(1 as i64)));'),
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
