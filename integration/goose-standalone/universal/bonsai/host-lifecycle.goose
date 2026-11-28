const MODEL_PAGE_SIZE: u64 = 16;
const MODEL_OWNER: u64 = 24;
const MODEL_LAYER_COUNT: u64 = 368;
const MODEL_EXPERT_LIMIT: u64 = 376;
const MODEL_ACFG: u64 = 384;
const MODEL_GCFG: u64 = 496;
const MODEL_MCFG: u64 = 552;
const MODEL_KV_STATE: u64 = 576;
const MODEL_HANDLES: u64 = 624;
const MODEL_RECORDS: u64 = 656;
const MODEL_INKLING_MODEL: u64 = 664;
const MODEL_GIGA_FLAG: u64 = 672;
const MODEL_BONSAI_MODEL: u64 = 680;
const MODEL_KV_FORMAT: u64 = 696;

const MODEL_Q4_0: i64 = 1;
const MODEL_F16: i64 = 2;

extern fn fg_cuda_model_getenv(key: u64) -> u64;
extern fn fg_cuda_gigachat4_select_profile();
extern fn giga_goose_init(argc: i32, argv: u64);
extern fn giga_server_open(base: u64, size: u64, capacity: i64) -> i32;
extern fn giga_server_reset();
extern fn giga_server_step(tokens: u64, count: i64, rows: i64, output: u64, limit: i64) -> i64;
extern fn inkling_model_init(argc: i32, argv: u64);
extern fn inkling_backend_open(base: u64, size: u64, capacity: i64, expert_cache: i64, format: i32) -> u64;
extern fn inkling_backend_eval(model: u64, tokens: u64, count: i64, prefill: bool, output: u64, limit: i64) -> i64;
extern fn inkling_backend_reset(model: u64) -> i32;
extern fn bonsai_session_open(path: u64, image: u64, frozen: u64, capacity: u32, batch: u32) -> u64;
extern fn bonsai_session_reset(model: u64) -> i32;
extern fn bonsai_session_step(model: u64, tokens: u64, count: u32, output: u64) -> i32;
extern fn dw_ram_residency(base: u64, bytes: i64, page: i64, out: VocabSpan) -> i64;
extern fn dw_ram_warm(base: u64, bytes: i64, page: i64, out: VocabSpan) -> i64;

extern fn qr_model_open(file: VocabSpan, base: u64, bytes: i64, state: VocabSpan, meta: VocabSpan, tensors: VocabSpan,
    meta_slots: VocabSpan, tensor_slots: VocabSpan, capacity: i64, max_positions: i64, owner: VocabSpan, records: VocabSpan,
    handles: VocabSpan, acfg: VocabSpan, gcfg: VocabSpan, mcfg: VocabSpan, sections: VocabSpan, kv: VocabSpan,
    name: VocabSpan, scratch: VocabSpan) -> i64;
extern fn qr_reset_state(owner: VocabSpan, records: VocabSpan, kv: VocabSpan) -> i64;
extern fn qr_cuda_state(owner: VocabSpan, records: VocabSpan, handle: i64) -> i64;
extern fn qr_cuda_dense(owner: VocabSpan, records: VocabSpan, handle: i64, device: i64) -> i64;
extern fn qr_cuda_experts(owner: VocabSpan, records: VocabSpan, handle: i64, requested: i64, result: VocabSpan) -> i64;
extern fn q35_model_step(owner: VocabSpan, handles: VocabSpan, records: VocabSpan, acfg: VocabSpan, gcfg: VocabSpan,
    mcfg: VocabSpan, kv: VocabSpan, tokens: VocabSpan, rows: i64, out: VocabSpan) -> i64;

var model_kv_requested: i64[2];

fn model_i64(address: u64) -> i64 { return vocab_word(address) as! i64; }

fn model_kv_compatible(kind: i64) -> bool {
    return (model_kv_requested[0]==0 || model_kv_requested[0]==kind) &&
        (model_kv_requested[1]==0 || model_kv_requested[1]==kind);
}

export fn hose_configure_kv(value: bool, format: u64) -> i64 {
    let length=vocab_strlen(format);
    var kind: i64=0;
    if length==4 && vocab_byte(format)==113 && vocab_byte(format+1)==52 &&
        vocab_byte(format+2)==95 && vocab_byte(format+3)==48 { kind=MODEL_Q4_0; }
    else if length==3 && vocab_byte(format)==102 && vocab_byte(format+1)==49 &&
        vocab_byte(format+2)==54 { kind=MODEL_F16; }
    if kind==0 { return -1; }
    model_kv_requested[if value { 1 } else { 0 }]=kind;
    return 0;
}

