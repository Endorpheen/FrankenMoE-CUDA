// Keep prefill graph memory bounded while all chunks share one KV/GDN state.
const Q35_PREFILL_CHUNK: i64 = 128;

extern "fg_q35_generation_eval" fn q35_generation_eval(tokens: const i32[:], logits: f32[:]) -> i64;
extern "fg_q35_generation_now" fn q35_generation_now() -> f64;
extern "hose_reset_state" fn q35_generation_reset();

export fn q35_generate(tokens: const i64[:], vocabulary: i64, capacity: i64, eog: const i64[:],
                       limit: i64, ignore_eos: bool, ids: i64[:], timing: f64[:]) -> i64 {
    if timing.len != 0 && timing.len != 3 { return -1; }
    for i in timing.len { timing[i] = 0.0; }
    if tokens.len <= 0 || capacity < tokens.len || limit <= 0 || limit > ids.len ||
       limit - 1 > capacity - tokens.len || vocabulary <= 0 || vocabulary > 2147483647 {
        return -1;
    }
    for i in tokens.len {
        if tokens[i] < 0 || tokens[i] >= vocabulary { return -1; }
    }

    var batch: i32[128] = [0; 128];
    var logits: f32[>..] = [];
    logits.resize(vocabulary, 0.0);
    q35_generation_reset();
    let start = q35_generation_now();
    var read: i64 = 0;
    while read < tokens.len {
        let remaining = tokens.len - read;
        let count = if remaining < Q35_PREFILL_CHUNK { remaining } else { Q35_PREFILL_CHUNK };
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
        let next = fg_sampler_greedy(logits[..], vocabulary);
        if next < 0 || next >= vocabulary { return -1; }
        ids[produced] = next;
        produced += 1;
        if produced == limit || (!ignore_eos && bt_is_eog(eog, eog.len, next) != 0) {
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
