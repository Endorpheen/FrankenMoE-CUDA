extern fn ggml_row_size(kind: i32, width: i64) -> u64;
extern fn i2_embed_row(row: I2DraftSpan, gains: I2DraftSpan, epsilon: f32, output: I2DraftSpan) -> i64;
extern "abort" fn i2_embedding_abort();

struct InklingEmbedding { base: u64, row_bytes: u64, gains: u64 }

fn i2_tensor_record(decoder: u64, id: u64) -> u64 {
    if id >= 704 { i2_embedding_abort(); }
    return decoder + I2_OPEN_TENSOR + id * 96;
}

fn fg_i2_host_coefficients(decoder: u64, id: i64, count: i64) -> u64 {
    let record = i2_tensor_record(decoder, id as! u64);
    let elements = i2d_read(record + 32) * i2d_read(record + 40) *
        i2d_read(record + 48) * i2d_read(record + 56);
    if i2d_read(record + 16) != 0 || elements != (count as! u64) { i2_embedding_abort(); }
    return i2d_read(decoder) + i2d_read(record + 80);
}

export fn i2_serial_mix(decoder: u64, physical: i32, history: I2DraftSpan, count: i32,
    output: I2DraftSpan, messages: I2DraftSpan) {
    let index = decoder + I2_OPEN_AUX + ((physical as! u64) * 30 + 17) * 8;
    let query = fg_i2_host_coefficients(decoder, i2d_read(index) as! i64, 9216);
    fg_i2_host_mix(history.address, count, output.address, i2d_span(query, 9216), 4, messages.address);
}

fn i2_embedding(decoder: u64) -> InklingEmbedding {
    let id = i2d_read(decoder + I2_OPEN_GLOBALS);
    let record = i2_tensor_record(decoder, id);
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


extern fn fg_i2_parallel_logits(decoder: u64, logits: u64) -> bool;
extern fn fg_i2_softcap_logits(logits: I2DraftSpan);
extern fn fg_i2_validate_logits(position: i32, logits: u64, handled: bool);

export fn fg_i2_read_token_logits(decoder: u64, absolute: i32, current: u64, history: u64, count: i32, decode: bool) {
    var normalized: f32[2304];
    let output = i2_serial_span(normalized);
    let global_weights = decoder + I2_OPEN_GLOBALS;
    let queries = fg_i2_host_coefficients(decoder, i2d_read(global_weights + 32) as! i64, 2304);
    fg_i2_host_mix(history, count, current, i2d_span(queries, 2304), 1, output.address);
    let gains = fg_i2_host_coefficients(decoder, i2d_read(global_weights + 24) as! i64, 2304);
    if fg_i2_host_measured_rms(output, i2d_span(gains, 2304), 0.000001, output) != 2304 { i2_embedding_abort(); }
    let logits = decoder + I2D_LOGITS;
    fg_i2_host_project(decoder, i2d_read(global_weights + 16) as! i64, output.address, 2304, logits, 201024);
    let handled = decode && fg_i2_parallel_logits(decoder, logits);
    if !handled { fg_i2_softcap_logits(i2d_span(logits, 201024)); }
    fg_i2_validate_logits(absolute, logits, handled);
}

export fn fg_i2_prepare_layer_inputs(decoder: u64, aux: u64, sliding: bool, current: u64,
    history: u64, count: i32, inputs: u64) {
    let query = i2d_read(aux + 80) as! i64;
    if query >= 0 {
        let queries: i32 = if sliding { 1 } else { 3 };
        let weights = (queries as! i64) * 2304;
        let coefficients = fg_i2_host_coefficients(decoder, query, weights);
        fg_i2_host_mix(history, count, current, i2d_span(coefficients, weights), queries, inputs);
        if sliding {
            i2d_transfer(inputs + 9216, inputs, 9216);
            i2d_transfer(inputs + 18432, inputs, 9216);
        }
    } else {
        for index in 3 { i2d_transfer(inputs + (index as! u64) * 9216, current, 9216); }
    }
}

extern fn fg_i2_measured_postconv(input: I2DraftSpan, weight: I2DraftSpan, history: I2DraftSpan, output: I2DraftSpan) -> i64;
extern fn fg_cuda_host_view_f32(address: u64, length: i64) -> f32[:];

export fn fg_i2_finish_attention(decoder: u64, aux: u64, physical: i32, slot: i32, current: u64,
    history: u64, count: i32, projection: u64, next: u64, ffn: u64) -> i32 {
    let weights = fg_i2_host_coefficients(decoder, i2d_read(aux + 16) as! i64, 9216);
    let convolution = i2d_read(decoder + I2D_HA) + (slot as! u64) * 27648;
    assert(fg_i2_measured_postconv(i2d_span(projection, 2304), i2d_span(weights, 9216),
        i2d_span(convolution, 6912), i2d_span(next, 2304)) == 2304);
    var count_after = count;
    if physical % 6 == 0 {
        assert(count < 11);
        fg_i2_host_save_history(history + (count as! u64) * 9216, current);
        count_after += 1;
    } else {
        let residual = fg_cuda_host_view_f32(current, 2304);
        var output = fg_cuda_host_view_f32(next, 2304);
        for index in 2304 { output[index] += residual[index]; }
    }
    let query = fg_i2_host_coefficients(decoder, i2d_read(aux + 72) as! i64, 2304);
    fg_i2_host_mix(history, count_after, next, i2d_span(query, 2304), 1, ffn);
    return count_after;
}
