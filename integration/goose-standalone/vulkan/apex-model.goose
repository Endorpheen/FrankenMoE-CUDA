const AV_WM_PRE: u64 = 0;
const AV_WM_POST: u64 = 1;
const AV_WM_ROUTER: u64 = 2;
const AV_WM_SHARED_GATE: u64 = 3;
const AV_WM_SHARED_UP: u64 = 4;
const AV_WM_SHARED_DOWN: u64 = 5;
const AV_WM_SHARED_ROUTER: u64 = 6;
const AV_WM_Q: u64 = 7;
const AV_WM_K: u64 = 8;
const AV_WM_V: u64 = 9;
const AV_WM_QN: u64 = 10;
const AV_WM_KN: u64 = 11;
const AV_WM_AO: u64 = 12;
const AV_WM_MIX: u64 = 13;
const AV_WM_Z: u64 = 14;
const AV_WM_ALPHA: u64 = 15;
const AV_WM_BETA: u64 = 16;
const AV_WM_DT_A: u64 = 17;
const AV_WM_CONV: u64 = 18;
const AV_WM_GN: u64 = 19;
const AV_WM_GO: u64 = 20;
const AV_SX_H0: u64 = 0;
const AV_SX_NORM: u64 = 2;
const AV_SX_MIX: u64 = 3;
const AV_SX_Q: u64 = 4;
const AV_SX_K: u64 = 5;
const AV_SX_V: u64 = 6;
const AV_SX_QN: u64 = 7;
const AV_SX_KN: u64 = 8;
const AV_SX_QR: u64 = 9;
const AV_SX_KR: u64 = 10;
const AV_SX_GATE: u64 = 11;
const AV_SX_ATTN: u64 = 12;
const AV_SX_GATED: u64 = 13;
const AV_SX_ALPHA: u64 = 14;
const AV_SX_BETA: u64 = 15;
const AV_SX_GATES: u64 = 16;
const AV_SX_CONV: u64 = 17;
const AV_SX_POST: u64 = 18;
const AV_SX_ROUTER: u64 = 19;
const AV_SX_GATE_FF: u64 = 20;
const AV_SX_UP_FF: u64 = 21;
const AV_SX_SWIGLU: u64 = 22;
const AV_SX_EXPERTS: u64 = 23;
const AV_SX_ROUTED: u64 = 24;
const AV_SX_SHARED: u64 = 25;
const AV_SX_SCALAR: u64 = 26;
const AV_SX_SHARED_GATED: u64 = 27;
const AV_SX_FF: u64 = 28;
const AV_SX_RESIDUAL: u64 = 29;

const AV_RESET: u64 = 2776;
const AV_BANK: u64 = 2848;
const AV_GLOBALS: u64 = 7576;
const AV_WORK: u64 = 7672;
const AV_HOST: u64 = 7704;
const AV_STATE: u64 = 7736;
const AV_GLOBAL: u64 = 7768;
const AV_LAYERS: u64 = 7864;
const AV_SCRATCH: u64 = 40824;
const AV_IDS: u64 = 41544;
const AV_SELECTED: u64 = 41568;
const AV_LOGITS: u64 = 41592;
const AV_FILE: u64 = 41616;
const AV_FILE_BYTES: u64 = 41624;
const AV_CAPACITY: u64 = 41632;
const AV_POSITION: u64 = 41636;
const AV_TOKEN: u64 = 41640;
const AV_SCALE: u64 = 41644;
const AV_FAILED: u64 = 41648;
const AV_LAYER_BYTES: u64 = 824;
const AV_LAYER_WEIGHTS: u64 = 32;
const AV_LAYER_PLANES: u64 = 704;
const AV_LAYER_OPS: u64 = 752;
const AV_LAYER_FIRST: u64 = 768;
const AV_LAYER_SECOND: u64 = 792;
const AV_LAYER_RECURRENT: u64 = 816;
const AV_VOCAB: u32 = 248320;

struct AvSpec { name: const u8[:], columns: i64, rows: i64, experts: i64, types: u32 }

