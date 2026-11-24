extern "i2_cuda_attention" fn i2_cuda_attention(query: const f32[:], keys: const f32[:], values: const f32[:],
    relative: const f32[:], bias: const f32[:], sinks: const f32[:], positions: const i64[:],
    absolute: i64, window: i64, output: f32[:]) -> i64;

extern "fg158_float" fn float_bits(x: i64) -> f32;
extern "tanhf" fn i2_tanh(value: f32) -> f32;
extern "expf" fn i2_exp(value: f32) -> f32;

// Match the reference SiTU graph's FP32 stage order. The caller provides
// separate or elementwise-identical buffers, as for the other unary kernels.
export fn inkling_row_major_i2_situ(gate: const f32[:], up: const f32[:], beta: f32, linear_beta: f32, out: f32[:]) -> i64 {
    if gate.len != up.len || out.len < gate.len || !(beta > 0.0) || !(linear_beta >= 0.0) ||
       beta > 3.4028234663852886e38 || linear_beta > 3.4028234663852886e38 { return -1; }

    let gate_scale: f32 = 1.0 / beta;
    let up_scale: f32 = if linear_beta > 0.0 { 1.0 / linear_beta } else { 0.0 };
    for i in gate.len {
        let gate_activation = i2_tanh(gate[i] * gate_scale) * beta;
        let gate_sigmoid: f32 = 1.0 / (1.0 + i2_exp(-gate[i]));
        let activated = gate_activation * gate_sigmoid;
        let up_activation = if linear_beta > 0.0 { i2_tanh(up[i] * up_scale) * linear_beta } else { up[i] };
        out[i] = activated * up_activation;
    }
    return gate.len;
}

extern "sqrtf" fn i2_sqrt(value: f32) -> f32;

// Produce gather indices into the learned relative-bias table. The final
// row is reserved for empty cells, future keys and out-of-range distances.
export fn inkling_row_major_i2_relative_positions(query: i64, keys: const i64[:], extent: i64, out: i32[:]) -> i64 {
    if query < 0 || extent < 1 || extent > 2147483647 || out.len < keys.len { return -1; }
    for i in keys.len {
        let key = keys[i];
        var relative = extent;
        if key >= 0 && key <= query {
            let distance = query - key;
            if distance < extent { relative = distance; }
        }
        out[i] = relative as i32;
    }
    return keys.len;
}

// Decode combines the existing depthwise convolution with per-head dynamic
// taps. Histories are oldest to newest, three values per channel. The tap
// vector has four entries per head, starting with the current token.
fn i2_shortconv_decode_impl(current: const f32[:], past: const f32[:], static_conv: const f32[:],
                              taps: const f32[:], out: f32[:]) -> i64 {
    let channels = current.len;
    if channels <= 0 || channels > 2147483647 || past.len != 3 * channels ||
       static_conv.len != channels || out.len < channels || taps.len % 4 != 0 { return -1; }
    let heads = taps.len / 4;
    if heads > 0 && channels % heads != 0 { return -1; }
    let scale: f32 = if heads == 0 { 1.0 / i2_sqrt(5.0) } else { 1.0 / i2_sqrt(9.0) };
    let group = if heads == 0 { 1 } else { channels / heads };
    for i in channels {
        var mixed = static_conv[i];
        if heads > 0 {
            let tap = (i / group) * 4;
            let history = 3 * i;
            mixed += current[i] * taps[tap];
            mixed += past[history + 2] * taps[tap + 1];
            mixed += past[history + 1] * taps[tap + 2];
            mixed += past[history] * taps[tap + 3];
        }
        out[i] = (current[i] + mixed) * scale;
    }
    return channels;
}

extern "expf" fn i2_attn_exp(value: f32) -> f32;

fn i2_attention_dot(a: const f32[:], b: const f32[:]) -> f32 {
    var sum: f64 = 0.0;
    for i in a.len {
        let product: f32 = a[i] * b[i];
        sum += product as f64;
    }
    return sum as f32;
}

