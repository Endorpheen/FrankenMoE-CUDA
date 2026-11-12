from pathlib import Path
import json, hashlib, re, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-449-p8-02-route-table'
S = E / 'source'
pin = json.loads((F / 'runtime/compiler/current.json').read_text())
C = Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest() == pin['sha256'], 'compiler pin mismatch'

out = S / 'routes-generated.c'
build = F / 'runtime/exp449/build'
build.mkdir(parents=True, exist_ok=True)
link = build / 'stdlib'
if not link.exists():
    link.symlink_to('/home/end0/AI/Fried-Goose/stdlib')
q = subprocess.run([str(C), '-o', str(out), str((S / 'routes.goose').resolve())],
                   capture_output=True, text=True, cwd=str(build))
assert q.returncode == 0, q.stderr
g = out.read_text()
qual = re.sub(r'\b(gg_)', 'fg449_', g)
qual = qual.replace('int main(', 'int fg449_generated_main(')
(S / 'routes-qualified.c').write_text(qual)

protos = dict((name, (ret, params)) for ret, name, params in
              re.findall(r'^static (\w+) (gg_function_\d+)\(([^)]*)\);', g, re.M))

parsed = [(ret, name, [t.split()[0] for t in params.split(',')][:-1])
          for name, (ret, params) in protos.items()]
route_id = [n for r, n, ps in parsed if r == 'int64_t' and ps == ['int64_t', 'gg_slice']]
requires_key = [n for r, n, ps in parsed if r == 'int64_t' and ps == ['int64_t']]
assert len(route_id) == 1 and len(requires_key) == 1, parsed

(S / 'routes-unit.c').write_text(
    '#include <stdint.h>\n'
    '#include "routes-qualified.c"\n'
    f'int64_t fg449_route_id(int64_t m,const char *p,int64_t n){{fg449_slice s; s.data=(void*)p; s.len=n;'
    f' return fg449_{route_id[0][3:]}(m,s,0);}}\n'
    f'int64_t fg449_requires_key(int64_t a){{return fg449_{requires_key[0][3:]}(a,0);}}\n')
(E / 'emission.json').write_text(json.dumps(dict(
    status='PASS', compiler=pin,
    entries=dict(route_id=route_id, requires_key=requires_key),
    generated_sha256=hashlib.sha256(g.encode()).hexdigest()), indent=2) + '\n')
print('PASS route table emission')
