const IV_RESET: u64 = 2776;
const IV_BANK: u64 = 2848;
const IV_GLOBALS: u64 = 7576;
const IV_IO: u64 = 7608;
const IV_STATE: u64 = 7640;
const IV_GLOBAL: u64 = 7672;
const IV_LAYERS: u64 = 7912;
const IV_CACHE: u64 = 55912;
const IV_VIEWS: u64 = 3892456;
const IV_FILE: u64 = 3893008;
const IV_FILE_BYTES: u64 = 3893016;
const IV_VISITS: u64 = 3893024;
const IV_CAPACITY: u64 = 3894560;
const IV_POSITION: u64 = 3894564;
const IV_TOKEN: u64 = 3894568;
const IV_FAILED: u64 = 3894572;
const IV_CURRENT: u64 = 3894576;
const IV_PRELUDE: u64 = 3903792;
const IV_BANK_OUTPUT: u64 = 3913008;
const IV_BLOCKS: u64 = 3922224;
const IV_LOOP_BANK: u64 = 4032816;
const IV_MESSAGES: u64 = 4143408;
const IV_COUNT: u64 = 4180272;
const IV_PRELUDE_COUNT: u64 = 4180276;
const IV_BANK_COUNT: u64 = 4180280;
const IV_TENSOR_HOST: u64 = 24;
const IV_TENSOR_FILE: u64 = 32;
const IV_TENSOR_BYTES: u64 = 40;
const IV_LAYER_AUX: u64 = 512;
const IV_LAYER_PLANES: u64 = 1952;
const IV_CACHE_ROWS: u64 = 48;
const IV_CACHE_KH: u64 = 52;
const IV_CACHE_VH: u64 = 12340;
const IV_CACHE_AH: u64 = 24628;
const IV_CACHE_FH: u64 = 52276;
const IV_S_I0: u64 = 0;
const IV_S_I1: u64 = 1;
const IV_S_QRS: u64 = 3;
const IV_S_KEY: u64 = 4;
const IV_S_VALUE: u64 = 5;
const IV_S_ATTN: u64 = 6;
const IV_S_GATED: u64 = 7;
const IV_S_P0: u64 = 8;
const IV_S_P5: u64 = 13;
const IV_S_GATE_UP: u64 = 16;
const IV_S_ACTIVATION: u64 = 17;
const IV_S_BRANCHES: u64 = 18;
const IV_S_ROUTER: u64 = 19;
const IV_S_ROUTES: u64 = 20;
const IV_S_ID: u64 = 21;
const IV_S_LOGITS: u64 = 22;

const IV_WIDTH: i64 = 2304;
const IV_ROW: u64 = 9216;
const IV_VOCAB: u32 = 200058;

extern fn i2_read_layout(data: VkGgufSlice, state: VkGgufSlice, meta: VkGgufSlice,
    slots: VkGgufSlice, layout: VkGgufSlice, visits: VkGgufSlice) -> i64;
extern fn i2_bind_text_weights(data: VkGgufSlice, state: VkGgufSlice, meta: VkGgufSlice,
    meta_slots: VkGgufSlice,tensors: VkGgufSlice,tensor_slots: VkGgufSlice,
    layout: VkGgufSlice, globals: VkGgufSlice, core: VkGgufSlice) -> i64;
extern fn i2_bind_text_aux(data: VkGgufSlice, state: VkGgufSlice, meta: VkGgufSlice,
    meta_slots: VkGgufSlice,tensors: VkGgufSlice,tensor_slots: VkGgufSlice, layout: VkGgufSlice, aux: VkGgufSlice) -> i64;
extern fn i2_vk_prepare(proj: VkGgufSlice, q: VkGgufSlice, rel: VkGgufSlice, kc: VkGgufSlice,
    vc: VkGgufSlice, pos: i64, slide: bool, kh: VkGgufSlice, vh: VkGgufSlice, qrs: VkGgufSlice,
    key: VkGgufSlice, value: VkGgufSlice) -> i64;
extern fn i2_rms_gain(source: VkGgufSlice, weight: VkGgufSlice, eps: f32, output: VkGgufSlice) -> i64;
extern fn i2_depth_mix(states: VkGgufSlice, width: i64, queries: VkGgufSlice,
    norms: VkGgufSlice, probability: VkGgufSlice, output: VkGgufSlice) -> i64;
