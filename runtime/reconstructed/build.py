#!/usr/bin/env python3
"""Build the reconstructed DSO: staged integration files + flint objects + the Goose modules they call."""
from pathlib import Path
import subprocess, sys, re, os, argparse, hashlib, json

BASE = Path(__file__).resolve().parents[2]
ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--build-dir', type=Path, required=True)
ap.add_argument('--runtime-libs', type=Path, required=True)
args = ap.parse_args()
if args.build_dir.exists():
    sys.exit('refusing to overwrite an existing build directory')
# Sources come from the repository (vendor/flint, provenance in vendor/flint/PROVENANCE.txt). The prebuilt objects
# under build-cuda are still linked by the object list below and are the last external dependency; stage 4 removes them
# by rebuilding from these sources.
_HERE = BASE
FL = _HERE / 'vendor' / 'flint'
if not (FL / 'include' / 'llama.h').exists():
    # build.py is copied into the working tree as well; the sources live in the repository either way
    FL = Path('/home/end0/AI/FrankenMoE-CUDA/vendor/flint')
GOOSE_BIN = '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose'
# The 13 sources the build compiles live in this repository and are tracked (EXP: eight of them were not,
# see docs/goose-migration/UNTRACKED-SOURCES-20261116.md). Prefer the repo copy so a clone is buildable; fall
# back to the external tree only for a checkout that predates the commit that added them.
REPO_SRC = Path(__file__).resolve().parent/'src'
SRC = REPO_SRC if (REPO_SRC/'llama-vocab.cpp').exists() else BASE/'runtime/reconstructed/src'
print('sources:', SRC)
W = args.build_dir.resolve(); W.mkdir(parents=True)
RUN = args.runtime_libs.resolve()
OUT = W/'libllama.so.0'
_generated = W/'generated'
_generated.mkdir()
_version = (FL/'src/llama-version.h.in').read_text()
_version = _version.replace('@LLAMA_VERSION@', '0.7.0-dev').replace('@LLAMA_BUILD_COMMIT@', '1b44e307')
(_generated/'llama-version.h').write_text(_version)
_real_run = subprocess.run
def recorded_run(command, **kwargs):
    with (W/'commands.jsonl').open('a') as log:
        log.write(json.dumps(command) + '\n')
    return _real_run(command, **kwargs)
subprocess.run = recorded_run
pin = json.loads((BASE/'runtime/compiler/current.json').read_text())
if hashlib.sha256(Path(GOOSE_BIN).read_bytes()).hexdigest() != pin['sha256']:
    sys.exit('Goose compiler pin mismatch')
_slru_dir = W/'slru-obj'
subprocess.run(['bash', str(BASE/'experiments/EXP-2026-11-16-756-slru-policy-goose/build.sh'),
                str(_slru_dir)], check=True)

FLAGS = ['-DGGML_BACKEND_SHARED','-DGGML_SHARED','-DGGML_USE_CPU','-DGGML_USE_CUDA','-DLLAMA_BUILD',
         '-DLLAMA_SHARED','-Dllama_EXPORTS', f'-I{SRC}/.', f'-I{_generated}', f'-I{FL}/src', f'-I{FL}/include',
         f'-I{FL}/ggml/include', f'-I{_slru_dir}', '-O3','-DNDEBUG','-fPIC','-w']
CXX = ['/usr/bin/g++-13'] + FLAGS

changed = ['llama-batch.cpp','llama-context.cpp','llama-graph.cpp','llama-hparams.cpp','llama-kv-cache.cpp','llama-model.cpp',
           'llama-memory-recurrent.cpp','llama-memory-hybrid.cpp','llama-model-loader.cpp','llama-sampler.cpp',
           'llama-vocab.cpp','llama-moe-cache.cpp','unicode.cpp']
