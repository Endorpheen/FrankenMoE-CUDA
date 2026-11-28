const BV_BYTES: u64 = 42128;
const BV_RESET: u64 = 2776;
const BV_LAYERS: u64 = 2848;
const BV_GLOBALS: u64 = 40736;
const BV_STATE: u64 = 40864;
const BV_SCRATCH: u64 = 40896;
const BV_HOST: u64 = 40928;
const BV_SIGNS: u64 = 40960;
const BV_EXTRA: u64 = 41032;
const BV_VIEWS: u64 = 41200;
const BV_CAPACITY: u64 = 41584;
const BV_POSITION: u64 = 41588;
const BV_TOKEN: u64 = 41592;
const BV_EPSILON: u64 = 41596;
const BV_FAILED: u64 = 41600;
const BV_EOG: u64 = 41608;
const BV_EOG_COUNT: u64 = 42120;
const BV_SLOT_BYTES: u64 = 592;
const BV_SLOT_BUFFER: u64 = 8;
const BV_SLOT_LAYER: u64 = 40;
const BV_HIDDEN: u32 = 5120;
const BV_ROW_BYTES: u64 = 1360;
const BV_VOCAB: u32 = 248320;
const BV_CHUNK: u64 = 4194304;

extern fn gr_arr_str_offsets(data: VkGgufSlice, state: VkGgufSlice, meta: VkGgufSlice,
    id: i64, offsets: VkGgufSlice, lengths: VkGgufSlice) -> i64;
extern fn bonsai_bind_layout(data: VkGgufSlice, state: VkGgufSlice, meta: VkGgufSlice,
    tensors: VkGgufSlice, meta_slots: VkGgufSlice, tensor_slots: VkGgufSlice,
    global: VkGgufSlice, layers: VkGgufSlice, mtp: VkGgufSlice) -> i64;
extern fn fg_vk_upload_zero(upload: u64, dest: u64) -> i32;
extern fn fg_vk_bonsai_attention_layer(graph: u64, layer: u64, scratch: u64,
    input: QvView, output: QvView, epsilon: f32, position: u32, capacity: u32) -> i32;
extern fn fg_vk_bonsai_recurrent_layer(graph: u64, layer: u64, scratch: u64,
    input: QvView, output: QvView, epsilon: f32) -> i32;
extern fn fg_vk_bonsai_forward(model: u64, logits: i64) -> i64;

struct BvLoader { file: u64, header: u64, upload: u64, chunk: u64, alignment: u64 }
struct BvPlan { end: u64, count: i64, parts: u64[69] }

fn bv_layer(model: u64, index: i64) -> u64 { return model + BV_LAYERS + (index as! u64) * BV_SLOT_BYTES; }

fn bv_alloc(model: u64, buffer: u64, bytes: u64, host: bool, transfer: bool) -> bool {
    let status = fg_vk_alloc_buffer(model, buffer, bytes, if transfer { 34 } else { 32 },
        if host { 2 } else { 1 }, if host { 4 } else { 0 });
    if status != 0 { qv_log("allocate Bonsai Vulkan buffer", status); }
    return status == 0;
}

fn bv_plan_part(header: u64, id: i64, plan: BvPlan&, start: u64) -> i64 {
    var info: i64[9];
    let span = vk_tok_slice(vk_tok_address(unsafe_c_address<c_ptr<const void>>(info[0])), 9);
    if gr_tensor_info(vk_gguf_state(header), vk_tok_slice(header + VK_GGUF_TENSORS, 12288), id, span) != 0 ||
        info[7] <= 0 || info[7] % 4 != 0 || info[8] < 0 { return -1; }
    let at = plan.count * 3;
    plan.parts[at] = info[8] as! u64;
    plan.parts[at + 1] = info[7] as! u64;
    plan.parts[at + 2] = start;
    plan.count += 1;
    return info[7];
}

fn bv_plan_tensor(loader: BvLoader, plan: BvPlan&, buffer: u64, view: u64, first: i64, second: i64) -> bool {
    let parts: i64 = if second >= 0 { 2 } else { 1 };
    if plan.count + parts > 23 { return false; }
    let start = qv_align(plan.end, loader.alignment);
    let a = bv_plan_part(loader.header, first, plan, start);
    if a < 0 { return false; }
    var bytes = a as! u64;
    if second >= 0 {
        let b = bv_plan_part(loader.header, second, plan, start + bytes);
        if b < 0 { return false; }
        bytes += b as! u64;
    }
    qv_set_view(view, buffer, start, bytes);
    plan.end = start + bytes;
    return true;
}

