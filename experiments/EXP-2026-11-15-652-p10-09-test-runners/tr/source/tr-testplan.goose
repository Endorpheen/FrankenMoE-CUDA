// P10-09 S1: the test-runner decisions a native-free build needs.
//
// Native sources, at base commit 70de805c:
//   runtime/tests/CMakeLists.txt          the test registry: per-arch fixtures
//                                        (FIXTURES_SETUP/REQUIRED), the split-shard
//                                        case, the python3-present skip;
//   upstream/llama.cpp/tools/server/tests/tests.sh
//                                        the SLOW_TESTS switch and the `-m "not slow"`
//                                        deselect;
//   upstream/llama.cpp/tools/server/tests/pytest.ini
//                                        the marker declaration.
//
// The oracle for this slice is CTest itself (cmake 4.2.3): the same registry is
// handed to ctest and to this module, and every decision below is compared
// against what ctest reports. The module owns exactly the decisions, not the
// process work: it never spawns a test, reads a file or keeps a clock. The
// caller supplies the definitions and, for each test it actually ran, the
// return code.
//
// Owned decisions, matching ctest's observed behaviour:
//   - selection: a test is selected iff it is not DISABLED, it matches the
//     include filter (POSIX extended regex, unanchored search) and does not
//     match the exclude filter; an empty filter selects everything;
//   - fixture closure: a selected test that requires fixture f pulls in f's
//     setup test and f's cleanup test even when a filter would have dropped
//     them, and recursively pulls in the fixtures those tests require;
//   - execution order: definition order, except that a fixture's setup tests
//     run immediately before the first test that requires them, and its
//     cleanup tests run immediately after the last selected test that requires
//     them;
//   - classification: rc 0 passes; a non-zero rc equal to the test's
//     SKIP_RETURN_CODE is Skipped; any other non-zero rc fails; a test whose
//     required fixture failed (or was never run) is "Not Run"; a disabled test
//     is "Not Run (Disabled)" and is excluded from the pass denominator;
//   - a fixture whose setup test SKIPPED is still usable: its dependents run
//     (verified against ctest), so only a failed or never-run setup blocks;
//   - a cleanup test runs even when its fixture's setup failed, and its own
//     failure is a failure;
//   - summary: T = reported - disabled, F = failed + notrun, the pass
//     percentage is F-derived and rounded half up, and the process exits 8
//     when F > 0 and 0 otherwise.
//
// The regex is the subset the filters in this tree use: literals, `.`, `^`,
// `$`, classes with ranges and negation, escapes, `*`, `+`, `?` and top-level
// alternation with `|`. Groups and back references are not implemented; a
// filter that needs them is reported as unmatched rather than silently
// mismatching, and that boundary is recorded in the README.

import std;

const TR_MAX_TESTS: i64 = 64;
const TR_MAX_FX: i64 = 30;

// tr_classify results.
const TR_PASS: i64 = 0;
const TR_FAIL: i64 = 1;
const TR_SKIP: i64 = 2;
const TR_NOTRUN: i64 = 3;
const TR_DISABLED: i64 = 4;

// Fixture states the caller keeps per fixture id.
const TR_FX_OK: i64 = 0;
const TR_FX_FAIL: i64 = 1;
const TR_FX_NOTRUN: i64 = 2;
const TR_FX_SKIP: i64 = 3;

// ── small helpers ───────────────────────────────────────────────────────────

