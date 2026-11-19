// DeterministicWorkload::check_static (deterministic_workload.cpp): a replay only proceeds when the
// model it is being replayed against is the one the workload was captured from.
// Returns 1 when the metadata agrees, 0 when it does not - the native returns true/false and the
// message names the values, which is presentation and is not ported.
// arch_ok is the byte comparison of the two architecture strings, done by the caller from its own
// bytes; the three integers are compared here, in the native order.
fn g_wl_check_static(arch_ok: i64, n_layer_same: i64, top_k_same: i64, marker: i64) -> i64 {
    if marker != 212 {
        return -1;
    }
    if arch_ok != 1 || n_layer_same != 1 || top_k_same != 1 {
        return 0;
    }
    return 1;
}

fn main() {
    let r = g_wl_check_static(1, 1, 1, 212);
    if r < 0 {
        return;
    }
}
