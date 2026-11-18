// Character class matching over the flat element arrays.
// Result encoding: matched -> (next_pos + 1), unmatched -> -(next_pos + 1),
// so the caller learns both the verdict and where the class ends.
fn g_match_char(etype: i64[:], eval: i64[:], pos_in: i64, chr: i64) -> i64 {
    var pos: i64 = pos_in;
    let t0: i64 = etype[pos];
    let positive: bool = t0 == 3 || t0 == 7;
    var found: bool = false;
    var done: bool = false;
    while !done {
        if etype[pos + 1] == 5 {
            // inclusive range
            if eval[pos] <= chr && chr <= eval[pos + 1] {
                found = true;
            }
            pos = pos + 2;
        } else if etype[pos] == 7 {
            found = true;
            pos = pos + 1;
        } else {
            if eval[pos] == chr {
                found = true;
            }
            pos = pos + 1;
        }
        if etype[pos] != 6 {
            done = true;
        }
    }
    if found == positive {
        return pos + 1;
    }
    return 0 - (pos + 1);
}


// match_partial_char: can a partial UTF-8 sequence still satisfy the class at pos?
//   n_remain = number of continuation bytes still expected, value = bits so far
fn g_match_partial(etype: i64[:], eval: i64[:], pos_in: i64, value_in: i64, n_remain: i64) -> i64 {
    var pos: i64 = pos_in;
    let t0: i64 = etype[pos];
    let positive: bool = t0 == 3 || t0 == 7;
    let partial_value: i64 = value_in;
    if n_remain < 0 || (n_remain == 1 && partial_value < 2) {
        return 0;
    }
    var shift: i64 = n_remain * 6;
    var low: i64 = partial_value * pow2(shift);
    var high: i64 = low + (pow2(shift) - 1);
    if low == 0 {
        if n_remain == 2 {
            low = 2048;
        } else if n_remain == 3 {
            low = 65536;
        }
    }
    var done: bool = false;
    while !done {
        if etype[pos + 1] == 5 {
            if eval[pos] <= high && low <= eval[pos + 1] {
                if positive { return 1; }
                return 0;
            }
            pos = pos + 2;
        } else if etype[pos] == 7 {
            return 1;
        } else {
            if low <= eval[pos] && eval[pos] <= high {
                if positive { return 1; }
                return 0;
            }
            pos = pos + 1;
        }
        if etype[pos] != 6 {
            done = true;
        }
    }
    if positive { return 0; }
    return 1;
}

fn pow2(n: i64) -> i64 {
    var v: i64 = 1;
    var i: i64 = 0;
    while i < n {
        v = v * 2;
        i = i + 1;
    }
    return v;
}


// reject_candidates for one stack, single-codepoint candidates (no partial UTF-8).
// rej[i] = 1 when candidate i is rejected. The stack transition after a class is
// candidate independent, so the descent is iterative instead of recursive.
// cand_cp is ncand rows of maxcp codepoints; curs starts at 0 for every candidate
fn g_reject(etype: i64[:], eval: i64[:], roff: i64[:], rlen: i64[:],
            st: i64[:], std_in: i64,
            cand_cp: i64[:], cp_len: i64[:], curs: i64[:], rej: i64[:], ncand: i64,
            pval: i64[:], pnr: i64[:],
            scratch: i64[:], maxd: i64, maxcp: i64) -> i64 {
    var depth: i64 = std_in;
    var i: i64 = 0;
    // copy the working stack into scratch
    while i < depth && i < maxd {
        scratch[i] = st[i];
        i = i + 1;
    }
    i = 0;
    while i < ncand {
        rej[i] = 0;
        i = i + 1;
    }
    var alive: i64 = ncand;
    var steps: i64 = 0;
    while alive > 0 && depth > 0 && steps < 4096 {
        steps = steps + 1;
        let pos: i64 = scratch[depth - 1];
        let t: i64 = etype[pos];
        if t == 3 || t == 4 || t == 7 {
            // char class at the top: test every candidate and descend past the class
            var next_depth: i64 = depth - 1;
            i = 0;
            while i < ncand {
                if rej[i] == 0 && curs[i] >= cp_len[i] && pnr[i] > 0 {
                    // codepoints exhausted but the byte sequence is incomplete: the
                    // class must be able to complete it, otherwise reject
                    if g_match_partial(etype, eval, pos, pval[i], pnr[i]) == 0 {
                        rej[i] = 1;
                        alive = alive - 1;
                    }
                }
                if rej[i] == 0 && curs[i] < cp_len[i] {
                    let r = g_match_char(etype, eval, pos, cand_cp[i * maxcp + curs[i]]);
                    if r > 0 {
                        // the class consumed this codepoint
                        curs[i] = curs[i] + 1;
                    } else {
                        rej[i] = 1;
                        alive = alive - 1;
                    }
                }
                i = i + 1;
            }
            let after2: i64 = match_end(etype, eval, pos);
            if etype[after2] == 0 {
                depth = next_depth;
            } else {
                scratch[next_depth] = after2;
                depth = next_depth + 1;
            }
            continue;
        }
        if t == 2 {
            // rule reference: expand by one alternate level
            let rid: i64 = eval[pos];
            if roff[rid] >= 0 && etype[roff[rid]] != 0 {
                scratch[depth - 1] = roff[rid];
            } else {
                depth = depth - 1;
            }
            continue;
        }
        // END or unsupported: stop
        break;
    }
    // a candidate with codepoints left when the stack is exhausted cannot be
    // consumed by this grammar state
    i = 0;
    while i < ncand {
        if rej[i] == 0 && curs[i] < cp_len[i] {
            rej[i] = 1;
            alive = alive - 1;
        }
        i = i + 1;
    }
    return alive;
}

