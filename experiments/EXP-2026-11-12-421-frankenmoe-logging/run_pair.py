#!/usr/bin/env python3
"""Paired A/B/B/A for logging modes; the ordering controls warm-up drift."""
from pathlib import Path
import hashlib, json, os, signal, socket, subprocess, sys, time, urllib.request

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
BIN = F / 'runtime/exp421/stage/bin'
OUT = F / 'runtime/exp421/build/pair'
OUT.mkdir(parents=True, exist_ok=True)
MODEL = '/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf'
PORT = 18313


def post(path, payload, timeout=900):
    r = urllib.request.Request(f'http://127.0.0.1:{PORT}{path}',
                               data=json.dumps(payload).encode(),
                               headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(r, timeout=timeout) as resp:
        return json.loads(resp.read())


def run(mode, tag):
    argv = [str(BIN / 'llama-server'), '-m', MODEL, '--alias', 'Apex-35B-V1.0',
            '-c', '512', '-ngl', 'all', '-fit', 'off', '--cpu-moe', '-fa', 'on',
            '-ctk', 'q4_0', '-ctv', 'q4_0', '-b', '4096', '-ub', '4096',
            '-t', '8', '-tb', '16', '-np', '1', '--cache-ram', '0',
            '--spec-type', 'none', '--jinja', '--temp', '1', '--top-p', '0.95',
            '--top-k', '20', '--min-p', '0', '--host', '127.0.0.1',
            '--port', str(PORT)]
    env = {'PATH': '/usr/bin:/bin', 'HOME': '/tmp', 'LD_LIBRARY_PATH': str(BIN),
           'FRANKENMOE_LOG_MODE': mode}
    log = open(OUT / f'{tag}.out', 'wb')
    err = open(OUT / f'{tag}.err', 'wb')
    proc = subprocess.Popen(argv, cwd=str(BIN), env=env, stdout=log, stderr=err)
    try:
        t0 = time.time()
        while time.time() - t0 < 900:
            if proc.poll() is not None:
                return None
            try:
                with urllib.request.urlopen(f'http://127.0.0.1:{PORT}/health', timeout=3) as r:
                    if json.loads(r.read()).get('status') == 'ok':
                        break
            except Exception:
                pass
            time.sleep(2)
        resp = post('/completion', {'prompt': 'Reply with exactly one word: hello.',
                                    'n_predict': 128, 'temperature': 1.0, 'seed': 123,
                                    'ignore_eos': True, 'cache_prompt': False,
                                    'return_tokens': True})
        return {'mode': mode, 'tag': tag,
                'decode': resp['timings']['predicted_per_second'],
                'prefill': resp['timings']['prompt_per_second'],
                'sha': hashlib.sha256((resp.get('content') or '').encode()).hexdigest()}
    finally:
        if proc.poll() is None:
            proc.send_signal(signal.SIGINT)
            try:
                proc.wait(timeout=120)
            except subprocess.TimeoutExpired:
                proc.kill()
        log.close(); err.close()


with socket.socket() as s:
    if s.connect_ex(('127.0.0.1', PORT)) == 0:
        print('port busy'); sys.exit(1)

order = sys.argv[1:] or ['classic', 'franken', 'franken', 'classic']
rows = []
for i, m in enumerate(order):
    r = run(m, f'{i}-{m}')
    print(json.dumps(r))
    rows.append(r)

vals = {m: [r['decode'] for r in rows if r['mode'] == m] for m in set(order)}
print(json.dumps({'decode': vals}, indent=2))
for m in vals:
    if len(vals[m]) > 1:
        print(m, 'decode mean', round(sum(vals[m]) / len(vals[m]), 3))