extern fn fg_vk_apex_forward(model: u64, logits: i64) -> i64;
extern fn fg_vk_graph_gdn_apply(g: u64, heads: u32, a: QvView, b: QvView, c: QvView, out: QvView) -> i32;
extern fn fg_vk_graph_conv(g: u64, channels: u32, a: QvView, b: QvView, c: QvView, out: QvView) -> i32;
extern fn fg_vk_graph_gdn(g: u64, shape: QvShape, a: QvView, b: QvView, c: QvView, out: QvView) -> i32;
extern fn fg_vk_graph_gated_rms(g: u64, shape: QvShape, a: QvView, b: QvView, c: QvView, out: QvView) -> i32;
extern fn fg_vk_graph_attention_q4(g: u64, shape: QvShape, a: QvView, b: QvView, c: QvView, out: QvView) -> i32;
extern fn fg_vk_expert_bank_open(bank: u64, vk: u64, slots: u32, bytes: u64) -> i32;
extern fn fg_vk_expert_bank_close(bank: u64);
extern fn fg_vk_expert_bank_stage(bank: u64, file: u64, bytes: u64, experts: u32, planes: u64, ids: u64, count: u32) -> i32;
extern fn fg_vk_expert_bank_record_split(graph: u64, bank: u64, hidden: u32, ff: u32,
    ops: u64, input: QvView, scratch: u64, branches: QvView) -> i32;

fn av_layer(model: u64, index: i64) -> u64 { return model + AV_LAYERS + (index as! u64) * AV_LAYER_BYTES; }
fn av_s(model: u64, index: u64) -> QvView { return qv_at(model + AV_SCRATCH, index); }
fn av_w(layer: u64, index: u64) -> u64 { return layer + AV_LAYER_WEIGHTS + index * 32; }

fn av_alloc(model: u64, buffer: u64, bytes: u64, host: bool, transfer: bool) -> bool {
    let status = fg_vk_alloc_buffer(model,buffer,bytes,if transfer { 34 } else { 32 },
        if host { 2 } else { 1 },if host { 4 } else { 0 });
    if status != 0 { qv_log("allocate Vulkan Apex buffer",status); }
    return status == 0;
}

fn av_tensor(header: u64, spec: AvSpec, name: const u8[:], data: i64[:], target: u64) -> bool {
    let state = vk_gguf_state(header);
    let tensors = vk_tok_slice(header+VK_GGUF_TENSORS,12288);
    let slots = vk_tok_slice(header+VK_GGUF_TENSOR_SLOTS,2048);
    let key = vk_tok_slice(vk_tok_address(unsafe_c_slice_data<c_ptr<const void>>(name)),name.len);
    let id = gr_find_tensor(vk_gguf_data(header),state,tensors,slots,key);
    let output = vk_tok_slice(vk_tok_address(unsafe_c_slice_data<c_ptr<const void>>(data)),9);
    if gr_tensor_info(state,tensors,id,output) != 0 || data[2] != spec.columns || data[3] != spec.rows ||
        data[4] != spec.experts || data[5] != 1 || data[7] <= 0 || data[7] % 4 != 0 || data[8] < 0 { return false; }
    let kind: u32 = if data[0] == 0 { 1 } else { if data[0] == 12 { 2 } else { if data[0] == 14 { 4 } else { 0 } } };
    vk_http_set_u32(target,if kind == 1 { 28 } else { if kind == 2 { 22 } else { 23 } });
    return (spec.types & kind) != 0;
}

fn av_meta(model: u64, header: u64) -> bool {
    if !vk_tok_meta_is(header,"general.architecture","qwen35moe") || !vk_tok_meta_is(header,"tokenizer.ggml.pre","qwen35") {
        return false;
    }
    let keys: (const u8[:])[18] = ["block_count","nextn_predict_layers","embedding_length","attention.head_count",
        "attention.head_count_kv","attention.key_length","attention.value_length","rope.dimension_count","full_attention_interval",
        "ssm.conv_kernel","ssm.state_size","ssm.group_count","ssm.time_step_rank","ssm.inner_size","expert_count",
        "expert_used_count","expert_feed_forward_length","expert_shared_feed_forward_length"];
    let vals: i64[18] = [41,1,2048,16,2,256,256,64,4,4,128,16,32,4096,256,8,512,512];
    for i in vals.len { if !qv_meta_is(header,str("qwen35moe.",keys[i]),vals[i]) { return false; } }
    let sections = vk_tok_meta(header,"qwen35moe.rope.dimension_sections");
    let layer_types = vk_tok_meta(header,"qwen35moe.attention.recurrent_layers");
    let expected: i64[4] = [11,11,10,0];
    let data = vk_gguf_data(header);
    let state = vk_gguf_state(header);
    let meta = vk_gguf_metadata(header);
    var value: i64 = 0;
    let out = vk_tok_slice(vk_tok_address(unsafe_c_address<c_ptr<const void>>(value)),1);
    for i in 4 {
        if gr_array_int(data,state,meta,sections,i,out) != 0 || value != expected[i] { return false; }
    }
    for i in 40 {
        let flag: i64 = if (i+1)%4 != 0 { 1 } else { 0 };
        if gr_array_int(data,state,meta,layer_types,i,out) != 0 || value != flag { return false; }
        vk_http_set_u32(av_layer(model,i)+AV_LAYER_RECURRENT,flag as! u32);
    }
    if !qv_meta(header,"qwen35moe.attention.layer_norm_rms_epsilon",value,true) || (value as! u32) != 0x358637bd ||
        !qv_meta(header,"qwen35moe.rope.freq_base",value,true) || (value as! u32) != 0x4b189680 { return false; }
    let scale = vk_tok_meta(header,"qwen35moe.expert_weights_scale");
    if scale >= 0 {
        if gr_meta_f32_bits(data,state,meta,scale,out) != 0 { return false; }
        vk_http_set_u32(model+AV_SCALE,value as! u32);
    }
    return (vk_http_u32(model+AV_SCALE) & 0x7f800000) != 0x7f800000 && qv_read_f32(model+AV_SCALE) >= 0.0 as! f32;
}