fn bv_materialize(model: u64, loader: BvLoader, buffer: u64, plan: BvPlan&) -> bool {
    if !bv_alloc(model, buffer, plan.end, false, true) { return false; }
    for i in plan.count {
        let at = i * 3;
        if fg_gguf_upload(loader.file, loader.upload, buffer, plan.parts[at+2],
            plan.parts[at], plan.parts[at+1], loader.chunk) == 0 { return false; }
    }
    return true;
}

fn bv_load_layer(model: u64, loader: BvLoader, slot: u64, t: const i64[:]) -> bool {
    var plan = BvPlan { end: 0, count: 0, parts: [0;69] };
    if t[0] != 0 && t[0] != 1 { return false; }
    let attention = t[0] == 0;
    vk_http_set_u32(slot, if attention { 1 } else { 0 });
    let w = slot + BV_SLOT_LAYER;
    let buffer = slot + BV_SLOT_BUFFER;
    for i in 3 {
        let sign = qv_at(model + BV_SIGNS, i as! u64);
        qv_set_view(w + (i as! u64) * 24, sign.buffer, sign.offset, sign.size);
    }
    if !bv_plan_tensor(loader, plan, buffer, w + 72, t[1], -1) ||
        !bv_plan_tensor(loader, plan, buffer, w + 264, t[if attention { 8 } else { 11 }], -1) { return false; }
    for i in 3 {
        if !bv_plan_tensor(loader, plan, buffer, w + ((12+i) as! u64)*24, t[12+i], -1) { return false; }
    }
    if attention {
        let source: i64[6] = [2,3,4,6,7,5];
        for i in source.len {
            if !bv_plan_tensor(loader, plan, buffer, w + 360 + (i as! u64)*24, t[source[i]], -1) { return false; }
        }
    } else {
        let source: i64[7] = [2,3,5,6,8,9,10];
        let second: i64[7] = [-1,-1,4,7,-1,-1,-1];
        for i in source.len {
            let paired: i64 = if second[i] < 0 { -1 } else { t[second[i]] };
            if !bv_plan_tensor(loader, plan, buffer, w + ((4+i) as! u64)*24, t[source[i]], paired) { return false; }
        }
    }
    return bv_materialize(model, loader, buffer, plan);
}

fn bv_load_eog(header: u64, model: u64) -> bool {
    var control: i64[3] = [-1,-1,-1];
    let suffix: (const u8[:])[3] = ["eos","eot","eom"];
    for i in 3 {
        let name = str("tokenizer.ggml.", suffix[i], "_token_id");
        if vk_tok_meta(header,name) >= 0 && !qv_meta(header,name,control[i],false) { return false; }
    }
    let data = vk_gguf_data(header);
    let words = vk_tok_table(496640, 8).address;
    if words == 0 { return false; }
    let offsets = vk_tok_slice(words, 248320);
    let lengths = vk_tok_slice(words + 1986560, 248320);
    let count = gr_arr_str_offsets(data,vk_gguf_state(header),vk_gguf_metadata(header),
        vk_tok_meta(header,"tokenizer.ggml.tokens"),offsets,lengths);
    var eog: i64 = 0;
    if count == 248320 {
        eog = bt_eog_ids(data, offsets, lengths, control[0], control[1], control[2], vk_tok_slice(model + BV_EOG, 64));
    }
    vk_tok_release(words);
    vk_gguf_write_word(model + BV_EOG_COUNT, eog as! u64);
    return eog > 0;
}

