#!/usr/bin/env python3
"""EXP265: A/B the MoE cache copy switch on one DSO under the 3072 MiB cache profile."""
from pathlib import Path
import hashlib, json, os, signal, socket, subprocess, sys, time, urllib.request

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-269-goose-checkpoint-policy-live'
STAGE = E / 'build/stage'
STAGE.mkdir(parents=True, exist_ok=True)
REF = F / 'runtime/exp244/results-apex-or244-or-goose'
BASE = F / 'runtime/reconstructed/build/stage-run'
LIB = E / 'build/libllama-server-impl.so'

for p in BASE.iterdir():
    q = STAGE / p.name
    if q.is_symlink() or q.exists():
        q.unlink()
    q.symlink_to(p.resolve())
q = STAGE / 'libllama-server-impl.so'
if q.is_symlink() or q.exists():
    q.unlink()
q.symlink_to(LIB.resolve())
server = STAGE / 'llama-server'
if server.is_symlink() or server.exists():
    server.unlink()
server.symlink_to((F / 'runtime/exp244/measured-apex-or244-or-goose/run/llama-server').resolve())

argv_base = json.loads((REF / 'argv.json').read_text())
argv_base[0] = str(server)

env_base = json.loads((REF / 'environment.json').read_text())
env_base['LD_LIBRARY_PATH'] = str(STAGE)
env_base.pop('LD_PRELOAD', None)
env_base.pop('GGML_GOOSE_MOE_CACHE_COPY', None)
env_base.pop('GGML_GOOSE_CTX_CHECKPOINTS', None)

results = {}
for arm in ['goose', 'native']:
    out = E / 'build' / ('run-' + arm); out.mkdir(parents=True, exist_ok=True)
    with socket.socket() as s:
        if s.connect_ex(('127.0.0.1', 8081)) == 0:
            print('port busy'); sys.exit(1)
    env = dict(env_base)
    env['GGML_GOOSE_CTX_CHECKPOINTS'] = '1' if arm == 'goose' else '0'
    log = open(out / 'server.log', 'wb')
    proc = subprocess.Popen(argv_base, cwd=str(STAGE), env=env, stdout=log, stderr=subprocess.STDOUT)

    def ready(t=600):
        t0 = time.time()
        while time.time() - t0 < t:
            if proc.poll() is not None:
                return False
            try:
                with urllib.request.urlopen('http://127.0.0.1:8081/health', timeout=3) as r:
                    if json.loads(r.read()).get('status') == 'ok':
                        return True
            except Exception:
                pass
            time.sleep(2)
        return False

    try:
        if not ready():
            print(arm, 'not ready'); print((out / 'server.log').read_text()[-2500:]); sys.exit(1)
        req = json.loads((REF / 'request.json').read_text())
        r = urllib.request.Request('http://127.0.0.1:8081/completion', data=json.dumps(req).encode(),
                                   headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(r, timeout=1800) as resp:
            first = json.loads(resp.read())
        (out / 'response.json').write_text(json.dumps(first, indent=2) + '\n')
        results[arm] = dict(
            sha=hashlib.sha256((first.get('content') or '').encode()).hexdigest(),
            tokens=first.get('tokens'),
            probs=first.get('completion_probabilities'),
            timings=first.get('timings'),
        )
    finally:
        if proc.poll() is None:
            proc.send_signal(signal.SIGINT)
            try:
                proc.wait(timeout=90)
            except subprocess.TimeoutExpired:
                proc.kill()
        log.close()
        print(arm, 'server stopped rc =', proc.returncode)

a, b = results['goose'], results['native']
summary = {
    'goose_sha': a['sha'], 'native_sha': b['sha'],
    'content_equal': a['sha'] == b['sha'],
    'tokens_equal': a['tokens'] == b['tokens'],
    'probs_equal': a['probs'] == b['probs'],
    'goose_timings': a['timings'], 'native_timings': b['timings'],
}
(E / 'ab-summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps({k: v for k, v in summary.items() if 'timings' not in k}, indent=2))
