#!/usr/bin/env python3
"""EXP261 Apex regression: accepted profile with the rebuilt libggml-cpu substituted."""
from pathlib import Path
import hashlib, json, shutil, signal, socket, subprocess, sys, time, urllib.request

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-261-pointwise-contract-fixes'
ACC = F / 'runtime/exp244/measured-apex-or244-or-goose/run'
REF = F / 'runtime/exp244/results-apex-or244-or-goose'
OUT = Path('/tmp/exp261-run'); OUT.mkdir(parents=True, exist_ok=True)
SLOTS = OUT / 'slots'; SLOTS.mkdir(exist_ok=True)
RUN = OUT / 'run'; RUN.mkdir(exist_ok=True)

BASE = F / 'runtime/reconstructed/build/stage-run'
for p in list(BASE.iterdir()) + [ACC / 'llama-server']:
    q = RUN / p.name
    if q.is_symlink() or q.exists():
        q.unlink()
    q.symlink_to(p.resolve())
new = E / 'build/libggml-cpu.so.0.27.0'
for name in ['libggml-cpu.so.0.27.0', 'libggml-cpu.so.0', 'libggml-cpu.so']:
    q = RUN / name
    if q.is_symlink() or q.exists():
        q.unlink()
    q.symlink_to(new.resolve())
print('substituted libggml-cpu', hashlib.sha256(new.read_bytes()).hexdigest()[:16])

argv = json.loads((REF / 'argv.json').read_text())
argv[0] = str(RUN / 'llama-server')
argv += ['--slot-save-path', str(SLOTS)]
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
    r = urllib.request.Request('http://127.0.0.1:8081' + path, data=json.dumps(payload).encode(),
                               headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(r, timeout=timeout) as resp:
        return json.loads(resp.read())

try:
    if not ready():
        print('not ready'); print((OUT / 'server.log').read_text()[-3000:]); sys.exit(1)
    req = json.loads((REF / 'request.json').read_text())
    first = post('/completion', req)
    (OUT / 'response-first.json').write_text(json.dumps(first, indent=2) + '\n')
    post('/slots/0?action=save', {'filename': 'apex-slot0.bin'}, 600)
    post('/slots/0?action=restore', {'filename': 'apex-slot0.bin'}, 600)
    second = post('/completion', req)
    summary = {
        'first_sha': hashlib.sha256((first.get('content') or '').encode()).hexdigest(),
        'second_sha': hashlib.sha256((second.get('content') or '').encode()).hexdigest(),
        'tokens_equal': first.get('tokens') == second.get('tokens'),
        'probs_equal': (first.get('completion_probabilities') or []) == (second.get('completion_probabilities') or []),
        'content_equal': first.get('content') == second.get('content'),
        'restore_ok': True,
        'slot_file_size': (SLOTS / 'apex-slot0.bin').stat().st_size if (SLOTS / 'apex-slot0.bin').exists() else 0,
        'timings': first.get('timings'),
    }
    (OUT / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps(summary, indent=2))
finally:
    if proc.poll() is None:
        proc.send_signal(signal.SIGINT)
        try:
            proc.wait(timeout=60)
        except subprocess.TimeoutExpired:
            proc.kill()
    log.close(); print('server stopped rc =', proc.returncode)
