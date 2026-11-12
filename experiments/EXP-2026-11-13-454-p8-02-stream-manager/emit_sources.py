from pathlib import Path
import json, hashlib, re, shutil, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-454-p8-02-stream-manager'
S = E / 'source'
B = F / 'runtime/exp454/build'
B.mkdir(parents=True, exist_ok=True)
pin = json.loads((F / 'runtime/compiler/current.json').read_text())
C = Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest() == pin['sha256'], 'compiler pin mismatch'

q = subprocess.run([str(C), '-o', 'manager-generated.c', str((S / 'manager.goose').resolve())],
                   capture_output=True, text=True, cwd=str(B))
assert q.returncode == 0, q.stderr
shutil.copy(B / 'manager-generated.c', S / 'manager-generated.c')
g = (S / 'manager-generated.c').read_text()
qual = re.sub(r'\b(gg_)', 'fg454_', g)
qual = qual.replace('int main(', 'int fg454_generated_main(')
(S / 'manager-qualified.c').write_text(qual)

protos = [(ret, name, [t.split()[0] for t in params.split(',')][:-1])
          for ret, name, params in re.findall(r'^static (\w+) (gg_function_\d+)\(([^)]*)\);', g, re.M)]


# main calls the four decision functions in declaration order, which is the only
# way to tell retire_plan from gc_cutoff: both take (i64, i64, out).
main_body = g[g.rfind('static void gg_function_0('):]
called = []
for name in re.findall(r'\b(gg_function_\d+)\(', main_body):
    if name != 'gg_function_0' and name not in called:
        called.append(name)
assert len(called) == 4, called
retire, stop, cutoff, gc = called
(S / 'manager-unit.c').write_text(
    '#include <stdint.h>\n'
    '#include "manager-qualified.c"\n'
    f'int64_t fg454_retire(int64_t op,int64_t found,uint64_t out){{return fg454_{retire[3:]}(op,found,out,0);}}\n'
    f'int64_t fg454_stop(uint64_t out){{return fg454_{stop[3:]}(out,0);}}\n'
    f'int64_t fg454_cutoff(int64_t now,int64_t ttl,uint64_t out){{return fg454_{cutoff[3:]}(now,ttl,out,0);}}\n'
    f'int64_t fg454_gc(int64_t completed,int64_t now,int64_t ttl,uint64_t out){{return fg454_{gc[3:]}(completed,now,ttl,out,0);}}\n')
(E / 'emission.json').write_text(json.dumps(dict(
    status='PASS', compiler=pin,
    entries=dict(retire=retire, stop=stop, cutoff=cutoff, gc=gc),
    generated_sha256=hashlib.sha256(g.encode()).hexdigest()), indent=2) + '\n')
print('PASS stream manager emission')
