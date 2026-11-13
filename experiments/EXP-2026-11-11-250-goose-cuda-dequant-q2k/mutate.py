"""Mutation gate for the Q6_K module.

A passing fixture is only meaningful if it fails on a broken body. Each mutation
is a plausible transcription slip; the mutated module is re-emitted with the same
pinned compiler, rebuilt and run against the native oracle. A mutation that
passes is a fixture defect, not a module property.
"""
from pathlib import Path
import subprocess, json, re, shlex

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-250-goose-cuda-dequant-q2k'
S = E/'source'; T = E/'tests'; B = F/'runtime/exp250/build'
g = S/'device.goose'
orig = g.read_text()

MUTATIONS = [
    ('dmin offset 82 -> 80 (reuses d)', 'blk+82 as i64', 'blk+80 as i64'),
    ('qs offset 16 -> 17', 'blk+16 as i64', 'blk+17 as i64'),
    ('second field shift 2 -> 1', '((q>>2)&3)', '((q>>1)&3)'),
    ('destination 128*n -> 127*n', '128*n+l', '127*n+l'),
    ('last scale is+6 -> is+4', 'blk+is+6 as i64', 'blk+is+4 as i64'),
]

db = json.loads(Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/compile_commands.json').read_text())
r = next(x for x in db if x['file'].endswith('/convert.cu'))

def build_cmd():
    cmd = shlex.split(r['command'])
    cmd.insert(1, '-I'+str(S))
    cmd.insert(1, '-I'+str(Path(r['file']).parent))
    i = cmd.index('-c'); cmd[i+1] = str(T/'gpu-fixture.cu'); del cmd[i]
    cmd[cmd.index('-o')+1] = str(B/'gpu-fixture')
    return cmd

def step(args, cwd, timeout=1800):
    return subprocess.run(list(map(str, args)), cwd=str(cwd), capture_output=True, text=True, timeout=timeout)

rows = []
try:
    for name, old, new in MUTATIONS:
        assert old in orig, ('mutation anchor missing', name, old)
        g.write_text(orig.replace(old, new, 1))
        e = step(['python3', E/'emit_sources.py'], F)
        if e.returncode:
            rows.append(dict(mutation=name, status='EMIT_FAILED', stderr=e.stderr[-2000:]))
            print(name, 'EMIT_FAILED'); continue
        c = step(build_cmd(), Path(r['directory']))
        if c.returncode:
            rows.append(dict(mutation=name, status='COMPILE_FAILED', stderr=(c.stdout+c.stderr)[-2000:]))
            print(name, 'COMPILE_FAILED'); continue
        q = step([B/'gpu-fixture'], B)
        m = re.search(r'failures=(\d+)', q.stdout)
        fails = int(m.group(1)) if m else -1
        rows.append(dict(mutation=name, status='CAUGHT' if fails > 0 else 'MISSED', failures=fails))
        print(name, 'failures=', fails)
finally:
    g.write_text(orig)

assert step(['python3', E/'emit_sources.py'], F).returncode == 0, 'restore emission failed'
step(build_cmd(), Path(r['directory']))
(E/'mutations.json').write_text(json.dumps(rows, indent=2)+'\n')
missed = [x for x in rows if x['status'] != 'CAUGHT']
print('MISSED' if missed else 'ALL_MUTATIONS_CAUGHT', len(rows))
raise SystemExit(1 if missed else 0)