fn av_globals(model: u64, l: VkWeightLoader) -> bool {
    let specs: AvSpec[3] = [AvSpec {name:"token_embd.weight",columns:2048,rows:248320,experts:1,types:2},
        AvSpec {name:"output_norm.weight",columns:2048,rows:1,experts:1,types:1},
        AvSpec {name:"output.weight",columns:2048,rows:248320,experts:1,types:4}];
    for i in 3 {
        var t: i64[9];
        let b = model+AV_GLOBALS+(i as! u64)*32;
        let w = model+AV_GLOBAL+(i as! u64)*32;
        if !av_tensor(l.header,specs[i],specs[i].name,t,w+24) ||
            !av_alloc(model,b,t[7] as! u64,false,true) ||
            fg_gguf_upload(l.file,l.upload,b,0,t[8] as! u64,t[7] as! u64,l.chunk) == 0 { return false; }
        qv_set_view(w,b,0,t[7] as! u64);
    }
    return true;
}

fn av_weights(model: u64, loader: VkWeightLoader, index: i64, maxima: u64[:]) -> bool {
    let specs: AvSpec[21] = [
        AvSpec {name:"attn_norm.weight",columns:2048,rows:1,experts:1,types:1},
        AvSpec {name:"post_attention_norm.weight",columns:2048,rows:1,experts:1,types:1},
        AvSpec {name:"ffn_gate_inp.weight",columns:2048,rows:256,experts:1,types:1},
        AvSpec {name:"ffn_gate_shexp.weight",columns:2048,rows:512,experts:1,types:2},
        AvSpec {name:"ffn_up_shexp.weight",columns:2048,rows:512,experts:1,types:2},
        AvSpec {name:"ffn_down_shexp.weight",columns:512,rows:2048,experts:1,types:6},
        AvSpec {name:"ffn_gate_inp_shexp.weight",columns:2048,rows:1,experts:1,types:1},
        AvSpec {name:"attn_q.weight",columns:2048,rows:8192,experts:1,types:2},
        AvSpec {name:"attn_k.weight",columns:2048,rows:512,experts:1,types:2},
        AvSpec {name:"attn_v.weight",columns:2048,rows:512,experts:1,types:6},
        AvSpec {name:"attn_q_norm.weight",columns:256,rows:1,experts:1,types:1},
        AvSpec {name:"attn_k_norm.weight",columns:256,rows:1,experts:1,types:1},
        AvSpec {name:"attn_output.weight",columns:4096,rows:2048,experts:1,types:2},
        AvSpec {name:"attn_qkv.weight",columns:2048,rows:8192,experts:1,types:6},
        AvSpec {name:"attn_gate.weight",columns:2048,rows:4096,experts:1,types:2},
        AvSpec {name:"ssm_alpha.weight",columns:2048,rows:32,experts:1,types:2},
        AvSpec {name:"ssm_beta.weight",columns:2048,rows:32,experts:1,types:2},
        AvSpec {name:"ssm_dt.bias",columns:32,rows:1,experts:1,types:1},
        AvSpec {name:"ssm_conv1d.weight",columns:4,rows:8192,experts:1,types:1},
        AvSpec {name:"ssm_norm.weight",columns:128,rows:1,experts:1,types:1},
        AvSpec {name:"ssm_out.weight",columns:4096,rows:2048,experts:1,types:2}
    ];
    let experts: AvSpec[3] = [AvSpec {name:"ffn_gate_exps.weight",columns:2048,rows:512,experts:256,types:2},
        AvSpec {name:"ffn_up_exps.weight",columns:2048,rows:512,experts:256,types:2},
        AvSpec {name:"ffn_down_exps.weight",columns:512,rows:2048,experts:256,types:6}];
    let layer = av_layer(model,index);
    let recur = vk_http_u32(layer+AV_LAYER_RECURRENT) != 0;
    var offsets: i64[21];
    var end: u64 = 0;
    var t: i64[9];
    for i in specs.len {
        if (i>=7 && i<=12 && recur) || (i>=13 && !recur) { continue; }
        let name = str("blk.",index,".",specs[i].name);
        let weight = av_w(layer,i as! u64);
        if !av_tensor(loader.header,specs[i],name,t,weight+24) {
            vk_http_error(str("Apex Vulkan: incompatible tensor ",name,"\n"));
            return false;
        }
        offsets[i] = t[8];
        let bytes = (t[7] as! u64) * (if i == 17 { 2 } else { 1 });
        qv_work_view(weight,layer,bytes,loader.alignment,end);
    }
    var ssm: u64 = 0;
    if recur {
        var unused: u32 = 0;
        let a = AvSpec {name:"ssm_a",columns:32,rows:1,experts:1,types:1};
        if !av_tensor(loader.header,a,str("blk.",index,".",a.name),t,vk_tok_address(unsafe_c_address<c_ptr<const void>>(unused))) { return false; }
        ssm = t[8] as! u64;
    }
    for i in 3 {
        let name = str("blk.",index,".",experts[i].name);
        if !av_tensor(loader.header,experts[i],name,t,layer+AV_LAYER_OPS+(i as! u64)*4) || t[7]%256 != 0 { return false; }
        let plane = layer+AV_LAYER_PLANES+(i as! u64)*16;
        let bytes = (t[7]/256) as! u64;
        vk_gguf_write_word(plane,t[8] as! u64);
        vk_gguf_write_word(plane+8,bytes);
        if bytes > maxima[i] { maxima[i]=bytes; }
    }
    if !av_alloc(model,layer,end,false,true) { return false; }
    for i in specs.len {
        let view = qv_view(av_w(layer,i as! u64));
        if view.size == 0 { continue; }
        let bytes = if i == 17 { view.size/2 } else { view.size };
        if fg_gguf_upload(loader.file,loader.upload,layer,view.offset,offsets[i] as! u64,bytes,loader.chunk) == 0 ||
            (i == 17 && fg_gguf_upload(loader.file,loader.upload,layer,view.offset+bytes,ssm,bytes,loader.chunk) == 0) { return false; }
    }
    return true;
}

