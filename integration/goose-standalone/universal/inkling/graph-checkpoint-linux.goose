const CKPT_OFF_DATA: u64 = 0;
const CKPT_OFF_MANIFEST: u64 = 8;
const CKPT_OFF_FRAMES: u64 = 16;
const CKPT_OFF_COUNT: u64 = 24;
const CKPT_OFF_LAYERS: u64 = 32;
const CKPT_OFF_OFFSET: u64 = 40;
const CKPT_I64_MAX: i64 = 9223372036854775807;
const CKPT_U64_MAX: u64 = 0xffffffffffffffff;

extern fn fg_graph_native_trace_get() -> u64;
extern fn fg_graph_native_trace_set(address: u64);
extern fn fg_cuda_state_read(address: u64, span: i64) -> i64;
extern fn fg_cuda_activation_read_address(address: u64, span: i64) -> i64;
extern fn ggml_nbytes(tensor: u64) -> u64;
extern fn ggml_row_size(kind: i32, elements: i64) -> u64;
extern fn ggml_is_contiguous(tensor: u64) -> bool;
extern fn ggml_type_name(kind: i32) -> u64;
extern fn graph_ckpt_unix_open(path: u64, flags: i32) -> i32;
extern fn graph_ckpt_unix_openat(directory: i32, name: u64, flags: i32, mode: u32) -> i32;
extern fn graph_ckpt_fdopen(fd: i32, mode: u64) -> u64;
extern fn graph_ckpt_close(fd: i32) -> i32;
extern fn graph_ckpt_fwrite(data: u64, size: u64, count: u64, file: u64) -> u64;

fn ckpt_word(address: u64, value: u64) {
    graph_host_copy(unsafe_c_ptr<c_ptr<void>>(address),unsafe_c_address<c_ptr<const void>>(value),8);
}

fn ckpt_cstring(value: const u8[:], bytes: u8[:]) -> u64 {
    for i in value.len { bytes[i]=value[i]; }
    bytes[value.len]=0;
    return graph_host_address(unsafe_c_address<c_ptr<const void>>(bytes[0]));
}

fn ckpt_path() -> u64 {
    var name: u8[24];
    return fg_cuda_host_getenv(ckpt_cstring("GOOSE_APEX_TRACE_DIR",name));
}

fn ckpt_row_count(tensor: u64) -> i64 { return graph_host_word(tensor+24) as! i64; }

fn ckpt_text(file: u64,text: const u8[:]) -> bool {
    let address=graph_host_address(unsafe_c_slice_data<c_ptr<const void>>(text));
    return graph_ckpt_fwrite(address,1,text.len as! u64,file)==(text.len as! u64);
}

fn ckpt_write(trace: u64,tensor: u64,role: const u8[:],layer: i64,row: i64,rows: i64,persistent: bool,whole: bool) -> bool {
    if tensor==0 { return false; }
    let data=graph_host_word(tensor+248);
    let total_rows=ckpt_row_count(tensor);
    if data==0 || row<0 || rows<=0 || row>total_rows || rows>total_rows-row { return false; }
    let total=ggml_nbytes(tensor);
    let kind=graph_host_i32(tensor);
    let width=ggml_row_size(kind,graph_host_word(tensor+16) as! i64);
    let stride=graph_host_word(tensor+56);
    if width==0 || stride<width || width>total || (row as! u64)>(total-width)/stride { return false; }
    let start=(row as! u64)*stride;
    if ((rows-1) as! u64)>(total-start-width)/stride { return false; }
    if whole && (!ggml_is_contiguous(tensor) || row!=0 || rows!=total_rows) { return false; }
    if !whole && (graph_host_word(tensor+32)!=1 || graph_host_word(tensor+40)!=1) { return false; }
    let span=if whole { total } else { ((rows-1) as! u64)*stride+width };
    let bytes=if whole { total } else { (rows as! u64)*width };
    let offset=graph_host_word(trace+CKPT_OFF_OFFSET);
    if span>(CKPT_I64_MAX as! u64) || bytes>CKPT_U64_MAX-offset { return false; }
    let source=data+start;
    let result=if persistent { fg_cuda_state_read(source,span as! i64) }
        else { fg_cuda_activation_read_address(source,span as! i64) };
    if result!=0 { return false; }

    let file=graph_host_word(trace+CKPT_OFF_DATA);
    if whole || stride==width {
        if graph_ckpt_fwrite(source,1,bytes,file)!=bytes { return false; }
    } else {
        for i in rows {
            if graph_ckpt_fwrite(source+(i as! u64)*stride,1,width,file)!=width { return false; }
        }
    }
    let line=str("{\"role\":\"",role,"\",\"layer\":",layer,",\"dtype\":\"",graph_host_text(ggml_type_name(kind)),
        "\",\"shape\":[",graph_host_word(tensor+16) as! i64,",",total_rows,",",
        graph_host_word(tensor+32) as! i64,",",graph_host_word(tensor+40) as! i64,
        "],\"row\":",row,",\"rows\":",rows,",\"offset\":",offset,",\"bytes\":",bytes,"}\n");
    let recorded=ckpt_text(graph_host_word(trace+CKPT_OFF_MANIFEST),line);
    ckpt_word(trace+CKPT_OFF_OFFSET,offset+bytes);
    return recorded;
}

