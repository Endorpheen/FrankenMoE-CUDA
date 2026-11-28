const DRAFT_HIDDEN: u64 = 20480;
const DRAFT_WEIGHTS: u64 = 128;
const DRAFT_KV: u64 = 248;
const DRAFT_TENSORS: u64 = 96;
const DRAFT_BATCH: u64 = 88;

type DraftCopy = c_fnptr(u64, u64, u64, u64) -> i32;

extern fn bonsai_mtp_head(model: u64, weights: VocabSpan, kv: VocabSpan, draft: VocabSpan,
    globals: VocabSpan, scratch: VocabSpan, token: i64, pos: i64) -> i64;
extern fn bonsai_mtp_kv(model: u64, weights: VocabSpan, kv: VocabSpan, draft: VocabSpan,
    globals: VocabSpan, scratch: VocabSpan, token: i64, pos: i64) -> i64;
extern fn bonsai_mtp_kv_batch(model: u64, weights: VocabSpan, kv: VocabSpan, draft: VocabSpan,
    globals: VocabSpan, scratch: VocabSpan, token: i64, pos: i64) -> i64;

export fn fg_bonsai_draft_open(model: u64) -> i32 {
    if batch_u32(model + 76) == 0 || batch_u32(model + 56) == 0 || batch_u32(model + 60) == 0 ||
        batch_u32(model + 4712) != 1 || batch_u32(model + BATCH_UBATCH) < batch_u32(model + VERIFY_ROWS_OFFSET) { return -1; }
    let plane = (batch_u32(model + BATCH_CONTEXT) as! u64) * 576;
    batch_set(model + DRAFT_BATCH, 1);
    let bytes = 2 * plane + 2 * DRAFT_HIDDEN + 216;
    let memory = bonsai_allocate(bytes, "draft KV and conditioning");
    if memory == 0 { return -1; }
    let carry = memory + 2 * plane;
    vocab_set(model + DRAFT_KV, memory);
    vocab_set(model + DRAFT_KV + 8, memory + plane);
    vocab_set(model + SESSION_DRAFT_CARRY, carry);
    vocab_set(model + 296, carry + DRAFT_HIDDEN);
    vocab_set(model + 304, carry + 2 * DRAFT_HIDDEN);
    vocab_log(str("draft_device_bytes=", bytes as! i64, "\n"));
    return session_zero(carry, DRAFT_HIDDEN, "draft zero seed");
}

fn draft_copy(out: u64, input: u64, bytes: u64, label: const u8[:]) -> i32 {
    let status = unsafe_c_fnptr<DraftCopy>(vocab_word(fg_bonsai_cuda_dispatch() + 104))(out, input, bytes, 0);
    return bonsai_cuda_status(status, label);
}

fn bonsai_save_conditioning(out: u64, input: u64) -> i32 {
    return draft_copy(out, input, DRAFT_HIDDEN, "draft conditioning");
}

fn bonsai_is_eog(model: u64, token: u32) -> i32 {
    for index in vocab_word(model + SESSION_EOG_COUNT) as! i64 {
        if vocab_word(model + SESSION_EOG + (index as! u64) * 8) == (token as! u64) { return 1; }
    }
    return 0;
}

fn draft_row(model: u64, position: u32) -> i32 {
    let keys = (position as! i64) + 1;
    let tiles = (keys + 255) / 256;
    var positions: i32[4] = [position as! i32, position as! i32, position as! i32, 0];
    var config: i64[22] = [256, 256, 1, 24, 4, 4, keys, 24576, 1024, 576, 144, 576, 144,
        0, 1024, 24576, 256, tiles, 24, 24, 24 * tiles * 258 * 4, keys];
    batch_set(model + BATCH_COUNT, 1);
    batch_set(model + BATCH_POSITION, position);
    batch_set(model + BATCH_WAVE, 24);
    let dispatch = fg_bonsai_cuda_dispatch();
    if batch_copy(dispatch, BONSAI_CUDA_UPLOAD, vocab_word(model + BATCH_POSITIONS),
        vocab_address(unsafe_c_address<c_ptr<const void>>(positions[0])), 16, "draft positions") != 0 ||
        batch_copy(dispatch, BONSAI_CUDA_UPLOAD, vocab_word(model + BATCH_ATTENTION),
        vocab_address(unsafe_c_address<c_ptr<const void>>(config[0])), 176, "draft attention") != 0 { return 1; }
    return 0;
}

