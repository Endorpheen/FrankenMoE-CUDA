from pathlib import Path
import json, hashlib, re, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-453-p8-02-stream-policy'
S = E / 'source'
pin = json.loads((F / 'runtime/compiler/current.json').read_text())
C = Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest() == pin['sha256'], 'compiler pin mismatch'

out = S / 'stream-generated.c'
q = subprocess.run([str(C), '-o', str(out), str(S / 'stream.goose')], capture_output=True, text=True)
assert q.returncode == 0, q.stderr
g = out.read_text()
qual = re.sub(r'\b(gg_)', 'fg453_', g)
qual = qual.replace('int main(', 'int fg453_generated_main(')
(S / 'stream-qualified.c').write_text(qual)

protos = [(ret, name, [t.split()[0] for t in params.split(',')])
          for ret, name, params in re.findall(r'^static (\w+) (gg_function_\d+)\(([^)]*)\);', g, re.M)]
protos = [(r, n, ps[:-1]) for r, n, ps in protos]


def pick(pattern):
    hit = [n for r, n, ps in protos if r == 'int64_t' and ps == pattern]
    assert len(hit) == 1, (pattern, protos)
    return hit[0]


app = pick(['int64_t'] * 4 + ['uint64_t'])
rd = pick(['int64_t'] * 5 + ['uint64_t'])
(S / 'stream-unit.c').write_text(
    '#include <stdint.h>\n'
    '#include "stream-qualified.c"\n'
    f'int64_t fg453_session_append(int64_t a,int64_t b,int64_t c,int64_t d,uint64_t e){{return fg453_{app[3:]}(a,b,c,d,e,0);}}\n'
    f'int64_t fg453_session_read(int64_t a,int64_t b,int64_t c,int64_t d,int64_t e,uint64_t f){{return fg453_{rd[3:]}(a,b,c,d,e,f,0);}}\n')
(E / 'emission.json').write_text(json.dumps(dict(
    status='PASS', compiler=pin, entries=dict(append=app, read=rd),
    generated_sha256=hashlib.sha256(g.encode()).hexdigest()), indent=2) + '\n')
print('PASS stream policy emission')
