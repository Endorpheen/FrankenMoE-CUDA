// P10-12 S4: the `moe-predict` accuracy report (gap G6).
//
// Native source: runtime/cli/main.cpp at base commit 1b3f4a1a, the static
// function at 540-588:
//
//   print_predict_report(const RunSummary & s)
//
// It is a public entrypoint's only unported logic and it is pure: no model, no
// context, no I/O beyond stdout. Four aggregate lines, an optional warning,
// then one row per MoE layer. The decisions are:
//
//   - the three predictors are quoted against the CONTROL, not 100%, because
//     the control ranks with no staleness and therefore bounds what this
//     measurement can show on this model;
//   - the two-layer horizon prints only when it has rows;
//   - the "control is below 100%" warning prints only when the control has
//     rows and its slot fraction is under 0.999;
//   - a layer with no routings at all is a dense layer and prints no row;
//   - a predictor with no routings AT that layer prints "-", never 0.0 —
//     layer 0 is the case: nothing precedes it, so the stale gate
//     structurally cannot reach it.
//
// The prefix is mp_. One conversion stays libc: `%.1f`, through the
// `mp_fmt_f1` extern, because binary-to-decimal for a double is the same
// concession mt_fmt_g6 makes in metrics-traces.goose. Everything else — the
// padding widths, the `-` rule, the row layout — is computed here.
//
// The report writes into `out` and returns the byte count, newline-terminated,
// exactly as the native printf sequence would leave it on stdout.

const MP_HIT: i64 = 0;
const MP_SLOTS: i64 = 1;
const MP_HITS: i64 = 2;
const MP_EXACT: i64 = 3;

extern "mp_fmt_f1" fn mp_fmt_f1(out: u8[:], at: i64, x: f64) -> i64;

fn mp_put(out: u8[:], pos: i64, c: u8) -> i64 {
    if pos >= 0 && pos < out.len {
        out[pos] = c;
    }
    return pos + 1;
}

fn mp_put_lit(out: u8[:], pos: i64, s: const u8[:]) -> i64 {
    var p: i64 = pos;
    for c in s {
        p = mp_put(out, p, c);
    }
    return p;
}

fn mp_put_u64(out: u8[:], pos: i64, v: u64) -> i64 {
    var buf: u8[24] = [0; 24];
    var n: i64 = 0;
    var x: u64 = v;
    if x == 0 {
        buf[0] = '0';
        n = 1;
    }
    while x > 0 {
        buf[n] = '0' + (x % 10) as u8;
        n = n + 1;
        x = x / 10;
    }
    var p: i64 = pos;
    var i: i64 = n - 1;
    while i >= 0 {
        p = mp_put(out, p, buf[i]);
        i = i - 1;
    }
    return p;
}

fn mp_put_i64(out: u8[:], pos: i64, v: i64) -> i64 {
    if v < 0 {
        let zero: u64 = 0;
        var p: i64 = mp_put(out, pos, '-');
        return mp_put_u64(out, p, zero - (v as u64));
    }
    return mp_put_u64(out, pos, v as u64);
}

// Right-aligned decimal in `width` columns, spaces on the left: `%5d`, `%6lld`.
fn mp_pad_i64(out: u8[:], pos: i64, v: i64, width: i64) -> i64 {
    var buf: u8[24] = [0; 24];
    var n: i64 = 0;
    var x: u64 = 0;
    var neg: i64 = 0;
    if v < 0 {
        let zero: u64 = 0;
        neg = 1;
        x = zero - (v as u64);
    } else {
        x = v as u64;
    }
    if x == 0 {
        buf[0] = '0';
        n = 1;
    }
    while x > 0 {
        buf[n] = '0' + (x % 10) as u8;
        n = n + 1;
        x = x / 10;
    }
    var total: i64 = n + neg;
    var p: i64 = pos;
    var nsp: i64 = width - total;
    while nsp > 0 {
        p = mp_put(out, p, ' ');
        nsp = nsp - 1;
    }
    if neg != 0 {
        p = mp_put(out, p, '-');
    }
    var i: i64 = n - 1;
    while i >= 0 {
        p = mp_put(out, p, buf[i]);
        i = i - 1;
    }
    return p;
}

// `%6.1f` in the table columns: one decimal, right-aligned to six columns.
// The one conversion libc owns writes at `pos`, then the bytes are shifted
// right to make room for the padding — the two stay in one buffer so no
// temporary view of an array is needed.
fn mp_pad_f1(out: u8[:], pos: i64, x: f64, width: i64) -> i64 {
    var n: i64 = mp_fmt_f1(out, pos, x);
    if n < 0 {
        n = 0;
    }
    if n > width {
        return pos + n;
    }
    var room: i64 = width - n;
    var i: i64 = n - 1;
    while i >= 0 {
        if pos + room + i < out.len {
            out[pos + room + i] = out[pos + i];
        }
        i = i - 1;
    }
    var p: i64 = pos;
    while room > 0 {
        p = mp_put(out, p, ' ');
        room = room - 1;
    }
    return p + n;
}

fn mp_hit(p: i64[:], base: i64) -> f64 {
    let slots: i64 = p[base + MP_SLOTS];
    if slots <= 0 {
        return 0.0;
    }
    return (p[base + MP_HITS] as f64) / slots;
}

fn mp_exact(p: i64[:], base: i64) -> f64 {
    let rows: i64 = p[base + MP_HIT];
    if rows <= 0 {
        return 0.0;
    }
    return (p[base + MP_EXACT] as f64) / rows;
}

