// Mix the ordered residual blocks for each learned query. Each block row
// is independently RMS-normalized before forming scores and output values.
export fn i2_depth_mix(states: const f32[:], width: i64, queries: const f32[:],
                        norm: f32[:], probabilities: f32[:], output: f32[:]) -> i64 {
    if width <= 0 || width > 8192 || states.len == 0 || states.len % width != 0 ||
       norm.len < states.len || queries.len == 0 || queries.len % width != 0 ||
       output.len < queries.len { return -1; }
    let count = states.len / width;
    let q_count = queries.len / width;
    if count > 128 || q_count > 24 || probabilities.len < count { return -1; }

    for row in count {
        let at = row * width;
        var sum: f64 = 0.0;
        for i in width {
            let square: f32 = states[at + i] * states[at + i];
            sum += square as f64;
        }
        let mean = (sum / (width as f64)) as f32;
        let scale: f32 = 1.0 / i2_sqrt(mean + 0.000001);
        for i in width { norm[at + i] = states[at + i] * scale; }
    }

    let score_scale: f32 = 1.0 / i2_sqrt(width as f32);
    for q in q_count {
        let q_at = q * width;
        var maximum: f32 = -3.4028234663852886e38;
        for row in count {
            let at = row * width;
            let score = i2_attention_dot(norm[at .. at + width], queries[q_at .. q_at + width]) * score_scale;
            probabilities[row] = score;
            if score > maximum { maximum = score; }
        }
        var total: f64 = 0.0;
        for row in count {
            let weight = i2_attn_exp(probabilities[row] - maximum);
            total += weight as f64;
            probabilities[row] = weight;
        }
        let inverse: f32 = 1.0 / (total as f32);
        for row in count { probabilities[row] *= inverse; }

        for i in width {
            var sum: f64 = 0.0;
            for row in count {
                let term = probabilities[row] * norm[row * width + i];
                sum += term as f64;
            }
            output[q_at + i] = sum as f32;
        }
    }
    return q_count;
}

// Gain and forget are the controller projections before tanh.
export fn i2_loop_blend(current: const f32[:], message: const f32[:], gain: const f32[:],
                         forget: const f32[:], output: f32[:]) -> i64 {
    let width = current.len;
    if width <= 0 || message.len != width || gain.len != width || forget.len != width ||
       output.len < width { return -1; }
    for i in width {
        let keep: f32 = 1.0 - i2_tanh(forget[i]);
        let residual = keep * current[i];
        let update = i2_tanh(gain[i]) * message[i];
        output[i] = residual + update;
    }
    return width;
}
