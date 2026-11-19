// Hot reload reconciliation (P8-06 S2) in Goose, from
// tools/server/server-models.cpp: server_models::load_models() 890-1220, the
// reload branch 1088-1210 and the helpers it reads.
//
// Owned decisions, in the native order:
//   1. the preset comparison that decides a reload: two presets differ when
//      their option maps differ after HOST, PORT, ALIAS and LOG_COLORS are
//      stripped, because update_args() injects them;
//   2. which running models a reload unloads: the name is gone from the source,
//      or its preset changed;
//   3. which entries a reload erases: a finished download always, an entry no
//      source lists any more, but never a download still in flight;
//   4. the alias pass: a preset alias is split on ',', stripped, emptied away,
//      then accepted unless the name or an alias of another model already
//      holds it. Rows are walked in name order and each accepted alias is
//      visible to the rows after it, exactly as the native map walk does;
//   5. tags: the same split and strip, no conflict rule;
//   6. the load-on-startup list: its order, its truthy vocabulary and the
//      models_max check that refuses the whole reload;
//   7. the dedup-cache-model hiding rule.
//
// Presets are data. `copt`/`popt` hold "KEY=VALUE" entries separated by '\n',
// keys unique and in byte order, which is what std::map<option> iterates.
// Alias sets are i64 bitmasks over a universe of strings: the model names plus
// every alias any preset asks for, sorted byte-wise, at most 60 entries. The
// host builds the universe, so a preset alias outside it is not modelled.

import std;

const HR_MAX: i64 = 60;
const HR_BUF: i64 = 8192;

const HR_SRC_CACHE: i64 = 2;

const HR_DOWNLOADING: i64 = 0;
const HR_DOWNLOADED: i64 = 2;
const HR_UNLOADED: i64 = 3;
const HR_LOADING: i64 = 4;
const HR_LOADED: i64 = 5;
const HR_SLEEPING: i64 = 6;

// ── byte helpers ───────────────────────────────────────────────────────────

fn hr_put_range(dst: u8[:], at: i64, s: const u8[:], pos_from: i64, n: i64) -> i64 {
    var p = at;
    var i: i64 = 0;

    while i < n {
        if p < dst.len {
            dst[p] = s[pos_from + i];
        }
        p = p + 1;
        i = i + 1;
    }
    return p;
}

fn hr_puts(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    return hr_put_range(dst, at, s, 0, s.len);
}

fn hr_putc(dst: u8[:], at: i64, c: u8) -> i64 {
    if at < dst.len {
        dst[at] = c;
    }
    return at + 1;
}

fn hr_put_i(dst: u8[:], at: i64, v: i64) -> i64 {
    var buf: u8[24] = [0; 24];
    var n: i64 = 0;
    var x = v;
    var q = at;

    if x < 0 {
        q = hr_putc(dst, q, 45);
        x = 0 - x;
    }
    if x == 0 {
        return hr_putc(dst, q, 48);
    }
    while x > 0 {
        buf[n] = (48 + x % 10) as u8;
        x = x / 10;
        n = n + 1;
    }
    var i = n;

    while i > 0 {
        i = i - 1;
        q = hr_putc(dst, q, buf[i]);
    }
    return q;
}

fn hr_cmp(a: const u8[:], ao: i64, al: i64, b: const u8[:], bo: i64, bl: i64) -> i64 {
    var n = al;

    if bl < n {
        n = bl;
    }
    var i: i64 = 0;

    while i < n {
        if a[ao + i] < b[bo + i] {
            return 0 - 1;
        }
        if a[ao + i] > b[bo + i] {
            return 1;
        }
        i = i + 1;
    }
    // a proper prefix sorts first
    if al < bl {
        return 0 - 1;
    }
    if al > bl {
        return 1;
    }
    return 0;
}

fn hr_eq(a: const u8[:], ao: i64, al: i64, b: const u8[:], bo: i64, bl: i64) -> bool {
    return hr_cmp(a, ao, al, b, bo, bl) == 0;
}

fn hr_eq_lit(a: const u8[:], ao: i64, al: i64, lit: const u8[:]) -> bool {
    return hr_eq(a, ao, al, lit, 0, lit.len);
}

// std::isspace in the C locale
fn hr_space(c: u8) -> i64 {
    if c == 32 {
        return 1;
    }
    if c == 9 || c == 10 || c == 11 || c == 12 || c == 13 {
        return 1;
    }
    return 0;
}

// string_strip: std::isspace at both ends
fn hr_strip_lo(s: const u8[:], pos_from: i64, len: i64) -> i64 {
    var a = pos_from;
    let end = pos_from + len;

    while a < end && hr_space(s[a]) != 0 {
        a = a + 1;
    }
    return a;
}