extern fn i2_loop_blend(cur: VkGgufSlice,msg: VkGgufSlice,gain: VkGgufSlice,forget: VkGgufSlice,out: VkGgufSlice) -> i64;
extern fn i2_postconv_token(proj: VkGgufSlice,weight: VkGgufSlice,history: VkGgufSlice,out: VkGgufSlice) -> i64;
extern fn i2_text_logits(logits: VkGgufSlice,vocab: i64,softcap: f32) -> i64;
extern fn fg_vk_inkling_forward(model: u64, visits: VkGgufSlice, logits: bool) -> i64;
extern fn fg_vk_graph_inkling_attention(g: u64, shape: QvShape, query: QvView, key: QvView, value: QvView, out: QvView) -> i32;
extern fn fg_vk_expert_bank_record_inkling(graph: u64,bank: u64,width: u32,ff: u32,
    input: QvView, gate_up: QvView, activation: QvView, branches: QvView) -> i32;

fn iv_layer(model: u64, index: i64) -> u64 { return model+IV_LAYERS+(index as! u64)*2000; }
fn iv_cache(model: u64, index: i64) -> u64 { return model+IV_CACHE+(index as! u64)*79928; }
fn iv_s(model: u64, index: u64) -> QvView { return qv_at(model+IV_VIEWS,index); }
fn iv_host(tensor: u64) -> u64 { return vk_gguf_read_word(tensor+IV_TENSOR_HOST); }
fn iv_tensor(base: u64, index: u64) -> u64 { return base+index*48; }
fn iv_copy(dst: u64, src: u64, bytes: u64) {
    vk_gguf_copy(unsafe_c_ptr<c_ptr<void>>(dst),unsafe_c_ptr<c_ptr<const void>>(src),bytes);
}

fn iv_alloc(model: u64, buffer: u64, bytes: u64, host: bool) -> bool {
    let result=fg_vk_alloc_buffer(model,buffer,bytes,if host { 32 } else { 34 },
        if host { 2 } else { 1 },if host { 4 } else { 0 });
    if result != 0 { qv_log("allocate Vulkan Inkling buffer",result); }
    return result == 0;
}

fn iv_record_tensor(header: u64,id: i64,tensor: u64) -> bool {
    var data: i64[9];
    let out=vk_tok_slice(vk_tok_address(unsafe_c_address<c_ptr<const void>>(data[0])),9);
    if gr_tensor_info(vk_gguf_state(header),vk_tok_slice(header+VK_GGUF_TENSORS,12288),id,out) != 0 ||
        data[7] <= 0 || data[7]%4 != 0 || data[8]<0 { return false; }
    vk_gguf_write_word(tensor+IV_TENSOR_BYTES,data[7] as! u64);
    vk_gguf_write_word(tensor+IV_TENSOR_FILE,data[8] as! u64);
    return true;
}

fn iv_plan(loader: VkWeightLoader,id: i64,tensor: u64,buffer: u64,size: u64&,host: bool) -> bool {
    if !iv_record_tensor(loader.header,id,tensor) { return false; }
    let bytes=vk_gguf_read_word(tensor+IV_TENSOR_BYTES);
    if host {
        let data=vk_tok_table(bytes as! i64,1).address;
        vk_gguf_write_word(tensor+IV_TENSOR_HOST,data);
        return data != 0 && fg_vk_crt_seek(loader.file,vk_gguf_read_word(tensor+IV_TENSOR_FILE) as! i64,0) == 0 &&
            fg_vk_crt_read(data,1,bytes,loader.file) == bytes;
    }
    qv_work_view(tensor,buffer,bytes,loader.alignment,size);
    return true;
}

fn iv_upload(loader: VkWeightLoader,tensor: u64) -> bool {
    let view=qv_view(tensor);
    return view.size == 0 || fg_gguf_upload(loader.file,loader.upload,view.buffer,view.offset,
        vk_gguf_read_word(tensor+IV_TENSOR_FILE),vk_gguf_read_word(tensor+IV_TENSOR_BYTES),loader.chunk) != 0;
}

