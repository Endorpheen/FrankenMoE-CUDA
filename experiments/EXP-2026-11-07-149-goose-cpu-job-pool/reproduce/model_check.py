from pathlib import Path
import hashlib
import json
import os
import signal
import socket
import subprocess
import threading
import time
import urllib.error
import urllib.request

import argparse
parser=argparse.ArgumentParser()
parser.add_argument('arm',choices=['omp','pthread'])
options=parser.parse_args()
original=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
r=Path('/home/end0/AI/LlamaServer/franken-goose-exp149-cpu-control-20261107')
out=r/'results/model'
out.mkdir(exist_ok=True)
name='audit-exp149-'+options.arm+'-apex-32k128'
prefix = out / name
assert not prefix.with_name(name + '-receipt.json').exists(), 'One diagnostic attempt only'
bundle=r/'variants'/('pthread' if options.arm=='pthread' else 'cpu-backend')/'run'
argv = json.loads((original/'results/plan-apex-native-argv.json').read_text())
argv[0] = str(bundle / 'llama-server')
request_bytes = (original/'results/plan-apex-native-32k128-request.json').read_bytes()
payload = json.loads(request_bytes)
assert len(payload['prompt']) == 32768 and payload['n_predict'] == 128
gate=json.loads((r/'results/sanitized/summary.json').read_text())
assert gate['status']=='PASS_EXP149_SCOPED_SANITIZED'
env = dict(os.environ)
env.pop('LD_PRELOAD', None)
for key in list(env):
    if key.startswith('GGML_GOOSE_'):
        env.pop(key)
flags = ['ALLOC', 'COALESCE', 'LIFETIME', 'GRAPH_PLAN', 'MOE', 'TILED',
         'TILED_GENERAL', 'ELEMENTWISE', 'MOE_QUANT', 'MOE_CONTROL', 'RMS',
         'CPU_BACKEND', 'CPU_EXECUTOR', 'CPU_CONTROL', 'CPU_BACKEND_API']
for flag in flags:
    env['GGML_GOOSE_' + flag] = '0' if options.arm=='cpp' else '1'
env['GGML_GOOSE_CPU_QUANT']='1'
env['GGML_GOOSE_CPU_POOL']='1'
assert json.loads((r/'results/inherited-quant-evidence.json').read_text())['status']=='PASS_EXACT_UNCHANGED_QUANT_SOURCE_AND_GENERATED_C'
assert json.loads((r/'results/extra-summary.json').read_text())['status'].startswith('PASS_')
assert json.loads((r/'results/tsan-summary.json').read_text())['status']=='PASS_SCOPED_PTHREAD_TSAN'
assert json.loads((r/'results/pthread-sanitized/summary.json').read_text())['status'].startswith('PASS_')
env.update(GGML_GOOSE_ALLOC_VERIFY='0', GGML_EXPERT_HOTSTORE='0', LD_LIBRARY_PATH=str(bundle))
env['LD_PRELOAD'] = ' '.join(str(r / 'build' / f) for f in [
    'libfranken-goose-silu.so', 'libfranken-goose-scale.so', 'libfranken-goose-rms-block.so'])
if options.arm=='cpp':env.pop('LD_PRELOAD',None)
with socket.socket() as sock:
    assert sock.connect_ex(('127.0.0.1', 8081)) != 0, 'Port occupied; leave other workload alone'

def save(suffix, value):
    (out / (name + suffix)).write_text(json.dumps(value, indent=2) + '\n')

files = list(bundle.glob('*.so*')) + [bundle / 'llama-server']
files += [Path(p) for p in env.get('LD_PRELOAD','').split()]
hashes = {str(p.resolve()): hashlib.sha256(p.read_bytes()).hexdigest() for p in files if p.is_file()}
source = r / 'source/cpu-backend'
source_hashes = {str(p.relative_to(source)): hashlib.sha256(p.read_bytes()).hexdigest()
                 for p in source.rglob('*') if p.is_file() and p.suffix in ['.goose', '.c', '.cpp', '.h']}
receipt = {'purpose': 'EXP149 full CPU job/pool policy: one request per OMP/pthread mode, fixed profile,no optimization series',
           'attempts': 1, 'completion_requests': 0, 'model': 'Apex',
           'baseline': 'One fresh original C++ request plus EXP146/EXP147/EXP148, sequential, same settings',
           'request_sha256': hashlib.sha256(request_bytes).hexdigest(),
           'libraries': hashes, 'source_sha256': source_hashes,
           'producer': json.loads((source / 'producer.json').read_text()),
           'checkpoint': '3e901fe plus isolated EXP149 CPU pool/job policy',
           'variant': options.arm,
           'production_changed': False, 'qwen_run': False, 'status': 'STARTING'}
save('-receipt.json', receipt)
save('-argv.json', argv)
save('-environment.json', {k: v for k, v in env.items()
                          if k.startswith('GGML_') or k in ['LD_PRELOAD', 'LD_LIBRARY_PATH']})
(out / (name + '-request.json')).write_bytes(request_bytes)
samples = []
done = threading.Event()

def swapout():
    return next(int(x.split()[1]) for x in Path('/proc/vmstat').read_text().splitlines()
                if x.startswith('pswpout '))