fn hr_strip_hi(s: const u8[:], pos_from: i64, len: i64) -> i64 {
    var e = pos_from + len;
    let a = hr_strip_lo(s, pos_from, len);

    while e > a && hr_space(s[e - 1]) != 0 {
        e = e - 1;
    }
    return e;
}

fn hr_bit(i: i64) -> i64 {
    var v: i64 = 1;
    var k: i64 = 0;

    while k < i {
        v = v * 2;
        k = k + 1;
    }
    return v;
}

fn hr_has(mask: i64, i: i64) -> i64 {
    if i < 0 || i >= HR_MAX {
        return 0;
    }
    let b = hr_bit(i);

    if (mask / b) % 2 == 0 {
        return 0;
    }
    return 1;
}

fn hr_set(mask: i64, i: i64) -> i64 {
    if i < 0 || i >= HR_MAX {
        return mask;
    }
    if hr_has(mask, i) != 0 {
        return mask;
    }
    return mask + hr_bit(i);
}

// indices 0..n-1 in std::map (byte-lexicographic) name order
fn hr_sort_rows(n: i64, names: const u8[:], off: i64[:], ln: i64[:], perm: i64[:]) -> i64 {
    var i: i64 = 0;

    while i < n {
        var j = i;
        var placed: i64 = 0;

        while j > 0 && placed == 0 {
            let a = perm[j - 1];

            if hr_cmp(names, off[a], ln[a], names, off[i], ln[i]) <= 0 {
                placed = 1;
            } else {
                perm[j] = a;
                j = j - 1;
            }
        }
        perm[j] = i;
        i = i + 1;
    }
    return n;
}

fn hr_is_running(st: i64) -> i64 {
    if st == HR_LOADED || st == HR_LOADING || st == HR_SLEEPING {
        return 1;
    }
    return 0;
}

// ── preset comparison (preset_options_for_compare 1029-1036) ───────────────

// preset_options_for_compare 1026-1034 and the option-map comparison at 1113.
// Write the option entries with HOST, PORT, ALIAS and LOG_COLORS removed, one
// per line. Two presets are the same when their normalised text is the same.
fn hr_preset_norm_body(opts: const u8[:], pos_from: i64, len: i64, out: u8[:]) -> i64 {
    let end = pos_from + len;
    var p: i64 = 0;
    var pos = pos_from;

    while pos < end {
        var e = pos;

        while e < end && opts[e] != 10 {
            e = e + 1;
        }
        // one entry [pos, e): KEY=VALUE, or a bare token with no '='
        var eq = pos;

        while eq < e && opts[eq] != 61 {
            eq = eq + 1;
        }
        var skip: i64 = 0;

        if eq < e {
            let kl = eq - pos;
            if hr_eq_lit(opts, pos, kl, "LLAMA_ARG_HOST") {
                skip = 1;
            }
            if hr_eq_lit(opts, pos, kl, "LLAMA_ARG_PORT") {
                skip = 1;
            }
            if hr_eq_lit(opts, pos, kl, "LLAMA_ARG_ALIAS") {
                skip = 1;
            }
            if hr_eq_lit(opts, pos, kl, "LLAMA_ARG_LOG_COLORS") {
                skip = 1;
            }
        }
        if skip == 0 {
            if p > 0 {
                p = hr_putc(out, p, 10);
            }
            p = hr_put_range(out, p, opts, pos, e - pos);
        }
        pos = e + 1;
    }
    return p;
}

export fn hr_preset_norm(opts: const u8[:], pos_from: i64, len: i64, out: u8[:]) -> i64 {
    return hr_preset_norm_body(opts, pos_from, len, out);
}

// ── the reload diff ────────────────────────────────────────────────────────

