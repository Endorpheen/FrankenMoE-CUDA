// ngram-mod acceptance bookkeeping (common_speculative_impl_ngram_mod::accept).
// f_acc = n_accepted / n_draft_last; below 0.25 it increments a low-acceptance streak,
// and five consecutive low streaks reset the ngram container and the prompt position.
// out[0] = new n_low, out[1] = 1 when a reset is required.
fn g_ngram_accept(n_accepted: i64, n_draft_last: i64, n_low: i64, out: i64[:], marker: i64) -> i64 {
    if marker != 192 {
        return -1;
    }
    if n_draft_last <= 0 {
        out[0] = n_low;
        out[1] = 0;
        return 0;
    }
    // f_acc < 0.25 compared without division: 4 * n_accepted < n_draft_last
    if (4 * n_accepted) < n_draft_last {
        let nl = n_low + 1;
        if nl >= 5 {
            out[0] = 0;
            out[1] = 1;
        } else {
            out[0] = nl;
            out[1] = 0;
        }
    } else {
        out[0] = 0;
        out[1] = 0;
    }
    return 0;
}

fn main() {
    let o: i64[2] = [0, 0];
    let r = g_ngram_accept(1, 8, 0, o, 192);
    if r < 0 {
        return;
    }
}
