"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-318-goose-kv-views'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('k stride between heads is one head wide', 'host',
     '  setat(out,5 as i64,ts*a*b);         // nb2: all kv heads of one token', '  setat(out,5 as i64,ts*a);         // nb2: all kv heads of one token'),
    ('k view ignores the stream offset', 'host',
     '  setat(out,7 as i64,ts*a*b*n*s0k);   // offset: one stream stride per s0', '  setat(out,7 as i64,0 as i64);'),
    ('v stride between tokens uses the head width', 'host',
     '    setat(out,5 as i64,ts*gqa);', '    setat(out,5 as i64,ts*a);'),
    ('transposed v first stride drops the head width', 'host',
     '    setat(out,4 as i64,ts*n_kv_*a);', '    setat(out,4 as i64,ts*n_kv_);'),
    ('transposed v stream stride drops the head count', 'host',
     '    setat(out,6 as i64,ts*n_kv_*gqa);', '    setat(out,6 as i64,ts*gqa);'),
    ('n_kv ignores the 256 floor', 'host',
     ' if n_pad_cur<(256 as i64){ n_pad_cur=256 as i64; }', ''),
    ('n_kv does not pad the used position', 'host',
     '     let p:i64=padto(used,n_pad_cur);', '     let p:i64=used;'),
    ('n_kv clamps from below', 'host',
     '     var m:i64=sz;\n     if v<m{ m=v; }', '     var m:i64=sz;\n     if v>m{ m=v; }'),
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