// 1091-1118
// out[i] = mapping row, reasons[i] = 1 the source dropped it, 2 the preset
// changed. Rows are emitted in name order, as the native map walk does.
fn hr_unload_body(n: i64, names: const u8[:], off: i64[:], ln: i64[:], status: i64[:],
                  copt: const u8[:], co: i64[:], cl: i64[:],
                  nm: i64, nnames: const u8[:], noff: i64[:], nln: i64[:],
                  popt: const u8[:], po: i64[:], pl: i64[:],
                  out: i64[:], reasons: i64[:]) -> i64 {
    var perm: i64[HR_MAX] = [0; HR_MAX];
    hr_sort_rows(n, names, off, ln, perm);

    var na: u8[HR_BUF] = [0; HR_BUF];
    var nb: u8[HR_BUF] = [0; HR_BUF];

    var count: i64 = 0;
    var i: i64 = 0;

    while i < n {
        let r = perm[i];

        if hr_is_running(status[r]) == 0 {
            i = i + 1;
        } else {
            var found: i64 = 0 - 1;
            var k: i64 = 0;

            while k < nm {
                if hr_eq(nnames, noff[k], nln[k], names, off[r], ln[r]) {
                    found = k;
                    k = nm;
                } else {
                    k = k + 1;
                }
            }
            if found < 0 {
                if count < out.len {
                    out[count] = r;
                    reasons[count] = 1;
                }
                count = count + 1;
            } else {
                let la = hr_preset_norm_body(copt, co[r], cl[r], na);
                let lb = hr_preset_norm_body(popt, po[found], pl[found], nb);

                if hr_eq(na, 0, la, nb, 0, lb) == false {
                    if count < out.len {
                        out[count] = r;
                        reasons[count] = 2;
                    }
                    count = count + 1;
                }
            }
            i = i + 1;
        }
    }
    return count;
}

export fn hr_unload_set(n: i64, names: const u8[:], off: i64[:], ln: i64[:], status: i64[:],
                        copt: const u8[:], co: i64[:], cl: i64[:],
                        nm: i64, nnames: const u8[:], noff: i64[:], nln: i64[:],
                        popt: const u8[:], po: i64[:], pl: i64[:],
                        out: i64[:], reasons: i64[:]) -> i64 {
    return hr_unload_body(n, names, off, ln, status, copt, co, cl, nm, nnames, noff, nln, popt, po, pl, out, reasons);
}

// 1120-1133: entries erased during the reload, in name order
fn hr_erase_body(n: i64, names: const u8[:], off: i64[:], ln: i64[:], status: i64[:],
                 nm: i64, nnames: const u8[:], noff: i64[:], nln: i64[:],
                 out: i64[:]) -> i64 {
    var perm: i64[HR_MAX] = [0; HR_MAX];
    hr_sort_rows(n, names, off, ln, perm);

    var count: i64 = 0;
    var i: i64 = 0;

    while i < n {
        let r = perm[i];
        var erase: i64 = 0;

        if status[r] == HR_DOWNLOADING {
            erase = 0; // the download thread is still busy, skip
        } else if status[r] == HR_DOWNLOADED {
            erase = 1; // the download finished and was added back by this load
        } else {
            var found: i64 = 0;
            var k: i64 = 0;

            while k < nm {
                if hr_eq(nnames, noff[k], nln[k], names, off[r], ln[r]) {
                    found = 1;
                    k = nm;
                } else {
                    k = k + 1;
                }
            }
            if found == 0 {
                erase = 1;
            }
        }
        if erase != 0 {
            if count < out.len {
                out[count] = r;
            }
            count = count + 1;
        }
        i = i + 1;
    }
    return count;
}

export fn hr_erase_set(n: i64, names: const u8[:], off: i64[:], ln: i64[:], status: i64[:],
                       nm: i64, nnames: const u8[:], noff: i64[:], nln: i64[:],
                       out: i64[:]) -> i64 {
    return hr_erase_body(n, names, off, ln, status, nm, nnames, noff, nln, out);
}

// ── aliases (1139-1169) ────────────────────────────────────────────────────

fn hr_uni_find(uni: const u8[:], uoff: i64[:], ulen: i64[:], nu: i64,
               s: const u8[:], pos_from: i64, len: i64) -> i64 {
    var i: i64 = 0;

    while i < nu {
        if hr_eq(uni, uoff[i], ulen[i], s, pos_from, len) {
            return i;
        }
        i = i + 1;
    }
    return 0 - 1;
}

