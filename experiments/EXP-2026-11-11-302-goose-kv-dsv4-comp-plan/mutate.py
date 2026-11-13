"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-302-goose-kv-dsv4-comp-plan'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('visible blocks divide by the wrong ratio', 'host',
     '   let nv:i64=(p+(1 as i64))/ratio;', '   let nv:i64=(p+(1 as i64))/(ratio+(1 as i64));'),
    ('state position uses the wrong modulus', 'host',
     '   setat(a_spos,cnt_spos,p%ratio);', '   setat(a_spos,cnt_spos,p%(ratio+(1 as i64)));'),
    ('write index uses the block offset instead of the block number', 'host',
     '     setat(a_widx,cnt_write,cache_off+p/ratio);', '     setat(a_widx,cnt_write,cache_off+p%ratio);'),
    ('duplicate token positions take the first token', 'host',
     '   if token_has_seq(i,seq_id,width,sid,nsid){ best=i; }', '   if token_has_seq(i,seq_id,width,sid,nsid){ if best < (0 as i64){ best=i; } }'),
    ('persist keeps the earlier position', 'host',
     '     if p>at(scratch,found*(3 as i64)+(2 as i64)){', '     if p<at(scratch,found*(3 as i64)+(2 as i64)){'),
    ('dummy write targets the wrong last slot', 'host',
     '     setat(a_widx,cnt_write,cache_off+kv_size-(1 as i64));', '     setat(a_widx,cnt_write,cache_off+kv_size-(2 as i64));'),
    ('n_kv keeps the last block instead of the maximum', 'host',
     '   if nv>n_kv{ n_kv=nv; }', '   n_kv=nv;'),
    ('coupled sets still fan out', 'host',
     ' if coupled{ return 1 as i64; }', ' if false{ return 1 as i64; }'),
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
