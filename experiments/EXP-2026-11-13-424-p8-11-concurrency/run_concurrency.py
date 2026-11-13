#!/usr/bin/env python3
"""P8-11: concurrent clients, invalid input, disconnect and a clean restart."""
from pathlib import Path
import hashlib, json, signal, socket, subprocess, threading, time, urllib.error, urllib.request

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
RUN = F / 'runtime/exp421/unified'
OUT = F / 'runtime/exp424/build'
OUT.mkdir(parents=True, exist_ok=True)
PORT = 18322
MODEL = '/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf'

argv = [str(RUN / 'llama-server'), '-m', MODEL, '--alias', 'Apex-35B-V1.0',
        '-c', '4096', '-ngl', 'all', '-fit', 'off', '--cpu-moe', '-fa', 'on',
        '-ctk', 'q4_0', '-ctv', 'q4_0', '-b', '2048', '-ub', '2048',
        '-t', '8', '-tb', '16', '-np', '4', '--cache-ram', '0',
        '--spec-type', 'none', '--jinja', '--temp', '1', '--top-p', '0.95',
        '--top-k', '20', '--min-p', '0', '--host', '127.0.0.1', '--port', str(PORT)]
env = {'PATH': '/usr/bin:/bin', 'HOME': '/tmp', 'LD_LIBRARY_PATH': str(RUN),
       'FRANKENMOE_LOG_MODE': 'classic'}


def post(path, payload, timeout=600):
    r = urllib.request.Request(f'http://127.0.0.1:{PORT}{path}',
                               data=json.dumps(payload).encode(),
                               headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(r, timeout=timeout) as resp:
        return resp.status, json.loads(resp.read())


def sha(d):
    return hashlib.sha256((d.get('content') or '').encode()).hexdigest()


with socket.socket() as s:
    if s.connect_ex(('127.0.0.1', PORT)) == 0:
        print('port busy'); raise SystemExit(1)

log = open(OUT / 'server.log', 'wb')
proc = subprocess.Popen(argv, cwd=str(RUN), env=env, stdout=open(OUT / 'stdout.log', 'wb'), stderr=log)
summary = {}
try:
    t0 = time.time()
    while time.time() - t0 < 900:
        if proc.poll() is not None:
            raise SystemExit('server exited early')
        try:
            with urllib.request.urlopen(f'http://127.0.0.1:{PORT}/health', timeout=3) as r:
                if json.loads(r.read()).get('status') == 'ok':
                    break
        except Exception:
            pass
        time.sleep(2)

    base_req = {'prompt': 'Reply with exactly one word: hello.', 'n_predict': 8,
                'temperature': 1.0, 'seed': 123, 'cache_prompt': False,
                'return_tokens': True}
    _, base = post('/completion', base_req)
    summary['base_sha'] = sha(base)
    summary['base_tokens'] = base.get('tokens')

    results = [None] * 4
    def worker(i):
        req = dict(base_req, prompt=f'Reply with exactly one word: hello. #{i}',
                   seed=123 + i)
        try:
            st, d = post('/completion', req)
            results[i] = {'status': st, 'sha': sha(d), 'tokens': d.get('tokens'),
                          'predicted': d['timings']['predicted_n']}
        except Exception as e:
            results[i] = {'error': repr(e)}

    th = [threading.Thread(target=worker, args=(i,)) for i in range(4)]
    for t in th: t.start()
    for t in th: t.join()
    summary['concurrent'] = results
    summary['concurrent_ok'] = all(r and r.get('status') == 200 for r in results)

    # Invalid JSON: must be rejected and must not kill the server.
    bad = urllib.request.Request(f'http://127.0.0.1:{PORT}/completion',
                                 data=b'{"prompt": ', headers={'Content-Type': 'application/json'})
    try:
        with urllib.request.urlopen(bad, timeout=30) as r:
            bad_status = r.status
    except urllib.error.HTTPError as e:
        bad_status = e.code
    summary['invalid_status'] = bad_status
    _, after_bad = post('/completion', base_req)
    summary['after_invalid_sha'] = sha(after_bad)

    # Streaming disconnect after three events, then a normal request.
    stream = urllib.request.Request(f'http://127.0.0.1:{PORT}/completion',
                                    data=json.dumps(dict(base_req, prompt='Count slowly.', stream=True,
                                                         n_predict=64)).encode(),
                                    headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(stream, timeout=60) as r:
        for i, _ in enumerate(r):
            if i >= 3:
                break
    time.sleep(1)
    _, after_cancel = post('/completion', base_req)
    summary['after_disconnect_sha'] = sha(after_cancel)
    summary['repeated_equal'] = sha(after_bad) == sha(after_cancel) == summary['base_sha']

    # Second lifecycle: same binary, same request, fresh process is checked by restart below.
finally:
    if proc.poll() is None:
        proc.send_signal(signal.SIGINT)
        try:
            proc.wait(timeout=120)
        except subprocess.TimeoutExpired:
            proc.kill()
    log.close()
    summary['server_rc'] = proc.returncode

summary['no_port_left'] = socket.socket().connect_ex(('127.0.0.1', PORT)) != 0

# Second lifecycle: fresh process, same request, must reproduce the base answer.
log2 = open(OUT / 'server2.log', 'wb')
proc2 = subprocess.Popen(argv, cwd=str(RUN), env=env, stdout=open(OUT / 'stdout2.log', 'wb'), stderr=log2)
try:
    t0 = time.time()
    while time.time() - t0 < 900:
        if proc2.poll() is not None:
            break
        try:
            with urllib.request.urlopen(f'http://127.0.0.1:{PORT}/health', timeout=3) as r:
                if json.loads(r.read()).get('status') == 'ok':
                    break
        except Exception:
            pass
        time.sleep(2)
    _, restarted = post('/completion', base_req)
    summary['restart_sha'] = sha(restarted)
    summary['restart_equal'] = summary['restart_sha'] == summary['base_sha']
finally:
    if proc2.poll() is None:
        proc2.send_signal(signal.SIGINT)
        try:
            proc2.wait(timeout=120)
        except subprocess.TimeoutExpired:
            proc2.kill()
    log2.close()
    summary['restart_rc'] = proc2.returncode

(OUT / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps(summary, indent=2))
