const BATCH_UBATCH: u64 = 14740;
const BATCH_CONTEXT: u64 = 14732;
const BATCH_POSITION: u64 = 14736;
const BATCH_WAVE: u64 = 14748;
const BATCH_COUNT: u64 = 14744;
const BATCH_ROPE_Q: u64 = 5672;
const BATCH_ROPE_K: u64 = 5680;
const BATCH_PACKED_SOURCE: u64 = 14648;
const BATCH_Q8_INPUT: u64 = 4704;
const BATCH_FFN_Q8_INPUT: u64 = 4760;
const BATCH_FFN_LAYER: u64 = 4776;
const BATCH_DOWN_Q8_OWNER: u64 = 4768;
const BATCH_POSITIONS: u64 = 5664;
const BATCH_ATTENTION: u64 = 5688;
const BATCH_TOKEN_IDS: u64 = 14624;
const BATCH_CAPTURE_LAYER: u64 = 16324;
const BATCH_TXN_PHASE: u64 = 312;
const BATCH_TXN_START: u64 = 316;
const BATCH_TXN_NEXT_LAYER: u64 = 324;
const BATCH_TXN_LAYERS: u64 = 320;
const BATCH_BOUND: u64 = 5704;
const BATCH_STATES: u64 = 13384;
const BATCH_GLOBALS: u64 = 14408;
const BATCH_B: u64 = 14432;
const BATCH_CAPTURE_PHASE: u64 = 16320;
const BATCH_CAPTURE_ENABLED: u64 = 16332;
const BATCH_IN_DECODE: u64 = 4716;

extern fn fg_bonsai_batch_rope(q: u64, k: u64, count: u32) -> i32;
extern fn fg_bonsai_batch_upload(dst: u64, src: u64, size: u64, role: u32) -> i32;
extern fn fg_bonsai_batch_verify(model: u64, count: u32) -> i32;
extern fn fg_bonsai_batch_init_verify(model: u64) -> i32;
extern fn bonsai_bonsai_graph(model: u64, weights: VocabSpan, states: VocabSpan, globals: VocabSpan, buffers: VocabSpan,
    token: i64, position: i64, output: i64) -> i64;
extern fn bonsai_mtp_target_graph(model: u64, weights: VocabSpan, states: VocabSpan, globals: VocabSpan, buffers: VocabSpan,
    token: i64, position: i64, output: i64) -> i64;
extern fn fg_bonsai_batch_graph_error(model: u64, status: i64);
extern fn fg_bonsai_batch_download(output: u64, source: u64) -> i32;

fn batch_u32(address: u64) -> u32 {
    var value: u32 = 0;
    vocab_memcpy(unsafe_c_address<c_ptr<void>>(value), unsafe_c_ptr<c_ptr<const void>>(address), 4);
    return value;
}

fn batch_set(address: u64, value: u32) {
    vocab_memcpy(unsafe_c_ptr<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(value), 4);
}

export fn evaluate_batch_output(model: u64, tokens: u64, count: u32, logits: u64, device_only: i32) -> i32 {
    let position = batch_u32(model + BATCH_POSITION);
    if count == 0 || count > batch_u32(model + BATCH_UBATCH) ||
        count > batch_u32(model + BATCH_CONTEXT) - position || (logits != 0 && count != 1) { return -1; }
    let pos = position as! i32;
    let keys = (position + count) as! i64;
    let tiles = (keys + 255) / 256;
    let per_task = tiles * 258 * 4;
    let tasks = 24 * (count as! i64);
    let maximum_wave = 33554432 / per_task;
    let wave = if maximum_wave > tasks { tasks } else { maximum_wave };
    batch_set(model + BATCH_WAVE, wave as! u32);
    if batch_u32(model + BATCH_COUNT) != count && fg_bonsai_batch_rope(
        vocab_word(model + BATCH_ROPE_Q), vocab_word(model + BATCH_ROPE_K), count) != 0 { return -1; }
    batch_set(model + BATCH_COUNT, count);
    vocab_set(model + BATCH_PACKED_SOURCE, 0);
    vocab_set(model + BATCH_Q8_INPUT, 0);
    vocab_set(model + BATCH_FFN_Q8_INPUT, 0);
    batch_set(model + BATCH_FFN_LAYER, 0);
    vocab_set(model + BATCH_DOWN_Q8_OWNER, 0);

    var positions: i32[4096];
    let n = count as! i64;
    for index in n {
        if batch_u32(tokens + (index as! u64) * 4) >= 248320 { return -1; }
        let row = pos + (index as! i32);
        positions[index] = row;
        positions[n + index] = row;
        positions[2 * n + index] = row;
        positions[3 * n + index] = 0;
    }
    var config: i64[22] = [256, 256, n, 24, 4, 4, keys, 24576, 1024, 576, 144, 576, 144,
        0, 1024, 24576, 256, tiles, tasks, wave, wave * per_task, (position as! i64) + 1];
    if fg_bonsai_batch_upload(vocab_word(model + BATCH_POSITIONS),
        vocab_address(unsafe_c_address<c_ptr<const void>>(positions[0])), 16 * (count as! u64), 0) != 0 ||
        fg_bonsai_batch_upload(vocab_word(model + BATCH_ATTENTION),
        vocab_address(unsafe_c_address<c_ptr<const void>>(config[0])), 176, 1) != 0 ||
        (count > 1 && fg_bonsai_batch_upload(vocab_word(model + BATCH_TOKEN_IDS), tokens, 4 * (count as! u64), 2) != 0) { return -1; }

    batch_set(model + BATCH_CAPTURE_LAYER, 0);
    let verify = fg_bonsai_batch_verify(model, count);
    if batch_u32(model + BATCH_TXN_PHASE) != 0 ||
        (verify != 0 && (logits != 0 || device_only == 0 || fg_bonsai_batch_init_verify(model) != 0)) { return -1; }
    if verify != 0 {
        batch_set(model + BATCH_TXN_PHASE, 1);
        batch_set(model + BATCH_TXN_START, position);
        batch_set(model + BATCH_TXN_NEXT_LAYER, 0);
    }
    let weights = vocab_span(model + BATCH_BOUND, 64 * 15);
    let states = vocab_span(model + BATCH_STATES, 64 * 2);
    let globals = vocab_span(model + BATCH_GLOBALS, 3);
    let buffers = vocab_span(model + BATCH_B, 24);
    let token = batch_u32(tokens) as! i64;
    let output: i64 = if logits != 0 || device_only != 0 { 1 } else { 0 };
    let result = if verify != 0 {
        bonsai_mtp_target_graph(model, weights, states, globals, buffers, token, pos as! i64, output)
    } else {
        bonsai_bonsai_graph(model, weights, states, globals, buffers, token, pos as! i64, output)
    };
    if result != 0 ||
        (verify != 0 && batch_u32(model + BATCH_TXN_NEXT_LAYER) != batch_u32(model + BATCH_TXN_LAYERS)) ||
        batch_u32(model + BATCH_CAPTURE_PHASE) != 0 ||
        (batch_u32(model + BATCH_CAPTURE_ENABLED) != 0 && batch_u32(model + BATCH_IN_DECODE) != 0 &&
         count == 1 && batch_u32(model + BATCH_CAPTURE_LAYER) != 64) {
        fg_bonsai_batch_graph_error(model, result);
        return -1;
    }
    if verify != 0 { batch_set(model + BATCH_TXN_PHASE, 2); }
    else { batch_set(model + BATCH_POSITION, position + count); }
    if logits != 0 { return fg_bonsai_batch_download(logits, vocab_word(model + BATCH_B + 152)); }
    return 0;
}