// end position of the class at pos (the native uses match_char(pos, 0).second)
fn match_end(etype: i64[:], eval: i64[:], pos_in: i64) -> i64 {
    var pos: i64 = pos_in;
    var done: bool = false;
    while !done {
        if etype[pos + 1] == 5 {
            pos = pos + 2;
        } else if etype[pos] == 7 {
            pos = pos + 1;
        } else {
            pos = pos + 1;
        }
        if etype[pos] != 6 {
            done = true;
        }
    }
    return pos;
}


// reject across several stacks: a candidate is rejected only when every stack
// rejects it, which is the native intersection of per-stack reject sets.
// stacks is nstacks rows of maxd element indexes; stackd holds each depth.
fn g_reject_multi(etype: i64[:], eval: i64[:], roff: i64[:], rlen: i64[:],
                  stacks: i64[:], stackd: i64[:], nstacks: i64,
                  cand_cp: i64[:], cp_len: i64[:], pval: i64[:], pnr: i64[:],
                  rej: i64[:], scratch: i64[:], tmp_rej: i64[:], tmp_cur: i64[:],
                  rowbuf: i64[:],
                  ncand: i64, maxd: i64, maxcp: i64) -> i64 {
    var i: i64 = 0;
    while i < ncand {
        rej[i] = 1;
        i = i + 1;
    }
    var s: i64 = 0;
    while s < nstacks {
        // reset the per-candidate cursors and evaluate one stack
        i = 0;
        while i < ncand {
            tmp_cur[i] = 0;
            tmp_rej[i] = 0;
            i = i + 1;
        }
        var d: i64 = 0;
        while d < stackd[s] {
            rowbuf[d] = stacks[s * maxd + d];
            d = d + 1;
        }
        let ignored = g_reject(etype, eval, roff, rlen,
                               rowbuf, stackd[s],
                               cand_cp, cp_len, tmp_cur, tmp_rej, ncand,
                               pval, pnr, scratch, maxd, maxcp);
        i = 0;
        while i < ncand {
            if tmp_rej[i] == 0 {
                rej[i] = 0;
            }
            i = i + 1;
        }
        s = s + 1;
    }
    var nrej: i64 = 0;
    i = 0;
    while i < ncand {
        nrej = nrej + rej[i];
        i = i + 1;
    }
    return nrej;
}

fn main() {
    let et: i64[8] = [3, 0, 3, 0, 0, 0, 0, 0];
    let ev: i64[8] = [97, 0, 98, 0, 0, 0, 0, 0];
    let ro: i64[2] = [0, 0];
    let rl: i64[2] = [4, 0];
    let stk: i64[16] = [0,0,0,0,0,0,0,0, 2,0,0,0,0,0,0,0];
    let std: i64[2] = [1, 1];
    let cp: i64[6] = [97, 98, 122, 0, 0, 0];
    let cpl: i64[3] = [1, 1, 1];
    let pv: i64[3] = [0, 0, 0];
    let pn: i64[3] = [0, 0, 0];
    let rej: i64[3] = [0, 0, 0];
    let sc: i64[8] = [0,0,0,0,0,0,0,0];
    let tr: i64[3] = [0, 0, 0];
    let tc: i64[3] = [0, 0, 0];
    let rb: i64[8] = [0,0,0,0,0,0,0,0];
    let r = g_reject_multi(et, ev, ro, rl, stk, std, 2, cp, cpl, pv, pn, rej, sc, tr, tc, rb, 3, 8, 2);
    if r < 0 {
        return;
    }
}