fn av_work(model: u64, upload: u64) -> bool {
    let align = vk_gguf_read_word(model+664);
    var work: u64 = 0;
    var host: u64 = 0;
    var state: u64 = 0;
    let widths: u64[30] = [2048,2048,2048,8192,4096,512,512,4096,512,4096,512,4096,4096,4096,
        32,32,64,8192,2048,256,512,512,512,16384,2048,2048,1,2048,2048,2048];
    for i in widths.len { qv_work_view(model+AV_SCRATCH+(i as! u64)*24,model+AV_WORK,widths[i]*4,align,work); }
    qv_work_view(model+AV_IDS,model+AV_HOST,4,align,host);
    qv_work_view(model+AV_SELECTED,model+AV_HOST,64,align,host);
    qv_work_view(model+AV_LOGITS,model+AV_HOST,993280,align,host);
    let cache = (vk_http_u32(model+AV_CAPACITY) as! u64)*288;
    for i in 40 {
        let layer = av_layer(model,i);
        let recurrent = vk_http_u32(layer+AV_LAYER_RECURRENT) != 0;
        qv_work_view(layer+AV_LAYER_FIRST,model+AV_STATE,if recurrent { 98304 } else { cache },align,state);
        qv_work_view(layer+AV_LAYER_SECOND,model+AV_STATE,if recurrent { 2097152 } else { cache },align,state);
    }
    return av_alloc(model,model+AV_WORK,work,false,false) && av_alloc(model,model+AV_HOST,host,true,false) &&
        av_alloc(model,model+AV_STATE,state,false,true) && fg_vk_upload_zero(upload,model+AV_STATE) != 0;
}

