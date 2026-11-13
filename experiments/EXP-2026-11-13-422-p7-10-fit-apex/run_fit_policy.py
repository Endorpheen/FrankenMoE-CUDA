#!/usr/bin/env python3
"""P7-10 memory-fit policy on Apex: native vs unified, --fit on/off.

No generation. Each server is started with trace logging, allowed to finish
model load, then stopped with SIGINT. Fit decisions and the resulting placement
are compared line by line after removing ports and paths.
"""
from pathlib import Path
import json, os, re, signal, socket, subprocess, time, urllib.request

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
NATIVE = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin')
OURS = F / 'runtime/exp421/unified'
OUT = F / 'runtime/exp422/build'
OUT.mkdir(parents=True, exist_ok=True)
MODEL = '/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf'


def wait_health(port, proc, t=300):
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


def run(arm, bindir, mode, port):
    argv = [str(bindir / 'llama-server'), '-m', MODEL, '--alias', 'Apex-35B-V1.0',
            '-c', '32768', '-ngl', 'all', '-fit', mode, '--cpu-moe', '-fa', 'on',
            '-ctk', 'q4_0', '-ctv', 'q4_0', '-b', '4096', '-ub', '4096',
            '-t', '8', '-tb', '16', '-np', '1', '--cache-ram', '0',
            '--spec-type', 'none', '--jinja', '-lv', '4',
            '--host', '127.0.0.1', '--port', str(port)]
    env = {'PATH': '/usr/bin:/bin', 'HOME': '/tmp', 'LD_LIBRARY_PATH': str(bindir)}
    tag = f'{arm}-{mode}'
    log = open(OUT / f'{tag}.log', 'wb')
    proc = subprocess.Popen(argv, cwd=str(bindir), env=env, stdout=log, stderr=subprocess.STDOUT)
    try:
        ready = wait_health(port, proc)
        return {'arm': arm, 'fit': mode, 'ready': ready, 'rc_before': proc.poll(),
                'tag': tag}
    finally:
        if proc.poll() is None:
            proc.send_signal(signal.SIGINT)
            try:
                proc.wait(timeout=120)
            except subprocess.TimeoutExpired:
                proc.kill()
        log.close()


def clean(text):
    out = []
    for line in text.splitlines():
        if line.startswith('FG'):
            continue
        line = re.sub(r'127\.0\.0\.1:\d+', '127.0.0.1:PORT', line)
        line = re.sub(r'0\.\d{2}\.\d{3}\.\d{3} ', '', line)
        if re.search(r'fit|offload|n_gpu_layers|n_ctx|buffer size|context|backend', line, re.I):
            out.append(line.strip())
    return out


with socket.socket() as s:
    if s.connect_ex(('127.0.0.1', 18320)) == 0 or s.connect_ex(('127.0.0.1', 18321)) == 0:
        print('port busy'); raise SystemExit(1)

rows = []
for arm, bindir, port in [('native', NATIVE, 18320), ('ours', OURS, 18321)]:
    for mode in ['on', 'off']:
        r = run(arm, bindir, mode, port)
        r['lines'] = clean((OUT / f"{r['tag']}.log").read_text(errors='replace'))
        rows.append(r)
        print(json.dumps({k: v for k, v in r.items() if k != 'lines'}))

report = {'runs': []}
for r in rows:
    report['runs'].append({k: v for k, v in r.items()})
for mode in ['on', 'off']:
    ours = next(r for r in rows if r['arm'] == 'ours' and r['fit'] == mode)
    nat = next(r for r in rows if r['arm'] == 'native' and r['fit'] == mode)
    report[f'{mode}_lines_equal'] = ours['lines'] == nat['lines']
    report[f'{mode}_diff_ours_only'] = [x for x in ours['lines'] if x not in nat['lines']][:10]
    report[f'{mode}_diff_native_only'] = [x for x in nat['lines'] if x not in ours['lines']][:10]
report['fit_changes_placement'] = {
    'ours': next(r for r in rows if r['arm'] == 'ours' and r['fit'] == 'on')['lines'] !=
            next(r for r in rows if r['arm'] == 'ours' and r['fit'] == 'off')['lines'],
    'native': next(r for r in rows if r['arm'] == 'native' and r['fit'] == 'on')['lines'] !=
              next(r for r in rows if r['arm'] == 'native' and r['fit'] == 'off')['lines'],
}
(OUT / 'summary.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({k: v for k, v in report.items() if k != 'runs'}, indent=2))