fn bonsai_draft_forward(model: u64, token: u32, position: u32, hidden: u64, logits: u64, want: i32) -> i32 {
    if batch_u32(model + BATCH_TXN_PHASE) != 0 || position >= batch_u32(model + BATCH_CONTEXT) || token >= 248320 { return -1; }
    let old_pos = batch_u32(model + BATCH_POSITION);
    let old_count = batch_u32(model + BATCH_COUNT);
    let old_wave = batch_u32(model + BATCH_WAVE);
    let old_decode = batch_u32(model + BATCH_IN_DECODE);
    let old_capture = batch_u32(model + BATCH_CAPTURE_ENABLED);
    let old_rope_q = vocab_word(model + BATCH_ROPE_Q);
    let old_rope_k = vocab_word(model + BATCH_ROPE_K);
    batch_set(model + BATCH_CAPTURE_ENABLED, 0);
    vocab_set(model + BATCH_ROPE_Q, vocab_word(model + 264));
    vocab_set(model + BATCH_ROPE_K, vocab_word(model + 272));
    // Only the draft's PQ2 vocabulary projection uses this flag.
    batch_set(model + BATCH_IN_DECODE, if want != 0 && batch_u32(model + 84) != 0 { 1 } else { 0 });
    var scratch: u64[24] = [0; 24];
    let data = vocab_address(unsafe_c_address<c_ptr<const void>>(scratch[0]));
    vocab_memcpy(unsafe_c_ptr<c_ptr<void>>(data), unsafe_c_ptr<c_ptr<const void>>(model + BATCH_B), 192);
    scratch[0] = hidden;
    var status = draft_row(model, position);
    if status == 0 {
        let weights = vocab_span(model + DRAFT_WEIGHTS, 15);
        let kv = vocab_span(model + DRAFT_KV, 2);
        let draft = vocab_span(model + DRAFT_TENSORS, 4);
        let globals = vocab_span(model + BATCH_GLOBALS, 3);
        let buffers = vocab_span(data, 24);
        let result = if want != 0 {
            bonsai_mtp_head(model, weights, kv, draft, globals, buffers, token as! i64, position as! i64)
        } else {
            bonsai_mtp_kv(model, weights, kv, draft, globals, buffers, token as! i64, position as! i64)
        };
        status = result as! i32;
    }
    if status == 0 && logits != 0 {
        status = batch_copy(fg_bonsai_cuda_dispatch(), BONSAI_CUDA_DOWNLOAD,
            logits, vocab_word(model + BATCH_B + 152), 248320 * 4, "draft logits");
    }
    vocab_set(model + BATCH_ROPE_Q, old_rope_q);
    vocab_set(model + BATCH_ROPE_K, old_rope_k);
    batch_set(model + BATCH_IN_DECODE, old_decode);
    batch_set(model + BATCH_CAPTURE_ENABLED, old_capture);
    batch_set(model + BATCH_POSITION, old_pos);
    batch_set(model + BATCH_COUNT, old_count);
    batch_set(model + BATCH_WAVE, old_wave);
    return status;
}

fn bonsai_draft_catchup(model: u64, ids: u64, rows: u32, already_written: u32) -> i32 {
    let start = batch_u32(model + BATCH_POSITION) - rows;
    let output = vocab_word(model + BATCH_B);
    let carry = vocab_word(model + SESSION_DRAFT_CARRY);
    var row = already_written;
    while row < rows {
        let state = if row == 0 { carry } else { output + ((row - 1) as! u64) * DRAFT_HIDDEN };
        if bonsai_draft_forward(model, batch_u32(ids + (row as! u64) * 4), start + row, state, 0, 0) != 0 { return -1; }
        row += 1;
    }
    return bonsai_save_conditioning(carry, output + ((rows - 1) as! u64) * DRAFT_HIDDEN);
}

fn draft_prefill_block(model: u64, tokens: u64, start: u32, count: u32, previous: u64) -> i32 {
    if count < 2 || count > 128 { return -1; }
    let states = vocab_word(model + BATCH_B + 104);
    let target_count = batch_u32(model + BATCH_COUNT);
    let offset = start - (batch_u32(model + BATCH_POSITION) - target_count);
    if bonsai_save_conditioning(states, previous) != 0 ||
        draft_copy(states + DRAFT_HIDDEN, vocab_word(model + BATCH_B) + (offset as! u64) * DRAFT_HIDDEN,
            ((count - 1) as! u64) * DRAFT_HIDDEN, "shifted prompt hidden rows") != 0 { return -1; }
    var positions: i32[512];
    let n = count as! i64;
    for row in n {
        let position = (start as! i32) + (row as! i32);
        positions[row] = position;
        positions[n + row] = position;
        positions[2 * n + row] = position;
        positions[3 * n + row] = 0;
    }
    let target_rope = vocab_word(model + BATCH_ROPE_K);
    let rope = vocab_word(model + 304);
    vocab_set(model + BATCH_ROPE_K, rope);
    batch_set(model + BATCH_COUNT, count);
    let dispatch = fg_bonsai_cuda_dispatch();
    let failed = bonsai_rope_config(rope, 4, n) != 0 ||
        batch_copy(dispatch, BONSAI_CUDA_UPLOAD, vocab_word(model + BATCH_POSITIONS),
            vocab_address(unsafe_c_address<c_ptr<const void>>(positions[0])), (count as! u64) * 16, "draft positions") != 0 ||
        batch_copy(dispatch, BONSAI_CUDA_UPLOAD, vocab_word(model + BATCH_TOKEN_IDS), tokens,
            (count as! u64) * 4, "draft token ids") != 0 ||
        bonsai_mtp_kv_batch(model, vocab_span(model + DRAFT_WEIGHTS, 15), vocab_span(model + DRAFT_KV, 2),
            vocab_span(model + DRAFT_TENSORS, 4), vocab_span(model + BATCH_GLOBALS, 3), vocab_span(model + BATCH_B, 24),
            batch_u32(tokens) as! i64, start as! i64) != 0;
    batch_set(model + BATCH_COUNT, target_count);
    vocab_set(model + BATCH_ROPE_K, target_rope);
    return if failed { 1 } else { 0 };
}

