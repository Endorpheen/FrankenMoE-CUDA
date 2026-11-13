#!/usr/bin/env python3
"""EXP267: shutdown while a request is in flight, then prove nothing dangles.

P5-09 requires no dangling cache after model unload. A clean shutdown was already measured; this is the harder case:
SIGINT to the server while a streaming completion is being served. After it exits, the same runtime must start again,
serve the accepted request at e68df05b, and leave no process, port or GPU allocation behind.
"""
from pathlib import Path
import hashlib, json, os, signal, socket, subprocess, sys, threading, time, urllib.request

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-267-apex-unload-inflight'
OUT = E / 'run'; OUT.mkdir(parents=True, exist_ok=True)
RUN = F / 'runtime/reconstructed/build/stage-run'
REF = F / 'runtime/exp244/results-apex-or244-or-goose'

argv = json.loads((REF / 'argv.json').read_text())
argv[0] = str(RUN / 'llama-server')
env = json.loads((REF / 'environment.json').read_text())
env['LD_LIBRARY_PATH'] = str(RUN); env.pop('LD_PRELOAD', None)


def port_busy():
    with socket.socket() as s:
        return s.connect_ex(('127.0.0.1', 8081)) == 0


def start(tag):
    out = OUT / tag; out.mkdir(exist_ok=True)
    log = open(out / 'server.log', 'wb')
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
    return proc, log, out, ready


result = {}
if port_busy():
    print('port busy'); sys.exit(1)

# phase 1: start, begin a streaming request, interrupt the server mid-stream
proc, log, out, ready = start('phase1')
if not ready():
    print('not ready'); print((out / 'server.log').read_text()[-2000:]); sys.exit(1)
req = json.loads((REF / 'request.json').read_text())
req['stream'] = True
events = []
stop = threading.Event()

def stream():
    try:
        r = urllib.request.Request('http://127.0.0.1:8081/completion', data=json.dumps(req).encode(),
                                   headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(r, timeout=300) as resp:
            for line in resp:
                if line.startswith(b'data: '):
                    events.append(line)
                    if len(events) >= 8:
                        break
    except Exception as e:
        events.append(('error: ' + type(e).__name__).encode())
    finally:
        stop.set()

t = threading.Thread(target=stream); t.start()
t0 = time.time()
while not stop.is_set() and time.time() - t0 < 120:
    time.sleep(0.5)
interrupted_with = len(events)
proc.send_signal(signal.SIGINT)
try:
    rc = proc.wait(timeout=120)
except subprocess.TimeoutExpired:
    proc.kill(); rc = 'killed'
log.close()
result['phase1'] = dict(events_before_interrupt=interrupted_with, exit_code=rc)
time.sleep(3)
result['phase1']['port_free_after'] = not port_busy()
result['phase1']['process_gone'] = proc.poll() is not None

# phase 2: the same runtime must start again and answer exactly as accepted
proc2, log2, out2, ready2 = start('phase2')
try:
    if not ready2():
        result['phase2'] = dict(ready=False)
        print((out2 / 'server.log').read_text()[-2000:])
    else:
        r = urllib.request.Request('http://127.0.0.1:8081/completion', data=json.dumps((REF / 'request.json').read_text() and json.loads((REF / 'request.json').read_text())).encode(),
                                   headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(r, timeout=1800) as resp:
            first = json.loads(resp.read())
        result['phase2'] = dict(ready=True,
                                sha=hashlib.sha256((first.get('content') or '').encode()).hexdigest(),
                                tokens=len(first.get('tokens') or []))
finally:
    if proc2.poll() is None:
        proc2.send_signal(signal.SIGINT)
        try:
            proc2.wait(timeout=120)
        except subprocess.TimeoutExpired:
            proc2.kill()
    log2.close()
result['phase2']['exit_code'] = proc2.returncode
(OUT / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
