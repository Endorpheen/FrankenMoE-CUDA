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

    let past = (count - 1) * width;
    return i2_depth_mix_reduce(norm[0 .. past], norm[past .. states.len], queries, probabilities, output, false);
}

fn i2_depth_mix_reduce(history: const f32[:], current: const f32[:], queries: const f32[:],
                       probabilities: f32[:], output: f32[:], reverse: bool) -> i64 {
    let width = current.len;
    let past = history.len / width;
    let count = past + 1;
    let q_count = queries.len / width;
    let score_scale: f32 = 1.0 / i2_sqrt(width as f32);
    for q in q_count {
        let q_at = q * width;
        var maximum: f32 = -3.4028234663852886e38;
        for row in past {
            let at = (if reverse { past - 1 - row } else { row }) * width;
            let score = i2_attention_dot(history[at .. at + width], queries[q_at .. q_at + width]) * score_scale;
            probabilities[row] = score;
            if score > maximum { maximum = score; }
        }
        let score = i2_attention_dot(current, queries[q_at .. q_at + width]) * score_scale;
        probabilities[past] = score;
        if score > maximum { maximum = score; }
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
            for row in past {
                let at = (if reverse { past - 1 - row } else { row }) * width;
                let term = probabilities[row] * history[at + i];
                sum += term as f64;
            }
            let term = probabilities[past] * current[i];
            sum += term as f64;
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

// History rows are already normalized, stored in chronological order.
export fn i2_depth_mix_cached(history: const f32[:], normalized_current: const f32[:], queries: const f32[:],
                             probabilities: f32[:], output: f32[:]) -> i64 {
    let width = normalized_current.len;
    if width <= 0 || width > 8192 || history.len % width != 0 || history.len / width >= 128 ||
       queries.len == 0 || queries.len % width != 0 || queries.len / width > 24 ||
       output.len < queries.len || probabilities.len <= history.len / width {
        return -1;
    }
    return i2_depth_mix_reduce(history, normalized_current, queries, probabilities, output, true);
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



export fn i2_prepare_native(projected: const f32[:], q_clip: const f32[:], r_clip: const f32[:],
    key_kernel: const f32[:], value_kernel: const f32[:], position: i64, sliding: bool,
    key_history: f32[:], value_history: f32[:], q: f32[:], r: f32[:], k: f32[:], v: f32[:]) -> i64 {
    let expected = if sliding { 8600 } else { 8664 };
    if position < 0 || projected.len != expected || q_clip.len != 3072 || r_clip.len != 384 ||
       key_kernel.len != 4096 || value_kernel.len != 4096 || q.len != 3072 || r.len != 384 ||
       k.len != 1024 || v.len != 1024 || key_history.len != 3072 || value_history.len != 3072 { return -1; }
    var k_raw: f32[1024] = [0.0; 1024];
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
    i2_advance_projection(key_history, projected[3072 .. 4096]);
    i2_advance_projection(value_history, projected[4096 .. 5120]);
    return 1;
}

export fn i2_finish_native(gate: const f32[:], output: f32[:]) -> i64 {
    if gate.len != 3072 || output.len != 3072 { return -1; }
    for i in output.len {
        let weight: f32 = 1.0 / (1.0 + i2_attn_exp(-gate[i]));
        output[i] *= weight;
    }
    return output.len;
}

extern "fg_i2_prefill_partition" fn i2_prefill_partition(work: u64, begin: i64, end: i64);
extern "fg143_gather_affinity" fn i2_prefill_affinity(prefill: bool, task: i64) -> i64;

thread_fn i2_prefill_worker(work: u64, rows: i64, tasks: i64, task: i64) {
    assert(i2_prefill_affinity(true, task) == 0);
    i2_prefill_partition(work, rows * task / tasks, rows * (task + 1) / tasks);
}

export fn i2_parallel_prefill(work: u64, rows: i64, tasks: i64) -> i64 {
    if work == 0 || rows <= 0 || tasks <= 0 || tasks > rows || tasks > 16 { return -1; }
    if tasks == 1 {
        i2_prefill_partition(work, 0, rows);
        return rows;
    }
    var workers: i64[>..] = [];
    for task in 1 .. tasks { workers.push(thread_spawn_contained(i2_prefill_worker, work, rows, tasks, task)); }
    i2_prefill_partition(work, 0, rows / tasks);
    var ok = true;
    for id in workers {
        if !thread_wait(id) { ok = false; }
    }
    return if ok { rows } else { -1 };
}

extern "fg_i2_prefill_team_worker" fn i2_team_participant(team: u64, task: i64);

thread_fn i2_prefill_team_worker(team: u64, task: i64) {
    i2_team_participant(team, task);
}

export fn i2_prefill_team_start(team: u64, tasks: i64, handles: i64[:]) -> i64 {
    if team == 0 || tasks <= 1 || tasks > 16 || handles.len != tasks - 1 { return -1; }
    for task in 1 .. tasks {
        handles[task - 1] = thread_spawn_contained(i2_prefill_team_worker, team, task);
    }
    return tasks - 1;
}

export fn i2_prefill_team_join(handles: const i64[:]) -> i64 {
    var ok = true;
    for id in handles {
        if !thread_wait(id) { ok = false; }
    }
    return if ok { 0 } else { -1 };
}

export fn inkling_row_major_i2_postconv_batch(input: const f32[:], weights: const f32[:],
    history: f32[:], out: f32[:], width: i64, from_channel: i64, to_channel: i64) -> i64 {
    if width < 1 || input.len % width != 0 || input.len != out.len ||
        input.len < width || weights.len != width * 4 || history.len != width * 3 ||
        from_channel < 0 || to_channel > width || to_channel <= from_channel {
        return -1;
    }
    let rows = input.len / width;
    let inverse: f32 = 1.0 / i2_sqrt(5.0);
    for t in rows {
        for offset in to_channel - from_channel {
            let i = from_channel + offset;
            let k = i * 4;
            let current = input[t * width + i];
            var value: f32 = 0.0;
            for tap in 3 {
                let prior = t + tap - 3;
                let sample = if prior < 0 { history[i * 3 + t + tap] } else { input[prior * width + i] };
                value += sample * weights[k + tap];
            }
            value += current * weights[k + 3];
            out[t * width + i] = (current + value) * inverse;
        }
    }
    for offset in to_channel - from_channel {
        let i = from_channel + offset;
        for tap in 3 {
            let prior = rows + tap - 3;
            if prior < 0 {
                history[i * 3 + tap] = history[i * 3 + rows + tap];
            } else {
                history[i * 3 + tap] = input[prior * width + i];
            }
        }
    }
    return to_channel - from_channel;
}

extern "fg_cuda_load_experts" fn i2_load_expert_ranges(groups: i64[:], layers: i64[:],
    bytes: i64, words32: i64, words64: i64) -> i64;
extern "fg_cuda_load_dense" fn i2_load_dense_ranges(ranges: i64[:], bytes: i64) -> i64;

export fn i2_place_expert_bank(tensors: const i64[:], core: const i64[:],
        data: i64, budget: i64) -> i64 {
    let layers_count = 24;
    let experts_count = 128;
    let up = tensors[12 * core[6] + 9] / experts_count;
    let down = tensors[12 * core[7] + 9] / experts_count;
    if up <= 0 || down <= 0 { return -1; }
    var layers: i64[96] = [0; 96];
    for layer in layers_count {
        let u = 12 * core[10 * layer + 6];
        let v = 12 * core[10 * layer + 7];
        if tensors[u + 9] != up * experts_count || tensors[v + 9] != down * experts_count { return -1; }
        layers[4 * layer + 1] = data + tensors[u + 10];
        layers[4 * layer + 3] = data + tensors[v + 10];
    }
    let spans = layers_count * experts_count;
    let fit = budget / (up + down);
    let slots = if fit < spans { fit } else { spans };
    if slots < experts_count { return -1; }
    var groups: i64[9] = [up, 0, down, experts_count, layers_count, slots, 0, spans, layers_count];
    return i2_load_expert_ranges(groups[..], layers[..], slots * (up + down),
        spans + 4 * slots + 4 + 5 * experts_count, layers_count + experts_count + 5);
}

fn i2_append_dense_range(ranges: i64[:], state: i64[:], budget: i64, start: i64, bytes: i64) {
    let aligned = (bytes + 31) / 32 * 32;
    if aligned > budget - state[1] { return; }
    let word = state[0];
    ranges[word] = start;
    ranges[word + 1] = bytes;
    ranges[word + 2] = state[1];
    state[0] += 3;
    state[1] += aligned;
}

export fn i2_place_weights(tensors: const i64[:], core: const i64[:],
        data: i64, excluded: i64, budget: i64, adaptive: bool, keep_routers: bool,
        ranges: i64[:], totals: i64[:]) -> i64 {
    var state: i64[2] = [0; 2];
    var router_bytes: i64 = 0;
    for i in tensors.len / 12 {
        let r = 12 * i;
        var router = false;
        if keep_routers && tensors[r + 2] == 0 && tensors[r + 4] == 2304 && tensors[r + 5] == 128 {
            for layer in 24 {
                if core[10 * layer + 5] == i { router = true; break; }
            }
        }
        if router || (tensors[r + 2] == 8 && i != excluded) {
            let before = state[1];
            i2_append_dense_range(ranges, state[..], budget, data + tensors[r + 10], tensors[r + 9]);
            if router { router_bytes += state[1] - before; }
        }
    }
    totals[0] = state[1];
    totals[1] = router_bytes;
    if !adaptive {
        for phase in 2 {
            for expert in 128 {
                for layer in 24 {
                    let repeated = layer >= 6 && layer < 18;
                    if repeated != (phase == 0) { continue; }
                    let a = 12 * core[layer * 10 + 6];
                    let b = 12 * core[layer * 10 + 7];
                    assert(tensors[a + 9] % 128 == 0 && tensors[b + 9] % 128 == 0);
                    let up = tensors[a + 9] / 128;
                    let down = tensors[b + 9] / 128;
                    if (up + 31) / 32 * 32 + (down + 31) / 32 * 32 > budget - state[1] { continue; }
                    i2_append_dense_range(ranges, state[..], budget, data + tensors[a + 10] + up * expert, up);
                    i2_append_dense_range(ranges, state[..], budget, data + tensors[b + 10] + down * expert, down);
                }
            }
        }
    }
    assert(state[0] > 0 && i2_load_dense_ranges(ranges[0..state[0]], state[1]) == state[1]);
    return state[1];
}

extern "i2_serial_layer" fn i2_host_serial_layer(decoder: u64, physical: i32, slot: i32,
    absolute: i32, sliding: i32, current: f32[:], history: f32[:], count: i32,
    messages: f32[:]) -> i32;
extern "i2_serial_mix" fn i2_host_serial_mix(decoder: u64, physical: i32, history: f32[:],
    count: i32, output: f32[:], messages: f32[:]);
extern "i2_serial_trace" fn i2_host_serial_trace(visit: i32, current: f32[:]);

export fn i2_walk_serial_token(decoder: u64, visits: const i64[:], absolute: i32,
        current: f32[:], history: f32[:], prelude: f32[:], bank_output: f32[:],
        bank: f32[:], messages: f32[:], trace: bool) -> i32 {
    let width = current.len;
    var count: i32 = 0;
    var prelude_count: i32 = 0;
    var bank_count: i32 = 0;
    for visit in visits.len / 4 {
        let at = visit * 4;
        let physical = visits[at] as i32;
        let slot = visits[at + 1] as i32;
        let pass = visits[at + 2];
        let sliding = visits[at + 3] as i32;
        if physical == 6 {
            if pass == 0 {
                for i in width { prelude[i] = current[i]; }
                prelude_count = count;
            } else {
                bank_count = count - prelude_count - 1;
                assert(bank_count >= 0 && bank_count < 11);
                let first = ((prelude_count as i64) + 1) * width;
                for i in (bank_count as i64) * width { bank[i] = history[first + i]; }
                for i in width { bank_output[i] = current[i]; current[i] = prelude[i]; }
                count = prelude_count;
            }
        }
        if pass > 0 { i2_host_serial_mix(decoder, physical, bank, bank_count, bank_output, messages); }
        count = i2_host_serial_layer(decoder, physical, slot, absolute, sliding, current,
                                     history, count, if pass > 0 { messages } else { messages[0..0] });
        if trace { i2_host_serial_trace(visit as i32, current); }
    }
    return count;
}

export fn i2_propose_draft(history: i32[:], result: i32[:], position: i64, first: i32) -> i32 {
    let order = 4;
    let remaining = history.len - position;
    let limit = if remaining < result.len { remaining } else { result.len };
    if position < 2 * order || limit < 2 { return 0; }
    history[position] = first;
    let suffix = position + 1 - order;
    var end = position - 2;
    while end >= order {
        let candidate = end - order;
        if history[candidate] == history[suffix] &&
           history[candidate + 1] == history[suffix + 1] &&
           history[candidate + 2] == history[suffix + 2] &&
           history[candidate + 3] == first {
            let available = position - end;
            let take = if available < limit - 1 { available } else { limit - 1 };
            result[0] = first;
            for i in take { result[i + 1] = history[end + i]; }
            return (take + 1) as i32;
        }
        end -= 1;
    }
    return 0;
}

extern "i2_adapter_draft_start" fn i2_start_draft(decoder: u64, count: i32) -> bool;
extern "i2_adapter_draft_reject" fn i2_reject_draft(decoder: u64) -> bool;
extern "i2_adapter_draft_take" fn i2_take_draft(decoder: u64, logits: f32[:]);
extern "i2_adapter_run_step" fn i2_run_step(decoder: u64, tokens: const i32[:], prompt: bool, logits: f32[:]);

export fn i2_eval_prepared(decoder: u64, tokens: const i32[:], prompt: bool, logits: f32[:],
        history: i32[:], proposals: i32[:], position: i64, pending: bool,
        used: i32, retain: bool) -> i64 {
    if history.len > 0 {
        if pending && !prompt && proposals[used as i64] == tokens[0] {
            history[position] = tokens[0];
            i2_take_draft(decoder, logits);
            return logits.len;
        }
        if pending && !i2_reject_draft(decoder) { return -1; }
        if !prompt && logits.len > 0 && retain {
            let proposed = i2_propose_draft(history, proposals, position, tokens[0]);
            if proposed > 1 && i2_start_draft(decoder, proposed) {
                i2_take_draft(decoder, logits);
                return logits.len;
            }
        }
        for i in tokens.len { history[position + i] = tokens[i]; }
    }
    i2_run_step(decoder, tokens, prompt, logits);
    return logits.len;
}

const I2_RETAIN_OUTPUT: u64 = 1;
const I2_USE_LOOP_STORAGE: u64 = 2;
const I2_ENTER_PRELUDE: u64 = 4;
const I2_SAVE_BOUNDARY: u64 = 8;
const I2_RELEASE_LOOP: u64 = 16;

fn i2_prefill_edges(visits: const i64[:], aux: const i64[:], visit: i64,
        use_edges: bool, boundary_enabled: bool, boundary_history: bool, extend: bool,
        use_full: bool, keep_intermediate: bool) -> u64 {
    let at = visit * 4;
    let current = visits[at];
    let current_pass = visits[at + 2];
    let more = at + 4 < visits.len;
    let enter_prelude = use_edges && current == 6 && current_pass == 0;
    let save_boundary = boundary_history && current == 6 && current_pass > 0;
    var result: u64 = (if enter_prelude { I2_ENTER_PRELUDE } else { 0 }) |
        (if save_boundary { I2_SAVE_BOUNDARY } else { 0 }) | (if current_pass < 0 { I2_RELEASE_LOOP } else { 0 });
    if !keep_intermediate { return result; }
    if !more {
        return if use_edges { result | I2_RETAIN_OUTPUT | I2_USE_LOOP_STORAGE } else { result };
    }
    let next = visits[at + 4];
    let pass = visits[at + 6];
    let rewind = next == 6 && pass > 0;
    let tail = next >= 18;
    let ordinary = current < 18 && next < 18 && (next % 6 != 0 || (boundary_enabled && next == 12));
    let prelude_entry = use_edges && next == 6 && pass == 0;
    let retain = (ordinary || prelude_entry || (extend && ((rewind && boundary_history) || tail))) &&
        (visits[at + 7] != 0 || use_full) && aux[next * 30 + 10] >= 0;
    if retain {
        result = result | I2_RETAIN_OUTPUT;
        if tail { result = result | I2_USE_LOOP_STORAGE; }
    }
    return result;
}


export fn i2_prefill_edge_plan(visits: const i64[:], aux: const i64[:], use_edges: bool,
        boundary_enabled: bool, extend: bool, use_full: bool, keep_intermediate: bool,
        ordinary: i64[:], boundary: i64[:]) {
    let count = visits.len / 4;
    assert(visits.len % 4 == 0 && ordinary.len == count && boundary.len == count);
    for visit in count {
        ordinary[visit] = i2_prefill_edges(visits, aux, visit, use_edges, boundary_enabled,
                                          false, extend, use_full, keep_intermediate) as i64;
        boundary[visit] = i2_prefill_edges(visits, aux, visit, use_edges, boundary_enabled,
                                          true, extend, use_full, keep_intermediate) as i64;
    }
}

extern "inkling_loop_storage" fn i2_prefill_loop_storage() -> u64;
extern "inkling_prelude_use" fn i2_prefill_use_prelude(save: bool) -> bool;
extern "inkling_loop_release" fn i2_prefill_release_loop();
extern "i2_prefill_save_rewind" fn i2_prefill_rewind_save(work: u64, pass: i64, current: bool);
extern "i2_prefill_read_boundary" fn i2_prefill_boundary_read(work: u64);
extern "i2_prefill_run_visit" fn i2_prefill_visit(work: u64, index: i64,
    device: i64[:], retain: bool, slot: u64, loop_live: bool, prelude_live: bool);

export fn i2_run_prefill_visits(work: u64, visits: const i64[:], ordinary: const i64[:],
        boundary: const i64[:], device: i64[:], loop_enabled: bool) {
    let count = visits.len / 4;
    assert(visits.len % 4 == 0 && ordinary.len == count && boundary.len == count && device.len == 4);
    var loop_live = false;
    var prelude_live = false;
    for visit in count {
        let flags = (if device[1] != 0 { boundary[visit] } else { ordinary[visit] }) as u64;
        let retain = (flags & I2_RETAIN_OUTPUT) != 0;
        let external = (flags & I2_USE_LOOP_STORAGE) != 0;
        let slot = if external { i2_prefill_loop_storage() } else { 0 };
        assert(!external || slot != 0);
        if device[0] != 0 && (flags & I2_ENTER_PRELUDE) != 0 {
            assert(i2_prefill_use_prelude(true));
            prelude_live = true;
        }
        if (flags & I2_RELEASE_LOOP) != 0 && loop_live {
            i2_prefill_release_loop();
            loop_live = false;
        }
        if (flags & I2_SAVE_BOUNDARY) != 0 {
            if loop_enabled {
                i2_prefill_rewind_save(work, visits[4 * visit + 2], device[0] != 0);
                device[0] = 0;
                if prelude_live {
                    assert(i2_prefill_use_prelude(false));
                    device[0] = 1;
                }
                loop_live = true;
            } else {
                i2_prefill_boundary_read(work);
            }
            device[1] = 0;
        }
        i2_prefill_visit(work, visit, device, retain, slot, loop_live, prelude_live);
    }
    device[3] = if prelude_live { 1 } else { 0 };
}