// 1143-1166
// Apply one reload's alias pass to every non-running row. `masks` arrives with
// the sets the mapping holds now and leaves with the accepted ones; running
// rows are left untouched. Conflict lines go to `conf` as
// "alias|model|other" separated by '\n', one per refused alias.
fn hr_alias_body(n: i64,
                 uni: const u8[:], uoff: i64[:], ulen: i64[:], nu: i64,
                 nm: const u8[:], noff: i64[:], nlen: i64[:],
                 astr: const u8[:], ao: i64[:], al: i64[:],
                 running: i64[:], masks: i64[:], conf: u8[:], meta: i64[:]) -> i64 {
    var perm: i64[HR_MAX] = [0; HR_MAX];
    hr_sort_rows(n, nm, noff, nlen, perm);

    var count: i64 = 0;
    var cp: i64 = 0;
    var i: i64 = 0;

    while i < n {
        let r = perm[i];

        if running[r] != 0 {
            i = i + 1;
        } else {
            var want: i64 = 0;
            let aend = ao[r] + al[r];
            var pos = ao[r];

            while pos < aend {
                var e = pos;

                while e < aend && astr[e] != 44 {
                    e = e + 1;
                }
                let s = hr_strip_lo(astr, pos, e - pos);
                let t = hr_strip_hi(astr, pos, e - pos);

                if t > s {
                    let u = hr_uni_find(uni, uoff, ulen, nu, astr, s, t - s);
                    if u >= 0 {
                        want = hr_set(want, u);
                    }
                }
                pos = e + 1;
            }

            var keep: i64 = 0;
            var b: i64 = 0;

            while b < nu {
                if hr_has(want, b) != 0 {
                    // the native set walks the candidates in string order
                    var other: i64 = 0 - 1;
                    var j: i64 = 0;

                    while j < n {
                        let o = perm[j];

                        if o != r {
                            var clash: i64 = 0;

                            if hr_eq(nm, noff[o], nlen[o], uni, uoff[b], ulen[b]) {
                                clash = 1;
                            }
                            if hr_has(masks[o], b) != 0 {
                                clash = 1;
                            }
                            if clash != 0 {
                                other = o;
                                j = n;
                            } else {
                                j = j + 1;
                            }
                        } else {
                            j = j + 1;
                        }
                    }
                    if other < 0 {
                        keep = hr_set(keep, b);
                    } else {
                        cp = hr_put_range(conf, cp, uni, uoff[b], ulen[b]);
                        cp = hr_putc(conf, cp, 124);
                        cp = hr_put_range(conf, cp, nm, noff[r], nlen[r]);
                        cp = hr_putc(conf, cp, 124);
                        cp = hr_put_range(conf, cp, nm, noff[other], nlen[other]);
                        cp = hr_putc(conf, cp, 10);
                        count = count + 1;
                    }
                }
                b = b + 1;
            }
            masks[r] = keep;
            i = i + 1;
        }
    }
    meta[0] = count;
    return cp;
}

export fn hr_alias_apply(n: i64,
                         uni: const u8[:], uoff: i64[:], ulen: i64[:], nu: i64,
                         nm: const u8[:], noff: i64[:], nlen: i64[:],
                         astr: const u8[:], ao: i64[:], al: i64[:],
                         running: i64[:], masks: i64[:], conf: u8[:], meta: i64[:]) -> i64 {
    return hr_alias_body(n, uni, uoff, ulen, nu, nm, noff, nlen, astr, ao, al, running, masks, conf, meta);
}

// ── tags (1167-1180) ───────────────────────────────────────────────────────

// The tag set of one preset, sorted and joined by ','. An absent option and an
// empty one both yield nothing.
export fn hr_tags(astr: const u8[:], present: i64, out: u8[:]) -> i64 {
    if present == 0 {
        return 0;
    }
    var picked: i64[HR_MAX] = [0; HR_MAX];
    var npick: i64 = 0;
    let end = astr.len;
    var pos: i64 = 0;

    while pos <= end {
        var e = pos;

        while e < end && astr[e] != 44 {
            e = e + 1;
        }
        let s = hr_strip_lo(astr, pos, e - pos);
        let t = hr_strip_hi(astr, pos, e - pos);

        if t > s {
            var dup: i64 = 0;
            var k: i64 = 0;

            while k < npick {
                if hr_eq(astr, picked[2 * k], picked[2 * k + 1], astr, s, t - s) {
                    dup = 1;
                }
                k = k + 1;
            }
            if dup == 0 && npick < HR_MAX {
                picked[2 * npick] = s;
                picked[2 * npick + 1] = t - s;
                npick = npick + 1;
            }
        }
        pos = e + 1;
    }

    // std::set<string> order: byte-lexicographic
    var order: i64[HR_MAX] = [0; HR_MAX];
    var i: i64 = 0;

    while i < npick {
        var j = i;
        var placed: i64 = 0;

        while j > 0 && placed == 0 {
            let a = order[j - 1];

            if hr_cmp(astr, picked[2 * a], picked[2 * a + 1], astr, picked[2 * i], picked[2 * i + 1]) <= 0 {
                placed = 1;
            } else {
                order[j] = a;
                j = j - 1;
            }
        }
        order[j] = i;
        i = i + 1;
    }

    var p: i64 = 0;
    var m: i64 = 0;

    while m < npick {
        let a = order[m];
        if m > 0 {
            p = hr_putc(out, p, 44);
        }
        p = hr_put_range(out, p, astr, picked[2 * a], picked[2 * a + 1]);
        m = m + 1;
    }
    return p;
}

