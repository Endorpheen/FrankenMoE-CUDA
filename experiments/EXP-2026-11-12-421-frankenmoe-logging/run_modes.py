#!/usr/bin/env python3
"""Run the staged FrankenMoE logger in classic, franken and json modes."""
from pathlib import Path
import hashlib, json, os, signal, socket, subprocess, sys, time, urllib.request

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
BIN = F / 'runtime/exp421/stage/bin'
OUT = F / 'runtime/exp421/build/modes'
OUT.mkdir(parents=True, exist_ok=True)
MODEL = '/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf'
PORT = 18311


def post(path, payload, timeout=900):
    r = urllib.request.Request(f'http://127.0.0.1:{PORT}{path}',
                               data=json.dumps(payload).encode(),
                               headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(r, timeout=timeout) as resp:
        return json.loads(resp.read())


def run_mode(mode):
    argv = [str(BIN / 'llama-server'), '-m', MODEL, '--alias', 'Apex-35B-V1.0',
            '-c', '512', '-ngl', 'all', '-fit', 'off', '--cpu-moe', '-fa', 'on',
            '-ctk', 'q4_0', '-ctv', 'q4_0', '-b', '4096', '-ub', '4096',
            '-t', '8', '-tb', '16', '-np', '1', '--cache-ram', '0',
            '--spec-type', 'none', '--jinja', '--temp', '1', '--top-p', '0.95',
            '--top-k', '20', '--min-p', '0', '--host', '127.0.0.1',
            '--port', str(PORT)]
    env = {'PATH': '/usr/bin:/bin', 'HOME': '/tmp', 'LD_LIBRARY_PATH': str(BIN),
           'FRANKENMOE_LOG_MODE': mode}
    logpath = OUT / f'{mode}.log'
    log = open(logpath, 'wb')
    errpath = OUT / f'{mode}.err.log'
    err = open(errpath, 'wb')
    proc = subprocess.Popen(argv, cwd=str(BIN), env=env, stdout=log, stderr=err)
    t0 = time.time()
    ready = False
    try:
        while time.time() - t0 < 900:
            if proc.poll() is not None:
                break
            try:
                with urllib.request.urlopen(f'http://127.0.0.1:{PORT}/health', timeout=3) as r:
                    if json.loads(r.read()).get('status') == 'ok':
                        ready = True
                        break
            except Exception:
                pass
            time.sleep(2)
        if not ready:
            return {'mode': mode, 'ready': False, 'rc': proc.poll(),
                    'log_tail': logpath.read_text(errors='replace')[-2000:]}
        startup_s = round(time.time() - t0, 2)
        resp = post('/completion', {'prompt': 'Reply with exactly one word: hello.',
                                    'n_predict': 128, 'temperature': 1.0, 'seed': 123, 'ignore_eos': True,
                                    'cache_prompt': False, 'return_tokens': True})
        return {
            'mode': mode, 'ready': True, 'startup_s': startup_s,
            'content_sha': hashlib.sha256((resp.get('content') or '').encode()).hexdigest(),
            'tokens': resp.get('tokens') or [],
            'timings': resp.get('timings') or {},
            'log_lines': len(logpath.read_text(errors='replace').splitlines()),
        }
    finally:
        if proc.poll() is None:
            proc.send_signal(signal.SIGINT)
            try:
                proc.wait(timeout=120)
            except subprocess.TimeoutExpired:
                proc.kill()
        log.close()
        err.close()


with socket.socket() as s:
    if s.connect_ex(('127.0.0.1', PORT)) == 0:
        print('port busy'); sys.exit(1)

res = {}
for mode in ['classic', 'franken', 'json']:
    res[mode] = run_mode(mode)
    print(json.dumps(res[mode]))

# Log checks
classic = (OUT / 'classic.err.log').read_text(errors='replace')
franken = (OUT / 'franken.err.log').read_text(errors='replace')
jsonl = (OUT / 'json.log').read_text(errors='replace')

json_objs, bad_json = [], []
for line in jsonl.splitlines():
    line = line.strip()
    if not line:
        continue
    if not line.startswith('{'):
        bad_json.append(line[:80])
        continue
    try:
        json_objs.append(json.loads(line))
    except Exception as e:
        bad_json.append(f'{line[:80]} :: {e}')

events = sorted({o.get('event') for o in json_objs if isinstance(o, dict)})
checks = {
    'all_ready': all(res[m].get('ready') for m in res),
    'same_tokens': len({tuple(res[m].get('tokens') or []) for m in res}) == 1,
    'same_content': len({res[m].get('content_sha') for m in res}) == 1,
    'json_bad_lines': bad_json[:10],
    'json_objects': len(json_objs),
    'json_events': events,
    'classic_has_cuda': 'CUDA' in classic or 'cuda' in classic,
    'classic_has_model': 'model' in classic.lower(),
    'classic_has_kv': 'KV' in classic or 'cache' in classic.lower(),
    'franken_banner_alive': 'FRANKENMOE IS ALIVE' in franken,
    'franken_banner_loaded': 'THE BEAST IS LOADED' in franken or 'UNLEASH THE MONSTER' in franken,
    'franken_error_banner': 'HIT A WALL' in franken,
    'franken_banner_counts': {b: franken.count(b) for b in
        ['FRANKENMOE IS ALIVE', 'UNLEASH THE MONSTER', 'THE BEAST IS LOADED',
         'CACHE ARMED', 'ALL SYSTEMS BURNING', 'FULL THROTTLE', 'HIT A WALL',
         'POWERING DOWN']},
}
(OUT / 'summary.json').write_text(json.dumps({'runs': res, 'checks': checks}, indent=2) + '\n')
print(json.dumps(checks, indent=2))
