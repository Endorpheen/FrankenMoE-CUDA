#!/usr/bin/env python3
"""Save the slot, restore it, and prove the state came back by repeating the same completion."""
from pathlib import Path
import hashlib, json, os, signal, socket, subprocess, sys, time, urllib.request

E = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed')
BASE = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
REF = BASE/'runtime/exp244/results-apex-or244-or-goose'
RUN = E/'build/stage-run'
OUT = E/'build/run-cancel'; OUT.mkdir(parents=True, exist_ok=True)
SLOTS = OUT/'slots'; SLOTS.mkdir(exist_ok=True)

argv = json.loads((REF/'argv.json').read_text())
argv[0] = str(RUN/'llama-server')
argv += ['--slot-save-path', str(SLOTS)]
env = json.loads((REF/'environment.json').read_text())
env['LD_LIBRARY_PATH'] = str(RUN); env.pop('LD_PRELOAD', None)
with socket.socket() as s:
    if s.connect_ex(('127.0.0.1', 8081)) == 0:
        print('port busy'); sys.exit(1)

log = open(OUT/'server.log','wb')
proc = subprocess.Popen(argv, cwd=str(RUN), env=env, stdout=log, stderr=subprocess.STDOUT)

def ready(t=600):
    t0=time.time()
    while time.time()-t0<t:
        if proc.poll() is not None: return False
        try:
            with urllib.request.urlopen('http://127.0.0.1:8081/health', timeout=3) as r:
                if json.loads(r.read()).get('status')=='ok': return True
        except Exception: pass
        time.sleep(2)
    return False

def post(path, payload, timeout=1800):
    r = urllib.request.Request('http://127.0.0.1:8081'+path, data=json.dumps(payload).encode(),
                               headers={'Content-Type':'application/json'})
    with urllib.request.urlopen(r, timeout=timeout) as resp:
        return json.loads(resp.read())

try:
    if not ready():
        print('not ready'); print((OUT/'server.log').read_text()[-2000:]); sys.exit(1)
    req = json.loads((REF/'request.json').read_text())

    import http.client
    stream_req = dict(req); stream_req['stream'] = True; stream_req['n_predict'] = 128
    conn = http.client.HTTPConnection('127.0.0.1', 8081, timeout=1800)
    conn.request('POST', '/completion', body=json.dumps(stream_req),
                 headers={'Content-Type': 'application/json', 'Connection': 'close'})
    resp = conn.getresponse()
    seen = 0
    while seen < 8:
        line = resp.readline()
        if not line: break
        if b'"content"' in line or b'data:' in line: seen += 1
    try:
        if conn.sock is not None: conn.sock.close()
    except Exception:
        pass
    try: resp.close()
    except Exception: pass
    conn.close()
    print(f'cancel: dropped the socket after {seen} stream reads', flush=True)
    time.sleep(3)

    out = post('/completion', req, 1800)
    content = out.get('content', '')
    sha = hashlib.sha256(content.encode()).hexdigest()
    ref = json.loads((REF/'summary.json').read_text())
    ok = sha == ref['content_sha256']
    print(json.dumps({'after_cancel_sha': sha, 'reference_sha': ref['content_sha256'],
                      'content_equal': ok, 'tokens': out.get('tokens_evaluated')}, indent=2), flush=True)
    (OUT/'result.json').write_text(json.dumps({'sha': sha, 'equal': ok}, indent=2)+'\n')
    sys.exit(0 if ok else 1)
finally:
    proc.send_signal(signal.SIGINT)
    try: proc.wait(60)
    except subprocess.TimeoutExpired: proc.kill()
    log.close()
    print('server stopped rc =', proc.returncode, flush=True)
