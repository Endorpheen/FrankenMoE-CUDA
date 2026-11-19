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


// Grammar automaton, index model. A position is a global element index.
// In-progress stacks live in the work pool; only stacks whose top is terminal
// (or that are empty) are appended to the result, as the native code does.
//   etype/eval      element type and value
//   roff/rlen       rule start and element count
//   wp/wpd          work pool, max_stacks rows of max_depth
//   out/outd        result stacks
fn g_advance(etype: i64[:], eval: i64[:], roff: i64[:], rlen: i64[:],
             seed: i64[:], seedd: i64,
             wp: i64[:], wpd: i64[:], out: i64[:], outd: i64[:],
             base: i64,
             max_stacks: i64, max_depth: i64) -> i64 {
    var nwork: i64 = 0;
    var nout: i64 = 0;
    var queue: i64[128] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
    var qn: i64 = 0;
    var i: i64 = 0;
    while i < seedd && i < max_depth {
        wp[i] = seed[i];
        i = i + 1;
    }
    wpd[0] = seedd;
    nwork = 1;
    queue[0] = 0;
    qn = 1;
    while qn > 0 {
        qn = qn - 1;
        let sid: i64 = queue[qn];
        let depth: i64 = wpd[sid];
        if depth == 0 {
            // empty stack: final, the grammar can end here
            if nout < max_stacks {
                outd[base + nout] = 0;
                nout = nout + 1;
            }
            continue;
        }
        let pos: i64 = wp[sid * max_depth + depth - 1];
        let t: i64 = etype[pos];
        if t == 3 || t == 4 || t == 7 || t == 8 || t == 9 {
            // terminal: this stack is final, copy it to the result
            if nout < max_stacks {
                var d: i64 = 0;
                while d < depth {
                    out[(base + nout) * max_depth + d] = wp[sid * max_depth + d];
                    d = d + 1;
                }
                outd[base + nout] = depth;
                nout = nout + 1;
            }
            continue;
        }
        if t != 2 {
            // only RULE_REF expands at a stack top; anything else is a range
            // modifier that cannot be a top
            continue;
        }
        let rid: i64 = eval[pos];
        var alt_start: i64 = roff[rid];
        let alt_end: i64 = roff[rid] + rlen[rid];
        var done: bool = false;
        while !done && nwork < max_stacks {
            var d: i64 = 0;
            while d < depth - 1 {
                wp[nwork * max_depth + d] = wp[sid * max_depth + d];
                d = d + 1;
            }
            // the continuation of the rule reference, unless it ends the sequence
            if etype[pos + 1] != 0 {
                wp[nwork * max_depth + d] = pos + 1;
                d = d + 1;
            }
            // the alternate itself, unless it is empty
            if etype[alt_start] != 0 {
                wp[nwork * max_depth + d] = alt_start;
                d = d + 1;
            }
            wpd[nwork] = d;
            if qn < 128 {
                queue[qn] = nwork;
                qn = qn + 1;
            }
            nwork = nwork + 1;
            var j: i64 = alt_start;
            var stop: bool = false;
            while j < alt_end && !stop {
                let tj: i64 = etype[j];
                if tj == 0 || tj == 1 {
                    stop = true;
                } else {
                    j = j + 1;
                }
            }
            if j < alt_end && etype[j] == 1 {
                alt_start = j + 1;
            } else {
                done = true;
            }
        }
    }
    return nout;
}


// reject_candidates for one stack, single-codepoint candidates (no partial UTF-8).
// rej[i] = 1 when candidate i is rejected. The stack transition after a class is
// candidate independent, so the descent is iterative instead of recursive.
// cand_cp is ncand rows of maxcp codepoints; curs starts at 0 for every candidate
fn g_reject(etype: i64[:], eval: i64[:], roff: i64[:], rlen: i64[:],
            st: i64[:], std_in: i64,
            cand_cp: i64[:], cp_len: i64[:], curs: i64[:], rej: i64[:], ncand: i64,
            pval: i64[:], pnr: i64[:], cand_id: i64[:],
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
        if t == 8 || t == 9 {
            i = 0;
            while i < ncand {
                if rej[i] == 0 {
                    if curs[i] >= cp_len[i] {
                        // no codepoints left: a partial sequence cannot satisfy a token rule
                        if pnr[i] > 0 {
                            rej[i] = 1;
                            alive = alive - 1;
                        }
                    } else {
                        let want_tok: bool = t == 8;
                        let is_tok: bool = eval[pos] == cand_id[i];
                        if is_tok != want_tok {
                            rej[i] = 1;
                            alive = alive - 1;
                        } else {
                            // a token rule consumes the whole token, so the rest of
                            // its codepoints are not leftovers
                            curs[i] = cp_len[i];
                        }
                    }
                }
                i = i + 1;
            }
            depth = depth - 1;
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
                  cand_cp: i64[:], cp_len: i64[:], pval: i64[:], pnr: i64[:], cand_id: i64[:],
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
                               pval, pnr, cand_id, scratch, maxd, maxcp);
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


// Composed entry: expand the input stack with the automaton, then reject across the
// resulting stacks. This is the shape the sampler filter needs.
fn g_grammar_reject(etype: i64[:], eval: i64[:], roff: i64[:], rlen: i64[:],
                    seed: i64[:], seedd: i64,
                    cand_cp: i64[:], cp_len: i64[:], pval: i64[:], pnr: i64[:],
                    cand_id: i64[:], ncand: i64,
                    wp: i64[:], wpd: i64[:], stacks: i64[:], stackd: i64[:],
                    scratch: i64[:], tmp_rej: i64[:], tmp_cur: i64[:], rowbuf: i64[:],
                    rej: i64[:], maxs: i64, maxd: i64, maxcp: i64) -> i64 {
    let n = g_advance(etype, eval, roff, rlen, seed, seedd, wp, wpd, stacks, stackd, 0, maxs, maxd);
    if n <= 0 {
        return -1;
    }
    return g_reject_multi(etype, eval, roff, rlen, stacks, stackd, n,
                          cand_cp, cp_len, pval, pnr, cand_id, rej, scratch, tmp_rej, tmp_cur,
                          rowbuf, ncand, maxd, maxcp);
}

fn main() {
    let et: i64[8] = [3, 0, 0, 0, 0, 0, 0, 0];
    let ev: i64[8] = [97, 0, 0, 0, 0, 0, 0, 0];
    let ro: i64[1] = [0];
    let rl: i64[1] = [2];
    let seed: i64[4] = [0, 0, 0, 0];
    let cp: i64[2] = [97, 0];
    let cpl: i64[1] = [1];
    let pv: i64[1] = [0];
    let pn: i64[1] = [0];
    let cid: i64[1] = [0];
    let wp: i64[16] = [0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0];
    let wpd: i64[4] = [0,0,0,0];
    let st: i64[16] = [0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0];
    let std: i64[4] = [0,0,0,0];
    let sc: i64[8] = [0,0,0,0,0,0,0,0];
    let tr: i64[1] = [0];
    let tc: i64[1] = [0];
    let rb: i64[4] = [0,0,0,0];
    let rej: i64[1] = [0];
    let r = g_grammar_reject(et, ev, ro, rl, seed, 1, cp, cpl, pv, pn, cid, 1,
                             wp, wpd, st, std, sc, tr, tc, rb, rej, 4, 4, 2);
    if r < 0 {
        return;
    }
}
