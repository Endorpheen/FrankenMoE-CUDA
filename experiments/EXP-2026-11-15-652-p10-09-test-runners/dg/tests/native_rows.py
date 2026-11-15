#!/usr/bin/env python3
# Native side of the benchmark-diagnostics gate (P10-09 S3).
#
# Executes the real analyze() from tests/native/bench_analyze.py (a byte-identical
# copy of scripts/bench-analyze.py) on every synthetic case and prints the same
# normalised rows the port has to produce:
#
#   A <case> <23 integer fields in DG_* order>
#   T <case> <throughput row text>
#   H <case> <cache-hit text>
#   S <case> <sized flag>
#   R <case> <sized row text>
#
# Nothing is simulated: the numbers come from the native function, quantised the
# same way the port quantises them (round to thousandths, ties to even). The row
# text is rendered from those integers on both sides, so the comparison does not
# depend on either side's float formatting.
#
# Usage: native_rows.py <cases-dir> <out.tsv>
import sys
from pathlib import Path

S = Path(__file__).resolve().parent.parent
NATIVE = S / 'tests' / 'native'


def load_analyze():
    src = (NATIVE / 'bench_analyze.py').read_text()
    head = src.split('\nmd = []\n')[0]
    g = {'__name__': 'bench_native', '__file__': str(NATIVE / 'bench_analyze.py')}
    exec(compile(head, str(NATIVE / 'bench_analyze.py'), 'exec'), g)
    return g['analyze']


def fmt(milli, dec):
    neg = milli < 0
    x = -milli if neg else milli
    s = '-' if neg else ''
    if dec == 0:
        return s + str((x + 500) // 1000)
    ip = x // 1000
    if dec == 1:
        fr = (x % 1000) // 100
        return '%s%d.%d' % (s, ip, fr)
    fr = (x % 1000) // 10
    return '%s%d.%02d' % (s, ip, fr)


def hit_text(raw):
    if not raw or raw in ('-', '-1.0'):
        return '-'
    try:
        return '%d%%' % round(float(raw))
    except ValueError:
        return '-'


def main():
    cases = Path(sys.argv[1]) if len(sys.argv) > 1 else S / 'build' / 'cases'
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else S / 'build' / 'native_dg.tsv'
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from make_cases import CASES
    analyze = load_analyze()
    rows = []
    for name, spec in CASES.items():
        r = analyze(str(cases / ('%s.csv' % name)))
        label = spec['label']
        if r is None:
            rows.append('A %s empty' % name)
            rows.append('T %s -' % name)
            rows.append('H %s -' % name)
            rows.append('S %s 0' % name)
            continue
        f = [
            r['n'],
            round(r['mean'] * 1000), round(r['min'] * 1000), round(r['max'] * 1000),
            round(r['median'] * 1000), round(r['p5'] * 1000), round(r['p95'] * 1000),
            round(r['read_MiB_tok'] * 1000),
            -1 if r['stall_ms_tok'] is None else round(r['stall_ms_tok'] * 1000),
            -1 if r['mgmt_ms_tok'] is None else round(r['mgmt_ms_tok'] * 1000),
            round(r['ttft'] * 1000), round(r['prefill_tps'] * 1000),
            round(r['load_s'] * 1000),
            round(r['cache_budget_MiB'] * 1000), round(r['cache_resident_MiB'] * 1000),
            round(r['spec_read_MiB_tok'] * 1000), round(r['spec_useful_pct'] * 1000),
            round(r['peak_rss_gb'] * 1000), round(r['mem_floor_gb'] * 1000),
            round(r['cpu0'] * 1000), round(r['cpu_max'] * 1000),
            round(r['batt_max'] * 1000),
        ]
        rows.append('A %s %s' % (name, ' '.join(str(v) for v in f)))
        row = '| %s | **%s** | %s | %s | %s | %s | %s | %s | %s | %s MiB | %s |' % (
            label, fmt(f[1], 2), fmt(f[2], 2), fmt(f[3], 2), fmt(f[4], 2), fmt(f[6], 2),
            fmt(f[11], 2), fmt(f[10], 1), hit_text(r['cache_hit']), fmt(f[7], 0),
            '-' if f[8] < 0 else fmt(f[8], 1))
        rows.append('T %s %s' % (name, row))
        rows.append('H %s %s' % (name, hit_text(r['cache_hit'])))
        sized = 1 if (r['cache_budget_MiB'] > 0 or r['spec_read_MiB_tok'] > 0) else 0
        rows.append('S %s %d' % (name, sized))
        if sized:
            budget = '%s MiB' % fmt(f[13], 0) if f[13] > 0 else '-'
            useful = '%s%%' % fmt(f[16], 0) if f[15] > 0 else '-'
            sread = '%s MiB' % fmt(f[15], 0) if f[15] > 0 else '-'
            rows.append('R %s | %s | %s | %s MiB | %s | %s |'
                        % (name, label, budget, fmt(f[14], 0), useful, sread))
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text('\n'.join(rows) + '\n')
    print('rows: %d' % len(rows))


if __name__ == '__main__':
    main()
