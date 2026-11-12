from pathlib import Path
import re, json, hashlib, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-291-goose-batch-reject'
S = E / 'source'
pin = json.loads((F / 'runtime/compiler/current.json').read_text())
C = Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest() == pin['sha256'], 'compiler pin mismatch'

NAMES = ['check_continuity', 'check_decreasing', 'check_seq_sets']


def body(g, m):
    at = m.end(); depth = 1
    while depth:
        depth += (g[at] == '{') - (g[at] == '}')
        at += 1
    return g[m.start():at]


out = S / 'batch_reject-generated.c'
q = subprocess.run([str(C), '-o', str(out), str(S / 'batch_reject.goose')], capture_output=True, text=True)
assert q.returncode == 0, q.stderr
g = out.read_text()
prefix = 'fg291b_'

def qual(s):
    return re.sub(r'\b(gg_)', lambda m: prefix + m[1], s)

main = g[g.rfind('static void gg_function_0('):]
called = [x for x in re.findall(r'\b(gg_function_\d+)\(', main) if x != 'gg_function_0']
assert len(called) == len(NAMES), called

funcs = ['static void gg_fail(const char*){asm("trap;");}']
for m in re.finditer(r'(?m)^static [^\n{]+?\b(gg_function_\d+|gg_div|gg_mod|gg_udiv|gg_umod|gg_double_bits|gg_index)\([^;{}]*\)\s*\{', g):
    if m[1] == 'gg_function_0':
        continue
    funcs.append(body(g, m))
text = qual('\n'.join(x[:x.index('{')].rstrip() + ';' for x in funcs) + '\n' + '\n'.join(funcs))
text = re.sub(r'\bgg_(?!fail_)', prefix + 'dx_', text)
assert not any(x in text for x in ['gg_get_stack(', 'gg_allocate(', 'gg_append(', 'gg_ref(']), 'stack allocator reachable'

wrappers = []
for name, fn in zip(NAMES, called):
    mdef = re.search(r'static [^\n{]+?\b' + fn + r'\(([^)]*)\)\s*\{', g)
    assert mdef, fn
    params = [p.strip() for p in mdef[1].split(',') if p.strip()]
    names = [p.split()[-1] for p in params]
    wire = [n for n in names if n != 'base']
    call = ', '.join(wire + (['0'] if names[-1] == 'base' else []))
    wparams = ', '.join(p for p in params if p.split()[-1] != 'base')
    wrappers.append(f'int64_t {prefix}{name}({wparams}){{return {prefix}{fn}({call});}}')

(S / 'batch_reject-functions.c').write_text(text + '\n' + '\n'.join(wrappers) + '\n')
(E / 'emission.json').write_text(json.dumps(dict(
    status='PASS_CURRENT_SELFHOSTED_SOURCE_AND_MECHANICAL_EXTRACTION', compiler=pin,
    modules=[dict(module='batch_reject', entries=called,
                  generated_c_sha256=hashlib.sha256(g.encode()).hexdigest())]), indent=2) + '\n')
print('PASS batch-reject emission, entries:', called)
