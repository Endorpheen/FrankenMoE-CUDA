from pathlib import Path
import re, json, hashlib, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-259-goose-mmq-load-q5k'
S = E / 'source'
pin = json.loads((F / 'runtime/compiler/current.json').read_text())
C = Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest() == pin['sha256'], 'compiler pin mismatch'


def body(g, m):
    at = m.end(); depth = 1
    while depth:
        depth += (g[at] == '{') - (g[at] == '}')
        at += 1
    return g[m.start():at]


rows = []
for name, prefix in [('qs5', 'fg259q_'), ('dm5', 'fg259m_')]:
    out = S / (name + '-generated.c')
    q = subprocess.run([str(C), '-o', str(out), str(S / (name + '.goose'))], capture_output=True, text=True)
    assert q.returncode == 0, q.stderr
    g = out.read_text()

    def qual(s, prefix=prefix):
        return re.sub(r'\b(gg_)', lambda m: prefix + m[1], s)

    tail = g[g.rfind('static void gg_function_0('):]
    entries = [x for x in re.findall(r'\b(gg_function_\d+)\(', tail) if x != 'gg_function_0']
    assert len(entries) == 1, entries
    entry = entries[0]

    funcs = ['static void gg_fail(const char*){asm("trap;");}']
    for m in re.finditer(r'(?m)^static [^\n{]+?\b(gg_function_\d+|gg_div|gg_mod|gg_udiv|gg_umod|gg_double_bits|gg_index)\([^;{}]*\)\s*\{', g):
        if m[1] == 'gg_function_0':
            continue
        funcs.append(body(g, m))
    text = qual('\n'.join(x[:x.index('{')].rstrip() + ';' for x in funcs) + '\n' + '\n'.join(funcs))
    text = re.sub(r'(?m)^static ', 'static __device__ __forceinline__ ', text)
    text = re.sub(r'\bgg_(?!fail_)', prefix + 'dx_', text)
    assert not any(x in text for x in ['gg_get_stack(', 'gg_allocate(', 'gg_append(', 'gg_ref(']), 'stack allocator reachable'

    mdef = re.search(r'static [^\n{]+?\b' + entry + r'\(([^)]*)\)\s*\{', g)
    assert mdef, entry
    params = [p.strip() for p in mdef[1].split(',') if p.strip()]
    names = [p.split()[-1] for p in params]
    has_base = names[-1] == 'base'
    wire = [n for n in names if n != 'base']
    call = ', '.join(wire + (['0'] if has_base else []))
    wparams = ', '.join(p for p in params if p.split()[-1] != 'base')
    text += (f'\nstatic __device__ __forceinline__ uint8_t {prefix}run({wparams})'
             f'{{return {prefix}{entry}({call});}}\n')

    (S / (name + '-functions-generated.cuh')).write_text('#pragma once\n' + text + '\n')
    rows.append(dict(module=name, entry=entry, wrapper=prefix + 'run', params=wire,
                     generated_c_sha256=hashlib.sha256(g.encode()).hexdigest()))

(E / 'emission.json').write_text(json.dumps(dict(
    status='PASS_CURRENT_SELFHOSTED_SOURCE_AND_MECHANICAL_EXTRACTION',
    compiler=pin, modules=rows), indent=2) + '\n')
print('PASS qs5/dm5 selfhosted emission and mechanical device extraction')
for r in rows:
    print(' ', r['module'], '->', r['wrapper'], r['params'])