fn iv_work(model: u64,upload: u64) -> bool {
    let align=vk_gguf_read_word(model+664);
    var io: u64=0;
    var state: u64=0;
    let counts: u64[23]=[4608,2304,2304,19864,1024,1024,3072,3072,3072,1024,1024,384,24,3072,32,32,2304,1152,18432,128,16,1,200058];
    for i in counts.len { qv_work_view(model+IV_VIEWS+(i as! u64)*24,model+IV_IO,counts[i]*4,align,io); }
    let cap=vk_http_u32(model+IV_CAPACITY);
    for i in 48 {
        let visit=model+IV_VISITS+((i*4) as! u64)*8;
        let slot=vk_gguf_read_word(visit+8) as! u32;
        if slot >= 48 { return false; }
        let cache=iv_cache(model,slot as! i64);
        if vk_http_u32(cache+IV_CACHE_ROWS) != 0 { return false; }
        let rows: u32=if vk_gguf_read_word(visit+24) != 0 && cap>1024 { 1024 } else { cap };
        vk_http_set_u32(cache+IV_CACHE_ROWS,rows);
        let bytes=(rows as! u64)*576;
        qv_work_view(cache,model+IV_STATE,bytes,align,state);
        qv_work_view(cache+24,model+IV_STATE,bytes,align,state);
    }
    return iv_alloc(model,model+IV_IO,io,true) && iv_alloc(model,model+IV_STATE,state,false) &&
        fg_vk_upload_zero(upload,model+IV_STATE) != 0;
}

fn iv_load_layer(model: u64,loader: VkWeightLoader,index: i64,core: const i64[:],aux: const i64[:],limits: u64[:]) -> bool {
    let layer=iv_layer(model,index);
    let weights=layer+32;
    let extra=layer+IV_LAYER_AUX;
    var size: u64=0;
    for j in 10 {
        let t=iv_tensor(weights,j as! u64);
        if j == 6 || j == 7 {
            if !iv_record_tensor(loader.header,core[j],t) { return false; }
            let bytes=vk_gguf_read_word(t+IV_TENSOR_BYTES);
            if bytes%128 != 0 { return false; }
            let plane=layer+IV_LAYER_PLANES+((j-6) as! u64)*16;
            vk_gguf_write_word(plane,vk_gguf_read_word(t+IV_TENSOR_FILE));
            vk_gguf_write_word(plane+8,bytes/128);
            if bytes/128>limits[j-6] { limits[j-6]=bytes/128; }
        } else if !iv_plan(loader,core[j],t,layer,size,j==9) { return false; }
    }
    for j in 30 {
        let host=j<5 || j==6 || j==7 || (j>=9 && j<=14) || j==17;
        if aux[j]>=0 && !iv_plan(loader,aux[j],iv_tensor(extra,j as! u64),layer,size,host) { return false; }
    }
    if !iv_alloc(model,layer,size,false) { return false; }
    for j in 10 { if !iv_upload(loader,iv_tensor(weights,j as! u64)) { return false; } }
    for j in 30 { if !iv_upload(loader,iv_tensor(extra,j as! u64)) { return false; } }
    return true;
}

fn iv_load_data(model: u64,l: VkWeightLoader,bytes: i64) -> bool {
    var layout: i64[9];
    var globals: i64[5];
    var core: i64[240];
    var aux: i64[720];
    let data=vk_gguf_data(l.header);
    let state=vk_gguf_state(l.header);
    let meta=vk_gguf_metadata(l.header);
    let meta_slots=vk_gguf_meta_slots(l.header);
    let tensors=vk_tok_slice(l.header+VK_GGUF_TENSORS,12288);
    let slots=vk_tok_slice(l.header+VK_GGUF_TENSOR_SLOTS,2048);
    let description=vk_tok_slice(vk_tok_address(unsafe_c_address<c_ptr<const void>>(layout[0])),9);
    if i2_read_layout(data,state,meta,meta_slots,description,vk_tok_slice(model+IV_VISITS,192)) != 48 ||
        layout[0] != 24 || layout[1] != 6 || layout[2] != 18 || layout[3] != 3 || layout[5] != 6 ||
        layout[6] != 1024 || layout[7] < (vk_http_u32(model+IV_CAPACITY) as! i64) ||
        i2_bind_text_weights(data,state,meta,meta_slots,tensors,slots,description,
            vk_tok_slice(vk_tok_address(unsafe_c_address<c_ptr<const void>>(globals[0])),5),
            vk_tok_slice(vk_tok_address(unsafe_c_address<c_ptr<const void>>(core[0])),240)) != 24 ||
        i2_bind_text_aux(data,state,meta,meta_slots,tensors,slots,description,
            vk_tok_slice(vk_tok_address(unsafe_c_address<c_ptr<const void>>(aux[0])),720)) != 24 {
        vk_http_error("Inkling Vulkan: incompatible GGUF layout\n");
        return false;
    }
    vk_gguf_write_word(model+IV_FILE_BYTES,bytes as! u64);
    var size: u64=0;
    for i in 5 {
        if !iv_plan(l,globals[i],iv_tensor(model+IV_GLOBAL,i as! u64),model+IV_GLOBALS,size,i!=0 && i!=2) { return false; }
    }
    if fg_vk_upload_open(l.upload,model,4194304) == 0 || !iv_alloc(model,model+IV_GLOBALS,size,false) { return false; }
    for i in 5 { if !iv_upload(l,iv_tensor(model+IV_GLOBAL,i as! u64)) { return false; } }
    var limits: u64[3]=[0,0,0];
    for i in 24 {
        if !iv_load_layer(model,l,i,core[i*10 .. i*10+10],aux[i*30 .. i*30+30],limits) { return false; }
    }
    return iv_work(model,l.upload) && fg_vk_expert_bank_open(model+IV_BANK,model,8,
        vk_tok_address(unsafe_c_address<c_ptr<const void>>(limits[0]))) != 0;
}

