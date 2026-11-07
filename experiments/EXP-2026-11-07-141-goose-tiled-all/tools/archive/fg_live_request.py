import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import threading
import time
import urllib.request

root = Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
name, length, predict = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
pid = int((root / 'results/current-pid.txt').read_text())
def post(path, body):
    request = urllib.request.Request('http://127.0.0.1:8081' + path,
        data=json.dumps(body).encode(), headers={'Content-Type': 'application/json'})
    with urllib.request.urlopen(request, timeout=600 if ((root / 'results/current-mode.txt').read_text().startswith('qwen') or 'cpu-tiled' in (root / 'results/current-mode.txt').read_text()) else 120) as response:
        return json.load(response)
prefix = 'qwen-tokens-' if (root / 'results/current-mode.txt').read_text().startswith('qwen') else 'tokens-'
tokenfile = root / 'results' / (prefix + str(length) + '.json')
if tokenfile.exists():
    tokens = json.loads(tokenfile.read_text())
else:
    text = ('Assess this service log. Explain the capacity risks and propose a safe plan.\n' +
            'The queue grows when arrivals exceed completed jobs. Preserve consistency, measure latency, check retries.\n' * (length // 8 + 20))
    tokens = post('/tokenize', {'content': text, 'add_special': True})['tokens'][:length]
    assert len(tokens) == length
    tokenfile.write_text(json.dumps(tokens))
payload = {'prompt': tokens, 'n_predict': predict, 'temperature': 0,
           'seed': 20261107, 'cache_prompt': False, 'ignore_eos': True,
           'n_probs': 5, 'return_tokens': True}
if length == 32768:
    payload.update(temperature=1.0, seed=123, top_p=0.95, top_k=20, min_p=0)
(root / 'results' / (name + '-request.json')).write_text(json.dumps(payload))
samples = []
done = threading.Event()
def memory():
    while not done.is_set():
        values = {}
        try:
            for line in Path('/proc/' + str(pid) + '/status').read_text().splitlines():
                if line.split(':')[0] in ['VmRSS', 'VmHWM', 'VmSwap', 'RssAnon', 'RssFile']:
                    values[line.split(':')[0]] = int(line.split()[1])
            gpu = subprocess.run(['nvidia-smi', '--query-compute-apps=pid,used_memory',
                '--format=csv,noheader,nounits'], capture_output=True, text=True, timeout=5)
            for line in gpu.stdout.splitlines():
                cells = line.split(',')
                if len(cells) == 2 and cells[0].strip() == str(pid):
                    values['gpu_mib'] = int(cells[1].strip())
            values['t'] = time.time()
            samples.append(values)
        except (OSError, ValueError, subprocess.TimeoutExpired):
            pass
        done.wait(0.5)
def swapout():
    for line in Path('/proc/vmstat').read_text().splitlines():
        if line.startswith('pswpout '):
            return int(line.split()[1])
startswap = swapout()
thread = threading.Thread(target=memory)
thread.start()
start = time.monotonic()
try:
    response = post('/completion', payload)
finally:
    elapsed = time.monotonic() - start
    done.set()
    thread.join()
(root / 'results' / (name + '-response.json')).write_text(json.dumps(response))
(root / 'results' / (name + '-memory.json')).write_text(json.dumps(samples))
summary = {'name': name, 'wall_seconds': elapsed, 'timings': response.get('timings'),
    'content_sha256': hashlib.sha256(response.get('content', '').encode()).hexdigest(),
    'tokens': response.get('tokens'), 'sampled_rss_peak_mib': max((x.get('VmRSS', 0) for x in samples), default=0) / 1024,
    'process_hwm_mib': max((x.get('VmHWM', 0) for x in samples), default=0) / 1024,
    'process_swap_peak_kib': max((x.get('VmSwap', 0) for x in samples), default=0),
    'sampled_anon_peak_mib': max((x.get('RssAnon', 0) for x in samples), default=0) / 1024,
    'sampled_file_peak_mib': max((x.get('RssFile', 0) for x in samples), default=0) / 1024,
    'sampled_gpu_peak_mib': max((x.get('gpu_mib', 0) for x in samples), default=0),
    'system_swapout_pages_delta': swapout() - startswap,
    'loadavg': os.getloadavg(), 'preliminary': True,
    'concurrent_download': not Path('/home/end0/AI/LocalAI/Models/Qwen3.8-Flash-Next-GGUF/download-verification.json').exists(),
    'controlled_quiet_run': False}
(root / 'results' / (name + '-summary.json')).write_text(json.dumps(summary, indent=2))
print(json.dumps({k: v for k, v in summary.items() if k != 'tokens'}, indent=2))
