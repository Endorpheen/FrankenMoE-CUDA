from pathlib import Path
import json, hashlib, re, shutil, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-456-p8-02-gcp-props'
S = E / 'source'
B = F / 'runtime/exp456/build'
B.mkdir(parents=True, exist_ok=True)
pin = json.loads((F / 'runtime/compiler/current.json').read_text())
C = Path(pin['compiler'])
assert hashlib.sha256(C.read_bytes()).hexdigest() == pin['sha256'], 'compiler pin mismatch'

mods = {}
for name in ['gcp', 'props']:
    q = subprocess.run([str(C), '-o', f'{name}-generated.c', str((S / f'{name}.goose').resolve())],
                       capture_output=True, text=True, cwd=str(B))
    assert q.returncode == 0, q.stderr
    shutil.copy(B / f'{name}-generated.c', S / f'{name}-generated.c')
    g = (S / f'{name}-generated.c').read_text()
    qual = re.sub(r'\b(gg_)', 'fg456_', g)
    qual = qual.replace('int main(', 'int fg456_generated_main_%s(' % name)
    (S / f'{name}-qualified.c').write_text(qual)
    protos = [(ret, fn, [t.split()[0] for t in params.split(',')][:-1])
              for ret, fn, params in re.findall(r'^static (\w+) (gg_function_\d+)\(([^)]*)\);', g, re.M)]
    mods[name] = dict(g=g, protos=protos, prefix='fg456_' if name == 'gcp' else 'fg456p_')


def qualify(text, prefix):
    # only the compiler's own gg_ symbols are renamed; the extern names that the
    # source declares (fg456_*) must keep matching their C stubs
    return re.sub(r'\bgg_', prefix, text)


# gcp: prefix every symbol with fg456g_ so two modules can share one binary
gcp = mods['gcp']
gcp_q = qualify(gcp['g'], 'fg456g_').replace('int main(', 'int fg456g_main(')
(S / 'gcp-qualified.c').write_text(gcp_q)
props = mods['props']
props_q = re.sub(r'\b(gg_)', 'fg456p_', props['g']).replace('int main(', 'int fg456p_main(')
(S / 'props-qualified.c').write_text(props_q)


def pick(protos, pattern):
    hit = [n for r, n, ps in protos if r == 'int64_t' and ps == pattern]
    assert len(hit) == 1, (pattern, protos)
    return hit[0]


# main calls mode, route, port, alias, predict_shape, instance_shape in that
# order; port_select and predict_shape share a signature, so order is the only
# reliable discriminator.
main_body = gcp['g'][gcp['g'].rfind('static void gg_function_0('):]
called = []
for name in re.findall(r'\b(gg_function_\d+)\(', main_body):
    if name != 'gg_function_0' and name not in called:
        called.append(name)
assert len(called) == 6, called
mode, route, port, alias, pshape, ishape = called

p = props['protos']
pbody = props['g'][props['g'].rfind('static void gg_function_0('):]
pcalled = []
for name in re.findall(r'\b(gg_function_\d+)\(', pbody):
    if name != 'gg_function_0' and name not in pcalled:
        pcalled.append(name)
assert len(pcalled) == 3, pcalled
keys_fn, mode_fn, post_fn = pcalled

(S / 'gcp-unit.c').write_text(
    '#include <stdint.h>\n'
    '#include "gcp-qualified.c"\n'
    f'int64_t fg456_mode(const char *p,int64_t n,int64_t *out){{return fg456g_{mode[3:]}((uint64_t)(uintptr_t)p,n,(uint64_t)(uintptr_t)out,0);}}\n'
    f'int64_t fg456_route(int64_t hv,const char *v,int64_t vn,const char *d,int64_t dn,char *out,int64_t cap){{'
    f' return fg456g_{route[3:]}(hv,(uint64_t)(uintptr_t)v,vn,(uint64_t)(uintptr_t)d,dn,(uint64_t)(uintptr_t)out,cap,0);}}\n'
    f'int64_t fg456_port(int64_t en,int64_t sp,int64_t ap,int64_t *out){{return fg456g_{port[3:]}(en,sp,ap,(uint64_t)(uintptr_t)out,0);}}\n'
    f'int64_t fg456_alias(const char *p,int64_t n,char *out,int64_t cap){{return fg456g_{alias[3:]}((uint64_t)(uintptr_t)p,n,(uint64_t)(uintptr_t)out,cap,0);}}\n'
    f'int64_t fg456_pshape(int64_t o,int64_t h,int64_t c,int64_t *out){{return fg456g_{pshape[3:]}(o,h,c,(uint64_t)(uintptr_t)out,0);}}\n'
    f'int64_t fg456_ishape(int64_t o,int64_t h,int64_t *out){{return fg456g_{ishape[3:]}(o,h,(uint64_t)(uintptr_t)out,0);}}\n')
(S / 'props-unit.c').write_text(
    '#include <stdint.h>\n'
    '#include "props-qualified.c"\n'
    f'int64_t fg456_keys(int64_t has,int64_t *out){{return fg456p_{keys_fn[3:]}(has,(uint64_t)(uintptr_t)out,0);}}\n'
    f'int64_t fg456_pmode(int64_t sleeping,int64_t *out){{return fg456p_{mode_fn[3:]}(sleeping,(uint64_t)(uintptr_t)out,0);}}\n'
    f'int64_t fg456_post(int64_t enabled,int64_t *out){{return fg456p_{post_fn[3:]}(enabled,(uint64_t)(uintptr_t)out,0);}}\n')
(E / 'emission.json').write_text(json.dumps(dict(status='PASS', compiler=pin, entries=dict(
    mode=mode, route=route, port=port, alias=alias, pshape=pshape, ishape=ishape,
    props=pcalled)), indent=2) + '\n')
print('PASS gcp/props emission')
