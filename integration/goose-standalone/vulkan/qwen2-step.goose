// Qwen2 operations map to the native Vulkan tensor bindings.
extern "fg_vk_qwen_op" fn vq_op(context: u64, op: i64, layer: i64) -> i64;

fn vq_layer(context: u64, layer: i64) -> bool {
    return vq_op(context, 1, layer) == 0 &&  // Attention normalization
           vq_op(context, 2, layer) == 0 &&  // Q/K/V projections and biases
           vq_op(context, 3, layer) == 0 &&  // RoPE and KV writes
           vq_op(context, 4, layer) == 0 &&  // GQA, output projection and residual
           vq_op(context, 5, layer) == 0 &&  // FFN normalization
           vq_op(context, 6, layer) == 0 &&  // Gate, up and SwiGLU
           vq_op(context, 7, layer) == 0;    // Down projection and residual
}

export fn fg_vk_qwen2_forward(context: u64, layers: i64, logits: i64) -> i64 {
    if layers <= 0 || vq_op(context, 0, 0) != 0 { return -1; }
    for layer in layers {
        if !vq_layer(context, layer) { return -2 - layer; }
    }
    if logits != 0 && vq_op(context, 8, 0) != 0 { return -100; }
    return 0;
}
