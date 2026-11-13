#!/usr/bin/env python3
"""EXP288: detokenisation and token-piece differential -- accepted runtime vs the native flint library."""
from pathlib import Path
import json, random, signal, socket, subprocess, sys, time, urllib.request

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-288-p7-07-detok-diff'
RUN = F / 'runtime/reconstructed/build/stage-run'
REF = F / 'runtime/exp244/results-apex-or244-or-goose'
OUT = E / 'run'; OUT.mkdir(parents=True, exist_ok=True)
MODEL = '/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf'
NATIVE_IDS = F / 'experiments/EXP-2026-11-12-287-p7-06-tokenizer-diff/native-ids.txt'

# deterministic piece sample: every id below 256, then random ids across the vocabulary
random.seed(288)
n_vocab = 248320
sample = list(range(256)) + [random.randrange(n_vocab) for _ in range(2000)]
(E / 'pieces.txt').write_text('\n'.join(str(i) for i in sample) + '\n')

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


def detok(tokens):
    body = {'tokens': tokens}
    r = urllib.request.Request('http://127.0.0.1:8081/detokenize', data=json.dumps(body).encode(),
                               headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(r, timeout=120) as resp:
        return json.loads(resp.read()).get('content') or ''


try:
    if not ready():
        print('not ready'); sys.exit(1)

    # arm 1: pieces
    native_pieces = [json.loads(l) for l in (E / 'native-pieces.txt').read_text().splitlines()] \
        if (E / 'native-pieces.txt').exists() else None
    if native_pieces is None:
        print('native-pieces.txt missing'); sys.exit(1)
    piece_bad = []
    for idx, tid in enumerate(sample):
        got = detok([tid])
        if got != native_pieces[idx]:
            piece_bad.append((tid, native_pieces[idx], got))

    # arm 2: the tokenizer corpus ids, detokenized through both arms
    id_rows = [json.loads(l) for l in NATIVE_IDS.read_text().splitlines()]
    native_text = [json.loads(l) for l in (E / 'native-text.txt').read_text().splitlines()] \
        if (E / 'native-text.txt').exists() else None
    if native_text is None:
        print('native-text.txt missing'); sys.exit(1)
    text_bad = []
    for idx, ids in enumerate(id_rows):
        got = detok(ids)
        if got != native_text[idx]:
            text_bad.append((idx, native_text[idx][:60], got[:60], ids[:8]))

    summary = {
        'pieces_cases': len(sample), 'pieces_mismatches': len(piece_bad),
        'pieces_first': piece_bad[:5],
        'text_cases': len(id_rows), 'text_mismatches': len(text_bad),
        'text_first': text_bad[:5],
    }
    (OUT / 'diff.json').write_text(json.dumps(summary, indent=2, ensure_ascii=False) + '\n')
    print(json.dumps({k: v for k, v in summary.items() if not k.endswith('_first')}, indent=2))
    sys.exit(0 if not piece_bad and not text_bad else 2)
finally:
    if proc.poll() is None:
        proc.send_signal(signal.SIGINT)
        try:
            proc.wait(timeout=120)
        except subprocess.TimeoutExpired:
            proc.kill()
    log.close()
