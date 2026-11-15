#!/usr/bin/env python3
# Differential gate for the test-runner slice (P10-09 S1).
#
# For every scenario this writes a CMake project, lets the real CTest decide
# (selection, fixture order, skips, summary, exit code), then generates a Goose
# driver that feeds the SAME definitions and the SAME process return codes into
# the port and compares the decisions.
#
#   build/<scen>/             the CMake project + ctest JUnit output
#   build/gate_<scen>.goose    the port plus the driver
#   build/native.json          the cached native truth (rebuild with --rebuild)
#
# The driver prints, one per line:  R<row>  D<id> <name>  S<summary>  X<exit>
# and the comparison below maps both sides onto the same status words.
#
# Usage: gen_tr.py [--rebuild]
import json
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

S = Path(__file__).resolve().parent.parent
PORT = S / 'source' / 'tr-testplan.goose'
BUILD = S / 'build'
GOOSE = '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose'

HEAD = """cmake_minimum_required(VERSION 3.20)
project(tr_gate NONE)
enable_testing()
"""


def T(name, rc=0, **props):
    d = {'name': name, 'rc': rc}
    d.update(props)
    return d


SCEN = {
    'basic': dict(tests=[T('alpha'), T('beta', 1), T('gamma')]),
    'all_skip': dict(tests=[T('s1', 1, skip=1), T('s2', 1, skip=1)]),
    'skip_mixed': dict(tests=[T('s1', 1, skip=1), T('p1'), T('s2', 1, skip=1)]),
    'disabled_mixed': dict(tests=[T('ok'), T('off', disabled=1), T('bad', 1)]),
    'fixtures_ok': dict(tests=[
        T('use1', required=1), T('setup', setup=1), T('use2', required=1), T('clean', cleanup=1)]),
    'setup_fail': dict(tests=[T('setup', 1, setup=1), T('use', required=1), T('clean', cleanup=1)]),
    'setup_skip': dict(tests=[T('setup', 1, setup=1, skip=1), T('use', required=1)]),
    'two_fixtures': dict(tests=[
        T('setupA', setup=1), T('setupB', setup=2), T('useA', required=1),
        T('useB', required=2), T('cleanB', cleanup=2), T('cleanA', cleanup=1)]),
    'nested_fixture': dict(tests=[
        T('use', required=2), T('setupB', setup=2, required=1), T('setupA', setup=1)]),
    'notrun_chain': dict(tests=[
        T('setup', 1, setup=3), T('a', required=3), T('b', required=3),
        T('pass'), T('clean', cleanup=3)]),
    'filter_include': dict(tests=[T('setup', setup=1), T('use', required=1), T('other')],
                           args=['-R', 'use|setup']),
    'filter_exclude': dict(tests=[T('keep'), T('skipme', 1, skip=1), T('keep2')],
                           args=['-E', 'skipme']),
    'filter_pull_setup': dict(tests=[T('setup', setup=1), T('use', required=1), T('other')],
                              args=['-R', 'use']),
    'filter_classes': dict(tests=[T('t0'), T('t1', 1), T('t22'), T('x9', 1), T('t')],
                           args=['-R', '^t[0-9]+$']),
    'filter_dot': dict(tests=[T('abc'), T('axc'), T('ac')], args=['-R', 'a.c']),
    'filter_neg_class': dict(tests=[T('ax'), T('ab'), T('bx')], args=['-R', '^[^x]']),
    'percent_round': dict(tests=[T('t0')] + [T('t%d' % i, 1) for i in range(1, 8)]),
    'fixture_two_deps': dict(tests=[
        T('setup', setup=4), T('usea', required=4), T('mid'), T('useb', required=4),
        T('clean', cleanup=4)]),
    'notrun_and_skip': dict(tests=[
        T('setup', 1, setup=2), T('dep', required=2), T('s', 1, skip=1), T('ok')]),
    'cleanup_first': dict(tests=[
        T('cleanA', cleanup=1), T('cleanB', cleanup=2), T('setupA', setup=1),
        T('setupB', setup=2), T('useA', required=1), T('useB', required=2)]),
    'cleanup_multi': dict(tests=[
        T('setupA', setup=1), T('setupB', setup=2), T('useA', required=1),
        T('useB', required=2), T('cleanBoth', cleanup=1, cleanup2=2),
        T('cleanA2', cleanup=1)]),
    'filter_setup_only': dict(tests=[T('setup', setup=1), T('use', required=1),
                                     T('clean', cleanup=1)], args=['-R', 'setup']),
    'filter_pull_cleanup': dict(tests=[T('setup', setup=1), T('use', required=1),
                                       T('clean', cleanup=1), T('other')], args=['-R', 'use']),
}