fn iv_load(model: u64) -> bool {
    let header=vk_tok_table(1,TK_HEADER_BYTES).address;
    let chunk=vk_tok_table(4194304,1).address;
    var reserve: u64[9]=[0;9];
    let upload=vk_tok_address(unsafe_c_address<c_ptr<const void>>(reserve[0]));
    let file=vk_gguf_read_word(model+IV_FILE);
    let bytes: i64=if header != 0 { fg_gguf_read_header(header,file) } else { -1 };
    var ok=false;
    if bytes>0 && chunk != 0 {
        ok=iv_load_data(model,VkWeightLoader {file:file,header:header,upload:upload,chunk:chunk,
            alignment:vk_gguf_read_word(model+664)},bytes);
    } else { vk_http_error("Inkling Vulkan: incompatible GGUF layout\n"); }
    fg_vk_upload_close(upload);
    if header != 0 { vk_tok_release(vk_gguf_read_word(header)); }
    vk_tok_release(header);
    vk_tok_release(chunk);
    return ok;
}

export fn fg_vk_inkling_close(model: u64) {
    if model == 0 { return; }
    let file=vk_gguf_read_word(model+IV_FILE);
    if file != 0 { fg_vk_tok_fclose(file); }
    fg_vk_graph_close(model+QV_GRAPH);
    fg_vk_upload_close(model+IV_RESET);
    fg_vk_expert_bank_close(model+IV_BANK);
    for i in 24 {
        let layer=iv_layer(model,i);
        for j in 40 { vk_tok_release(iv_host(iv_tensor(layer+32,j as! u64))); }
        fg_vk_free_buffer(model,layer);
    }
    for i in 5 { vk_tok_release(iv_host(iv_tensor(model+IV_GLOBAL,i as! u64))); }
    fg_vk_free_buffer(model,model+IV_GLOBALS);
    fg_vk_free_buffer(model,model+IV_IO);
    fg_vk_free_buffer(model,model+IV_STATE);
    fg_vk_close(model);
    vk_tok_release(model);
}

export fn fg_vk_inkling_open(path: u64,capacity: u32,device: u32,cpu: i32) -> u64 {
    if capacity==0 || capacity>32768 { return 0; }
    let model=vk_tok_table(1,4180288).address;
    if model==0 { return 0; }
    vk_http_set_u32(model+IV_CAPACITY,capacity);
    let mode: u8[3]=[114,98,0];
    let file=fg_vk_tok_fopen(path,vk_tok_address(unsafe_c_address<c_ptr<const void>>(mode[0])));
    vk_gguf_write_word(model+IV_FILE,file);
    if file==0 || fg_vk_open(model,device,cpu)==0 || !iv_load(model) ||
        fg_vk_graph_open(model+QV_GRAPH,model,32)==0 || fg_vk_upload_open(model+IV_RESET,model,4)==0 {
        fg_vk_inkling_close(model);
        return 0;
    }
    var name: u8[>..]=[];
    for i in 256 {
        let byte=vk_gguf_read_byte(model+60+(i as! u64));
        if byte==0 { break; }
        name.push(byte);
    }
    vk_http_error(str("Vulkan Inkling on ",name,", context=",capacity,", Q4_0 KV\n"));
    return model;
}

fn iv_access(model: u64,index: u64,bytes: u64,data: u64,write: i32) -> bool {
    let view=iv_s(model,index);
    return bytes<=view.size && fg_vk_access_buffer(model,model+IV_IO,view.offset,data,bytes,write)==0;
}

fn iv_project(model: u64,op: u32,width: u32,rows: u32,input: u64,weight: u64,out: u64) -> bool {
    return fg_vk_graph_record(model+QV_GRAPH,op,qv_shape(width,rows),iv_s(model,input),qv_view(weight),iv_s(model,out)) != 0;
}

