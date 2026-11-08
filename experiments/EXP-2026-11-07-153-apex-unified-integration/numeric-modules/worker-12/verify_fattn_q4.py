#!/usr/bin/env python3
"""Source/hash-bound verification for worker-12 Apex Q4_0 FlashAttention."""
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent
OMO = ROOT.parent
_CANDIDATES = [Path('/inputs/native/ggml/src/ggml-cuda'),
               OMO / 'inputs' / 'native' / 'ggml' / 'src' / 'ggml-cuda']
NATIVE = next(p for p in _CANDIDATES if p.is_dir())
SRC = NATIVE / 'fattn-vec.cuh'
PATCHED = ROOT / 'active-fattn-vec.patched.cuh'
PATCH = ROOT / 'fattn-q4-default-on.patch'
DEVICE = ROOT / 'generated' / 'fattn-q4-device.cu'
HOST = ROOT / 'generated' / 'fattn-q4-host.inc'
GEN = ROOT / 'generated' / 'fattn-q4.c'
MANIFEST = ROOT / 'generated' / 'fattn-q4-manifest.json'
KERNEL = ROOT / 'fattn-q4-kernel.cu'
FIXTURE = ROOT / 'fattn-q4-fixture.cpp'
OUT = ROOT / 'generated' / 'fattn-q4-replay.json'

EXPECTED_SOURCE_HASHES = {
    'fattn-vec.cuh': '70e35f944d7d13118cae4d8a782525771de0ef4f557d2841360c7187dace2be4',
    'fattn.cu': 'b286c1ac71064d43003d34d8cebfe2fa9627657d22a5ca4eb83f60d0ef0fd44e',
    'fattn-common.cuh': 'd47e737d80329f4b0862e92109b060a4ef7031041165b812ce0d6792b9466679',
}

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def rebuild_patched(text):
    a1 = '#include "common.cuh"\n#include "fattn-common.cuh"\n'
    text = text.replace(a1, a1 + '#include "fattn-q4-goose-bridge.h"\n', 1)
    a2 = ('template <int D, int cols_per_block, ggml_type type_K, ggml_type type_V, bool use_logit_softcap>\n'
          'void ggml_cuda_flash_attn_ext_vec_case_impl')
    helper = ('// worker-12: default-on Goose Q4_0 K/V vector kernel. GGML_GOOSE_FA_Q4=0 is\n'
              '// the explicit rollback to the native template instance.\n'
              'static bool fg012_fattn_q4_enabled() {\n'
              '    const char * e = std::getenv("GGML_GOOSE_FA_Q4");\n'
              "    return e == nullptr || e[0] != '0' || e[1] != '\\0';\n"
              '}\n\n')
    text = text.replace(a2, helper + a2, 1)
    a3 = ('    fattn_kernel_t fattn_kernel = flash_attn_ext_vec<D, cols_per_block, type_K, type_V, '
          'use_logit_softcap>;')
    new = ('    constexpr bool fg012_q4_kv = type_K == GGML_TYPE_Q4_0 && type_V == GGML_TYPE_Q4_0;\n'
           '    const bool fg012_goose = fg012_q4_kv && D >= 64 && D <= 256 && D % 64 == 0 &&\n'
           '        fg012_fattn_q4_enabled();\n'
           '    fattn_kernel_t fattn_kernel = fg012_goose\n'
           '        ? (cols_per_block == 1 ? fg012_flash_attn_ext_vec_q4_ncols1\n'
           '                               : fg012_flash_attn_ext_vec_q4_ncols2)\n'
           '        : flash_attn_ext_vec<D, cols_per_block, type_K, type_V, use_logit_softcap>;')
    return text.replace(a3, new, 1)

