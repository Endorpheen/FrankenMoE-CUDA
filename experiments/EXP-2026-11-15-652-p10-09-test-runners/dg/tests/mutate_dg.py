#!/usr/bin/env python3
# Mutation run for the benchmark diagnostics (P10-09 S3).
#
# Each mutation edits one decision in source/dg-diagnostics.goose, rebuilds the
# driver and re-compares with the native rows (build/native_dg.tsv, produced by
# executing the real analyze()). Any row mismatch or crash is a catch.
#
#   mutate_dg.py 0 6     # half 1: parsing and statistics
#   mutate_dg.py 6 12    # half 2: fallbacks and the report rows
import subprocess
import sys
from pathlib import Path

S = Path(__file__).resolve().parent.parent
SRC = S / 'source' / 'dg-diagnostics.goose'
ORIG = SRC.read_text()

M = [
    # ---- half 1: parsing and statistics -----------------------------------
    ('the wall time is read from the wrong column',
     'let wn: i64 = dg_field(line, 2, fbuf);',
     'let wn: i64 = dg_field(line, 1, fbuf);'),
    ('a row with an unparsable wall is kept',
     '                    if ok[0] != 0 {\n                        walls[walls_n] = w;',
     '                    if ok[0] == 0 {\n                        walls[walls_n] = w;'),
    ('the aggregate mean divides by the positive walls',
     'walls_n as f64 / total_s',
     'n as f64 / total_s'),
    ('an even count takes the lower middle value',
     'return (values[n / 2 - 1] + values[n / 2]) / 2.0;',
     'return values[n / 2 - 1];'),
    ('p5 and p95 swap places',
     'out[DG_P95] = dg_milli(dg_percentile(tps, n, 950));',
     'out[DG_P95] = dg_milli(dg_percentile(tps, n, 50));'),
    ('the per-token read divides by the positive walls',
     'out[DG_READ_TOK] = dg_milli(read_mib / walls_n as f64);',
     'out[DG_READ_TOK] = dg_milli(read_mib / n as f64);'),
    # ---- half 2: fallbacks and rows ---------------------------------------
    ('-1.0 cache hit reads as a measurement',
     'if raw.len == 0 || dg_eq(raw, "-") || dg_eq(raw, "-1.0") {',
     'if raw.len == 0 || dg_eq(raw, "-") {'),
    ('an absent cache hit prints as zero',
     'if raw.len == 0 || dg_eq(raw, "-") || dg_eq(raw, "-1.0") {\n        return dg_put_s(dst, at, "-");\n    }',
     'if raw.len == 0 || dg_eq(raw, "-") || dg_eq(raw, "-1.0") {\n        return dg_put_s(dst, at, "0%");\n    }'),
    ('an absent stall prints as zero',
     '    if fields[DG_STALL] < 0 {\n        p = dg_put_s(out, p, "-");',
     '    if fields[DG_STALL] < 0 {\n        p = dg_fmt(out, p, 0, 1);'),
    ('the sizing table requires both budget and prefetch',
     'if fields[DG_BUDGET] > 0 || fields[DG_SPEC_READ] > 0 {',
     'if fields[DG_BUDGET] > 0 && fields[DG_SPEC_READ] > 0 {'),
    ('the sizing table hides the prefetch usefulness',
     'if fields[DG_SPEC_READ] > 0 {\n        p = dg_fmt(out, p, fields[DG_SPEC_USEFUL], 0);',
     'if fields[DG_SPEC_READ] < 0 {\n        p = dg_fmt(out, p, fields[DG_SPEC_USEFUL], 0);'),
    ('two-decimal values lose their zero padding',
     'if dec == 2 && fr < 10 {',
     'if dec == 2 && fr < 0 {'),
]


def main():
    lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
    caught = 0
    for idx in range(lo, min(hi, len(M))):
        name, find, repl = M[idx]
        if ORIG.count(find) != 1:
            print('%-3d SETUP  %-52s (pattern occurs %d times)' % (idx, name, ORIG.count(find)))
            continue
        SRC.write_text(ORIG.replace(find, repl))
        try:
            r = subprocess.run(['python3', str(S / 'tests' / 'gen_dg.py')],
                               capture_output=True, text=True, timeout=600)
            ok = r.returncode != 0
        finally:
            SRC.write_text(ORIG)
        if ok:
            caught += 1
            first = [l for l in (r.stdout + r.stderr).splitlines() if l.startswith('FAIL')]
            print('%-3d caught %-52s %s' % (idx, name, first[0].strip()[:58] if first else ''))
        else:
            print('%-3d MISSED %-52s' % (idx, name))
    total = min(hi, len(M)) - lo
    print('caught %d/%d' % (caught, total))
    return 0 if caught == total else 1


if __name__ == '__main__':
    sys.exit(main())