export fn hose_kv_format() -> i64 { return model_i64(fg_cuda_model_host_address()+MODEL_KV_FORMAT); }

fn model_env(name: const u8[:]) -> u64 {
    var text: u8[32];
    for i in name.len { text[i]=name[i]; }
    text[name.len]=0;
    return fg_cuda_model_getenv(vocab_address(unsafe_c_address<c_ptr<const void>>(text[0])));
}

fn model_owner(host: u64) -> VocabSpan { return vocab_span(host+MODEL_OWNER,13); }
fn model_records(host: u64) -> VocabSpan { return vocab_table(host,MODEL_RECORDS,model_i64(host+MODEL_LAYER_COUNT)*32); }

fn model_warm(host: u64,out: u64) -> i64 {
    return dw_ram_warm(vocab_word(host+VOCAB_BASE),model_i64(host+VOCAB_SIZE),
        model_i64(host+MODEL_PAGE_SIZE),vocab_span(out,4));
}

export fn hose_model_residency(out: u64) -> i64 {
    let host=fg_cuda_model_host_address();
    return dw_ram_residency(vocab_word(host+VOCAB_BASE),model_i64(host+VOCAB_SIZE),
        model_i64(host+MODEL_PAGE_SIZE),vocab_span(out,4));
}

export fn fg_q35_generation_prepare() -> i64 {
    let host=fg_cuda_model_host_address();
    if vocab_word(host+MODEL_BONSAI_MODEL)!=0 { return 0; }
    var residency: i64[4]=[0;4];
    let out=vocab_address(unsafe_c_address<c_ptr<const void>>(residency[0]));
    if hose_model_residency(out)!=0 { return -1; }
    if residency[1]==residency[0] { return 0; }
    return model_warm(host,out);
}

fn model_open_trunk(host: u64,capacity: i64) {
    let records=vocab_alloc(4096,8);
    let count=model_i64(host+VOCAB_STATE+24)+4;
    let scratch=vocab_alloc(count,8);
    vocab_set(host+MODEL_RECORDS,records);
    var sections: i64[4]=[0;4];
    var name: u8[128]=[0;128];
    let layers=qr_model_open(vocab_file(host),vocab_word(host+VOCAB_BASE),model_i64(host+VOCAB_SIZE),
        vocab_span(host+VOCAB_STATE,8),vocab_table(host,VOCAB_KV,2048),vocab_table(host,VOCAB_TENSORS,24576),
        vocab_table(host,VOCAB_KV_SLOTS,512),vocab_table(host,VOCAB_TENSOR_SLOTS,8192),capacity,2147483647,
        model_owner(host),vocab_span(records,4096),vocab_span(host+MODEL_HANDLES,4),vocab_span(host+MODEL_ACFG,14),
        vocab_span(host+MODEL_GCFG,7),vocab_span(host+MODEL_MCFG,3),
        vocab_span(vocab_address(unsafe_c_address<c_ptr<const void>>(sections[0])),4),vocab_span(host+MODEL_KV_STATE,6),
        vocab_span(vocab_address(unsafe_c_address<c_ptr<const void>>(name[0])),128),vocab_span(scratch,count));
    vocab_free(unsafe_c_ptr<c_ptr<void>>(scratch));
    vocab_set(host+MODEL_LAYER_COUNT,layers as! u64);
    if layers<=0 {
        vocab_log(str("Goose model loader returned ",layers,"\n"));
        vocab_fatal("model loader");
    }
    if vocab_word(host+MODEL_OWNER+64)!=vocab_word(host+VOCAB_TOKENS) { vocab_fatal("model vocabulary size"); }
}