// ── load-on-startup (1067-1086) ────────────────────────────────────────────

// common_arg_utils::is_truthy, common/arg.cpp 1337-1339
fn hr_truthy_range(value: const u8[:], pos_from: i64, len: i64) -> i64 {
    if hr_eq(value, pos_from, len, "on", 0, 2) {
        return 1;
    }
    if hr_eq(value, pos_from, len, "enabled", 0, 7) {
        return 1;
    }
    if hr_eq(value, pos_from, len, "true", 0, 4) {
        return 1;
    }
    if hr_eq(value, pos_from, len, "1", 0, 1) {
        return 1;
    }
    return 0;
}

export fn hr_truthy(value: const u8[:]) -> i64 {
    return hr_truthy_range(value, 0, value.len);
}

// 1067-1083: the models a first load queues, in name order
fn hr_startup_pick_body(n: i64, names: const u8[:], off: i64[:], ln: i64[:],
                        vstr: const u8[:], vo: i64[:], vl: i64[:], present: i64[:],
                        out: i64[:]) -> i64 {
    var perm: i64[HR_MAX] = [0; HR_MAX];
    hr_sort_rows(n, names, off, ln, perm);

    var count: i64 = 0;
    var i: i64 = 0;

    while i < n {
        let r = perm[i];

        if present[r] != 0 {
            var q: u8[64] = [0; 64];
            let m = hr_put_range(q, 0, vstr, vo[r], vl[r]);

            if hr_truthy_range(q, 0, m) != 0 {
                if count < out.len {
                    out[count] = r;
                }
                count = count + 1;
            }
        }
        i = i + 1;
    }
    return count;
}

export fn hr_startup_pick(n: i64, names: const u8[:], off: i64[:], ln: i64[:],
                          vstr: const u8[:], vo: i64[:], vl: i64[:], present: i64[:],
                          out: i64[:]) -> i64 {
    return hr_startup_pick_body(n, names, off, ln, vstr, vo, vl, present, out);
}

// 1078-1086: the models_max guard of a first load, 1 refuses the whole load
export fn hr_startup(count: i64, models_max: i64, out: u8[:]) -> i64 {
    if count > models_max {
        var p = hr_puts(out, 0, "number of models to load on startup (");
        p = hr_put_i(out, p, count);
        p = hr_puts(out, p, ") exceeds models_max (");
        p = hr_put_i(out, p, models_max);
        p = hr_putc(out, p, 41);
        return 1;
    }
    return 0;
}

// ── dedup-cache hiding (944-975) ───────────────────────────────────────────

// A cache model is hidden when its resolved file is the file a dedup preset
// resolves to. `pure` marks a cache entry no other source merged into.
fn hr_hidden_body(n: i64,
                  cpath: const u8[:], poff: i64[:], plen: i64[:], pure: i64[:],
                  np: i64, paths: const u8[:], xoff: i64[:], xlen: i64[:],
                  out: i64[:]) -> i64 {
    var count: i64 = 0;
    var i: i64 = 0;

    while i < n {
        var hidden: i64 = 0;

        if pure[i] != 0 && plen[i] > 0 {
            var k: i64 = 0;

            while k < np {
                if hr_eq(paths, xoff[k], xlen[k], cpath, poff[i], plen[i]) {
                    hidden = 1;
                    k = np;
                } else {
                    k = k + 1;
                }
            }
        }
        out[i] = hidden;
        count = count + hidden;
        i = i + 1;
    }
    return count;
}

export fn hr_hidden(n: i64,
                    cpath: const u8[:], poff: i64[:], plen: i64[:], pure: i64[:],
                    np: i64, paths: const u8[:], xoff: i64[:], xlen: i64[:],
                    out: i64[:]) -> i64 {
    return hr_hidden_body(n, cpath, poff, plen, pure, np, paths, xoff, xlen, out);
}

// 975: the log line a hidden cache model prints, one per line
export fn hr_hidden_log(n: i64, cnames: const u8[:], coff: i64[:], clen: i64[:], flags: i64[:], out: u8[:]) -> i64 {
    var p: i64 = 0;
    var i: i64 = 0;

    while i < n {
        if flags[i] != 0 {
            p = hr_puts(out, p, "hiding cache model name=");
            p = hr_put_range(out, p, cnames, coff[i], clen[i]);
            p = hr_puts(out, p, " (deduplicated by a preset)");
            p = hr_putc(out, p, 10);
        }
        i = i + 1;
    }
    return p;
}