// Scalar one-token attention for the Inkling text layout. Inputs are the
// projected, clipped Q/K and normalized V. Relative vectors include the
// model's position scaling for this query. Cache rows may be in any order.
fn i2_attention_decode_impl(query: const f32[:], keys: const f32[:], values: const f32[:],
                               relative: const f32[:], bias: const f32[:], sink: const f32[:],
                               positions: const i64[:], absolute: i64, window: i64,
                               scratch: f32[:], output: f32[:]) -> i64 {
    let heads: i64 = 24;
    let kv_heads: i64 = 8;
    let head_width: i64 = 128;
    let relative_width: i64 = 16;
    let relative_extent: i64 = 1024;
    let rows = positions.len;
    let q_size = heads * head_width;
    let kv_row = kv_heads * head_width;
    if absolute < 0 || window < 0 || rows < 1 || rows > 1048576 || scratch.len < rows ||
       query.len != q_size || output.len < q_size || relative.len != heads * relative_width ||
       bias.len != relative_width * relative_extent || sink.len != heads ||
       keys.len != rows * kv_row || values.len != keys.len { return -1; }

    let qk_scale: f32 = 1.0 / i2_sqrt(128.0);
    for head in heads {
        let q_at = head * head_width;
        let r_at = head * relative_width;
        let kv_at = (head / (heads / kv_heads)) * head_width;
        var maximum: f32 = 0.0;
        for row in rows {
            let k = row * kv_row + kv_at;
            let position = positions[row];
            var logit = float_bits(4286578688);
            if position >= 0 && position <= absolute {
                let distance = absolute - position;
                if window == 0 || distance < window {
                    var relative_score: f32 = 0.0;
                    if distance < relative_extent {
                        let b = distance * relative_width;
                        relative_score = i2_attention_dot(relative[r_at .. r_at + relative_width],
                            bias[b .. b + relative_width]);
                    }
                    let qk = i2_attention_dot(query[q_at .. q_at + head_width], keys[k .. k + head_width]);
                    logit = qk * qk_scale + (relative_score - sink[head]);
                }
            }
            scratch[row] = logit;
            if logit > maximum { maximum = logit; }
        }

        var total: f64 = i2_attn_exp(-maximum) as f64;
        for row in rows {
            let weight = i2_attn_exp(scratch[row] - maximum);
            total += weight as f64;
            scratch[row] = weight;
        }
        let inverse: f32 = 1.0 / (total as f32);
        for row in rows { scratch[row] *= inverse; }

        var sums: f64[128] = [0.0; 128];
        for row in rows {
            let at = row * kv_row + kv_at;
            let weight = scratch[row];
            for channel in head_width {
                let product: f32 = weight * values[at + channel];
                sums[channel] += product as f64;
            }
        }
        for channel in head_width {
            output[q_at + channel] = sums[channel] as f32;
        }
    }
    return q_size;
}

export fn inkling_row_major_i2_shortconv_decode(current: const f32[:], past: const f32[:], static_conv: const f32[:],
                              taps: const f32[:], out: f32[:]) -> i64 {
    return i2_shortconv_decode_impl(current, past, static_conv, taps, out);
}

export fn inkling_row_major_i2_attention_decode(query: const f32[:], keys: const f32[:], values: const f32[:],
                               relative: const f32[:], bias: const f32[:], sink: const f32[:],
                               positions: const i64[:], absolute: i64, window: i64,
                               scratch: f32[:], output: f32[:]) -> i64 {
    return i2_attention_decode_impl(query, keys, values, relative, bias, sink, positions, absolute, window, scratch, output);
}

// Each record is indexed by cache slot and contains its row capacity and
// K, V and short-convolution offsets. Local slots use a bounded ring.
const I2_KV_WORDS: i64 = 4;
const I2_MAX_CONTEXT: i64 = 1048576;
const I2_CACHE_ALIGNMENT: i64 = 32;

fn i2_cache_span(raw: i64) -> i64 {
    return ((raw + I2_CACHE_ALIGNMENT - 1) / I2_CACHE_ALIGNMENT) * I2_CACHE_ALIGNMENT;
}

export fn inkling_row_major_i2_cache_layout(visits: const i64[:], count: i64, capacity: i64, window: i64,
                          key_bytes: i64, value_bytes: i64, history_bytes: i64, slots: i64[:]) -> i64 {
    if count < 1 || count > 128 || visits.len / 4 < count || slots.len / I2_KV_WORDS < count ||
       capacity < 1 || capacity > I2_MAX_CONTEXT || window < 1 || window > I2_MAX_CONTEXT ||
       key_bytes < 1 || key_bytes > 1048576 || value_bytes < 1 || value_bytes > 1048576 ||
       history_bytes < 0 || history_bytes > 1048576 { return -1; }

    var seen: i64[128] = [0; 128];
    for visit in count {
        let slot = visits[visit * 4 + 1];
        let sliding = visits[visit * 4 + 3];
        if slot < 0 || slot >= count || seen[slot] != 0 || sliding < 0 || sliding > 1 { return -1; }
        seen[slot] = 1;
    }

    var next: i64 = 0;
    for visit in count {
        let slot = visits[visit * 4 + 1];
        let sliding = visits[visit * 4 + 3] != 0;
        let rows = if sliding && window < capacity { window } else { capacity };
        let at = slot * I2_KV_WORDS;
        slots[at] = rows;
        slots[at + 1] = next;
        next += i2_cache_span(rows * key_bytes);
        slots[at + 2] = next;
        next += i2_cache_span(rows * value_bytes);
        slots[at + 3] = next;
        next += i2_cache_span(history_bytes);
    }
    return next;
}

