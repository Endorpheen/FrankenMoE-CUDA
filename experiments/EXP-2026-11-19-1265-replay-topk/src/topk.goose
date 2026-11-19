// DeterministicWorkload::replay_topk, the replay-side decision.
// 1 the workload has no more route records, 2 the batch shape does not match the record, 0 accepted.
// The native cursor advances BEFORE the comparison, so a failing call still consumes the record;
// out[0] carries the cursor the native leaves behind, and the harness checks that as well.
fn wl_replay_topk(cursor: i64, routes_n: i64,
                  e_phase: i64, e_base: i64, e_batch: i64, e_layer: i64, e_topk: i64, e_tokn: i64,
                  a_phase: i64, a_base: i64, a_batch: i64, a_layer: i64, a_topk: i64, a_tokn: i64,
                  out: i64[:], marker: i64) -> i64 {
    if marker != 216 {
        return -1;
    }
    if cursor >= routes_n {
        out[0] = cursor;
        return 1;
    }
    out[0] = cursor + 1;
    if e_phase != a_phase || e_base != a_base || e_batch != a_batch ||
       e_layer != a_layer || e_topk != a_topk || e_tokn != a_tokn {
        return 2;
    }
    return 0;
}

fn main() {
    let o: i64[1] = [0];
    if wl_replay_topk(0, 1, 0, 0, 1, 0, 2, 1, 0, 0, 1, 0, 2, 1, o, 216) < 0 {
        return;
    }
}
