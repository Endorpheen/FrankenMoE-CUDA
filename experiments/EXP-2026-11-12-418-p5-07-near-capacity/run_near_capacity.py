#!/usr/bin/env python3
"""P5-07 near-capacity, save/restore and rewind witness on the accepted runtime.

Fixed profile from runtime/exp244, with only -c changed to 512 so the request is
near capacity instead of 3 percent of a 256K context. The port is changed and a
slot-save directory is added; nothing else is different.

Sequence:
  B1  short prompt on a fresh slot
  A   ~440-token prompt in a 512 context
  save + restore, then A again -> must equal A
  erase the slot (rewind), then B again -> must equal B1
"""
from pathlib import Path
import hashlib, json, os, signal, socket, subprocess, sys, time, urllib.request

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
REF = F / 'runtime/exp244/results-apex-or244-or-goose'
RUN = F / 'runtime/exp244/run'
OUT = F / 'runtime/exp418/build'
OUT.mkdir(parents=True, exist_ok=True)
SLOTS = OUT / 'slots'
SLOTS.mkdir(exist_ok=True)
PORT = 18307

argv = json.loads((REF / 'argv.json').read_text())
argv[0] = str(RUN / 'llama-server')
argv[argv.index('-c') + 1] = '512'
argv[argv.index('--port') + 1] = str(PORT)
argv += ['--slot-save-path', str(SLOTS)]
env = json.loads((REF / 'environment.json').read_text())
env['LD_LIBRARY_PATH'] = str(RUN)
env.pop('LD_PRELOAD', None)

with socket.socket() as s:
    if s.connect_ex(('127.0.0.1', PORT)) == 0:
        print('port busy'); sys.exit(1)

log = open(OUT / 'server.log', 'wb')
proc = subprocess.Popen(argv, cwd=str(RUN), env=env, stdout=log, stderr=subprocess.STDOUT)


def ready(t=900):
    t0 = time.time()
    while time.time() - t0 < t:
        if proc.poll() is not None:
            return False
        try:
            with urllib.request.urlopen(f'http://127.0.0.1:{PORT}/health', timeout=3) as r:
                if json.loads(r.read()).get('status') == 'ok':
                    return True
        except Exception:
            pass
        time.sleep(2)
    return False


def post(path, payload, timeout=1800):
    r = urllib.request.Request(f'http://127.0.0.1:{PORT}{path}',
                               data=json.dumps(payload).encode(),
                               headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(r, timeout=timeout) as resp:
        return json.loads(resp.read())


def token_count(text):
    return len(post('/tokenize', {'content': text})['tokens'])


def build_prompt(target=440):
    base = 'The quick brown fox jumps over the lazy dog. '
    n = 1
    while token_count(base * n) < target:
        n += 1
    text = base * n
    # Trim words from the end until the prompt fits under target + 20.
    while token_count(text) > target + 20:
        text = ' '.join(text.split()[:-1])
    return text


def summary(resp):
    return {
        'content_sha': hashlib.sha256((resp.get('content') or '').encode()).hexdigest(),
        'tokens': resp.get('tokens') or [],
        'probs': resp.get('completion_probabilities') or [],
        'tokens_predicted': resp.get('tokens_predicted'),
        'stop_type': resp.get('stop_type'),
    }


try:
    if not ready():
        print('not ready')
        print((OUT / 'server.log').read_text()[-3000:])
        sys.exit(1)

    short = 'Reply with exactly one word: hello.'
    long_prompt = build_prompt(440)
    n_long = token_count(long_prompt)
    n_short = token_count(short)
    print(json.dumps({'short_tokens': n_short, 'long_tokens': n_long, 'context': 512}))

    req = {'prompt': short, 'n_predict': 8, 'temperature': 1.0, 'seed': 123,
           'cache_prompt': False, 'return_tokens': True, 'n_probs': 5}
    b1 = summary(post('/completion', req))
    (OUT / 'b1.json').write_text(json.dumps(b1, indent=2) + '\n')

    req_long = dict(req, prompt=long_prompt)
    a1 = summary(post('/completion', req_long))
    (OUT / 'a1.json').write_text(json.dumps(a1, indent=2) + '\n')

    save = post('/slots/0?action=save', {'filename': 'near.bin'}, 600)
    restore = post('/slots/0?action=restore', {'filename': 'near.bin'}, 600)
    (OUT / 'save.json').write_text(json.dumps(save, indent=2) + '\n')
    (OUT / 'restore.json').write_text(json.dumps(restore, indent=2) + '\n')

    a2 = summary(post('/completion', req_long))
    (OUT / 'a2.json').write_text(json.dumps(a2, indent=2) + '\n')

    erase = post('/slots/0?action=erase', {}, 600)
    (OUT / 'erase.json').write_text(json.dumps(erase, indent=2) + '\n')
    b2 = summary(post('/completion', req))
    (OUT / 'b2.json').write_text(json.dumps(b2, indent=2) + '\n')

    result = {
        'short_tokens': n_short,
        'long_tokens': n_long,
        'near_capacity_ratio': round(n_long / 512.0, 4),
        'a_tokens_equal': a1['tokens'] == a2['tokens'],
        'a_probs_equal': a1['probs'] == a2['probs'],
        'a_content_equal': a1['content_sha'] == a2['content_sha'],
        'b_tokens_equal': b1['tokens'] == b2['tokens'],
        'b_probs_equal': b1['probs'] == b2['probs'],
        'b_content_equal': b1['content_sha'] == b2['content_sha'],
        'save_ok': 'n_saved' in save,
        'restore_ok': 'n_restored' in restore,
        'slot_file_size': (SLOTS / 'near.bin').stat().st_size if (SLOTS / 'near.bin').exists() else 0,
        'erase_ok': 'n_erased' in erase,
    }
    result['all_equal'] = all(result[k] for k in
        ['a_tokens_equal', 'a_probs_equal', 'a_content_equal',
         'b_tokens_equal', 'b_probs_equal', 'b_content_equal'])
    (OUT / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    sys.exit(0 if result['all_equal'] else 1)
finally:
    if proc.poll() is None:
        proc.send_signal(signal.SIGINT)
        try:
            proc.wait(timeout=120)
        except subprocess.TimeoutExpired:
            proc.kill()
    log.close()
    print('server stopped rc =', proc.returncode)
