#!/usr/bin/env python3
"""Run the fixed profile, then ask the server to save slot 0 so the state-write path is exercised."""
from pathlib import Path
import hashlib, json, os, signal, socket, subprocess, sys, time, urllib.request

E = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed')
BASE = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
REF = BASE/'runtime/exp244/results-apex-or244-or-goose'
RUN = E/'build/stage-run'
OUT = E/'build/run-apex-save'
OUT.mkdir(parents=True, exist_ok=True)
SLOTS = OUT/'slots'; SLOTS.mkdir(exist_ok=True)

argv = json.loads((REF/'argv.json').read_text())
argv[0] = str(RUN/'llama-server')
if '--slot-save-path' not in argv:
    argv += ['--slot-save-path', str(SLOTS)]
env = json.loads((REF/'environment.json').read_text())
env['LD_LIBRARY_PATH'] = str(RUN); env.pop('LD_PRELOAD', None)

with socket.socket() as s:
    if s.connect_ex(('127.0.0.1', 8081)) == 0:
        print('port busy'); sys.exit(1)

log = open(OUT/'server.log', 'wb')
proc = subprocess.Popen(argv, cwd=str(RUN), env=env, stdout=log, stderr=subprocess.STDOUT)

def wait_ready(t=600):
    t0 = time.time()
    while time.time()-t0 < t:
        if proc.poll() is not None: return False
        try:
            with urllib.request.urlopen('http://127.0.0.1:8081/health', timeout=3) as r:
                if json.loads(r.read()).get('status') == 'ok': return True
        except Exception: pass
        time.sleep(2)
    return False

try:
    if not wait_ready():
        print('not ready'); print((OUT/'server.log').read_text()[-2000:]); sys.exit(1)
    maps = open(f'/proc/{proc.pid}/maps').read()
    (OUT/'maps.txt').write_text('\n'.join(sorted({l.split()[-1] for l in maps.splitlines() if 'libllama' in l}))+'\n')

    req = json.loads((REF/'request.json').read_text())
    r = urllib.request.Request('http://127.0.0.1:8081/completion', data=json.dumps(req).encode(),
                               headers={'Content-Type':'application/json'})
    with urllib.request.urlopen(r, timeout=1800) as resp:
        data = json.loads(resp.read())
    (OUT/'response.json').write_text(json.dumps(data, indent=2)+'\n')

    # save the slot: this is the call that reaches llama_kv_cache::state_write
    body = json.dumps({'filename': 'apex-slot0.bin'}).encode()
    rs = urllib.request.Request('http://127.0.0.1:8081/slots/0?action=save', data=body,
                                headers={'Content-Type':'application/json'})
    try:
        with urllib.request.urlopen(rs, timeout=300) as resp:
            saved = json.loads(resp.read())
        (OUT/'save.json').write_text(json.dumps(saved, indent=2)+'\n')
        ok = True
    except Exception as e:
        (OUT/'save.json').write_text(json.dumps({'error': str(e)})+'\n')
        ok = False

    summary = {
        'status': 'RESPONSE_RECEIVED',
        'tokens': len(data.get('tokens') or []),
        'content_sha256': hashlib.sha256((data.get('content') or '').encode()).hexdigest(),
        'timings': data.get('timings'),
        'save_ok': ok,
        'slot_file': str(SLOTS/'apex-slot0.bin'),
        'slot_file_size': (SLOTS/'apex-slot0.bin').stat().st_size if (SLOTS/'apex-slot0.bin').exists() else 0,
    }
    (OUT/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
    print(json.dumps({k: summary[k] for k in ('status','tokens','content_sha256','save_ok','slot_file_size')}, indent=2))
finally:
    if proc.poll() is None:
        proc.send_signal(signal.SIGINT)
        try: proc.wait(timeout=60)
        except subprocess.TimeoutExpired: proc.kill()
    log.close()
    print('server stopped rc =', proc.returncode)
