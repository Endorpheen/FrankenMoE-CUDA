from pathlib import Path
import json, hashlib, re, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-445-p8-01-startup-policy'
S = E / 'source'
pin = json.loads((F / 'runtime/compiler/current.json').read_text())
C = Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest() == pin['sha256'], 'compiler pin mismatch'

out = S / 'startup-generated.c'
q = subprocess.run([str(C), '-o', str(out), str(S / 'startup.goose')], capture_output=True, text=True)
assert q.returncode == 0, q.stderr
g = out.read_text()
qual = re.sub(r'\b(gg_)', 'fg445_', g)
qual = qual.replace('int main(', 'int fg445_generated_main(')
(S / 'startup-qualified.c').write_text(qual)

main = g[g.rfind('static void gg_function_0('):]
entries = [x for x in re.findall(r'\b(gg_function_\d+)\(', main) if x != 'gg_function_0']
assert len(entries) == 1, entries
(S / 'startup-unit.c').write_text(
    '#include <stdint.h>\n'
    '#include "startup-qualified.c"\n'
    'int64_t fg445_decide(int64_t a,int64_t b,int64_t c,int64_t d,int64_t e,int64_t f,\n'
    '                     int64_t g,int64_t h,int64_t i,int64_t j,int64_t k,int64_t l,\n'
    '                     int64_t m,uint64_t n){return fg445_' + entries[0][3:] +
    '(a,b,c,d,e,f,g,h,i,j,k,l,m,n,0);}\n')
(E / 'emission.json').write_text(json.dumps(dict(
    status='PASS', compiler=pin, entries=entries,
    generated_sha256=hashlib.sha256(g.encode()).hexdigest()), indent=2) + '\n')
print('PASS startup policy emission')
