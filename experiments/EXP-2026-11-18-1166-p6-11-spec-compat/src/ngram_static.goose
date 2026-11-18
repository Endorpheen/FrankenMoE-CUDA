// Static-only draft attempt (common_ngram_cache try_draft over nc_static) plus the
// threshold tables it indexes. LLAMA_NGRAM_STATIC = 2 and LLAMA_NGRAM_MAX = 4 come from
// ngram-cache.h; the static path uses the lax table at index LLAMA_NGRAM_STATIC-1 = 1.
// Returns the chosen token, or -1 when no draft is possible.
fn g_static_lax_thresholds(level: i64, out: i64[:], marker: i64) -> i64 {
    if marker != 198 {
        return -1;
    }
    if level < 0 || level > 3 {
        return -1;
    }
    // { 2, 2, 1, 1 } and { 66, 50, 50, 50 }
    var s: i64 = 1;
    if level == 0 || level == 1 {
        s = 2;
    }
    var p: i64 = 50;
    if level == 0 {
        p = 66;
    }
    out[0] = s;
    out[1] = p;
    return 0;
}

fn g_static_draft(tok: i64[:], cnt: i64[:], n: i64, marker: i64) -> i64 {
    if marker != 199 {
        return -1;
    }
    if n <= 0 || tok.len < n || cnt.len < n {
        return -1;
    }
    var best: i64 = -1;
    var max_c: i64 = 0;
    var sum_c: i64 = 0;
    var i: i64 = 0;
    while i < n {
        if cnt[i] > max_c {
            best = tok[i];
            max_c = cnt[i];
        }
        sum_c = sum_c + cnt[i];
        i = i + 1;
    }
    // lax table at index LLAMA_NGRAM_STATIC-1
    var idx: i64 = 1;
    if idx == 0 {
        idx = 0;
    }
    var min_sample: i64 = 2;
    var min_pct: i64 = 50;
    if idx == 0 {
        min_sample = 2;
        min_pct = 66;
    }
    if sum_c < min_sample {
        return -1;
    }
    if 100 * max_c < min_pct * sum_c {
        return -1;
    }
    return best;
}

fn main() {
    let t: i64[3] = [10, 11, 12];
    let c: i64[3] = [5, 3, 1];
    let o: i64[2] = [0, 0];
    if g_static_lax_thresholds(1, o, 198) < 0 {
        return;
    }
    let r = g_static_draft(t, c, 3, 199);
    if r < -2 {
        return;
    }
}
