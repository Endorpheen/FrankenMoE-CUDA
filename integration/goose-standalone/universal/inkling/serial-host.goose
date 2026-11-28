extern fn ggml_row_size(kind: i32, width: i64) -> u64;
extern fn i2_embed_row(row: I2DraftSpan, gains: I2DraftSpan, epsilon: f32, output: I2DraftSpan) -> i64;
extern "abort" fn i2_embedding_abort();

struct InklingEmbedding { base: u64, row_bytes: u64, gains: u64 }

fn i2_embedding(decoder: u64) -> InklingEmbedding {
    let id = i2d_read(decoder + I2_OPEN_GLOBALS);
    if id >= 704 { i2_embedding_abort(); }
    let record = decoder + I2_OPEN_TENSOR + id * 96;
    let base = i2d_read(decoder) + i2d_read(record + 80);
    let gains = fg_i2_host_coefficients(decoder, i2d_read(decoder + I2_OPEN_GLOBALS + 8) as! i64, 2304);
    return InklingEmbedding {base: base, row_bytes: ggml_row_size(8, 2304), gains: gains};
}

fn i2_embed_token(embedding: InklingEmbedding, token: i32, output: I2DraftSpan) -> bool {
    let row = embedding.base + embedding.row_bytes * (token as! u64);
    return i2_embed_row(i2d_span(row, embedding.row_bytes as! i64), i2d_span(embedding.gains, 2304), 0.000001, output) == 2304;
}

export fn fg_i2_embed_prefill(decoder: u64, ids: u64, count: i32, output: u64, stride: u64) -> bool {
    let embedding = i2_embedding(decoder);
    for index in count as! i64 {
        let slot = index as! u64;
        if !i2_embed_token(embedding, i2d_int(ids + slot * 4), i2d_span(output + slot * stride, 2304)) { return false; }
    }
    return true;
}
extern fn fg_i2_serial_finish(decoder: u64, position: i32, current: u64, history: u64, count: i32, logits: bool);
extern fn i2_walk_serial_token(decoder: u64, visits: I2DraftSpan, position: i32,
    current: I2DraftSpan, history: I2DraftSpan, prelude: I2DraftSpan, bank_output: I2DraftSpan,
    bank: I2DraftSpan, messages: I2DraftSpan, trace: bool) -> i32;

fn i2_serial_span(values: f32[:]) -> I2DraftSpan {
    return i2d_span(i2d_address(unsafe_c_slice_data<c_ptr<const void>>(values)), values.len);
}

export fn fg_i2_backend_token(decoder: u64, token: i32, position: i32, logits: bool) {
    var current: f32[2304];
    var prelude: f32[2304];
    var bank_output: f32[2304];
    var history: f32[27648];
    var bank: f32[27648];
    var messages: f32[9216];
    let active = i2_serial_span(current);
    let blocks = i2_serial_span(history);
    if !i2_embed_token(i2_embedding(decoder), token, active) { i2_embedding_abort(); }
    let trace = position == 4097 && fg_cuda_host_getenv(i2d_address(unsafe_c_slice_data<c_ptr<const void>>("I2_DRAFT_PARITY\0"))) != 0;
    let count = i2_walk_serial_token(decoder, i2d_span(decoder + 81456, 192), position, active, blocks,
        i2_serial_span(prelude), i2_serial_span(bank_output), i2_serial_span(bank), i2_serial_span(messages), trace);
    i2d_log(str("Pure Goose visited 48 layers for token ", token as! i64, " at position ", position as! i64, "\n"));
    fg_i2_serial_finish(decoder, position, active.address, blocks.address, count, logits);
}
