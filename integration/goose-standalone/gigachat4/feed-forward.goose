extern "ggml_argsort_top_k" fn giga_argsort_top_k(ctx: u64, x: u64, count: i32) -> u64;
extern "ggml_get_rows" fn giga_get_rows(ctx: u64, x: u64, ids: u64) -> u64;
extern "ggml_sum_rows" fn giga_sum_rows(ctx: u64, x: u64) -> u64;
extern "ggml_div" fn giga_div(ctx: u64, x: u64, y: u64) -> u64;
extern "ggml_scale_bias" fn giga_scale_bias(ctx: u64, x: u64, scale: f32, bias: f32) -> u64;
extern "ggml_clamp" fn giga_clamp(ctx: u64, x: u64, low: f32, high: f32) -> u64;
extern "ggml_swiglu_split" fn giga_swiglu(ctx: u64, gate: u64, up: u64) -> u64;
extern "ggml_swiglu_clamp" fn giga_swiglu_limited(ctx: u64, gate: u64, up: u64, limit: f32) -> u64;
extern "ggml_mul_mat_id" fn giga_mat_id(ctx: u64, w: u64, x: u64, ids: u64) -> u64;
extern "ggml_build_forward_expand" fn giga_expand(graph: u64, x: u64);

fn giga_ff_activation(ctx: u64, gate: u64, up: u64, limit: f32) -> u64 {
    if limit > 0.000001 {
        let clamped = giga_swiglu_limited(ctx, gate, up, limit);
        giga_op_param(clamped as! i64, 13, 1);
        return clamped;
    }
    return giga_swiglu(ctx, gate, up);
}

export fn giga_dense_ffn(ctx: u64, input: u64, gate: u64, up: u64,
                          down: u64, limit: f32) -> u64 {
    if ctx == 0 || input == 0 || gate == 0 || up == 0 || down == 0 { return 0; }
    let hidden = giga_ff_activation(ctx, giga_mat(ctx, gate, input), giga_mat(ctx, up, input), limit);
    return giga_mat(ctx, down, hidden);
}

// Weight order follows the routed and shared FFN bindings.
export fn giga_moe_ffn(ctx: u64, graph: u64, input: u64, weights: u64[:],
                        tokens: i64, normalize: bool, norm_eps: f32,
                        routing_scale: f32, routed_limit: f32, shared_limit: f32) -> u64 {
    if ctx == 0 || graph == 0 || input == 0 || weights.len < 8 ||
       tokens < 1 || !(routing_scale > 0.0) { return 0; }
    for i in 8 { if weights[i] == 0 { return 0; } }

    let probs = giga_sigmoid(ctx, giga_mat(ctx, weights[0], input));
    let picks = giga_argsort_top_k(ctx, giga_add(ctx, probs, weights[1]), 8);
    var mix = giga_get_rows(ctx, giga_reshape3(ctx, probs, 1, 192, tokens), picks);
    if normalize {
        let flat = giga_reshape2(ctx, mix, 8, tokens);
        var sum = giga_sum_rows(ctx, flat);
        if norm_eps > 0.0 {
            sum = giga_scale_bias(ctx, sum, 1.0, norm_eps);
        } else {
            sum = giga_clamp(ctx, sum, 0.00006103515625, 1.0 / 0.0);
        }
        mix = giga_reshape3(ctx, giga_div(ctx, flat, sum), 1, 8, tokens);
    }
    if routing_scale != 1.0 { mix = giga_scale(ctx, mix, routing_scale); }
    giga_expand(graph, mix);
    giga_expand(graph, input);

    let x = giga_reshape3(ctx, input, 2560, 1, tokens);
    let up = giga_mat_id(ctx, weights[3], x, picks);
    let gate = giga_mat_id(ctx, weights[2], x, picks);
    let active = giga_ff_activation(ctx, gate, up, routed_limit);
    let routed = giga_mul(ctx, giga_mat_id(ctx, weights[4], active, picks), mix);
    giga_expand(graph, routed);

    var views: u64[8] = [0; 8];
    for i in 8 {
        views[i] = giga_view2(ctx, routed, 2560, tokens, 81920, (i as u64) * 10240);
        giga_expand(graph, views[i]);
    }
    var total = views[0];
    for i in 7 {
        total = giga_add(ctx, total, views[i + 1]);
        giga_expand(graph, total);
    }
    let shared = giga_dense_ffn(ctx, input, weights[5], weights[6], weights[7], shared_limit);
    return giga_add(ctx, total, shared);
}
