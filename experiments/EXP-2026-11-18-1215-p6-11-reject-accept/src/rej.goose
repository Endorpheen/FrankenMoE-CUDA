// Rejection acceptance of a drafted token (common_sampler_sample_and_accept_n_rejection).
// The draft is accepted when the target's probability for it is at least the draft's, or when
// the uniform draw falls below the ratio p/q. A draft the draft-model gave zero probability for
// can never be accepted - the native code guards the division instead of dividing by zero.
// Returns 1 to accept the drafted token, 0 to reject it and sample from the residual.
fn g_reject_accept(p_x: f64, q_x: f64, u: f64, marker: i64) -> i64 {
    if marker != 209 {
        return -1;
    }
    if q_x <= 0.0 {
        return 0;
    }
    if p_x >= q_x {
        return 1;
    }
    let ratio: f64 = p_x / q_x;
    if u < ratio {
        return 1;
    }
    return 0;
}

fn main() {
    let r = g_reject_accept(0.4, 0.5, 0.5, 209);
    if r < 0 {
        return;
    }
}