fn model_open_apex(host: u64,capacity: i64,requested: i64) {
    if !model_kv_compatible(MODEL_Q4_0) { vocab_fatal("Apex requires q4_0 for both KV caches"); }
    vocab_set(host+MODEL_KV_FORMAT,MODEL_Q4_0 as! u64);
    model_open_trunk(host,capacity);
    var residency: i64[4]=[0;4];
    let status=model_warm(host,vocab_address(unsafe_c_address<c_ptr<const void>>(residency[0])));
    if status!=0 {
        vocab_log(str("Goose model RAM: ",residency[3]," of ",residency[2]," pages resident, status ",status,"\n"));
        vocab_fatal("model RAM residency");
    }
    let owner=model_owner(host);
    let records=model_records(host);
    let handle=model_i64(host+MODEL_HANDLES+8);
    if qr_cuda_state(owner,records,handle)<0 { vocab_fatal("model CUDA state"); }
    if qr_cuda_dense(owner,records,handle,model_i64(host+MODEL_HANDLES+16))<0 { vocab_fatal("model CUDA placement"); }
    var expert: i64[2]=[0,0];
    let bytes=qr_cuda_experts(owner,records,handle,requested,
        vocab_span(vocab_address(unsafe_c_address<c_ptr<const void>>(expert[0])),2));
    if bytes==-2 { vocab_fatal("requested GPU expert cache requires an available CUDA device"); }
    if bytes==-3 {
        vocab_log(str("Goose GPU expert cache: requested ",requested/1048576,
            " MiB, available ",expert[0]," bytes after model allocations\n"));
        vocab_exit(1);
    }
    if bytes==-4 { vocab_fatal("requested GPU expert cache cannot hold one layer of experts per group"); }
    if bytes==-5 { vocab_fatal("model has no supported GPU expert cache groups"); }
    if bytes<0 { vocab_fatal("model CUDA expert cache"); }
    vocab_set(host+MODEL_EXPERT_LIMIT,expert[1] as! u64);
    if fg_q35_generation_prepare()!=0 { vocab_fatal("model RAM residency after CUDA placement"); }
}

fn model_set_context(host: u64,capacity: i64,layers: u64) {
    vocab_set(host+MODEL_OWNER+80,capacity as! u64);
    vocab_set(host+MODEL_LAYER_COUNT,layers);
}

export fn hose_host_load_context(path: u64,capacity: i64) {
    let host=fg_cuda_model_host_address();
    model_host_map(host,path);
    load_vocab();
    if vocab_byte(host+VOCAB_BONSAI_LAYOUT)!=0 {
        var prefill: i64[2];
        hose_prefill_profile(vocab_address(unsafe_c_address<c_ptr<const void>>(prefill[0])));
        if capacity<=0 || capacity>262144 || prefill[1]<=0 || prefill[1]>1024 ||
            !model_kv_compatible(MODEL_Q4_0) || vocab_word(host+VOCAB_TOKENS)!=248320 {
            vocab_fatal("Bonsai context, ubatch, KV or vocabulary");
        }
        let session=bonsai_session_open(path,model_env("BONSAI_CUDA_IMAGE"),model_env("BONSAI_FROZEN_PTX"),
            capacity as! u32,prefill[1] as! u32);
        if session==0 { vocab_fatal("Bonsai model or CUDA placement"); }
        vocab_set(host+MODEL_BONSAI_MODEL,session);
        vocab_set(host+MODEL_KV_FORMAT,MODEL_Q4_0 as! u64);
        model_set_context(host,capacity,64);
        return;
    }
    let architecture=vocab_text("general.architecture");
    let base=vocab_word(host+VOCAB_BASE);
    let size=vocab_word(host+VOCAB_SIZE);
    if vocab_meta_is(host,architecture,vocab_text("inkling2_loop")) {
        inkling_model_init(0,0);
        let k=model_kv_requested[0];
        let v=model_kv_requested[1];
        let format=if k!=0 { k } else { if v!=0 { v } else { MODEL_Q4_0 } };
        vocab_set(host+MODEL_KV_FORMAT,format as! u64);
        if !model_kv_compatible(format) { vocab_fatal("Inkling requires matching formats for K and V"); }
        if vocab_word(host+VOCAB_TOKENS)!=200058 || vocab_byte(host+VOCAB_INKLING)==0 { vocab_fatal("Inkling vocabulary"); }
        let requested=fg_cuda_model_expert_request();
        let decoder=inkling_backend_open(base,size,capacity,requested,if format==MODEL_Q4_0 { 1 } else { 0 });
        if decoder==0 { vocab_fatal("Inkling model loading or CUDA placement"); }
        vocab_set(host+MODEL_INKLING_MODEL,decoder);
        model_set_context(host,capacity,24);
        vocab_set(host+MODEL_EXPERT_LIMIT,requested as! u64);
        return;
    }
    if vocab_meta_is(host,architecture,vocab_text("gigachat4")) {
        fg_cuda_gigachat4_select_profile();
        if fg_q35_generation_prepare()!=0 { vocab_fatal("GigaChat4 model RAM residency"); }
        giga_goose_init(0,0);
        if vocab_word(host+VOCAB_TOKENS)!=128256 || giga_server_open(base,size,capacity)!=0 {
            vocab_fatal("GigaChat4 model loader");
        }
        vocab_set_byte(host+MODEL_GIGA_FLAG,1);
        model_set_context(host,capacity,32);
        return;
    }
    model_open_apex(host,capacity,fg_cuda_model_expert_request());
}