def main():
    src = SRC.read_text()
    manifest = json.loads(MANIFEST.read_text())
    device = DEVICE.read_text()
    host = HOST.read_text()
    kernel = KERNEL.read_text()
    fixture = FIXTURE.read_text()
    gen = GEN.read_text()
    patched = PATCHED.read_text()
    patch = PATCH.read_text()
    bridge = (ROOT / 'fattn-q4-goose-bridge.h').read_text()

    checks = {}
    checks['source_hash_fattn_vec'] = sha(SRC) == EXPECTED_SOURCE_HASHES['fattn-vec.cuh']
    checks['source_hash_fattn'] = sha(NATIVE / 'fattn.cu') == EXPECTED_SOURCE_HASHES['fattn.cu']
    checks['source_hash_common'] = sha(NATIVE / 'fattn-common.cuh') == EXPECTED_SOURCE_HASHES['fattn-common.cuh']
    checks['patch_rebuilds_candidate'] = rebuild_patched(src) == patched
    checks['patch_names_native_and_goose'] = ('flash_attn_ext_vec<D, cols_per_block' in patch and
                                              'fg012_flash_attn_ext_vec_q4_ncols1' in patch)
    checks['patch_default_on'] = 'e == nullptr || e[0] != \'0\'' in patch
    checks['patch_has_rollback'] = 'GGML_GOOSE_FA_Q4=0' in patch
    checks['generated_sha_matches_manifest'] = sha(GEN) == manifest['generated_sha256']
    checks['entry_arity_42'] = manifest['entry_arity'] == 42
    checks['entry_present'] = manifest['entry'] in gen
    checks['device_has_no_host_runtime'] = not re.search(
        r'\b(gg_fail|gg_get_stack|gg_append|gg_allocate|printf|fprintf)\b', device)
    checks['device_functions_qualified'] = all(
        re.search(r'static\s+(__host__\s+__device__|__device__)\s+__forceinline__\s+[\w \*]+\s+'
                  + re.escape(name) + r'\s*\(', device)
        for name in manifest['device_only'] + manifest['host_device'])
    checks['bridge_strict_prototypes'] = (bridge.count('extern "C" __global__ void fg012_') == 2 and
                                          'const int64_t nb33' in bridge and
                                          'const uint3 ne01' in bridge)
    checks['kernel_projects_entry'] = ('gg_function_' in kernel and
                                       'fg012_flash_attn_ext_vec_q4_ncols1' in kernel and
                                       'fg012_flash_attn_ext_vec_q4_ncols2' in kernel)
    checks['kernel_calls_manifest_entry'] = manifest['entry'] + '(' in kernel
    checks['host_exposes_all_roles'] = all(
        f'#define FG012_{role}' in host for role in manifest['roles'])
    checks['fixture_uses_emitted_roles'] = all(
        f'FG012_{role}' in fixture for role in [
            'ALIBI', 'APPLY_MASK', 'APPLY_SOFTCAP', 'DEQ4', 'DEQ4_SCALAR', 'KQ_OFFSET',
            'Q4_DOT', 'Q4_NIBBLE', 'Q8_AMAX4', 'Q8_PACK', 'Q8_ROUND', 'Q8_SUM4',
            'Q8_LANE_BYTE', 'SM_EXP', 'SM_MAX_NEW', 'SM_MIX', 'SM_VALUE'])
    common = (NATIVE / 'fattn-common.cuh').read_text()
    dispatch = (NATIVE / 'fattn.cu').read_text()
    checks['source_backed_names'] = (
        all(name in common for name in ['vec_dot_fattn_vec_KQ_q4_0', 'dequantize_V_q4_0',
                                        'quantize_q8_1_to_shared']) and
        'flash_attn_ext_vec' in src and
        'ggml_cuda_get_fattn_vec_case' in dispatch and
        'FATTN_VEC_CASES_ALL_D(Q4_0, Q4_0)' in dispatch)
    # The generated device unit must not contain the original native kernel.
    checks['no_native_kernel_text'] = 'flash_attn_ext_vec<' not in device and 'K_q4_0[ib].d' not in device

    result = {
        'schema': 'fg012.fattn-q4-replay/1',
        'status': 'PASS' if all(checks.values()) else 'FAIL',
        'checks': checks,
        'source_hashes': {k: sha(NATIVE / k) for k in EXPECTED_SOURCE_HASHES},
        'generated_hashes': {
            'goose': sha(ROOT / 'fattn-q4.goose'),
            'generated_c': sha(GEN),
            'device_cu': sha(DEVICE),
            'host_inc': sha(HOST),
            'kernel_cu': sha(KERNEL),
            'patch': sha(PATCH),
            'bridge': sha(ROOT / 'fattn-q4-goose-bridge.h'),
            'patched_candidate': sha(PATCHED),
        },
        'entry': manifest['entry'],
        'roles': manifest['roles'],
        'device_only': manifest['device_only'],
        'note': 'GPU execution, full-model parity and production CMake wiring stay with the parent gate.',
    }
    OUT.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    return 0 if result['status'] == 'PASS' else 1

if __name__ == '__main__':
    raise SystemExit(main())