fn bv_meta(header: u64, model: u64, signs: i32[:]) -> bool {
    let names: (const u8[:])[12] = [
        "qwen35.embedding_length", "qwen35.feed_forward_length", "qwen35.attention.head_count",
        "qwen35.attention.head_count_kv", "qwen35.attention.key_length", "qwen35.attention.value_length",
        "qwen35.rope.dimension_count", "qwen35.ssm.conv_kernel", "qwen35.ssm.state_size", "qwen35.ssm.group_count",
        "qwen35.ssm.time_step_rank", "qwen35.ssm.inner_size"];
    let expected: i64[12] = [5120,17408,24,4,256,256,64,4,128,16,48,6144];
    for i in 12 { if !qv_meta_is(header,names[i],expected[i]) { return false; } }
    var eps: i64 = 0;
    var theta: i64 = 0;
    var maximum: i64 = 0;
    if !qv_meta(header,"qwen35.attention.layer_norm_rms_epsilon",eps,true) ||
        !qv_meta(header,"qwen35.rope.freq_base",theta,true) ||
        !qv_meta(header,"qwen35.context_length",maximum,false) ||
        maximum < (vk_http_u32(model + BV_CAPACITY) as! i64) { return false; }
    vk_http_set_u32(model + BV_EPSILON, eps as! u32);
    if qv_read_f32(model + BV_EPSILON) != 0.000001 as! f32 || (theta as! u32) != 0x4b189680 { return false; }
    let stride = vk_tok_meta(header,"prism.hadamard.sign_widths");
    let source = vk_tok_meta(header,"prism.hadamard.sign_values");
    let data = vk_gguf_data(header);
    let state = vk_gguf_state(header);
    let meta = vk_gguf_metadata(header);
    if gr_arr_n(state,meta,source) != signs.len || gr_arr_n(state,meta,stride) != 3 { return false; }
    let widths: i64[3] = [5120,6144,17408];
    var value: i64 = 0;
    let out = vk_tok_slice(vk_tok_address(unsafe_c_address<c_ptr<const void>>(value)),1);
    for i in 3 {
        if gr_array_int(data,state,meta,stride,i,out) != 0 || value != widths[i] { return false; }
    }
    for i in signs.len {
        if gr_array_int(data,state,meta,source,i,out) != 0 || (value != -1 && value != 1) { return false; }
        signs[i] = value as! i32;
    }
    return bv_load_eog(header,model);
}

fn bv_weight_data(model: u64, loader: BvLoader) -> bool {
    let header = loader.header;
    var signs: i32[28672];
    var globals: i64[3];
    var layers: i64[975];
    var mtp: i64[4];
    if fg_gguf_read_header(header,loader.file) < 0 || bonsai_bind_layout(vk_gguf_data(header),
        vk_gguf_state(header),vk_gguf_metadata(header),vk_tok_slice(header + VK_GGUF_TENSORS,12288),
        vk_gguf_meta_slots(header),vk_tok_slice(header + VK_GGUF_TENSOR_SLOTS,2048),
        vk_tok_slice(vk_tok_address(unsafe_c_address<c_ptr<const void>>(globals[0])),3),
        vk_tok_slice(vk_tok_address(unsafe_c_address<c_ptr<const void>>(layers[0])),975),
        vk_tok_slice(vk_tok_address(unsafe_c_address<c_ptr<const void>>(mtp[0])),4)) != 64 || !bv_meta(header,model,signs) {
        vk_http_error("Bonsai Vulkan: incompatible GGUF layout\n");
        return false;
    }
    let buffers = model + BV_GLOBALS;
    for i in 3 {
        var plan = BvPlan { end: 0, count: 0, parts: [0;69] };
        var view: u64[3];
        let buffer = buffers + (i as! u64) * 32;
        if !bv_plan_tensor(loader,plan,buffer,vk_tok_address(unsafe_c_address<c_ptr<const void>>(view[0])),globals[i],-1) ||
            !bv_materialize(model,loader,buffer,plan) { return false; }
    }
    let counts: u64[3] = [5120,6144,17408];
    var end: u64 = 0;
    let buffer = buffers + 96;
    for i in 3 {
        qv_work_view(model + BV_SIGNS + (i as! u64) * 24,buffer,counts[i]*4,loader.alignment,end);
    }
    if !bv_alloc(model,buffer,end,false,true) { return false; }
    var at: i64 = 0;
    for i in 3 {
        let part = qv_at(model + BV_SIGNS,i as! u64);
        if fg_vk_upload_write(loader.upload,buffer,part.offset,
            vk_tok_address(unsafe_c_address<c_ptr<const void>>(signs[at])),counts[i]*4) == 0 { return false; }
        at += counts[i] as! i64;
    }
    for i in 64 {
        if !bv_load_layer(model,loader,bv_layer(model,i),layers[i*15 .. i*15+15]) { return false; }
    }
    return true;
}

fn bv_load_weights(model: u64, file: u64, upload: u64) -> bool {
    let header = vk_tok_table(1,TK_HEADER_BYTES).address;
    let chunk = vk_tok_table(BV_CHUNK as! i64,1).address;
    let loader = BvLoader { file: file, header: header, upload: upload,
        chunk: chunk, alignment: vk_gguf_read_word(model+664) };
    let ok = header != 0 && chunk != 0 && bv_weight_data(model,loader);
    if header != 0 { vk_tok_release(vk_gguf_read_word(header)); }
    vk_tok_release(header);
    vk_tok_release(chunk);
    return ok;
}

