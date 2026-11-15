#!/usr/bin/env python3
# Differential gate for the benchmark diagnostics (P10-09 S3).
#
# tests/native_rows.py executes the real analyze() on every synthetic case and
# writes build/native_dg.tsv. This embeds the same CSV + .metrics texts in one
# Goose driver, runs the port at -O1 and -O0, and compares row by row:
#
#   A <case> <23 fields>   the analysis, in thousandths
#   T <case> <row>         the throughput markdown row
#   H <case> <text>        the cache-hit cell ("-" when not measured)
#   S <case> 0|1           whether the run belongs in the cache-sizing table
#   R <case> <row>         the cache-sizing row
#
# Usage: gen_dg.py [--rebuild]
import subprocess
import sys
from pathlib import Path

S = Path(__file__).resolve().parent.parent
PORT = S / 'source' / 'dg-diagnostics.goose'
BUILD = S / 'build'
GOOSE = '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose'


def gs(s):
    out = []
    for b in s.encode():
        if b == 92:
            out.append('\\\\')
        elif b == 34:
            out.append('\\"')
        elif b == 10:
            out.append('\\n')
        elif b == 13:
            out.append('\\r')
        elif 32 <= b < 127:
            out.append(chr(b))
        else:
            out.append('\\u{%02x}' % b)
    return '"' + ''.join(out) + '"'


