#!/usr/bin/env python3
"""EXP282: P8-05 inventory probe -- which utility endpoints actually serve this model."""
from pathlib import Path
import json, signal, socket, subprocess, sys, time, urllib.error, urllib.request

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-282-p8-05-endpoint-probe'
OUT = E / 'run'; OUT.mkdir(parents=True, exist_ok=True)
RUN = F / 'runtime/reconstructed/build/stage-run'
REF = F / 'runtime/exp244/results-apex-or244-or-goose'

argv = json.loads((REF / 'argv.json').read_text())
argv[0] = str(RUN / 'llama-server')
env = json.loads((REF / 'environment.json').read_text())
env['LD_LIBRARY_PATH'] = str(RUN); env.pop('LD_PRELOAD', None)
if socket.socket().connect_ex(('127.0.0.1', 8081)) == 0:
    print('port busy'); sys.exit(1)
log = open(OUT / 'server.log', 'wb')
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


def call(method, path, payload=None, timeout=120):
    data = json.dumps(payload).encode() if payload is not None else None
    r = urllib.request.Request('http://127.0.0.1:8081' + path, data=data,
                               headers={'Content-Type': 'application/json'}, method=method)
    try:
        with urllib.request.urlopen(r, timeout=timeout) as resp:
            return resp.status, json.loads(resp.read())
    except urllib.error.HTTPError as e:
        body = e.read().decode('utf-8', 'replace')
        try:
            body = json.loads(body)
        except Exception:
            body = body[:200]
        return e.code, body

results = {}
try:
    if not ready():
        print('not ready'); sys.exit(1)

    st, tok = call('POST', '/tokenize', {'content': 'Hello world'})
    results['tokenize'] = dict(status=st, tokens=(tok or {}).get('tokens'))
    ids = (tok or {}).get('tokens') or []
    st2, det = call('POST', '/detokenize', {'tokens': ids})
    results['detokenize'] = dict(status=st2, content=(det or {}).get('content'))
    results['roundtrip_ok'] = results['detokenize']['content'] == 'Hello world'

    st3, tpl = call('POST', '/apply-template', {'messages': [{'role': 'user', 'content': 'Hi'}]})
    results['apply_template'] = dict(status=st3, prompt=(tpl or {}).get('prompt', '')[:120])

    st4, emb = call('POST', '/embeddings', {'content': 'Hello'})
    results['embeddings'] = dict(status=st4, detail=str(emb)[:160])

    st5, rr = call('POST', '/rerank', {'query': 'q', 'documents': ['a', 'b']})
    results['rerank'] = dict(status=st5, detail=str(rr)[:160])

    st6, inf = call('POST', '/infill', {'input_prefix': 'def f(', 'input_suffix': '):'})
    results['infill'] = dict(status=st6, detail=str(inf)[:160])

    st7, props = call('GET', '/props')
    results['props'] = dict(status=st7, n_ctx=(props or {}).get('default_generation_settings', {}).get('n_ctx'),
                            chat_template_present=bool((props or {}).get('chat_template')))
finally:
    if proc.poll() is None:
        proc.send_signal(signal.SIGINT)
        try:
            proc.wait(timeout=120)
        except subprocess.TimeoutExpired:
            proc.kill()
    log.close()

(OUT / 'probe.json').write_text(json.dumps(results, indent=2) + '\n')
print(json.dumps(results, indent=2))
