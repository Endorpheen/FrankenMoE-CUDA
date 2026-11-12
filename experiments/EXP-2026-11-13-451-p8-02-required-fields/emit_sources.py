from pathlib import Path
import json, hashlib, re, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-451-p8-02-required-fields'
S = E / 'source'
pin = json.loads((F / 'runtime/compiler/current.json').read_text())
C = Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest() == pin['sha256'], 'compiler pin mismatch'

out = S / 'required-generated.c'
q = subprocess.run([str(C), '-o', str(out), str(S / 'required.goose')], capture_output=True, text=True)
assert q.returncode == 0, q.stderr
g = out.read_text()
qual = re.sub(r'\b(gg_)', 'fg451_', g)
qual = qual.replace('int main(', 'int fg451_generated_main(')
(S / 'required-qualified.c').write_text(qual)

# main calls missing(cat, mask, out): the only entry with (i64, i64, u64).
protos = dict((name, (ret, params)) for ret, name, params in
              re.findall(r'^static (\w+) (gg_function_\d+)\(([^)]*)\);', g, re.M))
entry = [n for n, (r, p) in protos.items() if r == 'int64_t' and p.count(',') == 3]
assert len(entry) == 1, protos
(S / 'required-unit.c').write_text(
    '#include <stdint.h>\n'
    '#include "required-qualified.c"\n'
    f'int64_t fg451_missing(int64_t a,int64_t b,uint64_t c){{return fg451_{entry[0][3:]}(a,b,c,0);}}\n')
(E / 'emission.json').write_text(json.dumps(dict(
    status='PASS', compiler=pin, entries=entry,
    generated_sha256=hashlib.sha256(g.encode()).hexdigest()), indent=2) + '\n')
print('PASS required-field emission')
