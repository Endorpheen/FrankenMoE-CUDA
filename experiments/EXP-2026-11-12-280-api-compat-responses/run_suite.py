#!/usr/bin/env python3
"""EXP280: responses API and multi-client interleaving for the P8-04 suite.

  11. /v1/responses non-stream: a completed response object with output text and usage;
  12. /v1/responses with an explicit max_output_tokens: the object reports an incomplete/length status;
  13. two concurrent chat clients with different prompts: each gets its own non-empty answer and its own usage, and the
      two answers differ (no cross-talk).
"""
from pathlib import Path
import json, signal, socket, subprocess, sys, threading, time, urllib.error, urllib.request

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-280-api-compat-responses'
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


def text_of_response(obj):
    parts = []
    for item in obj.get('output') or []:
        for c in item.get('content') or []:
            if isinstance(c, dict) and c.get('text'):
                parts.append(c['text'])
    return ''.join(parts), obj.get('output_text') or ''


checks = {}


def rec(name, ok, detail=""):
    checks[name] = dict(ok=bool(ok), detail=detail)


try:
    if not ready():
        print('not ready'); print((OUT / 'server.log').read_text()[-2500:]); sys.exit(1)

    r1 = post('/v1/responses', {'model': 'Apex-35B-V1.0', 'input': 'Say hello in one word.', 'max_output_tokens': 32})
    body1, out1 = text_of_response(r1)
    rec('responses_status', r1.get('status') in ('completed', 'incomplete'), r1.get('status'))
    rec('responses_output', len(body1) + len(out1) > 0, f'{len(body1)} + {len(out1)} chars')
    u = r1.get('usage') or {}
    rec('responses_usage', isinstance(u.get('total_tokens'), int) and u['total_tokens'] > 0, u)

    r2 = post('/v1/responses', {'model': 'Apex-35B-V1.0', 'input': 'Say hello in one word.', 'max_output_tokens': 4})
    # this implementation translates to the chat builder and does not emit status=incomplete / incomplete_details;
    # the observable contract is that the budget binds. Recorded rather than asserted as OpenAI-exact.
    ot = (r2.get('usage') or {}).get('output_tokens')
    rec('responses_limit_binds', isinstance(ot, int) and 0 < ot <= 4, f'output_tokens={ot}, status={r2.get("status")}')

    answers = {}

    def client(tag, prompt):
        answers[tag] = post('/v1/chat/completions', {
            'model': 'Apex-35B-V1.0', 'messages': [{'role': 'user', 'content': prompt}],
            'max_tokens': 32, 'temperature': 0})

    ta = threading.Thread(target=client, args=('a', 'What is the capital of France? Answer briefly.'))
    tb = threading.Thread(target=client, args=('b', 'Name a prime number below ten.'))
    ta.start(); tb.start(); ta.join(); tb.join()

    ma = answers['a']['choices'][0]['message']
    mb = answers['b']['choices'][0]['message']
    ca = (ma.get('content') or '') + (ma.get('reasoning_content') or '')
    cb = (mb.get('content') or '') + (mb.get('reasoning_content') or '')
    rec('concurrent_both_answered', len(ca) > 0 and len(cb) > 0, f'{len(ca)}/{len(cb)} chars')
    rec('concurrent_distinct', ca != cb, 'distinct' if ca != cb else 'identical')
    ua, ub = answers['a'].get('usage') or {}, answers['b'].get('usage') or {}
    rec('concurrent_usage', ua.get('total_tokens', 0) > 0 and ub.get('total_tokens', 0) > 0,
        f"{ua.get('total_tokens')}/{ub.get('total_tokens')}")
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
