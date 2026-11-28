const QV_GRAPH: u64 = 2392;
const QV_WEIGHTS: u64 = 2776;
const QV_WORK: u64 = 2808;
const QV_HOST: u64 = 2840;
const QV_LAYERS: u64 = 2872;
const QV_GLOBALS: u64 = 9784;
const QV_SCRATCH: u64 = 9856;
const QV_KEYS: u64 = 10240;
const QV_VALUES: u64 = 10816;
const QV_CONTEXT: u64 = 11392;
const QV_POSITION: u64 = 11396;
const QV_EPSILON: u64 = 11400;
const QV_THETA: u64 = 11404;
const QV_VOCAB: u32 = 151936;
const QV_HIDDEN: u32 = 896;
const QV_FFN: u32 = 4864;
const QV_KV: u32 = 128;
const QV_CHUNK: u64 = 4194304;

struct QvView { buffer: u64, offset: u64, size: u64 }
struct QvShape {
    width: u32, rows: u32, batch: u32, epsilon: f32, position: u32,
    kv_heads: u32, context: u32, theta: f32, rotary: u32
}
struct QvSpec { name: const u8[:], kind: i64, width: i64, rows: i64 }

extern fn gr_meta_f32_bits(data: VkGgufSlice, state: VkGgufSlice, meta: VkGgufSlice, id: i64, out: VkGgufSlice) -> i64;
extern fn gr_find_tensor(data: VkGgufSlice, state: VkGgufSlice, tensors: VkGgufSlice, slots: VkGgufSlice, key: VkGgufSlice) -> i64;
extern fn gr_tensor_info(state: VkGgufSlice, tensors: VkGgufSlice, id: i64, out: VkGgufSlice) -> i64;
extern fn fg_vk_open(vk: u64, index: u32, cpu: i32) -> i32;
extern fn fg_vk_close(vk: u64);
extern fn fg_vk_alloc_buffer(vk: u64, buffer: u64, size: u64, usage: u32, required: u32, preferred: u32) -> i32;
extern fn fg_vk_free_buffer(vk: u64, buffer: u64);
extern fn fg_vk_access_buffer(vk: u64, buffer: u64, offset: u64, data: u64, size: u64, write: i32) -> i32;
extern fn fg_vk_upload_open(upload: u64, vk: u64, capacity: u64) -> i32;
extern fn fg_vk_upload_close(upload: u64);
extern fn fg_vk_graph_open(graph: u64, vk: u64, capacity: u32) -> i32;
extern fn fg_vk_graph_close(graph: u64);
extern fn fg_vk_graph_begin(graph: u64) -> i32;
extern fn fg_vk_graph_submit(graph: u64) -> i32;
extern fn fg_vk_graph_record(graph: u64, op: u32, shape: QvShape, input: QvView, weight: QvView, output: QvView) -> i32;
extern fn fg_vk_graph_attention(graph: u64, shape: QvShape, query: QvView, keys: QvView, values: QvView, output: QvView) -> i32;
extern fn fg_vk_qwen2_forward(model: u64, layers: i64, output: i64) -> i64;

fn qv_align(size: u64, alignment: u64) -> u64 { return (size + alignment - 1) & (0 - alignment); }

fn qv_read_f32(address: u64) -> f32 {
    var value: f32 = 0.0 as! f32;
    vk_gguf_copy(unsafe_c_address<c_ptr<void>>(value), unsafe_c_ptr<c_ptr<const void>>(address), 4);
    return value;
}

fn qv_log(action: const u8[:], status: i32) {
    vk_http_error(str("Vulkan: ", action, " (", status, ")\n"));
}

fn qv_meta(header: u64, key: const u8[:], out: i64&, floating: bool) -> bool {
    let slice = vk_tok_slice(vk_tok_address(unsafe_c_address<c_ptr<const void>>(out)), 1);
    let id = vk_tok_meta(header, key);
    if floating { return gr_meta_f32_bits(vk_gguf_data(header), vk_gguf_state(header), vk_gguf_metadata(header), id, slice) == 0; }
    return gr_meta_int(vk_gguf_data(header), vk_gguf_state(header), vk_gguf_metadata(header), id, slice) == 0;
}

fn qv_meta_is(header: u64, key: const u8[:], expected: i64) -> bool {
    var value: i64 = 0;
    return qv_meta(header, key, value, false) && value == expected;
}

