"""Mutation gate for the Q6_K module.

A passing fixture is only meaningful if it fails on a broken body. Each mutation
is a plausible transcription slip; the mutated module is re-emitted with the same
pinned compiler, rebuilt and run against the native oracle. A mutation that
passes is a fixture defect, not a module property.
"""
from pathlib import Path
import subprocess, json, re, shlex

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-255-goose-cuda-dequant-iq2xxs'
S = E/'source'; T = E/'tests'; B = F/'runtime/exp255/build'
g = S/'device.goose'
orig = g.read_text()

MUTATIONS = [
    ('q2 stride 8*ib -> 4*ib', 'blk+2 as i64+8*ib', 'blk+2 as i64+4*ib'),
    ('destination 8*il -> 4*il', '32*ib+8*il', '32*ib+4*il'),
    ('sign index shift 7*il -> 6*il', 'sh7:u32=(7*il) as u32', 'sh7:u32=(6*il) as u32'),
    ('scale factor 0.25 -> 0.5', '(0.25 as f32)', '(0.5 as f32)'),
    ('sign is ignored', 'if b>0 as i64{return -1.0 as f32;}', 'if b>0 as i64{return 1.0 as f32;}'),
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