fn iv_normalize(source: u64,weight: u64,output: u64) -> bool {
    return i2_rms_gain(vk_tok_slice(source,IV_WIDTH),vk_tok_slice(weight,if weight!=0 { IV_WIDTH } else { 0 }),
        0.000001 as! f32,vk_tok_slice(output,IV_WIDTH))==IV_WIDTH;
}

fn iv_depth(blocks: u64,count: i64,cur: u64,queries: u64,many: i64,output: u64) -> bool {
    if count<0 || count>=12 { return false; }
    var states: f32[27648];
    var norms: f32[27648];
    var probability: f32[12];
    let dest=vk_tok_address(unsafe_c_address<c_ptr<const void>>(states[0]));
    for i in count { iv_copy(dest+(i as! u64)*IV_ROW,blocks+((count-1-i) as! u64)*IV_ROW,IV_ROW); }
    iv_copy(dest+(count as! u64)*IV_ROW,cur,IV_ROW);
    let n=(count+1)*IV_WIDTH;
    let q=many*IV_WIDTH;
    return i2_depth_mix(vk_tok_slice(dest,n),IV_WIDTH,vk_tok_slice(queries,q),
        vk_tok_slice(vk_tok_address(unsafe_c_address<c_ptr<const void>>(norms[0])),n),
        vk_tok_slice(vk_tok_address(unsafe_c_address<c_ptr<const void>>(probability[0])),count+1),
        vk_tok_slice(output,q))==many;
}

fn iv_control(model: u64,weights: u64,cur: u64,message: u64) -> bool {
    var norm: f32[4608];
    var forget: f32[2304];
    var gain: f32[2304];
    let n=vk_tok_address(unsafe_c_address<c_ptr<const void>>(norm[0]));
    let f=vk_tok_address(unsafe_c_address<c_ptr<const void>>(forget[0]));
    let g=vk_tok_address(unsafe_c_address<c_ptr<const void>>(gain[0]));
    let graph=model+QV_GRAPH;
    return iv_normalize(cur,0,n) && iv_normalize(message,0,n+IV_ROW) &&
        iv_access(model,IV_S_I0,IV_ROW*2,n,1) && fg_vk_graph_begin(graph)!=0 &&
        iv_project(model,6,4608,64,IV_S_I0,weights,IV_S_P0) &&
        iv_project(model,6,64,2304,IV_S_P0,weights+48,IV_S_I1) &&
        iv_project(model,6,64,2304,IV_S_P0,weights+96,IV_S_P5) && fg_vk_graph_submit(graph)!=0 &&
        iv_access(model,IV_S_I1,IV_ROW,f,0) && iv_access(model,IV_S_P5,IV_ROW,g,0) &&
        i2_loop_blend(vk_tok_slice(cur,IV_WIDTH),vk_tok_slice(message,IV_WIDTH),vk_tok_slice(g,IV_WIDTH),
            vk_tok_slice(f,IV_WIDTH),vk_tok_slice(cur,IV_WIDTH))==IV_WIDTH;
}