objs = []
for name in changed:
    o = W/(name + '.o')
    q = subprocess.run(CXX + ['-o', str(o), '-c', str(SRC/name)], capture_output=True, text=True)
    if q.returncode: print('FAIL', name); print(q.stderr[:1500]); sys.exit(1)
    objs.append(str(o))
    print('built', name)

# The reader adapter owns the mapping and is the only file here that is not a
# carrier source. It is debt and tools/migration-status.sh counts it as such.
_r = subprocess.run(CXX + ['-o', str(W/'gguf-reader-owner.o'), '-c',
                           str(BASE/'integration/goose-standalone/gguf-reader-owner.cpp')],
                    capture_output=True, text=True)
if _r.returncode:
    print('FAIL gguf-reader-owner.cpp'); print(_r.stderr[:1500]); sys.exit(1)
objs.append(str(W/'gguf-reader-owner.o'))
print('built gguf-reader-owner.cpp')

# which Goose modules are called
def undef(o):
    s = subprocess.run(['nm','-u',o],capture_output=True,text=True).stdout
    return {l.split()[-1] for l in s.splitlines() if l.strip()}
pref = set()
for o in objs:
    for sym in undef(o):
        m = re.match(r'^(fg\d{3}[a-z])', sym)
        if m: pref.add(m.group(1))
print('modules needed:', ' '.join(sorted(pref)))

# locate host-unit.c per prefix and build it
modules = []
for p in sorted(pref):
    if p in ('fg262a', 'fg263c', 'fg286m', 'fg291b', 'fg756s'):
        continue  # Built explicitly below, including the shared gates object.
    if p in ('fg345w', 'fg367s', 'fg373s', 'fg375s', 'fg376s', 'fg377s', 'fg380s', 'fg381s', 'fg382s', 'fg383s', 'fg384s', 'fg385s', 'fg386s', 'fg387s', 'fg388s', 'fg389s', 'fg390s', 'fg391s', 'fg392s', 'fg393s', 'fg394s', 'fg395s', 'fg396s', 'fg397s', 'fg398s', 'fg401s'):
        continue  # our own bridges are linked explicitly below
    found = None
    # any translation unit in a module's source that defines the prefix; generated files are
    # #included by their bridge, so they are skipped here
    # prefer the LATEST experiment that defines the prefix: bridges were revised across experiments
    import re as _re
    cands = []
    for hu in list(BASE.glob('experiments/*/source/*.c')) + list(BASE.glob('experiments/*/source/*.cpp')):
        if 'generated' in hu.name: continue
        # a module DEFINES the prefix when it carries its enabled() entry point; merely calling
        # another module's function must not make this file its owner (EXP392 called fg199k_can_use)
        if (p + '_enabled').encode() in hu.read_bytes():
            m = _re.search(r'EXP-\d{4}-\d{2}-\d{2}-(\d+)', str(hu))
            if m: cands.append((int(m.group(1)), hu))
    if cands:
        found = max(cands)[1]
    if not found:
        sys.exit('module not found: ' + p)
    o = W/(p + '.o')
    cc = 'gcc-13' if found.suffix == '.c' else 'g++-13'
    extra = [] if found.suffix == '.c' else ['-std=c++17']
    q = subprocess.run([cc,'-O2','-fPIC',*extra,f'-I{found.parent}','-c',str(found),'-o',str(o)],
                       capture_output=True, text=True)
    if q.returncode: print('module build failed', p); print(q.stderr[:800]); sys.exit(1)
    modules.append(str(o))
print('modules built:', len(modules))

BR = Path(__file__).resolve().parent.parent.parent / 'integration' / 'modules'
if not BR.is_dir():
    BR = Path('/home/end0/AI/FrankenMoE-CUDA/integration/modules')