fn av_load(model: u64) -> bool {
    let header = vk_tok_table(1,TK_HEADER_BYTES).address;
    let chunk = vk_tok_table(4194304,1).address;
    var storage: u64[9] = [0;9];
    let upload = vk_tok_address(unsafe_c_address<c_ptr<const void>>(storage[0]));
    let file = vk_gguf_read_word(model+AV_FILE);
    let bytes: i64 = if header != 0 { fg_gguf_read_header(header,file) } else { -1 };
    var maxima: u64[3] = [0,0,0];
    var ok = bytes>0 && chunk != 0 && av_meta(model,header) && fg_vk_upload_open(upload,model,4194304) != 0;
    if ok {
        vk_gguf_write_word(model+AV_FILE_BYTES,bytes as! u64);
        let loader = VkWeightLoader {file:file,header:header,upload:upload,chunk:chunk,alignment:vk_gguf_read_word(model+664)};
        ok = av_globals(model,loader);
        if ok {
            for i in 40 {
                if !av_weights(model,loader,i,maxima) { ok=false; break; }
            }
        }
        ok = ok && av_work(model,upload) &&
            fg_vk_expert_bank_open(model+AV_BANK,model,8,vk_tok_address(unsafe_c_address<c_ptr<const void>>(maxima[0]))) != 0;
    } else { vk_http_error("Apex Vulkan: incompatible GGUF metadata or upload setup failed\n"); }
    fg_vk_upload_close(upload);
    if header != 0 { vk_tok_release(vk_gguf_read_word(header)); }
    vk_tok_release(header);
    vk_tok_release(chunk);
    return ok;
}

export fn fg_vk_apex_close(model: u64) {
    if model == 0 { return; }
    let file = vk_gguf_read_word(model+AV_FILE);
    if file != 0 { fg_vk_tok_fclose(file); }
    fg_vk_graph_close(model+QV_GRAPH);
    fg_vk_upload_close(model+AV_RESET);
    fg_vk_expert_bank_close(model+AV_BANK);
    for i in 40 { fg_vk_free_buffer(model,av_layer(model,i)); }
    for i in 3 { fg_vk_free_buffer(model,model+AV_GLOBALS+(i as! u64)*32); }
    fg_vk_free_buffer(model,model+AV_WORK);
    fg_vk_free_buffer(model,model+AV_HOST);
    fg_vk_free_buffer(model,model+AV_STATE);
    fg_vk_close(model);
    vk_tok_release(model);
}

export fn fg_vk_apex_open(path: u64, capacity: u32, device: u32, cpu: i32) -> u64 {
    if capacity == 0 || capacity > 32768 { return 0; }
    let model = vk_tok_table(1,41656).address;
    if model == 0 { return 0; }
    vk_http_set_u32(model+AV_CAPACITY,capacity);
    let mode: u8[3] = [114,98,0];
    let file = fg_vk_tok_fopen(path,vk_tok_address(unsafe_c_address<c_ptr<const void>>(mode[0])));
    vk_gguf_write_word(model+AV_FILE,file);
    if file == 0 || fg_vk_open(model,device,cpu) == 0 || !av_load(model) ||
        fg_vk_graph_open(model+QV_GRAPH,model,48) == 0 || fg_vk_upload_open(model+AV_RESET,model,4) == 0 {
        fg_vk_apex_close(model);
        return 0;
    }
    var name: u8[>..] = [];
    for i in 256 {
        let byte = vk_gguf_read_byte(model+60+(i as! u64));
        if byte == 0 { break; }
        name.push(byte);
    }
    vk_http_error(str("Vulkan Apex on ",name,", context=",capacity,", MTP off\n"));
    return model;
}

fn av_element(model: u64, op: u32, width: u32, a: QvView, b: QvView, out: QvView) -> bool {
    var shape = qv_shape(width,0);
    shape.epsilon = 0.000001 as! f32;
    return fg_vk_graph_record(model+QV_GRAPH,op,shape,a,b,out) != 0;
}

fn av_project(model: u64, width: u32, rows: u32, a: QvView, weight: u64, out: QvView) -> bool {
    return fg_vk_graph_record(model+QV_GRAPH,vk_http_u32(weight+24),qv_shape(width,rows),a,qv_view(weight),out) != 0;
}

