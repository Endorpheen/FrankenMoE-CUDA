from pathlib import Path
import hashlib
import json
import math
import os
import shutil
import struct
import subprocess
import sys

r = Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
s = r / 'source/cpu-backend'
b = r / 'variants/cpu-backend/build'
results = r / 'results'
sys.path.insert(0, '/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/gguf-py')
from gguf import GGUFReader

model = Path('/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf')
reader = GGUFReader(model)
tensor = next(t for t in reader.tensors if t.name == 'token_embd.weight')
width, rows = map(int, tensor.shape)
assert width == 2048 and tensor.tensor_type.name == 'Q4_K'
ids = set(json.loads((results / 'plan-apex-native-32k128-request.json').read_text())['prompt'])
for arm in ['plan-apex-native-32k128', 'b1-diagnostic-254-apex-32k128']:
    ids.update(json.loads((results / (arm + '-response.json')).read_text())['tokens'])
ids = sorted(ids)
assert all(0 <= i < rows for i in ids)
rowbytes = int(tensor.n_bytes) // rows
fixture = b / 'apex-embedding-selected-rows.local.bin'
with model.open('rb') as source, fixture.open('wb') as dest:
    dest.write(struct.pack('<III', int(tensor.tensor_type), width, len(ids)))
    for i in ids:
        source.seek(int(tensor.data_offset) + i*rowbytes)
        data = source.read(rowbytes)
        assert len(data) == rowbytes
        dest.write(data)
shutil.copy2('/tmp/fg143_apex_contract.c', s / 'apex-contract-fixture.c')
command = ['/usr/bin/gcc-13', '-O2', '-I'+str(r/'source/ggml/include'),
           str(s/'apex-contract-fixture.c'), '-L'+str(r/'variants/cpu-backend/run'),
           '-lggml-cpu', '-lggml-base', '-lm', '-lpthread', '-o', str(b/'apex-contract-fixture')]
compiled = subprocess.run(command, capture_output=True, text=True)
(b/'apex-contract-compile.log').write_text(compiled.stdout+compiled.stderr)
assert compiled.returncode == 0, compiled.stderr[-1500:]
metadata = {}
raw = {}
runs = {}
for arm, bundle in [('native', r/'run'), ('candidate', r/'variants/cpu-backend/run')]:
    env = dict(os.environ)
    env.pop('LD_PRELOAD', None)
    for key in list(env):
        if key.startswith('GGML_GOOSE_'):
            env[key] = '0'
    env.update(LD_LIBRARY_PATH=str(bundle), GGML_GOOSE_CPU_BACKEND='1', GGML_GOOSE_CPU_EXECUTOR='1',
               GGML_GOOSE_ALLOC='0', GGML_GOOSE_COALESCE='0', GGML_GOOSE_LIFETIME='0', GGML_GOOSE_GRAPH_PLAN='0')
    output = results / ('cpu-backend-apex-contract-'+arm+'.bin')
    p = subprocess.run([str(b/'apex-contract-fixture'), str(fixture), str(output)], env=env,
                       capture_output=True, text=True, timeout=180)
    (results/('cpu-backend-apex-contract-'+arm+'.jsonl')).write_text(p.stdout)
    (results/('cpu-backend-apex-contract-'+arm+'.log')).write_text(p.stderr)
    runs[arm] = {'exit_code': p.returncode, 'cases': len(p.stdout.splitlines())}
    print(arm, runs[arm], p.stderr[-700:], flush=True)
    if p.returncode == 0:
        metadata[arm] = [json.loads(line) for line in p.stdout.splitlines()]
        raw[arm] = output.read_bytes()
checks = []
if len(raw) == 2:
    assert metadata['native'] == metadata['candidate']
    offset = 0
    for row in metadata['native']:
        n = row['bytes']
        a = raw['native'][offset:offset+n]
        c = raw['candidate'][offset:offset+n]
        errors = []
        for i, (x, y) in enumerate(zip(struct.unpack('<'+str(n//4)+'f', a), struct.unpack('<'+str(n//4)+'f', c))):
            if x == y or math.isnan(x) and math.isnan(y):
                continue
            error = abs(x-y)
            if not math.isfinite(error) or error > 2e-5+2e-5*abs(x):
                errors.append({'index': i, 'native': x, 'candidate': y, 'error': error})
        checks.append({**row, 'exact': a == c, 'violations': len(errors), 'examples': errors[:4]})
        offset += n
    assert offset == len(raw['native']) == len(raw['candidate'])
report = {'status': ('PASS_SELECTED_APEX_CONTRACTS_NOT_FULL_MODEL_EQUIVALENCE'
                     if len(raw)==2 and all(not c['violations'] for c in checks) else 'FAIL_APEX_FUNCTIONAL_CONTRACT'),
          'runs': runs, 'checks': checks, 'embedding_rows': len(ids),
          'input_sha256': hashlib.sha256(fixture.read_bytes()).hexdigest(),
          'scope': 'Actual Apex embedding rows selected by preserved prompt/responses plus wide-range2048-column GLU layouts,1/8/16threads. Functional outputs only; no operation timings or model reruns.',
          'tolerance': 'Unchanged abs2e-5+rel2e-5; exact equality also recorded',
          'whole_model_equivalence': 'OPEN: no repeated Apex inference in this gate',
          'model_weight_bytes_local_only': True}
(results/'cpu-backend-apex-contract-summary.json').write_text(json.dumps(report, indent=2)+'\n')
(s/'apex-contract-command.json').write_text(json.dumps(command, indent=2)+'\n')
print(json.dumps({'status': report['status'], 'cases': len(checks),
                  'exact': sum(c['exact'] for c in checks),
                  'failed': [c['name'] for c in checks if c['violations']]}, indent=2), flush=True)
