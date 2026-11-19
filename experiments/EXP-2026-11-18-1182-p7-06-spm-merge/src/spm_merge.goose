// SPM merge order (llm_tokenizer_spm_session::tokenize).
// The queue holds bigrams; the best one is popped by score with ties on the smaller left
// index. A popped bigram whose right symbol is no longer adjacent is stale and skipped.
// After a merge, two new bigrams become candidates: (prev(left), left) and (left, right+1).
// The decision this function produces is the ORDER of merges, which is what the tokenizer
// output depends on; the caller turns it into tokens.
// pairs are candidate merges: pl/ps/pr, pr must equal the right symbol index.
// Returns the number of merges performed and writes their pair indices to out.
fn g_spm_merge(n: i64, pl: i64[:], pr: i64[:], pscore: f64[:], psize: i64[:], np: i64,
               next: i64[:], prev: i64[:], symn: i64[:], out: i64[:], marker: i64) -> i64 {
    if marker != 203 {
        return -1;
    }
    if n <= 0 || n > next.len || n > prev.len || n > symn.len {
        return -1;
    }
    var i: i64 = 0;
    while i < n {
        next[i] = i + 1;
        prev[i] = i - 1;
        symn[i] = 1;
        i = i + 1;
    }
    var nmerged: i64 = 0;
    var done: i64 = 0;
    while done == 0 && nmerged < out.len {
        var best: i64 = -1;
        var bleft: i64 = 0;
        var k: i64 = 0;
        while k < np {
            let l = pl[k];
            let r = pr[k];
            // the size check is not redundant with adjacency: a left symbol that has
            // already absorbed its neighbour is still adjacent, but its text has grown,
            // so the pair was queued for a different text and the native code skips it
            if l >= 0 && l < n && r >= 0 && r < n && symn[l] > 0 && symn[r] > 0 &&
                    next[l] == r && symn[l] + symn[r] == psize[k] {
                if best < 0 || pscore[k] > pscore[best] || (pscore[k] == pscore[best] && l < bleft) {
                    best = k;
                    bleft = l;
                }
            }
            k = k + 1;
        }
        if best < 0 {
            done = 1;
        } else {
            let l = pl[best];
            let r = pr[best];
            symn[l] = symn[l] + symn[r];
            symn[r] = 0;
            next[l] = next[r];
            var nx = next[r];
            if nx < n {
                prev[nx] = l;
            }
            out[nmerged] = best;
            nmerged = nmerged + 1;
        }
    }
    return nmerged;
}

fn main() {
    let pl: i64[3] = [0, 1, 0];
    let pr: i64[3] = [1, 2, 2];
    let ps: f64[3] = [1.0, 2.0, 0.5];
    let pn: i64[3] = [2, 2, 3];
    let nx: i64[4] = [0, 0, 0, 0];
    let pv: i64[4] = [0, 0, 0, 0];
    let sn: i64[4] = [0, 0, 0, 0];
    let o: i64[4] = [0, 0, 0, 0];
    let r = g_spm_merge(4, pl, pr, ps, pn, 3, nx, pv, sn, o, 203);
    if r < 0 {
        return;
    }
}
