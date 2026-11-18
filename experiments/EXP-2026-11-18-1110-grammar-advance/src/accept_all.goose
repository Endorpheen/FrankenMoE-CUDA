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
            if base + nout < max_stacks {
                outd[base + nout] = 0;
                nout = nout + 1;
            }
            continue;
        }
        let pos: i64 = wp[sid * max_depth + depth - 1];
        let t: i64 = etype[pos];
        if t == 3 || t == 4 || t == 7 || t == 8 || t == 9 {
            // terminal: this stack is final, copy it to the result
            if base + nout < max_stacks {
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

fn main() {
    let et: i64[8] = [2, 0, 3, 0, 3, 0, 0, 0];
    let ev: i64[8] = [1, 0, 97, 0, 120, 0, 0, 0];
    let ro: i64[3] = [0, 2, 4];
    let rl: i64[3] = [2, 2, 2];
    let seed: i64[8] = [0, 0, 0, 0, 0, 0, 0, 0];
    let wp: i64[64] = [0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
                       0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0];
    let wpd: i64[8] = [0,0,0,0,0,0,0,0];
    let out: i64[64] = [0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
                        0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0];
    let outd: i64[8] = [0,0,0,0,0,0,0,0];
    let n = g_advance(et, ev, ro, rl, seed, 1, wp, wpd, out, outd, 0, 8, 8);
    if n < 0 {
        return;
    }
}

// accept_chr, first slice: shift every stack past a matched character class.
// Rule references are not expanded here yet (advance_stack does that in the native
// flow); token rules are skipped as the native function does.
fn g_accept(etype: i64[:], eval: i64[:], roff: i64[:], rlen: i64[:],
            stacks: i64[:], stackd: i64[:], nstacks: i64, chr: i64,
            out: i64[:], outd: i64[:], wp: i64[:], wpd: i64[:],
            maxs: i64, maxd: i64) -> i64 {
    var nout: i64 = 0;
    var s: i64 = 0;
    while s < nstacks {
        let depth: i64 = stackd[s];
        if depth == 0 {
            s = s + 1;
            continue;
        }
        let pos: i64 = stacks[s * maxd + depth - 1];
        let t: i64 = etype[pos];
        if t == 8 || t == 9 {
            s = s + 1;
            continue;
        }
        let m = g_match_char(etype, eval, pos, chr);
        if m > 0 {
            let after: i64 = m - 1;
            var tmp: i64[8] = [0,0,0,0,0,0,0,0];
            var d: i64 = 0;
            while d < depth - 1 && d < maxd {
                tmp[d] = stacks[s * maxd + d];
                d = d + 1;
            }
            if etype[after] != 0 {
                tmp[d] = after;
                d = d + 1;
            }
            // expand rule references the way the native accept does
            let n = g_advance(etype, eval, roff, rlen, tmp, d, wp, wpd, out, outd, nout, maxs, maxd);
            if n > 0 {
                nout = nout + n;
            }
        }
        s = s + 1;
    }
    return nout;
}

fn main() {
    let et: i64[8] = [3, 3, 0, 0, 0, 0, 0, 0];
    let ev: i64[8] = [97, 98, 0, 0, 0, 0, 0, 0];
    let st: i64[4] = [1, 0, 0, 0];
    let std: i64[1] = [1];
    let out: i64[16] = [0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0];
    let outd: i64[4] = [0,0,0,0];
    let ro: i64[1] = [0];
    let rl: i64[1] = [2];
    let wp: i64[16] = [0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0];
    let wpd: i64[4] = [0,0,0,0];
    let r = g_accept(et, ev, ro, rl, st, std, 1, 97, out, outd, wp, wpd, 4, 4);
    if r < 0 {
        return;
    }
}
