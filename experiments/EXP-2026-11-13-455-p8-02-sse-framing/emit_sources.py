from pathlib import Path
import json, hashlib, re, shutil, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-455-p8-02-sse-framing'
S = E / 'source'
B = F / 'runtime/exp455/build'
B.mkdir(parents=True, exist_ok=True)
pin = json.loads((F / 'runtime/compiler/current.json').read_text())
C = Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest() == pin['sha256'], 'compiler pin mismatch'

q = subprocess.run([str(C), '-o', 'sse-generated.c', str((S / 'sse.goose').resolve())],
                   capture_output=True, text=True, cwd=str(B))
assert q.returncode == 0, q.stderr
shutil.copy(B / 'sse-generated.c', S / 'sse-generated.c')
g = (S / 'sse-generated.c').read_text()
qual = re.sub(r'\b(gg_)', 'fg455_', g)
qual = qual.replace('int main(', 'int fg455_generated_main(')
(S / 'sse-qualified.c').write_text(qual)

protos = [(ret, name, [t.split()[0] for t in params.split(',')][:-1])
          for ret, name, params in re.findall(r'^static (\w+) (gg_function_\d+)\(([^)]*)\);', g, re.M)]
frame = [n for r, n, ps in protos if r == 'int64_t' and ps == ['int64_t', 'int64_t', 'uint64_t']]
size = [n for r, n, ps in protos if r == 'int64_t' and ps == ['gg_slice', 'int64_t', 'int64_t', 'uint64_t']]
assert len(frame) == 1 and len(size) == 1, protos
(S / 'sse-unit.c').write_text(
    '#include <stdint.h>\n'
    '#include "sse-qualified.c"\n'
    f'int64_t fg455_frame(int64_t flavor,int64_t has_event,int64_t *out){{'
    f' return fg455_{frame[0][3:]}(flavor,has_event,(uint64_t)(uintptr_t)out,0);}}\n'
    f'int64_t fg455_size(const int64_t *steps,int64_t n,int64_t payload,int64_t event,uint64_t out){{'
    f' fg455_slice s; s.data=(void*)steps; s.len=n;'
    f' return fg455_{size[0][3:]}(s,payload,event,out,0);}}\n')
(E / 'emission.json').write_text(json.dumps(dict(
    status='PASS', compiler=pin, entries=dict(frame=frame, size=size),
    generated_sha256=hashlib.sha256(g.encode()).hexdigest()), indent=2) + '\n')
print('PASS sse framing emission')