DRIVER_HEAD = r'''
fn g_emit(buf: u8[:], n: i64) -> i64 {
    print(buf[0..n]);
    return n;
}

fn g_case(name: const u8[:], label: const u8[:], csv: const u8[:], metrics: const u8[:]) -> i64 {
    var fields: i64[32] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    let ok: i64 = dg_analyze(csv, metrics, fields);
    if ok == 0 {
        var e0: u8[256] = [%(z256)s];
        var p: i64 = 0;
        p = dg_put_s(e0, p, "A ");
        p = dg_put_s(e0, p, name);
        p = dg_put_s(e0, p, " empty");
        g_emit(e0, p);
        var t0: u8[256] = [%(z256)s];
        p = 0;
        p = dg_put_s(t0, p, "T ");
        p = dg_put_s(t0, p, name);
        p = dg_put_s(t0, p, " -");
        g_emit(t0, p);
        var h0: u8[128] = [%(z128)s];
        p = 0;
        p = dg_put_s(h0, p, "H ");
        p = dg_put_s(h0, p, name);
        p = dg_put_s(h0, p, " -");
        g_emit(h0, p);
        var s0: u8[128] = [%(z128)s];
        p = 0;
        p = dg_put_s(s0, p, "S ");
        p = dg_put_s(s0, p, name);
        p = dg_put_s(s0, p, " 0");
        return g_emit(s0, p);
    }
    var al: u8[512] = [%(z512)s];
    var p2: i64 = 0;
    p2 = dg_put_s(al, p2, "A ");
    p2 = dg_put_s(al, p2, name);
    var i: i64 = 0;
    while i < 22 {
        al[p2] = 32;
        p2 = p2 + 1;
        p2 = dg_put_int(al, p2, fields[i]);
        i = i + 1;
    }
    g_emit(al, p2);
    var hb: u8[64] = [%(z64)s];
    var hn: i64 = dg_hit_raw(csv, hb);
    if hn < 0 {
        hn = 0;
    }
    var hitbuf: u8[64] = [%(z64)s];
    var tl: u8[512] = [%(z512)s];
    var tp: i64 = 0;
    tp = dg_put_s(tl, tp, "T ");
    tp = dg_put_s(tl, tp, name);
    tp = dg_put_s(tl, tp, " ");
    tp = tp + dg_row(label, fields, hb[0..hn], tl[tp..511]);
    g_emit(tl, tp);
    var hl: u8[128] = [%(z128)s];
    var hp: i64 = 0;
    hp = dg_put_s(hl, hp, "H ");
    hp = dg_put_s(hl, hp, name);
    hp = dg_put_s(hl, hp, " ");
    hp = dg_hit(hl, hp, hb[0..hn]);
    g_emit(hl, hp);
    var szl: u8[128] = [%(z128)s];
    var sp: i64 = 0;
    sp = dg_put_s(szl, sp, "S ");
    sp = dg_put_s(szl, sp, name);
    szl[sp] = 32;
    sp = sp + 1;
    sp = dg_put_int(szl, sp, dg_sized(fields));
    g_emit(szl, sp);
    if dg_sized(fields) != 0 {
        var rl: u8[512] = [%(z512)s];
        var rp: i64 = 0;
        rp = dg_put_s(rl, rp, "R ");
        rp = dg_put_s(rl, rp, name);
        rp = dg_put_s(rl, rp, " ");
        rp = rp + dg_sized_row(label, fields, rl[rp..511]);
        g_emit(rl, rp);
    }
    return 0;
}
'''


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    native = BUILD / 'native_dg.tsv'
    if '--rebuild' in sys.argv or not native.exists():
        r = subprocess.run(['python3', str(S / 'tests' / 'make_cases.py')], capture_output=True, text=True)
        if r.returncode != 0:
            print('FAIL cases:', (r.stderr or r.stdout)[-400:])
            return 1
        r = subprocess.run(['python3', str(S / 'tests' / 'native_rows.py')], capture_output=True, text=True)
        if r.returncode != 0:
            print('FAIL native:', (r.stderr or r.stdout)[-600:])
            return 1
    sys.path.insert(0, str(S / 'tests'))
    from make_cases import CASES
    calls = []
    for name, spec in CASES.items():
        csv = (BUILD / 'cases' / ('%s.csv' % name)).read_text()
        met = (BUILD / 'cases' / ('%s.metrics' % name)).read_text()
        calls.append('    g_case("%s", %s,\n           %s,\n           %s);'
                     % (name, gs(spec['label']), gs(csv), gs(met)))
    subs = {'z64': ', '.join(['0'] * 64), 'z128': ', '.join(['0'] * 128), 'z256': ', '.join(['0'] * 256), 'z512': ', '.join(['0'] * 512)}
    driver = (PORT.read_text() + DRIVER_HEAD % subs + 'fn main() {\n'
              + '\n'.join(calls) + '\n}\n')
    gpath = BUILD / 'gate_dg.goose'
    gpath.write_text(driver)
    outs = {}
    for opt in ('-O1', '-O0'):
        run = subprocess.run([GOOSE, 'run', opt, str(gpath)], capture_output=True, text=True)
        if run.returncode != 0:
            print('FAIL goose %s: %s' % (opt, (run.stderr or run.stdout)[-900:]))
            return 1
        outs[opt] = run.stdout.splitlines()
    if outs['-O1'] != outs['-O0']:
        print('FAIL -O1 and -O0 disagree')
        return 1
    port_lines = [l for l in outs['-O1'] if l.strip()]
    (BUILD / 'goose_dg.tsv').write_text('\n'.join(port_lines) + '\n')
    native_lines = native.read_text().splitlines()
    bad = 0
    if len(native_lines) != len(port_lines):
        print('FAIL row count: native=%d port=%d' % (len(native_lines), len(port_lines)))
        bad += 1
    for i in range(max(len(native_lines), len(port_lines))):
        a = native_lines[i] if i < len(native_lines) else '<missing>'
        b = port_lines[i] if i < len(port_lines) else '<missing>'
        if a != b:
            bad += 1
            if bad <= 8:
                print('FAIL row %d:\n  native: %s\n  port:   %s' % (i, a[:220], b[:220]))
    kinds = {}
    for l in native_lines:
        kinds[l.split(' ')[0]] = kinds.get(l.split(' ')[0], 0) + 1
    print('rows: %d (%s)' % (len(native_lines), ', '.join('%s=%d' % kv for kv in sorted(kinds.items()))))
    print('gate: %d/%d rows identical' % (len(native_lines) - bad, len(native_lines)))
    return 0 if bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
