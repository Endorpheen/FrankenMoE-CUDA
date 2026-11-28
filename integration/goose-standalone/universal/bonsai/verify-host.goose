const VERIFY_LAYER_IDS: u64 = 328;
const VERIFY_PREFIX: u64 = 584;
const VERIFY_ROWS_OFFSET: u64 = 52;
const VERIFY_POSITIONS: u64 = 280;

extern fn bonsai_bonsai_verify_position_packets(position: u64, rows: i64, packets: VocabSpan) -> bool;
extern fn bonsai_bonsai_verify_launch_rows(model: u64, operation: i32, input: VocabSpan, config: VocabSpan) -> i32;
extern fn bonsai_bonsai_verify_conv_args(input: VocabSpan, prefix: VocabSpan, rows: u32, output: VocabSpan) -> i32;
extern fn bonsai_bonsai_verify_gdn_rows(model: u64, input: VocabSpan, prefix: VocabSpan, state: u64, rows: u32) -> i32;
extern fn bonsai_bonsai_verify_commit(states: VocabSpan, prefix: VocabSpan, layers: VocabSpan,
    start: u32, rows: u32, kept: u32) -> i64;
extern fn fg_bonsai_verify_launch(model: u64, kernel: i32, x: u32, y: u32, z: u32, threads: u32, args: u64) -> i32;

export fn fg_bonsai_batch_verify(model: u64, count: u32) -> i32 {
    return if batch_u32(model + 56) != 0 && batch_u32(model + 60) != 0 &&
        batch_u32(model + BATCH_IN_DECODE) != 0 && batch_u32(model + 4712) == 1 &&
        count == batch_u32(model + VERIFY_ROWS_OFFSET) { 1 } else { 0 };
}

fn fg_bonsai_batch_init_verify(model: u64) -> i32 {
    let rows = batch_u32(model + VERIFY_ROWS_OFFSET) as! i64;
    var packets: u64[120] = [0; 120];
    let address = vocab_address(unsafe_c_address<c_ptr<const void>>(packets[0]));
    if !bonsai_bonsai_verify_position_packets(batch_u32(model + BATCH_POSITION) as! u64, rows,
        vocab_span(address, rows * 24)) { return -1; }
    return batch_copy(fg_bonsai_cuda_dispatch(), BONSAI_CUDA_UPLOAD, vocab_word(model + VERIFY_POSITIONS),
        address, (rows as! u64) * 192, "verify row configs");
}

export fn bonsai_bonsai_verify_attn_launch(model: u64, role: u32, x: u32, y: u32, z: u32, threads: u32, args: u64) -> i32 {
    let kernels: i32[17] = [86, 88, 91, 95, 87, 14, 2, 51, 4, 5, 8, 74, 72, 40, 39, 10, 73];
    if role >= 17 { return -1; }
    return fg_bonsai_verify_launch(model, kernels[role as! i64], x, y, z, threads, args);
}

fn verify_flag(model: u64, offset: u64, flag: u64) -> u64 {
    return if batch_u32(model + offset) != 0 { flag } else { 0 };
}

export fn fg_bonsai_launch_verify_rows(model: u64, operation: i32, input: u64) -> i32 {
    let flags = verify_flag(model, 4780, 1) | verify_flag(model, 14752, 2) | verify_flag(model, 4744, 4) |
        (if batch_u32(model + 4744) == 2 { 8 } else { 0 }) | verify_flag(model, 4752, 16) |
        verify_flag(model, 4748, 32) | verify_flag(model, 64, 64) | verify_flag(model, 72, 128) |
        verify_flag(model, 68, 256);
    var config: u64[8] = [batch_u32(model + BATCH_POSITION) as! u64, batch_u32(model + VERIFY_ROWS_OFFSET) as! u64,
        flags, vocab_word(model + 14656), vocab_word(model + 5696), vocab_word(model + VERIFY_POSITIONS),
        vocab_word(model + 264), vocab_word(model + 272)];
    return bonsai_bonsai_verify_launch_rows(model, operation, vocab_span(input, 8),
        vocab_span(vocab_address(unsafe_c_address<c_ptr<const void>>(config[0])), 8));
}

export fn fg_bonsai_transaction_conv(model: u64, input: u64) -> i32 {
    let index = batch_u32(model + BATCH_TXN_NEXT_LAYER);
    if index >= batch_u32(model + BATCH_TXN_LAYERS) { return -1; }
    var args: u64[8] = [0; 8];
    let base = vocab_address(unsafe_c_address<c_ptr<const void>>(args[0]));
    let prefix = vocab_span(model + VERIFY_PREFIX + (index as! u64) * 64, 8);
    let count = bonsai_bonsai_verify_conv_args(vocab_span(input, 8), prefix,
        batch_u32(model + VERIFY_ROWS_OFFSET), vocab_span(base, 8));
    if count < 0 { return -1; }
    var params: u64[8] = [0; 8];
    for argument in count as! i64 { params[argument] = base + (argument as! u64) * 8; }
    let kernel: i32 = if count == 8 { 96 } else { if count == 7 { 92 } else { 83 } };
    return fg_bonsai_verify_launch(model, kernel, 40, 1, 1, 256,
        vocab_address(unsafe_c_address<c_ptr<const void>>(params[0])));
}

export fn fg_bonsai_transaction_gdn(model: u64, input: u64) -> i32 {
    let index = batch_u32(model + BATCH_TXN_NEXT_LAYER);
    if index >= batch_u32(model + BATCH_TXN_LAYERS) { return -1; }
    let layer = batch_u32(model + VERIFY_LAYER_IDS + (index as! u64) * 4) as! u64;
    let prefix = vocab_span(model + VERIFY_PREFIX + (index as! u64) * 64, 8);
    let state = vocab_word(model + BATCH_STATES + layer * 16 + 8);
    if bonsai_bonsai_verify_gdn_rows(model, vocab_span(input, 8), prefix, state,
        batch_u32(model + VERIFY_ROWS_OFFSET)) != 0 { return -1; }
    batch_set(model + BATCH_TXN_NEXT_LAYER, index + 1);
    return 0;
}

export fn bonsai_gdn_defer_state_swap(model: u64) -> i64 {
    return if batch_u32(model + BATCH_TXN_PHASE) == 1 { 1 } else { 0 };
}

fn bonsai_commit_target(model: u64, kept: u32) -> i32 {
    if batch_u32(model + BATCH_TXN_PHASE) != 2 { return -1; }
    let position = bonsai_bonsai_verify_commit(vocab_span(model + BATCH_STATES, 128),
        vocab_span(model + VERIFY_PREFIX, 512),
        vocab_span(model + VERIFY_LAYER_IDS, batch_u32(model + BATCH_TXN_LAYERS) as! i64),
        batch_u32(model + BATCH_TXN_START), batch_u32(model + VERIFY_ROWS_OFFSET), kept);
    if position < 0 { return -1; }
    batch_set(model + BATCH_POSITION, position as! u32);
    batch_set(model + BATCH_TXN_PHASE, 0);
    return 0;
}