fn bv_work(model: u64, upload: u64) -> bool {
    let align = vk_gguf_read_word(model+664);
    var scratch: u64 = 0;
    var state: u64 = 0;
    var host: u64 = 0;
    let widths: u64[16] = [5120,5120,12288,6144,1024,10240,10240,6144,6144,6144,5120,5120,17408,17408,17408,17408];
    for i in widths.len {
        qv_work_view(model+BV_VIEWS+(i as! u64)*24,model+BV_SCRATCH,widths[i]*4,align,scratch);
    }
    for i in 5 {
        qv_work_view(model+BV_EXTRA+(i as! u64)*24,model+BV_SCRATCH,20480,align,scratch);
    }
    qv_work_view(model+BV_EXTRA+120,model+BV_HOST,4,align,host);
    qv_work_view(model+BV_EXTRA+144,model+BV_HOST,993280,align,host);
    for i in 64 {
        let slot = bv_layer(model,i);
        let layer = slot+BV_SLOT_LAYER;
        if vk_http_u32(slot) != 0 {
            let bytes = (vk_http_u32(model+BV_CAPACITY) as! u64)*576;
            qv_work_view(layer+504,model+BV_STATE,bytes,align,state);
            qv_work_view(layer+528,model+BV_STATE,bytes,align,state);
        } else {
            qv_work_view(layer+360,model+BV_STATE,122880,align,state);
            qv_work_view(layer+384,model+BV_STATE,3145728,align,state);
        }
    }
    return bv_alloc(model,model+BV_SCRATCH,scratch,false,false) &&
        bv_alloc(model,model+BV_STATE,state,false,true) && bv_alloc(model,model+BV_HOST,host,true,false) &&
        fg_vk_upload_zero(upload,model+BV_STATE) != 0;
}

export fn fg_vk_bonsai_close(model: u64) {
    if model == 0 { return; }
    fg_vk_graph_close(model+QV_GRAPH);
    fg_vk_upload_close(model+BV_RESET);
    for i in 64 { fg_vk_free_buffer(model,bv_layer(model,i)+BV_SLOT_BUFFER); }
    for i in 4 { fg_vk_free_buffer(model,model+BV_GLOBALS+(i as! u64)*32); }
    fg_vk_free_buffer(model,model+BV_STATE);
    fg_vk_free_buffer(model,model+BV_SCRATCH);
    fg_vk_free_buffer(model,model+BV_HOST);
    fg_vk_close(model);
    vk_tok_release(model);
}

export fn fg_vk_bonsai_open(path: u64, capacity: u32, device: u32, allow_cpu: i32) -> u64 {
    if capacity == 0 || capacity > 32768 { return 0; }
    let mode: u8[3] = [114,98,0];
    let file = fg_vk_tok_fopen(path,vk_tok_address(unsafe_c_address<c_ptr<const void>>(mode[0])));
    if file == 0 { fg_vk_tok_perror(path); return 0; }
    let model = vk_tok_table(1,BV_BYTES).address;
    if model != 0 { vk_http_set_u32(model+BV_CAPACITY,capacity); }
    var storage: u64[9] = [0;9];
    let upload = vk_tok_address(unsafe_c_address<c_ptr<const void>>(storage[0]));
    let ok = model != 0 && fg_vk_open(model,device,allow_cpu) != 0 &&
        fg_vk_upload_open(upload,model,BV_CHUNK) != 0 && bv_load_weights(model,file,upload) &&
        bv_work(model,upload) && fg_vk_graph_open(model+QV_GRAPH,model,1696) != 0 &&
        fg_vk_upload_open(model+BV_RESET,model,4) != 0;
    fg_vk_upload_close(upload);
    fg_vk_tok_fclose(file);
    if !ok { fg_vk_bonsai_close(model); return 0; }
    var name: u8[>..] = [];
    for i in 256 {
        let byte = vk_gguf_read_byte(model+60+(i as! u64));
        if byte == 0 { break; }
        name.push(byte);
    }
    vk_http_error(str("Vulkan Bonsai on ",name,", context=",capacity,", MTP off\n"));
    return model;
}

