from pathlib import Path
import json, hashlib, re, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-452-p8-02-multipart'
S = E / 'source'
pin = json.loads((F / 'runtime/compiler/current.json').read_text())
C = Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest() == pin['sha256'], 'compiler pin mismatch'

out = S / 'multipart-generated.c'
q = subprocess.run([str(C), '-o', str(out), str(S / 'multipart.goose')], capture_output=True, text=True)
assert q.returncode == 0, q.stderr
g = out.read_text()
qual = re.sub(r'\b(gg_)', 'fg452_', g)
qual = qual.replace('int main(', 'int fg452_generated_main(')
(S / 'multipart-qualified.c').write_text(qual)

protos = [(ret, name, [t.strip() for t in params.split(',')])
          for ret, name, params in re.findall(r'^static (\w+) (gg_function_\d+)\(([^)]*)\);', g, re.M)]
protos = [(r, n, [t.split()[0] for t in ps if t.strip() != 'int64_t base']) for r, n, ps in protos]


def pick(pattern):
    hit = [n for r, n, ps in protos if r == 'int64_t' and ps == pattern]
    assert len(hit) == 1, (pattern, protos)
    return hit[0]


dec = pick(['uint64_t', 'int64_t', 'uint64_t', 'int64_t'])
act = pick(['int64_t', 'uint64_t', 'uint64_t', 'uint64_t', 'uint64_t'])
src = pick(['int64_t', 'int64_t'])
(S / 'multipart-unit.c').write_text(
    '#include <stdint.h>\n'
    '#include "multipart-qualified.c"\n'
    f'int64_t fg452_decode(uint64_t a,int64_t b,uint64_t c,int64_t d){{return fg452_{dec[3:]}(a,b,c,d,0);}}\n'
    f'int64_t fg452_actions(int64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e){{return fg452_{act[3:]}(a,b,c,d,e,0);}}\n'
    f'int64_t fg452_source(int64_t a,int64_t b){{return fg452_{src[3:]}(a,b,0);}}\n')
(E / 'emission.json').write_text(json.dumps(dict(
    status='PASS', compiler=pin, entries=dict(decode=dec, actions=act, source=src),
    generated_sha256=hashlib.sha256(g.encode()).hexdigest()), indent=2) + '\n')
print('PASS multipart emission')
