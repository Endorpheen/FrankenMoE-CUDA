extern "giga_bits_to_float" fn giga_f32(bits: u32) -> f32;
extern "logf" fn giga_log(x: f32) -> f32;

fn giga_meta_bits(data: const u8[:], state: i64[:], kv: i64[:], slots: i64[:],
                   key: const u8[:], default_bits: i64) -> i64 {
    let index = gr_find_kv(data, state, kv, slots, key);
    if index < 0 { return default_bits; }
    var value: i64[1] = [0; 1];
    if gr_meta_f32_bits(data, state, kv, index, value) != 0 { return -1; }
    return value[0];
}

export fn giga_read_config(data: const u8[:], state: i64[:], kv: i64[:], slots: i64[:],
                            tuning: f32[:], rope: f32[:], limits: f32[:], flags: i64[:]) -> i64 {
    if tuning.len < 5 || rope.len < 6 || limits.len < 65 || flags.len < 2 { return -1; }
    var mode: i64[4] = [0; 4];
    let scaling = gr_find_kv(data, state, kv, slots, "gigachat4.rope.scaling.type");
    if gr_val_span(state, kv, scaling, mode) != 0 || mode[0] != 8 || mode[2] != 4 ||
       data[mode[1] .. mode[1] + 4] != "yarn" { return -2; }
    let original = giga_int(data, state, kv, slots, "gigachat4.rope.scaling.original_context_length");
    let norm = giga_int(data, state, kv, slots, "gigachat4.expert_weights_norm");
    if original <= 0 || original > 1048576 || norm < 0 || norm > 1 { return -2; }

    let eps = giga_meta_bits(data, state, kv, slots, "gigachat4.attention.layer_norm_rms_epsilon", -1);
    let scale = giga_meta_bits(data, state, kv, slots, "gigachat4.expert_weights_scale", -1);
    let cap = giga_meta_bits(data, state, kv, slots, "gigachat4.final_logit_softcapping", 0);
    let base = giga_meta_bits(data, state, kv, slots, "gigachat4.rope.freq_base", -1);
    let factor = giga_meta_bits(data, state, kv, slots, "gigachat4.rope.scaling.factor", -1);
    let beta_fast = giga_meta_bits(data, state, kv, slots, "gigachat4.rope.scaling.yarn_beta_fast", 1107296256);
    let beta_slow = giga_meta_bits(data, state, kv, slots, "gigachat4.rope.scaling.yarn_beta_slow", 1065353216);
    let log_mul = giga_meta_bits(data, state, kv, slots, "gigachat4.rope.scaling.yarn_log_multiplier", 0);
    let attn_factor = giga_meta_bits(data, state, kv, slots, "gigachat4.rope.scaling.attn_factor", 1065353216);
    if eps < 0 || scale < 0 || cap < 0 || base < 0 || factor < 0 ||
       beta_fast < 0 || beta_slow < 0 || log_mul < 0 || attn_factor < 0 { return -3; }
    let stretch = giga_f32(factor as u32);
    if !(stretch >= 1.0) { return -3; }
    let correction = 1.0 + 0.1 * giga_log(stretch);
    let yarn_multiplier = giga_f32(log_mul as u32) / 0.1;
    let all_dims = 1.0 + 0.1 * yarn_multiplier * giga_log(stretch);
    let attention = giga_f32(attn_factor as u32);
    let rope_attention = ((correction / all_dims) * (1.0 / correction)) * attention;
    let mscale = (rope_attention * correction) * all_dims;
    tuning[0] = giga_f32(eps as u32);
    tuning[1] = mscale * mscale / giga_sqrt(192.0);
    tuning[2] = 0.0;
    tuning[3] = giga_f32(scale as u32);
    tuning[4] = giga_f32(cap as u32);
    rope[0] = giga_f32(base as u32);
    rope[1] = 1.0 / stretch;
    rope[2] = 1.0;
    rope[3] = rope_attention;
    rope[4] = giga_f32(beta_fast as u32);
    rope[5] = giga_f32(beta_slow as u32);

    let routed = gr_find_kv(data, state, kv, slots, "gigachat4.swiglu_clamp_exp");
    let shared = gr_find_kv(data, state, kv, slots, "gigachat4.swiglu_clamp_shexp");
    if gr_arr_type(state, kv, routed) != 6 || gr_arr_type(state, kv, shared) != 6 { return -4; }
    var raw: i64[2] = [0; 2];
    for i in 33 {
        if gr_array_raw_bits(data, state, kv, shared, i, raw) != 0 || raw[1] != 4 { return -4; }
        limits[32 + i] = giga_f32(raw[0] as u32);
        if i < 32 {
            if gr_array_raw_bits(data, state, kv, routed, i, raw) != 0 || raw[1] != 4 { return -4; }
            limits[i] = giga_f32(raw[0] as u32);
        }
    }
    flags[0] = original;
    flags[1] = norm;
    return 0;
}
