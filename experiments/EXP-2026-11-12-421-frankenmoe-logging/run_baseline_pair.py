#!/usr/bin/env python3
"""Baseline Apex pair with the historical GPU expert cache: 32K/128 + 3584 MiB."""
from pathlib import Path
import hashlib, json, signal, socket, subprocess, sys, time, urllib.request

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
BIN = F / 'runtime/exp421/stage/bin'
REF = F / 'runtime/exp244/results-apex-or244-or-goose'
OUT = F / 'runtime/exp421/build/baseline-3584'
OUT.mkdir(parents=True, exist_ok=True)
PORT = 18314

argv = json.loads((REF / 'argv.json').read_text())
argv[0] = str(BIN / 'llama-server')
argv[argv.index('--port') + 1] = str(PORT)
argv += ['--moe-cache-mib', '3584']
request = json.loads((REF / 'request.json').read_text())


def post(path, payload, timeout=1800):
    r = urllib.request.Request(f'http://127.0.0.1:{PORT}{path}',
                               data=json.dumps(payload).encode(),
                               headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(r, timeout=timeout) as resp:
        return json.loads(resp.read())


def run(mode, tag):
    env = {'PATH': '/usr/bin:/bin', 'HOME': '/tmp', 'LD_LIBRARY_PATH': str(BIN),
           'FRANKENMOE_LOG_MODE': mode,
           'GGML_GOOSE_ALLOC': '1', 'GGML_GOOSE_COALESCE': '1',
           'GGML_GOOSE_LIFETIME': '1', 'GGML_GOOSE_GRAPH_PLAN': '1',
           'GGML_GOOSE_MOE': '1', 'GGML_GOOSE_TILED': '1',
           'GGML_GOOSE_TILED_GENERAL': '1', 'GGML_GOOSE_ELEMENTWISE': '1',
           'GGML_GOOSE_MOE_QUANT': '1', 'GGML_GOOSE_MOE_CONTROL': '1',
           'GGML_GOOSE_RMS': '1'}
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
        resp = post('/completion', request)
        return {'mode': mode, 'tag': tag,
                'decode': resp['timings']['predicted_per_second'],
                'prefill': resp['timings']['prompt_per_second'],
                'predicted_n': resp['timings']['predicted_n'],
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

for i, m in enumerate(['classic', 'franken', 'franken', 'classic']):
    r = run(m, f'{i}-{m}')
    print(json.dumps(r))
