const SESSION_NEEDS_RESET: u64 = 8;
const SESSION_MTP_ENABLED: u64 = 16;
const SESSION_MTP_REQUEST: u64 = 20;
const SESSION_MTP_NEXT: u64 = 44;
const SESSION_MTP_END: u64 = 48;
const SESSION_DRAFT_CARRY: u64 = 288;
const SESSION_CHOICE_MASK: u64 = 5624;
const SESSION_EOG: u64 = 14768;
const SESSION_EOG_COUNT: u64 = 15280;

type BonsaiContext = c_fnptr(u64) -> i32;
type BonsaiMemset = c_fnptr(u64, u8, u64) -> i32;

extern fn fg_bonsai_model_open(path: u64, image: u64, frozen: u64, context: u32, batch: u32) -> u64;
extern fn bonsai_bonsai_capture_abort(capture: u64);

fn session_context(operation: u64, argument: u64) -> i32 {
    return unsafe_c_fnptr<BonsaiContext>(vocab_word(fg_bonsai_cuda_dispatch() + operation))(argument);
}

fn session_bind(model: u64, label: const u8[:]) -> i32 {
    return bonsai_cuda_status(session_context(32, vocab_word(model)), label);
}

fn session_pop() -> i32 {
    var current: u64 = 0;
    return session_context(40, vocab_address(unsafe_c_address<c_ptr<const void>>(current)));
}

fn session_close_capture(model: u64) {
    let capture = model + 15288;
    let stream = vocab_word(capture);
    if stream == 0 || session_context(32, vocab_word(model)) != 0 { return; }
    bonsai_bonsai_capture_abort(capture);
    let destroy = unsafe_c_fnptr<BonsaiContext>(bonsai_ffn_driver[8]);
    for layer in 64 {
        let graph = vocab_word(capture + 8 + (layer as! u64) * 8);
        if graph != 0 { destroy(graph); }
    }
    unsafe_c_fnptr<BonsaiContext>(bonsai_ffn_driver[1])(stream);
    session_pop();
}

export fn bonsai_session_close(session: u64) {
    if session == 0 { return; }
    let model = vocab_word(session);
    session_close_capture(model);
    bonsai_cuda_status(session_context(24, vocab_word(model)), "destroy Bonsai context");
    vocab_free(unsafe_c_ptr<c_ptr<void>>(model));
    vocab_free(unsafe_c_ptr<c_ptr<void>>(session));
}

export fn bonsai_session_open(path: u64, image: u64, frozen: u64, context: u32, batch: u32) -> u64 {
    if path == 0 || batch == 0 || batch > 1024 { return 0; }
    let allocation = vocab_calloc(1, 16);
    var session: u64 = 0;
    vocab_memcpy(unsafe_c_address<c_ptr<void>>(session), unsafe_c_address<c_ptr<const void>>(allocation), 8);
    if session == 0 { return 0; }
    bonsai_goose_init(0, 0);
    let model = fg_bonsai_model_open(path, image, frozen, context, batch);
    if model == 0 {
        vocab_free(allocation);
        return 0;
    }
    vocab_set(session, model);
    if bonsai_cuda_status(session_pop(), "detach Bonsai context") != 0 {
        bonsai_session_close(session);
        return 0;
    }
    return session;
}

export fn fg_bonsai_session_finish(session: u64, status: i32) -> i32 {
    let restored = session_pop();
    let result = if bonsai_cuda_status(restored, "restore caller context") != 0 { -1 } else { status };
    batch_set(session + SESSION_NEEDS_RESET, if result != 0 { 1 } else { 0 });
    return result;
}

fn session_zero(destination: u64, size: u64, label: const u8[:]) -> i32 {
    let status = unsafe_c_fnptr<BonsaiMemset>(vocab_word(fg_bonsai_cuda_dispatch() + 120))(destination, 0, size);
    return bonsai_cuda_status(status, label);
}

export fn bonsai_session_reset(session: u64) -> i32 {
    if session == 0 { return -1; }
    let model = vocab_word(session);
    if session_bind(model, "bind Bonsai context") != 0 { return -1; }
    var status: i32 = 0;
    for layer in 64 {
        if status != 0 { break; }
        let offset = layer as! u64;
        if vocab_word(model + BATCH_BOUND + offset * 120) == 0 { continue; }
        let state = model + BATCH_STATES + offset * 16;
        status = session_zero(vocab_word(state), 122880, "reset convolution");
        if status == 0 { status = session_zero(vocab_word(state + 8), 3145728, "reset GDN"); }
    }
    if status == 0 { status = session_zero(vocab_word(model + BATCH_B + 184), 3145728, "reset GDN scratch"); }
    if status == 0 && batch_u32(model + SESSION_MTP_ENABLED) != 0 {
        status = session_zero(vocab_word(model + SESSION_DRAFT_CARRY), 20480, "reset draft seed");
    }
    if status == 0 {
        batch_set(model + SESSION_MTP_NEXT, 0);
        batch_set(model + SESSION_MTP_END, 0);
        batch_set(model + SESSION_MTP_REQUEST, 0);
        batch_set(model + BATCH_TXN_PHASE, 0);
        // Attention writes new rows before reading only the active prefix.
        batch_set(model + BATCH_IN_DECODE, 0);
        batch_set(model + BATCH_POSITION, 0);
        batch_set(model + BATCH_COUNT, 0);
        vocab_set(model + BATCH_PACKED_SOURCE, 0);
    }
    status = fg_bonsai_session_finish(session, status);
    if status == 0 && batch_u32(model + BATCH_POSITION) == 0 { vocab_log("[bench-cold] reset position=0\n"); }
    return status;
}

