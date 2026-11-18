// One level of common_ngram_cache try_draft.
// The best candidate is chosen by the PRODUCT of the primary count and the static
// weight (100*count, or 1 when the token is absent from the static part), while the
// two acceptance tests use the plain primary count. Mixing those two up is the easy
// mistake here, so both are kept separate below.
// cs[i] < 0 means "token absent from the static part".
// Returns the chosen token, or -1 when this level produces no draft.
fn g_try_draft(tok: i64[:], cp: i64[:], cs: i64[:], n: i64,
               min_sample: i64, min_percent: i64, out: i64[:], marker: i64) -> i64 {
    if marker != 197 {
        return -1;
    }
    if n <= 0 || tok.len < n || cp.len < n || cs.len < n {
        return -1;
    }
    var best_tok: i64 = -1;
    var max_cp: i64 = 0;
    var max_cs: i64 = 0;
    var sum_cp: i64 = 0;
    var i: i64 = 0;
    while i < n {
        var w: i64 = 1;
        if cs[i] >= 0 {
            w = 100 * cs[i];
        }
        if cp[i] * w > max_cp * max_cs {
            best_tok = tok[i];
            max_cp = cp[i];
            max_cs = w;
        }
        sum_cp = sum_cp + cp[i];
        i = i + 1;
    }
    out[0] = max_cp;
    out[1] = sum_cp;
    if sum_cp < min_sample {
        return -1;
    }
    if 100 * max_cp < min_percent * sum_cp {
        return -1;
    }
    return best_tok;
}

fn main() {
    let t: i64[3] = [10, 11, 12];
    let c: i64[3] = [5, 3, 1];
    let s: i64[3] = [-1, 2, 0];
    let o: i64[2] = [0, 0];
    let r = g_try_draft(t, c, s, 3, 1, 1, o, 197);
    if r < -1 {
        return;
    }
}
