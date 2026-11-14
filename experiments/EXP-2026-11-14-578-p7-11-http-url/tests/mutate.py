#!/usr/bin/env python3
# Mutation run for the HTTP url policy slice (EXP574 S1).
# Each mutation edits the Goose port, regenerates the gate driver, runs it with
# `goose run` and compares with the native oracle. Any diff, run failure or
# missing output counts as caught. Usage: mutate.py [lo hi]
from pathlib import Path
import subprocess
import sys

S = Path(__file__).resolve().parent.parent
SRC = S / 'source' / 'http-url-policy.goose'
ORIG = SRC.read_text()
G = '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose'

M = [
    # ---- half 1: scheme, ports, authority split ---------------------------
    ('the https scheme is rejected', '!http_eq(scheme, "https")', '!http_eq(scheme, "htps")'),
    ('the default http port becomes 81', 'port[0] = 80;', 'port[0] = 81;'),
    ('the default https port becomes 80', 'port[0] = 443;', 'port[0] = 80;'),
    ('a bad port reports the IPv6 status', '            return 4;', '            return 3;'),
    ('the bracketed port separator is wrong',
     'if after < host_to && url[after] == 58 {', 'if after < host_to && url[after] == 59 {'),
    ('the password length is off by one',
     '            len[2] = auth_to - colon - 1;', '            len[2] = auth_to - colon;'),
    ('the invented path slash is lost', '    off[4] = path_off;', '    off[4] = 0;'),
    # ---- half 2: loopback and NO_PROXY -----------------------------------
    ('0.0.0.0 is no longer loopback', 'http_eq_fold(host, "0.0.0.0")', 'http_eq_fold(host, "0.0.0.1")'),
    ('the bare .localhost becomes loopback', 'if host.len > 10 {', 'if host.len >= 10 {'),
    ('the .localhost suffix is misspelled',
     'if http_eq_fold(tail, ".localhost") {', 'if http_eq_fold(tail, ".localhosu") {'),
    ('the loopback first octet is 128', 'if octets[0] != 127 {', 'if octets[0] != 128 {'),
    ('four-digit octets are accepted', 'if dot == pos || dot - pos > 3 {', 'if dot == pos || dot - pos > 4 {'),
    ('the star entry no longer matches', 'if http_eq_fold(entry, "*") {', 'if http_eq_fold(entry, "**") {'),
    ('the subdomain dot check is wrong',
     'host[host.len - body.len - 1] == 46', 'host[host.len - body.len - 1] == 65'),
]


def run():
    subprocess.run(['python3', str(S / 'tests' / 'gen_corpus.py')],
                   capture_output=True, text=True, timeout=120)
    try:
        r = subprocess.run([G, 'run', str(S / 'build' / 'gate.goose')],
                           capture_output=True, text=True, timeout=280, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'run timeout'
    if r.returncode != 0:
        tail = (r.stderr or r.stdout).strip().splitlines()
        return 1, 'run failed: ' + (tail[-1][:120] if tail else 'no output')
    got = r.stdout
    want = (S / 'build' / 'native.out').read_text()
    if got == want:
        return 0, 'identical'
    diff = sum(1 for a, b in zip(got.splitlines(), want.splitlines()) if a != b)
    return 1, f'{diff} differing lines'


lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
caught = 0
missed = []
try:
    for idx, (label, old, new) in enumerate(M[lo:hi], start=lo):
        if ORIG.count(old) != 1:
            print(f'[skip] {idx} {label}: anchor count {ORIG.count(old)}')
            continue
        SRC.write_text(ORIG.replace(old, new, 1))
        rc, detail = run()
        ok = rc != 0
        caught += ok
        if not ok:
            missed.append(idx)
        print(f'[{"caught" if ok else "MISSED"}] {idx} {label}: {detail}')
        SRC.write_text(ORIG)
finally:
    SRC.write_text(ORIG)
    rc, detail = run()
    print(f'baseline after restore: rc={rc} {detail}')
print(f'mutations {caught}/{hi-lo} caught (range {lo}:{hi}) missed={missed}')
