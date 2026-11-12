from pathlib import Path
import json, hashlib, re, shutil, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-452-p8-02-multipart'
S = E / 'source'
B = F / 'runtime/exp452/build'
B.mkdir(parents=True, exist_ok=True)
pin = json.loads((F / 'runtime/compiler/current.json').read_text())
C = Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest() == pin['sha256'], 'compiler pin mismatch'

q = subprocess.run([str(C), '-o', 'multipart-generated.c', str((S / 'multipart.goose').resolve())],
                   capture_output=True, text=True, cwd=str(B))
assert q.returncode == 0, q.stderr
shutil.copy(B / 'multipart-generated.c', S / 'multipart-generated.c')
g = (S / 'multipart-generated.c').read_text()
qual = re.sub(r'\b(gg_)', 'fg452_', g)
qual = qual.replace('int main(', 'int fg452_generated_main(')
(S / 'multipart-qualified.c').write_text(qual)

protos = [(ret, name, [t.split()[0] for t in params.split(',')][:-1])
          for ret, name, params in re.findall(r'^static (\w+) (gg_function_\d+)\(([^)]*)\);', g, re.M)]


def pick(pattern):
    hit = [n for r, n, ps in protos if r == 'int64_t' and ps == pattern]
    assert len(hit) == 1, (pattern, protos)
    return hit[0]


dec = pick(['uint64_t', 'int64_t', 'uint64_t', 'int64_t'])
act = pick(['int64_t', 'gg_slice', 'gg_slice', 'gg_slice', 'gg_slice'])
src = pick(['int64_t', 'int64_t'])
(S / 'multipart-unit.c').write_text(
    '#include <stdint.h>\n'
    '#include "multipart-qualified.c"\n'
    f'int64_t fg452_decode(uint64_t a,int64_t b,uint64_t c,int64_t d){{return fg452_{dec[3:]}(a,b,c,d,0);}}\n'
    'int64_t fg452_actions(int64_t n,const char *keys,int64_t klen,const int64_t *koffs,'
    'const int64_t *klens,int64_t *out){\n'
    '  fg452_slice s; s.data=(void*)keys; s.len=klen;\n'
    '  fg452_slice so; so.data=(void*)koffs; so.len=n;\n'
    '  fg452_slice sl; sl.data=(void*)klens; sl.len=n;\n'
    '  fg452_slice sr; sr.data=(void*)out;   sr.len=n;\n'
    f'  return fg452_{act[3:]}(n,s,so,sl,sr,0);}}\n'
    f'int64_t fg452_source(int64_t a,int64_t b){{return fg452_{src[3:]}(a,b,0);}}\n')
(E / 'emission.json').write_text(json.dumps(dict(
    status='PASS', compiler=pin, entries=dict(decode=dec, actions=act, source=src),
    generated_sha256=hashlib.sha256(g.encode()).hexdigest()), indent=2) + '\n')
print('PASS multipart emission')
