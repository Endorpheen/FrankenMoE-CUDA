// Prefix agreement between the drafted tokens and the tokens the target produced.
// The accepted count is the length of the agreeing prefix, and the position of the
// first disagreement is the rebase point the caller uses afterwards.
// Returns the accepted count; writes the rebase position into out[0].
fn g_spec_prefix(draft: i64[:], target: i64[:], k: i64, out: i64[:], marker: i64) -> i64 {
    if marker != 191 {
        return -1;
    }
    if k < 0 || draft.len < k || target.len < k {
        return -1;
    }
    var i: i64 = 0;
    while i < k {
        if draft[i] != target[i] {
            out[0] = i;
            return i;
        }
        i = i + 1;
    }
    out[0] = k;
    return k;
}

fn main() {
    let d: i64[8] = [1, 2, 3, 0, 0, 0, 0, 0];
    let t: i64[8] = [1, 2, 9, 0, 0, 0, 0, 0];
    let o: i64[2] = [0, 0];
    let r = g_spec_prefix(d, t, 3, o, 191);
    if r < 0 {
        return;
    }
}
