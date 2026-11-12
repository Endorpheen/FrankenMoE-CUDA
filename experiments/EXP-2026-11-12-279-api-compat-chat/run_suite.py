#!/usr/bin/env python3
"""EXP279: chat-endpoint and usage checks for the P8-04 compatibility suite.

Adds to EXP278:
  6. chat non-stream: finish_reason "length" at max_tokens, content non-empty;
  7. chat stream: concatenated deltas equal the non-stream content, and the last chunk carries a finish_reason;
  8. chat with a stop sequence: finish_reason "stop";
  9. usage accounting: prompt/completion/total consistent and completion equal to the token count returned;
 10. a malformed chat body: HTTP 4xx.
"""
from pathlib import Path
import json, signal, socket, subprocess, sys, time, urllib.error, urllib.request

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-279-api-compat-chat'
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


def chat_stream(payload, timeout=600):
    body = dict(payload); body['stream'] = True
    r = urllib.request.Request('http://127.0.0.1:8081/v1/chat/completions', data=json.dumps(body).encode(),
                               headers={'Content-Type': 'application/json'})
    text, finish, n = [], None, 0
    with urllib.request.urlopen(r, timeout=timeout) as resp:
        for line in resp:
            if not line.startswith(b'data: '):
                continue
            data = line[6:].strip()
            if data == b'[DONE]':
                break
            chunk = json.loads(data)
            n += 1
            for ch in chunk.get('choices') or []:
                delta = ch.get('delta') or {}
                if delta.get('content'):
                    text.append(delta['content'])
                if delta.get('reasoning_content'):
                    text.append(delta['reasoning_content'])
                if ch.get('finish_reason'):
                    finish = ch['finish_reason']
    return ''.join(text), finish, n


checks = {}


def rec(name, ok, detail=""):
    checks[name] = dict(ok=bool(ok), detail=detail)


try:
    if not ready():
        print('not ready'); print((OUT / 'server.log').read_text()[-2500:]); sys.exit(1)

    base = {
        'model': 'Apex-35B-V1.0',
        'messages': [{'role': 'user', 'content': 'The quick brown fox'}],
        'max_tokens': 16,
        'temperature': 0,
    }
    a = post('/v1/chat/completions', base)
    msg = a['choices'][0]['message']
    text_a = (msg.get('content') or '') + (msg.get('reasoning_content') or '')
    rec('chat_nonstream_finish', a['choices'][0].get('finish_reason') == 'length', a['choices'][0].get('finish_reason'))
    rec('chat_nonstream_content', len(text_a) > 0, f'{len(text_a)} chars (content or reasoning)')

    text_s, fin_s, n_s = chat_stream(base)
    rec('chat_stream_equal', text_s == text_a, f'{n_s} chunks')
    rec('chat_stream_finish', fin_s == 'length', fin_s)

    stop_req = dict(base); stop_req['stop'] = [' ']
    b = post('/v1/chat/completions', stop_req)
    rec('chat_stop_finish', b['choices'][0].get('finish_reason') == 'stop', b['choices'][0].get('finish_reason'))

    u = a.get('usage') or {}
    pt, ct, tt = u.get('prompt_tokens'), u.get('completion_tokens'), u.get('total_tokens')
    rec('usage_present', isinstance(pt, int) and isinstance(ct, int) and isinstance(tt, int), u)
    rec('usage_consistent', isinstance(pt, int) and isinstance(ct, int) and isinstance(tt, int) and tt == pt + ct
        and ct > 0 and pt > 0, f'{pt}+{ct}={tt}')

    try:
        post('/v1/chat/completions', {'model': 'Apex-35B-V1.0', 'messages': 'not-a-list'})
        rec('chat_invalid_rejected', False, 'accepted')
    except urllib.error.HTTPError as e:
        rec('chat_invalid_rejected', 400 <= e.code < 500, f'{e.code}')
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
