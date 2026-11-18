// Grammar automaton, index model: rules are flat arrays, a position is a global
// element index, and stacks live in caller-provided buffers.
//   etype[i], eval[i]      element type and value
//   roff[r], rlen[r]       rule start and element count
//   out[pos]               stack pool, max_stacks rows of max_depth
//   outd[s]                depth of stack s
fn g_advance(etype: i64[:], eval: i64[:], roff: i64[:], rlen: i64[:],
             seed: i64[:], seedd: i64,
             out: i64[:], outd: i64[:], max_stacks: i64, max_depth: i64) -> i64 {
    var nst: i64 = 0;
    // worklist of stack ids; stack 0..nst-1 live in out
    var work: i64[64] = [0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
                         0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
                         0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0];
    var wn: i64 = 0;
    var i: i64 = 0;
    // seed stack becomes the first output
    while i < seedd && i < max_depth {
        out[i] = seed[i];
        i = i + 1;
    }
    outd[0] = seedd;
    nst = 1;
    work[0] = 0;
    wn = 1;
    while wn > 0 {
        wn = wn - 1;
        let sid: i64 = work[wn];
        let depth: i64 = outd[sid];
        if depth == 0 {
            continue;
        }
        let pos: i64 = out[sid * max_depth + depth - 1];
        let t: i64 = etype[pos];
        if t == 3 || t == 4 || t == 7 || t == 8 || t == 9 {
            // CHAR, CHAR_NOT, CHAR_ANY, TOKEN, TOKEN_NOT: terminal, stack is final
            continue;
        }
        if t == 2 {
            // RULE_REF: expand each alternate of the referenced rule
            let rid: i64 = eval[pos];
            var alt_start: i64 = roff[rid];
            let alt_end: i64 = roff[rid] + rlen[rid];
            var done: bool = false;
            while !done && nst < max_stacks {
                // build the new stack: everything below pos, then the alternate
                var d: i64 = 0;
                while d < depth - 1 && d < max_depth {
                    out[nst * max_depth + d] = out[sid * max_depth + d];
                    d = d + 1;
                }
                if alt_start < alt_end {
                    out[nst * max_depth + d] = alt_start;
                    d = d + 1;
                }
                outd[nst] = d;
                if wn < 64 {
                    work[wn] = nst;
                    wn = wn + 1;
                }
                nst = nst + 1;
                // to the next alternate: scan to END or ALT
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
            continue;
        }
        // CHAR_ALT / CHAR_RNG_UPPER / others: not reachable as a stack top
    }
    return nst;
}

fn main() {
    let et: i64[8] = [0, 3, 1, 3, 1, 0, 0, 0];
    let ev: i64[8] = [1, 97, 0, 120, 0, 0, 0, 0];
    let ro: i64[3] = [0, 3, 0];
    let rl: i64[3] = [3, 3, 0];
    let seed: i64[8] = [0, 0, 0, 0, 0, 0, 0, 0];
    let out: i64[64] = [0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
                        0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0];
    let outd: i64[8] = [0,0,0,0,0,0,0,0];
    let n = g_advance(et, ev, ro, rl, seed, 1, out, outd, 8, 8);
    if n < 0 {
        return;
    }
}