fn fg_bonsai_session_draft(model: u64, ids: u64, rows: u32) -> i32 {
    if batch_u32(model + DRAFT_BATCH) == 0 || rows < 2 { return bonsai_draft_catchup(model, ids, rows, 0); }
    let start = batch_u32(model + BATCH_POSITION) - rows;
    let output = vocab_word(model + BATCH_B);
    let carry = vocab_word(model + SESSION_DRAFT_CARRY);
    var at: u32 = 0;
    while at < rows {
        let left = rows - at;
        let count = if left > 128 { 128 } else { left };
        let input = ids + (at as! u64) * 4;
        let previous = if at == 0 { carry } else { output + ((at - 1) as! u64) * DRAFT_HIDDEN };
        let status = if count == 1 { bonsai_draft_forward(model, batch_u32(input), start + at, previous, 0, 0) }
            else { draft_prefill_block(model, input, start + at, count, previous) };
        if status != 0 { return -1; }
        at += count;
    }
    return bonsai_save_conditioning(carry, output + ((rows - 1) as! u64) * DRAFT_HIDDEN);
}

fn draft_args(args: const u64[:]) -> u64 {
    return vocab_address(unsafe_c_slice_data<c_ptr<const void>>(args));
}

export fn bonsai_mtp_eh(model: u64, weights: i64, input: i64, output: i64) -> i64 {
    let args: u64[3] = [vocab_address(unsafe_c_address<c_ptr<const void>>(weights)),
        vocab_address(unsafe_c_address<c_ptr<const void>>(input)), vocab_address(unsafe_c_address<c_ptr<const void>>(output))];
    return fg_bonsai_verify_launch(model, 79, 1280, 1, 1, 128, draft_args(args)) as! i64;
}

export fn bonsai_mtp_concat_launch(model: u64, embeddings: i64, hidden: i64, joined: i64) -> i64 {
    let args: u64[3] = [vocab_address(unsafe_c_address<c_ptr<const void>>(embeddings)),
        vocab_address(unsafe_c_address<c_ptr<const void>>(hidden)), vocab_address(unsafe_c_address<c_ptr<const void>>(joined))];
    return fg_bonsai_verify_launch(model, 84, 20 * batch_u32(model + BATCH_COUNT), 1, 1, 256, draft_args(args)) as! i64;
}

fn draft_matrix(model: u64, weights: i64, input: i64, output: i64, width: i64, rows: i64, batch: bool) -> i64 {
    let tokens = batch_u32(model + BATCH_COUNT);
    if (!batch && tokens != 1) || width <= 0 || width % 32 != 0 || rows <= 0 || rows % 4 != 0 { return -1; }
    let blocks = (width as! u32) / 32;
    let nrows = rows as! u32;
    let args: u64[6] = [vocab_address(unsafe_c_address<c_ptr<const void>>(weights)),
        vocab_address(unsafe_c_address<c_ptr<const void>>(input)), vocab_address(unsafe_c_address<c_ptr<const void>>(output)),
        vocab_address(unsafe_c_address<c_ptr<const void>>(blocks)), vocab_address(unsafe_c_address<c_ptr<const void>>(nrows)),
        vocab_address(unsafe_c_address<c_ptr<const void>>(tokens))];
    return fg_bonsai_verify_launch(model, if batch { 85 } else { 80 }, nrows / 4,
        if batch { (tokens + 3) / 4 } else { 1 }, 1, 128, draft_args(args)) as! i64;
}

export fn bonsai_mtp_mat(model: u64, weights: i64, input: i64, output: i64, width: i64, rows: i64) -> i64 {
    return draft_matrix(model, weights, input, output, width, rows, false);
}

export fn bonsai_mtp_mat_batch(model: u64, weights: i64, input: i64, output: i64, width: i64, rows: i64) -> i64 {
    return draft_matrix(model, weights, input, output, width, rows, true);
}