fn iv_attention(model: u64,layer: u64,cache: u64,input: u64,sliding: bool,output: u64) -> bool {
    let a=layer+IV_LAYER_AUX;
    let w=layer+32;
    let graph=model+QV_GRAPH;
    let sizes: u32[8]=[3072,1024,1024,384,24,3072,32,32];
    let inputs: u64[8]=[0,1,2,0,0,0,1,2];
    let weights: u64[8]=[w,w+48,w+96,a+240,a+384,w+192,a+720,a+768];
    let num: i64=if sliding { 6 } else { 8 };
    for i in 3 {
        if !iv_access(model,i as! u64,IV_ROW,input+(i as! u64)*IV_ROW,1) { return false; }
    }
    if fg_vk_graph_begin(graph)==0 { return false; }
    for i in num {
        if !iv_project(model,6,2304,sizes[i],inputs[i],weights[i],IV_S_P0+(i as! u64)) { return false; }
    }
    if fg_vk_graph_submit(graph)==0 { return false; }
    var projected: f32[8664];
    var qrs: f32[19864];
    var key: f32[1024];
    var value: f32[1024];
    let projected_addr=vk_tok_address(unsafe_c_address<c_ptr<const void>>(projected[0]));
    let p=vk_tok_address(unsafe_c_address<c_ptr<const void>>(qrs[0]));
    let k=vk_tok_address(unsafe_c_address<c_ptr<const void>>(key[0]));
    let v=vk_tok_address(unsafe_c_address<c_ptr<const void>>(value[0]));
    var at: i64=0;
    for i in num {
        if !iv_access(model,IV_S_P0+(i as! u64),(sizes[i] as! u64)*4,projected_addr+(at as! u64)*4,0) { return false; }
        at+=sizes[i] as! i64;
    }
    let position=vk_http_u32(model+IV_POSITION);
    if i2_vk_prepare(vk_tok_slice(projected_addr,at),vk_tok_slice(iv_host(a+192),3072),
        vk_tok_slice(iv_host(a+336),384),vk_tok_slice(iv_host(a),4096),vk_tok_slice(iv_host(a+48),4096),
        position as! i64,sliding,vk_tok_slice(cache+IV_CACHE_KH,3072),vk_tok_slice(cache+IV_CACHE_VH,3072),
        vk_tok_slice(p,3480),vk_tok_slice(k,1024),vk_tok_slice(v,1024)) != 0 { return false; }
    iv_copy(p+13920,iv_host(a+288),65536);
    let rows=vk_http_u32(cache+IV_CACHE_ROWS);
    var store=qv_shape(128,8);
    store.position=position%rows;
    store.context=rows;
    var attend=qv_shape(128,24);
    attend.position=position;
    attend.context=rows;
    attend.kv_heads=8;
    attend.rotary=if sliding { rows } else { 0 };
    let kv_key=qv_view(cache);
    let kv_value=qv_view(cache+24);
    let view_key=iv_s(model,IV_S_KEY);
    let view_value=iv_s(model,IV_S_VALUE);
    return iv_access(model,IV_S_QRS,79456,p,1) && iv_access(model,IV_S_KEY,4096,k,1) &&
        iv_access(model,IV_S_VALUE,4096,v,1) && fg_vk_graph_begin(graph)!=0 &&
        fg_vk_graph_record(graph,20,store,view_key,view_key,kv_key)!=0 &&
        fg_vk_graph_record(graph,20,store,view_value,view_value,kv_value)!=0 &&
        fg_vk_graph_inkling_attention(graph,attend,iv_s(model,IV_S_QRS),kv_key,kv_value,iv_s(model,IV_S_ATTN))!=0 &&
        fg_vk_graph_record(graph,19,qv_shape(3072,0),iv_s(model,IV_S_ATTN),iv_s(model,IV_S_P5),iv_s(model,IV_S_GATED))!=0 &&
        iv_project(model,6,3072,2304,IV_S_GATED,w+144,IV_S_P0) &&
        fg_vk_graph_submit(graph)!=0 && iv_access(model,IV_S_P0,IV_ROW,output,0);
}

fn iv_moe(model: u64,layer: u64,input: u64,out: u64) -> bool {
    let g=model+QV_GRAPH;
    var route=qv_shape(128,8);
    route.theta=8.0 as! f32;
    var selected: u32[16];
    let ids=vk_tok_address(unsafe_c_address<c_ptr<const void>>(selected[0]));
    let routes=iv_s(model,IV_S_ROUTES);
    let branches=iv_s(model,IV_S_BRANCHES);
    let core=layer+32;
    return iv_access(model,IV_S_I0,IV_ROW,input,1) && fg_vk_graph_begin(g)!=0 &&
        iv_project(model,28,2304,128,IV_S_I0,core+240,IV_S_ROUTER) &&
        fg_vk_graph_record(g,35,route,iv_s(model,IV_S_ROUTER),qv_view(core+384),routes)!=0 &&
        fg_vk_graph_submit(g)!=0 && iv_access(model,IV_S_ROUTES,64,ids,0) &&
        fg_vk_expert_bank_stage(model+IV_BANK,vk_gguf_read_word(model+IV_FILE),vk_gguf_read_word(model+IV_FILE_BYTES),
            128,layer+IV_LAYER_PLANES,ids,8)!=0 && fg_vk_graph_begin(g)!=0 &&
        fg_vk_expert_bank_record_inkling(g,model+IV_BANK,2304,1152,iv_s(model,IV_S_I0),iv_s(model,IV_S_GATE_UP),
            iv_s(model,IV_S_ACTIVATION),branches)!=0 &&
        fg_vk_graph_record(g,30,qv_shape(2304,8),branches,routes,iv_s(model,IV_S_P0))!=0 &&
        fg_vk_graph_submit(g)!=0 && iv_access(model,IV_S_P0,IV_ROW,out,0);
}