export fn fg143_graph_observe(address: u64) -> bool {
    let trace=fg_graph_native_trace_get();
    if trace==0 { return false; }
    let roles: (const u8[:])[4]=["post_attention_norm","experts","ffn","layer_output"];
    let tail: (const u8[:])[3]=["head_rms","head_norm","logits"];
    let end=1+4*(graph_host_word(trace+CKPT_OFF_LAYERS) as! i64);
    let frames=graph_host_word(trace+CKPT_OFF_FRAMES);
    let count=graph_host_word(trace+CKPT_OFF_COUNT) as! i64;
    for i in count {
        if graph_host_word(frames+(i as! u64)*8)!=address { continue; }
        let role=if i==0 { "embedding" } else { if i<end { roles[(i-1)%4] } else { tail[i-end] } };
        let layer: i64=if i>0 && i<end { (i-1)/4 } else { -1 };
        let rows=ckpt_row_count(address);
        if i==end+2 || (layer>=0 && (i-1)%4==1) {
            if !ckpt_write(trace,address,role,layer,0,rows,false,false) { return false; }
        } else {
            if !ckpt_write(trace,address,role,layer,0,1,false,false) { return false; }
            if rows>1 && !ckpt_write(trace,address,role,layer,rows-1,1,false,false) { return false; }
        }
    }
    return true;
}

export fn fg_q35_checkpoint_enabled() -> bool {
    let path=ckpt_path();
    return path!=0 && graph_host_byte(path)!=0;
}

fn ckpt_file(directory: i32,prefix: i64,suffix: const u8[:]) -> u64 {
    let digits=str(prefix);
    var zeros: u8[>..]=[];
    if digits.len<12 { for i in 12-digits.len { zeros.push(48); } }
    var name: u8[56];
    let file=graph_ckpt_unix_openat(directory,ckpt_cstring(str("step-",zeros,digits,".",suffix),name),524481,384);
    if file<0 { return 0; }
    let mode: u8[3]=[119,98,0];
    let stream=graph_ckpt_fdopen(file,graph_host_address(unsafe_c_address<c_ptr<const void>>(mode[0])));
    if stream==0 { graph_ckpt_close(file); }
    return stream;
}

fn ckpt_persistent(trace: u64,layers: const i64[:],count: i64,tokens: i64) -> bool {
    for i in count {
        let at=i*32;
        let recurrent=layers[at]==1;
        let left=layers[at+(if recurrent { 20 } else { 22 })] as! u64;
        let right=layers[at+(if recurrent { 21 } else { 23 })] as! u64;
        if left==0 || right==0 { return false; }
        if !ckpt_write(trace,left,if recurrent { "ssm_history" } else { "kv_keys" },i,0,
            if recurrent { ckpt_row_count(left) } else { tokens },true,recurrent) ||
            !ckpt_write(trace,right,if recurrent { "gdn_state" } else { "kv_values" },i,0,
                if recurrent { ckpt_row_count(right) } else { tokens },true,recurrent) { return false; }
    }
    return true;
}

export fn fg_q35_checkpoint_compute(plan: u64,frames: const i64[:],layers: const i64[:],prefix: i64,tokens: i64) -> i32 {
    let path=ckpt_path();
    let count=layers.len/32;
    if path==0 || graph_host_byte(path)==0 || plan==0 || fg_graph_native_trace_get()!=0 || prefix<0 || tokens<=0 ||
        prefix>CKPT_I64_MAX-tokens || count<1 || count>128 || layers.len%32!=0 || frames.len!=4*count+4 { return -1; }
    let directory=graph_ckpt_unix_open(path,589824);
    if directory<0 { return -1; }
    let data=ckpt_file(directory,prefix,"bin");
    let manifest: u64=if data!=0 { ckpt_file(directory,prefix,"jsonl") } else { 0 };
    graph_ckpt_close(directory);
    if data==0 || manifest==0 {
        if data!=0 { graph_storage_fclose(data); }
        graph_host_log(str("Goose: cannot create numerical checkpoint for prefix ",prefix,"\n"));
        return -1;
    }

    var trace: u64[6]=[data,manifest,graph_host_address(unsafe_c_slice_data<c_ptr<const void>>(frames)),
        frames.len as! u64,count as! u64,0];
    let address=graph_host_address(unsafe_c_address<c_ptr<const void>>(trace[0]));
    fg_graph_native_trace_set(address);
    var valid=fg_standalone_graph_plan_compute(plan)==0 && ckpt_persistent(address,layers,count,prefix+tokens);
    fg_graph_native_trace_set(0);
    if graph_storage_fclose(data)!=0 { valid=false; }
    if valid && !ckpt_text(manifest,str("{\"complete\":true,\"prefix\":",prefix,",\"tokens\":",tokens,"}\n")) { valid=false; }
    if graph_storage_fclose(manifest)!=0 { valid=false; }
    if !valid { graph_host_log(str("Goose: numerical checkpoint failed for prefix ",prefix,"\n")); }
    return if valid { 0 } else { -1 };
}
