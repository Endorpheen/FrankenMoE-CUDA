
// RMS normalization used on the residual and projection inputs. A gain
// may either be absent or have one value for every channel.
export fn i2_rms_gain(input: const f32[:], weight: const f32[:], epsilon: f32, out: f32[:]) -> i64 {
    let n = input.len;
    if n < 1 || out.len < n || (weight.len != 0 && weight.len != n) || !(epsilon > 0.0) ||
       epsilon > 3.4028234663852886e38 { return -1; }
    var squares: f64 = 0.0;
    for i in n {
        let square: f32 = input[i] * input[i];
        squares += square as f64;
    }
    let mean = (squares / (n as f64)) as f32;
    let inverse: f32 = 1.0 / i2_sqrt(mean + epsilon);
    for i in n {
        let normalized = input[i] * inverse;
        out[i] = if weight.len == 0 { normalized } else { normalized * weight[i] };
    }
    return n;
}

// Post-attention and post-MoE convolution are static and have separate
// history within each cache slot. Commit history only after shape checks.
export fn i2_postconv_token(input: const f32[:], weights: const f32[:],
                             history: f32[:], out: f32[:]) -> i64 {
    let n = input.len;
    if n < 1 || n > 8192 || weights.len != 4 * n || history.len != 3 * n || out.len < n {
        return -1;
    }
    let inverse: f32 = 1.0 / i2_sqrt(5.0);
    for i in n {
        let at = 3 * i;
        let k = 4 * i;
        let current = input[i];
        var value: f32 = 0.0;
        for tap in 3 { value += history[at + tap] * weights[k + tap]; }
        value += current * weights[k + 3];
        out[i] = (current + value) * inverse;
        history[at] = history[at + 1];
        history[at + 1] = history[at + 2];
        history[at + 2] = current;
    }
    return n;
}

// Output rows use the unpadded vocabulary. Padded entries are kept out of
// the sampler, rather than relying on model padding values.
export fn i2_text_logits(logits: f32[:], unpadded: i64, cap: f32) -> i64 {
    if unpadded < 1 || unpadded > logits.len || !(cap > 0.0) || cap > 3.4028234663852886e38 {
        return -1;
    }
    let inverse: f32 = 1.0 / cap;
    for i in unpadded {
        let scaled = logits[i] * inverse;
        logits[i] = i2_tanh(scaled) * cap;
    }
    return unpadded;
}
