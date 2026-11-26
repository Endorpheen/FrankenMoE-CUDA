extern "fg_vk_inkling_op" fn vi_op(context: u64, op: i64, physical: i64, slot: i64,
                                  pass: i64, sliding: bool) -> i64;

export fn fg_vk_inkling_forward(context: u64, visits: const i64[:], logits: bool) -> i64 {
    if visits.len != 48 * 4 || vi_op(context, 0, 0, 0, 0, false) != 0 { return -1; }
    for i in 48 {
        let at = i * 4;
        if vi_op(context, 1, visits[at], visits[at + 1], visits[at + 2], visits[at + 3] != 0) != 0 {
            return -2 - i;
        }
    }
    if logits && vi_op(context, 2, 0, 0, 0, false) != 0 { return -50; }
    return 0;
}
