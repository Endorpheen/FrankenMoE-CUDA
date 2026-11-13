from pathlib import Path
import subprocess, json, shlex, os
F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-285-goose-cuda-softmax-coop'
S = E/'source'; T = E/'tests'
B = F/'runtime/exp285/build'; B.mkdir(parents=True, exist_ok=True)
rows = []
env = dict(os.environ)
env['LD_LIBRARY_PATH'] = '/opt/cuda/targets/x86_64-linux/lib:/usr/local/cuda/lib64:' + env.get('LD_LIBRARY_PATH','')

def run(label, args, cwd=B):
    q = subprocess.run(list(map(str, args)), cwd=str(cwd), env=env,
                       capture_output=True, text=True, timeout=1800)
    rows.append(dict(label=label, argv=[str(x) for x in args], cwd=str(cwd), exit_code=q.returncode))
    (E/'local-test-commands.json').write_text(json.dumps(rows, indent=2)+'\n')
    (B/(label+'.stdout')).write_text(q.stdout)
    (B/(label+'.stderr')).write_text(q.stderr)
    print('---', label, 'rc=', q.returncode, flush=True)
    out = (q.stdout + q.stderr).strip()
    if out: print(out[-4000:], flush=True)
    if q.returncode and q.returncode != 77:
        raise SystemExit(q.returncode)
    return q

db = json.loads(Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/compile_commands.json').read_text())
r = next(x for x in db if x['file'].endswith('/convert.cu'))
cmd = shlex.split(r['command'])
cmd.insert(1, '-I'+str(S))
cmd.insert(1, '-I'+str(E/'reference'))
cmd.insert(1, '-I'+str(Path(r['file']).parent))
i = cmd.index('-c'); cmd[i+1] = str(T/'gpu-fixture.cu'); del cmd[i]
cmd[cmd.index('-o')+1] = str(B/'gpu-fixture')
run('gpu-fixture-build', cmd, Path(r['directory']))
run('gpu-fixture', [B/'gpu-fixture'])

run('host-fixture-build', ['gcc-13','-std=gnu11','-O2','-I'+str(S), T/'host-fixture.c','-lm','-o',B/'host-fixture'])
run('host-fixture', [B/'host-fixture'])
print('EXP285_LOCAL_GATES_PASS')