def write_cmake(spec, d):
    lines = [HEAD]
    for t in spec['tests']:
        lines.append("add_test(NAME %s COMMAND ${CMAKE_COMMAND} -E %s)\n"
                     % (t['name'], 'true' if t['rc'] == 0 else 'false'))
        props = []
        if t.get('disabled'):
            props.append('DISABLED TRUE')
        if t.get('skip') is not None:
            props.append('SKIP_RETURN_CODE %d' % t['skip'])
        if t.get('setup'):
            props.append('FIXTURES_SETUP FX%d' % t['setup'])
        if t.get('required'):
            props.append('FIXTURES_REQUIRED FX%d' % t['required'])
        cleanups = []
        if t.get('cleanup'):
            cleanups.append('FX%d' % t['cleanup'])
        if t.get('cleanup2'):
            cleanups.append('FX%d' % t['cleanup2'])
        if cleanups:
            props.append('FIXTURES_CLEANUP "%s"' % ';'.join(cleanups))
        if props:
            lines.append("set_tests_properties(%s PROPERTIES %s)\n" % (t['name'], ' '.join(props)))
    d.mkdir(parents=True, exist_ok=True)
    (d / 'CMakeLists.txt').write_text(''.join(lines))


def parse_junit(path):
    root = ET.parse(path).getroot()
    by_name = {}
    for tc in root.iter('testcase'):
        name = tc.get('name')
        status = tc.get('status')
        msg = ''
        for ch in tc:
            if ch.tag == 'skipped':
                msg = ch.get('message') or ''
        if status == 'run':
            st = 'Passed'
        elif status == 'fail':
            st = 'Failed'
        elif status == 'disabled':
            st = 'Disabled'
        elif msg.startswith('SKIP_RETURN_CODE'):
            st = 'Skipped'
        else:
            st = 'Not Run'
        by_name[name] = st
    return by_name


def parse_progress(stdout, names_by_id):
    """Execution order and the status words from ctest's own progress lines."""
    order = []
    status = {}
    for line in stdout.splitlines():
        m = re.search(r'Test\s+#(\d+): (\S+)', line)
        if not m:
            continue
        nm = m.group(2)
        tail = line[m.end():]
        if 'Not Run (Disabled)' in tail:
            st = 'Disabled'
        elif 'Not Run' in tail:
            st = 'Not Run'
        elif 'Skipped' in tail:
            st = 'Skipped'
        elif 'Failed' in tail:
            st = 'Failed'
        elif 'Passed' in tail:
            st = 'Passed'
        else:
            st = '?'
        order.append(nm)
        status[nm] = st
    return order, status


def z(n, v='0'):
    return ", ".join([v] * n)


def arr(vals):
    return ", ".join(str(v) for v in vals)


def gs(s):
    out = []
    for b in s.encode():
        if b == 92:
            out.append('\\\\')
        elif b == 34:
            out.append('\\"')
        elif 32 <= b < 127:
            out.append(chr(b))
        else:
            out.append('\\u{%02x}' % b)
    return '"' + ''.join(out) + '"'