fn qv_validate(header: u64, model: u64, context: u32) -> bool {
    if !vk_tok_meta_is(header, "general.architecture", "qwen2") ||
        !qv_meta_is(header, "qwen2.block_count", 24) ||
        !qv_meta_is(header, "qwen2.embedding_length", 896) ||
        !qv_meta_is(header, "qwen2.feed_forward_length", 4864) ||
        !qv_meta_is(header, "qwen2.attention.head_count", 14) ||
        !qv_meta_is(header, "qwen2.attention.head_count_kv", 2) { return false; }
    var maximum: i64 = 0;
    var eps: i64 = 0;
    var theta: i64 = 0;
    if !qv_meta(header, "qwen2.context_length", maximum, false) || maximum < (context as! i64) ||
        !qv_meta(header, "qwen2.attention.layer_norm_rms_epsilon", eps, true) ||
        !qv_meta(header, "qwen2.rope.freq_base", theta, true) { return false; }
    vk_http_set_u32(model + QV_EPSILON, eps as! u32);
    vk_http_set_u32(model + QV_THETA, theta as! u32);
    return qv_read_f32(model + QV_EPSILON) == 0.000001 as! f32 &&
        qv_read_f32(model + QV_THETA) == 1000000.0 as! f32;
}

fn qv_bind(header: u64, spec: QvSpec, name: const u8[:], file_size: u64, align: u64,
        weights: u64[:], entry: i64, end: u64&) -> bool {
    let state = vk_gguf_state(header);
    let tensors = vk_tok_slice(header + VK_GGUF_TENSORS, 12288);
    let slots = vk_tok_slice(header + VK_GGUF_TENSOR_SLOTS, 2048);
    let key = vk_tok_slice(vk_tok_address(unsafe_c_slice_data<c_ptr<const void>>(name)), name.len);
    let index = gr_find_tensor(vk_gguf_data(header), state, tensors, slots, key);
    var info: i64[9];
    let output = vk_tok_slice(vk_tok_address(unsafe_c_address<c_ptr<const void>>(info[0])), 9);
    let dims: i64 = if spec.rows == 1 { 1 } else { 2 };
    if gr_tensor_info(state, tensors, index, output) != 0 || info[0] != spec.kind || info[1] != dims ||
        info[2] != spec.width || info[3] != spec.rows || info[7] <= 0 || info[8] < 0 ||
        (info[8] as! u64) > file_size || (info[7] as! u64) > file_size - (info[8] as! u64) {
        vk_http_error(str("Qwen2 Vulkan: unsupported or missing tensor ", name, "\n"));
        return false;
    }
    let offset = qv_align(end, align);
    weights[entry * 3] = info[8] as! u64;
    weights[entry * 3 + 1] = info[7] as! u64;
    weights[entry * 3 + 2] = offset;
    end = offset + qv_align(info[7] as! u64, 4);
    return true;
}

fn qv_layout(header: u64, model: u64, weights: u64[:], size: u64, end: u64&) -> bool {
    let global: QvSpec[3] = [
        QvSpec { name: "token_embd.weight", kind: 2, width: 896, rows: 151936 },
        QvSpec { name: "output_norm.weight", kind: 0, width: 896, rows: 1 },
        QvSpec { name: "output.weight", kind: 8, width: 896, rows: 151936 }
    ];
    let layer: QvSpec[12] = [
        QvSpec { name: "attn_norm.weight", kind: 0, width: 896, rows: 1 },
        QvSpec { name: "attn_q.weight", kind: 2, width: 896, rows: 896 },
        QvSpec { name: "attn_k.weight", kind: 2, width: 896, rows: 128 },
        QvSpec { name: "attn_v.weight", kind: 2, width: 896, rows: 128 },
        QvSpec { name: "attn_q.bias", kind: 0, width: 896, rows: 1 },
        QvSpec { name: "attn_k.bias", kind: 0, width: 128, rows: 1 },
        QvSpec { name: "attn_v.bias", kind: 0, width: 128, rows: 1 },
        QvSpec { name: "attn_output.weight", kind: 2, width: 896, rows: 896 },
        QvSpec { name: "ffn_norm.weight", kind: 0, width: 896, rows: 1 },
        QvSpec { name: "ffn_gate.weight", kind: 2, width: 896, rows: 4864 },
        QvSpec { name: "ffn_up.weight", kind: 2, width: 896, rows: 4864 },
        QvSpec { name: "ffn_down.weight", kind: 2, width: 4864, rows: 896 }
    ];
    let alignment = vk_gguf_read_word(model + 664);
    for i in global.len {
        if !qv_bind(header, global[i], global[i].name, size, alignment, weights, i, end) { return false; }
    }
    for l in 24 {
        for i in layer.len {
            if !qv_bind(header, layer[i], str("blk.", l, ".", layer[i].name),
                size, alignment, weights, 3 + l * 12 + i, end) { return false; }
        }
    }
    return true;
}

