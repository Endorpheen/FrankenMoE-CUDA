from pathlib import Path
import json, hashlib, re, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-446-p8-02-prerouting'
S = E / 'source'
pin = json.loads((F / 'runtime/compiler/current.json').read_text())
C = Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest() == pin['sha256'], 'compiler pin mismatch'

out = S / 'prerouting-generated.c'
q = subprocess.run([str(C), '-o', str(out), str(S / 'prerouting.goose')], capture_output=True, text=True)
assert q.returncode == 0, q.stderr
g = out.read_text()
qual = re.sub(r'\b(gg_)', 'fg446_', g)
qual = qual.replace('int main(', 'int fg446_generated_main(')
(S / 'prerouting-qualified.c').write_text(qual)

main = g[g.rfind('static void gg_function_0('):]
entries = [x for x in re.findall(r'\b(gg_function_\d+)\(', main) if x != 'gg_function_0']
assert len(entries) == 1, entries
(S / 'prerouting-unit.c').write_text(
    '#include <stdint.h>\n'
    '#include "prerouting-qualified.c"\n'
    'int64_t fg446_preroute(int64_t a,int64_t b,int64_t c,uint64_t d,int64_t e,int64_t f,\n'
    '                       int64_t g,int64_t h,int64_t i,int64_t j,uint64_t k,int64_t l,\n'
    '                       uint64_t m,int64_t n,uint64_t o,int64_t p,uint64_t q){\n'
    '  return fg446_' + entries[0][3:] + '(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,0);}\n')
(E / 'emission.json').write_text(json.dumps(dict(
    status='PASS', compiler=pin, entries=entries,
    generated_sha256=hashlib.sha256(g.encode()).hexdigest()), indent=2) + '\n')
print('PASS pre-routing emission')