fn tr_eq(a: const u8[:], b: const u8[:]) -> bool {
    if a.len != b.len {
        return false;
    }
    var i: i64 = 0;
    while i < a.len {
        if a[i] != b[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn tr_has(mask: i64, bit: i64) -> bool {
    if bit < 0 || bit >= 30 {
        return false;
    }
    return (mask & (1 as i64 << bit)) != 0;
}

fn tr_digit(c: u8) -> bool {
    return c >= 48 && c <= 57;
}

// ── ERE subset matcher ──────────────────────────────────────────────────────

// Length of the atom at pi: 0 when the pattern ends or the construct is not
// supported (a group, a back reference), which makes the whole match fail.
fn tr_atom_len(pat: const u8[:], pi: i64) -> i64 {
    if pi >= pat.len {
        return 0;
    }
    let c: u8 = pat[pi];
    if c == 92 {
        if pi + 1 >= pat.len {
            return 0;
        }
        return 2;
    }
    if c == 91 {
        var j: i64 = pi + 1;
        if j < pat.len && pat[j] == 94 {
            j = j + 1;
        }
        if j < pat.len && pat[j] == 93 {
            j = j + 1;
        }
        while j < pat.len && pat[j] != 93 {
            j = j + 1;
        }
        if j >= pat.len {
            return 0;
        }
        return j + 1 - pi;
    }
    if c == 40 || c == 41 || c == 123 || c == 125 {
        return 0;
    }
    return 1;
}

fn tr_class_ok(pat: const u8[:], pi: i64, c: u8) -> bool {
    var j: i64 = pi + 1;
    var neg: i64 = 0;
    if j < pat.len && pat[j] == 94 {
        neg = 1;
        j = j + 1;
    }
    var hit: i64 = 0;
    var first: i64 = 1;
    while j < pat.len && (first != 0 || pat[j] != 93) {
        first = 0;
        var lo: u8 = pat[j];
        if lo == 92 && j + 1 < pat.len {
            j = j + 1;
            lo = pat[j];
        }
        if j + 2 < pat.len && pat[j + 1] == 45 && pat[j + 2] != 93 {
            var hi: u8 = pat[j + 2];
            if hi == 92 && j + 3 < pat.len {
                hi = pat[j + 3];
                j = j + 1;
            }
            if c >= lo && c <= hi {
                hit = 1;
            }
            j = j + 3;
        } else {
            if c == lo {
                hit = 1;
            }
            j = j + 1;
        }
    }
    if neg != 0 {
        return hit == 0;
    }
    return hit != 0;
}

fn tr_atom_ok(pat: const u8[:], pi: i64, c: u8) -> bool {
    let a: u8 = pat[pi];
    if a == 46 {
        return c != 10;
    }
    if a == 91 {
        return tr_class_ok(pat, pi, c);
    }
    if a == 92 {
        if pi + 1 >= pat.len {
            return false;
        }
        let e: u8 = pat[pi + 1];
        if e == 100 {
            return tr_digit(c);
        }
        if e == 68 {
            return !tr_digit(c);
        }
        if e == 119 {
            return tr_digit(c) || (c >= 97 && c <= 122) || (c >= 65 && c <= 90) || c == 95;
        }
        if e == 87 {
            return !(tr_digit(c) || (c >= 97 && c <= 122) || (c >= 65 && c <= 90) || c == 95);
        }
        if e == 115 {
            return c == 32 || (c >= 9 && c <= 13);
        }
        if e == 83 {
            return !(c == 32 || (c >= 9 && c <= 13));
        }
        return c == e;
    }
    return c == a;
}

// Backtracking match of one alternative starting exactly at si.
recursive fn tr_m(pat: const u8[:], pi: i64, s: const u8[:], si: i64) -> i64 {
    if pi >= pat.len {
        return si;
    }
    if pat[pi] == 36 {
        if si == s.len {
            return si;
        }
        return 0 - 1;
    }
    let al: i64 = tr_atom_len(pat, pi);
    if al == 0 {
        return 0 - 1;
    }
    var q: u8 = 0;
    if pi + al < pat.len {
        let c: u8 = pat[pi + al];
        if c == 42 || c == 43 || c == 63 {
            q = c;
        }
    }
    var next: i64 = pi + al;
    if q != 0 {
        next = next + 1;
    }
    if q == 0 {
        if si < s.len && tr_atom_ok(pat, pi, s[si]) {
            return tr_m(pat, next, s, si + 1);
        }
        return 0 - 1;
    }
    if q == 63 {
        if si < s.len && tr_atom_ok(pat, pi, s[si]) {
            let r: i64 = tr_m(pat, next, s, si + 1);
            if r >= 0 {
                return r;
            }
        }
        return tr_m(pat, next, s, si);
    }
    var k: i64 = 0;
    while si + k < s.len && tr_atom_ok(pat, pi, s[si + k]) {
        k = k + 1;
    }
    var mn: i64 = 0;
    if q == 43 {
        mn = 1;
    }
    if k < mn {
        return 0 - 1;
    }
    var take: i64 = k;
    while take >= mn {
        let r: i64 = tr_m(pat, next, s, si + take);
        if r >= 0 {
            return r;
        }
        take = take - 1;
    }
    return 0 - 1;
}

// One alternative, anchored or searched from every start position.
fn tr_alt(pat: const u8[:], s: const u8[:]) -> bool {
    var pi: i64 = 0;
    var anchored: i64 = 0;
    if pi < pat.len && pat[pi] == 94 {
        anchored = 1;
        pi = pi + 1;
    }
    if anchored != 0 {
        return tr_m(pat, pi, s, 0) >= 0;
    }
    var start: i64 = 0;
    while start <= s.len {
        if tr_m(pat, pi, s, start) >= 0 {
            return true;
        }
        start = start + 1;
    }
    return false;
}

// POSIX-ERE-subset match: every top-level `|` alternative is tried in order.
export fn tr_match(pat: const u8[:], s: const u8[:]) -> bool {
    if pat.len == 0 {
        return true;
    }
    var start: i64 = 0;
    var i: i64 = 0;
    while i <= pat.len {
        if i == pat.len || pat[i] == 124 {
            if tr_alt(pat[start..i], s) {
                return true;
            }
            start = i + 1;
        }
        i = i + 1;
    }
    return false;
}

// ── selection ───────────────────────────────────────────────────────────────

// ctest: an empty -R selects everything, an empty -E excludes nothing, and
// both filters are searched, not anchored. DISABLED is not a filter: a disabled
// test stays in the list and is reported, it just never runs (see tr_plan).
export fn tr_selected(name: const u8[:], filter: const u8[:], exclude: const u8[:]) -> bool {
    if filter.len != 0 && !tr_match(filter, name) {
        return false;
    }
    if exclude.len != 0 && tr_match(exclude, name) {
        return false;
    }
    return true;
}

// ── fixture closure and execution order ─────────────────────────────────────

fn tr_setup_mask(n: i64, setup_fx: const i64[:], f: i64) -> i64 {
    var m: i64 = 0;
    var i: i64 = 0;
    while i < n {
        if tr_has(setup_fx[i], f) {
            m = m | (1 as i64 << i);
        }
        i = i + 1;
    }
    return m;
}

fn tr_cleanup_index(n: i64, clean_fx: const i64[:], f: i64) -> i64 {
    var i: i64 = 0;
    while i < n {
        if tr_has(clean_fx[i], f) {
            return i;
        }
        i = i + 1;
    }
    return 0 - 1;
}

// ctest's serial order: dependencies first, in ascending definition index, then
// the test itself; a test already emitted is not emitted twice.
recursive fn tr_dfs(i: i64, n: i64, dep: const i64[:], emitted: i64[:], order: i64[:], n_out: i64[:]) -> i64 {
    if i < 0 || i >= n || emitted[i] != 0 {
        return n_out[0];
    }
    var b: i64 = 0;
    while b < n {
        if tr_has(dep[i], b) {
            tr_dfs(b, n, dep, emitted, order, n_out);
        }
        b = b + 1;
    }
    if emitted[i] == 0 {
        emitted[i] = 1;
        order[n_out[0]] = i;
        n_out[0] = n_out[0] + 1;
    }
    return n_out[0];
}

// CTest's UpdateForFixtures + CreateSerialTestCostList, decision for decision:
//   1. the selected tests keep definition order; a disabled test stays in the
//      list (so it is reported) but takes no part in fixture handling;
//   2. a test that requires fixture f depends on every setup test of f, and the
//      first test to require f appends f's setups and then f's cleanups to the
//      end of the list (in definition order), unless they are already there;
//   3. afterwards every cleanup test depends on all tests that require its
//      fixture and on the fixture's setup tests;
//   4. the run order is a depth-first walk over the roots in definition order,
//      emitting dependencies before dependents.
// Returns the number of reported tests, or -1 when the table does not fit.
export fn tr_plan(
    names_off: const i64[:],
    names_len: const i64[:],
    blob: const u8[:],
    n: i64,
    setup_fx: const i64[:],
    req_fx: const i64[:],
    clean_fx: const i64[:],
    disabled: const i64[:],
    filter: const u8[:],
    exclude: const u8[:],
    order: i64[:],
    n_out: i64[:],
) -> i64 {
    if n > TR_MAX_TESTS || n > order.len {
        return 0 - 1;
    }
    var inlist: i64[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var dep: i64[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var list: i64[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var emitted: i64[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var dependents: i64[30] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var seen_fx: i64[30] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var nl: i64 = 0;
    var i: i64 = 0;
    while i < n {
        let name: const u8[:] = blob[names_off[i]..names_off[i] + names_len[i]];
        if tr_selected(name, filter, exclude) {
            inlist[i] = 1;
            list[nl] = i;
            nl = nl + 1;
        }
        i = i + 1;
    }
    var p: i64 = 0;
    while p < nl {
        let t: i64 = list[p];
        if disabled[t] == 0 {
            var f: i64 = 0;
            while f < TR_MAX_FX {
                if tr_has(req_fx[t], f) {
                    dependents[f] = dependents[f] | (1 as i64 << t);
                    dep[t] = dep[t] | tr_setup_mask(n, setup_fx, f);
                    if seen_fx[f] == 0 {
                        seen_fx[f] = 1;
                        var s: i64 = 0;
                        while s < n {
                            if tr_has(setup_fx[s], f) && inlist[s] == 0 {
                                inlist[s] = 1;
                                list[nl] = s;
                                nl = nl + 1;
                            }
                            s = s + 1;
                        }
                        var c: i64 = 0;
                        while c < n {
                            if tr_has(clean_fx[c], f) && inlist[c] == 0 {
                                inlist[c] = 1;
                                list[nl] = c;
                                nl = nl + 1;
                            }
                            c = c + 1;
                        }
                    }
                }
                f = f + 1;
            }
        }
        p = p + 1;
    }
    var c2: i64 = 0;
    while c2 < n {
        if inlist[c2] != 0 && clean_fx[c2] != 0 {
            var f2: i64 = 0;
            while f2 < TR_MAX_FX {
                if tr_has(clean_fx[c2], f2) {
                    dep[c2] = dep[c2] | dependents[f2];
                    dep[c2] = dep[c2] | tr_setup_mask(n, setup_fx, f2);
                }
                f2 = f2 + 1;
            }
        }
        c2 = c2 + 1;
    }
    n_out[0] = 0;
    var r: i64 = 0;
    while r < n {
        if inlist[r] != 0 {
            tr_dfs(r, n, dep, emitted, order, n_out);
        }
        r = r + 1;
    }
    var reported: i64 = 0;
    var q: i64 = 0;
    while q < n {
        if inlist[q] != 0 {
            reported = reported + 1;
        }
        q = q + 1;
    }
    return reported;
}

// ── classification ──────────────────────────────────────────────────────────

// ctest's per-test verdict from the return code it saw. dep_blocked is 1 when a
// required fixture's setup failed or never ran; a skipped setup does not block.
export fn tr_classify(rc: i64, has_rc: i64, skip_rc: i64, has_skip: i64, dep_blocked: i64) -> i64 {
    if has_rc == 0 || dep_blocked != 0 {
        return TR_NOTRUN;
    }
    if rc == 0 {
        return TR_PASS;
    }
    if has_skip != 0 && rc == skip_rc {
        return TR_SKIP;
    }
    return TR_FAIL;
}

// The verdict for one test, disabled first: ctest reports a disabled test as
// "Not Run (Disabled)" without ever consulting its command.
export fn tr_status(rc: i64, has_rc: i64, skip_rc: i64, has_skip: i64, dep_blocked: i64, disabled: i64) -> i64 {
    if disabled != 0 {
        return TR_DISABLED;
    }
    return tr_classify(rc, has_rc, skip_rc, has_skip, dep_blocked);
}

// A required fixture blocks its dependents when its setup failed or never ran.
// A skipped setup leaves the fixture usable, as ctest does.
export fn tr_dep_blocked(req: i64, fx_state: const i64[:]) -> i64 {
    var f: i64 = 0;
    while f < TR_MAX_FX {
        if tr_has(req, f) {
            let st: i64 = fx_state[f];
            if st == TR_FX_FAIL || st == TR_FX_NOTRUN {
                return 1;
            }
        }
        f = f + 1;
    }
    return 0;
}

// The fixture state a setup test's verdict leaves behind.
export fn tr_fx_state(status: i64) -> i64 {
    if status == TR_PASS {
        return TR_FX_OK;
    }
    if status == TR_SKIP {
        return TR_FX_SKIP;
    }
    if status == TR_NOTRUN {
        return TR_FX_NOTRUN;
    }
    return TR_FX_FAIL;
}

// ── output ──────────────────────────────────────────────────────────────────

fn tr_put_s(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    var i: i64 = 0;
    while i < s.len {
        dst[at + i] = s[i];
        i = i + 1;
    }
    return at + s.len;
}

fn tr_put_int(dst: u8[:], at: i64, v: i64) -> i64 {
    var p = at;
    var x = v;
    if x < 0 {
        dst[p] = 45;
        p = p + 1;
        x = 0 - x;
    }
    if x == 0 {
        dst[p] = 48;
        return p + 1;
    }
    var buf: u8[24] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var n: i64 = 0;
    while x > 0 {
        buf[n] = (48 + (x % 10)) as u8;
        x = x / 10;
        n = n + 1;
    }
    var i = n;
    while i > 0 {
        i = i - 1;
        dst[p] = buf[i];
        p = p + 1;
    }
    return p;
}

fn tr_status_text(status: i64) -> const u8[:] {
    if status == TR_PASS {
        return "Passed";
    }
    if status == TR_SKIP {
        return "Skipped";
    }
    if status == TR_DISABLED {
        return "Disabled";
    }
    if status == TR_NOTRUN {
        return "Not Run";
    }
    return "Failed";
}

// One progress row, the shape ctest prints: `<seq>/<total> Test #<id>: <name> ***<Status>`.
export fn tr_row(seq: i64, total: i64, id: i64, name: const u8[:], status: i64, out: u8[:]) -> i64 {
    var p: i64 = 0;
    p = tr_put_int(out, p, seq);
    p = tr_put_s(out, p, "/");
    p = tr_put_int(out, p, total);
    p = tr_put_s(out, p, " Test #");
    p = tr_put_int(out, p, id);
    p = tr_put_s(out, p, ": ");
    p = tr_put_s(out, p, name);
    if status == TR_PASS {
        p = tr_put_s(out, p, "   Passed");
    } else {
        p = tr_put_s(out, p, " ***");
        if status == TR_DISABLED {
            p = tr_put_s(out, p, "Not Run (Disabled)");
        } else {
            p = tr_put_s(out, p, tr_status_text(status));
        }
    }
    return p;
}

// The pass percentage: 100*(T-F)/T rounded half up, T = reported - disabled.
export fn tr_percent(reported: i64, disabled: i64, failed_plus_notrun: i64) -> i64 {
    let t: i64 = reported - disabled;
    if t <= 0 {
        return 0;
    }
    let good: i64 = t - failed_plus_notrun;
    return (100 * good + t / 2) / t;
}

// The summary line ctest prints. Returns the line length; the process exit
// code (8 when any test failed or was not run, 0 otherwise) lands in
// out_code[0] so the caller gets both from one call.
export fn tr_summary(
    reported: i64,
    disabled: i64,
    failed: i64,
    notrun: i64,
    out: u8[:],
    out_code: i64[:],
) -> i64 {
    let f: i64 = failed + notrun;
    let pct: i64 = tr_percent(reported, disabled, f);
    var p: i64 = 0;
    p = tr_put_int(out, p, pct);
    p = tr_put_s(out, p, "% tests passed, ");
    p = tr_put_int(out, p, f);
    p = tr_put_s(out, p, " tests failed out of ");
    p = tr_put_int(out, p, reported - disabled);
    if f > 0 {
        out_code[0] = 8;
    } else {
        out_code[0] = 0;
    }
    return p;
}

// One row of the failure list: `\t<id> - <name> (<status text>)`.
export fn tr_fail_row(id: i64, name: const u8[:], status: i64, out: u8[:]) -> i64 {
    var p: i64 = 0;
    out[p] = 9;
    p = p + 1;
    p = tr_put_int(out, p, id);
    p = tr_put_s(out, p, " - ");
    p = tr_put_s(out, p, name);
    p = tr_put_s(out, p, " (");
    p = tr_put_s(out, p, tr_status_text(status));
    p = tr_put_s(out, p, ")");
    return p;
}
