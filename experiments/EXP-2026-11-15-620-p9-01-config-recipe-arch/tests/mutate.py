#!/usr/bin/env python3
# P9-01 mutation run: 12 mutations in two halves of six.
#
#   python3 tests/mutate.py 1     # cfg x4 + ar x2
#   python3 tests/mutate.py 2     # ar x2 + cc x4
#
# Each mutation rewrites ONE decision in one slice, regenerates that slice with
# goose at -O0, links its own fixture and reruns it. A mutation that is not
# caught is a hole in the gate, not a curiosity: both halves must be 6/6.
# Builds are serial on purpose (the repository measures one build at a time).
from pathlib import Path
import shutil
import subprocess
import sys

B = Path(__file__).resolve().parent.parent
OUT = B / 'build'
WORK = OUT / 'mut'
GOOSE = '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose'
CORE = Path('/home/end0/AI/FrankenMoE-CUDA/runtime/core')
CC = 'gcc-13'
CXX = 'g++-13'

SRC = {
    'cfg': B / 'source' / 'config-validate.goose',
    'ar': B / 'source' / 'arch-registry.goose',
    'cc': B / 'source' / 'config-resolve.goose',
}
HDR = {'cfg': 'cfg.h', 'ar': 'ar.h', 'cc': 'cc.h'}
FIX = {
    'cfg': B / 'tests' / 'config_fixture.cpp',
    'ar': B / 'tests' / 'arch_fixture.cpp',
    'cc': B / 'tests' / 'config_resolve_fixture.cpp',
}
SUM = {'cfg': 'FG622_S1', 'ar': 'FG622_S2', 'cc': 'FG622_S3'}
NATIVE = {'cfg': 'native-config.o', 'ar': 'native-arch.o', 'cc': None}
NATIVE_SRC = {
    'cfg': (CORE / 'src' / 'config.cpp'),
    'ar': (CORE / 'src' / 'moe' / 'arch_registry.cpp'),
}

# Half 1: config boundaries/order (4), registry table/matcher (2).
M1 = [
    ('cfg', 'n_predict zero is accepted',
     'if i[0] <= 0 {', 'if i[0] < 0 {'),
    ('cfg', 'ubatch equal to n_ctx is rejected',
     'if i[3] > i[2] {', 'if i[3] >= i[2] {'),
    ('cfg', 'cache_mb 0 is treated as the pathological band',
     'if i[18] > 0 && i[18] < CFG_CACHE_MIN_MB && i[31] == 0 {',
     'if i[18] < CFG_CACHE_MIN_MB && i[31] == 0 {'),
    ('cfg', 'NaN passes the drop range',
     'if !(f[3] >= 0.0 && f[3] <= 1.0) {', 'if f[3] < 0.0 || f[3] > 1.0 {'),
    ('ar', 'gemma4 loses its down projection',
     '        if slot == 1 {\n            return "ffn_down_exps";\n        }\n',
     '        if slot == 1 {\n            return "";\n        }\n'),
    ('ar', 'the .weight tail accepts a trailing space',
     "ar_at(name, at + suf.len + 7) == 0", "ar_at(name, at + suf.len + 7) == 32"),
]

# Half 2: registry identity/order (2), CLI resolution (4).
M2 = [
    ('ar', 'the first registry row is skipped by lookup',
     '    var i: i64 = 0;\n    while i < AR_COUNT {\n        if ar_eq(arch, ar_nth_arch(i)) != 0 {',
     '    var i: i64 = 1;\n    while i < AR_COUNT {\n        if ar_eq(arch, ar_nth_arch(i)) != 0 {'),
    ('ar', 'gemma4 is renamed in the table',
     '    if i == 3 {\n        return "gemma4";\n    }',
     '    if i == 3 {\n        return "gemma4b";\n    }'),
    ('cc', 'a typed flag no longer wins over the environment',
     '    if seen != 0 {\n        return flag_val;\n    }',
     '    if seen != 0 {\n        return dflt;\n    }'),
    ('cc', 'an empty environment variable is read as 0',
     '    if raw.len == 0 {\n        return dflt;\n    }\n    return cc_atoi_parse(raw);',
     '    if raw.len == 0 {\n        return 0;\n    }\n    return cc_atoi_parse(raw);'),
    ('cc', 'BMOE_CACHE_MB no longer suppresses auto-sizing',
     '    if env_cache_present != 0 {\n        return 0;\n    }\n    return 1;',
     '    return 1;'),
    ('cc', 'ahwb is mapped to anon',
     'if raw.len == 4 && raw[0] == 97 && raw[1] == 104 && raw[2] == 119 && raw[3] == 98 {\n        return 3;\n    }',
     'if raw.len == 4 && raw[0] == 97 && raw[1] == 104 && raw[2] == 119 && raw[3] == 98 {\n        return 2;\n    }'),
]

