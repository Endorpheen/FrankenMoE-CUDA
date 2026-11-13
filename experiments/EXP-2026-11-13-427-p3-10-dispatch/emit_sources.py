from pathlib import Path
import json, hashlib, re, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-427-p3-10-dispatch'
S = E / 'source'
pin = json.loads((F / 'runtime/compiler/current.json').read_text())
C = Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest() == pin['sha256'], 'compiler pin mismatch'

out = S / 'dispatch-generated.c'
q = subprocess.run([str(C), '-o', str(out), str(S / 'dispatch.goose')], capture_output=True, text=True)
assert q.returncode == 0, q.stderr
g = out.read_text()
qual = re.sub(r'\bgg_', 'fg427_', g)
qual = qual.replace('int main(', 'int fg427_generated_main(')
(S / 'dispatch-qualified.c').write_text(qual)


def wrapper(name, fn, n):
    args = ','.join(f'a[{i}]' for i in range(n))
    return f'int {name}(const int64_t *a){{return {fn}({args},0);}}\n'


(S / 'dispatch-unit.c').write_text(
    '#include <stdint.h>\n#include "dispatch-qualified.c"\n' +
    wrapper('fg427_mmf', 'fg427_function_1', 26) +
    wrapper('fg427_mmvf', 'fg427_function_2', 31))
(E / 'emission.json').write_text(json.dumps({
    'status': 'PASS', 'compiler': pin,
    'generated_sha256': hashlib.sha256(g.encode()).hexdigest(),
}, indent=2) + '\n')
print('PASS dispatch emission')
