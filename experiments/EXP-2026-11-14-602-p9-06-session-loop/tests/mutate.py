#!/usr/bin/env python3
# Mutation run for P9-06 S5 (session loop decisions).
#
#   python3 tests/mutate.py 1     # 6 mutations, gate must fail on each
#   python3 tests/mutate.py 2     # 6 mutations
#
# Each mutation rewrites one decision in source/session-loop.goose, rebuilds the
# module at -O0 and reruns the same fixture. Builds are serial on purpose: the
# repository measures one build at a time.
from pathlib import Path
import shutil
import subprocess
import sys

B = Path(__file__).resolve().parent.parent
SRC = B / 'source' / 'session-loop.goose'
WORK = B / 'build' / 'mut'
G = '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose'

M1 = [
    ('the n_ctx bound is exclusive (equal sum rejected)',
     '    if n_prompt + n_predict + 8 > n_ctx {',
     '    if n_prompt + n_predict + 8 >= n_ctx {'),
    ('the 8-token framing margin is dropped',
     '    if n_prompt + n_predict + 8 > n_ctx {',
     '    if n_prompt + n_predict > n_ctx {'),
    ('a replay one token short of its length is accepted',
     '    if n_predict < token_count {',
     '    if n_predict <= token_count {'),
    ('the budget arm misses n_gen == n_predict',
     '    if n_gen >= n_predict {',
     '    if n_gen > n_predict {'),
    ('the replay stop fires one token late',
     '    if replay != 0 && n_gen == replay_tokens {',
     '    if replay != 0 && n_gen + 1 == replay_tokens {'),
    ('end-of-generation is tested before the replay stop',
     '    if replay != 0 && n_gen == replay_tokens {\n'
     '        return SL_STOP_REPLAY;\n'
     '    }\n'
     '    if eog != 0 {\n'
     '        return SL_STOP_EOG;\n'
     '    }',
     '    if eog != 0 {\n'
     '        return SL_STOP_EOG;\n'
     '    }\n'
     '    if replay != 0 && n_gen == replay_tokens {\n'
     '        return SL_STOP_REPLAY;\n'
     '    }'),
]

M2 = [
    ('the abort predicate requires every cause at once',
     '    if cancel != 0 || source_fatal != 0 || hook_fatal != 0 {',
     '    if cancel != 0 && source_fatal != 0 && hook_fatal != 0 {'),
    ('a workload mismatch is reported before the cancel',
     '    if cancel != 0 {\n'
     '        if phase == 0 {\n'
     '            return SL_CANCEL_PREFILL;\n'
     '        }\n'
     '        return SL_CANCEL_DECODE;\n'
     '    }\n'
     '    if workload_err != 0 {\n'
     '        return SL_FAIL_WORKLOAD;\n'
     '    }',
     '    if workload_err != 0 {\n'
     '        return SL_FAIL_WORKLOAD;\n'
     '    }\n'
     '    if cancel != 0 {\n'
     '        if phase == 0 {\n'
     '            return SL_CANCEL_PREFILL;\n'
     '        }\n'
     '        return SL_CANCEL_DECODE;\n'
     '    }'),
    ('the prefill and decode cancels are swapped',
     '        if phase == 0 {\n'
     '            return SL_CANCEL_PREFILL;',
     '        if phase == 1 {\n'
     '            return SL_CANCEL_PREFILL;'),
    ('the drafted slot is not carved out of the budget',
     '    return n_predict - n_gen - 1;',
     '    return n_predict - n_gen;'),
    ('the draft width takes the wider of the two bounds',
     '    if draft_max < room {\n'
     '        return draft_max;\n'
     '    }\n'
     '    return room;',
     '    if draft_max > room {\n'
     '        return draft_max;\n'
     '    }\n'
     '    return room;'),
    ('a cancelled workload is accepted as a finished run',
     '    if has_workload != 0 {\n'
     '        return SL_RES_CANCELLED_WORKLOAD;\n'
     '    }',
     '    if has_workload != 0 {\n'
     '        return SL_RES_OK;\n'
     '    }'),
]

MUT = {1: M1, 2: M2}


def run(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=300)


def main():
    half = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    if half not in MUT:
        print('usage: mutate.py 1|2')
        return 2
    WORK.mkdir(parents=True, exist_ok=True)
    orig = SRC.read_text()
    caught = survived = broken = 0
    for i, (name, old, new) in enumerate(MUT[half], 1):
        tag = f'h{half}-{i}'
        n = orig.count(old)
        if n != 1:
            print(f'MUT {tag} SETUP-ERROR occurrences={n} :: {name}')
            broken += 1
            continue
        (WORK / 'p.goose').write_text(orig.replace(old, new))
        g = run(f'cd {WORK} && timeout 200 {G} --standalone -o p.c --header g.h p.goose')
        if g.returncode != 0 or not (WORK / 'p.c').exists():
            print(f'MUT {tag} BUILD-FAIL (goose) :: {name}')
            broken += 1
            continue
        c = run(f'cd {WORK} && timeout 200 gcc-13 -O0 -std=c11 -w -c p.c -o p.o')
        if c.returncode != 0:
            print(f'MUT {tag} BUILD-FAIL (gcc) :: {name}')
            broken += 1
            continue
        l = run(f'cd {WORK} && timeout 200 g++-13 -O0 -std=c++20 -I. {B}/tests/session_loop_fixture.cpp '
                f'p.o -o pfix')
        if l.returncode != 0:
            print(f'MUT {tag} BUILD-FAIL (fixture) :: {name}')
            broken += 1
        else:
            r = run(f'cd {WORK} && timeout 120 ./pfix')
            tail = [ln for ln in r.stdout.splitlines() if ln.startswith('FG577_S5')]
            line = tail[-1] if tail else 'no summary (stopped before it)'
            if r.returncode != 0:
                caught += 1
                print(f'MUT {tag} CAUGHT :: {name} :: {line}')
            else:
                survived += 1
                print(f'MUT {tag} SURVIVED :: {name} :: {line}')
        for f in ('p.c', 'g.h', 'p.o', 'pfix'):
            (WORK / f).unlink(missing_ok=True)
    shutil.rmtree(WORK, ignore_errors=True)
    print(f'HALF {half} total={len(MUT[half])} caught={caught} survived={survived} broken={broken}')
    return 0 if survived == 0 and broken == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
