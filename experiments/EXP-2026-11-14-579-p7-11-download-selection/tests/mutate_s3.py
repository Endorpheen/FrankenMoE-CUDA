#!/usr/bin/env python3
# Mutation run for the download selection slice (S3).
# Each mutation edits the Goose port, regenerates the gate driver, runs it with
# `goose run` and compares with the native oracle. Any diff, run failure or
# missing output counts as caught. Usage: mutate_s3.py [lo hi]
from pathlib import Path
import subprocess
import sys

S = Path(__file__).resolve().parent.parent
SRC = S / 'source' / 'download-selection.goose'
ORIG = SRC.read_text()
G = '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose'

M = [
    # ---- half 1: split names, tag, quant bits, parts -----------------------
    ('the .gguf suffix test changes case', 'if !d_ends(name, ".gguf") {', 'if !d_ends(name, ".GGUF") {'),
    ('the split marker byte is wrong', 'var ok: bool = name[at] == 45;', 'var ok: bool = name[at] == 46;'),
    ('the split suffix offset is one byte short', 'let at: i64 = p_end - 15;', 'let at: i64 = p_end - 14;'),
    ('the -of- marker is misspelled', 'name[at + 7] == 111', 'name[at + 7] == 112'),
    ('only the dash introduces a tag',
     'if te > 0 && (name[te - 1] == 45 || name[te - 1] == 46) {',
     'if te > 0 && (name[te - 1] == 45) {'),
    ('the tag is not upper-cased', 'tag_out[k] = d_upper(name[te + k]);', 'tag_out[k] = name[te + k];'),
    ('quant bits start at the tag start', 'let v = d_digit_value(tag, at);', 'let v = d_digit_value(tag, 0);'),
    ('the part name marker is misspelled', 'p = d_puts(out, p, "-of-");', 'p = d_puts(out, p, "-0f-");'),
    # ---- half 2: selection, sidecars, repo tag, resume ---------------------
    ('the first default tag is Q5_K_M',
     'var r = d_scan_tag(blob, off, len, n, "Q4_K_M");',
     'var r = d_scan_tag(blob, off, len, n, "Q5_K_M");'),
    ('a split part with index 2 is accepted',
     '''        if d_filename_is_model(path) != 0 && d_contains_tag(path, tag) {
            var info: i64[6] = [0, 0, 0, 0, 1, 1];
            var tb: u8[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

            d_split_info(path, info, tb);
            if info[5] <= 1 || info[4] == 1 {
                return j;
            }
        }''',
     '''        if d_filename_is_model(path) != 0 && d_contains_tag(path, tag) {
            var info: i64[6] = [0, 0, 0, 0, 1, 1];
            var tb: u8[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

            d_split_info(path, info, tb);
            if info[5] <= 1 || info[4] == 2 {
                return j;
            }
        }'''),
    ('imatrix files count as models', 'if d_contains(base, "imatrix") {', 'if d_contains(base, "imatrixx") {'),
    ('the mmproj sidecar keyword never matches', 'if d_contains(name, "mmproj") ||', 'if d_contains(name, "xmproj") ||'),
    ('the plan keeps the absent sidecars', 'if present[i] != 0 {', 'if present[i] == 0 {'),
    ('the repo tag starts after the first colon', 'out[2] = lastc + 1;', 'out[2] = first + 1;'),
    ('200 does not drop the partial', 'if status == 416 || status == 200 {', 'if status == 416 {'),
    ('a range request is sent at offset zero',
     'if supports_ranges != 0 && downloaded > 0 {',
     'if supports_ranges != 0 && downloaded >= 0 {'),
]


def run():
    subprocess.run(['python3', str(S / 'tests' / 'gen_s3.py')],
                   capture_output=True, text=True, timeout=120)
    try:
        r = subprocess.run([G, 'run', str(S / 'build' / 'gate_s3.goose')],
                           capture_output=True, text=True, timeout=280, errors='replace')
    except subprocess.TimeoutExpired:
        return 1, 'run timeout'
    if r.returncode != 0:
        tail = (r.stderr or r.stdout).strip().splitlines()
        return 1, 'run failed: ' + (tail[-1][:120] if tail else 'no output')
    got = r.stdout
    want = (S / 'build' / 'native_s3.out').read_text()
    if got == want:
        return 0, 'identical'
    diff = sum(1 for a, b in zip(got.splitlines(), want.splitlines()) if a != b)
    diff += abs(len(got.splitlines()) - len(want.splitlines()))
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
