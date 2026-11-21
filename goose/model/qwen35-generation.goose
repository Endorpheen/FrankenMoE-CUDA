const Q35_MAX_PREFILL_BATCH: i64 = 4096;

extern "fg_q35_generation_eval" fn q35_generation_eval(tokens: const i32[:], logits: f32[:]) -> i64;
extern "fg_q35_generation_profile" fn q35_generation_profile(profile: i64[:]);
extern "fg_q35_generation_prepare" fn q35_generation_prepare() -> i64;
extern "fg_q35_generation_now" fn q35_generation_now() -> f64;
extern "hose_reset_state" fn q35_generation_reset();

export fn q35_prefill_valid(batch: i64, ubatch: i64) -> bool {
    return ubatch > 0 && ubatch <= batch && batch <= Q35_MAX_PREFILL_BATCH;
}

fn q35_prefill_batch_count(remaining: i64, batch: i64, ubatch: i64, checkpoint_tail: bool) -> i64 {
    var count = if remaining < batch { remaining } else { batch };
    if checkpoint_tail {
        // Match the recurrent server's two near-end checkpoint boundaries.
        let offsets: i64[2] = [ubatch + 4, 4];
        for i in 2 {
            let keep = if offsets[i] < batch { offsets[i] } else { batch };
            if remaining > keep && remaining - keep < count { count = remaining - keep; }
        }
    }
    return count;
}

struct Q35Sample {
    seed: i64, top_k: i64, min_keep: i64, repeat_last_n: i64,
    top_p: f32, min_p: f32, typical_p: f32, repeat_penalty: f32, temperature: f32, ignore_eos: bool
}

fn q35_sample_defaults() -> Q35Sample {
    return Q35Sample { seed: 0, top_k: 0, min_keep: 1, repeat_last_n: 64,
        top_p: 1.0, min_p: 0.0, typical_p: 1.0, repeat_penalty: 1.0, temperature: 0.0, ignore_eos: false };
}

fn q35_sample_valid(s: const Q35Sample &) -> bool {
    return s.seed >= 0 && s.seed <= 4294967295 && s.top_k >= 0 &&
        s.min_keep >= 0 && s.repeat_last_n >= -1 &&
        s.top_p > 0.0 && s.top_p <= 1.0 && s.min_p >= 0.0 && s.min_p <= 1.0 &&
        s.typical_p >= 0.0 && s.typical_p <= 1.0 &&
        s.repeat_penalty > 0.0 && s.repeat_penalty <= 3.4028234663852886e38 &&
        s.temperature >= 0.0 && s.temperature <= 3.4028234663852886e38;
}