def memory(pid):
    while not done.is_set():
        values = {'t': time.time()}
        try:
            for line in Path(f'/proc/{pid}/status').read_text().splitlines():
                key = line.split(':')[0]
                if key in ['VmRSS', 'VmHWM', 'VmSwap', 'RssAnon', 'RssFile']:
                    values[key] = int(line.split()[1])
            gpu = subprocess.run(['nvidia-smi', '--query-compute-apps=pid,used_memory',
                                  '--format=csv,noheader,nounits'], capture_output=True,
                                 text=True, timeout=5)
            for line in gpu.stdout.splitlines():
                cells = line.split(',')
                if len(cells) == 2 and cells[0].strip() == str(pid):
                    values['gpu_mib'] = int(cells[1])
            samples.append(values)
        except (OSError, ValueError, subprocess.TimeoutExpired):
            pass
        done.wait(0.5)

server = None
monitor = None
try:
    with (out / (name + '-server.log')).open('wb') as log:
        server = subprocess.Popen(argv, env=env, stdout=log, stderr=subprocess.STDOUT)
    receipt.update(pid=server.pid, status='LOADING')
    save('-receipt.json', receipt)
    print('Starting one diagnostic Apex server, pid', server.pid, flush=True)
    deadline = time.monotonic() + 240
    while True:
        if server.poll() is not None:
            raise RuntimeError(f'Server exited before readiness: {server.returncode}')
        try:
            with urllib.request.urlopen('http://127.0.0.1:8081/health', timeout=2) as response:
                if response.status == 200:
                    break
        except (urllib.error.URLError, TimeoutError):
            pass
        if time.monotonic() > deadline:
            raise TimeoutError('Model readiness deadline reached; no completion sent')
        time.sleep(0.5)
    print('Apex ready; sending the single preserved 32768/128 request', flush=True)
    monitor = threading.Thread(target=memory, args=(server.pid,))
    monitor.start()
    start_swap = swapout()
    start_load = os.getloadavg()
    start = time.monotonic()
    receipt.update(status='ONE_REQUEST_IN_FLIGHT', completion_requests=1)
    save('-receipt.json', receipt)
    request = urllib.request.Request('http://127.0.0.1:8081/completion', data=request_bytes,
                                    headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(request, timeout=900) as response:
        result = json.load(response)
    elapsed = time.monotonic() - start
    done.set()
    monitor.join()
    save('-response.json', result)
    save('-memory.json', samples)
    timings = result.get('timings', {})
    assert timings.get('prompt_n') == 32768 and timings.get('predicted_n') == 128
    assert timings.get('cache_n') == 0
    original = json.loads((original/'results/plan-apex-native-32k128-response.json').read_text())
    exact = {key: result.get(key) == original.get(key)
             for key in ['content', 'tokens', 'completion_probabilities']}
    same_positions = sum(a == b for a, b in zip(original['tokens'], result['tokens']))
    summary = {'status': 'COMPLETED_SINGLE_DIAGNOSTIC_NOT_B1_ACCEPTANCE',
               'name': name, 'wall_seconds': elapsed, 'timings': timings,
               'exact': exact, 'matching_token_positions': same_positions,
               'sampled_rss_peak_mib': max((s.get('VmRSS', 0) for s in samples), default=0) / 1024,
               'process_hwm_mib': max((s.get('VmHWM', 0) for s in samples), default=0) / 1024,
               'sampled_anon_peak_mib': max((s.get('RssAnon', 0) for s in samples), default=0) / 1024,
               'sampled_file_peak_mib': max((s.get('RssFile', 0) for s in samples), default=0) / 1024,
               'process_swap_peak_kib': max((s.get('VmSwap', 0) for s in samples), default=0),
               'sampled_gpu_peak_mib': max((s.get('gpu_mib', 0) for s in samples), default=0),
               'system_swapout_pages_delta': swapout() - start_swap,
               'loadavg_start': start_load, 'loadavg_end': os.getloadavg(),
               'preliminary': True, 'controlled_quiet_run': False,
               'baseline': 'Saved original C++ measurement, identical request and workload argv',
               'quality_scope': 'One fixed-prompt response equivalence only; not a general quality benchmark',
               'no_optimization': True, 'no_microbenchmarks': True,
               'no_repeated_series': True, 'no_b1_acceptance': True}
    save('-summary.json', summary)
    receipt['status'] = summary['status']
    print(json.dumps(summary, indent=2), flush=True)
except Exception as error:
    receipt.update(status='FAILED_SINGLE_DIAGNOSTIC_NO_RETRY', error=str(error))
    print(receipt['status'], str(error), flush=True)
    raise
finally:
    done.set()
    if monitor is not None:
        monitor.join()
    if server is not None and server.poll() is None:
        server.send_signal(signal.SIGINT)
        try:
            server.wait(timeout=30)
        except subprocess.TimeoutExpired:
            server.terminate()
            server.wait(timeout=15)
    if server is not None:
        receipt['server_exit_code'] = server.returncode
    save('-receipt.json', receipt)
    print('Own diagnostic server stopped; no repeat request scheduled', flush=True)
