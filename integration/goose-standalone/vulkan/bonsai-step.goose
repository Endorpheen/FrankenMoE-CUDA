
extern "fg_vk_bonsai_op" fn vb_op(context: u64, op: i64, layer: i64) -> i64;
export fn fg_vk_bonsai_forward(context: u64, logits: i64) -> i64 {
    if vb_op(context, 0, 0) != 0 { return -1; }
    for layer in 64 {
        if vb_op(context, 1, layer) != 0 { return -2 - layer; }
    }
    if logits != 0 && vb_op(context, 2, 0) != 0 { return -66; }
    return 0;
}
