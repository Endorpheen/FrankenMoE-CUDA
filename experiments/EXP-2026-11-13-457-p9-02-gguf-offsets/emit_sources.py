from pathlib import Path
import json, hashlib, re, shutil, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-457-p9-02-gguf-offsets'
S = E / 'source'
B = F / 'runtime/exp457/build'
B.mkdir(parents=True, exist_ok=True)
pin = json.loads((F / 'runtime/compiler/current.json').read_text())
C = Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest() == pin['sha256'], 'compiler pin mismatch'

q = subprocess.run([str(C), '-o', 'offsets-generated.c', str((S / 'offsets.goose').resolve())],
                   capture_output=True, text=True, cwd=str(B))
assert q.returncode == 0, q.stderr
shutil.copy(B / 'offsets-generated.c', S / 'offsets-generated.c')
g = (S / 'offsets-generated.c').read_text()
qual = re.sub(r'\b(gg_)', 'fg457_', g)
qual = qual.replace('int main(', 'int fg457_generated_main(')
(S / 'offsets-qualified.c').write_text(qual)

protos = [(ret, name, [t.split()[0] for t in params.split(',')][:-1])
          for ret, name, params in re.findall(r'^static (\w+) (gg_function_\d+)\(([^)]*)\);', g, re.M)]


def pick(pattern):
    hit = [n for r, n, ps in protos if r == 'int64_t' and ps == pattern]
    assert len(hit) == 1, (pattern, protos)
    return hit[0]


up = pick(['int64_t', 'int64_t'])
ok = pick(['int64_t'])
nb = pick(['int64_t', 'int64_t', 'int64_t', 'uint64_t'])
lay = pick(['gg_slice', 'gg_slice', 'int64_t', 'int64_t', 'gg_slice'])
(S / 'offsets-unit.c').write_text(
    '#include <stdint.h>\n'
    '#include "offsets-qualified.c"\n'
    f'int64_t fg457_up(int64_t x,int64_t n){{return fg457_{up[3:]}(x,n,0);}}\n'
    f'int64_t fg457_ok(int64_t a){{return fg457_{ok[3:]}(a,0);}}\n'
    f'int64_t fg457_nbytes(int64_t t,int64_t b,int64_t ne,int64_t *out){{return fg457_{nb[3:]}(t,b,ne,(uint64_t)(uintptr_t)out,0);}}\n'
    'int64_t fg457_layout(const int64_t *rel,const int64_t *nbv,int64_t n,int64_t align,int64_t data_start,int64_t *out){\n'
    '  fg457_slice r; r.data=(void*)rel; r.len=n;\n'
    '  fg457_slice b; b.data=(void*)nbv; b.len=n;\n'
    f'  fg457_slice o; o.data=(void*)out; o.len=n+1;\n'
    f'  return fg457_{lay[3:]}(r,b,align,data_start,o,0);}}\n')
(E / 'emission.json').write_text(json.dumps(dict(
    status='PASS', compiler=pin, entries=dict(up=up, ok=ok, nbytes=nb, layout=lay),
    generated_sha256=hashlib.sha256(g.encode()).hexdigest()), indent=2) + '\n')
print('PASS gguf offsets emission')