// Resolve a committed absolute token position to its physical row.
fn i2_cache_row_impl(position: i64, capacity: i64, rows: i64, sliding: bool) -> i64 {
    if position < 0 || capacity < 1 || capacity > I2_MAX_CONTEXT || position >= capacity ||
       rows < 1 || rows > capacity || (!sliding && rows != capacity) { return -1; }
    return if sliding { position % rows } else { position };
}

export fn inkling_row_major_i2_cache_row(position: i64, capacity: i64, rows: i64, sliding: bool) -> i64 {
    return i2_cache_row_impl(position, capacity, rows, sliding);
}

export fn inkling_row_major_i2_reset_cache_positions(positions: i64[:]) {
    for row in positions.len { positions[row] = -1; }
}

// Bind the new key and value before publishing its position. The caller
// serializes the text stream and supplies one model-visit cache slot.
fn i2_store_kv_impl(position: i64, capacity: i64, sliding: bool, positions: i64[:],
                      key: const f32[:], value: const f32[:], keys: f32[:], values: f32[:]) -> i64 {
    let rows = positions.len;
    let row = i2_cache_row_impl(position, capacity, rows, sliding);
    if row < 0 || key.len < 1 || value.len < 1 || keys.len % rows != 0 ||
       values.len % rows != 0 || keys.len / rows != key.len || values.len / rows != value.len {
        return -1;
    }
    let key_at = row * key.len;
    for i in key.len { keys[key_at + i] = key[i]; }
    let value_at = row * value.len;
    for i in value.len { values[value_at + i] = value[i]; }
    positions[row] = position;
    return row;
}

export fn inkling_row_major_i2_store_kv(position: i64, capacity: i64, sliding: bool, positions: i64[:],
                      key: const f32[:], value: const f32[:], keys: f32[:], values: f32[:]) -> i64 {
    return i2_store_kv_impl(position, capacity, sliding, positions, key, value, keys, values);
}

