#!/usr/bin/env python3
"""Run the fixed Apex profile against the stage DSO and save the protocol data.

One run: start the stage server on 8081, wait for readiness, send the accepted 32K/128 request,
store the response next to the reference, stop the server. Comparison is a separate step.
"""
from pathlib import Path
import hashlib, json, os, signal, socket, subprocess, sys, time, urllib.request

E = Path(__file__).resolve().parent
BASE = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
REF = BASE / 'runtime/exp244/results-apex-or244-or-goose'
RUN = E / 'build/stage-run'
OUT = E / 'build/run-apex-stage'
OUT.mkdir(parents=True, exist_ok=True)

argv = json.loads((REF/'argv.json').read_text())
argv[0] = str(RUN/'llama-server')
env = json.loads((REF/'environment.json').read_text())
env['LD_LIBRARY_PATH'] = str(RUN)
env.pop('LD_PRELOAD', None)

with socket.socket() as s:
    if s.connect_ex(('127.0.0.1', 8081)) == 0:
        print('port 8081 occupied; refusing to disturb another workload'); sys.exit(1)

(OUT/'argv.json').write_text(json.dumps(argv, indent=2)+'\n')
(OUT/'environment.json').write_text(json.dumps(env, indent=2)+'\n')
(OUT/'libraries.json').write_text(json.dumps({str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                                               for p in sorted(RUN.glob('*.so*')) + [RUN/'llama-server']}, indent=2)+'\n')

log = open(OUT/'server.log', 'wb')
proc = subprocess.Popen(argv, cwd=str(RUN), env=env, stdout=log, stderr=subprocess.STDOUT)

def wait_ready(timeout=600):
    t0 = time.time()
    while time.time() - t0 < timeout:
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
    if not wait_ready():
        print('server did not become ready'); print((OUT/'server.log').read_text()[-3000:]); sys.exit(1)
    req = json.loads((REF/'request.json').read_text())
    body = json.dumps(req).encode()
    r = urllib.request.Request('http://127.0.0.1:8081/completion', data=body,
                               headers={'Content-Type': 'application/json'})
    t0 = time.time()
    with urllib.request.urlopen(r, timeout=1800) as resp:
        data = json.loads(resp.read())
    wall = time.time() - t0
    (OUT/'response.json').write_text(json.dumps(data, indent=2)+'\n')
    summary = {
        'status': 'RESPONSE_RECEIVED',
        'wall_seconds': wall,
        'token_count': len(data.get('tokens', []) or []),
        'content_sha256': hashlib.sha256((data.get('content') or '').encode()).hexdigest(),
        'timings': data.get('timings'),
        'stage_dso_sha256': hashlib.sha256((E/'build/libllama-exp347.so.0.3.0').read_bytes()).hexdigest(),
        'preliminary': True,
    }
    (OUT/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
    print(json.dumps({k: summary[k] for k in ('status','wall_seconds','token_count','content_sha256')}, indent=2))
finally:
    if proc.poll() is None:
        proc.send_signal(signal.SIGINT)
        try:
            proc.wait(timeout=60)
        except subprocess.TimeoutExpired:
            proc.kill()
    log.close()
    print('server stopped, rc =', proc.returncode)