fn iv_add(dst: u64,src: u64) {
    for i in IV_WIDTH {
        let d=dst+(i as! u64)*4;
        let value=qv_read_f32(d)+qv_read_f32(src+(i as! u64)*4);
        iv_copy(d,vk_tok_address(unsafe_c_address<c_ptr<const void>>(value)),4);
    }
}

fn iv_visit(model: u64,physical: u32,slot: u32,pass: i32,sliding: bool) -> bool {
    if physical>=24 || slot>=48 { return false; }
    let layer=iv_layer(model,physical as! i64);
    let cache=iv_cache(model,slot as! i64);
    let a=layer+IV_LAYER_AUX;
    let current=model+IV_CURRENT;
    let blocks=model+IV_BLOCKS;
    let messages=model+IV_MESSAGES;
    var count=vk_http_u32(model+IV_COUNT) as! i32;
    if physical==6 {
        let prelude=model+IV_PRELUDE;
        if pass==0 {
            iv_copy(prelude,current,IV_ROW);
            vk_http_set_u32(model+IV_PRELUDE_COUNT,count as! u32);
        } else {
            let pre=vk_http_u32(model+IV_PRELUDE_COUNT) as! i32;
            let bank=count-pre-1;
            if bank<0 || bank>=12 { return false; }
            vk_http_set_u32(model+IV_BANK_COUNT,bank as! u32);
            iv_copy(model+IV_LOOP_BANK,blocks+((pre+1) as! u64)*IV_ROW,(bank as! u64)*IV_ROW);
            iv_copy(model+IV_BANK_OUTPUT,current,IV_ROW);
            iv_copy(current,prelude,IV_ROW);
            count=pre;
            vk_http_set_u32(model+IV_COUNT,count as! u32);
        }
    }
    if pass>0 && !iv_depth(model+IV_LOOP_BANK,(vk_http_u32(model+IV_BANK_COUNT) as! i32) as! i64,
        model+IV_BANK_OUTPUT,iv_host(a+816),4,messages) { return false; }
    var input: f32[6912];
    var projection: f32[2304];
    var next: f32[2304];
    var ffn: f32[2304];
    let input_address=vk_tok_address(unsafe_c_address<c_ptr<const void>>(input[0]));
    let p=vk_tok_address(unsafe_c_address<c_ptr<const void>>(projection[0]));
    let n=vk_tok_address(unsafe_c_address<c_ptr<const void>>(next[0]));
    let f=vk_tok_address(unsafe_c_address<c_ptr<const void>>(ffn[0]));
    let q=iv_host(a+480);
    if q != 0 {
        if !iv_depth(blocks,count as! i64,current,q,if sliding { 1 } else { 3 },input_address) { return false; }
        if sliding { iv_copy(input_address+IV_ROW,input_address,IV_ROW); iv_copy(input_address+2*IV_ROW,input_address,IV_ROW); }
    } else {
        for i in 3 { iv_copy(input_address+(i as! u64)*IV_ROW,current,IV_ROW); }
    }
    for i in 3 {
        let row=input_address+(i as! u64)*IV_ROW;
        let norm: u64=if sliding { a+528 } else { a+576+(i as! u64)*48 };
        if (pass>0 && !iv_control(model,a+864+(i as! u64)*144,row,messages+(i as! u64)*IV_ROW)) ||
            !iv_normalize(row,iv_host(norm),row) { return false; }
    }
    if !iv_attention(model,layer,cache,input_address,sliding,p) ||
        i2_postconv_token(vk_tok_slice(p,IV_WIDTH),vk_tok_slice(iv_host(a+96),9216),
            vk_tok_slice(cache+IV_CACHE_AH,6912),vk_tok_slice(n,IV_WIDTH)) != IV_WIDTH { return false; }
    if physical%6==0 {
        if count>=11 { return false; }
        iv_copy(blocks+(count as! u64)*IV_ROW,current,IV_ROW);
        count+=1;
        vk_http_set_u32(model+IV_COUNT,count as! u32);
    } else { iv_add(n,current); }
    if !iv_depth(blocks,count as! i64,n,iv_host(a+432),1,f) ||
        (pass>0 && !iv_control(model,a+1296,f,messages+3*IV_ROW)) ||
        !iv_normalize(f,iv_host(layer+464),f) || !iv_moe(model,layer,f,p) ||
        i2_postconv_token(vk_tok_slice(p,IV_WIDTH),vk_tok_slice(iv_host(a+144),9216),
            vk_tok_slice(cache+IV_CACHE_FH,6912),vk_tok_slice(current,IV_WIDTH)) != IV_WIDTH { return false; }
    iv_add(current,n);
    return true;
}

