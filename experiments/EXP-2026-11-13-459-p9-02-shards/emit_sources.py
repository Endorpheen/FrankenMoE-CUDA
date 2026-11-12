from pathlib import Path
import json, hashlib, re, shutil, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-459-p9-02-shards'
S = E / 'source'
B = F / 'runtime/exp459/build'
B.mkdir(parents=True, exist_ok=True)
pin = json.loads((F / 'runtime/compiler/current.json').read_text())
C = Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest() == pin['sha256'], 'compiler pin mismatch'

q = subprocess.run([str(C), '-o', 'shards-generated.c', str((S / 'shards.goose').resolve())],
                   capture_output=True, text=True, cwd=str(B))
assert q.returncode == 0, q.stderr
shutil.copy(B / 'shards-generated.c', S / 'shards-generated.c')
g = (S / 'shards-generated.c').read_text()
qual = re.sub(r'\b(gg_)', 'fg459_', g)
qual = qual.replace('int main(', 'int fg459_generated_main(')
(S / 'shards-qualified.c').write_text(qual)

main_body = g[g.rfind('static void gg_function_0('):]
# keep duplicates: the compiler shares identical bodies, so split_count_ok,
# shard_index_ok and tensor_count_ok all appear as the same emitted function.
called = [n for n in re.findall(r'\b(gg_function_\d+)\(', main_body) if n != 'gg_function_0']
# main calls digits, width, split_path, first_shard_ok, split_count_ok,
# shard_index_ok, tensor_count_ok, magic_ok, version_ok in that order.
assert len(called) == 9, called
digits, width, split_path, first, count_ok, index_ok, tensor_ok, magic, version = called

(S / 'shards-unit.c').write_text(
    '#include <stdint.h>\n'
    '#include "shards-qualified.c"\n'
    f'int64_t fg459_digits(int64_t v){{return fg459_{digits[3:]}(v,0);}}\n'
    f'int64_t fg459_width(int64_t v){{return fg459_{width[3:]}(v,0);}}\n'
    f'int64_t fg459_split_path(const char *pre,int64_t pn,int64_t no,int64_t count,char *out,int64_t cap){{\n'
    f'  fg459_slice p; p.data=(void*)pre; p.len=pn;\n'
    f'  fg459_slice o; o.data=(void*)out; o.len=cap;\n'
    f'  return fg459_{split_path[3:]}(p,no,count,o,0);}}\n'
    f'int64_t fg459_first(int64_t i){{return fg459_{first[3:]}(i,0);}}\n'
    f'int64_t fg459_count_ok(int64_t a,int64_t b){{return fg459_{count_ok[3:]}(a,b,0);}}\n'
    f'int64_t fg459_index_ok(int64_t a,int64_t b){{return fg459_{index_ok[3:]}(a,b,0);}}\n'
    f'int64_t fg459_tensor_ok(int64_t a,int64_t b){{return fg459_{tensor_ok[3:]}(a,b,0);}}\n'
    f'int64_t fg459_magic(int64_t v){{return fg459_{magic[3:]}(v,0);}}\n'
    f'int64_t fg459_version(int64_t v){{return fg459_{version[3:]}(v,0);}}\n')
(E / 'emission.json').write_text(json.dumps(dict(
    status='PASS', compiler=pin, entries=called,
    generated_sha256=hashlib.sha256(g.encode()).hexdigest()), indent=2) + '\n')
print('PASS shards emission')
