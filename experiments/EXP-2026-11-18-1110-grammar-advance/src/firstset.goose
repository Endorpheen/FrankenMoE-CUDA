// Allowed first codepoints at the current stack tops, as inclusive ranges.
// Returns the number of ranges written (each into lo[]/hi[]), or -1 when the
// decision cannot be expressed as ranges (a token rule or an inverse class is at a
// top), in which case the caller must run the full automaton.
fn g_first_set(etype: i64[:], eval: i64[:],
               stacks: i64[:], stackd: i64[:], nstacks: i64,
               lo: i64[:], hi: i64[:], max_ranges: i64) -> i64 {
    var n: i64 = 0;
    var s: i64 = 0;
    while s < nstacks {
        let d: i64 = stackd[s];
        if d == 0 {
            // empty stack: end of sequence is allowed, which ranges cannot express
            return -1;
        }
        let pos: i64 = stacks[s * 8 + d - 1];
        let t: i64 = etype[pos];
        if t == 4 || t == 8 || t == 9 {
            // CHAR_NOT, TOKEN, TOKEN_NOT: not expressible as positive ranges here
            return -1;
        }
        if t != 3 && t != 7 {
            return -1;
        }
        var p: i64 = pos;
        var done: bool = false;
        while !done {
            if etype[p + 1] == 5 {
                if n < max_ranges {
                    lo[n] = eval[p];
                    hi[n] = eval[p + 1];
                    n = n + 1;
                }
                p = p + 2;
            } else if etype[p] == 7 {
                if n < max_ranges {
                    lo[n] = 0;
                    hi[n] = 1114111;
                    n = n + 1;
                }
                p = p + 1;
            } else {
                if n < max_ranges {
                    lo[n] = eval[p];
                    hi[n] = eval[p];
                    n = n + 1;
                }
                p = p + 1;
            }
            if etype[p] != 6 {
                done = true;
            }
        }
        s = s + 1;
    }
    return n;
}

fn main() {
    let et: i64[8] = [3, 5, 0, 0, 0, 0, 0, 0];
    let ev: i64[8] = [97, 122, 0, 0, 0, 0, 0, 0];
    let st: i64[8] = [0, 0, 0, 0, 0, 0, 0, 0];
    let std: i64[1] = [1];
    let lo: i64[8] = [0,0,0,0,0,0,0,0];
    let hi: i64[8] = [0,0,0,0,0,0,0,0];
    let r = g_first_set(et, ev, st, std, 1, lo, hi, 8);
    if r < 0 {
        return;
    }
}