MUT = {1: M1, 2: M2}


def run(cmd, cwd=None):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=600, cwd=cwd)


def native_obj(slice_name):
    name = NATIVE[slice_name]
    if name is None:
        return ''
    obj = OUT / name
    src = NATIVE_SRC[slice_name]
    if not obj.is_file() or src.stat().st_mtime > obj.stat().st_mtime:
        r = run(f'{CXX} -O2 -std=c++17 -I{CORE / "include"} -c {src} -o {obj}')
        if r.returncode != 0:
            return None
    return str(obj)


def main():
    half = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    if half not in MUT:
        print('usage: mutate.py 1|2')
        return 2
    WORK.mkdir(parents=True, exist_ok=True)
    caught = survived = broken = 0
    for i, (slice_name, name, old, new) in enumerate(MUT[half], 1):
        tag = f'h{half}-{i}'
        d = WORK / tag
        shutil.rmtree(d, ignore_errors=True)
        d.mkdir(parents=True)
        base = SRC[slice_name].read_text()
        n = base.count(old)
        if n != 1:
            print(f'MUT {tag} SETUP-ERROR occurrences={n} :: {slice_name} :: {name}')
            broken += 1
            continue
        (d / 'p.goose').write_text(base.replace(old, new, 1))
        hdr = HDR[slice_name]
        g = run(f'timeout 300 {GOOSE} --standalone -o p.c --header {hdr} p.goose', cwd=d)
        if g.returncode != 0:
            print(f'MUT {tag} BUILD-FAIL (goose) :: {name}\n{g.stderr.strip()[:200]}')
            broken += 1
            continue
        c = run(f'timeout 300 {CC} -O0 -std=c11 -w -c p.c -o p.o', cwd=d)
        if c.returncode != 0:
            print(f'MUT {tag} BUILD-FAIL (gcc) :: {name}')
            broken += 1
            continue
        nat = native_obj(slice_name)
        if nat is None:
            print(f'MUT {tag} BUILD-FAIL (native) :: {name}')
            broken += 1
            continue
        link = (f'timeout 300 {CXX} -O0 -std=c++20 -I{d} -I{CORE / "include"} '
                f'{FIX[slice_name]} p.o {nat} -o pfix')
        l = run(link, cwd=d)
        if l.returncode != 0:
            print(f'MUT {tag} BUILD-FAIL (fixture) :: {name}\n{l.stderr.strip()[:200]}')
            broken += 1
        else:
            r = run('timeout 300 ./pfix', cwd=d)
            line = 'no summary (stopped before it)'
            for ln in r.stdout.splitlines():
                if ln.startswith(SUM[slice_name]):
                    line = ln
            if r.returncode != 0:
                caught += 1
                print(f'MUT {tag} CAUGHT :: {slice_name} :: {name} :: {line}')
            else:
                survived += 1
                print(f'MUT {tag} SURVIVED :: {slice_name} :: {name} :: {line}')
        shutil.rmtree(d, ignore_errors=True)
    shutil.rmtree(WORK, ignore_errors=True)
    print(f'HALF {half} total={len(MUT[half])} caught={caught} survived={survived} broken={broken}')
    return 0 if survived == 0 and broken == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