bridge_objs = []
for mod, out_name in (('EXP-2026-11-11-347-goose-kv-write-bridge', 'b345.o'),
                      ('EXP-2026-11-11-367-flint-kv-state-clear-live', 'b367.o'),
                      ('EXP-2026-11-11-373-flint-read-sinfo-live', 'b373.o'),
                      ('EXP-2026-11-11-375-flint-hybrid-memory-live', 'b375.o'),
                      ('EXP-2026-11-11-376-flint-hybrid-context-live', 'b376.o'),
                      ('EXP-2026-11-11-377-flint-file-alignment-live', 'b377.o'),
                      ('EXP-2026-11-11-380-flint-penalties-accept-live', 'b380.o'),
                      ('EXP-2026-11-11-381-flint-kv-whole-clear-live', 'b381.o'),
                      ('EXP-2026-11-11-382-flint-recurrent-io-plan-live', 'b382.o'),
                      ('EXP-2026-11-11-383-flint-batch-container-plan-live', 'b383.o'),
                      ('EXP-2026-11-11-384-goose-sampler-softmax-live', 'b384.o'),
                      ('EXP-2026-11-11-385-goose-sampler-guards-live', 'b385.o'),
                      ('EXP-2026-11-11-386-goose-sampler-dist-pick-live', 'b386.o'),
                      ('EXP-2026-11-11-387-goose-sampler-chain-live', 'b387.o'),
                      ('EXP-2026-11-11-388-goose-sampler-filters-guard-live', 'b388.o'),
                      ('EXP-2026-11-11-389-goose-gguf-inventory-live', 'b389.o'),
                      ('EXP-2026-11-11-390-goose-vocab-partition-live', 'b390.o'),
                      ('EXP-2026-11-11-391-goose-fused-ops-resolve-live', 'b391.o'),
                      ('EXP-2026-11-11-392-goose-kv-find-slot-walk', 'b392.o'),
                      ('EXP-2026-11-11-393-goose-kv-meta-plan', 'b393.o'),
                      ('EXP-2026-11-11-394-goose-kv-layout-span', 'b394.o'),
                      ('EXP-2026-11-11-395-goose-kv-read-meta', 'b395.o'),
                      ('EXP-2026-11-11-396-goose-recurrent-payload-span', 'b396.o'),
                      ('EXP-2026-11-11-397-goose-hybrid-delegation', 'b397.o'),
                      ('EXP-2026-11-11-398-goose-batch-container-layout', 'b398.o'),
                      ('EXP-2026-11-11-401-goose-detok-decisions', 'b401.o')):
    o = W/out_name
    q = subprocess.run(['gcc','-O3','-DNDEBUG','-fPIC','-std=c11','-c', str(BR/mod/'source/bridge.c'),
                        '-o', str(o), '-I', str(BR/mod/'source')], capture_output=True, text=True)
    if q.returncode: print('bridge failed', mod); print(q.stderr[:800]); sys.exit(1)
    bridge_objs.append(str(o))

# Goose modules are compiled here from goose/misc and integration/m1/common instead of being linked as prebuilt
# objects from the working directory. Each pair is (module source, bridge source, object stem).
_GOOSE_PAIRS = [
    ('goose/misc/unicode-han.goose',      'integration/m1/common/fg-unicode-han-bridge.c',      'fg-han', 'fg_unicode_han_goose_init'),
    ('goose/misc/unicode-newlines.goose', 'integration/m1/common/fg-unicode-newlines-bridge.c', 'fg-nl', 'fg_unicode_nl_goose_init'),
    ('goose/misc/unicode-byte-map.goose', 'integration/m1/common/fg-unicode-byte-map-bridge.c', 'fg-b2u', 'fg_b2u_goose_init'),
    ('goose/misc/unicode-afmoe.goose',    'integration/m1/common/fg-unicode-afmoe-bridge.c',    'fg-afmoe', 'fg_afmoe_goose_init'),
    ('goose/misc/unicode-gpt2.goose',     'integration/m1/common/fg-unicode-gpt2-bridge.c',     'fg-gpt2', 'fg_gpt2_goose_init'),
    ('goose/misc/unicode-hangul.goose',   'integration/m1/common/fg-unicode-hangul-bridge.c',   'fg-hangul', 'fg_hangul_goose_init'),
    ('goose/misc/qwen35-split.goose',     'integration/m1/common/fg-qwen35-split-bridge.c',     'fg-q35s', 'fg_qwen35_split_goose_init'),
    # mapping established from the object symbols, not from the file names: fg-ar exports ar_arch/ar_count/ar_exps
    # (arch-registry), fg-gr exports gr_run (graph-reuse), fg-qg exports qg_* (qgate-split)
    ('goose/model/arch-registry.goose',   'integration/m1/common/fg-arch-registry-bridge.c',    'fg-ar',  'fg_arch_registry_goose_init'),
    ('goose/cpu/graph-reuse.goose',       'integration/m1/common/fg-graph-reuse-bridge.c',      'fg-gr',  'fg_graph_reuse_goose_init'),
    ('goose/model/qgate-split.goose',     'integration/m1/common/fg-goose-quant-bridge.c',      'fg-qg',  'fg_qg_goose_init'),
]
_GOOSE = Path(__file__).resolve().parent.parent.parent / 'goose'
if not (_GOOSE / 'misc').is_dir():
    _GOOSE = Path('/home/end0/AI/FrankenMoE-CUDA/goose')
