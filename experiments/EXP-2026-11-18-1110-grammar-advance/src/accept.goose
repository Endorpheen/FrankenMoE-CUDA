// accept_chr, first slice: shift every stack past a matched character class.
// Rule references are not expanded here yet (advance_stack does that in the native
// flow); token rules are skipped as the native function does.
fn g_accept(etype: i64[:], eval: i64[:],
            stacks: i64[:], stackd: i64[:], nstacks: i64, chr: i64,
            out: i64[:], outd: i64[:], maxs: i64, maxd: i64) -> i64 {
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
            var d: i64 = 0;
            while d < depth - 1 {
                out[nout * maxd + d] = stacks[s * maxd + d];
                d = d + 1;
            }
            if etype[after] != 0 {
                out[nout * maxd + d] = after;
                d = d + 1;
            }
            outd[nout] = d;
            nout = nout + 1;
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
    let r = g_accept(et, ev, st, std, 1, 97, out, outd, 4, 4);
    if r < 0 {
        return;
    }
}
