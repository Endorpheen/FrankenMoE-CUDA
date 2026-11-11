"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-303-goose-kv-dsv4-comp-plan-overlap'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('previous window walks backwards', 'host',
     '      let src:i64=source_idx(src_start-ratio+j,seq_id,state_size,n_tokens,posarr,width,sid,nsid,n_stream,state_rows);',
     '      let src:i64=source_idx(src_start-ratio-j,seq_id,state_size,n_tokens,posarr,width,sid,nsid,n_stream,state_rows);'),
    ('current window overwrites the previous half', 'host',
     '  setat(a_ridx,cnt_prev+cc,at(a_cur,cc));', '  setat(a_ridx,cc,at(a_cur,cc));'),
    ('read count keeps only the previous half', 'host',
     ' cnt_read=cnt_prev+cnt_cur;', ' cnt_read=cnt_prev;'),
    ('dummy blocks fill only one half', 'host',
     '    j=0;\n    while j<ratio{\n     setat(a_cur,cnt_cur,src);\n     cnt_cur=cnt_cur+1 as i64;\n     j=j+1 as i64;\n    }',
     '    j=0;'),
    ('previous half is never collected', 'host',
     '      setat(a_prev,cnt_prev,src);\n      cnt_prev=cnt_prev+1 as i64;', ''),
    ('previous window starts at the current block', 'host',
     '      let src:i64=source_idx(src_start-ratio+j,seq_id,state_size,n_tokens,posarr,width,sid,nsid,n_stream,state_rows);',
     '      let src:i64=source_idx(src_start+j,seq_id,state_size,n_tokens,posarr,width,sid,nsid,n_stream,state_rows);'),
    ('write index uses the block offset instead of the block number', 'host',
     '     setat(a_widx,cnt_write,cache_off+p/ratio);', '     setat(a_widx,cnt_write,cache_off+p%ratio);'),
    ('duplicate token positions take the first token', 'host',
     '   if token_has_seq(i,seq_id,width,sid,nsid){ best=i; }', '   if token_has_seq(i,seq_id,width,sid,nsid){ if best < (0 as i64){ best=i; } }'),
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
