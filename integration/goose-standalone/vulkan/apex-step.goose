extern "fg_vk_apex_op" fn va_op(context: u64, op: i64, layer: i64) -> i64;

export fn fg_vk_apex_forward(context: u64, logits: i64) -> i64 {
    if va_op(context, 0, 0) != 0 { return -1; }
    for layer in 40 {
        if va_op(context, 1, layer) != 0 { return -2 - layer; }
    }
    if logits != 0 && va_op(context, 2, 0) != 0 { return -42; }
    return 0;
}