def gen_driver(spec, offs, blob):
    tests = spec['tests']
    n = len(tests)
    args = spec.get('args', [])
    filt, excl = '', ''
    i = 0
    while i < len(args):
        if args[i] == '-R':
            filt = args[i + 1]
            i += 2
        elif args[i] == '-E':
            excl = args[i + 1]
            i += 2
        else:
            i += 1
    port = PORT.read_text()
    driver = '''
fn name_of(blob: const u8[:], off: i64, len: i64) -> const u8[:] {
    return blob[off..off + len];
}

fn main() {
    var blob: u8[%(blob_n)d] = [%(blob)s];
    var off: i64[%(n)d] = [%(off)s];
    var len: i64[%(n)d] = [%(len)s];
    var setup: i64[%(n)d] = [%(setup)s];
    var req: i64[%(n)d] = [%(req)s];
    var clean: i64[%(n)d] = [%(clean)s];
    var disabled: i64[%(n)d] = [%(disabled)s];
    var skiprc: i64[%(n)d] = [%(skiprc)s];
    var hasskip: i64[%(n)d] = [%(hasskip)s];
    var rc: i64[%(n)d] = [%(rc)s];
    var hasrc: i64[%(n)d] = [%(hasrc)s];
    var order: i64[64] = [%(z64)s];
    var n_out: i64[2] = [0, 0];
    var fx: i64[30] = [%(fx0)s];
    var status: i64[64] = [%(m1)s];
    var ndis: i64 = 0;
    var i: i64 = 0;
    while i < %(n)d {
        if disabled[i] != 0 { ndis = ndis + 1; }
        i = i + 1;
    }
    let rep = tr_plan(off, len, blob, %(n)d, setup, req, clean, disabled, %(filt)s, %(excl)s, order, n_out);
    let total: i64 = rep;
    var nf: i64 = 0;
    var nn: i64 = 0;
    var pos: i64 = 0;
    while pos < n_out[0] {
        let ti = order[pos];
        var blocked = tr_dep_blocked(req[ti], fx);
        var hc = hasrc[ti];
        if blocked != 0 { hc = 0; }
        let st = tr_status(rc[ti], hc, skiprc[ti], hasskip[ti], blocked, disabled[ti]);
        status[ti] = st;
        if st == TR_FAIL { nf = nf + 1; }
        if st == TR_NOTRUN { nn = nn + 1; }
        var f: i64 = 0;
        while f < 30 {
            if tr_has(setup[ti], f) { fx[f] = tr_fx_state(st); }
            f = f + 1;
        }
        var rl: u8[256] = [%(z256)s];
        rl[0] = 82;
        let p = tr_row(pos + 1, total, ti + 1, name_of(blob, off[ti], len[ti]), st, rl[1..255]);
        print(rl[0..p + 1]);
        pos = pos + 1;
    }
    i = 0;
    while i < %(n)d {
        if disabled[i] != 0 {
            var dl: u8[256] = [%(z256)s];
            var dp: i64 = 1;
            dl[0] = 68;
            dp = tr_put_int(dl, dp, i + 1);
            dl[dp] = 32;
            dp = dp + 1;
            dp = tr_put_s(dl, dp, name_of(blob, off[i], len[i]));
            print(dl[0..dp]);
        }
        i = i + 1;
    }
    var sl: u8[128] = [%(z128)s];
    var code: i64[2] = [0, 0];
    let slen = tr_summary(total, ndis, nf, nn, sl[1..127], code);
    sl[0] = 83;
    print(sl[0..1 + slen]);
    var xl: u8[32] = [%(z32)s];
    var xp: i64 = 1;
    xl[0] = 88;
    xp = tr_put_int(xl, xp, code[0]);
    print(xl[0..xp]);
}
''' % dict(blob_n=max(1, len(blob)), blob=arr(blob), n=n, off=arr(offs),
           len=arr([len(t['name'].encode()) for t in tests]),
           setup=arr([(1 << t['setup']) if t.get('setup') else 0 for t in tests]),
           req=arr([(1 << t['required']) if t.get('required') else 0 for t in tests]),
           clean=arr([((1 << t['cleanup']) if t.get('cleanup') else 0)
                      | ((1 << t['cleanup2']) if t.get('cleanup2') else 0) for t in tests]),
           disabled=arr([1 if t.get('disabled') else 0 for t in tests]),
           skiprc=arr([t.get('skip', 0) for t in tests]),
           hasskip=arr([1 if t.get('skip') is not None else 0 for t in tests]),
           rc=arr([t['rc'] for t in tests]),
           hasrc=arr([1 for _ in tests]),
           z64=z(64), fx0=z(30, '2'), m1=z(64, '-1'), z256=z(256), z128=z(128), z32=z(32),
           filt=gs(filt) if filt else '""', excl=gs(excl) if excl else '""')
    return port + driver


