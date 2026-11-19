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

fn main() {
    let et: i64[8] = [2, 0, 3, 5, 3, 6, 0, 0];
    let ev: i64[8] = [1, 0, 97, 122, 120, 0, 0, 0];
    let r = g_match_char(et, ev, 2, 109);
    if r == 0 {
        return;
    }
}