export fn hose_host_load(path: u64) { hose_host_load_context(path,2048); }

export fn hose_reset_state() {
    let host=fg_cuda_model_host_address();
    let bonsai=vocab_word(host+MODEL_BONSAI_MODEL);
    let inkling=vocab_word(host+MODEL_INKLING_MODEL);
    if bonsai!=0 {
        if bonsai_session_reset(bonsai)!=0 { vocab_fatal("Bonsai model state reset"); }
    } else if inkling!=0 {
        if inkling_backend_reset(inkling)!=0 { vocab_fatal("Inkling model state reset"); }
    } else if vocab_byte(host+MODEL_GIGA_FLAG)!=0 { giga_server_reset(); }
    else if qr_reset_state(model_owner(host),model_records(host),vocab_span(host+MODEL_KV_STATE,6))!=0 {
        vocab_fatal("model state reset");
    }
}

fn model_step_raw(host: u64,tokens: VocabSpan,rows: i64,output: VocabSpan) -> i64 {
    if tokens.address==0 || (output.len!=0 && output.address==0) { return -1; }
    if vocab_byte(host+MODEL_GIGA_FLAG)!=0 {
        return giga_server_step(tokens.address,tokens.len,rows,output.address,output.len);
    }
    let status=q35_model_step(model_owner(host),vocab_span(host+MODEL_HANDLES,4),model_records(host),
        vocab_span(host+MODEL_ACFG,14),vocab_span(host+MODEL_GCFG,7),vocab_span(host+MODEL_MCFG,3),
        vocab_span(host+MODEL_KV_STATE,6),tokens,rows,output);
    if status==-2 { vocab_fatal("model step recovery"); }
    return status;
}

export fn fg_cuda_model_step(tokens: const i32[:], rows: i64, output: f32[:]) -> i64 {
    let input=vocab_span(vocab_address(unsafe_c_slice_data<c_ptr<const void>>(tokens)),tokens.len);
    let out=vocab_span(vocab_address(unsafe_c_slice_data<c_ptr<const void>>(output)),output.len);
    return model_step_raw(fg_cuda_model_host_address(),input,rows,out);
}

fn model_try_logits(count: u64) -> u64 {
    let pointer=vocab_malloc(count*4);
    var address: u64=0;
    vocab_memcpy(unsafe_c_address<c_ptr<void>>(address),unsafe_c_address<c_ptr<const void>>(pointer),8);
    return address;
}

export fn hose_step(tokens: u64,count: i64,logits: u64) -> i64 {
    if logits==0 { return -1; }
    vocab_set(logits,0);
    let host=fg_cuda_model_host_address();
    let vocab=model_i64(host+VOCAB_TOKENS);
    let bonsai=vocab_word(host+MODEL_BONSAI_MODEL);
    let inkling=vocab_word(host+MODEL_INKLING_MODEL);
    if bonsai!=0 {
        if count!=1 { return -1; }
        let row=model_try_logits(vocab as! u64);
        if row==0 { return -1; }
        if bonsai_session_step(bonsai,tokens,1,row)!=0 {
            vocab_free(unsafe_c_ptr<c_ptr<void>>(row));
            return -1;
        }
        vocab_set(logits,row);
        return vocab;
    }
    if (inkling!=0 && count!=1) || count<=0 || count>4096 || vocab<=0 ||
        (vocab as! u64)>0x3fffffffffffffff/(count as! u64) { return -1; }
    let capacity=count*vocab;
    let buffer=model_try_logits(capacity as! u64);
    if buffer==0 { return -1; }
    let size=if inkling!=0 { inkling_backend_eval(inkling,tokens,count,false,buffer,capacity) }
        else { model_step_raw(host,vocab_span(tokens,count),-1,vocab_span(buffer,capacity)) };
    if size<0 { vocab_free(unsafe_c_ptr<c_ptr<void>>(buffer)); } else { vocab_set(logits,buffer); }
    return size;
}