fn q35_sample_generate(tokens: const i64[:], vocabulary: i64, capacity: i64, eog: const i64[:],
                       limit: i64, s: const Q35Sample &, ids: i64[:], timing: f64[:],
                       top: i64, prob_ids: i64[:], logp: f32[:]) -> i64 {
    if timing.len != 0 && timing.len != 3 { return -1; }
    for i in timing.len { timing[i] = 0.0; }
    if tokens.len <= 0 || capacity < tokens.len || limit <= 0 || limit > ids.len ||
       limit - 1 > capacity - tokens.len || vocabulary <= 0 || vocabulary > 2147483647 ||
       !q35_sample_valid(s) || top < 0 || top > vocabulary ||
       (top > 0 && (prob_ids.len / limit < top + 1 || logp.len / limit < top + 1)) {
        return -1;
    }
    for i in tokens.len {
        if tokens[i] < 0 || tokens[i] >= vocabulary { return -1; }
    }

    var profile: i64[3] = [0; 3];
    q35_generation_profile(profile[..]);
    if !q35_prefill_valid(profile[0], profile[1]) { return -1; }
    // The native evaluator re-enters Goose. Keep its borrowed token slice
    // in fixed storage, outside the allocator frames used by that call.
    var batch: i32[4096] = [0; 4096];
    var logits: f32[>..] = [];
    logits.resize(vocabulary, 0.0);
    var candidates: f32[>..] = [];
    var probabilities: f32[>..] = [];
    var order: i64[>..] = [];
    var typical_work: f32[>..] = [];
    var repeat_marks: u8[>..] = [];
    var rng: i64[625] = [0; 625];
    var pick: i64[3] = [0; 3];
    let seeded = s.temperature > 0.0;
    let filter = seeded || s.typical_p < 1.0;
    let recent = if s.repeat_last_n < 0 { capacity } else { s.repeat_last_n };
    let repeats = recent > 0 && s.repeat_penalty != 1.0;
    if filter || top > 0 {
        probabilities.resize(vocabulary, 0.0);
        order.resize(vocabulary, 0);
    }
    if s.typical_p < 1.0 {
        let candidates = if s.top_k > 0 && s.top_k < vocabulary { s.top_k } else { vocabulary };
        typical_work.resize(candidates, 0.0);
    }
    if repeats { repeat_marks.resize(vocabulary, 0); }
    if top > 0 { candidates.resize(vocabulary, 0.0); }
    if seeded && sm_mt_seed(rng, s.seed) != 0 { return -1; }
    let sampler_i: i64[3] = [s.top_k, s.min_keep, 1];
    let sampler_f: f32[4] = [s.top_p, s.min_p, s.temperature, s.typical_p];
    if q35_generation_prepare() != 0 { return -1; }
    q35_generation_reset();
    let start = q35_generation_now();
    var read: i64 = 0;
    var batch_end: i64 = 0;
    while read < tokens.len {
        if read == batch_end {
            batch_end += q35_prefill_batch_count(tokens.len - read, profile[0], profile[1], profile[2] != 0);
        }
        let remaining = batch_end - read;
        let count = if remaining < profile[1] { remaining } else { profile[1] };
        for i in count { batch[i] = tokens[read + i] as i32; }
        read += count;
        let output = if read == tokens.len { logits[..] } else { logits[0 .. 0] };
        if q35_generation_eval(batch[0 .. count], output) != output.len {
            q35_generation_reset();
            return -1;
        }
    }
    let prefill = q35_generation_now() - start;

    var produced: i64 = 0;
    while produced < limit {
        let draw = if top == 0 { logits[..] } else { candidates[..] };
        if top > 0 {
            for i in vocabulary { draw[i] = logits[i]; }
        }
        if s.ignore_eos {
            for i in eog.len {
                let token = eog[i];
                if token >= 0 && token < vocabulary { draw[token] = float_bits(4286578688); }
            }
        }
        if repeats {
            let generated_start = if produced > recent { produced - recent } else { 0 };
            let prompt_budget = recent - (produced - generated_start);
            let prompt_start = if tokens.len > prompt_budget { tokens.len - prompt_budget } else { 0 };
            sm_repeat_history(draw, tokens[prompt_start .. tokens.len],
                ids[generated_start .. produced], repeat_marks[..], s.repeat_penalty);
        }
        var next: i64 = -1;
        if filter {
            if sm_select(draw, order[..], probabilities[..], typical_work[..],
                         vocabulary, sampler_i, sampler_f, rng, pick) == 0 {
                next = pick[1];
            }
        } else {
            next = fg_sampler_greedy(draw, vocabulary);
        }
        if next < 0 || next >= vocabulary { return -1; }
        let is_eog = bt_is_eog_impl(eog, eog.len, next) != 0;
        if s.ignore_eos && is_eog { return -1; }
        if top > 0 {
            let at = produced * (top + 1);
            let end = at + top + 1;
            if !sm_pre_sample_logprobs(logits[..], order[..], probabilities[..], next, top,
                                       prob_ids[at .. end], logp[at .. end]) { return -1; }
        }
        ids[produced] = next;
        produced += 1;
        if produced == limit || is_eog {
            break;
        }
        batch[0] = next as i32;
        if q35_generation_eval(batch[0 .. 1], logits[..]) != vocabulary {
            q35_generation_reset();
            return -1;
        }
    }
    if timing.len == 3 {
        timing[0] = prefill;
        timing[1] = q35_generation_now() - start - prefill;
        timing[2] = tokens.len as f64;
    }
    return produced;
}

export fn q35_generate(tokens: const i64[:], vocabulary: i64, capacity: i64, eog: const i64[:],
                       limit: i64, ignore_eos: bool, ids: i64[:], timing: f64[:]) -> i64 {
    var s = q35_sample_defaults();
    s.ignore_eos = ignore_eos;
    var no_ids: i64[0] = [];
    var no_probs: f32[0] = [];
    return q35_sample_generate(tokens, vocabulary, capacity, eog, limit, s, ids, timing,
        0, no_ids, no_probs);
}
