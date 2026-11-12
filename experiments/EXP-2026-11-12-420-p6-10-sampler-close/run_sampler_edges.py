#!/usr/bin/env python3
"""P6-10 completion: EOS/ignore_eos and adaptive-sampler probe on two lines.

Both servers use the same argv, model and context 512. Our runtime is the
accepted exp244 checkpoint; the control is the flint build-cuda/bin. The short
prompt reaches EOS in three tokens, which makes ignore_eos observable.
"""
from pathlib import Path
import hashlib, json, os, signal, socket, subprocess, sys, time, urllib.request

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
REF = F / 'runtime/exp244/results-apex-or244-or-goose'
OURS = F / 'runtime/exp244/run'
NATIVE = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin')
OUT = F / 'runtime/exp420/build'
OUT.mkdir(parents=True, exist_ok=True)

base_argv = json.loads((REF / 'argv.json').read_text())
base_argv[base_argv.index('-c') + 1] = '512'
base_env = json.loads((REF / 'environment.json').read_text())
base_env.pop('LD_PRELOAD', None)


def wait_health(port, proc, t=900):
    t0 = time.time()
    while time.time() - t0 < t:
        if proc.poll() is not None:
            return False
        try:
            with urllib.request.urlopen(f'http://127.0.0.1:{port}/health', timeout=3) as r:
                if json.loads(r.read()).get('status') == 'ok':
                    return True
        except Exception:
            pass
        time.sleep(2)
    return False


def post(port, path, payload, timeout=900):
    r = urllib.request.Request(f'http://127.0.0.1:{port}{path}',
                               data=json.dumps(payload).encode(),
                               headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(r, timeout=timeout) as resp:
        return json.loads(resp.read())


def run_arm(name, bindir, port):
    argv = list(base_argv)
    argv[0] = str(bindir / 'llama-server')
    argv[argv.index('--port') + 1] = str(port)
    env = dict(base_env)
    env['LD_LIBRARY_PATH'] = str(bindir)
    log = open(OUT / f'{name}.server.log', 'wb')
    proc = subprocess.Popen(argv, cwd=str(bindir), env=env, stdout=log, stderr=subprocess.STDOUT)
    try:
        if not wait_health(port, proc):
            print(f'{name}: not ready')
            print((OUT / f'{name}.server.log').read_text()[-2000:])
            return None
        short = 'Reply with exactly one word: hello.'
        hot = ('Once upon a time there was a very curious cat who liked to walk '
               'through the old town every morning before the shops opened. ')
        results = {}
        results['base'] = post(port, '/completion', {
            'prompt': short, 'n_predict': 8, 'temperature': 1.0, 'seed': 123,
            'cache_prompt': False, 'return_tokens': True, 'n_probs': 5})
        results['ignore_eos'] = post(port, '/completion', {
            'prompt': short, 'n_predict': 8, 'temperature': 1.0, 'seed': 123,
            'cache_prompt': False, 'return_tokens': True, 'n_probs': 5,
            'ignore_eos': True})
        results['hot_base'] = post(port, '/completion', {
            'prompt': hot, 'n_predict': 32, 'temperature': 1.5, 'seed': 7,
            'cache_prompt': False, 'return_tokens': True,
            'samplers': ['top_k', 'top_p', 'temperature']})
        results['hot_adaptive'] = post(port, '/completion', {
            'prompt': hot, 'n_predict': 32, 'temperature': 1.5, 'seed': 7,
            'cache_prompt': False, 'return_tokens': True,
            'samplers': ['top_k', 'top_p', 'temperature', 'adaptive_p'],
            'adaptive_target': 0.5, 'adaptive_decay': 0.9})
        return results
    finally:
        if proc.poll() is None:
            proc.send_signal(signal.SIGINT)
            try:
                proc.wait(timeout=120)
            except subprocess.TimeoutExpired:
                proc.kill()
        log.close()
        print(f'{name}: server rc =', proc.returncode)


def summarize(r):
    return {
        'tokens': r.get('tokens') or [],
        'content_sha': hashlib.sha256((r.get('content') or '').encode()).hexdigest(),
        'tokens_predicted': r.get('tokens_predicted'),
        'stop_type': r.get('stop_type'),
        'probs': r.get('completion_probabilities') or [],
    }


for port, b in [(18308, OURS), (18309, NATIVE)]:
    with socket.socket() as s:
        if s.connect_ex(('127.0.0.1', port)) == 0:
            print('port busy', port); sys.exit(1)

ours = run_arm('ours', OURS, 18308)
nat = run_arm('native', NATIVE, 18309)
if ours is None or nat is None:
    sys.exit(1)

report = {}
for name in ['base', 'ignore_eos', 'hot_base', 'hot_adaptive']:
    o, n = summarize(ours[name]), summarize(nat[name])
    report[name] = {
        'ours': o, 'native': n,
        'tokens_equal': o['tokens'] == n['tokens'],
        'content_equal': o['content_sha'] == n['content_sha'],
        'probs_equal': o['probs'] == n['probs'],
    }
report['ignore_eos_changes_output'] = report['base']['ours']['tokens'] != report['ignore_eos']['ours']['tokens']
report['adaptive_changes_output'] = report['hot_base']['ours']['tokens'] != report['hot_adaptive']['ours']['tokens']
report['base_stop'] = report['base']['ours']['stop_type']
report['ignore_eos_stop'] = report['ignore_eos']['ours']['stop_type']
(OUT / 'summary.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({k: v for k, v in report.items() if not isinstance(v, dict)}, indent=2))
print(json.dumps({k: {kk: vv for kk, vv in v.items() if kk.endswith('equal')}
                  for k, v in report.items() if isinstance(v, dict)}, indent=2))