fn av_recurrent(m: u64, layer: u64) -> bool {
    let g=m+QV_GRAPH;
    let norm=av_s(m,AV_SX_NORM);
    var shape=qv_shape(128,64);
    shape.kv_heads=16;
    shape.epsilon=0.000001 as! f32;
    var gdn=qv_shape(128,32);
    gdn.kv_heads=16;
    var gate=qv_shape(128,0);
    gate.batch=32;
    gate.epsilon=0.000001 as! f32;
    return av_project(m,2048,8192,norm,av_w(layer,AV_WM_MIX),av_s(m,AV_SX_MIX)) &&
        av_project(m,2048,4096,norm,av_w(layer,AV_WM_Z),av_s(m,AV_SX_GATE)) &&
        av_project(m,2048,32,norm,av_w(layer,AV_WM_ALPHA),av_s(m,AV_SX_ALPHA)) &&
        av_project(m,2048,32,norm,av_w(layer,AV_WM_BETA),av_s(m,AV_SX_BETA)) &&
        fg_vk_graph_gdn_apply(g,32,av_s(m,AV_SX_ALPHA),av_s(m,AV_SX_BETA),qv_view(av_w(layer,AV_WM_DT_A)),av_s(m,AV_SX_GATES)) != 0 &&
        fg_vk_graph_conv(g,8192,av_s(m,AV_SX_MIX),qv_view(av_w(layer,AV_WM_CONV)),qv_view(layer+AV_LAYER_FIRST),av_s(m,AV_SX_CONV)) != 0 &&
        fg_vk_graph_record(g,31,shape,av_s(m,AV_SX_CONV),av_s(m,AV_SX_CONV),av_s(m,AV_SX_MIX)) != 0 &&
        fg_vk_graph_gdn(g,gdn,av_s(m,AV_SX_MIX),qv_view(layer+AV_LAYER_SECOND),av_s(m,AV_SX_GATES),av_s(m,AV_SX_ATTN)) != 0 &&
        fg_vk_graph_gated_rms(g,gate,av_s(m,AV_SX_ATTN),qv_view(av_w(layer,AV_WM_GN)),av_s(m,AV_SX_GATE),av_s(m,AV_SX_GATED)) != 0 &&
        av_project(m,4096,2048,av_s(m,AV_SX_GATED),av_w(layer,AV_WM_GO),av_s(m,AV_SX_POST));
}

fn av_attention(m: u64, layer: u64) -> bool {
    let g=m+QV_GRAPH;
    let norm=av_s(m,AV_SX_NORM);
    let q_split=qv_shape(256,16);
    var gate=q_split;
    gate.position=1;
    var q_norm=qv_shape(256,0);
    q_norm.batch=16;
    q_norm.epsilon=0.000001 as! f32;
    var k_norm=q_norm;
    k_norm.batch=2;
    var q_rope=q_split;
    q_rope.position=vk_http_u32(m+AV_POSITION);
    q_rope.rotary=64;
    q_rope.theta=10000000.0 as! f32;
    var k_rope=q_rope;
    k_rope.rows=2;
    var store=qv_shape(256,2);
    store.position=q_rope.position;
    store.context=vk_http_u32(m+AV_CAPACITY);
    var attention=store;
    attention.rows=16;
    attention.kv_heads=2;
    let key=qv_view(layer+AV_LAYER_FIRST);
    let val=qv_view(layer+AV_LAYER_SECOND);
    return av_project(m,2048,8192,norm,av_w(layer,AV_WM_Q),av_s(m,AV_SX_MIX)) &&
        av_project(m,2048,512,norm,av_w(layer,AV_WM_K),av_s(m,AV_SX_K)) &&
        av_project(m,2048,512,norm,av_w(layer,AV_WM_V),av_s(m,AV_SX_V)) &&
        fg_vk_graph_record(g,18,q_split,av_s(m,AV_SX_MIX),av_s(m,AV_SX_MIX),av_s(m,AV_SX_Q)) != 0 &&
        fg_vk_graph_record(g,18,gate,av_s(m,AV_SX_MIX),av_s(m,AV_SX_MIX),av_s(m,AV_SX_GATE)) != 0 &&
        fg_vk_graph_record(g,0,q_norm,av_s(m,AV_SX_Q),qv_view(av_w(layer,AV_WM_QN)),av_s(m,AV_SX_QN)) != 0 &&
        fg_vk_graph_record(g,0,k_norm,av_s(m,AV_SX_K),qv_view(av_w(layer,AV_WM_KN)),av_s(m,AV_SX_KN)) != 0 &&
        fg_vk_graph_record(g,3,q_rope,av_s(m,AV_SX_QN),av_s(m,AV_SX_QN),av_s(m,AV_SX_QR)) != 0 &&
        fg_vk_graph_record(g,3,k_rope,av_s(m,AV_SX_KN),av_s(m,AV_SX_KN),av_s(m,AV_SX_KR)) != 0 &&
        fg_vk_graph_record(g,34,qv_shape(4096,256),av_s(m,AV_SX_QR),av_s(m,AV_SX_QR),av_s(m,AV_SX_Q)) != 0 &&
        fg_vk_graph_record(g,34,qv_shape(512,256),av_s(m,AV_SX_KR),av_s(m,AV_SX_KR),av_s(m,AV_SX_K)) != 0 &&
        fg_vk_graph_record(g,34,qv_shape(512,64),av_s(m,AV_SX_V),av_s(m,AV_SX_V),av_s(m,AV_SX_KN)) != 0 &&
        fg_vk_graph_record(g,20,store,av_s(m,AV_SX_K),av_s(m,AV_SX_K),key) != 0 &&
        fg_vk_graph_record(g,20,store,av_s(m,AV_SX_KN),av_s(m,AV_SX_KN),val) != 0 &&
        fg_vk_graph_attention_q4(g,attention,av_s(m,AV_SX_Q),key,val,av_s(m,AV_SX_ATTN)) != 0 &&
        fg_vk_graph_record(g,34,qv_shape(4096,64),av_s(m,AV_SX_ATTN),av_s(m,AV_SX_ATTN),av_s(m,AV_SX_QR)) != 0 &&
        av_element(m,19,4096,av_s(m,AV_SX_QR),av_s(m,AV_SX_GATE),av_s(m,AV_SX_GATED)) &&
        av_project(m,4096,2048,av_s(m,AV_SX_GATED),av_w(layer,AV_WM_AO),av_s(m,AV_SX_POST));
}

