#!/usr/bin/env python3
"""Run the unified Apex baseline: stage-run + Goose logging, 32K/128 + 3584 MiB."""
from pathlib import Path
import hashlib, json, signal, socket, subprocess, time, urllib.request

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
RUN = F / 'runtime/exp421/unified'
REF = F / 'runtime/exp244/results-apex-or244-or-goose'
OUT = F / 'runtime/exp421/build/unified-3584'
OUT.mkdir(parents=True, exist_ok=True)
PORT = 18317

argv = json.loads((REF / 'argv.json').read_text())
argv[0] = str(RUN / 'llama-server')
argv[argv.index('--port') + 1] = str(PORT)
argv += ['--moe-cache-mib', '3584']
request = json.loads((REF / 'request.json').read_text())
env = {'PATH': '/usr/bin:/bin', 'HOME': '/tmp', 'LD_LIBRARY_PATH': str(RUN),
       'FRANKENMOE_LOG_MODE': 'franken'}

with socket.socket() as s:
    if s.connect_ex(('127.0.0.1', PORT)) == 0:
        print('port busy'); raise SystemExit(1)

out = open(OUT / 'stdout.log', 'wb')
err = open(OUT / 'stderr.log', 'wb')
proc = subprocess.Popen(argv, cwd=str(RUN), env=env, stdout=out, stderr=err)
try:
    t0 = time.time()
    while time.time() - t0 < 900:
        if proc.poll() is not None:
            raise SystemExit('server exited early')
        try:
            with urllib.request.urlopen(f'http://127.0.0.1:{PORT}/health', timeout=3) as r:
                if json.loads(r.read()).get('status') == 'ok':
                    break
        except Exception:
            pass
        time.sleep(2)
    req = urllib.request.Request(f'http://127.0.0.1:{PORT}/completion',
                                 data=json.dumps(request).encode(),
                                 headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(req, timeout=1800) as resp:
        d = json.loads(resp.read())
    result = {
        'prefill_tok_s': d['timings']['prompt_per_second'],
        'decode_tok_s': d['timings']['predicted_per_second'],
        'predicted_n': d['timings']['predicted_n'],
        'sha': hashlib.sha256((d.get('content') or '').encode()).hexdigest(),
    }
    (OUT / 'metrics.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
finally:
    if proc.poll() is None:
        proc.send_signal(signal.SIGINT)
        try:
            proc.wait(timeout=120)
        except subprocess.TimeoutExpired:
            proc.kill()
    out.close(); err.close()
    print('server rc', proc.returncode)
