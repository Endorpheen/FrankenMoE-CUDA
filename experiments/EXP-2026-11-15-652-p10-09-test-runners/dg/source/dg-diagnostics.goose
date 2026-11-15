// P10-09 S3: the benchmark diagnostics a native-free build needs to read its
// own run output — the analysis half of scripts/bench-analyze.py.
//
// Native source at base commit 70de805c:
//   upstream/BigMoeOnEdge/scripts/bench-analyze.py   analyze() (45-105): the
//                                                    header-driven column map,
//                                                    the row skip rules, the
//                                                    `# summary` trailer, the
//                                                    aggregate mean, the median,
//                                                    p5/p95, the derived
//                                                    MiB/token and the TTFT;
//   upstream/BigMoeOnEdge/scripts/trace_io.py        percentile() (46-53):
//                                                    linear interpolation on an
//                                                    already sorted list;
//                                                    read_kv_file() (34-43): the
//                                                    flat key=value sidecar.
//
// Scope: the numbers and the fallbacks, not the file walking and not the model
// table. The caller hands in one CSV and its .metrics sidecar as text; the port
// returns the same fields analyze() returns, scaled to integer thousandths so
// the comparison never depends on printf float formatting (which this
// toolchain's `print` does not implement). The README records that choice.
//
// Owned decisions, in the native order:
//   - a column is found by NAME from the `step,...` header, so trailing columns
//     added later are picked up and older CSVs without them still parse;
//   - a data row contributes a wall time only if column 2 parses; a row with a
//     missing or non-numeric wall is dropped in full, while a bad optional
//     column (stall_ms, mgmt_ms) only drops that field;
//   - `# summary` is whitespace-separated key=value and is read by name; a
//     missing or non-numeric key reads as 0.0, except cache_hit_pct which keeps
//     its raw text because "-1.0" means "not measured";
//   - decode tok/s per token = 1000/wall, taken only over walls > 0, while the
//     aggregate mean divides n by the sum of ALL walls (a zero wall still costs
//     time, it just cannot be a rate);
//   - median is statistics.median: the middle value, or the mean of the two
//     middle values for an even count;
//   - p5/p95 are linear interpolations of the sorted rates at q*(n-1);
//   - TTFT is load_s + prefill_s, and read MiB/token is read_MiB/n;
//   - spec_useful_pct divides by spec_experts and reads 0 when it is absent;
//   - a missing row in the report prints a dash, never a zero.
//
// Not owned: reading files, listing the bench directory, the model/ORDER table
// and the markdown document around the rows.

import std;
import math;

const DG_MAX_ROWS: i64 = 64;
const DG_MAX_COLS: i64 = 32;
const DG_MAX_KEY: i64 = 32;

// dg_analyze out[] indices.
const DG_N: i64 = 0;
const DG_MEAN: i64 = 1;
const DG_MIN: i64 = 2;
const DG_MAX: i64 = 3;
const DG_MEDIAN: i64 = 4;
const DG_P5: i64 = 5;
const DG_P95: i64 = 6;
const DG_READ_TOK: i64 = 7;
const DG_STALL: i64 = 8;   // -1 when absent
const DG_MGMT: i64 = 9;    // -1 when absent
const DG_TTFT: i64 = 10;
const DG_PREFILL: i64 = 11;
const DG_LOAD: i64 = 12;
const DG_BUDGET: i64 = 13;
const DG_RESIDENT: i64 = 14;
const DG_SPEC_READ: i64 = 15;
const DG_SPEC_USEFUL: i64 = 16;
const DG_PEAK_RSS: i64 = 17;
const DG_MEM_FLOOR: i64 = 18;
const DG_CPU0: i64 = 19;
const DG_CPU_MAX: i64 = 20;
const DG_BATT_MAX: i64 = 21;
const DG_OUT: i64 = 22;

// ── text helpers ────────────────────────────────────────────────────────────

fn dg_put_s(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    var i: i64 = 0;
    while i < s.len {
        dst[at + i] = s[i];
        i = i + 1;
    }
    return at + s.len;
}

