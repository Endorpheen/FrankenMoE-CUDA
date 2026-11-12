from pathlib import Path
import re, json, hashlib, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-263-goose-moe-cache-copy-decisions'
S = E / 'source'
pin = json.loads((F / 'runtime/compiler/current.json').read_text())
C = Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest() == pin['sha256'], 'compiler pin mismatch'

NAMES = ['run_from_slot', 'run_from_fills', 'stats_class', 'fill_bytes']


def body(g, m):
    at = m.end(); depth = 1
    while depth:
        depth += (g[at] == '{') - (g[at] == '}')
        at += 1
    return g[m.start():at]


out = S / 'copy_decisions-generated.c'
q = subprocess.run([str(C), '-o', str(out), str(S / 'copy_decisions.goose')], capture_output=True, text=True)
assert q.returncode == 0, q.stderr
g = out.read_text()
prefix = 'fg263c_'


def qual(s):
    return re.sub(r'\b(gg_)', lambda m: prefix + m[1], s)


main = g[g.rfind('static void gg_function_0('):]
called = re.findall(r'\b(gg_function_\d+)\(', main)
called = [x for x in called if x != 'gg_function_0']
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
    wrappers.append(f'static int64_t {prefix}{name}({wparams}){{return {prefix}{fn}({call});}}')

(S / 'copy_decisions-functions.c').write_text(text + '\n' + '\n'.join(wrappers) + '\n')
(E / 'emission.json').write_text(json.dumps(dict(
    status='PASS_CURRENT_SELFHOSTED_SOURCE_AND_MECHANICAL_EXTRACTION', compiler=pin,
    modules=[dict(module='copy_decisions', entries=called,
                  generated_c_sha256=hashlib.sha256(g.encode()).hexdigest())]), indent=2) + '\n')
print('PASS copy-decisions emission, entries:', called)
