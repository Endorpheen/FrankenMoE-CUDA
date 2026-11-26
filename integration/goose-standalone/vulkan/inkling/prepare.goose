extern "sqrtf" fn i2_sqrt(value: f32) -> f32;
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

// Clipping is based on the norm of the scaled row, not on individual
// channel magnitudes. An empty scale leaves that row unchanged before clipping.
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

extern "tanhf" fn i2_tanh(value: f32) -> f32;

// Apply the model's pre-attention transforms for the resident Q4 Vulkan cache.
export fn i2_vk_prepare(projected: const f32[:], q_clip: const f32[:], r_clip: const f32[:],
                        key_kernel: const f32[:], value_kernel: const f32[:], position: i64,
                        sliding: bool, key_history: f32[:], value_history: f32[:],
                        qrs: f32[:], key: f32[:], value: f32[:]) -> i64 {
    let count = if sliding { 8600 } else { 8664 };
    if projected.len != count || q_clip.len != 3072 || r_clip.len != 384 ||
       key_kernel.len != 4096 || value_kernel.len != 4096 || position < 0 ||
       key_history.len != 3072 || value_history.len != 3072 ||
       qrs.len != 3480 || key.len != 1024 || value.len != 1024 { return -1; }
    var raw_key: f32[1024] = [0.0; 1024];
    var empty: f32[0] = [];
    let k_taps = if sliding { projected[8600 .. 8600] } else { projected[8600 .. 8632] };
    let v_taps = if sliding { projected[8600 .. 8600] } else { projected[8632 .. 8664] };
    i2_projection_conv(projected[3072 .. 4096], key_history, key_kernel, k_taps, raw_key[..]);
    i2_projection_conv(projected[4096 .. 5120], value_history, value_kernel, v_taps, value);
    if i2_clip_heads_impl(projected[0 .. 3072], 128, q_clip, 22.627416610717773, qrs[0 .. 3072]) < 0 ||
       i2_clip_heads_impl(raw_key[..], 128, empty[..], 22.627416610717773, key) < 0 ||
       i2_clip_heads_impl(projected[5120 .. 5504], 16, r_clip, 8.0, qrs[3072 .. 3456]) < 0 { return -1; }
    for head in 8 {
        let at = head * 128;
        let part = value[at .. at + 128];
        let inverse: f32 = 1.0 / i2_sqrt(i2_attention_dot(part, part) / 128.0 + 0.000001);
        for i in 128 { value[at + i] *= inverse; }
    }
    if !sliding {
        let fraction: f32 = ((position as f32) + 1.0) / 8192.0;
        let tau: f32 = 1.0 + 0.1 * i2_attention_log(if fraction < 1.0 { 1.0 } else { fraction });
        for i in 3456 { qrs[i] *= tau; }
    }
    for i in 24 { qrs[3456 + i] = projected[5504 + i]; }
    i2_advance_projection(key_history, projected[3072 .. 4096]);
    i2_advance_projection(value_history, projected[4096 .. 5120]);
    return 0;
}
