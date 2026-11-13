#!/usr/bin/env python3
"""EXP266: live rewind check on the accepted runtime -- completion, slot erase, completion again.

P5-07 requires exact saved state and final outputs on reuse/rewind. Save/restore already has a witness; this covers the
rewind path, which reaches the sequence-removal and clear decisions instead.
"""
from pathlib import Path
import hashlib, json, signal, socket, subprocess, sys, time, urllib.request

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-266-apex-rewind-live'
OUT = E / 'run'; OUT.mkdir(parents=True, exist_ok=True)
RUN = F / 'runtime/reconstructed/build/stage-run'
REF = F / 'runtime/exp244/results-apex-or244-or-goose'

argv = json.loads((REF / 'argv.json').read_text())
slots = OUT / 'slots'; slots.mkdir(exist_ok=True)
argv[0] = str(RUN / 'llama-server')
argv += ['--slot-save-path', str(slots)]
env = json.loads((REF / 'environment.json').read_text())
env['LD_LIBRARY_PATH'] = str(RUN); env.pop('LD_PRELOAD', None)
with socket.socket() as s:
    if s.connect_ex(('127.0.0.1', 8081)) == 0:
        print('port busy'); sys.exit(1)
log = open(OUT / 'server.log', 'wb')
proc = subprocess.Popen(argv, cwd=str(RUN), env=env, stdout=log, stderr=subprocess.STDOUT)


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


def post(path, payload, timeout=1800):
    req = urllib.request.Request('http://127.0.0.1:8081' + path, data=json.dumps(payload).encode(),
                                 headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return json.loads(resp.read())


try:
    if not ready():
        print('not ready'); print((OUT / 'server.log').read_text()[-2500:]); sys.exit(1)
    req = json.loads((REF / 'request.json').read_text())
    first = post('/completion', req)
    erased = post('/slots/0?action=erase', {})
    second = post('/completion', req)
    summary = {
        'first_sha': hashlib.sha256((first.get('content') or '').encode()).hexdigest(),
        'second_sha': hashlib.sha256((second.get('content') or '').encode()).hexdigest(),
        'tokens_equal': first.get('tokens') == second.get('tokens'),
        'probs_equal': (first.get('completion_probabilities') or []) == (second.get('completion_probabilities') or []),
        'content_equal': first.get('content') == second.get('content'),
        'erase_response': erased,
        'first_timings': first.get('timings'), 'second_timings': second.get('timings'),
    }
    (OUT / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps({k: v for k, v in summary.items() if 'timings' not in k}, indent=2))
finally:
    if proc.poll() is None:
        proc.send_signal(signal.SIGINT)
        try:
            proc.wait(timeout=90)
        except subprocess.TimeoutExpired:
            proc.kill()
    log.close(); print('server stopped rc =', proc.returncode)