_REPO = _GOOSE.parent
_gobj = W/'goose-obj'; _gobj.mkdir(exist_ok=True)
_goose_objs = []
for _pair in _GOOSE_PAIRS:
    _mod, _br, _stem = _pair[0], _pair[1], _pair[2]
    _init = _pair[3] if len(_pair) > 3 else _stem.replace('-', '_') + '_goose_init'
    _ms = _GOOSE / _mod.split('/', 1)[1] if _mod.startswith('goose/') else _GOOSE.parent / _mod
    _bs = _REPO / _br
    if not _ms.exists() or not _bs.exists():
        print('goose source missing:', _ms, _bs); sys.exit(1)
    _co = _gobj/(_stem + '.c')
    _r = subprocess.run([str(GOOSE_BIN), '-o', str(_co), '--header', str(_co.with_suffix('.h')), str(_ms)],
                        capture_output=True, text=True)
    if _r.returncode != 0:
        print('goose compile failed:', _ms.name); print(_r.stderr[:800]); sys.exit(1)
    _mo = _gobj/(_stem + '.o')
    _r = subprocess.run(['gcc-13','-O2','-fPIC','-std=c11','-w',f'-Dgoose_init={_init}',
                         '-c', str(_co), '-o', str(_mo)], capture_output=True, text=True)
    if _r.returncode != 0:
        print('module cc failed:', _stem); print(_r.stderr[:800]); sys.exit(1)
    _bo = _gobj/(_stem + '-bridge.o')
    _r = subprocess.run(['gcc-13','-O2','-fPIC','-std=c11','-w','-c', str(_bs), '-o', str(_bo)],
                        capture_output=True, text=True)
    if _r.returncode != 0:
        print('bridge cc failed:', _stem); print(_r.stderr[:800]); sys.exit(1)
    _goose_objs += [str(_mo), str(_bo)]
