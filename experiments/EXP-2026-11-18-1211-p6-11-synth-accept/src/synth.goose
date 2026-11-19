// Synthetic acceptance of a drafted token (server_sample_and_accept_synth).
// The token is accepted when the target agrees by probability, and a drafted end-of-sequence is
// refused even when the probability is high - it would end the generation early. On replay the
// decision is taken again from the recorded state, so acceptance is unconditional there.
// Returns 1 when the drafted token is accepted, 0 when the sampled token replaces it.
fn g_synth_accept(prob: f64, u: f64, is_replay: i64, is_eog: i64, marker: i64) -> i64 {
    if marker != 208 {
        return -1;
    }
    var take: i64 = 0;
    if is_replay == 1 {
        take = 1;
    } else {
        if u < prob {
            take = 1;
        }
    }
    if take == 1 && is_replay == 0 && is_eog == 1 {
        take = 0;
    }
    return take;
}

fn main() {
    let r = g_synth_accept(0.5, 0.25, 0, 0, 208);
    if r < 0 {
        return;
    }
}
