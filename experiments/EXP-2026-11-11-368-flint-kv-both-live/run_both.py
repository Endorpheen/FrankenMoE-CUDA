#!/usr/bin/env python3
"""Completion, save (write witness), truncated restore (clear witness), second completion."""
from pathlib import Path
import hashlib, json, os, signal, socket, subprocess, sys, time, urllib.request

E = Path(__file__).resolve().parent
BASE = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
REF = BASE/'runtime/exp244/results-apex-or244-or-goose'
RUN = E/'build/stage-run'; OUT = E/'build/run-both'; OUT.mkdir(parents=True, exist_ok=True)
SLOTS = OUT/'slots'; SLOTS.mkdir(exist_ok=True)

argv = json.loads((REF/'argv.json').read_text()); argv[0]=str(RUN/'llama-server')
argv += ['--slot-save-path', str(SLOTS)]
env = json.loads((REF/'environment.json').read_text()); env['LD_LIBRARY_PATH']=str(RUN); env.pop('LD_PRELOAD',None)
with socket.socket() as s:
    if s.connect_ex(('127.0.0.1',8081))==0: print('port busy'); sys.exit(1)
log=open(OUT/'server.log','wb')
proc=subprocess.Popen(argv,cwd=str(RUN),env=env,stdout=log,stderr=subprocess.STDOUT)

def ready(t=600):
    t0=time.time()
    while time.time()-t0<t:
        if proc.poll() is not None: return False
        try:
            with urllib.request.urlopen('http://127.0.0.1:8081/health',timeout=3) as r:
                if json.loads(r.read()).get('status')=='ok': return True
        except Exception: pass
        time.sleep(2)
    return False

def post(path,payload,timeout=1800):
    r=urllib.request.Request('http://127.0.0.1:8081'+path,data=json.dumps(payload).encode(),
                             headers={'Content-Type':'application/json'})
    with urllib.request.urlopen(r,timeout=timeout) as resp: return json.loads(resp.read())

try:
    if not ready(): print('not ready'); print((OUT/'server.log').read_text()[-1500:]); sys.exit(1)
    req=json.loads((REF/'request.json').read_text())
    first=post('/completion',req); (OUT/'response.json').write_text(json.dumps(first,indent=2)+'\n')
    post('/slots/0?action=save',{'filename':'good.bin'},600)
    good=SLOTS/'good.bin'; bad=SLOTS/'bad.bin'
    with open(good,'rb') as f, open(bad,'wb') as g: g.write(f.read(1024*1024))
    outcome=None
    try:
        post('/slots/0?action=restore',{'filename':'bad.bin'},600); outcome='restore-succeeded(unexpected)'
    except Exception as e:
        outcome='restore-failed: %s' % type(e).__name__
    summary={'completion_sha':hashlib.sha256((first.get('content') or '').encode()).hexdigest(),
             'slot_file_size':good.stat().st_size,'restore_outcome':outcome}
    (OUT/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2))
finally:
    if proc.poll() is None:
        proc.send_signal(signal.SIGINT)
        try: proc.wait(timeout=60)
        except subprocess.TimeoutExpired: proc.kill()
    log.close(); print('server stopped rc =',proc.returncode)
