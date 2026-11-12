"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-298-goose-kv-iswa-policy'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('size padding rounds down', 'host',
     ' let r:i64=(x+(255 as i64))/(256 as i64);', ' let r:i64=x/(256 as i64);'),
    ('size ignores the micro-batch', 'host',
     '  let want:i64=n_swa*sc+n_ubatch;', '  let want:i64=n_swa*sc;'),
    ('full-size SWA cache is not honoured', 'host',
     '  if swa_full{ size_swa=size_base; }', ''),
    ('unified caches do not scale by the sequence count', 'host',
     '  if unified{ sc=n_seq_max; }', ''),
    ('base and SWA filters are swapped', 'host',
     '     if !sw{ b=1 as i64; }', '     if sw{ b=1 as i64; }'),
    ('seq_rm succeeds if either cache does', 'host',
     '    if rb!=(0 as i64)&&rs!=(0 as i64){ ret=1 as i64; }', '    if rb!=(0 as i64)||rs!=(0 as i64){ ret=1 as i64; }'),
    ('breakdown overwrites instead of adding', 'host',
     '       st(out,(found*(2 as i64)+1 as i64)*(8 as i64),old+val);', '       st(out,(found*(2 as i64)+1 as i64)*(8 as i64),val);'),
    ('breakdown keeps insertion order', 'host',
     ' // insertion sort by key', ' ret=n;\n put(packet,3,0,1 as u64);\n put(packet,3,1,ret as u64);\n return true;\n // insertion sort by key'),
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