// Mix the ordered residual blocks for each learned query. Each block row
// is independently RMS-normalized before forming scores and output values.
export fn inkling_row_major_i2_depth_mix(states: const f32[:], width: i64, queries: const f32[:],
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
export fn inkling_row_major_i2_loop_blend(current: const f32[:], message: const f32[:], gain: const f32[:],
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

extern "logf" fn i2_attention_log(value: f32) -> f32;

fn i2_projection_conv(projected: const f32[:], history: const f32[:], kernel: const f32[:],
                      taps: const f32[:], output: f32[:]) {
    var base: f32[1024] = [0.0; 1024];
    for ch in 1024 {
        var value: f32 = 0.0;
        for lag in 3 { value += history[3 * ch + lag] * kernel[4 * ch + lag]; }
        value += projected[ch] * kernel[4 * ch + 3];
        base[ch] = value;
    }
    i2_shortconv_decode_impl(projected, history, base, taps, output);
}

fn i2_advance_projection(history: f32[:], projected: const f32[:]) {
    for i in projected.len {
        let at = i * 3;
        history[at] = history[at + 1];
        history[at + 1] = history[at + 2];
        history[at + 2] = projected[i];
    }
}

// The packed projection block contains Q, K, V, relative query, sink,
// gate and optional K/V taps in the order from i2_attention_projections.
// This is the CPU text-decode path for one physical cache slot.
export fn inkling_row_major_i2_attention_token(projected: const f32[:], q_clip: const f32[:], r_clip: const f32[:],
                             key_kernel: const f32[:], value_kernel: const f32[:], bias: const f32[:],
                             position: i64, capacity: i64, sliding: bool, positions: i64[:],
                             keys: f32[:], values: f32[:], key_history: f32[:], value_history: f32[:],
                             scratch: f32[:], output: f32[:]) -> i64 {
    let window: i64 = if sliding { 1024 } else { 0 };
    let rows = if sliding && capacity > window { window } else { capacity };
    let expected = if sliding { 8600 } else { 8664 };
    if i2_cache_row_impl(position, capacity, rows, sliding) < 0 || positions.len != rows ||
       projected.len != expected || q_clip.len != 3072 || r_clip.len != 384 ||
       key_kernel.len != 4096 || value_kernel.len != 4096 || bias.len != 16384 ||
       keys.len != rows * 1024 || values.len != keys.len || key_history.len != 3072 ||
       value_history.len != 3072 || scratch.len < rows || output.len < 3072 { return -1; }

    var q: f32[3072] = [0.0; 3072];
    var r: f32[384] = [0.0; 384];
    var k_raw: f32[1024] = [0.0; 1024];
    var k: f32[1024] = [0.0; 1024];
    var v: f32[1024] = [0.0; 1024];
    var no_weight: f32[0] = [];
    let k_taps = if sliding { projected[8600 .. 8600] } else { projected[8600 .. 8632] };
    let v_taps = if sliding { projected[8600 .. 8600] } else { projected[8632 .. 8664] };
    i2_projection_conv(projected[3072 .. 4096], key_history, key_kernel, k_taps, k_raw);
    i2_projection_conv(projected[4096 .. 5120], value_history, value_kernel, v_taps, v);
    i2_clip_heads_impl(projected[0 .. 3072], 128, q_clip, 22.627416610717773, q);
    i2_clip_heads_impl(k_raw, 128, no_weight, 22.627416610717773, k);
    i2_clip_heads_impl(projected[5120 .. 5504], 16, r_clip, 8.0, r);

    for head in 8 {
        let at = head * 128;
        let square_sum = i2_attention_dot(v[at .. at + 128], v[at .. at + 128]);
        let norm: f32 = 1.0 / i2_sqrt(square_sum / 128.0 + 0.000001);
        for i in 128 { v[at + i] *= norm; }
    }
    if !sliding {
        let fraction: f32 = ((position as f32) + 1.0) / 8192.0;
        let tau: f32 = 1.0 + 0.1 * i2_attention_log(if fraction < 1.0 { 1.0 } else { fraction });
        for i in q.len { q[i] *= tau; }
        for i in r.len { r[i] *= tau; }
    }
    if i2_store_kv_impl(position, capacity, sliding, positions, k, v, keys, values) < 0 { return -1; }
    i2_advance_projection(key_history, projected[3072 .. 4096]);
    i2_advance_projection(value_history, projected[4096 .. 5120]);
    var size = i2_cuda_attention(q, keys, values, r, bias, projected[5504 .. 5528],
        positions, position, window, output);
    if size == 0 {
        size = i2_attention_decode_impl(q, keys, values, r, bias, projected[5504 .. 5528],
            positions, position, window, scratch, output);
    }
    if size < 0 { return -1; }
    for i in size {
        let gate: f32 = 1.0 / (1.0 + i2_attn_exp(-projected[5528 + i]));
        output[i] *= gate;
    }
    return size;
}

// RMS normalization used on the residual and projection inputs. A gain
// may either be absent or have one value for every channel.
export fn inkling_row_major_i2_rms_gain(input: const f32[:], weight: const f32[:], epsilon: f32, out: f32[:]) -> i64 {
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
export fn inkling_row_major_i2_postconv_token(input: const f32[:], weights: const f32[:],
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
export fn inkling_row_major_i2_text_logits(logits: f32[:], unpadded: i64, cap: f32) -> i64 {
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

fn i2_clip_heads_impl(input: const f32[:], width: i64, weight: const f32[:],
                         limit: f32, output: f32[:]) -> i64 {
    if width < 1 || width > 8192 || input.len == 0 || input.len % width != 0 ||
       (weight.len != 0 && weight.len != input.len) || output.len < input.len ||
       !(limit > 0.0) || limit > 3.4028234663852886e38 { return -1; }

    for row in input.len / width {
        let at = row * width;
        var sum: f64 = 0.0;
        for i in width {
            let k = at + i;
            let scaled = if weight.len == 0 { input[k] } else { input[k] * weight[k] };
            output[k] = scaled;
            let square: f32 = scaled * scaled;
            sum += square as f64;
        }
        let clip = limit / (i2_sqrt(sum as f32) + 0.000001);
        let factor = if clip < 1.0 { clip } else { 1.0 };
        for i in width { output[at + i] *= factor; }
    }
    return input.len;
}
