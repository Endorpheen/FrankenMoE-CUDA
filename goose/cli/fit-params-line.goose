// P10-02 S3: the fitted-argument line printed by the maintained fit-params
// tool.
//
// Native source: upstream/llama.cpp/tools/fit-params/fit-params.cpp at the base
// commit 137bc40c, lines 44-71. The file has no pure function: the whole
// decision set is the printf sequence that turns the fitted parameters into a
// reusable command line. It is ported here as such:
//
//   "-c %u -ngl %i"                                            45
//   trailing zero tensor splits are dropped while nd > 1        47-49
//   "-ts " then uint32_t(tensor_split[id]) joined by ","        50-58
//   first non-null override opens " -ot \""                    61-64
//   pattern "=" backend name, joined by ","                    65-67
//   closing quote only when at least one override was printed  69
//
// The prefix is fp_. The gate compares the composed line against a
// transcription of those printf calls (main itself loads a model, so it cannot
// be linked into the gate). The float-to-uint32 conversion is the native
// decision: values are truncated toward zero, and the corpus stays inside
// [0, 2^32), where the native cast is defined.
//
// Not owned: the fitting itself (common_fit_params), the MiB report behind
// --fit-print, and the argument parser (common_params_parse, P7-10).

fn fp_copy(s: const u8[:], out: u8[:], at0: i64) -> i64 {
    var at: i64 = at0;
    var i: i64 = 0;
    while i < s.len && at < out.len {
        out[at] = s[i];
        at = at + 1;
        i = i + 1;
    }
    return at;
}

fn fp_putc(out: u8[:], at: i64, c: u8) -> i64 {
    if at < out.len {
        out[at] = c;
        return at + 1;
    }
    return at;
}

fn fp_put_int(out: u8[:], at0: i64, v0: i64) -> i64 {
    var at: i64 = at0;
    var v: i64 = v0;
    if v < 0 {
        at = fp_putc(out, at, 45);
        if v == (0 - 9223372036854775807 - 1) {
            return fp_copy("9223372036854775808", out, at);
        }
        v = 0 - v;
    }
    if v == 0 {
        return fp_putc(out, at, 48);
    }
    var buf: i64[24] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var n: i64 = 0;
    while v > 0 {
        buf[n] = v % 10;
        v = v / 10;
        n = n + 1;
    }
    var i: i64 = n;
    while i > 0 {
        i = i - 1;
        at = fp_putc(out, at, (48 + buf[i]) as u8);
    }
    return at;
}

// uint32_t(x): truncation toward zero for the defined non-negative range.
export fn fp_u32(x: f64) -> i64 {
    if x <= 0.0 {
        return 0;
    }
    return x as i64;
}

// while (nd > 1 && tensor_split[nd - 1] == 0.0f) nd--;
export fn fp_ts_count(splits: const f64[:], ndev: i64) -> i64 {
    return fp_ts_n(splits, ndev);
}
fn fp_ts_n(splits: const f64[:], ndev: i64) -> i64 {
    var nd: i64 = ndev;
    if nd > splits.len {
        nd = splits.len;
    }
    while nd > 1 && splits[nd - 1] == 0.0 {
        nd = nd - 1;
    }
    return nd;
}

// The complete line, newline included. poff/plen and boff/blen describe the
// override patterns and backend names; a negative plen is the native null
// pattern and ends the list.
export fn fp_line(n_ctx: i64, ngl: i64, splits: const f64[:], ndev: i64,
                  pats: const u8[:], poff: const i64[:], plen: const i64[:],
                  bufts: const u8[:], boff: const i64[:], blen: const i64[:],
                  n_override: i64, out: u8[:]) -> i64 {
    var at: i64 = 0;
    at = fp_copy("-c ", out, at);
    at = fp_put_int(out, at, fp_u32(n_ctx));
    at = fp_copy(" -ngl ", out, at);
    at = fp_put_int(out, at, ngl);

    let nd: i64 = fp_ts_n(splits, ndev);
    if nd > 1 {
        at = fp_copy(" -ts ", out, at);
        var id: i64 = 0;
        while id < nd {
            if id > 0 {
                at = fp_putc(out, at, 44);
            }
            at = fp_put_int(out, at, fp_u32(splits[id]));
            id = id + 1;
        }
    }

    var any: i64 = 0;
    var i: i64 = 0;
    while i < n_override && i < plen.len && plen[i] >= 0 {
        if i == 0 {
            at = fp_copy(" -ot \"", out, at);
        } else {
            at = fp_putc(out, at, 44);
        }
        var k: i64 = 0;
        while k < plen[i] && poff[i] + k < pats.len {
            at = fp_putc(out, at, pats[poff[i] + k]);
            k = k + 1;
        }
        at = fp_putc(out, at, 61);
        k = 0;
        while k < blen[i] && boff[i] + k < bufts.len {
            at = fp_putc(out, at, bufts[boff[i] + k]);
            k = k + 1;
        }
        any = 1;
        i = i + 1;
    }
    if any != 0 {
        at = fp_putc(out, at, 34);
    }
    return fp_putc(out, at, 10);
}