fn dg_put_int(dst: u8[:], at: i64, v: i64) -> i64 {
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

fn dg_eq(a: const u8[:], b: const u8[:]) -> bool {
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

fn dg_at(s: const u8[:], p: i64) -> u8 {
    if p < 0 || p >= s.len {
        return 0;
    }
    return s[p];
}

fn dg_digit(c: u8) -> bool {
    return c >= 48 && c <= 57;
}

// ── decimal parsing and fixed-point rounding ────────────────────────────────

// A decimal number as the native float() sees one: sign, integer digits,
// fraction digits. ok[0] is 1 on success, 0 on a non-numeric value.
fn dg_parse(s: const u8[:], ok: i64[:]) -> f64 {
    ok[0] = 0;
    var i: i64 = 0;
    while i < s.len && (s[i] == 32 || s[i] == 9) {
        i = i + 1;
    }
    if i >= s.len {
        return 0.0;
    }
    var neg: i64 = 0;
    if s[i] == 45 || s[i] == 43 {
        if s[i] == 45 {
            neg = 1;
        }
        i = i + 1;
    }
    var saw: i64 = 0;
    var iv: i64 = 0;
    while i < s.len && dg_digit(s[i]) {
        iv = iv * 10 + (s[i] - 48);
        saw = 1;
        i = i + 1;
    }
    var fv: f64 = 0.0;
    if i < s.len && s[i] == 46 {
        i = i + 1;
        var scale: f64 = 10.0;
        var fdig: i64 = 0;
        while i < s.len && dg_digit(s[i]) {
            fdig = fdig * 10 + (s[i] - 48);
            fv = fv + (s[i] - 48) as f64 / scale;
            scale = scale * 10.0;
            saw = 1;
            i = i + 1;
        }
    }
    if saw == 0 {
        return 0.0;
    }
    var v: f64 = iv as f64 + fv;
    if neg != 0 {
        v = 0.0 - v;
    }
    ok[0] = 1;
    return v;
}

// Round to the nearest integer, ties to even — Python's round() on the same
// double, which is how the reported thousandths are produced on both sides.
fn dg_rint(x: f64) -> i64 {
    let f: f64 = floor(x);
    let d: f64 = x - f;
    var base: i64 = f as i64;
    if d > 0.5 {
        return base + 1;
    }
    if d < 0.5 {
        return base;
    }
    // exact tie: pick the even neighbour
    if (base / 2) * 2 == base {
        return base;
    }
    return base + 1;
}

fn dg_milli(x: f64) -> i64 {
    return dg_rint(x * 1000.0);
}

// ── the key=value sidecar ───────────────────────────────────────────────────

fn dg_kv(text: const u8[:], key: const u8[:], out_value: u8[:]) -> i64 {
    var start: i64 = 0;
    while start <= text.len {
        var end: i64 = start;
        while end < text.len && text[end] != 10 {
            end = end + 1;
        }
        let line: const u8[:] = text[start..end];
        var eq: i64 = 0 - 1;
        var i: i64 = 0;
        while i < line.len {
            if line[i] == 61 && eq < 0 {
                eq = i;
            }
            i = i + 1;
        }
        if eq > 0 && dg_eq(line[0..eq], key) {
            var n: i64 = 0;
            var j: i64 = eq + 1;
            while j < line.len {
                let c: u8 = line[j];
                if c != 13 {
                    out_value[n] = c;
                    n = n + 1;
                }
                j = j + 1;
            }
            return n;
        }
        start = end + 1;
    }
    return 0 - 1;
}

fn dg_kv_f64(text: const u8[:], key: const u8[:]) -> f64 {
    var buf: u8[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var ok: i64[2] = [0, 0];
    let n: i64 = dg_kv(text, key, buf);
    if n < 0 {
        return 0.0;
    }
    let v: f64 = dg_parse(buf[0..n], ok);
    if ok[0] == 0 {
        return 0.0;
    }
    return v;
}

// ── statistics ──────────────────────────────────────────────────────────────

fn dg_sorted(a: f64[:], n: i64) -> i64 {
    var i: i64 = 1;
    while i < n {
        let x: f64 = a[i];
        var j: i64 = i - 1;
        while j >= 0 && a[j] > x {
            a[j + 1] = a[j];
            j = j - 1;
        }
        a[j + 1] = x;
        i = i + 1;
    }
    return n;
}

// trace_io.percentile: linear interpolation on an already sorted list, 0.0
// when empty. q is in thousandths so 5 and 95 stay exact.
export fn dg_percentile(values: const f64[:], n: i64, q_milli: i64) -> f64 {
    if n <= 0 {
        return 0.0;
    }
    let q: f64 = q_milli as f64 / 1000.0;
    let i: f64 = q * (n - 1) as f64;
    var lo: i64 = i as i64;
    if lo < 0 {
        lo = 0;
    }
    if lo > n - 1 {
        lo = n - 1;
    }
    var hi: i64 = lo + 1;
    if hi > n - 1 {
        hi = n - 1;
    }
    return values[lo] + (values[hi] - values[lo]) * (i - lo as f64);
}

// statistics.median on an already sorted list.
export fn dg_median(values: const f64[:], n: i64) -> f64 {
    if n <= 0 {
        return 0.0;
    }
    if n % 2 == 1 {
        return values[n / 2];
    }
    return (values[n / 2 - 1] + values[n / 2]) / 2.0;
}

// ── the analysis ────────────────────────────────────────────────────────────

fn dg_col_index(head: const u8[:], name: const u8[:]) -> i64 {
    var idx: i64 = 0;
    var start: i64 = 0;
    var i: i64 = 0;
    while i <= head.len {
        if i == head.len || head[i] == 44 {
            if dg_eq(head[start..i], name) {
                return idx;
            }
            idx = idx + 1;
            start = i + 1;
        }
        i = i + 1;
    }
    return 0 - 1;
}

// The number of comma-separated fields in a row and the field at want.
fn dg_field(row: const u8[:], want: i64, out: u8[:]) -> i64 {
    var idx: i64 = 0;
    var start: i64 = 0;
    var i: i64 = 0;
    while i <= row.len {
        if i == row.len || row[i] == 44 {
            if idx == want {
                var n: i64 = 0;
                var j: i64 = start;
                while j < i {
                    out[n] = row[j];
                    n = n + 1;
                    j = j + 1;
                }
                return n;
            }
            idx = idx + 1;
            start = i + 1;
        }
        i = i + 1;
    }
    return 0 - 1;
}

fn dg_count_fields(row: const u8[:]) -> i64 {
    var idx: i64 = 1;
    var i: i64 = 0;
    while i < row.len {
        if row[i] == 44 {
            idx = idx + 1;
        }
        i = i + 1;
    }
    return idx;
}

// The last `# summary ...` line, with its marker removed and the spaces turned
// into newlines so the flat token list reads as one key=value per line. That is
// the native `row.replace("# summary", "").split()` rule; the caller's
// `tail -1` picks the LAST trailer, which is what a re-run appends.
fn dg_summary_body(csv: const u8[:], out: u8[:]) -> i64 {
    var start: i64 = 0;
    var last: i64 = 0 - 1;
    var last_len: i64 = 0;
    while start <= csv.len {
        var end: i64 = start;
        while end < csv.len && csv[end] != 10 {
            end = end + 1;
        }
        var n: i64 = end - start;
        if n > 0 && csv[start + n - 1] == 13 {
            n = n - 1;
        }
        if n > 9 && dg_eq(csv[start..start + n][0..9], "# summary") {
            last = start + 9;
            last_len = n - 9;
        }
        start = end + 1;
    }
    if last < 0 {
        return 0;
    }
    var p: i64 = 0;
    var i: i64 = last;
    while i < last + last_len && p < out.len {
        let c: u8 = csv[i];
        if c == 32 || c == 9 {
            out[p] = 10;
        } else {
            out[p] = c;
        }
        p = p + 1;
        i = i + 1;
    }
    return p;
}

// One CSV plus its .metrics sidecar. out[] receives DG_OUT fields in thousandths
// (rates, seconds, MiB), plain counts for n, and -1 for an absent optional
// column. Returns 1 when at least one wall parsed, 0 when the file is empty of
// usable rows (the native analyze() returns None there).
export fn dg_analyze(csv: const u8[:], metrics: const u8[:], out: i64[:]) -> i64 {
    if out.len < DG_OUT {
        return 0 - 1;
    }
    var walls: f64[64] = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0];
    var stalls: f64[64] = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0];
    var mgmts: f64[64] = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0];
    var head: u8[256] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var head_len: i64 = 0;
    var cols: i64 = 0;
    var stall_col: i64 = 0 - 1;
    var mgmt_col: i64 = 0 - 1;
    var walls_n: i64 = 0;
    var stalls_n: i64 = 0;
    var mgmts_n: i64 = 0;
    var start: i64 = 0;
    while start <= csv.len {
        var end: i64 = start;
        while end < csv.len && csv[end] != 10 {
            end = end + 1;
        }
        var line: const u8[:] = csv[start..end];
        if line.len > 0 && line[line.len - 1] == 13 {
            line = line[0..line.len - 1];
        }
        if line.len >= 5 && dg_eq(line[0..5], "step,") {
            var i: i64 = 0;
            while i < line.len && i < 256 {
                head[i] = line[i];
                i = i + 1;
            }
            head_len = i;
            cols = dg_count_fields(line);
            stall_col = dg_col_index(line, "stall_ms");
            mgmt_col = dg_col_index(line, "mgmt_ms");
        } else if line.len > 0 && dg_digit(line[0]) {
            if cols > 2 && walls_n < DG_MAX_ROWS {
                var fbuf: u8[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
                var ok: i64[2] = [0, 0];
                let wn: i64 = dg_field(line, 2, fbuf);
                if wn > 0 {
                    let w: f64 = dg_parse(fbuf[0..wn], ok);
                    if ok[0] != 0 {
                        walls[walls_n] = w;
                        walls_n = walls_n + 1;
                        if stall_col >= 0 {
                            let sn: i64 = dg_field(line, stall_col, fbuf);
                            if sn > 0 {
                                let sv: f64 = dg_parse(fbuf[0..sn], ok);
                                if ok[0] != 0 && stalls_n < DG_MAX_ROWS {
                                    stalls[stalls_n] = sv;
                                    stalls_n = stalls_n + 1;
                                }
                            }
                        }
                        if mgmt_col >= 0 {
                            let mn: i64 = dg_field(line, mgmt_col, fbuf);
                            if mn > 0 {
                                let mv: f64 = dg_parse(fbuf[0..mn], ok);
                                if ok[0] != 0 && mgmts_n < DG_MAX_ROWS {
                                    mgmts[mgmts_n] = mv;
                                    mgmts_n = mgmts_n + 1;
                                }
                            }
                        }
                    }
                }
            }
        }
        start = end + 1;
    }
    if walls_n == 0 {
        out[DG_N] = 0;
        return 0;
    }
    var body: u8[1024] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    let bl: i64 = dg_summary_body(csv, body);
    // rates, sorted, over positive walls only
    var tps: f64[64] = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0];
    var n: i64 = 0;
    var total_s: f64 = 0.0;
    var i2: i64 = 0;
    while i2 < walls_n {
        total_s = total_s + walls[i2];
        if walls[i2] > 0.0 {
            tps[n] = 1000.0 / walls[i2];
            n = n + 1;
        }
        i2 = i2 + 1;
    }
    total_s = total_s / 1000.0;
    dg_sorted(tps, n);
    var stall_mean: f64 = 0.0;
    var mgmt_mean: f64 = 0.0;
    if stalls_n > 0 {
        var a: i64 = 0;
        while a < stalls_n {
            stall_mean = stall_mean + stalls[a];
            a = a + 1;
        }
        stall_mean = stall_mean / stalls_n as f64;
    }
    if mgmts_n > 0 {
        var b: i64 = 0;
        while b < mgmts_n {
            mgmt_mean = mgmt_mean + mgmts[b];
            b = b + 1;
        }
        mgmt_mean = mgmt_mean / mgmts_n as f64;
    }
    let read_mib: f64 = dg_kv_f64(body[0..bl], "read_MiB");
    let prefill: f64 = dg_kv_f64(body[0..bl], "prefill_tps");
    let load_s: f64 = dg_kv_f64(body[0..bl], "load_s");
    let prefill_s: f64 = dg_kv_f64(body[0..bl], "prefill_s");
    let experts: f64 = dg_kv_f64(body[0..bl], "spec_experts");
    let useful: f64 = dg_kv_f64(body[0..bl], "spec_useful");
    // n stays the number of parsed wall rows, exactly as the native analyze()
    // counts it: the aggregate mean and every per-token figure divide by it,
    // while the rate statistics use only the positive walls.
    out[DG_N] = walls_n;
    out[DG_MEAN] = dg_milli(if total_s != 0.0 { walls_n as f64 / total_s } else { 0.0 });
    out[DG_MIN] = dg_milli(if n > 0 { tps[0] } else { 0.0 });
    out[DG_MAX] = dg_milli(if n > 0 { tps[n - 1] } else { 0.0 });
    out[DG_MEDIAN] = dg_milli(dg_median(tps, n));
    out[DG_P5] = dg_milli(dg_percentile(tps, n, 50));
    out[DG_P95] = dg_milli(dg_percentile(tps, n, 950));
    out[DG_READ_TOK] = dg_milli(read_mib / walls_n as f64);
    out[DG_STALL] = if stalls_n > 0 { dg_milli(stall_mean) } else { 0 - 1 };
    out[DG_MGMT] = if mgmts_n > 0 { dg_milli(mgmt_mean) } else { 0 - 1 };
    out[DG_TTFT] = dg_milli(load_s + prefill_s);
    out[DG_PREFILL] = dg_milli(prefill);
    out[DG_LOAD] = dg_milli(load_s);
    out[DG_BUDGET] = dg_milli(dg_kv_f64(body[0..bl], "cache_budget_MiB"));
    out[DG_RESIDENT] = dg_milli(dg_kv_f64(body[0..bl], "cache_resident_MiB"));
    out[DG_SPEC_READ] = dg_milli(dg_kv_f64(body[0..bl], "spec_read_MiB") / walls_n as f64);
    out[DG_SPEC_USEFUL] = dg_milli(if experts != 0.0 { 100.0 * useful / experts } else { 0.0 });
    out[DG_PEAK_RSS] = dg_milli(dg_kv_f64(metrics, "peak_rss_kb") / 1048576.0);
    out[DG_MEM_FLOOR] = dg_milli(dg_kv_f64(metrics, "mem_avail_floor_kb") / 1048576.0);
    out[DG_CPU0] = dg_milli(dg_kv_f64(metrics, "cpu_temp_before_mC") / 1000.0);
    out[DG_CPU_MAX] = dg_milli(dg_kv_f64(metrics, "cpu_temp_max_mC") / 1000.0);
    out[DG_BATT_MAX] = dg_milli(dg_kv_f64(metrics, "batt_temp_max_dC") / 10.0);
    return 1;
}

