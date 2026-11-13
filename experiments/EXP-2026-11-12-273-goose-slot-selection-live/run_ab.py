#!/usr/bin/env python3
"""EXP271 A/B: two clients (one defers) and one cancel, on the same DSO with the queue switch flipped."""
from pathlib import Path
import hashlib, json, signal, socket, subprocess, sys, threading, time, urllib.request

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-273-goose-slot-selection-live'
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
srv = STAGE / 'llama-server'
if srv.is_symlink() or srv.exists():
    srv.unlink()
srv.symlink_to((F / 'runtime/exp244/measured-apex-or244-or-goose/run/llama-server').resolve())

argv = json.loads((REF / 'argv.json').read_text())
argv[0] = str(srv)
env_base = json.loads((REF / 'environment.json').read_text())
env_base['LD_LIBRARY_PATH'] = str(STAGE); env_base.pop('LD_PRELOAD', None)
env_base.pop('GGML_GOOSE_SLOT_SELECT', None)
req = json.loads((REF / 'request.json').read_text())


def post(path, payload, timeout=1800):
    r = urllib.request.Request('http://127.0.0.1:8081' + path, data=json.dumps(payload).encode(),
                               headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(r, timeout=timeout) as resp:
        return json.loads(resp.read())


results = {}
for arm in ['goose', 'native']:
    out = E / 'build' / ('run-' + arm); out.mkdir(parents=True, exist_ok=True)
    if socket.socket().connect_ex(('127.0.0.1', 8081)) == 0:
        print('port busy'); sys.exit(1)
    env = dict(env_base); env['GGML_GOOSE_SLOT_SELECT'] = '1' if arm == 'goose' else '0'
    log = open(out / 'server.log', 'wb')
    proc = subprocess.Popen(argv, cwd=str(STAGE), env=env, stdout=log, stderr=subprocess.STDOUT)

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
            print(arm, 'not ready'); print((out / 'server.log').read_text()[-2000:]); sys.exit(1)
        got = {}

        def client_a():
            got['a'] = post('/completion', req)

        def client_b():
            time.sleep(2)
            got['b'] = post('/completion', req)

        def client_c():
            time.sleep(1)
            body = dict(req); body['stream'] = True
            r = urllib.request.Request('http://127.0.0.1:8081/completion', data=json.dumps(body).encode(),
                                       headers={'Content-Type': 'application/json'})
            try:
                with urllib.request.urlopen(r, timeout=60) as resp:
                    for i, _line in enumerate(resp):
                        if i >= 3:
                            break
            except Exception:
                pass
            got['c'] = 'disconnected'

        ta = threading.Thread(target=client_a); tb = threading.Thread(target=client_b)
        tc = threading.Thread(target=client_c)
        ta.start(); tb.start(); tc.start()
        ta.join(); tb.join(); tc.join()

        results[arm] = {k: (hashlib.sha256((v.get('content') or '').encode()).hexdigest() if isinstance(v, dict) else v)
                        for k, v in got.items()}
        results[arm]['a_tokens'] = got['a'].get('tokens')
        results[arm]['a_probs'] = got['a'].get('completion_probabilities')
        results[arm]['b_tokens'] = got['b'].get('tokens') if 'b' in got else None
        results[arm]['b_probs'] = got['b'].get('completion_probabilities') if 'b' in got else None
    finally:
        if proc.poll() is None:
            proc.send_signal(signal.SIGINT)
            try:
                proc.wait(timeout=120)
            except subprocess.TimeoutExpired:
                proc.kill()
        log.close()
        print(arm, 'stopped rc =', proc.returncode)

g, n = results['goose'], results['native']
summary = {
    'a_sha_equal': g.get('a') == n.get('a'),
    'a_tokens_equal': g['a_tokens'] == n['a_tokens'],
    'a_probs_equal': g['a_probs'] == n['a_probs'],
    'b_present_both': 'b' in g and 'b' in n,
    'b_sha_equal': g.get('b') == n.get('b'),
    'b_tokens_equal': g.get('b_tokens') == n.get('b_tokens'),
    'b_probs_equal': g.get('b_probs') == n.get('b_probs'),
    'a_sha': g.get('a'),
}
(E / 'ab-summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps(summary, indent=2))