export fn bonsai_session_step(session: u64, tokens: u64, count: u32, logits: u64) -> i32 {
    if session == 0 || batch_u32(session + SESSION_NEEDS_RESET) != 0 || tokens == 0 || count == 0 { return -1; }
    let model = vocab_word(session);
    if count > batch_u32(model + BATCH_CONTEXT) - batch_u32(model + BATCH_POSITION) { return -1; }
    for index in count as! i64 {
        if batch_u32(tokens + (index as! u64) * 4) >= 248320 { return -1; }
    }
    if session_bind(model, "bind Bonsai context") != 0 { return -1; }
    var status: i32 = 0;
    var at: u32 = 0;
    while at < count && status == 0 {
        let remaining = count - at;
        let available = remaining - (if logits != 0 && remaining > 1 { 1 } else { 0 });
        let limit = batch_u32(model + BATCH_UBATCH);
        let batch = if available < limit { available } else { limit };
        let input = tokens + (at as! u64) * 4;
        status = evaluate_batch_output(model, input, batch, if remaining == 1 { logits } else { 0 }, 0);
        if status == 0 && batch_u32(model + SESSION_MTP_REQUEST) != 0 && batch_u32(model + BATCH_IN_DECODE) == 0 {
            status = fg_bonsai_session_draft(model, input, batch);
        }
        at += batch;
    }
    status = fg_bonsai_session_finish(session, status);
    if status == 0 && logits != 0 && batch_u32(model + BATCH_POSITION) == 32768 {
        vocab_log("[bench-cold] prefill position=32768\n");
    }
    return status;
}

export fn bonsai_session_step_phase(session: u64, ids: u64, count: u32, logits: u64, decode: i32) -> i32 {
    let model = vocab_word(session);
    batch_set(model + BATCH_IN_DECODE, decode as! u32);
    let status = bonsai_session_step(session, ids, count, logits);
    batch_set(model + BATCH_IN_DECODE, 0);
    return status;
}

export fn bonsai_session_choice_available(session: u64, eog: u64, count: i64) -> i32 {
    if session == 0 || eog == 0 { return 0; }
    let model = vocab_word(session);
    if vocab_word(model + SESSION_CHOICE_MASK) == 0 || vocab_word(model + SESSION_EOG_COUNT) as! i64 != count { return 0; }
    for index in count {
        let offset = (index as! u64) * 8;
        if vocab_word(eog + offset) != vocab_word(model + SESSION_EOG + offset) { return 0; }
    }
    return 1;
}

export fn bonsai_session_choose_next(session: u64, token: u32, ignore_eos: i32) -> i64 {
    if session == 0 || batch_u32(session + SESSION_NEEDS_RESET) != 0 || token >= 248320 { return -1; }
    let model = vocab_word(session);
    if vocab_word(model + SESSION_CHOICE_MASK) == 0 ||
        batch_u32(model + BATCH_POSITION) >= batch_u32(model + BATCH_CONTEXT) ||
        session_bind(model, "bind Bonsai choice context") != 0 { return -1; }
    batch_set(model + BATCH_IN_DECODE, 1);
    var choice: u32[3] = [0; 3];
    var status = evaluate_batch_output(model,
        vocab_address(unsafe_c_address<c_ptr<const void>>(token)), 1, 0, 1);
    if status == 0 {
        status = bonsai_choice_rows(model, 1, ignore_eos, vocab_address(unsafe_c_address<c_ptr<const void>>(choice[0])), "token choice");
    }
    batch_set(model + BATCH_IN_DECODE, 0);
    if fg_bonsai_session_finish(session, if status != 0 { 1 } else { 0 }) != 0 { return -1; }
    return if choice[2] != 0 || choice[0] >= 248320 { -2 } else { choice[0] as! i64 };
}

export fn bonsai_session_read_choice_logits(session: u64, logits: u64) -> i32 {
    if session == 0 || logits == 0 || batch_u32(session + SESSION_NEEDS_RESET) != 0 { return -1; }
    let model = vocab_word(session);
    if session_bind(model, "bind Bonsai fallback context") != 0 { return -1; }
    return fg_bonsai_session_finish(session, batch_copy(fg_bonsai_cuda_dispatch(), BONSAI_CUDA_DOWNLOAD,
        logits, vocab_word(model + BATCH_B + 152), 248320 * 4, "fallback logits"));
}