export fn fg_vk_bonsai_op(model: u64, op: i64, layer: i64) -> i64 {
    let graph = model+QV_GRAPH;
    let views = model+BV_EXTRA;
    let epsilon = qv_read_f32(model+BV_EPSILON);
    let signs = qv_at(model+BV_SIGNS,0);
    let g = model+BV_GLOBALS;
    if op == 0 {
        let first = vk_http_u32(model+BV_TOKEN)/16*16;
        var gather = qv_shape(BV_HIDDEN,16);
        gather.position = first;
        let stripe = QvView { buffer: g, offset: (first as! u64)*BV_ROW_BYTES, size: 16*BV_ROW_BYTES };
        var inverse = qv_shape(BV_HIDDEN,0);
        inverse.position = 1;
        let ok = fg_vk_graph_record(graph,11,gather,qv_at(views,5),stripe,qv_at(views,0)) != 0 &&
            fg_vk_graph_record(graph,12,inverse,qv_at(views,0),signs,qv_at(views,1)) != 0;
        return if ok { 0 } else { -1 };
    }
    if op == 1 && layer >= 0 && layer < 64 {
        let slot = bv_layer(model,layer);
        let i = (layer & 1) as! u64;
        let input = qv_at(views,1+i);
        let output = qv_at(views,2-i);
        let p = slot+BV_SLOT_LAYER;
        let ok = if vk_http_u32(slot) != 0 { fg_vk_bonsai_attention_layer(graph,p,model+BV_VIEWS,
            input,output,epsilon,vk_http_u32(model+BV_POSITION),vk_http_u32(model+BV_CAPACITY)) }
            else { fg_vk_bonsai_recurrent_layer(graph,p,model+BV_VIEWS,input,output,epsilon) };
        return if ok != 0 { 0 } else { -1 };
    }
    if op != 2 { return -1; }
    var shape = qv_shape(BV_HIDDEN,0);
    shape.epsilon = epsilon;
    let weight = QvView { buffer: g+32, offset: 0, size: 20480 };
    if fg_vk_graph_record(graph,0,shape,qv_at(views,1),weight,qv_at(views,3)) == 0 ||
        fg_vk_graph_record(graph,12,shape,qv_at(views,3),signs,qv_at(views,4)) == 0 { return -1; }
    var first: u32 = 0;
    while first < BV_VOCAB {
        let remain = BV_VOCAB-first;
        let rows: u32 = if remain < 32768 { remain } else { 32768 };
        let stripe = QvView { buffer: g+64, offset: (first as! u64)*BV_ROW_BYTES, size: (rows as! u64)*BV_ROW_BYTES };
        var mm = qv_shape(BV_HIDDEN,rows);
        mm.position = first;
        mm.context = BV_VOCAB;
        if fg_vk_graph_record(graph,10,mm,qv_at(views,4),stripe,qv_at(views,6)) == 0 { return -1; }
        first += rows;
    }
    return 0;
}

export fn fg_vk_bonsai_step(model: u64, token: u32, logits: u64) -> i32 {
    let position = vk_http_u32(model+BV_POSITION);
    if vk_http_u32(model+BV_FAILED) != 0 || token >= BV_VOCAB || position >= vk_http_u32(model+BV_CAPACITY) { return 0; }
    vk_http_set_u32(model+BV_TOKEN,token);
    let graph = model+QV_GRAPH;
    let id = qv_at(model+BV_EXTRA,5);
    let output = qv_at(model+BV_EXTRA,6);
    var ok = fg_vk_access_buffer(model,model+BV_HOST,id.offset,
        vk_tok_address(unsafe_c_address<c_ptr<const void>>(token)),4,1) == 0 && fg_vk_graph_begin(graph) != 0;
    let status: i64 = if ok { fg_vk_bonsai_forward(model,if logits != 0 { 1 } else { 0 }) } else { -1 };
    if status != 0 { vk_http_error(str("Bonsai Vulkan: failed to record graph (",status,")\n")); }
    ok = ok && status == 0 && fg_vk_graph_submit(graph) != 0 && (logits == 0 ||
        fg_vk_access_buffer(model,model+BV_HOST,output.offset,logits,output.size,0) == 0);
    if ok { vk_http_set_u32(model+BV_POSITION,position+1); }
    else { vk_http_set_u32(model+BV_FAILED,1); }
    return if ok { 1 } else { 0 };
}

export fn fg_vk_bonsai_is_eog(model: u64, token: u32) -> i32 {
    for i in (vk_gguf_read_word(model+BV_EOG_COUNT) as! i64) {
        if vk_gguf_read_word(model+BV_EOG+(i as! u64)*8) == (token as! u64) { return 1; }
    }
    return 0;
}

export fn fg_vk_bonsai_reset(model: u64) -> i32 {
    if vk_http_u32(model+BV_POSITION) == 0 && vk_http_u32(model+BV_FAILED) == 0 { return 1; }
    if fg_vk_upload_zero(model+BV_RESET,model+BV_STATE) == 0 { return 0; }
    vk_http_set_u32(model+2768,0);
    vk_http_set_u32(model+BV_POSITION,0);
    vk_http_set_u32(model+BV_FAILED,0);
    return 1;
}
