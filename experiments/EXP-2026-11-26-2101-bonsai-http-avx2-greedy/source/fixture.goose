extern "fg_q35_topk_first_above" fn sm_topk_first_above(lv: const f32[:], start: i64, end: i64, threshold: f32) -> i64;

export fn probe_greedy(lv: f32[:], n: i64) -> i64 {
    return sm_greedy(lv, n);
}