// ── report rows ─────────────────────────────────────────────────────────────

// A thousandth-scaled value with `dec` decimals, truncated: no libc float
// formatting, so the same integer on both sides renders the same text.
export fn dg_fmt(dst: u8[:], at: i64, milli: i64, dec: i64) -> i64 {
    var p = at;
    var x = milli;
    if x < 0 {
        dst[p] = 45;
        p = p + 1;
        x = 0 - x;
    }
    if dec == 0 {
        return dg_put_int(dst, p, (x + 500) / 1000);
    }
    let div: i64 = if dec == 1 { 100 } else { 10 };
    let ip: i64 = x / 1000;
    var fr: i64 = (x % 1000) / div;
    p = dg_put_int(dst, p, ip);
    dst[p] = 46;
    p = p + 1;
    if dec == 2 && fr < 10 {
        dst[p] = 48;
        p = p + 1;
    }
    return dg_put_int(dst, p, fr);
}

// cache_hit_pct keeps its raw text: "-" and "-1.0" both mean "not measured"
// and print as a dash. Anything else prints as a whole percentage.
fn dg_hit(dst: u8[:], at: i64, raw: const u8[:]) -> i64 {
    if raw.len == 0 || dg_eq(raw, "-") || dg_eq(raw, "-1.0") {
        return dg_put_s(dst, at, "-");
    }
    var ok: i64[2] = [0, 0];
    let v: f64 = dg_parse(raw, ok);
    if ok[0] == 0 {
        return dg_put_s(dst, at, "-");
    }
    var p: i64 = dg_put_int(dst, at, dg_rint(v));
    dst[p] = 37;
    return p + 1;
}