// One table column: `%6.1f` of the slot fraction, or the literal `-` padded to
// the same six columns when the predictor scored no routing at this layer.
fn mp_pct(out: u8[:], pos: i64, rows: i64, slots: i64, hits: i64) -> i64 {
    if rows == 0 {
        return mp_put_lit(out, pos, "     -");
    }
    var f: f64 = 0.0;
    if slots > 0 {
        // Both operands must be f64 explicitly: `/` on two i64 is this
        // language's integer division, and hit_frac is a double division.
        let hits_f: f64 = hits;
        let slots_f: f64 = slots;
        f = hits_f / slots_f;
    }
    return mp_pad_f1(out, pos, 100.0 * f, 6);
}

// The report. `head` is 12 values: rows/slots/hits/exact for the stale gate,
// then prev-token, then the fresh-gate control. `stale2` is the same 4 for the
// two-layer horizon. `per_layer` is 12 values per layer in the same order.
export fn mp_report(head: i64[:], stale2: i64[:], unscored: i64, n_layers: i64, per_layer: i64[:],
                    out: u8[:]) -> i64 {
    var p: i64 = 0;
    // Line 1: aggregate over the three predictors, all against the control.
    p = mp_put_lit(out, p, "moe-predict: stale-gate ");
    p = mp_pad_f1(out, p, 100.0 * mp_hit(head, 0), 1);
    p = mp_put_lit(out, p, "% of routed slots (");
    p = mp_pad_f1(out, p, 100.0 * mp_exact(head, 0), 1);
    p = mp_put_lit(out, p, "% whole routings) | prev-token ");
    p = mp_pad_f1(out, p, 100.0 * mp_hit(head, 4), 1);
    p = mp_put_lit(out, p, "% (");
    p = mp_pad_f1(out, p, 100.0 * mp_exact(head, 4), 1);
    p = mp_put_lit(out, p, "%) | fresh-gate control ");
    p = mp_pad_f1(out, p, 100.0 * mp_hit(head, 8), 1);
    p = mp_put_lit(out, p, "% (");
    p = mp_pad_f1(out, p, 100.0 * mp_exact(head, 8), 1);
    p = mp_put_lit(out, p, "%)\n");

    // Line 2: the horizon the async prefetch actually runs at, aggregate only.
    if stale2[MP_HIT] > 0 {
        p = mp_put_lit(out, p, "moe-predict: stale-2 (two layers early) ");
        p = mp_pad_f1(out, p, 100.0 * mp_hit(stale2, 0), 1);
        p = mp_put_lit(out, p, "% of routed slots (");
        p = mp_pad_f1(out, p, 100.0 * mp_exact(stale2, 0), 1);
        p = mp_put_lit(out, p, "% whole routings)\n");
    }

    // Line 3: the denominators, which genuinely differ per predictor.
    p = mp_put_lit(out, p, "moe-predict: scored — stale-gate ");
    p = mp_put_i64(out, p, head[0]);
    p = mp_put_lit(out, p, " routings/");
    p = mp_put_i64(out, p, head[1]);
    p = mp_put_lit(out, p, " slots, prev-token ");
    p = mp_put_i64(out, p, head[4]);
    p = mp_put_lit(out, p, "/");
    p = mp_put_i64(out, p, head[5]);
    p = mp_put_lit(out, p, ", control ");
    p = mp_put_i64(out, p, head[8]);
    p = mp_put_lit(out, p, "/");
    p = mp_put_i64(out, p, head[9]);
    p = mp_put_lit(out, p, "; ");
    p = mp_put_i64(out, p, unscored);
    p = mp_put_lit(out, p, " routings the stale-gate could not rank\n");

    // Line 4: say it rather than let the reader assume staleness cost what the
    // ranking did. Two predicates: the control has rows, and its slot fraction
    // is under 0.999 (a control with no slots reads as 0.0 there, as native).
    if head[8] > 0 {
        var warn: i64 = 0;
        if head[9] > 0 {
            if mp_hit(head, 8) < 0.999 {
                warn = 1;
            }
        } else {
            warn = 1;
        }
        if warn != 0 {
            p = mp_put_lit(out, p, "moe-predict: the control is below 100%, so this model's expert selection is not raw-logit\n");
            p = mp_put_lit(out, p, "             ranking (an added selection bias, or group-limited routing). The stale-gate\n");
            p = mp_put_lit(out, p, "             figure understates the method by about the control's own gap.\n");
        }
    }

    if n_layers == 0 {
        return p;
    }
    p = mp_put_lit(out, p, "  layer   stale    prev   ctrl   routings\n");
    var il: i64 = 0;
    while il < n_layers {
        let b: i64 = il * 12;
        let a_rows: i64 = per_layer[b + 0];
        let b_rows: i64 = per_layer[b + 4];
        let c_rows: i64 = per_layer[b + 8];
        // A dense layer routes nothing and prints no row.
        if !(a_rows == 0 && b_rows == 0 && c_rows == 0) {
            p = mp_put_lit(out, p, "  ");
            p = mp_pad_i64(out, p, il, 5);
            p = mp_put_lit(out, p, "  ");
            p = mp_pct(out, p, a_rows, per_layer[b + 1], per_layer[b + 2]);
            p = mp_put_lit(out, p, "  ");
            p = mp_pct(out, p, b_rows, per_layer[b + 5], per_layer[b + 6]);
            p = mp_put_lit(out, p, " ");
            p = mp_pct(out, p, c_rows, per_layer[b + 9], per_layer[b + 10]);
            p = mp_put_lit(out, p, "     ");
            p = mp_pad_i64(out, p, a_rows, 6);
            p = mp_put(out, p, '\n');
        }
        il = il + 1;
    }
    return p;
}
