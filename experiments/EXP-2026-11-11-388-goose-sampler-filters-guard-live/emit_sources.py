from pathlib import Path
import re,json,hashlib,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E=F/'experiments/EXP-2026-11-11-388-goose-sampler-filters-guard-live'
S=E/'source'
pin=json.loads((F/'runtime/compiler/current.json').read_text())
C=Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest()==pin['sha256'],'compiler pin mismatch'
q=subprocess.run([str(C),'-o',str(S/'host-generated.c'),str(S/'host.goose')],capture_output=True,text=True)
assert q.returncode==0,q.stderr
g=(S/'host-generated.c').read_text()
prefix='fg388ds_'
qual=lambda s:re.sub(r'\b(gg_)',lambda m:prefix+m[1],s)
main=g[g.rfind('static void gg_function_0('):]
entries=[x for x in re.findall(r'\b(gg_function_\d+)\(',main) if x!='gg_function_0']
assert len(entries)==1,entries
(S/'host-ffi.h').write_text('#pragma once\n#include <stdint.h>\n#ifdef __cplusplus\nextern "C" {\n#endif\nuint8_t fg388dh_run(int64_t);\n#ifdef __cplusplus\n}\n#endif\n')
(S/'host-qualified.c').write_text(qual(g))
(S/'host-unit.c').write_text('#include "host-ffi.h"\n#define main fg388ds_generated_main\n#include "host-qualified.c"\n#undef main\nuint8_t fg388dh_run(int64_t p){return '+prefix+entries[0]+'(p,1);}\n')
print('PASS sampler-filters-guard-live emission')
