from pathlib import Path
import json, hashlib, re, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-421-frankenmoe-logging'
S = E / 'source'
pin = json.loads((F / 'runtime/compiler/current.json').read_text())
C = Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest() == pin['sha256'], 'compiler pin mismatch'

out = S / 'logfmt-generated.c'
q = subprocess.run([str(C), '-o', str(out), str(S / 'logfmt.goose')],
                   capture_output=True, text=True)
assert q.returncode == 0, q.stderr
g = out.read_text()
qual = re.sub(r'\bgg_', 'fglog_', g)
qual = qual.replace('int main(', 'int fglog_generated_main(')
(S / 'logfmt-qualified.c').write_text(qual)
(S / 'logfmt-unit.c').write_text(
    '#include <stdint.h>\n'
    '#include "logfmt-qualified.c"\n'
    'int64_t fglog_run(uint64_t out,int64_t cap,uint64_t evout,int64_t level,int64_t mode,\n'
    '                  int64_t last,uint64_t text,int64_t len,uint64_t ts,int64_t tslen){\n'
    '    return fglog_function_1(out,cap,evout,level,mode,last,text,len,ts,tslen,0);\n'
    '}\n')
(E / 'emission.json').write_text(json.dumps({
    'status': 'PASS', 'compiler': pin,
    'module': 'logfmt', 'generated_sha256': hashlib.sha256(g.encode()).hexdigest(),
}, indent=2) + '\n')
print('PASS logfmt emission')
