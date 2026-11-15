#!/usr/bin/env python3
# Differential gate for the tiny-MoE fixture plan (P10-09 S2).
#
# tests/capture_fx.py runs the real generator with recording stand-ins and
# writes build/native_fx.tsv. This generates one Goose driver from the port and
# compares the two files line by line, at -O1 and -O0.
#
# Usage: gen_fx.py [--rebuild]
import subprocess
import sys
from pathlib import Path

S = Path(__file__).resolve().parent.parent
PORT = S / 'source' / 'fx-fixture-plan.goose'
BUILD = S / 'build'
GOOSE = '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose'

BUILDS = [
    ('qwen3moe', 'FX_QWEN', '/tmp/out/tiny-moe.gguf', 0, 1),
    ('gemma4', 'FX_GEMMA', '/tmp/out/tiny-moe-gemma4.gguf', 0, 1),
    ('qwen3moe-split', 'FX_QWEN', '/tmp/out/tiny-moe-split.gguf', 20, 0),
]

DRIVER = r'''
fn fx_emit(buf: u8[:], n: i64) -> i64 {
    print(buf[0..n]);
    return n;
}

// One build: the writer construction, then the metadata calls, then the tensor
// plan, then the banner. `full` is 0 for the split build, where the gate only
// needs the writer arguments and the banner (the tensor plan is identical).
fn fx_build(name: const u8[:], arch: i64, arch_name: const u8[:], path: const u8[:], split: i64, full: i64) -> i64 {
    var wl: u8[512] = [%(z512)s];
    var wp: i64 = 0;
    wp = fx_put_s(wl, wp, "W ");
    wp = fx_put_s(wl, wp, name);
    wl[wp] = 32;
    wp = wp + 1;
    wp = fx_put_s(wl, wp, path);
    wp = fx_put_s(wl, wp, " ");
    wp = fx_put_s(wl, wp, arch_name);
    wp = fx_put_s(wl, wp, " split=");
    wp = fx_put_int(wl, wp, split);
    wp = fx_put_s(wl, wp, " small_first=");
    wp = fx_put_int(wl, wp, fx_small_first(split));
    fx_emit(wl, wp);
    if full == 0 {
        var b0: u8[512] = [%(z512)s];
        var p0: i64 = 0;
        p0 = fx_put_s(b0, p0, "B ");
        p0 = fx_put_s(b0, p0, name);
        p0 = fx_put_s(b0, p0, " ");
        p0 = p0 + fx_banner(arch, path, b0[p0..511]);
        return fx_emit(b0, p0);
    }
    var mr: u8[4096] = [%(z4096)s];
    var ml: u8[4200] = [%(z4200)s];
    var i: i64 = 0;
    let nm: i64 = fx_meta_count(arch);
    while i < nm {
        let mrn: i64 = fx_meta(arch, i, mr);
        var mp: i64 = 0;
        mp = fx_put_s(ml, mp, "M ");
        mp = fx_put_s(ml, mp, name);
        ml[mp] = 32;
        mp = mp + 1;
        mp = fx_put_int(ml, mp, i);
        ml[mp] = 32;
        mp = mp + 1;
        mp = fx_put_s(ml, mp, mr[0..mrn]);
        fx_emit(ml, mp);
        i = i + 1;
    }
    var tl: u8[512] = [%(z512)s];
    i = 0;
    let nt: i64 = fx_tensor_count(arch);
    while i < nt {
        var tp: i64 = 0;
        tp = fx_put_s(tl, tp, "T ");
        tp = fx_put_s(tl, tp, name);
        tl[tp] = 32;
        tp = tp + 1;
        tp = fx_put_int(tl, tp, i);
        tl[tp] = 32;
        tp = tp + 1;
        tp = tp + fx_tensor(arch, i, tl[tp..511]);
        fx_emit(tl, tp);
        i = i + 1;
    }
    var bl: u8[512] = [%(z512)s];
    var bp: i64 = 0;
    bp = fx_put_s(bl, bp, "B ");
    bp = fx_put_s(bl, bp, name);
    bp = fx_put_s(bl, bp, " ");
    bp = bp + fx_banner(arch, path, bl[bp..511]);
    return fx_emit(bl, bp);
}

fn main() {
%(calls)s
    var g: u8[512] = [%(z512)s];
    let r: i64 = fx_split_guard(FX_GEMMA, 20, g);
    var gl: u8[512] = [%(z512)s];
    var gp: i64 = 0;
    gp = fx_put_s(gl, gp, "G gemma4 ");
    if r != 0 {
        gp = fx_put_s(gl, gp, "refused ");
        var gn: i64 = 0;
        while gn < 512 {
            if g[gn] == 0 {
                gn = 512;
            } else {
                gl[gp] = g[gn];
                gp = gp + 1;
                gn = gn + 1;
            }
        }
    } else {
        gp = fx_put_s(gl, gp, "accepted");
    }
    fx_emit(gl, gp);
}
''' % {
    'z512': ', '.join(['0'] * 512),
    'z4096': ', '.join(['0'] * 4096),
    'z4200': ', '.join(['0'] * 4200),
    'calls': '\n'.join(
        '    fx_build("%s", %s, "%s", "%s", %d, %d);' % (name, arch, name.split('-')[0], path, split, full)
        for name, arch, path, split, full in BUILDS),
}


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    native = BUILD / 'native_fx.tsv'
    if '--rebuild' in sys.argv or not native.exists():
        r = subprocess.run(['python3', str(S / 'tests' / 'capture_fx.py'), str(native)],
                           capture_output=True, text=True)
        if r.returncode != 0:
            print('FAIL native capture:', (r.stderr or r.stdout)[-500:])
            return 1
    native_lines = native.read_text().splitlines()
    gpath = BUILD / 'gate_fx.goose'
    gpath.write_text(PORT.read_text() + DRIVER)
    outs = {}
    for opt in ('-O1', '-O0'):
        run = subprocess.run([GOOSE, 'run', opt, str(gpath)], capture_output=True, text=True)
        if run.returncode != 0:
            print('FAIL goose %s: %s' % (opt, (run.stderr or run.stdout)[-800:]))
            return 1
        outs[opt] = run.stdout.splitlines()
    if outs['-O1'] != outs['-O0']:
        print('FAIL -O1 and -O0 disagree')
        return 1
    port_lines = [l for l in outs['-O1'] if l.strip()]
    (BUILD / 'goose_fx.tsv').write_text('\n'.join(port_lines) + '\n')
    bad = 0
    if len(native_lines) != len(port_lines):
        print('FAIL row count: native=%d port=%d' % (len(native_lines), len(port_lines)))
        bad += 1
    for i in range(max(len(native_lines), len(port_lines))):
        a = native_lines[i] if i < len(native_lines) else '<missing>'
        b = port_lines[i] if i < len(port_lines) else '<missing>'
        if a != b:
            bad += 1
            if bad <= 6:
                print('FAIL row %d:\n  native: %s\n  port:   %s' % (i, a[:150], b[:150]))
    groups = {}
    for l in native_lines:
        groups[l.split(' ')[0]] = groups.get(l.split(' ')[0], 0) + 1
    print('rows: %d (%s)' % (len(native_lines), ', '.join('%s=%d' % kv for kv in sorted(groups.items()))))
    print('gate: %d/%d rows identical' % (len(native_lines) - bad, len(native_lines)))
    return 0 if bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