def build_native(name, spec):
    """Native truth for one scenario: its own CMake project and a real ctest."""
    d = BUILD / name
    if d.exists():
        shutil.rmtree(d)
    write_cmake(spec, d)
    r = subprocess.run(['cmake', '-S', str(d), '-B', str(d / 'b')], capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError('cmake: ' + r.stderr[-400:])
    junit = d / 'j.xml'
    cargs = ['ctest', '--output-junit', str(junit)] + spec.get('args', [])
    r = subprocess.run(cargs, cwd=str(d / 'b'), capture_output=True, text=True)
    summ = ''
    for line in r.stdout.splitlines():
        if 'tests passed,' in line and 'out of' in line:
            summ = line.strip()
    by_name = parse_junit(junit)
    order, status = parse_progress(r.stdout, None)
    return {'status': by_name, 'order': order, 'progress': status,
            'summary': summ, 'exit': r.returncode}


def run_port(name, spec):
    blob = bytearray()
    offs = []
    for t in spec['tests']:
        offs.append(len(blob))
        blob += t['name'].encode()
    g = BUILD / ('gate_%s.goose' % name)
    g.write_text(gen_driver(spec, offs, blob))
    outs = {}
    for opt in ('-O1', '-O0'):
        run = subprocess.run([GOOSE, 'run', opt, str(g)], capture_output=True, text=True)
        if run.returncode != 0:
            raise RuntimeError('goose %s: %s' % (opt, (run.stderr or run.stdout)[-500:]))
        outs[opt] = [l for l in run.stdout.splitlines() if l.strip()]
    if outs['-O1'] != outs['-O0']:
        raise RuntimeError('the two optimisation levels disagree')
    rows = []
    disabled = []
    summary = ''
    exit_code = None
    for line in outs['-O1']:
        if line.startswith('R'):
            m = re.search(r'Test #(\d+): (\S+)\s', line)
            if not m:
                raise RuntimeError('unparsable row %r' % line)
            tail = line[m.end():]
            if 'Not Run (Disabled)' in tail:
                st = 'Disabled'
            elif 'Not Run' in tail:
                st = 'Not Run'
            elif 'Skipped' in tail:
                st = 'Skipped'
            elif 'Failed' in tail:
                st = 'Failed'
            elif 'Passed' in tail:
                st = 'Passed'
            else:
                st = '?'
            rows.append((m.group(2), st))
        elif line.startswith('D'):
            m = re.match(r'D(\d+) (\S+)', line)
            disabled.append(m.group(2))
        elif line.startswith('S'):
            summary = line[1:]
        elif line.startswith('X'):
            exit_code = int(line[1:])
    return rows, disabled, summary, exit_code


def compare(native, port):
    rows, disabled, summary, exit_code = port
    problems = []
    exp = native['status']
    for nm, st in rows:
        if exp.get(nm) != st:
            problems.append('status %s: native=%s port=%s' % (nm, exp.get(nm), st))
    got = dict(rows)
    for nm, st in exp.items():
        if nm not in got:
            problems.append('test %s missing from the run order' % nm)
    for nm in disabled:
        if exp.get(nm) != 'Disabled':
            problems.append('disabled row %s: native=%s' % (nm, exp.get(nm)))
    port_order = [nm for nm, st in rows]
    if native['order'] != port_order:
        problems.append('order: native=%s port=%s' % (native['order'], port_order))
    if native['summary'] != summary:
        problems.append('summary: native=%r port=%r' % (native['summary'], summary))
    if native['exit'] != exit_code:
        problems.append('exit: native=%d port=%d' % (native['exit'], exit_code))
    return problems


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    cache_path = BUILD / 'native.json'
    rebuild = '--rebuild' in sys.argv or not cache_path.exists()
    cache = {} if rebuild else json.loads(cache_path.read_text())
    bad = 0
    for name, spec in SCEN.items():
        if name not in cache:
            try:
                cache[name] = build_native(name, spec)
            except RuntimeError as e:
                print('FAIL %-18s native: %s' % (name, e))
                bad += 1
                continue
        try:
            port = run_port(name, spec)
        except RuntimeError as e:
            print('FAIL %-18s %s' % (name, e))
            bad += 1
            continue
        problems = compare(cache[name], port)
        if problems:
            bad += 1
            for p in problems:
                print('FAIL %-18s %s' % (name, p))
        else:
            print('ok   %-18s %d tests, summary %r, exit %d'
                  % (name, len(cache[name]['status']), cache[name]['summary'], cache[name]['exit']))
    cache_path.write_text(json.dumps(cache, indent=1, sort_keys=True))
    print('scenarios: %d, failing: %d' % (len(SCEN), bad))
    return 0 if bad == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
