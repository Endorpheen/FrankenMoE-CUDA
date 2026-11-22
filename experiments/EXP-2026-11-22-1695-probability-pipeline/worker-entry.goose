export fn fg_probability_worker(logits: f32[:], order: i64[:], selected: i64,
                                top: i64, ids: i64[:], logp: f32[:]) -> bool {
    return sm_pre_sample_logprobs(logits, order, selected, top, ids, logp);
}
