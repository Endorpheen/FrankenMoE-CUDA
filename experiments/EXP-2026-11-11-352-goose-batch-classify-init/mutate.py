"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-352-goose-batch-classify-init'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('an entry with neither token nor embedding is accepted', 'host',
     '  if t==(0 as i64)&&e==(0 as i64){ done(packet,cntp,1 as i64); return true; }', ''),
    ('a non-uniform state batch is accepted', 'host',
     '  if at(hs_ptr,i)!=state_expected{ done(packet,cntp,2 as i64); return true; }', ''),
    ('both-token-and-embedding entries may be mixed', 'host',
     ' if n_both>(0 as i64)&&n_both!=n{ done(packet,cntp,3 as i64); return true; }', ''),
    ('a mixed batch is accepted where it is forbidden', 'host',
     ' if mixed!=(0 as i64)&&allow_mixed==(0 as i64){ done(packet,cntp,4 as i64); return true; }', ''),
    ('a token id out of range is accepted', 'host',
     '    if v<(0 as i64)||v>=n_vocab{ done(packet,cntp,5 as i64); return true; }', ''),
    ('an embedding-marked entry reads its token id', 'host',
     '   if mixed!=(0 as i64){ emb_marked=at(has_embd,i3); }', ''),
    ('a sequence id out of range is accepted', 'host',
     '   if sid<(0 as i64)||sid>=n_seq_max{ done(packet,cntp,6 as i64); return true; }', ''),
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
