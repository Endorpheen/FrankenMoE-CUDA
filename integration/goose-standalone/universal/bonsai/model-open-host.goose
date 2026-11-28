type BonsaiMemInfo = c_fnptr(u64, u64) -> i32;

extern fn fg_bonsai_embedded_images(model: u64, image: u64, frozen: u64, mtp_image: u64) -> i32;
extern fn bonsai_mtp_goose_init(argc: i32, argv: u64);

extern fn bonsai_bonsai_load_weights_plan(model: u64, file: VocabSpan, mapping: u64, bytes: u64,
    mtp: bool, globals: VocabSpan, bound: VocabSpan, draft: VocabSpan, draft_layer: VocabSpan,
    eog: VocabSpan, epsilon: VocabSpan, result: VocabSpan) -> i32;

export fn bonsai_bonsai_log_gguf_parse(status: i64) {
    vocab_log(str("GGUF parse: ", status, "\n"));
}

fn bonsai_load_weights(model: u64, mapping: u64, bytes: u64) -> i32 {
    let limit: i64 = if bytes > 67108864 { 67108864 } else { bytes as! i64 };
    return bonsai_bonsai_load_weights_plan(model, vocab_span(mapping, limit), mapping, bytes,
        batch_u32(model + SESSION_MTP_ENABLED) != 0, vocab_span(model + BATCH_GLOBALS, 3), vocab_span(model + BATCH_BOUND, 960),
        vocab_span(model + DRAFT_TENSORS, 4), vocab_span(model + DRAFT_WEIGHTS, 15), vocab_span(model + SESSION_EOG, 64),
        vocab_span(model + 16336, 1), vocab_span(model + SESSION_EOG_COUNT, 1));
}

fn bonsai_open_initialize(model: u64, path: u64, image: u64, frozen: u64, mtp_image: u64) -> i32 {
    if fg_bonsai_profile(model, mtp_image) != 0 ||
        fg_bonsai_embedded_images(model, image, frozen, mtp_image) != 0 ||
        bonsai_map_weights(model, path) != 0 { return -1; }
    let choice = bonsai_profile_env("BONSAI_HTTP_DEVICE_CHOICE\0");
    if bonsai_profile_is(choice, "1") && fg_bonsai_choice_open(model) != 0 { return -1; }
    if batch_u32(model + SESSION_MTP_ENABLED) != 0 {
        bonsai_mtp_goose_init(0, 0);
        if vocab_word(model + SESSION_CHOICE_MASK) == 0 || fg_bonsai_draft_open(model) != 0 { return -1; }
    }
    fg_bonsai_ffn_graph_open(model);
    var available: u64 = 0;
    var total: u64 = 0;
    let info = unsafe_c_fnptr<BonsaiMemInfo>(vocab_word(fg_bonsai_cuda_dispatch() + 88))(
        vocab_address(unsafe_c_address<c_ptr<const void>>(available)),
        vocab_address(unsafe_c_address<c_ptr<const void>>(total)));
    if bonsai_cuda_status(info, "free VRAM") == 0 {
        vocab_log(str("Device free: ", available as! i64, " / ", total as! i64, " bytes\n"));
    }
    return 0;
}

fn bonsai_model_open(path: u64, image: u64, frozen: u64, context: u32, batch: u32) -> u64 {
    if context == 0 || context > 262144 || !bonsai_cuda_ready() { return 0; }
    let allocation = vocab_calloc(1, 16344);
    var model: u64 = 0;
    vocab_memcpy(unsafe_c_address<c_ptr<void>>(model), unsafe_c_address<c_ptr<const void>>(allocation), 8);
    if model == 0 { return 0; }
    batch_set(model + BATCH_CONTEXT, context);
    batch_set(model + BATCH_UBATCH, batch);
    let mtp_image = bonsai_profile_env("BONSAI_HTTP_MTP_IMAGE\0");
    if bonsai_open_initialize(model, path, image, frozen, mtp_image) == 0 { return model; }
    let cuda = vocab_word(model);
    if cuda != 0 { session_context(24, cuda); }
    vocab_free(allocation);
    return 0;
}
