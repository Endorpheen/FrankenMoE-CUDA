"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-312-goose-hybrid-idx-poolstate'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('clean sentinel becomes a negative position', 'host',
     ' var stale_from:i64=2147483647 as i64;', ' var stale_from:i64=(0 as i64)-1 as i64;'),
    ('representative-row uniqueness is skipped', 'host',
     '    if at(rep_gen,rep)!=gen{', '    if true{'),
    ('stale boundary includes an earlier pool', 'host',
     '    if lastp<stale_from{ lo=mid+(1 as i64); }else{ hi=mid; }', '    if lastp<=stale_from{ lo=mid+(1 as i64); }else{ hi=mid; }'),
    ('position lookup skips an equal first cell', 'host',
     '     if fp<=pp{ lo=mid+(1 as i64); }else{ hi=mid; }', '     if fp<pp{ lo=mid+(1 as i64); }else{ hi=mid; }'),
    ('token outside the pool still marks it', 'host',
     '     if pp<=lastp{ found=kk; }', '     if pp<=lastp+(1 as i64){ found=kk; }'),
    ('order mode maps rank to the wrong pool', 'host',
     '     let kk:i64=r/kpool;', '     let kk:i64=r/(kpool+(1 as i64));'),
    ('graph bound ignores the unused padding slot', 'host',
     ' var cap:i64=kpool_pad(n_pool_real)-(1 as i64);', ' var cap:i64=kpool_pad(n_pool_real);'),
    ('graph bound ignores the sequence count', 'host',
     ' let bound:i64=n_tokens/kpool+n_seqs_unq;', ' let bound:i64=n_tokens/kpool;'),
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
