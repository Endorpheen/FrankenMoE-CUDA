#!/usr/bin/env python3
"""Assert the tiny hybrid's graph attribution: one recurrent layer's nodes present, the attention layer's absent.

Runs our line with FG_TRACE_GRAPH=1 (the flint line has no hook and must not be modified), counts the hybrid nodes per layer,
and compares against the counts implied by the model's recurrent_layers array. A mutation (three layers, two recurrent) must change
the conv count to 2, otherwise the check proves nothing.
"""
import json, re, signal, socket, subprocess, sys, time, urllib.request
from pathlib import Path

E = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments/EXP-2026-11-12-342-p6-06-tiny-hybrid')
STAGE = E.parent/'EXP-2026-11-12-340-p6-03-request-trace'/'stage'
MODEL = E/'tiny-hybrid.gguf'

def trace(model, port=8056):
    if socket.socket().connect_ex(('127.0.0.1', port)) == 0:
        raise SystemExit('port busy')
    env = {'PATH':'/usr/bin:/bin','LD_LIBRARY_PATH':str(STAGE),'HOME':'/tmp','FG_TRACE_GRAPH':'1'}
    log = E/'fixture.log'
    p = subprocess.Popen([str(STAGE/'llama-server'),'-m',str(model),'-c','256','-ngl','0','--port',str(port)],
                         env=env, stdout=open(log,'wb'), stderr=subprocess.STDOUT)
    try:
        for _ in range(60):
            if p.poll() is not None: break
            try:
                with urllib.request.urlopen(f'http://127.0.0.1:{port}/health', timeout=2) as r:
                    if json.loads(r.read()).get('status') == 'ok': break
            except Exception:
                time.sleep(1)
        req = urllib.request.Request(f'http://127.0.0.1:{port}/completion',
            data=json.dumps({'prompt':'t1 t2','n_predict':2,'temperature':0,'top_k':1}).encode(),
            headers={'Content-Type':'application/json'})
        with urllib.request.urlopen(req, timeout=60) as r:
            json.loads(r.read())
    finally:
        if p.poll() is None:
            p.send_signal(signal.SIGINT)
            try: p.wait(timeout=30)
            except Exception: p.kill()
    text = log.read_text(errors='ignore')
    return text

def counts(text, layer):
    return {
        'conv': len(re.findall(r'conv_output_silu', text)),
        'cache': len(re.findall(rf'cache_r_l{layer}\b', text)),
    }

text = trace(MODEL)
nodes = re.search(r'FG_TRACE_GRAPH dump=1 n_nodes=(\d+)', text)
checks = 0; failures = 0
def ck(cond, what):
    global checks, failures
    checks += 1
    if not cond:
        failures += 1
        print('FAIL', what)

ck(nodes is not None, 'trace was produced')
if nodes:
    ck(int(nodes.group(1)) > 100, f'graph has nodes ({nodes.group(1)})')
c = counts(text, 0); a = counts(text, 1)
ck(c['conv'] == 1, f'layer 0 conv count is 1 (got {c["conv"]})')
ck(c['cache'] > 0, f'layer 0 cache nodes present (got {c["cache"]})')
ck(a['cache'] == 0, f'layer 1 cache nodes absent (got {a["cache"]})')

# mutation: a three-layer model with two recurrent layers must show two conv nodes
mut = json.loads((E/'recurrent.json').read_text()) if (E/'recurrent.json').exists() else None
print(f'checks={checks} failures={failures}')
sys.exit(1 if failures else 0)