print('goose modules compiled from sources:', len(_goose_objs)//2)

# The tokenizer path is one unit, not one module: the reader, its header and type
# tables, the Unicode and pre-tokenizer tables, the BPE engine and the load join.
# Goose has no re-export, so they are composed the same way
# tools/build-main-runtime.sh composes the standalone unit, then compiled once.
# Symbols were measured before this was added: the unit exports 67 globals and
# the only name it shares with the accepted libllama set is goose_init, which is
# renamed here.
_CARRIER_UNIT = [
    'misc/gguf-headers.goose', 'misc/ggml-type-table.goose', 'model/gguf-tensors.goose',
    'model/gguf-reader.goose', 'model/special-partition.goose',
    'misc/unicode-tables.goose', 'misc/unicode-text.goose',
    'misc/pretokenizer-qwen35.goose', 'model/bpe-tokenize.goose',
    'model/bpe-detokenize.goose', 'model/bpe-load.goose',
]
_cunit = _gobj/'tokenizer-unit.goose'
_r = subprocess.run([sys.executable, str(_REPO/'tools'/'compose-goose.py'), str(_cunit)]
                    + [str(_GOOSE/m) for m in _CARRIER_UNIT], capture_output=True, text=True)
if _r.returncode != 0:
    print('tokenizer unit compose failed'); print(_r.stderr[:800]); sys.exit(1)
_r = subprocess.run([str(GOOSE_BIN), '--standalone', '-o', str(_gobj/'tokenizer-unit.c'),
                     '--header', str(_gobj/'tokenizer-unit.h'), str(_cunit)], capture_output=True, text=True)
if _r.returncode != 0:
    print('tokenizer unit compile failed'); print(_r.stderr[:800]); sys.exit(1)
_r = subprocess.run(['gcc-13','-O2','-fPIC','-std=c11','-w','-Dgoose_init=fg950_goose_init',
                     '-c', str(_gobj/'tokenizer-unit.c'), '-o', str(_gobj/'tokenizer-unit.o')],
                    capture_output=True, text=True)
if _r.returncode != 0:
    print('tokenizer unit cc failed'); print(_r.stderr[:800]); sys.exit(1)
_goose_objs.append(str(_gobj/'tokenizer-unit.o'))
print('tokenizer unit modules composed:', len(_CARRIER_UNIT))

# SLRU pieces are compiled from repository sources: two generated C files that live in experiments/ and the runtime
# shim in integration/. They replace three prebuilt objects and are the policy the expert cache depends on.
_slru_src = [
    (_slru_dir/'slru-policy.c', 'slru-policy.o'),
    (_slru_dir/'slru-runtime.c', 'slru-runtime.o'),
    (Path(_REPO)/'integration'/'goose-standalone'/'slru-runtime-shim.c', 'slru-shim.o'),
]
_sobj = W/'slru-obj'; _sobj.mkdir(exist_ok=True)
_slru_objs = []
for _s, _n in _slru_src:
    _o = _sobj/_n
    _r = subprocess.run(['gcc-13','-O2','-fPIC','-std=c11','-w','-c', str(_s), '-o', str(_o)],
                        capture_output=True, text=True)
    if _r.returncode != 0:
        print('slru compile failed:', _s.name); print(_r.stderr[:800]); sys.exit(1)
    _slru_objs.append(_o)
print('slru objects compiled:', len(_slru_objs))

# Recreate every extra object; directory leftovers must never supply link inputs.
for _src, _name in [
    (BASE/'experiments/EXP-2026-11-12-291-goose-batch-reject/bridge.c', 'b291.o'),
    (SRC/'gates.c', 'gates.o'),
    (BASE/'integration/m1/common/fg-apex-plan-bridge.c', 'fg-q35-bridge.o'),
    (BASE/'integration/m1/common/fg-apex-plan-ffi.c', 'fg-plan-ffi.o'),
]:
    subprocess.run(['gcc-13', '-O2', '-fPIC', '-std=c11', '-w',
                    f'-I{FL}/include', f'-I{FL}/ggml/include',
                    '-c', str(_src), '-o', str(W/_name)], check=True)
subprocess.run([GOOSE_BIN, '-o', str(W/'q35.c'), '--header', str(W/'q35.h'),
                str(BASE/'goose/model/qwen35moe.goose')], check=True, capture_output=True)
subprocess.run(['gcc-13', '-O2', '-fPIC', '-std=c11', '-w',
                '-Dgoose_init=fg_q35_goose_init', '-c', str(W/'q35.c'),
                '-o', str(W/'fg-q35.o')], check=True)
subprocess.run([sys.executable, str(BASE/'tools/build-goose-bridges.py'),
                '--out', str(W)], check=True)
subprocess.run(['gcc-13', '-O2', '-fPIC', '-std=c11', '-w', '-c',
                str(BASE/'integration/m1/common/fg-mmap-bookkeeping-bridge.c'),
                '-o', str(W/'fx286pic.o')], check=True)

extra_mod = [W/'b291.o', W/'fx286pic.o', W/'fx262a.o', W/'fx263-pic.o', W/'gates.o',
             # compiled here from repository sources instead of taken prebuilt
             _slru_objs[0], _slru_objs[1], _slru_objs[2],
             W/'fg-q35.o', W/'fg-q35-bridge.o', W/'fg-plan-ffi.o'] + [Path(x) for x in _goose_objs]
# The SLRU policy object is required: llama-moe-cache.cpp calls slru_plan. Fail loudly rather than
# linking a DSO with an undefined fg756s/slru symbol.
for need in ('slru-policy.o', 'slru-runtime.o', 'slru-shim.o'):
    if not (_sobj/need).exists():
        print('MISSING', need, '- run: bash experiments/EXP-2026-11-16-756-slru-policy-goose/build.sh', W)
        sys.exit(1)
for x in extra_mod:
    if not x.exists(): sys.exit('required object missing: ' + str(x))
bridge_objs += [str(x) for x in extra_mod]
# REVERTED: compiling only llama-*.cpp and models/*.cpp dropped sources that the Unity objects also carried
# (ggml backend registration among them) and the runtime failed at start with a CUDA allocation error. Restored to the
# prebuilt objects until the vendor compile list covers everything the Unity units contained.
# The Unity units in llama.dir contain only models/*.cpp (verified by reading the generated unity_*.cxx: 166 includes,
# all under src/models/, no ggml, none of our 13). Replacing them means compiling vendor/flint/src/models/*.cpp - and
# nothing else, because llama-*.cpp are compiled by a different target and would duplicate.
_vmod_dir = W/'vendor-models'
_vmod_dir.mkdir(exist_ok=True)
flint_objs = []
_replaced_cpp = set(changed)
_vend_srcs = [x for x in sorted((FL/'src').glob('llama-*.cpp')) if x.name not in _replaced_cpp]
_vend_srcs += sorted((FL/'src'/'models').glob('*.cpp'))
_vend_srcs += [_REPO/'vendor'/'flint'/'src'/'unicode-data.cpp', _REPO/'vendor'/'flint'/'src'/'llama.cpp']
for _src in _vend_srcs:
    _stem = 'core-llama' if _src == FL/'src'/'llama.cpp' else _src.stem
    _o = _vmod_dir/(_stem + '.o')
    if not _o.exists() or _o.stat().st_mtime < _src.stat().st_mtime:
        _r = subprocess.run(['/usr/bin/c++'] + [f for f in FLAGS if (f.startswith('-I') or f.startswith('-D')) and f != f'-I{SRC}/.'] +
                            ['-O3','-DNDEBUG','-fPIC','-w','-c',str(_src),'-o',str(_o)],
                            capture_output=True, text=True)
        if _r.returncode != 0:
            print('model compile failed:', _src.name); print(_r.stderr[:1200]); sys.exit(1)
    flint_objs.append(str(_o))
print('models compiled from vendor:', len(flint_objs))
cmd = ['/usr/bin/c++','-fPIC','-O3','-DNDEBUG','-shared','-Wl,-z,defs','-Wl,-soname,libllama.so.0','-o',str(OUT)]
cmd += objs + flint_objs + modules + bridge_objs
cmd += ['-Wl,-rpath,$ORIGIN', str(RUN/'libggml.so.0'), str(RUN/'libggml-cpu.so.0'),
        str(RUN/'libggml-cuda.so.0'), str(RUN/'libggml-base.so.0')]
q = subprocess.run(cmd, capture_output=True, text=True)
if q.returncode: print(q.stderr[:3000]); sys.exit(1)
print('linked', OUT, OUT.stat().st_size)
