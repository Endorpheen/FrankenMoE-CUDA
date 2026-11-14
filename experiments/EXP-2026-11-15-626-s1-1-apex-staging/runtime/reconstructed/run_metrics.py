#!/usr/bin/env python3
"""One preliminary Apex run: prefill/decode timings, peak RSS of the server tree, VRAM attributed by PID."""
from pathlib import Path
import json, os, signal, socket, subprocess, sys, threading, time, urllib.request, hashlib

E = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed')
BASE = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
REF = BASE/'runtime/exp244/results-apex-or244-or-goose'
RUN = E/'build/stage-run'
OUT = E/'build/run-metrics'; OUT.mkdir(parents=True, exist_ok=True)

argv = json.loads((REF/'argv.json').read_text())
argv[0] = str(RUN/'llama-server')
env = json.loads((REF/'environment.json').read_text())
env['LD_LIBRARY_PATH'] = str(RUN); env.pop('LD_PRELOAD', None)

with socket.socket() as s:
    if s.connect_ex(('127.0.0.1', 8081)) == 0:
        print('port busy'); sys.exit(1)

log = open(OUT/'server.log','wb')
proc = subprocess.Popen(argv, cwd=str(RUN), env=env, stdout=log, stderr=subprocess.STDOUT)

peak_rss_kb = 0
peak_vram_mib = 0
gpu_total_mib = 0
stop = threading.Event()

def children(pid):
    out=[]
    try:
        for t in os.listdir(f'/proc/{pid}/task'):
            for c in Path(f'/proc/{pid}/task/{t}/children').read_text().split():
                out.append(int(c)); out += children(int(c))
    except OSError: pass
    return out

def sample():
    global peak_rss_kb, peak_vram_mib, gpu_total_mib
    while not stop.is_set():
        total = 0
        try:
            for pid in [proc.pid] + children(proc.pid):
                try:
                    for line in Path(f'/proc/{pid}/status').read_text().splitlines():
                        if line.startswith('VmRSS:'):
                            total += int(line.split()[1])
                except OSError: pass
        except OSError: pass
        peak_rss_kb = max(peak_rss_kb, total)
        try:
            q = subprocess.run(['nvidia-smi','--query-compute-apps=pid,used_memory','--format=csv,noheader,nounits'],
                               capture_output=True, text=True, timeout=5).stdout
            mine = 0
            for line in q.strip().splitlines():
                parts = [p.strip() for p in line.split(',')]
                if len(parts) != 2: continue
                try: pid, mib = int(parts[0]), int(parts[1])
                except ValueError: continue
                if pid == proc.pid or pid in children(proc.pid): mine += mib
            peak_vram_mib = max(peak_vram_mib, mine)
            gq = subprocess.run(['nvidia-smi','--query-gpu=memory.used','--format=csv,noheader,nounits'],
                                capture_output=True, text=True, timeout=5).stdout
            gpu_total_mib = max(gpu_total_mib, int(gq.strip().splitlines()[0]))
        except Exception:
            pass
        time.sleep(1)

threading.Thread(target=sample, daemon=True).start()

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
        print('not ready'); print((OUT/'server.log').read_text()[-1500:]); sys.exit(1)
    req = json.loads((REF/'request.json').read_text())
    t0 = time.time()
    out = post('/completion', req, 1800)
    wall = time.time()-t0
    content = out.get('content','')
    sha = hashlib.sha256(content.encode()).hexdigest()
    ref = json.loads((REF/'summary.json').read_text())
    tim = out.get('timings', {})
    res = {
        'model': argv[argv.index('-m')+1],
        'n_predict': req.get('n_predict'),
        'prompt_n': tim.get('prompt_n'),
        'predicted_n': tim.get('predicted_n'),
        'prefill_tok_s': tim.get('prompt_per_second'),
        'decode_tok_s': tim.get('predicted_per_second'),
        'wall_s': wall,
        'peak_rss_mib': round(peak_rss_kb/1024, 1),
        'peak_vram_mib_attributed': peak_vram_mib,
        'gpu_total_used_mib_max': gpu_total_mib,
        'completion_sha': sha,
        'reference_sha': ref['content_sha256'],
        'content_equal': sha == ref['content_sha256'],
        'tokens': out.get('tokens_evaluated'),
    }
    (OUT/'metrics.json').write_text(json.dumps(res, indent=2)+'\n')
    print(json.dumps(res, indent=2), flush=True)
finally:
    stop.set()
    proc.send_signal(signal.SIGINT)
    try: proc.wait(60)
    except subprocess.TimeoutExpired: proc.kill()
    log.close()
    print('server stopped rc =', proc.returncode, flush=True)