// The summary trailer's raw cache_hit_pct value, for dg_hit.
fn dg_hit_raw(csv: const u8[:], out: u8[:]) -> i64 {
    var body: u8[1024] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    let bl: i64 = dg_summary_body(csv, body);
    if bl == 0 {
        return 0 - 1;
    }
    return dg_kv(body[0..bl], "cache_hit_pct", out);
}

// One markdown throughput row, the layout bench-analyze.py emits. A missing
// column prints as an em dash, never as a zero. milli_fields carries the
// DG_* thousandths; present tells which optional columns exist.
export fn dg_row(
    label: const u8[:],
    fields: const i64[:],
    hit_raw: const u8[:],
    out: u8[:],
) -> i64 {
    var p: i64 = dg_put_s(out, 0, "| ");
    p = dg_put_s(out, p, label);
    p = dg_put_s(out, p, " | **");
    p = dg_fmt(out, p, fields[DG_MEAN], 2);
    p = dg_put_s(out, p, "** | ");
    p = dg_fmt(out, p, fields[DG_MIN], 2);
    p = dg_put_s(out, p, " | ");
    p = dg_fmt(out, p, fields[DG_MAX], 2);
    p = dg_put_s(out, p, " | ");
    p = dg_fmt(out, p, fields[DG_MEDIAN], 2);
    p = dg_put_s(out, p, " | ");
    p = dg_fmt(out, p, fields[DG_P95], 2);
    p = dg_put_s(out, p, " | ");
    p = dg_fmt(out, p, fields[DG_PREFILL], 2);
    p = dg_put_s(out, p, " | ");
    p = dg_fmt(out, p, fields[DG_TTFT], 1);
    p = dg_put_s(out, p, " | ");
    p = dg_hit(out, p, hit_raw);
    p = dg_put_s(out, p, " | ");
    p = dg_fmt(out, p, fields[DG_READ_TOK], 0);
    p = dg_put_s(out, p, " MiB | ");
    if fields[DG_STALL] < 0 {
        p = dg_put_s(out, p, "-");
    } else {
        p = dg_fmt(out, p, fields[DG_STALL], 1);
    }
    return dg_put_s(out, p, " |");
}