fn qv_upload(file: u64, model: u64, weights: const u64[:]) -> bool {
    var staging: u64[9] = [0; 9];
    let upload = vk_tok_address(unsafe_c_address<c_ptr<const void>>(staging[0]));
    let chunk = vk_tok_table(QV_CHUNK as! i64, 1).address;
    var ok = chunk != 0 && fg_vk_upload_open(upload, model, QV_CHUNK) != 0;
    if ok {
        for i in 291 {
            if fg_gguf_upload(file, upload, model + QV_WEIGHTS, weights[i*3+2],
                weights[i*3], weights[i*3+1], chunk) == 0 { ok = false; break; }
        }
    }
    fg_vk_upload_close(upload);
    vk_tok_release(chunk);
    return ok;
}

fn qv_view(address: u64) -> QvView {
    return QvView { buffer: vk_gguf_read_word(address), offset: vk_gguf_read_word(address + 8),
        size: vk_gguf_read_word(address + 16) };
}

fn qv_set_view(address: u64, buffer: u64, offset: u64, bytes: u64) {
    let view: u64[3] = [buffer, offset, bytes];
    vk_gguf_copy(unsafe_c_ptr<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(view[0]), 24);
}

fn qv_work_view(address: u64, buffer: u64, bytes: u64, align: u64, end: u64&) {
    let offset = qv_align(end, align);
    qv_set_view(address, buffer, offset, bytes);
    end = offset + bytes;
}

fn qv_work(model: u64, context: u32) -> bool {
    let alignment = vk_gguf_read_word(model + 664);
    var device_end: u64 = 0;
    var host_end: u64 = 0;
    let widths: u64[16] = [1,896,896,896,128,128,896,128,128,896,896,896,4864,4864,4864,151936];
    for i in widths.len {
        let target = model + QV_SCRATCH + (i as! u64) * 24;
        if i == 0 || i == 15 { qv_work_view(target, model + QV_HOST, widths[i] * 4, alignment, host_end); }
        else { qv_work_view(target, model + QV_WORK, widths[i] * 4, alignment, device_end); }
    }
    let kv_bytes = (context as! u64) * 512;
    for i in 24 {
        let off = (i as! u64) * 24;
        qv_work_view(model + QV_KEYS + off, model + QV_WORK, kv_bytes, alignment, device_end);
        qv_work_view(model + QV_VALUES + off, model + QV_WORK, kv_bytes, alignment, device_end);
    }
    var status = fg_vk_alloc_buffer(model, model + QV_WORK, device_end, 32, 1, 0);
    if status == 0 { status = fg_vk_alloc_buffer(model, model + QV_HOST, host_end, 32, 2, 4); }
    if status != 0 { qv_log("allocate Vulkan Qwen2 workspace", status); }
    return status == 0;
}

export fn fg_vk_qwen2_close(model: u64) {
    fg_vk_graph_close(model + QV_GRAPH);
    fg_vk_free_buffer(model, model + QV_WEIGHTS);
    fg_vk_free_buffer(model, model + QV_WORK);
    fg_vk_free_buffer(model, model + QV_HOST);
    fg_vk_close(model);
}