fn av_layer_step(m: u64, index: i64) -> bool {
    let layer=av_layer(m,index);
    let graph=m+QV_GRAPH;
    let parity=(index & 1) as! u64;
    let input=av_s(m,parity);
    let output=av_s(m,1-parity);
    let norm=av_s(m,AV_SX_NORM);
    let residual=av_s(m,AV_SX_RESIDUAL);
    let route_values=qv_view(m+AV_SELECTED);
    let router=av_s(m,AV_SX_ROUTER);
    var route=qv_shape(256,8);
    route.theta=qv_read_f32(m+AV_SCALE);
    var selected: u32[16];
    let ids=vk_tok_address(unsafe_c_address<c_ptr<const void>>(selected[0]));
    return fg_vk_graph_begin(graph) != 0 &&
        av_element(m,0,2048,input,qv_view(av_w(layer,AV_WM_PRE)),norm) &&
        (if vk_http_u32(layer+AV_LAYER_RECURRENT) != 0 { av_recurrent(m,layer) } else { av_attention(m,layer) }) &&
        av_element(m,9,2048,input,av_s(m,AV_SX_POST),residual) &&
        av_element(m,0,2048,residual,qv_view(av_w(layer,AV_WM_POST)),norm) &&
        av_project(m,2048,256,norm,av_w(layer,AV_WM_ROUTER),router) &&
        fg_vk_graph_record(graph,29,route,router,router,route_values) != 0 &&
        av_project(m,2048,512,norm,av_w(layer,AV_WM_SHARED_GATE),av_s(m,AV_SX_GATE_FF)) &&
        av_project(m,2048,512,norm,av_w(layer,AV_WM_SHARED_UP),av_s(m,AV_SX_UP_FF)) &&
        av_element(m,2,512,av_s(m,AV_SX_GATE_FF),av_s(m,AV_SX_UP_FF),av_s(m,AV_SX_SWIGLU)) &&
        av_project(m,512,2048,av_s(m,AV_SX_SWIGLU),av_w(layer,AV_WM_SHARED_DOWN),av_s(m,AV_SX_SHARED)) &&
        av_project(m,2048,1,norm,av_w(layer,AV_WM_SHARED_ROUTER),av_s(m,AV_SX_SCALAR)) &&
        av_element(m,33,2048,av_s(m,AV_SX_SHARED),av_s(m,AV_SX_SCALAR),av_s(m,AV_SX_SHARED_GATED)) &&
        fg_vk_graph_submit(graph) != 0 &&
        fg_vk_access_buffer(m,m+AV_HOST,route_values.offset,ids,64,0) == 0 &&
        fg_vk_expert_bank_stage(m+AV_BANK,vk_gguf_read_word(m+AV_FILE),vk_gguf_read_word(m+AV_FILE_BYTES),
            256,layer+AV_LAYER_PLANES,ids,8) != 0 && fg_vk_graph_begin(graph) != 0 &&
        fg_vk_expert_bank_record_split(graph,m+AV_BANK,2048,512,layer+AV_LAYER_OPS,norm,
            m+AV_SCRATCH+AV_SX_GATE_FF*24,av_s(m,AV_SX_EXPERTS)) != 0 &&
        fg_vk_graph_record(graph,30,qv_shape(2048,8),av_s(m,AV_SX_EXPERTS),route_values,av_s(m,AV_SX_ROUTED)) != 0 &&
        av_element(m,9,2048,av_s(m,AV_SX_ROUTED),av_s(m,AV_SX_SHARED_GATED),av_s(m,AV_SX_FF)) &&
        av_element(m,9,2048,av_s(m,AV_SX_FF),residual,output) && fg_vk_graph_submit(graph) != 0;
}

