#!/usr/bin/env python3
"""EXP287: tokenizer differential -- accepted runtime vs the native flint library, same corpus."""
from pathlib import Path
import json, signal, socket, subprocess, sys, time, urllib.request

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-287-p7-06-tokenizer-diff'
RUN = F / 'runtime/reconstructed/build/stage-run'
REF = F / 'runtime/exp244/results-apex-or244-or-goose'
OUT = E / 'run'; OUT.mkdir(parents=True, exist_ok=True)

# decode the corpus with the same escapes the native driver applies
def lines(p):
    out = []
    for raw in Path(p).read_text().splitlines():
        s, i, acc = raw, 0, []
        while i < len(s):
            if s[i] != '\\':
                acc.append(s[i]); i += 1; continue
            i += 1
            c = s[i]; i += 1
            if c == 'n': acc.append('\n')
            elif c == 't': acc.append('\t')
            elif c == 'r': acc.append('\r')
            elif c == 'u':
                acc.append(chr(int(s[i:i+4], 16))); i += 4
            else: acc.append(c)
        out.append(''.join(acc))
    return out

corpus = lines(E / 'corpus.txt')
native = [json.loads(l) for l in (E / 'native-ids.txt').read_text().splitlines()]

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


try:
    if not ready():
        print('not ready'); sys.exit(1)
    rows, mismatches = [], []
    for idx, text in enumerate(corpus):
        body = {'content': text, 'add_special': False, 'parse_special': True}
        r = urllib.request.Request('http://127.0.0.1:8081/tokenize', data=json.dumps(body).encode(),
                                   headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(r, timeout=120) as resp:
            got = json.loads(resp.read()).get('tokens') or []
        want = native[idx] if idx < len(native) else None
        ok = got == want
        rows.append(dict(i=idx, text=text, native=want, goose=got, ok=ok))
        if not ok:
            mismatches.append(rows[-1])
    (OUT / 'diff.json').write_text(json.dumps(dict(total=len(rows), mismatches=len(mismatches), rows=rows), indent=2) + '\n')
    print(f'cases: {len(rows)}, mismatches: {len(mismatches)}')
    for m in mismatches[:6]:
        print('  mismatch', m['i'], repr(m['text']), 'native', m['native'], 'goose', m['goose'])
    sys.exit(0 if not mismatches else 2)
finally:
    if proc.poll() is None:
        proc.send_signal(signal.SIGINT)
        try:
            proc.wait(timeout=120)
        except subprocess.TimeoutExpired:
            proc.kill()
    log.close()
