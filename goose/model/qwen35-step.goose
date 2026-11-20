extern "ggml_tensor_overhead" fn q35_tensor_overhead() -> u64;
extern "ggml_graph_overhead_custom" fn q35_graph_overhead(size: u64, grads: bool) -> u64;
extern "ggml_new_graph_custom" fn q35_graph_new(ctx: i64, size: u64, grads: bool) -> i64;
extern "ggml_build_forward_expand" fn q35_graph_append(graph: i64, root: i64);
extern "fg_standalone_graph_alloc" fn q35_graph_allocate(graph: i64) -> u64;
extern "fg_standalone_graph_alloc_output" fn q35_graph_allocate_output(graph: i64, output: i64) -> u64;
extern "fg_standalone_graph_compute_output" fn q35_graph_execute(graph: i64, output: i64) -> i32;
extern "fg_q35_release_graph" fn q35_graph_free(storage: u64);
extern "fg_q35_write_ids" fn q35_write_ids(data: u64, tokens: const i32[:]);
extern "fg_q35_read_logits" fn q35_read_logits(data: u64, output: f32[:]);
extern "fg457_st_i64" fn q35_store_index(data: u64, byte: u64, value: i64);

// Keep the graph builders on their existing concrete slice ABI.
extern "q35_trunk_output" fn q35_step_trunk(c: i64, ids: i64, embedding: i64, head: i64, norm: i64,
    layers: i64[:], layer_count: i64, positions: i64, mask: i64, indices: i64,
    attn: i64[:], gdn: i64[:], moe: i64[:], sections: i64, state: i64[:], ticket: i64, work: i64[:],
    branch: i64[:], expert: i64[:], frames: i64[:], roots: i64[:], logit_rows: i64) -> i64;

fn q35_step_inputs(tokens: const i32[:], ids: i64, positions: i64, indices: i64, prefix: i64) -> bool {
    let token_data = qa_get(ids, 4, 0);
    let rope_data = qa_get(positions, 4, 0);
    let row_data = qa_get(indices, 4, 0);
    if token_data == 0 || rope_data == 0 || row_data == 0 { return false; }
    q35_write_ids(token_data, tokens);
    for i in tokens.len {
        let position = q35_kv_position(prefix, i, 0);
        q35_store_index(row_data, (8 * i) as u64, position);
        for plane in 4 {
            sl_write(rope_data, plane * tokens.len + i, q35_kv_position(prefix, i, plane));
        }
    }
    return true;
}

fn q35_step_execute(c: i64, graph_size: i64, handles: const i64[:], layers: i64[:],
                    attn: i64[:], gdn: i64[:], moe: i64[:], state: i64[:], ticket: i64,
                    tokens: const i32[:], logit_rows: i64, logits: f32[:], executed: bool &) -> i64 {
    let count = tokens.len;
    let ids = fresh(c, 26, count, 1, 1, 1);
    let positions = fresh(c, 26, count * 4, 1, 1, 1);
    let indices = fresh(c, 27, count, 1, 1, 1);
    let layer_count = layers.len / 32;
    var work: i64[16] = [0; 16];
    var branch: i64[40] = [0; 40];
    var expert: i64[154] = [0; 154];
    var frames: i64[516] = [0; 516];
    var roots: i64[258] = [0; 258];
    let result = q35_step_trunk(c, ids, handles[0], handles[1], handles[2], layers, layer_count,
        positions, 0, indices, attn, gdn, moe, handles[3], state, ticket, work[..], branch[..], expert[..],
        frames[0 .. 4 * layer_count + 4], roots[0 .. 2 * layer_count + 2], logit_rows);
    if result == 0 || (logit_rows != 0 && logits.len != pred(13, result, 0)) { return -1; }
    let graph = q35_graph_new(c, graph_size as u64, false);
    if graph == 0 { return -1; }
    for i in roots[0] { q35_graph_append(graph, roots[i + 1]); }
    let selected = if logit_rows != 0 { result } else { 0 };
    let storage = if logit_rows < 0 { q35_graph_allocate(graph) } else { q35_graph_allocate_output(graph, selected) };
    if storage == 0 { return -1; }
    var outcome: i64 = -1;
    if q35_step_inputs(tokens, ids, positions, indices, state[2]) {
        executed = true;
        if q35_graph_execute(graph, selected) == 0 && q35_kv_commit(state, ticket) >= 0 {
            if selected != 0 { q35_read_logits(qa_get(selected, 4, 0), logits); }
            outcome = logits.len;
        }
    }
    q35_graph_free(storage);
    return outcome;
}

// The frame is borrowed from the model owner for precisely one KV ticket.
// -2 signals that partial execution could not be recovered by a state reset.
export fn q35_model_step(owner: i64[:], handles: const i64[:], layers: i64[:],
                         attn: i64[:], gdn: i64[:], moe: i64[:], state: i64[:],
                         tokens: const i32[:], logit_rows: i64, logits: f32[:]) -> i64 {
    if owner.len < 13 || owner[0] != 9191 || owner[5] <= 0 || owner[5] > 128 || owner[8] <= 0 ||
       handles.len < 4 || layers.len < owner[5] * 32 || moe.len < 3 || moe[1] <= 0 || moe[1] > 64 ||
       tokens.len > 4096 || logit_rows < -1 || logit_rows > 1 ||
       (logit_rows == 0 && logits.len != 0) || q35_kv_read_rows(state, tokens.len) <= 0 { return -1; }
    for i in tokens.len {
        if tokens[i] < 0 || (tokens[i] as i64) >= owner[8] { return -1; }
    }
    // Bound every layer's graph nodes, new tensor headers and external leaves.
    // The expert views and their reductions scale with the selected count.
    let graph_size = 128 + owner[5] * (128 + 4 * moe[1]);
    let header_bytes = (q35_graph_overhead(graph_size as u64, false) +
        (graph_size as u64) * q35_tensor_overhead()) as i64;
    var frame: i64[4] = [0; 4];
    let c = qr_frame_open_mode(owner, header_bytes, frame[..], 1);
    if c == 0 { return -1; }
    let ticket = q35_kv_begin(state, tokens.len);
    var outcome: i64 = -1;
    if ticket > 0 {
        var executed = false;
        let records = layers[0 .. owner[5] * 32];
        outcome = q35_step_execute(c, graph_size, handles, records, attn, gdn, moe, state, ticket,
            tokens, logit_rows, logits, executed);
        if outcome < 0 {
            if executed {
                if qr_reset_state(owner, records, state) != 0 { outcome = -2; }
            } else if q35_kv_abort(state, ticket) < 0 { outcome = -2; }
        }
    }
    if qr_frame_close(owner, frame[..]) != 0 { return -2; }
    return outcome;
}