export fn fg_vk_apex_op(m: u64, op: i64, layer: i64) -> i64 {
    let graph=m+QV_GRAPH;
    if op == 0 {
        let first=vk_http_u32(m+AV_TOKEN)/32*32;
        var gather=qv_shape(2048,32);
        gather.position=first;
        let stripe=QvView {buffer:m+AV_GLOBALS,offset:(first as! u64)*1152,size:36864};
        let ok=fg_vk_graph_begin(graph) != 0 && fg_vk_graph_record(graph,24,gather,qv_view(m+AV_IDS),stripe,av_s(m,AV_SX_H0)) != 0 &&
            fg_vk_graph_submit(graph) != 0;
        return if ok { 0 } else { -1 };
    }
    if op == 1 && layer>=0 && layer<40 { return if av_layer_step(m,layer) { 0 } else { -1 }; }
    let norm=av_s(m,AV_SX_NORM);
    if op != 2 || fg_vk_graph_begin(graph) == 0 ||
        !av_element(m,0,2048,av_s(m,AV_SX_H0),qv_view(m+AV_GLOBAL+32),norm) { return -1; }
    var first: u32=0;
    while first<AV_VOCAB {
        let left=AV_VOCAB-first;
        let rows: u32=if left<32768 { left } else { 32768 };
        let stripe=QvView {buffer:m+AV_GLOBALS+64,offset:(first as! u64)*1680,size:(rows as! u64)*1680};
        var mm=qv_shape(2048,rows);
        mm.position=first;
        mm.context=AV_VOCAB;
        if fg_vk_graph_record(graph,23,mm,norm,stripe,qv_view(m+AV_LOGITS)) == 0 { return -1; }
        first+=rows;
    }
    return if fg_vk_graph_submit(graph) != 0 { 0 } else { -1 };
}

export fn fg_vk_apex_step(m: u64, token: u32, logits: u64) -> i32 {
    let position=vk_http_u32(m+AV_POSITION);
    if vk_http_u32(m+AV_FAILED) != 0 || token>=AV_VOCAB || position>=vk_http_u32(m+AV_CAPACITY) { return 0; }
    vk_http_set_u32(m+AV_TOKEN,token);
    let status: i64=if fg_vk_access_buffer(m,m+AV_HOST,qv_view(m+AV_IDS).offset,
        vk_tok_address(unsafe_c_address<c_ptr<const void>>(token)),4,1) == 0 {
        fg_vk_apex_forward(m,if logits != 0 { 1 } else { 0 })
    } else { -1 };
    if status != 0 { vk_http_error(str("Apex Vulkan: model step failed (",status,")\n")); }
    let out=qv_view(m+AV_LOGITS);
    let ok=status == 0 && (logits == 0 || fg_vk_access_buffer(m,m+AV_HOST,out.offset,logits,out.size,0) == 0);
    if ok { vk_http_set_u32(m+AV_POSITION,position+1); } else { vk_http_set_u32(m+AV_FAILED,1); }
    return if ok { 1 } else { 0 };
}

export fn fg_vk_apex_reset(m: u64) -> i32 {
    if vk_http_u32(m+AV_POSITION) == 0 && vk_http_u32(m+AV_FAILED) == 0 { return 1; }
    if fg_vk_upload_zero(m+AV_RESET,m+AV_STATE) == 0 { return 0; }
    vk_http_set_u32(m+2768,0);
    vk_http_set_u32(m+AV_POSITION,0);
    vk_http_set_u32(m+AV_FAILED,0);
    return 1;
}
