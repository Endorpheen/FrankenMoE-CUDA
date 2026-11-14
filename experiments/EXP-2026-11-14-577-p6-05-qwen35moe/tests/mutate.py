#!/usr/bin/env python3
# Mutation gate for the qwen35moe plan/shape slice (P6-05 architecture 1).
#
#   python3 tests/mutate.py 1     # first six mutations
#   python3 tests/mutate.py 2     # second six mutations
#   python3 tests/mutate.py all   # both halves, one run at a time
#
# Each mutation rewrites source/qwen35moe.goose, rebuilds through tests/build.sh
# at -O0 in build/mut (the -O1 acceptance build is never touched) and runs the
# fixture. A mutation is CAUGHT when the fixture reports a failure or exits
# non-zero. A mutation that goose or the compiler rejects is BUILD-FAILED: it
# does not count as caught, because the point is a semantically wrong program
# that still passes the Goose check.
#
# Results go to mutations-half<N>.json next to this experiment; raw logs stay in
# build/mut-log/ because they are reproducible from the mutation table.
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import sys

E = Path(__file__).resolve().parent.parent
SRC = E / 'source' / 'qwen35moe.goose'
BUILD = E / 'tests' / 'build.sh'
OUT = E / 'build' / 'mut'
LOG = E / 'build' / 'mut-log'

# Half 1: the plan and layer-kind decisions (q35_kind / q35_plan).
M1 = [
    ('hybrid interval drops the +1 marker',
     '    return if (il + 1) % interval != 0 { K_RECR } else { K_ATTN };',
     '    return if il % interval != 0 { K_RECR } else { K_ATTN };'),
    ('recurrent and attention kinds are swapped',
     '    return if (il + 1) % interval != 0 { K_RECR } else { K_ATTN };',
     '    return if (il + 1) % interval != 0 { K_ATTN } else { K_RECR };'),
    ('the MTP layer is dropped from the plan',
     '        return if il < p[P_N_LAYER_ALL] { K_MTP } else { -1 };',
     '        return if il < p[P_N_LAYER] { K_MTP } else { -1 };'),
    ('the per-layer marker is read inverted',
     '        if r == 1 { return K_RECR; }\n        if r == 0 { return K_ATTN; }',
     '        if r == 1 { return K_ATTN; }\n        if r == 0 { return K_RECR; }'),
    ('a negative layer index is accepted',
     '    if il < 0 {\n        return -1;\n    }',
     '    if il < 0 {\n        return K_ATTN;\n    }'),
    ('the plan starts one layer late',
     '        kinds[i] = q35_kind(p, i, flags_base);',
     '        kinds[i] = q35_kind(p, i + 1, flags_base);'),
]

# Half 2: per-tensor shapes (q35_shape).
M2 = [
    ('the GDN output node is lost',
     '        if tid == T_SSM_OUT { q_put3(out, value_d, n_embd, 1); return 1; }',
     '        // mutation: ssm_out branch removed'),
    ('qk-norm uses the embedding width',
     '        if tid == T_ATTN_Q_NORM || tid == T_ATTN_K_NORM { q_put3(out, head_k, 1, 1); return 1; }',
     '        if tid == T_ATTN_Q_NORM || tid == T_ATTN_K_NORM { q_put3(out, n_embd, 1, 1); return 1; }'),
    ('n_expert_used fills the router table',
     '        if tid == T_FFN_GATE_INP { q_put3(out, n_embd, n_expert, 1); return 1; }',
     '        if tid == T_FFN_GATE_INP { q_put3(out, n_embd, used, 1); return 1; }'),
    ('the shared expert ignores its declared width',
     '    let n_ff_shexp_v = n_ff_shexp(p[P_N_FF_SHEXP], n_ff);',
     '    let n_ff_shexp_v = n_ff;'),
    ('the output tensor is lost',
     '''    if tid == T_OUTPUT {
        // output is optional; when absent the loader duplicates token_embd and
        // the declared shape is the same either way.
        q_put3(out, n_embd, n_vocab, 1);
        return 1;
    }''',
     '    // mutation: T_OUTPUT branch removed'),
    ('attention QKV forgets the value block',
     '        if tid == T_ATTN_QKV { q_put3(out, n_embd, qkv_fused, 1); return 1; }',
     '        if tid == T_ATTN_QKV { q_put3(out, n_embd, q_width + n_embd_k_gqa, 1); return 1; }'),
]

HALVES = {1: M1, 2: M2}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def clean_gen() -> None:
    # Force goose/gcc to run for every mutation instead of trusting mtime.
    for name in ('gen.o', 'runtime-gen.c', 'runtime-gen.h', '.opt'):
        (OUT / name).unlink(missing_ok=True)


