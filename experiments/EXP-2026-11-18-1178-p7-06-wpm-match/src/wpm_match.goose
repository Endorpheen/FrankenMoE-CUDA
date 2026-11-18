// WPM match-length search (llm_tokenizer_wpm_session::tokenize).
// At position i the search starts at min(n, i + max_token_len + 1) and walks DOWN to
// i + 1, so the longest matching piece wins. The upper bound includes the +1 the native
// loop uses, which is deliberately preserved: changing it would change which pieces are
// considered for a word longer than max_token_len.
// okmask bit L is set when the lookup of length L succeeded.
// Returns the chosen length, or -1 when nothing matched.
fn g_wpm_pick(n: i64, i: i64, maxlen: i64, okmask: i64, marker: i64) -> i64 {
    if marker != 201 {
        return -1;
    }
    if n <= 0 || i < 0 || i >= n || maxlen < 0 {
        return -1;
    }
    var j: i64 = i + maxlen + 1;
    if j > n {
        j = n;
    }
    while j > i {
        var val: i64 = okmask;
        var idx: i64 = 0;
        while idx < j {
            val = val / 2;
            idx = idx + 1;
        }
        // val - 2*(val/2) is bit j of the mask
        if val - 2 * (val / 2) == 1 {
            return j - i;
        }
        j = j - 1;
    }
    return -1;
}

fn main() {
    let r = g_wpm_pick(10, 0, 3, 8, 201);
    if r < -1 {
        return;
    }
}
