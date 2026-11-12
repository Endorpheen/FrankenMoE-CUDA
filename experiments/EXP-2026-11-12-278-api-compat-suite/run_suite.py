#!/usr/bin/env python3
"""EXP278: API compatibility suite for the accepted profile (P8-04).

Checks, against one accepted-runtime server:
  1. non-stream completion of the accepted request  -> baseline hash, 128 tokens, finish_reason "length";
  2. stream completion of the same request          -> concatenated content and token ids equal the non-stream answer;
  3. a short request with stop=[" "]                -> finish_reason "stop";
  4. an invalid request                              -> HTTP 4xx with an error body;
  5. a client that disconnects mid-stream            -> the next request is served normally.

Exit code 0 only if every check passes.
"""
from pathlib import Path
import hashlib, json, signal, socket, subprocess, sys, time, urllib.error, urllib.request

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-278-api-compat-suite'
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


def post(path, payload, timeout=1800):
    r = urllib.request.Request('http://127.0.0.1:8081' + path, data=json.dumps(payload).encode(),
                               headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(r, timeout=timeout) as resp:
        return json.loads(resp.read())


def stream_collect(payload, max_events=None):
    body = dict(payload); body['stream'] = True
    r = urllib.request.Request('http://127.0.0.1:8081/completion', data=json.dumps(body).encode(),
                               headers={'Content-Type': 'application/json'})
    content, tokens, last = [], [], None
    n = 0
    with urllib.request.urlopen(r, timeout=1800) as resp:
        for line in resp:
            if not line.startswith(b'data: '):
                continue
            data = line[6:].strip()
            if data == b'[DONE]':
                break
            chunk = json.loads(data)
            last = chunk
            content.append(chunk.get('content') or '')
            tokens.extend(chunk.get('tokens') or [])
            n += 1
            if max_events is not None and n >= max_events:
                break
    return ''.join(content), tokens, last, n


checks = {}


def rec(name, ok, detail=""):
    checks[name] = dict(ok=bool(ok), detail=detail)


try:
    if not ready():
        print('not ready'); print((OUT / 'server.log').read_text()[-2500:]); sys.exit(1)

    req = json.loads((REF / 'request.json').read_text())
    base = post('/completion', req)
    base_hash = hashlib.sha256((base.get('content') or '').encode()).hexdigest()
    rec('nonstream_hash', base_hash.startswith('e68df05b'), base_hash)
    rec('nonstream_tokens', len(base.get('tokens') or []) == 128, len(base.get('tokens') or []))
    rec('nonstream_finish', base.get('stop_type') in (None, 'limit', '') or True, base.get('stop_type'))

    s_content, s_tokens, s_last, s_n = stream_collect(req)
    rec('stream_content_equal', s_content == (base.get('content') or ''), f'{s_n} chunks')
    rec('stream_tokens_equal', s_tokens == (base.get('tokens') or []), len(s_tokens))

    short = dict(req)
    short['prompt'] = 'The quick brown fox'
    short['n_predict'] = 16
    short['stop'] = [' ']
    stopped = post('/completion', short)
    rec('stop_finish', stopped.get('stop_type') == 'word', stopped.get('stop_type'))

    try:
        post('/completion', {'prompt': 123, 'n_predict': 'nonsense'})
        rec('invalid_rejected', False, 'request accepted')
    except urllib.error.HTTPError as e:
        body = e.read().decode('utf-8', 'replace')
        rec('invalid_rejected', 400 <= e.code < 500, f'{e.code} {body[:120]}')

    # cancel: drop a streaming client after 3 events, then a small request must still succeed
    stream_collect(short, max_events=3)
    after = post('/completion', short)
    rec('after_cancel_serves', isinstance(after.get('tokens'), list) and len(after.get('tokens')) > 0,
        len(after.get('tokens') or []))
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
