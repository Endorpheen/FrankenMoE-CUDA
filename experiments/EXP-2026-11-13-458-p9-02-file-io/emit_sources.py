from pathlib import Path
import json, hashlib, re, shutil, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-458-p9-02-file-io'
S = E / 'source'
B = F / 'runtime/exp458/build'
B.mkdir(parents=True, exist_ok=True)
pin = json.loads((F / 'runtime/compiler/current.json').read_text())
C = Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest() == pin['sha256'], 'compiler pin mismatch'

q = subprocess.run([str(C), '-o', 'io-generated.c', str((S / 'io.goose').resolve())],
                   capture_output=True, text=True, cwd=str(B))
assert q.returncode == 0, q.stderr
shutil.copy(B / 'io-generated.c', S / 'io-generated.c')
g = (S / 'io-generated.c').read_text()
qual = re.sub(r'\b(gg_)', 'fg458_', g)
qual = qual.replace('int main(', 'int fg458_generated_main(')
(S / 'io-qualified.c').write_text(qual)

# main calls fits, array_check, string_check, seek, read_raw, file_remain in that
# order; several pairs share a signature, so the call order is the discriminator.
main_body = g[g.rfind('static void gg_function_0('):]
called = []
for name in re.findall(r'\b(gg_function_\d+)\(', main_body):
    if name != 'gg_function_0' and name not in called:
        called.append(name)
assert len(called) == 6, called
fits, arr, strchk, seek, raw, frem = called

(S / 'io-unit.c').write_text(
    '#include <stdint.h>\n'
    '#include "io-qualified.c"\n'
    '// const declarations become module globals; initialise them before use\n'
    '__attribute__((constructor)) static void fg458_boot(void){ fg458_init_globals(0, 0); }\n'
    f'int64_t fg458_fits(int64_t s,int64_t r){{return fg458_{fits[3:]}(s,r,0);}}\n'
    f'int64_t fg458_arr(int64_t n,int64_t e,int64_t r,int64_t *out){{return fg458_{arr[3:]}(n,e,r,(uint64_t)(uintptr_t)out,0);}}\n'
    f'int64_t fg458_str(int64_t s,int64_t r,int64_t *out){{return fg458_{strchk[3:]}(s,r,(uint64_t)(uintptr_t)out,0);}}\n'
    f'int64_t fg458_seek(int64_t a,int64_t o,int64_t r,int64_t *out){{return fg458_{seek[3:]}(a,o,r,(uint64_t)(uintptr_t)out,0);}}\n'
    f'int64_t fg458_raw(int64_t s,int64_t c,int64_t o,int64_t r,int64_t end,int64_t *out){{'
    f' return fg458_{raw[3:]}(s,c,o,r,end,(uint64_t)(uintptr_t)out,0);}}\n'
    f'int64_t fg458_fremain(int64_t end,int64_t cur,int64_t *out){{return fg458_{frem[3:]}(end,cur,(uint64_t)(uintptr_t)out,0);}}\n')
(E / 'emission.json').write_text(json.dumps(dict(
    status='PASS', compiler=pin, entries=dict(fits=fits, arr=arr, str=strchk, seek=seek, raw=raw, frem=frem),
    generated_sha256=hashlib.sha256(g.encode()).hexdigest()), indent=2) + '\n')
print('PASS file io emission')
