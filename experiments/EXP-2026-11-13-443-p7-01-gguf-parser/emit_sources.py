from pathlib import Path
import json, hashlib, re, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-443-p7-01-gguf-parser'
S = E / 'source'
pin = json.loads((F / 'runtime/compiler/current.json').read_text())
C = Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest() == pin['sha256'], 'compiler pin mismatch'

out = S / 'gguf-generated.c'
q = subprocess.run([str(C), '-o', str(out), str(S / 'gguf.goose')], capture_output=True, text=True)
assert q.returncode == 0, q.stderr
g = out.read_text()
qual = re.sub(r'\b(gg_)', 'fg443_', g)
qual = qual.replace('int main(', 'int fg443_generated_main(')
(S / 'gguf-qualified.c').write_text(qual)

entries = [x for x in re.findall(r'\b(gg_function_\d+)\(', g[g.rfind('static void gg_function_0('):]) if x != 'gg_function_0']
assert len(entries) >= 1, entries
(S / 'gguf-unit.c').write_text(
    '#include <stdint.h>\n#include "gguf-qualified.c"\n'
    'int64_t fg443_parse(uint64_t buf,int64_t len,uint64_t out,int64_t cap){return fg443_' + entries[0][3:] + '(buf,len,out,cap,0);}\n')
(E / 'emission.json').write_text(json.dumps(dict(
    status='PASS', compiler=pin, entries=entries,
    generated_sha256=hashlib.sha256(g.encode()).hexdigest()), indent=2) + '\n')
print('PASS gguf emission')
