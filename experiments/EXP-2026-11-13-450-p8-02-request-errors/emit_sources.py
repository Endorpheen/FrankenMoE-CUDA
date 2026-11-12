from pathlib import Path
import json, hashlib, re, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-450-p8-02-request-errors'
S = E / 'source'
pin = json.loads((F / 'runtime/compiler/current.json').read_text())
C = Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest() == pin['sha256'], 'compiler pin mismatch'

out = S / 'errors-generated.c'
q = subprocess.run([str(C), '-o', str(out), str(S / 'errors.goose')], capture_output=True, text=True)
assert q.returncode == 0, q.stderr
g = out.read_text()
qual = re.sub(r'\b(gg_)', 'fg450_', g)
qual = qual.replace('int main(', 'int fg450_generated_main(')
(S / 'errors-qualified.c').write_text(qual)

body = g[g.rfind('static void gg_function_0('):]
called = []
for name in re.findall(r'\b(gg_function_\d+)\(', body):
    if name != 'gg_function_0' and name not in called:
        called.append(name)
assert len(called) == 3, called

(S / 'errors-unit.c').write_text(
    '#include <stdint.h>\n'
    '#include "errors-qualified.c"\n'
    f'int64_t fg450_classify(int64_t a,uint64_t b){{return fg450_{called[0][3:]}(a,b,0);}}\n'
    f'int64_t fg450_code(int64_t a,uint64_t b){{return fg450_{called[1][3:]}(a,b,0);}}\n'
    f'int64_t fg450_status(int64_t a,int64_t c,uint64_t b){{return fg450_{called[2][3:]}(a,c,b,0);}}\n'
    '\n')
(E / 'emission.json').write_text(json.dumps(dict(
    status='PASS', compiler=pin, entries=called,
    generated_sha256=hashlib.sha256(g.encode()).hexdigest()), indent=2) + '\n')
print('PASS request error policy emission')