fn iv_embed(model: u64) -> bool {
    let first=vk_http_u32(model+IV_TOKEN)/32*32;
    var weights=qv_view(model+IV_GLOBAL);
    weights.offset+=(first as! u64)*2448;
    weights.size=78336;
    var gather=qv_shape(2304,32);
    gather.position=first;
    vk_http_set_u32(model+IV_COUNT,0);
    vk_http_set_u32(model+IV_PRELUDE_COUNT,0);
    vk_http_set_u32(model+IV_BANK_COUNT,0);
    let graph=model+QV_GRAPH;
    let current=model+IV_CURRENT;
    return iv_access(model,IV_S_ID,4,model+IV_TOKEN,1) && fg_vk_graph_begin(graph)!=0 &&
        fg_vk_graph_record(graph,27,gather,iv_s(model,IV_S_ID),weights,iv_s(model,IV_S_P0))!=0 &&
        fg_vk_graph_submit(graph)!=0 && iv_access(model,IV_S_P0,IV_ROW,current,0) &&
        iv_normalize(current,iv_host(model+IV_GLOBAL+48),current);
}

fn iv_head(model: u64) -> bool {
    var normalized: f32[2304];
    let n=vk_tok_address(unsafe_c_address<c_ptr<const void>>(normalized[0]));
    let global=model+IV_GLOBAL;
    let graph=model+QV_GRAPH;
    if !iv_depth(model+IV_BLOCKS,(vk_http_u32(model+IV_COUNT) as! i32) as! i64,model+IV_CURRENT,iv_host(global+192),1,n) ||
        !iv_normalize(n,iv_host(global+144),n) || !iv_access(model,IV_S_I0,IV_ROW,n,1) || fg_vk_graph_begin(graph)==0 { return false; }
    var first: u32=0;
    while first<IV_VOCAB {
        let remaining=IV_VOCAB-first;
        let rows: u32=if remaining<32768 { remaining } else { 32768 };
        var weight=qv_view(global+96);
        weight.offset+=(first as! u64)*2448;
        weight.size=(rows as! u64)*2448;
        var shape=qv_shape(2304,rows);
        shape.position=first;
        shape.context=IV_VOCAB;
        if fg_vk_graph_record(graph,6,shape,iv_s(model,IV_S_I0),weight,iv_s(model,IV_S_LOGITS))==0 { return false; }
        first+=rows;
    }
    return fg_vk_graph_submit(graph)!=0;
}

export fn fg_vk_inkling_op(model: u64,op: i64,physical: i64,slot: i64,pass: i64,sliding: bool) -> i64 {
    if op==0 { return if iv_embed(model) { 0 } else { -1 }; }
    if op==1 { return if iv_visit(model,physical as! u32,slot as! u32,pass as! i32,sliding) { 0 } else { -1 }; }
    return if op==2 && iv_head(model) { 0 } else { -1 };
}

export fn fg_vk_inkling_step(model: u64,token: u32,logits: u64) -> i32 {
    let pos=vk_http_u32(model+IV_POSITION);
    if vk_http_u32(model+IV_FAILED)!=0 || token>=IV_VOCAB || pos>=vk_http_u32(model+IV_CAPACITY) { return 0; }
    vk_http_set_u32(model+IV_TOKEN,token);
    let status=fg_vk_inkling_forward(model,vk_tok_slice(model+IV_VISITS,192),logits!=0);
    if status!=0 { vk_http_error(str("Inkling Vulkan: model step failed (",status,")\n")); }
    let ok=status==0 && (logits==0 || (iv_access(model,IV_S_LOGITS,800232,logits,0) &&
        i2_text_logits(vk_tok_slice(logits,200058),200058,30.0 as! f32)==200058));
    if ok { vk_http_set_u32(model+IV_POSITION,pos+1); } else { vk_http_set_u32(model+IV_FAILED,1); }
    return if ok { 1 } else { 0 };
}

export fn fg_vk_inkling_reset(model: u64) -> i32 {
    if vk_http_u32(model+IV_POSITION)==0 && vk_http_u32(model+IV_FAILED)==0 { return 1; }
    if fg_vk_upload_zero(model+IV_RESET,model+IV_STATE)==0 { return 0; }
    for i in 48 { vk_gguf_clear(unsafe_c_ptr<c_ptr<void>>(iv_cache(model,i)+IV_CACHE_KH),0,79872); }
    vk_http_set_u32(model+2768,0);
    vk_http_set_u32(model+IV_POSITION,0);
    vk_http_set_u32(model+IV_FAILED,0);
    return 1;
}
