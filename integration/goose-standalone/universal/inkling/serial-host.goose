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

fn fg_i2_prepare_layer_inputs(decoder: u64, aux: u64, sliding: bool, current: u64,
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

fn fg_i2_finish_attention(decoder: u64, aux: u64, physical: i32, slot: i32, current: u64,
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

fn fg_i2_serial_moe(decoder: u64, core: u64, aux: u64, slot: i32, absolute: i32,
    ffn: u64, message: u64, next: u64, current: u64, projection: u64, work: u64) {
    if fg_i2_resident_moe() {
        fg_i2_host_moe_resident(decoder, core, aux, slot, absolute, 1, ffn, message, next, current);
        return;
    }
    if message != 0 { fg_i2_host_control(decoder, aux, 3, ffn, message); }
    i2_run_host_moe(decoder, core, ffn, 1, projection, work);
    let convolution = fg_i2_host_coefficients(decoder, i2_host_aux(aux, 3), 9216);
    let history = i2d_read(decoder + 887144) + (slot as! u64) * 27648;
    i2_finish_host_moe(projection, convolution, history, next, current);
}

fn i2_run_host_moe(decoder: u64, core: u64, ffn: u64, count: i32, projection: u64, work: u64) {
    let gain = fg_i2_host_coefficients(decoder, i2_host_aux(core, 9), 2304);
    for index in count as! i64 {
        let row = i2d_span(ffn + (index as! u64) * 9216, 2304);
        assert(fg_i2_host_measured_rms(row, i2d_span(gain, 2304), 0.000001, row) == 2304);
    }
    let ctx = fg_i2_host_context();
    let order: i32[4] = [5, 8, 6, 7];
    var weights: u64[4];
    var nodes: u64[8] = [0; 8];
    for index in 4 { weights[index] = fg_i2_host_weight(ctx, decoder, i2_host_aux(core, order[index])); }
    let tensor = fg_i2_host_rows_input(ctx, ffn, 2304, count);
    let output = fg_i2_host_measured_moe(ctx as! i64, tensor as! i64,
        i2d_span(i2d_address(unsafe_c_address<c_ptr<const void>>(weights[0])), 4), i2d_span(work, 16),
        i2d_span(i2d_address(unsafe_c_address<c_ptr<const void>>(nodes[0])), 8));
    assert(output != 0);
    fg_i2_host_evaluate_one(ctx, output as! u64, projection, 2304 * count);
    ggml_free(ctx);
}

fn i2_finish_host_moe(projection: u64, weights_conv: u64, history: u64, next: u64, current: u64) {
    assert(fg_i2_measured_postconv(i2d_span(projection, 2304), i2d_span(weights_conv, 9216),
        i2d_span(history, 6912), i2d_span(current, 2304)) == 2304);
    let residual = fg_cuda_host_view_f32(next, 2304);
    var values = fg_cuda_host_view_f32(current, 2304);
    for index in 2304 {
        let value = values[index] + residual[index];
        values[index] = value;
        assert(value <= 3.402823466e38 && value >= -3.402823466e38);
    }
}

export fn fg_i2_prefill_moe(decoder: u64, core: u64, aux: u64, slot: i32, count: i32,
    ffn: u64, projection: u64, residual: u64, tokens: u64, work: u64) {
    i2_run_host_moe(decoder, core, ffn, count, projection, work);
    let convolution = fg_i2_host_coefficients(decoder, i2_host_aux(aux, 3), 9216);
    let history = i2d_read(decoder + 887144) + (slot as! u64) * 27648;
    for index in count as! i64 {
        let row = index as! u64;
        i2_finish_host_moe(projection + row * 9216, convolution, history, residual + row * 9216, tokens + row * 285708);
    }
}

extern fn fg_i2_serial_native_attention(decoder: u64, core: u64, aux: u64, slot: i32, absolute: i32,
    sliding: bool, inputs: u64, output: u64, work: u64);

export fn i2_serial_layer(decoder: u64, physical: i32, slot: i32, absolute: i32,
    sliding_mode: i32, current: I2DraftSpan, history: I2DraftSpan, count: i32, message_rows: I2DraftSpan) -> i32 {
    let sliding = sliding_mode != 0;
    let layer = physical as! u64;
    let core = decoder + I2_OPEN_CORE + layer * 80;
    let aux = decoder + I2_OPEN_AUX + layer * 240;
    let messages = if message_rows.len == 0 { 0 } else { message_rows.address };
    var inputs: f32[6912];
    let input = i2_serial_span(inputs).address;
    fg_i2_prepare_layer_inputs(decoder, aux, sliding, current.address, history.address, count, input);
    var projection: f32[2304];
    let proj = i2_serial_span(projection).address;
    var work: i64[16] = [0; 16];
    let scratch = i2d_address(unsafe_c_address<c_ptr<const void>>(work[0]));
    if fg_i2_serial_graph_enabled() {
        let rows: u64[3] = [input, input + 9216, input + 18432];
        let row_table = i2d_address(unsafe_c_address<c_ptr<const void>>(rows[0]));
        if i2d_byte(decoder + I2D_RETAIN) != 0 && i2_joined_bridge_enabled() &&
            fg_i2_host_cached_history() && fg_i2_resident_moe() {
            let add_residual = physical % 6 != 0;
            var saved_count = count;
            if !add_residual {
                assert(count < 11);
                fg_i2_host_save_history(history.address + (count as! u64) * 9216, current.address);
                saved_count += 1;
            }
            fg_i2_host_joined_layer(decoder, core, aux, slot, absolute, sliding, add_residual,
                row_table, messages, current.address, history.address, saved_count);
            return saved_count;
        }
        let signals: u64[3] = [messages, messages + 9216, messages + 18432];
        let signal_table = if messages == 0 { 0 } else { i2d_address(unsafe_c_address<c_ptr<const void>>(signals[0])) };
        fg_i2_host_attention_graph(decoder, core, aux, slot, absolute, 1, sliding, row_table, signal_table, proj);
    } else {
        for index in 3 {
            let offset = (index as! u64) * 9216;
            if messages != 0 { fg_i2_host_control(decoder, aux, index as! i32, input + offset, messages + offset); }
            let gain_index: i32 = if sliding { 11 } else { 12 + (index as! i32) };
            let gain = fg_i2_host_coefficients(decoder, i2_host_aux(aux, gain_index), 2304);
            let row = i2d_span(input + offset, 2304);
            assert(fg_i2_host_measured_rms(row, i2d_span(gain, 2304), 0.000001, row) == 2304);
        }
        fg_i2_serial_native_attention(decoder, core, aux, slot, absolute, sliding, input, proj, scratch);
    }
    var next: f32[2304];
    var ffn: f32[2304];
    let next_address = i2_serial_span(next).address;
    let ffn_address = i2_serial_span(ffn).address;
    let result = fg_i2_finish_attention(decoder, aux, physical, slot, current.address, history.address,
        count, proj, next_address, ffn_address);
    fg_i2_serial_moe(decoder, core, aux, slot, absolute, ffn_address,
        if messages == 0 { 0 } else { messages + 27648 }, next_address, current.address, proj, scratch);
    return result;
}

extern fn fg_i2_measured_attention_projections(ctx: i64, input: I2DraftSpan, weights: I2DraftSpan, sliding: bool,
    work: I2DraftSpan, output: I2DraftSpan) -> i64;
extern fn fg_i2_evaluate_attention_inputs(ctx: u64, output: u64, matrices: i32, projected: u64, count: i32);

export fn fg_i2_project_attention_inputs(decoder: u64, core: u64, aux: u64, sliding: bool,
    rows: u64, count: i32, projected: u64, work: u64) -> i32 {
    let ctx = fg_i2_host_context();
    var inputs: u64[3];
    var outputs: u64[8] = [0; 8];
    var weights: u64[8] = [0; 8];
    for index in 3 {
        inputs[index] = fg_i2_host_rows_input(ctx, i2d_read(rows + (index as! u64) * 8), 2304, count);
    }
    let ids: i64[8] = [i2_host_aux(core, 0), i2_host_aux(core, 1), i2_host_aux(core, 2),
        i2_host_aux(aux, 5), i2_host_aux(aux, 8), i2_host_aux(core, 4), i2_host_aux(aux, 15), i2_host_aux(aux, 16)];
    let matrices: i32 = if sliding { 6 } else { 8 };
    let packed: i32 = if sliding { 8600 } else { 8664 };
    for index in matrices as! i64 { weights[index] = fg_i2_host_weight(ctx, decoder, ids[index]); }
    let output = i2d_address(unsafe_c_address<c_ptr<const void>>(outputs[0]));
    assert(fg_i2_measured_attention_projections(ctx as! i64,
        i2d_span(i2d_address(unsafe_c_address<c_ptr<const void>>(inputs[0])), 3),
        i2d_span(i2d_address(unsafe_c_address<c_ptr<const void>>(weights[0])), 8),
        sliding, i2d_span(work, 16), i2d_span(output, 8)) == matrices as! i64);
    fg_i2_evaluate_attention_inputs(ctx, output, matrices, projected, packed * count);
    ggml_free(ctx);
    return packed;
}

extern fn i2_prepare_native(projected: I2DraftSpan, q_clip: I2DraftSpan, r_clip: I2DraftSpan,
    key_kernel: I2DraftSpan, value_kernel: I2DraftSpan, position: i64, sliding: bool,
    key_history: I2DraftSpan, value_history: I2DraftSpan, q: I2DraftSpan, r: I2DraftSpan,
    k: I2DraftSpan, v: I2DraftSpan) -> i64;

fn i2_native_weight(decoder: u64, aux: u64, index: i32, count: i64) -> I2DraftSpan {
    return i2d_span(fg_i2_host_coefficients(decoder, i2_host_aux(aux, index), count), count);
}

export fn fg_i2_prepare_native_row(decoder: u64, args: u64, aux: u64, slot: i32, position: i32, index: i32,
    sliding: bool, row: u64, packed: i32) {
    let token = index as! u64;
    let offset = (slot as! u64) * 12288;
    let key_history = i2d_read(decoder + 887120) + offset;
    let value_history = i2d_read(decoder + 887128) + offset;
    let q = i2d_read(args) + token * 12288;
    let k = i2d_read(args + 8) + token * 4096;
    let v = i2d_read(args + 16) + token * 4096;
    let r = i2d_read(args + 24) + token * 1536;
    assert(i2_prepare_native(i2d_span(row, packed as! i64),
        i2_native_weight(decoder, aux, 4, 3072), i2_native_weight(decoder, aux, 7, 384),
        i2_native_weight(decoder, aux, 0, 4096), i2_native_weight(decoder, aux, 1, 4096),
        position as! i64, sliding, i2d_span(key_history, 3072), i2d_span(value_history, 3072),
        i2d_span(q, 3072), i2d_span(r, 384), i2d_span(k, 1024), i2d_span(v, 1024)) == 1);
    i2d_transfer(i2d_read(args + 32) + token * 96, row + 22016, 96);
    i2d_transfer(i2d_read(args + 40) + token * 12288, row + 22112, 12288);
}

export fn fg_i2_prepare_prefill_rows(decoder: u64, args: u64, aux: u64, slot: i32, begin: i32, count: i32,
    sliding: bool, projections: u64, packed: i32) {
    let sizes: u64[8] = [12288, 4096, 4096, 1536, 96, 12288, 128, 128];
    let matrices: i64 = if sliding { 6 } else { 8 };
    var row: f32[8664];
    let output = i2_serial_span(row).address;
    for index in count as! i64 {
        var source = projections;
        var destination = output;
        for matrix in matrices {
            let bytes = sizes[matrix];
            i2d_transfer(destination, source + (index as! u64) * bytes, bytes);
            source += (count as! u64) * bytes;
            destination += bytes;
        }
        let position = index as! i32;
        fg_i2_prepare_native_row(decoder, args, aux, slot, begin + position, position, sliding, output, packed);
    }
}

const I2_NATIVE_WORKSPACE_COUNT: u64 = 887104;
const I2_NATIVE_WORKSPACE: u64 = 887112;

extern fn i2_finish_native(gate: I2DraftSpan, output: I2DraftSpan) -> i64;
extern "realloc" fn i2_native_realloc(base: c_ptr<void>, bytes: u64) -> c_ptr<void>;

export fn fg_i2_native_workspace(decoder: u64, count: i32, args: u64) {
    let rows = count as! u64;
    let floats = rows * 8600;
    var storage = i2d_read(decoder + I2_NATIVE_WORKSPACE);
    if i2d_read(decoder + I2_NATIVE_WORKSPACE_COUNT) < floats {
        let allocation = i2_native_realloc(unsafe_c_ptr<c_ptr<void>>(storage), floats * 4);
        i2d_copy(unsafe_c_address<c_ptr<void>>(storage), unsafe_c_address<c_ptr<const void>>(allocation), 8);
        assert(storage != 0);
        i2d_set(decoder + I2_NATIVE_WORKSPACE, storage);
        i2d_set(decoder + I2_NATIVE_WORKSPACE_COUNT, floats);
    }
    let widths: u64[5] = [3072, 1024, 1024, 384, 24];
    var address = storage;
    for index in 5 {
        i2d_set(args + (index as! u64) * 8, address);
        address += widths[index] * rows * 4;
    }
    i2d_set(args + 40, address);
}

export fn fg_i2_native_finish(args: u64, count: i32, output: u64) {
    let gate = i2d_read(args + 40);
    for index in count as! i64 {
        let offset = (index as! u64) * 12288;
        assert(i2_finish_native(i2d_span(gate + offset, 3072), i2d_span(output + offset, 3072)) == 3072);
    }
}

extern fn fg_i2_measure_native_attention(batch: u64, inputs: u64, output: u64);

fn i2_native_batch(decoder: u64, aux: u64, slot: i32, begin: i32, count: i32, sliding: bool,
    args: u64, output: u64, batch: u64) {
    i2d_set_int(batch, slot);
    i2d_set_int(batch + 4, i2d_int(decoder + I2D_CAP));
    i2d_set_int(batch + 8, begin);
    i2d_set_int(batch + 12, count);
    i2d_set_byte(batch + 16, if sliding { 1 } else { 0 });
    i2d_set_int(batch + 20, i2d_int(decoder + I2D_FMT));
    if args != 0 {
        i2d_transfer(batch + 24, args, 32);
        i2d_set(batch + 64, i2d_read(args + 32));
    }
    i2d_set(batch + 56, i2_native_weight(decoder, aux, 6, 16384).address);
    i2d_set(batch + 72, output);
}

fn i2_native_q8_weight(decoder: u64, id: i64, width: u64, rows: u64) -> u64 {
    let record = i2_tensor_record(decoder, id as! u64);
    assert(i2d_read(record + 16) == 8 && i2d_read(record + 32) == width &&
        i2d_read(record + 40) == rows && i2d_read(record + 48) == 1 && i2d_read(record + 56) == 1);
    return i2d_read(decoder) + i2d_read(record + 80);
}

fn i2_native_projection(decoder: u64, weight_id: i64, gate: u64, output: u64) {
    i2d_set(output, gate);
    i2d_set(output + 8, i2_native_q8_weight(decoder, weight_id, 3072, 2304));
    i2d_set(output + 16, 2304 * ggml_row_size(8, 3072));
    i2d_set_int(output + 24, 2304);
}

export fn fg_i2_native_submit(decoder: u64, args: u64, aux: u64, slot: i32, begin: i32, count: i32, sliding: bool, output: u64) {
    var batch: u64[10] = [0; 10];
    let packet = i2d_address(unsafe_c_address<c_ptr<const void>>(batch[0]));
    i2_native_batch(decoder, aux, slot, begin, count, sliding, args, output, packet);
    fg_i2_measure_native_attention(packet, 0, 0);
    fg_i2_native_finish(args, count, output);
}

export fn fg_i2_native_project(decoder: u64, args: u64, aux: u64, slot: i32, begin: i32, count: i32,
    sliding: bool, weight_id: i64, output: u64) {
    var batch: u64[10] = [0; 10];
    var projection: u64[4] = [0; 4];
    let packet = i2d_address(unsafe_c_address<c_ptr<const void>>(batch[0]));
    let projected = i2d_address(unsafe_c_address<c_ptr<const void>>(projection[0]));
    i2_native_batch(decoder, aux, slot, begin, count, sliding, args, output, packet);
    i2_native_projection(decoder, weight_id, i2d_read(args + 40), projected);
    fg_i2_measure_native_attention(packet, 0, projected);
}

export fn fg_i2_host_bind_attention(decoder: u64, core: u64, aux: u64, slot: i32, begin: i32, count: i32,
    sliding: bool, call: u64) {
    i2_node_clear(unsafe_c_ptr<c_ptr<void>>(call), 0, 232);
    i2_native_batch(decoder, aux, slot, begin, count, sliding, 0, 0, call);
    let input = call + 80;
    let ids: i64[8] = [i2_host_aux(core, 0), i2_host_aux(core, 1), i2_host_aux(core, 2),
        i2_host_aux(aux, 5), i2_host_aux(aux, 8), i2_host_aux(core, 4), i2_host_aux(aux, 15), i2_host_aux(aux, 16)];
    let widths: u64[8] = [3072, 1024, 1024, 384, 24, 3072, 32, 32];
    let matrices: i64 = if sliding { 6 } else { 8 };
    for index in matrices {
        i2d_set(input + 24 + (index as! u64) * 8, i2_native_q8_weight(decoder, ids[index], 2304, widths[index]));
    }
    i2d_set(input + 88, i2_native_weight(decoder, aux, 4, 3072).address);
    i2d_set(input + 96, i2_native_weight(decoder, aux, 7, 384).address);
    i2d_set(input + 104, i2_native_weight(decoder, aux, 0, 4096).address);
    i2d_set(input + 112, i2_native_weight(decoder, aux, 1, 4096).address);
    i2_native_projection(decoder, i2_host_aux(core, 3), 0, call + 200);
}

export fn fg_i2_native_resident(decoder: u64, core: u64, aux: u64, slot: i32, begin: i32, count: i32,
    sliding: bool, q: u64, k: u64, v: u64, output: u64) {
    var storage: u64[29];
    let call = i2d_address(unsafe_c_address<c_ptr<const void>>(storage[0]));
    fg_i2_host_bind_attention(decoder, core, aux, slot, begin, count, sliding, call);
    i2d_set(call + 72, output);
    i2d_set(call + 80, q);
    i2d_set(call + 88, k);
    i2d_set(call + 96, v);
    fg_i2_measure_native_attention(call, call + 80, call + 200);
}

export fn prefill_projection(decoder: u64, id: i64, input: u64, width: i32, output: u64, rows: i32, count: i32) {
    let ctx = fg_i2_host_context();
    let weight = fg_i2_host_weight(ctx, decoder, id);
    let input_tensor = fg_i2_host_rows_input(ctx, input, width, count);
    fg_i2_host_evaluate_one(ctx, ggml_mul_mat(ctx, weight, input_tensor), output, rows * count);
    ggml_free(ctx);
}

export fn prefill_loop_inputs(decoder: u64, token: u64, layer: i32, pass: i32, device_loop: bool, device_prelude: bool) {
    let prelude = token + 9216;
    let previous = token + 18432;
    let bank = token + 138240;
    let count = i2d_int(token + 285696);
    let saved = i2d_int(token + 285700);
    var bank_count = i2d_int(token + 285704);
    if layer == 6 && pass == 0 {
        if !device_prelude { i2d_transfer(prelude, token, 9216); }
        i2d_set_int(token + 285700, count);
    } else if layer == 6 {
        bank_count = count - saved - 1;
        assert(bank_count >= 0 && bank_count < 11);
        i2d_set_int(token + 285704, bank_count);
        if !device_loop {
            i2d_transfer(bank, token + 27648 + ((saved + 1) as! u64) * 9216, (bank_count as! u64) * 9216);
            i2d_transfer(previous, token, 9216);
        }
        if !device_prelude { i2d_transfer(token, prelude, 9216); }
        i2d_set_int(token + 285696, saved);
    }
    if pass > 0 && !device_loop {
        let index = decoder + I2_OPEN_AUX + ((layer as! u64) * 30 + 17) * 8;
        let query = fg_i2_host_coefficients(decoder, i2d_read(index) as! i64, 9216);
        fg_i2_host_mix(bank, bank_count, previous, i2d_span(query, 9216), 4, token + 248832);
    }
}

export fn fg_i2_prefill_control(decoder: u64, aux: u64, which: i32, count: i32, tokens: u64, current: u64, buffers: u64) {
    let at = 18 + which * 3;
    let normalized = i2d_read(buffers);
    let hidden = i2d_read(buffers + 8);
    let gain = i2d_read(buffers + 16);
    let forget = i2d_read(buffers + 24);
    let combined = i2d_read(buffers + 32);
    let messages = tokens + 248832 + (which as! u64) * 9216;
    if fg_i2_host_resident_control() {
        for index in count as! i64 {
            let row = index as! u64;
            i2d_transfer(gain + row * 9216, messages + row * 285708, 9216);
        }
        fg_i2_host_control_resident(decoder, aux, at, current, gain, count);
        return;
    }
    for index in count as! i64 {
        let row = index as! u64;
        let input = normalized + row * 18432;
        i2_host_normalize(current + row * 9216, input);
        i2_host_normalize(messages + row * 285708, input + 9216);
    }
    if fg_i2_host_resident_plan() {
        fg_i2_host_control_group(decoder, aux, at, normalized, count, combined);
        for index in count as! i64 {
            let row = index as! u64;
            let packet = combined + row * 18432;
            i2d_transfer(gain + row * 9216, packet, 9216);
            i2d_transfer(forget + row * 9216, packet + 9216, 9216);
        }
    } else {
        prefill_projection(decoder, i2_host_aux(aux, at), normalized, 4608, hidden, 64, count);
        prefill_projection(decoder, i2_host_aux(aux, at + 1), hidden, 64, forget, 2304, count);
        prefill_projection(decoder, i2_host_aux(aux, at + 2), hidden, 64, gain, 2304, count);
    }
    for index in count as! i64 {
        let row = index as! u64;
        let offset = row * 9216;
        let output = i2d_span(current + offset, 2304);
        assert(fg_i2_host_measured_blend(output, i2d_span(messages + row * 285708, 2304),
            i2d_span(gain + offset, 2304), i2d_span(forget + offset, 2304), output) == 2304);
    }
}

extern fn inkling_arena_depthmix(past: u64, next: u64, query: u64, output: u64, rows: i32, save_history: bool, full: u64) -> bool;

export fn fg_i2_prefill_postconv(decoder: u64, aux: u64, slot: i32, count: i32, input: u64, output: u64) {
    let weight = i2_native_weight(decoder, aux, 2, 9216);
    let history = i2d_span(i2d_read(decoder + I2D_HA) + (slot as! u64) * 27648, 6912);
    for index in count as! i64 {
        let offset = (index as! u64) * 9216;
        assert(fg_i2_measured_postconv(i2d_span(input + offset, 2304), weight, history,
            i2d_span(output + offset, 2304)) == 2304);
    }
}

export fn fg_i2_prefill_depthmix(decoder: u64, aux: u64, tokens: u64, count: i32, save_history: bool,
    resident: bool, staging: u64, residual: u64, output: u64) {
    let rows = i2d_int(tokens + 285696);
    for index in count as! i64 {
        let token = tokens + (index as! u64) * 285708;
        assert(i2d_int(token + 285696) == rows);
        if !resident {
            let source = if save_history { token + 27648 + ((rows - 1) as! u64) * 9216 } else { token };
            i2d_transfer(staging + (index as! u64) * 9216, source, 9216);
        }
    }
    assert(inkling_arena_depthmix(if resident { 0 } else { staging }, residual,
        i2_native_weight(decoder, aux, 9, 2304).address, output, rows, save_history, 0));
}

extern fn inkling_moe_retain_output(host: u64, device: u64);
extern fn inkling_loop_bind(queries: u64, q: u64, k: u64, v: u64, messages: u64);
extern fn inkling_loop_generated() -> u32;
extern fn inkling_arena_depthmix_close() -> u32;

export fn fg_i2_prefill_resident_moe(decoder: u64, core: u64, aux: u64, tokens: u64,
    slot: i32, begin: i32, layer: i32, pass: i32, count: i32, device_loop: bool,
    prune_host: bool, retain: bool, direct_depthmix: bool, output_slot: u64,
    ffn: u64, residual: u64, output: u64, gain: u64) -> bool {
    let rows = count as! i64;
    var messages: u64 = 0;
    if pass > 0 {
        if device_loop {
            if !prune_host {
                for index in rows * 2304 { i2d_set_int(gain + (index as! u64) * 4, 0x7fc00000); }
            }
            inkling_loop_bind(i2_native_weight(decoder, aux, 17, 9216).address, 0, 0, 0, gain);
        } else {
            for index in rows {
                let row = index as! u64;
                i2d_transfer(gain + row * 9216, tokens + row * 285708 + 276480, 9216);
            }
        }
        messages = gain;
    }
    if retain { inkling_moe_retain_output(output, output_slot); }
    fg_i2_host_moe_resident(decoder, core, aux, slot, begin, count, ffn, messages, residual, output);
    inkling_moe_retain_output(0, 0);
    if device_loop {
        assert(inkling_loop_generated() == 8);
        inkling_loop_bind(0, 0, 0, 0, 0);
        i2d_log(str("Inkling device loop messages begin=", begin, " layer=", layer, " pass=", pass, "\n"));
    }
    if direct_depthmix {
        let reads = inkling_arena_depthmix_close();
        assert(reads != 0);
        i2d_log(str("Inkling direct DepthMix begin=", begin, " layer=", layer, " pass=", pass, " rows=", count,
            " device_reads=", reads, "\n"));
    }
    if retain {
        i2d_log(str("Inkling resident MoE handoff begin=", begin, " layer=", layer, " pass=", pass, "\n"));
    } else {
        for index in rows {
            let row = index as! u64;
            i2d_transfer(tokens + row * 285708, output + row * 9216, 9216);
        }
    }
    return retain;
}

extern fn inkling_loop_storage() -> u64;
extern fn inkling_loop_save(storage: u64, previous: u64) -> bool;
extern fn inkling_arena_boundary_read(output: u64, index: i32, last_only: bool) -> bool;

export fn i2_prefill_save_rewind(work: u64, pass: i64, resident: bool) {
    let staging = i2d_read(work + 16);
    if !resident {
        let tokens = i2d_read(work + 8);
        for index in i2d_int(work + 32) as! i64 {
            let row = index as! u64;
            i2d_transfer(staging + row * 9216, tokens + row * 285708, 9216);
        }
    }
    assert(inkling_loop_save(inkling_loop_storage(), if resident { 0 } else { staging }));
    if resident {
        i2d_log(str("Inkling resident rewind begin=", i2d_int(work + 36), " pass=", pass as! i32, "\n"));
    }
}

export fn i2_prefill_read_boundary(work: u64) {
    let staging = i2d_read(work + 16);
    let tokens = i2d_read(work + 8);
    assert(inkling_arena_boundary_read(staging, 2, false));
    for index in i2d_int(work + 32) as! i64 {
        let row = index as! u64;
        i2d_transfer(tokens + row * 285708 + 46080, staging + row * 9216, 9216);
    }
}

extern "calloc" fn i2_prefill_calloc(count: u64, bytes: u64) -> c_ptr<void>;
extern fn fg_cuda_inkling_prefill_readback(bytes: u64) -> u64;
extern fn fg_i2_prefill_team_start(team: u64, count: i32) -> bool;
extern fn fg_i2_prefill_set_readback(output: u64, elements: u64);
extern fn fg_i2_prefill_check_residual(position: i32, current: u64);
extern fn inkling_arena_window(enabled: bool, begin: i32);
extern fn inkling_prelude_clear();
extern fn inkling_prefill_final_rows(current: u64, prelude: u64) -> bool;
extern fn i2_prefill_edge_plan(visits: I2DraftSpan, aux: I2DraftSpan, edges: bool,
    boundary_enabled: bool, extend: bool, full: bool, keep: bool, ordinary: I2DraftSpan, boundary: I2DraftSpan);
extern fn i2_run_prefill_visits(work: u64, visits: I2DraftSpan, ordinary: I2DraftSpan,
    boundary: I2DraftSpan, device: I2DraftSpan, loop_enabled: bool);

fn i2_prefill_env_pointer(name: const u8[:]) -> u64 {
    return fg_cuda_host_getenv(i2d_address(unsafe_c_slice_data<c_ptr<const void>>(name)));
}

fn i2_prefill_env_equal(value: u64, text: const u8[:]) -> bool {
    if value == 0 { return false; }
    for index in text.len {
        if i2d_byte(value + (index as! u64)) != text[index] { return false; }
    }
    return i2d_byte(value + (text.len as! u64)) == 0;
}

fn i2_prefill_bind_readback(scratch: u64, count: i32) -> bool {
    var output = scratch + 343277576;
    let elements = (count as! u64) * 2304;
    let config = i2_prefill_env_pointer("I2_DIRECT_PREFILL_READBACK\0");
    let direct = count >= 512 && fg_i2_serial_graph_enabled() && fg_i2_resident_moe() && !i2_prefill_env_equal(config, "0");
    if direct && i2_prefill_env_equal(config, "pinned") {
        let pinned = fg_cuda_inkling_prefill_readback(elements * 4);
        if pinned != 0 { output = pinned; }
    }
    i2d_set(scratch + 343277568, output);
    if direct { fg_i2_prefill_set_readback(output, elements); }
    return direct;
}

fn i2_prefill_finish_rows(decoder: u64, tokens: u64, begin: i32, count: i32, need_logits: bool, device: const i64[:]) {
    let last = tokens + ((count - 1) as! u64) * 285708;
    let resident = device[0] != 0;
    let prelude = device[3] != 0;
    if device[1] != 0 && need_logits { assert(inkling_arena_boundary_read(last + 46080, 2, true)); }
    if device[2] != 0 && need_logits { assert(inkling_arena_boundary_read(last + 55296, 3, true)); }
    if need_logits && (resident || prelude) {
        assert(inkling_prefill_final_rows(if resident { last } else { 0 }, if prelude { last + 36864 } else { 0 }));
    }
    if prelude { i2d_log(str("Inkling resident prelude begin=", begin, "\n")); }
    if resident {
        let logits: i32 = if need_logits { 1 } else { 0 };
        i2d_log(str("Inkling final activation retained begin=", begin, " logits=", logits, "\n"));
    }
}

export fn fg_i2_backend_prefill(decoder: u64, ids: u64, begin: i32, count: i32, need_logits: bool) {
    assert(count > 0 && count <= 4096);
    let allocation = i2_prefill_calloc(count as! u64, 285708);
    var tokens: u64 = 0;
    i2d_copy(unsafe_c_address<c_ptr<void>>(tokens), unsafe_c_address<c_ptr<const void>>(allocation), 8);
    let scratch = i2d_alloc(646316040);
    assert(tokens != 0 && scratch != 0);
    let direct = i2_prefill_bind_readback(scratch, count);
    assert(fg_i2_embed_prefill(decoder, ids, count, tokens, 285708));
    var team_storage: u64[37] = [0; 37];
    let team = i2d_address(unsafe_c_address<c_ptr<const void>>(team_storage[0]));
    let workers = if !i2d_env_is("I2_PREFILL_TEAM", 48) && fg_i2_prefill_team_start(team, count) { team } else { 0 };
    let full_batch = count == 4096;
    inkling_arena_window(full_batch, begin);
    let loop_enabled = !i2d_env_is("I2_DEVICE_LOOP_MESSAGES", 48);
    let full = !i2d_env_is("I2_FULL_DEVICE_DEPTHMIX", 48);
    let boundary_enabled = !i2d_env_is("I2_DEVICE_BLOCK_HISTORY", 48) &&
        !i2d_env_is("I2_SLIDING_DEVICE_DEPTHMIX", 48) && full;
    let extend = boundary_enabled && loop_enabled && !i2d_env_is("I2_DEVICE_REWIND_TAIL", 48);
    let edges = extend && full_batch && direct && fg_i2_resident_moe() && fg_i2_host_cached_history() &&
        !i2d_env_is("I2_DEVICE_PREFILL_EDGES", 48) && !i2d_env_is("I2_PARALLEL_CUDA_POSTCONV", 48) &&
        i2_prefill_env_pointer("I2_CHECK_STATE\0") == 0;
    let keep = full_batch && !i2d_env_is("I2_RETAIN_PREFILL_MOE", 48);
    var ordinary: i64[48];
    var boundary: i64[48];
    let visits = i2d_span(decoder + I2_OPEN_VISITS, 192);
    let ordinary_span = i2d_span(i2d_address(unsafe_c_address<c_ptr<const void>>(ordinary[0])), 48);
    let boundary_span = i2d_span(i2d_address(unsafe_c_address<c_ptr<const void>>(boundary[0])), 48);
    i2_prefill_edge_plan(visits, i2d_span(decoder + I2_OPEN_AUX, 720), edges, boundary_enabled, extend, full,
        keep, ordinary_span, boundary_span);
    let work: u64[5] = [decoder, tokens, scratch, workers, (count as! u64) | ((begin as! u32 as! u64) << 32)];
    var device: i64[4] = [0; 4];
    inkling_prelude_clear();
    i2_run_prefill_visits(i2d_address(unsafe_c_address<c_ptr<const void>>(work[0])), visits, ordinary_span, boundary_span,
        i2d_span(i2d_address(unsafe_c_address<c_ptr<const void>>(device[0])), 4), loop_enabled);
    i2_prefill_finish_rows(decoder, tokens, begin, count, need_logits, device);
    inkling_prelude_clear();
    inkling_arena_window(false, begin);
    if workers != 0 { fg_i2_close_logits_team(workers); }
    for index in count as! i64 { fg_i2_prefill_check_residual(begin + (index as! i32), tokens + (index as! u64) * 285708); }
    if need_logits {
        let draft_count = i2d_int(decoder + I2D_DRAFT + I2D_COUNT);
        let draft = draft_count > 1 && draft_count == count;
        var index = if draft { 0 } else { count - 1 };
        while index < count {
            let row = tokens + (index as! u64) * 285708;
            fg_i2_read_token_logits(decoder, begin + index, row, row + 27648, i2d_int(row + 285696), draft);
            if draft {
                let scores = i2d_read(decoder + I2D_DRAFT + I2D_SCORES);
                i2d_transfer(scores + (index as! u64) * 800232, decoder + I2D_LOGITS, 800232);
            }
            index += 1;
        }
    }
    fg_i2_prefill_set_readback(0, 0);
    i2d_free(unsafe_c_ptr<c_ptr<void>>(scratch));
    i2d_free(allocation);
}

extern fn fg_standalone_cpu_batch_threads() -> i64;
extern fn fg_i2_prefill_layer(decoder: u64, tokens: u64, scratch: u64, count: i32, begin: i32,
    visit: u64, workers: u64, current: u64, retain: bool, slot: u64, boundary: u64, tail: u64,
    loop_enabled: bool, prelude: bool);
extern fn fg_i2_prefill_trace_visit(visit: i32, current: u64);

export fn fg_i2_prefill_task_count(count: i32) -> i32 {
    if count < 64 || i2d_env_is("I2_PARALLEL_PREP", 48) { return 1; }
    let configured = fg_standalone_cpu_batch_threads();
    let limit = (count / 32) as! i64;
    let workers = if configured > 16 { 16 } else { configured };
    let bounded = if workers > limit { limit } else { workers };
    return if bounded > 1 { bounded as! i32 } else { 1 };
}

export fn i2_prefill_run_visit(work: u64, index: i64, device: I2DraftSpan, retain: bool,
    slot: u64, loop_enabled: bool, prelude: bool) {
    let decoder = i2d_read(work);
    let tokens = i2d_read(work + 8);
    let count = i2d_int(work + 32);
    let begin = i2d_int(work + 36);
    var state: u8[3];
    for field in 3 {
        state[field] = if i2d_read(device.address + (field as! u64) * 8) != 0 { 1 } else { 0 };
    }
    let base = i2d_address(unsafe_c_address<c_ptr<const void>>(state[0]));
    fg_i2_prefill_layer(decoder, tokens, i2d_read(work + 16), count, begin,
        decoder + I2_OPEN_VISITS + (index as! u64) * 32, i2d_read(work + 24), base, retain, slot,
        base + 1, base + 2, loop_enabled, prelude);
    for field in 3 { i2d_set(device.address + (field as! u64) * 8, state[field] as! u64); }
    if begin == 4096 && count > 1 && i2_prefill_env_pointer("I2_DRAFT_PARITY\0") != 0 {
        fg_i2_prefill_trace_visit(index as! i32, tokens + 285708);
    }
}

extern fn fg_i2_swap_direct_output(output: u64) -> u64;
extern fn fg_cuda_i2_prefill_conv(weights: u64, history: u64);
extern fn inkling_arena_residual_open(output: u64);
extern fn inkling_arena_depthmix_reads() -> u32;

fn i2_poison_prefill_rows(output: u64, count: i32) {
    for index in (count as! i64) * 2304 { i2d_set_int(output + (index as! u64) * 4, 0x7fc00000); }
}

export fn fg_i2_prefill_resident_attention(decoder: u64, core: u64, aux: u64,
    slot: i32, begin: i32, layer: i32, pass: i32, count: i32, sliding: bool, device_postconv: bool,
    direct_depthmix: bool, prune_host: bool, device_loop: bool, attention_mix: bool,
    inputs: u64, messages: u64, output: u64) {
    let rows: u64[3] = [inputs, inputs + 37748736, inputs + 75497472];
    let row_table = i2d_address(unsafe_c_address<c_ptr<const void>>(rows[0]));
    var saved_output: u64 = 0;
    if device_postconv {
        let history = i2d_read(decoder + I2D_HA) + (slot as! u64) * 27648;
        fg_cuda_i2_prefill_conv(i2_native_weight(decoder, aux, 2, 9216).address, history);
        saved_output = fg_i2_swap_direct_output(output);
        if direct_depthmix {
            if !prune_host { i2_poison_prefill_rows(output, count); }
            inkling_arena_residual_open(output);
        }
    }
    if device_loop {
        let q = i2d_read(messages);
        let k = i2d_read(messages + 8);
        let v = i2d_read(messages + 16);
        if !prune_host {
            i2_poison_prefill_rows(q, count);
            i2_poison_prefill_rows(k, count);
            i2_poison_prefill_rows(v, count);
        }
        inkling_loop_bind(i2_native_weight(decoder, aux, 17, 9216).address, q, k, v, 0);
    }
    fg_i2_host_attention_graph(decoder, core, aux, slot, begin, count, sliding,
        row_table, if pass > 0 { messages } else { 0 }, output);
    if device_loop {
        assert(inkling_loop_generated() == 7);
        inkling_loop_bind(0, 0, 0, 0, 0);
    }
    if attention_mix {
        let reads = inkling_arena_depthmix_reads();
        assert(reads != 0);
        let kind = if sliding { "sliding" } else { "full" };
        i2d_log(str("Inkling ", kind, " DepthMix begin=", begin, " layer=", layer, " pass=", pass, " reads=", reads, "\n"));
    }
    if device_postconv {
        fg_cuda_i2_prefill_conv(0, 0);
        fg_i2_swap_direct_output(saved_output);
    }
}