export fn fg_vk_qwen2_open(model: u64, path: u64, context: u32, index: u32, allow_cpu: i32) -> i32 {
    vk_gguf_clear(unsafe_c_ptr<c_ptr<void>>(model), 0, VK_HTTP_QWEN_BYTES);
    vk_http_set_u32(model + QV_CONTEXT, context);
    if context == 0 || context > 32768 { return 0; }
    let mode: u8[3] = [114,98,0];
    let file = fg_vk_tok_fopen(path, vk_tok_address(unsafe_c_address<c_ptr<const void>>(mode[0])));
    if file == 0 { fg_vk_tok_perror(path); return 0; }
    let header = vk_tok_table(1, TK_HEADER_BYTES).address;
    var ok = false;
    var size: u64 = 0;
    var weights: u64[873];
    if header != 0 {
        let file_size = fg_gguf_read_header(header, file);
        ok = file_size >= 0 && qv_validate(header, model, context);
        if !ok { vk_http_error("Qwen2 Vulkan: invalid or incompatible GGUF metadata\n"); }
        if ok { ok = fg_vk_open(model, index, allow_cpu) != 0 && qv_layout(header, model, weights, file_size as! u64, size); }
        vk_tok_release(vk_gguf_read_word(header));
        vk_tok_release(header);
    }
    if ok {
        let result = fg_vk_alloc_buffer(model, model + QV_WEIGHTS, size, 34, 1, 0);
        if result != 0 { qv_log("allocate Vulkan model weights", result); }
        ok = result == 0 && qv_upload(file, model, weights);
    }
    fg_vk_tok_fclose(file);
    if ok { ok = qv_work(model, context) && fg_vk_graph_open(model + QV_GRAPH, model, 512) != 0; }
    if !ok { fg_vk_qwen2_close(model); return 0; }
    for i in 291 {
        let address = if i < 3 { model + QV_GLOBALS + (i as! u64) * 24 }
            else { model + QV_LAYERS + ((i - 3) as! u64) * 24 };
        qv_set_view(address, model + QV_WEIGHTS, weights[i*3+2], qv_align(weights[i*3+1], 4));
    }
    return 1;
}

fn qv_shape(width: u32, rows: u32) -> QvShape {
    return QvShape { width: width, rows: rows, batch: 1, epsilon: 0.0 as! f32, position: 0,
        kv_heads: 0, context: 0, theta: 0.0 as! f32, rotary: 0 };
}

fn qv_at(base: u64, index: u64) -> QvView { return qv_view(base + index * 24); }

fn qv_record(model: u64, op: u32, width: u32, rows: u32, input: u64, weight: QvView, output: u64) -> bool {
    var shape = qv_shape(width, rows);
    shape.epsilon = qv_read_f32(model + QV_EPSILON);
    shape.theta = qv_read_f32(model + QV_THETA);
    if op == 3 || op == 4 { shape.position = vk_http_u32(model + QV_POSITION); }
    if op == 4 { shape.context = vk_http_u32(model + QV_CONTEXT); }
    return fg_vk_graph_record(model + QV_GRAPH, op, shape, qv_at(model + QV_SCRATCH, input),
        weight, qv_at(model + QV_SCRATCH, output)) != 0;
}

fn qv_kv_store(model: u64, input: QvView, dest: QvView) -> bool {
    var shape = qv_shape(64, 2);
    shape.position = vk_http_u32(model + QV_POSITION);
    shape.context = vk_http_u32(model + QV_CONTEXT);
    return fg_vk_graph_record(model + QV_GRAPH, 4, shape, input, input, dest) != 0;
}

fn qv_logits(model: u64) -> bool {
    var start: u32 = 0;
    while start < QV_VOCAB {
        let remaining = QV_VOCAB - start;
        let count: u32 = if remaining < 32768 { remaining } else { 32768 };
        var shape = qv_shape(QV_HIDDEN, count);
        shape.position = start;
        shape.context = QV_VOCAB;
        var stripe = qv_at(model + QV_GLOBALS, 2);
        stripe.offset += (start as! u64) * 952;
        stripe.size = qv_align((count as! u64) * 952, 4);
        if fg_vk_graph_record(model + QV_GRAPH, 6, shape, qv_at(model + QV_SCRATCH, 2), stripe,
            qv_at(model + QV_SCRATCH, 15)) == 0 { return false; }
        start += count;
    }
    return true;
}

