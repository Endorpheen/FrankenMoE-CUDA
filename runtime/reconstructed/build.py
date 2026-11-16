#!/usr/bin/env python3
"""Build the reconstructed DSO: staged integration files + flint objects + the Goose modules they call."""
from pathlib import Path
import subprocess, sys, re, os

BASE = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
# Sources come from the repository (vendor/flint, provenance in vendor/flint/PROVENANCE.txt). The prebuilt objects
# under build-cuda are still linked by the object list below and are the last external dependency; stage 4 removes them
# by rebuilding from these sources.
_HERE = Path(__file__).resolve().parent.parent
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
W = BASE/'runtime/reconstructed/build'; W.mkdir(parents=True, exist_ok=True)
RUN = BASE/'runtime/exp244/run'
OUT = W/'libllama-exp371.so.0.7.0'

FLAGS = ['-DGGML_BACKEND_SHARED','-DGGML_SHARED','-DGGML_USE_CPU','-DGGML_USE_CUDA','-DLLAMA_BUILD',
         '-DLLAMA_SHARED','-Dllama_EXPORTS', f'-I{SRC}/.', f'-I{FL}/build-cuda/src', f'-I{FL}/src', f'-I{FL}/include',
         f'-I{FL}/ggml/include', '-O3','-DNDEBUG','-fPIC','-w']
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
        print('module not found:', p); continue
    o = W/(p + '.o')
    cc = 'gcc-13' if found.suffix == '.c' else 'g++-13'
    extra = [] if found.suffix == '.c' else ['-std=c++17']
    q = subprocess.run([cc,'-O2','-fPIC',*extra,f'-I{found.parent}','-c',str(found),'-o',str(o)],
                       capture_output=True, text=True)
    if q.returncode: print('module build failed', p); print(q.stderr[:800]); continue
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
    ('goose/misc/unicode-han.goose',      'integration/m1/common/fg-unicode-han-bridge.c',      'fg-han'),
    ('goose/misc/unicode-newlines.goose', 'integration/m1/common/fg-unicode-newlines-bridge.c', 'fg-nl'),
    ('goose/misc/unicode-byte-map.goose', 'integration/m1/common/fg-unicode-byte-map-bridge.c', 'fg-b2u'),
    ('goose/misc/unicode-afmoe.goose',    'integration/m1/common/fg-unicode-afmoe-bridge.c',    'fg-afmoe'),
    ('goose/misc/unicode-gpt2.goose',     'integration/m1/common/fg-unicode-gpt2-bridge.c',     'fg-gpt2'),
    ('goose/misc/unicode-hangul.goose',   'integration/m1/common/fg-unicode-hangul-bridge.c',   'fg-hangul'),
    ('goose/misc/qwen35-split.goose',     'integration/m1/common/fg-qwen35-split-bridge.c',     'fg-q35s'),
]
_GOOSE = Path(__file__).resolve().parent.parent.parent / 'goose'
if not (_GOOSE / 'misc').is_dir():
    _GOOSE = Path('/home/end0/AI/FrankenMoE-CUDA/goose')
_REPO = _GOOSE.parent
_gobj = W/'goose-obj'; _gobj.mkdir(exist_ok=True)
_goose_objs = []
for _mod, _br, _stem in _GOOSE_PAIRS:
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
    _r = subprocess.run(['gcc-13','-O2','-fPIC','-std=c11','-w',f'-Dgoose_init={_stem.replace("-","_")}_goose_init',
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

# SLRU pieces are compiled from repository sources: two generated C files that live in experiments/ and the runtime
# shim in integration/. They replace three prebuilt objects and are the policy the expert cache depends on.
_slru_src = [
    (Path(_REPO)/'experiments'/'EXP-2026-11-16-756-slru-policy-goose'/'build'/'slru-policy.c', 'slru-policy.o'),
    (Path(_REPO)/'experiments'/'EXP-2026-11-16-756-slru-policy-goose'/'build'/'slru-runtime.c', 'slru-runtime.o'),
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

extra_mod = [W/'b291.o', W/'fx286pic.o', W/'fx262a.o', W/'fx263-pic.o', W/'gates.o',
             # compiled here from repository sources instead of taken prebuilt
             _slru_objs[0], _slru_objs[1], _slru_objs[2],
             W/'fg-ar.o', W/'fg-ar-bridge.o',
             W/'fg-q35.o', W/'fg-qg.o', W/'fg-q35-bridge.o', W/'fg-plan-ffi.o',
             W/'fg-gr.o', W/'fg-gr-bridge.o'] + [Path(x) for x in _goose_objs]
# The SLRU policy object is required: llama-moe-cache.cpp calls slru_plan. Fail loudly rather than
# linking a DSO with an undefined fg756s/slru symbol.
for need in ('slru-policy.o', 'slru-runtime.o', 'slru-shim.o'):
    if not (W/need).exists():
        print('MISSING', need, '- run: bash experiments/EXP-2026-11-16-756-slru-policy-goose/build.sh', W)
        sys.exit(1)
bridge_objs += [str(x) for x in extra_mod if x.exists()]
# REVERTED: compiling only llama-*.cpp and models/*.cpp dropped sources that the Unity objects also carried
# (ggml backend registration among them) and the runtime failed at start with a CUDA allocation error. Restored to the
# prebuilt objects until the vendor compile list covers everything the Unity units contained.
# The Unity units in llama.dir contain only models/*.cpp (verified by reading the generated unity_*.cxx: 166 includes,
# all under src/models/, no ggml, none of our 13). Replacing them means compiling vendor/flint/src/models/*.cpp - and
# nothing else, because llama-*.cpp are compiled by a different target and would duplicate.
_vmod_dir = W/'vendor-models'
_vmod_dir.mkdir(exist_ok=True)
flint_objs = []
for _src in sorted((FL/'src'/'models').glob('*.cpp')):
    _o = _vmod_dir/(_src.stem + '.o')
    if not _o.exists() or _o.stat().st_mtime < _src.stat().st_mtime:
        _r = subprocess.run(['/usr/bin/c++'] + [f for f in FLAGS if f.startswith('-I') or f.startswith('-D')] +
                            ['-O3','-DNDEBUG','-fPIC','-w','-c',str(_src),'-o',str(_o)],
                            capture_output=True, text=True)
        if _r.returncode != 0:
            print('model compile failed:', _src.name); print(_r.stderr[:1200]); sys.exit(1)
    flint_objs.append(str(_o))
print('models compiled from vendor:', len(flint_objs))
cmd = ['/usr/bin/c++','-fPIC','-O3','-DNDEBUG','-shared','-Wl,-soname,libllama.so.0','-o',str(OUT)]
cmd += objs + flint_objs + modules + bridge_objs
cmd += [f'-Wl,-rpath,{RUN}:', str(RUN/'libggml.so.0'), str(RUN/'libggml-cpu.so.0'),
        str(RUN/'libggml-cuda.so.0'), str(RUN/'libggml-base.so.0')]
q = subprocess.run(cmd, capture_output=True, text=True)
if q.returncode: print(q.stderr[:3000]); sys.exit(1)
print('linked', OUT, OUT.stat().st_size)
