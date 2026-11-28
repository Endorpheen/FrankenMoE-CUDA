type BonsaiAllocate = c_fnptr(u64, u64) -> i32;

extern fn bonsai_bonsai_workspace_plan(context: u64, batch: u64, rows: u64, flags: u64,
    bound: VocabSpan, states: VocabSpan, scratch: VocabSpan, prefix: VocabSpan, layers: VocabSpan, layout: VocabSpan) -> bool;
extern fn bonsai_bonsai_workspace_rebase(base: u64, rows: u64, flags: u64,
    states: VocabSpan, scratch: VocabSpan, prefix: VocabSpan, layout: VocabSpan);

fn bonsai_allocate(bytes: u64, label: const u8[:]) -> u64 {
    var address: u64 = 0;
    let allocate = unsafe_c_fnptr<BonsaiAllocate>(vocab_word(fg_bonsai_cuda_dispatch() + 80));
    let status = allocate(vocab_address(unsafe_c_address<c_ptr<const void>>(address)), bytes);
    return if bonsai_cuda_status(status, label) == 0 { address } else { 0 };
}

export fn bonsai_bonsai_upload_weights(model: u64, source: u64, bytes: u64) -> u64 {
    let gpu = bonsai_allocate(bytes, "resident GGUF weights");
    if gpu == 0 { return 0; }
    vocab_set(model + 5648, gpu);
    if batch_copy(fg_bonsai_cuda_dispatch(), BONSAI_CUDA_UPLOAD, gpu, source, bytes, "resident GGUF upload") != 0 { return 0; }
    return gpu;
}

export fn bonsai_bonsai_bind_weight_workspace(model: u64, bytes: u64, signs: VocabSpan) -> i32 {
    vocab_log(str("Device GGUF payload: ", bytes as! i64, " bytes (includes inactive MTP tensors)\n"));
    let flags = verify_flag(model, 14664, 1) | verify_flag(model, SESSION_MTP_ENABLED, 2) |
        verify_flag(model, 76, 4) | verify_flag(model, 56, 8) | verify_flag(model, 14668, 16);
    let rows = batch_u32(model + VERIFY_ROWS_OFFSET) as! u64;
    var values: u64[17] = [0; 17];
    let layout = vocab_span(vocab_address(unsafe_c_address<c_ptr<const void>>(values[0])), 17);
    let states = vocab_span(model + BATCH_STATES, 128);
    let scratch = vocab_span(model + BATCH_B, 24);
    let prefix = vocab_span(model + VERIFY_PREFIX, 512);
    if !bonsai_bonsai_workspace_plan(batch_u32(model + BATCH_CONTEXT) as! u64, batch_u32(model + BATCH_UBATCH) as! u64,
        rows, flags, vocab_span(model + BATCH_BOUND, 960), states, scratch, prefix, vocab_span(model + VERIFY_LAYER_IDS, 64), layout) { return -1; }
    vocab_set(model + 14656, values[1]);
    batch_set(model + BATCH_TXN_LAYERS, values[2] as! u32);
    let memory = bonsai_allocate(values[0], "state and scratch");
    if memory == 0 { return -1; }
    vocab_set(model + 5656, memory);
    if session_zero(memory, values[0], "state initialization") != 0 { return -1; }
    bonsai_bonsai_workspace_rebase(memory, rows, flags, states, scratch, prefix, layout);
    vocab_set(model + BATCH_POSITIONS, values[3]);
    vocab_set(model + 14632, values[4]);
    vocab_set(model + BATCH_TOKEN_IDS, values[5]);
    vocab_set(model + 14640, values[6]);
    vocab_set(model + BATCH_ROPE_Q, values[7]);
    vocab_set(model + BATCH_ROPE_K, values[8]);
    vocab_set(model + BATCH_ATTENTION, values[9]);
    vocab_set(model + 5696, values[10]);
    if batch_u32(model + 56) != 0 {
        vocab_set(model + 264, values[11]);
        vocab_set(model + 272, values[12]);
        vocab_set(model + VERIFY_POSITIONS, values[13]);
        if bonsai_rope_config(values[11], 24, 1) != 0 || bonsai_rope_config(values[12], 4, 1) != 0 { return -1; }
    }
    batch_set(model + 14728, values[14] as! u32);
    vocab_set(model + 14712, values[15]);
    vocab_set(model + 14720, values[16]);
    let sizes: u64[3] = [5120 * 4, 6144 * 4, 17408 * 4];
    let dispatch = fg_bonsai_cuda_dispatch();
    var source = signs.address;
    for index in 3 {
        if batch_copy(dispatch, BONSAI_CUDA_UPLOAD, vocab_word(model + BATCH_B + 160 + (index as! u64) * 8),
            source, sizes[index], "Hadamard signs") != 0 { return -1; }
        source += sizes[index];
    }
    vocab_log(str("Device workspace: ", values[0] as! i64, " bytes (Q4_0 K/V, GDN, activations)\n"));
    return 0;
}

export fn fg_bonsai_choice_open(model: u64) -> i32 {
    var tokens: u32[7760] = [0; 7760];
    for index in vocab_word(model + SESSION_EOG_COUNT) as! i64 {
        let id = vocab_word(model + SESSION_EOG + (index as! u64) * 8) as! u32;
        if id >= 248320 { return -1; }
        tokens[(id / 32) as! i64] |= (1 as! u32) << (id % 32);
    }
    let rows = if batch_u32(model + SESSION_MTP_ENABLED) != 0 { batch_u32(model + VERIFY_ROWS_OFFSET) as! u64 } else { 1 };
    let mask_bytes: u64 = 7760 * 4;
    let partial = rows * 243 * 12;
    let gpu = bonsai_allocate(mask_bytes + partial + rows * 12, "choice workspace");
    if gpu == 0 { return -1; }
    vocab_set(model + SESSION_CHOICE_MASK, gpu);
    if batch_copy(fg_bonsai_cuda_dispatch(), BONSAI_CUDA_UPLOAD, gpu,
        vocab_address(unsafe_c_address<c_ptr<const void>>(tokens[0])), mask_bytes, "choice EOG mask") != 0 { return -1; }
    vocab_set(model + 5632, gpu + mask_bytes);
    vocab_set(model + 5640, gpu + mask_bytes + partial);
    return 0;
}

fn bonsai_choice_rows(model: u64, rows: u32, ignore_eos: i32, result: u64, label: const u8[:]) -> i32 {
    let logits = vocab_word(model + BATCH_B + 152);
    let mask = if ignore_eos != 0 { vocab_word(model + 5624) } else { 0 };
    let width: u32 = 248320;
    let jobs: u32 = 243;
    let job_address = vocab_address(unsafe_c_address<c_ptr<const void>>(jobs));
    let tiles: u64[5] = [vocab_address(unsafe_c_address<c_ptr<const void>>(logits)),
        vocab_address(unsafe_c_address<c_ptr<const void>>(mask)), model + 5632,
        vocab_address(unsafe_c_address<c_ptr<const void>>(width)), job_address];
    let finish: u64[3] = [model + 5632, model + 5640, job_address];
    if fg_bonsai_verify_launch(model, 77, jobs, rows, 1, 256, draft_args(tiles)) != 0 ||
        fg_bonsai_verify_launch(model, 78, rows, 1, 1, 256, draft_args(finish)) != 0 ||
        batch_copy(fg_bonsai_cuda_dispatch(), BONSAI_CUDA_DOWNLOAD, result, vocab_word(model + 5640),
            (rows as! u64) * 12, label) != 0 { return 1; }
    return 0;
}

