from pathlib import Path
import json, hashlib, re, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-449-p8-02-route-table'
S = E / 'source'
pin = json.loads((F / 'runtime/compiler/current.json').read_text())
C = Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest() == pin['sha256'], 'compiler pin mismatch'

out = S / 'routes-generated.c'
q = subprocess.run([str(C), '-o', str(out), str(S / 'routes.goose')], capture_output=True, text=True)
assert q.returncode == 0, q.stderr
g = out.read_text()
qual = re.sub(r'\b(gg_)', 'fg449_', g)
qual = qual.replace('int main(', 'int fg449_generated_main(')
(S / 'routes-qualified.c').write_text(qual)

protos = dict((name, (ret, params)) for ret, name, params in
              re.findall(r'^static (\w+) (gg_function_\d+)\(([^)]*)\);', g, re.M))

route_id = [n for n, (r, p) in protos.items() if r == 'int64_t' and p.count(',') == 3]
route_key = [n for n, (r, p) in protos.items() if r == 'uint64_t' and p.count(',') == 3]
requires_key = [n for n, (r, p) in protos.items() if r == 'int64_t' and p.count(',') == 1]
assert len(route_id) == 1 and len(route_key) == 1 and len(requires_key) == 1, protos

(S / 'routes-unit.c').write_text(
    '#include <stdint.h>\n'
    '#include "routes-qualified.c"\n'
    f'int64_t fg449_route_id(int64_t a,uint64_t b,int64_t c){{return fg449_{route_id[0][3:]}(a,b,c,0);}}\n'
    f'uint64_t fg449_route_key(int64_t a,uint64_t b,int64_t c){{return fg449_{route_key[0][3:]}(a,b,c,0);}}\n'
    f'int64_t fg449_requires_key(int64_t a){{return fg449_{requires_key[0][3:]}(a,0);}}\n')
(E / 'emission.json').write_text(json.dumps(dict(
    status='PASS', compiler=pin,
    entries=dict(route_id=route_id, route_key=route_key, requires_key=requires_key),
    generated_sha256=hashlib.sha256(g.encode()).hexdigest()), indent=2) + '\n')
print('PASS route table emission')
