#!/usr/bin/env python3
"""EXP281: streaming /v1/responses -- the last item of P8-04's criterion.

Checks: the stream emits the standard event names, terminates with response.completed carrying usage, and the accumulated
output_text / reasoning_text deltas equal the non-stream answer for the same deterministic request.
"""
from pathlib import Path
import json, signal, socket, subprocess, sys, time, urllib.request

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-281-api-compat-responses-stream'
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


def post(path, payload, timeout=600):
    r = urllib.request.Request('http://127.0.0.1:8081' + path, data=json.dumps(payload).encode(),
                               headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(r, timeout=timeout) as resp:
        return json.loads(resp.read())


def aggregate(obj):
    text, reasoning, types = [], [], []
    for item in obj.get('output') or []:
        types.append(item.get('type'))
        if item.get('type') == 'message':
            for c in item.get('content') or []:
                if isinstance(c, dict) and c.get('text'):
                    text.append(c['text'])
        for s in item.get('summary') or []:
            if isinstance(s, dict) and s.get('text'):
                reasoning.append(s['text'])
        if item.get('type') == 'reasoning':
            for c in item.get('content') or []:
                if isinstance(c, dict) and c.get('text'):
                    reasoning.append(c['text'])
    return ''.join(text), ''.join(reasoning), types


def stream_events(payload, timeout=600):
    body = dict(payload); body['stream'] = True
    r = urllib.request.Request('http://127.0.0.1:8081/v1/responses', data=json.dumps(body).encode(),
                               headers={'Content-Type': 'application/json'})
    text, reasoning, types, completed = [], [], [], None
    with urllib.request.urlopen(r, timeout=timeout) as resp:
        for line in resp:
            s = line.decode('utf-8', 'replace').strip()
            if not s.startswith('data: '):
                continue
            ev = json.loads(s[6:])
            types.append(ev.get('type'))
            if ev.get('type') == 'response.output_text.delta':
                text.append(ev.get('delta') or '')
            elif ev.get('type') == 'response.reasoning_text.delta':
                reasoning.append(ev.get('delta') or '')
            elif ev.get('type') == 'response.completed':
                completed = ev.get('response') or {}
    return ''.join(text), ''.join(reasoning), types, completed

checks = {}


def rec(name, ok, detail=""):
    checks[name] = dict(ok=bool(ok), detail=detail)


try:
    if not ready():
        print('not ready'); print((OUT / 'server.log').read_text()[-2500:]); sys.exit(1)

    req = {'model': 'Apex-35B-V1.0', 'input': 'Say hi in one short sentence.', 'max_output_tokens': 48, 'temperature': 0}
    ns = post('/v1/responses', req)
    t_ns, r_ns, types_ns = aggregate(ns)
    rec('nonstream_output', len(t_ns) + len(r_ns) > 0, f'text {len(t_ns)}, reasoning {len(r_ns)}, items {types_ns}')

    t_s, r_s, types_s, completed = stream_events(req)
    rec('stream_events_present', 'response.created' in types_s and 'response.completed' in types_s,
        f'{len(types_s)} events, first {types_s[:3]}')
    rec('stream_terminated', completed is not None and completed.get('status') == 'completed',
        (completed or {}).get('status'))
    u = (completed or {}).get('usage') or {}
    rec('stream_usage', isinstance(u.get('total_tokens'), int) and u['total_tokens'] > 0, u)
    rec('stream_text_equal', t_s == t_ns, f'{len(t_s)} vs {len(t_ns)} chars')
    rec('stream_reasoning_equal', r_s == r_ns, f'{len(r_s)} vs {len(r_ns)} chars')
finally:
    if proc.poll() is None:
        proc.send_signal(signal.SIGINT)
        try:
            proc.wait(timeout=120)
        except subprocess.TimeoutExpired:
            proc.kill()
    log.close()

ok = all(c['ok'] for c in checks.values())
(OUT / 'suite.json').write_text(json.dumps(dict(ok=ok, checks=checks), indent=2) + '\n')
print(json.dumps(checks, indent=2))
print('SUITE', 'PASS' if ok else 'FAIL')
sys.exit(0 if ok else 1)
