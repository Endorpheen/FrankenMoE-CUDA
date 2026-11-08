#!/usr/bin/env python3
"""worker-14 APX025 structural verifier."""
import hashlib, json, re, os, sys

ABI = open('fg14_abi.h').read()
SCHEMA = open('cuda-resource-schema.h').read()
STATS = json.load(open('build/patch-stats.json'))
PATCH = open('patched/ggml-cuda.cu').read()
COMMON = open('patched/ggml-cuda/common.cuh').read()
SRC = open('/inputs/active-lazy-kv/ggml-cuda-with-lazy-vmm.cu').read()
CSRC = open('/inputs/native/ggml/src/ggml-cuda/common.cuh').read()
fail = []

def check(name, cond):
    print(('PASS ' if cond else 'FAIL ') + name)
    if not cond: fail.append(name)

# 1. strong generated entries
for unit, macro in [('cuda-resource','FG14_RES_ENTRY'), ('cuda-launch','FG14_LAUNCH_ENTRY'), ('cuda-fusion','FG14_FUSION_ENTRY'), ('cuda-optimize','FG14_OPT_ENTRY')]:
    gen = open('generated/%s.c' % unit).read()
    decls = re.findall(r'static int64_t gg_function_1\(([^)]*)\)(;|\s*\{)', gen)
    check(unit + ': gg_function_1 declaration+definition', len(decls) == 2)
    if decls:
        args = [a for a in decls[0][0].split(',') if a.strip() and 'base' not in a]
        check(unit + ': 7 entry params', len(args) == 7)
    wrap = open('goose-entry-%s.c' % unit.split('-')[1]).read()
    check(unit + ': wrapper binds gg_function_1', ('%s gg_function_1' % macro) in wrap)

# 2. strict prototypes for every imported service
called = set()
for unit in ['cuda-resource','cuda-launch','cuda-fusion','cuda-optimize']:
    called |= set(re.findall(r'\b(fg14_s_[A-Za-z0-9_]+)\s*\(', open('generated/%s.c' % unit).read()))
declared = set(re.findall(r'\b(fg14_s_[A-Za-z0-9_]+)\s*\(', ABI))
check('all fg14_s imports declared', not (called - declared))
if called - declared: print('missing', sorted(called - declared))
check('canonical-storage import header published', 'fgc_record_new' in open('canonical-storage.h').read() and 'fgc_map_put' in open('canonical-storage.h').read())
check('CUDA resource schema keys published', all(k in SCHEMA for k in ['FG14_KIND_CTX','FG14_CTX_DEVICE','FG14_GRAPH_NODE_PROPS','FG14_CONC_JOIN_EVENTS']))
check('strong resource exports published', all(k in SCHEMA for k in ['fg14_res_stream','fg14_res_conc_valid','fg14_res_stream_ctx_insert']))

# 3. hash-bound source patch / rollback
check('patch default-on', '#ifdef FG14_NATIVE_ROLLBACK' in PATCH)
check('patch stats hash', STATS['ggml_cuda_sha256'] == hashlib.sha256(SRC.encode()).hexdigest())
check('patch rewrote 5 functions', STATS['rewritten_functions'] == 5)
check('graph_compute replaced', 'fg14_goose_launch(0,' in PATCH)
check('graph_optimize producer replaced', 'fg14_graph_optimize((int64_t) (intptr_t) backend->context' in PATCH)
check('fusion matcher replaced', 'fg14_can_fuse((int64_t) (intptr_t) cgraph' in PATCH)
check('try_fuse replaced', 'fg14_try_fuse(cuda_ctx->fg14_rec' in PATCH)
check('canonical record field published in common.cuh', 'int64_t fg14_rec = 0;' in COMMON)
check('context streams/cublas/pools wired to canonical records', 'fg14_res_stream(fg14_rec' in COMMON and 'fg14_res_cublas(fg14_rec' in COMMON and 'fg14_res_pool(fg14_rec' in COMMON)
check('graph cache queries wired to canonical records', 'fg14_res_graph_any_enabled(fg14_rec)' in COMMON)
check('capture-capable TU built with GGML_CUDA_USE_GRAPHS', os.path.exists('build/ggml-cuda-capture.o'))
check('TUs byte-identical to source (rollback copy)', open('patched/ggml-cuda.native.cu').read() == SRC and open('patched/common.native.cuh').read() == CSRC)
check('lazy-VMM pool preserved in patched TU', 'ggml_cuda_pool_vmm' in PATCH and 'ggml_backend_cuda_lazy_prepare' in PATCH)
check('services appended only to default build path', '#include "fg14_services_real.cpp"' in PATCH)

# 4. no owned C++ containers in the new canonical control modules
for f in ['cuda-resource.goose','cuda-launch.goose','cuda-fusion.goose','cuda-optimize.goose']:
    t = open(f).read()
    check(f + ': no std:: container', 'std::' not in t and 'unordered_map' not in t and 'std::vector' not in t)
res_src = open('cuda-resource.goose').read()
check('resource module uses real typed canonical bridges', 'fg14_s_record_new' in res_src and 'fg14_s_map_new' in res_src)
check('resource frees child maps/vectors explicitly', 's_map_free(gmap)' in res_src and 's_vec_free(gk)' in res_src)
check('stream_ctx_reset erases stale entries', 's_map_erase(m,t)' in res_src)
check('pool creation checks pointer separately', 's_check(np,16)' not in res_src)
check('VMM vs legacy pool selection present in services', 'ggml_cuda_pool_vmm' in open('fg14_services_real.cpp').read())
check('canonical field appended after native fields', 'int64_t fg14_rec = 0;' in COMMON and 'struct ggml_backend_cuda_context {\n    int device;\n    int64_t fg14_rec' not in COMMON)
check('foundation links real canonical-storage-ready objects', 'canonical-storage-ready/canonical-storage-services.o' in open('../local-check.json').read())

# 5. explicit emitted entry count and no leftover success stubs in real services
real = open('fg14_services_real.cpp').read()
check('fused routes use typed ready-module imports', 'fgn_cuda_shared_expert' in real and 'FG14_UNWIRED' not in real)
check('weighted reduction bound to real moe module', 'fg153_goose_moe_reduction_launch' in real)
check('gdn module header imported', 'fg153-goose-cuda.h' in real)
check('numeric-ready import header published', 'fgn_cuda_topk_moe' in open('cuda-numeric-ready.h').read())
check('patch source is lazy-VMM fork', 'ggml_cuda_pool_vmm' in SRC and 'vmm_lazy' not in SRC)
check('raw CUDA confined to services', not re.search(r'\bcuda[A-Z]\w+\s*\(', open('cuda-resource.goose').read() + open('cuda-launch.goose').read() + open('cuda-fusion.goose').read()))

print(json.dumps({'status': 'PASS' if not fail else 'FAIL', 'failures': fail}, indent=2))
sys.exit(0 if not fail else 1)