def run_gate(tag: str):
    clean_gen()
    env = dict(os.environ, OPT='-O0', OUT_DIR=str(OUT))
    try:
        r = subprocess.run(['bash', str(BUILD)], capture_output=True, text=True,
                           timeout=300, env=env)
    except subprocess.TimeoutExpired:
        return None, 'timeout', 'timeout'
    out = r.stdout + r.stderr
    (LOG / f'mut-{tag}.log').write_text(out)
    summaries = [ln for ln in out.splitlines() if ln.startswith('FG579_ARCH1')]
    failures = sum(int(x) for x in re.findall(r'failures=(\d+)', out))
    # No summary means goose, the compiler or the link failed: the mutation was
    # not even a well-formed program.
    built = len(summaries) == 2
    return (built, r.returncode, failures), summaries, out


def run_half(half: int) -> int:
    mutations = HALVES[half]
    orig = SRC.read_text()
    assert SRC.read_text() == orig
    rows = []
    caught = 0
    try:
        for index, (name, old, new) in enumerate(mutations, start=1):
            tag = f'h{half}-{index}'
            occurrences = orig.count(old)
            if occurrences != 1:
                rows.append(dict(index=index, name=name, status='SETUP-ERROR',
                                 anchor_occurrences=occurrences))
                print(f'MUT {tag} SETUP-ERROR anchor_occurrences={occurrences} :: {name}')
                continue
            SRC.write_text(orig.replace(old, new, 1))
            result, summaries, out = run_gate(tag)
            if result is None:
                rows.append(dict(index=index, name=name, status='BUILD-FAILED',
                                 detail='timeout'))
                print(f'MUT {tag} BUILD-FAILED timeout :: {name}')
                continue
            built, rc, failures = result
            if not built:
                tail = [ln for ln in out.strip().splitlines() if ln.strip()]
                detail = tail[-1][:160] if tail else 'no output'
                rows.append(dict(index=index, name=name, status='BUILD-FAILED',
                                 detail=detail, exit_code=rc))
                print(f'MUT {tag} BUILD-FAILED :: {name} :: {detail}')
                continue
            ok = failures > 0 or rc != 0
            caught += ok
            rows.append(dict(index=index, name=name,
                             status='CAUGHT' if ok else 'SURVIVED',
                             failures=failures, exit_code=rc, summary=summaries))
            print(f'MUT {tag} {"CAUGHT" if ok else "SURVIVED"} :: {name} :: '
                  f'{summaries[-1] if summaries else "no summary"}')
    finally:
        SRC.write_text(orig)

    payload = dict(
        experiment='EXP-2026-11-14-577-p6-05-qwen35moe',
        half=half,
        opt='-O0',
        gate_binary=str(OUT / 'fg579-arch1'),
        source_sha256=sha256(SRC),
        fixture_sha256=sha256(E / 'tests' / 'fixture.cpp'),
        total=len(mutations),
        caught=caught,
        mutations=rows,
    )
    path = E / f'mutations-half{half}.json'
    path.write_text(json.dumps(payload, indent=2) + '\n')
    print(f'HALF {half} caught={caught}/{len(mutations)}')
    return 0 if caught == len(mutations) else 1


def baseline() -> int:
    """The unmutated source must still pass at -O1 with the original line."""
    env = dict(os.environ, OPT='-O1')
    env.pop('OUT_DIR', None)
    r = subprocess.run(['bash', str(BUILD)], capture_output=True, text=True,
                       timeout=300, env=env)
    out = (r.stdout + r.stderr)
    (LOG / 'baseline.log').write_text(out)
    line = [ln for ln in out.splitlines() if ln.startswith('FG579_ARCH1 cases=')]
    ok = r.returncode == 0 and line and line[0].endswith('failures=0')
    print(f'BASELINE {"OK" if ok else "FAILED"} :: {line[0] if line else "no summary"}')
    return 0 if ok else 1


def main() -> int:
    if len(sys.argv) != 2 or sys.argv[1] not in ('1', '2', 'all'):
        print(__doc__ or 'usage: mutate.py 1|2|all')
        return 2
    OUT.mkdir(parents=True, exist_ok=True)
    LOG.mkdir(parents=True, exist_ok=True)
    halves = [1, 2] if sys.argv[1] == 'all' else [int(sys.argv[1])]
    rc = 0
    for half in halves:
        rc |= run_half(half)
    rc |= baseline()
    return rc


if __name__ == '__main__':
    sys.exit(main())