fn qv_operation(m: u64, op: i64, layer: u64) -> bool {
    let w = m + QV_LAYERS + layer * 288;
    let s = m + QV_SCRATCH;
    let h = QV_HIDDEN;
    let kv = QV_KV;
    let keys = qv_at(m + QV_KEYS, layer);
    let values = qv_at(m + QV_VALUES, layer);
    if op == 0 { return qv_record(m, 7, h, QV_VOCAB, 0, qv_at(m + QV_GLOBALS, 0), 1); }
    if op == 1 { return qv_record(m, 0, h, 0, 1, qv_at(w, 0), 2); }
    if op == 2 {
        return qv_record(m, 1, h, h, 2, qv_at(w, 1), 3) &&
            qv_record(m, 1, h, kv, 2, qv_at(w, 2), 4) && qv_record(m, 1, h, kv, 2, qv_at(w, 3), 5) &&
            qv_record(m, 8, h, 0, 3, qv_at(w, 4), 6) && qv_record(m, 8, kv, 0, 4, qv_at(w, 5), 7) &&
            qv_record(m, 8, kv, 0, 5, qv_at(w, 6), 8);
    }
    if op == 3 {
        return qv_record(m, 3, 64, 14, 6, qv_at(s, 6), 3) && qv_record(m, 3, 64, 2, 7, qv_at(s, 7), 4) &&
            qv_kv_store(m, qv_at(s, 4), keys) && qv_kv_store(m, qv_at(s, 8), values);
    }
    if op == 4 {
        var attention = qv_shape(64, 14);
        attention.position = vk_http_u32(m + QV_POSITION);
        attention.context = vk_http_u32(m + QV_CONTEXT);
        attention.kv_heads = 2;
        return fg_vk_graph_attention(m + QV_GRAPH, attention, qv_at(s, 3), keys, values, qv_at(s, 9)) != 0 &&
            qv_record(m, 1, h, h, 9, qv_at(w, 7), 10) && qv_record(m, 9, h, 0, 1, qv_at(s, 10), 11);
    }
    if op == 5 { return qv_record(m, 0, h, 0, 11, qv_at(w, 8), 2); }
    if op == 6 {
        return qv_record(m, 1, h, QV_FFN, 2, qv_at(w, 9), 12) &&
            qv_record(m, 1, h, QV_FFN, 2, qv_at(w, 10), 13) &&
            qv_record(m, 2, QV_FFN, 0, 12, qv_at(s, 13), 14);
    }
    if op == 7 {
        return qv_record(m, 1, QV_FFN, h, 14, qv_at(w, 11), 10) && qv_record(m, 9, h, 0, 11, qv_at(s, 10), 1);
    }
    if op == 8 { return qv_record(m, 0, h, 0, 1, qv_at(m + QV_GLOBALS, 1), 2) && qv_logits(m); }
    return false;
}

export fn fg_vk_qwen_op(model: u64, op: i64, layer: i64) -> i64 {
    if model == 0 || layer < 0 || layer >= 24 { return -1; }
    return if qv_operation(model, op, layer as! u64) { 0 } else { -1 };
}

export fn fg_vk_qwen2_step(model: u64, id: u32, logits: u64) -> i32 {
    let position = vk_http_u32(model + QV_POSITION);
    if id >= QV_VOCAB || position >= vk_http_u32(model + QV_CONTEXT) { return 0; }
    let scratch = model + QV_SCRATCH;
    let graph = model + QV_GRAPH;
    if fg_vk_access_buffer(model, model + QV_HOST, qv_at(scratch, 0).offset,
        vk_tok_address(unsafe_c_address<c_ptr<const void>>(id)), 4, 1) != 0 || fg_vk_graph_begin(graph) == 0 { return 0; }
    let result = fg_vk_qwen2_forward(model, 24, if logits != 0 { 1 } else { 0 });
    if result != 0 {
        vk_http_error(str("Qwen2 Vulkan: graph record failed (", result, ")\n"));
        return 0;
    }
    let output = qv_at(scratch, 15);
    if fg_vk_graph_submit(graph) == 0 || (logits != 0 &&
        fg_vk_access_buffer(model, model + QV_HOST, output.offset, logits, output.size, 0) != 0) { return 0; }
    vk_http_set_u32(model + QV_POSITION, position + 1);
    return 1;
}

export fn fg_vk_choose(logits: u64, vocabulary: u32, ignore_eog: u64, chosen: u64) -> i32 {
    var best: u32 = 0xffffffff;
    var maximum: f32 = 0.0 as! f32;
    for i in (vocabulary as! i64) {
        let id = i as! u32;
        let at = logits + (i as! u64) * 4;
        if (vk_http_u32(at) & 0x7f800000) == 0x7f800000 { return 0; }
        let value = qv_read_f32(at);
        if (best == 0xffffffff || value > maximum) && fg_vk_tokenizer_is_eog(ignore_eog, id) == 0 {
            best = id;
            maximum = value;
        }
    }
    if best == 0xffffffff { return 0; }
    vk_http_set_u32(chosen, best);
    return 1;
}
