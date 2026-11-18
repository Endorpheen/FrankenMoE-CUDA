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

fn main() {
    let et: i64[8] = [3, 0, 0, 0, 0, 0, 0, 0];
    let ev: i64[8] = [97, 0, 0, 0, 0, 0, 0, 0];
    let ro: i64[2] = [0, 0];
    let rl: i64[2] = [2, 0];
    let st: i64[8] = [0, 0, 0, 0, 0, 0, 0, 0];
    let cp: i64[8] = [97, 0, 98, 0, 0, 0, 0, 0];
    let cpl: i64[4] = [1, 1, 0, 0];
    let cur: i64[4] = [0, 0, 0, 0];
    let rej: i64[4] = [0, 0, 0, 0];
    let pv: i64[4] = [0, 0, 0, 0];
    let pn: i64[4] = [0, 0, 0, 0];
    let sc: i64[8] = [0, 0, 0, 0, 0, 0, 0, 0];
    let r = g_reject(et, ev, ro, rl, st, 1, cp, cpl, cur, rej, 2, pv, pn, sc, 8, 4);
    if r < 0 {
        return;
    }
}