// The third table lists only the runs that exercised cache sizing or temporal
// prefetch: a fixed non-prefetch run has budget == resident and no speculative
// reads, so its row would be uninformative.
export fn dg_sized(fields: const i64[:]) -> i64 {
    if fields[DG_BUDGET] > 0 || fields[DG_SPEC_READ] > 0 {
        return 1;
    }
    return 0;
}

// One cache-sizing row: `| label | budget | resident | useful | read |`, dashes
// where the field does not apply.
export fn dg_sized_row(label: const u8[:], fields: const i64[:], out: u8[:]) -> i64 {
    var p: i64 = dg_put_s(out, 0, "| ");
    p = dg_put_s(out, p, label);
    p = dg_put_s(out, p, " | ");
    if fields[DG_BUDGET] > 0 {
        p = dg_fmt(out, p, fields[DG_BUDGET], 0);
        p = dg_put_s(out, p, " MiB");
    } else {
        p = dg_put_s(out, p, "-");
    }
    p = dg_put_s(out, p, " | ");
    p = dg_fmt(out, p, fields[DG_RESIDENT], 0);
    p = dg_put_s(out, p, " MiB | ");
    if fields[DG_SPEC_READ] > 0 {
        p = dg_fmt(out, p, fields[DG_SPEC_USEFUL], 0);
        p = dg_put_s(out, p, "%");
    } else {
        p = dg_put_s(out, p, "-");
    }
    p = dg_put_s(out, p, " | ");
    if fields[DG_SPEC_READ] > 0 {
        p = dg_fmt(out, p, fields[DG_SPEC_READ], 0);
        p = dg_put_s(out, p, " MiB");
    } else {
        p = dg_put_s(out, p, "-");
    }
    return dg_put_s(out, p, " |");
}
