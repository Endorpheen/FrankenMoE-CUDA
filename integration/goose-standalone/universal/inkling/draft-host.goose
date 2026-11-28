const I2D_CAP: u64 = 82992;
const I2D_POS: u64 = 82996;
const I2D_FMT: u64 = 83000;
const I2D_LOGITS: u64 = 83004;
const I2D_HA: u64 = 887136;
const I2D_RETAIN: u64 = 887152;
const I2D_DRAFT: u64 = 887160;
const I2D_JOINED: u64 = 922608;
const I2D_HISTORY: u64 = 0;
const I2D_SCORES: u64 = 8;
const I2D_SAVED: u64 = 16;
const I2D_TOKENS: u64 = 24;
const I2D_BEGIN: u64 = 88;
const I2D_COUNT: u64 = 92;
const I2D_USED: u64 = 96;
const I2D_GROUPS: u64 = 100;
const I2D_ACCEPTED: u64 = 104;
const I2D_REJECTS: u64 = 108;
const I2D_HISTORY_BYTES: u64 = 1327104;
const I2D_VOCAB: i64 = 200058;

struct I2DraftSpan { address: u64, len: i64 }

extern fn fg_i2_backend_draft_features() -> bool;
extern fn inkling_attention_reset() -> i32;

fn fg_i2_backend_reset_attention(decoder: u64) -> i32 {
    if inkling_attention_reset() != 0 { return -1; }
    i2_node_clear(unsafe_c_ptr<c_ptr<void>>(i2d_read(decoder + 887120)), 0, 589824);
    i2_node_clear(unsafe_c_ptr<c_ptr<void>>(i2d_read(decoder + 887128)), 0, 589824);
    i2_node_clear(unsafe_c_ptr<c_ptr<void>>(i2d_read(decoder + I2D_HA)), 0, I2D_HISTORY_BYTES);
    i2_node_clear(unsafe_c_ptr<c_ptr<void>>(i2d_read(decoder + 887144)), 0, I2D_HISTORY_BYTES);
    return 0;
}
extern fn fg_i2_backend_prefill(decoder: u64,tokens: u64,pos: i32,count: i32,logits: bool);
extern fn fg_cuda_host_getenv(name: u64) -> u64;
extern fn fg_cuda_expert_stats(out: I2DraftSpan);
extern fn fg_cuda_inkling_expert_phase(prefill: bool,pos: i32,count: i32);
extern fn inkling_draft_verification(active: bool);
extern fn inkling_draft_native_snapshot(restore: bool,pos: i32) -> bool;
extern fn inkling_draft_moe_snapshot(restore: bool,pos: i32) -> bool;
extern fn inkling_draft_bridge_history(out: u64,pos: i32) -> bool;
extern fn i2_eval_prepared(decoder: u64,tokens: I2DraftSpan,prompt: bool,logits: I2DraftSpan,
    history: I2DraftSpan,proposals: I2DraftSpan,pos: i64,pending: bool,used: i32,retain: bool) -> i64;
extern "memcpy" fn i2d_copy(dst: c_ptr<void>,src: c_ptr<const void>,n: u64) -> c_ptr<void>;
extern "malloc" fn i2d_malloc(n: u64) -> c_ptr<void>;
extern "free" fn i2d_free(p: c_ptr<void>);
extern "gs_os_write_stderr" fn i2d_log(text: const u8[:]);

fn i2d_address(pointer: c_ptr<const void>) -> u64 {
    var address: u64=0;
    i2d_copy(unsafe_c_address<c_ptr<void>>(address),unsafe_c_address<c_ptr<const void>>(pointer),8);
    return address;
}

fn i2d_read(address: u64) -> u64 {
    var value: u64=0;
    i2d_copy(unsafe_c_address<c_ptr<void>>(value),unsafe_c_ptr<c_ptr<const void>>(address),8);
    return value;
}

fn i2d_int(address: u64) -> i32 {
    var value: i32=0;
    i2d_copy(unsafe_c_address<c_ptr<void>>(value),unsafe_c_ptr<c_ptr<const void>>(address),4);
    return value;
}

fn i2d_byte(address: u64) -> u8 {
    var value: u8=0;
    i2d_copy(unsafe_c_address<c_ptr<void>>(value),unsafe_c_ptr<c_ptr<const void>>(address),1);
    return value;
}

fn i2d_set(address: u64,value: u64) {
    i2d_copy(unsafe_c_ptr<c_ptr<void>>(address),unsafe_c_address<c_ptr<const void>>(value),8);
}

fn i2d_set_int(address: u64,value: i32) {
    i2d_copy(unsafe_c_ptr<c_ptr<void>>(address),unsafe_c_address<c_ptr<const void>>(value),4);
}

fn i2d_set_byte(address: u64,value: u8) {
    i2d_copy(unsafe_c_ptr<c_ptr<void>>(address),unsafe_c_address<c_ptr<const void>>(value),1);
}

fn i2d_transfer(dst: u64,src: u64,bytes: u64) {
    i2d_copy(unsafe_c_ptr<c_ptr<void>>(dst),unsafe_c_ptr<c_ptr<const void>>(src),bytes);
}

fn i2d_alloc(bytes: u64) -> u64 {
    let pointer=i2d_malloc(bytes);
    var address: u64=0;
    i2d_copy(unsafe_c_address<c_ptr<void>>(address),unsafe_c_address<c_ptr<const void>>(pointer),8);
    return address;
}

fn i2d_env_is(name: const u8[:],value: u8) -> bool {
    var key: u8[32];
    for i in name.len { key[i]=name[i]; }
    key[name.len]=0;
    let address=fg_cuda_host_getenv(i2d_address(unsafe_c_address<c_ptr<const void>>(key[0])));
    return address!=0 && i2d_byte(address)==value && i2d_byte(address+1)==0;
}

fn i2d_span(address: u64,len: i64) -> I2DraftSpan { return I2DraftSpan {address:address,len:len}; }

fn i2d_clear_bridge(decoder: u64) {
    for i in 48 { i2d_set_byte(decoder+I2D_JOINED+360+(i as! u64)*368,0); }
}

export fn inkling_backend_reset(decoder: u64) -> i32 {
    if decoder==0 || fg_i2_backend_reset_attention(decoder)!=0 { return -1; }
    let draft=decoder+I2D_DRAFT;
    let groups=i2d_int(draft+I2D_GROUPS) as! u32;
    if groups!=0 {
        i2d_log(str("Inkling speculation groups=",groups," cached=",i2d_int(draft+I2D_ACCEPTED) as! u32,
            " rejects=",i2d_int(draft+I2D_REJECTS) as! u32,"\n"));
    }
    i2d_set_int(decoder+I2D_POS,0);
    i2d_set_int(draft+I2D_COUNT,0);
    i2d_set_int(draft+I2D_USED,0);
    i2d_set_int(draft+I2D_GROUPS,0);
    i2d_set_int(draft+I2D_ACCEPTED,0);
    i2d_set_int(draft+I2D_REJECTS,0);
    i2d_clear_bridge(decoder);
    return 0;
}

fn i2d_init(decoder: u64) -> bool {
    let draft=decoder+I2D_DRAFT;
    if i2d_read(draft+I2D_HISTORY)!=0 { return true; }
    if !i2d_env_is("I2_NGRAM_VERIFY",49) || i2d_int(decoder+I2D_POS)!=0 ||
        i2d_int(decoder+I2D_FMT)!=1 || !fg_i2_backend_draft_features() { return false; }
    let history=i2d_alloc((i2d_int(decoder+I2D_CAP) as! u64)*4);
    let scores=i2d_alloc((I2D_VOCAB as! u64)*64);
    let saved=i2d_alloc(I2D_HISTORY_BYTES);
    if history==0 || scores==0 || saved==0 {
        i2d_free(unsafe_c_ptr<c_ptr<void>>(history));
        i2d_free(unsafe_c_ptr<c_ptr<void>>(scores));
        i2d_free(unsafe_c_ptr<c_ptr<void>>(saved));
        return false;
    }
    i2d_set(draft+I2D_HISTORY,history);
    i2d_set(draft+I2D_SCORES,scores);
    i2d_set(draft+I2D_SAVED,saved);
    return true;
}

export fn i2_adapter_draft_start(decoder: u64,count: i32) -> bool {
    let draft=decoder+I2D_DRAFT;
    let position=i2d_int(decoder+I2D_POS);
    let history=i2d_read(decoder+I2D_HA);
    let saved=i2d_read(draft+I2D_SAVED);
    i2d_transfer(saved,history,I2D_HISTORY_BYTES);
    if (i2d_byte(decoder+I2D_JOINED+360)!=0 && !inkling_draft_bridge_history(history,position)) ||
        !inkling_draft_native_snapshot(false,position) || !inkling_draft_moe_snapshot(false,position) {
        i2d_transfer(history,saved,I2D_HISTORY_BYTES);
        return false;
    }
    let preserve=!i2d_env_is("I2_VERIFY_LFU",48);
    var before: i64[6]=[0;6];
    var after: i64[6]=[0;6];
    fg_cuda_expert_stats(i2d_span(i2d_address(unsafe_c_address<c_ptr<const void>>(before[0])),6));
    let fills=before[1];
    let h2d=before[3];
    fg_cuda_inkling_expert_phase(!preserve,position,count);
    i2d_set_int(draft+I2D_COUNT,count);
    inkling_draft_verification(true);
    fg_i2_backend_prefill(decoder,draft+I2D_TOKENS,position,count,true);
    inkling_draft_verification(false);
    i2d_set_int(draft+I2D_BEGIN,position);
    i2d_set_int(draft+I2D_USED,0);
    i2d_set_int(draft+I2D_GROUPS,((i2d_int(draft+I2D_GROUPS) as! u32)+1) as! i32);
    fg_cuda_expert_stats(i2d_span(i2d_address(unsafe_c_address<c_ptr<const void>>(after[0])),6));
    i2d_log(str("Inkling speculation start position=",position," count=",count," lfu=",if preserve { 1 } else { 0 },
        " fills=",after[1]-fills," h2d=",after[3]-h2d,"\n"));
    return true;
}

export fn i2_adapter_draft_reject(decoder: u64) -> bool {
    let draft=decoder+I2D_DRAFT;
    let position=i2d_int(draft+I2D_BEGIN);
    if !inkling_draft_native_snapshot(true,position) || !inkling_draft_moe_snapshot(true,position) { return false; }
    i2d_transfer(i2d_read(decoder+I2D_HA),i2d_read(draft+I2D_SAVED),I2D_HISTORY_BYTES);
    let retain=i2d_byte(decoder+I2D_RETAIN);
    i2d_set_byte(decoder+I2D_RETAIN,1);
    let used=i2d_int(draft+I2D_USED);
    for i in (used as! i64) {
        let next=position+(i as! i32);
        fg_cuda_inkling_expert_phase(false,next,1);
        fg_i2_backend_token(decoder,i2d_int(draft+I2D_TOKENS+(i as! u64)*4),next,false);
    }
    i2d_set_byte(decoder+I2D_RETAIN,retain);
    i2d_log(str("Inkling speculation rejected position=",position," used=",used," count=",i2d_int(draft+I2D_COUNT),"\n"));
    i2d_set_int(draft+I2D_REJECTS,((i2d_int(draft+I2D_REJECTS) as! u32)+1) as! i32);
    i2d_set_int(draft+I2D_COUNT,0);
    i2d_set_int(draft+I2D_USED,0);
    return true;
}

export fn i2_adapter_draft_take(decoder: u64,logits: f32[:]) {
    let draft=decoder+I2D_DRAFT;
    let used=i2d_int(draft+I2D_USED);
    if logits.len!=0 {
        i2d_transfer(i2d_address(unsafe_c_slice_data<c_ptr<const void>>(logits)),
            i2d_read(draft+I2D_SCORES)+(used as! u64)*(I2D_VOCAB as! u64)*4,(logits.len as! u64)*4);
    }
    if used!=0 { i2d_set_int(draft+I2D_ACCEPTED,((i2d_int(draft+I2D_ACCEPTED) as! u32)+1) as! i32); }
    i2d_set_int(draft+I2D_USED,used+1);
    i2d_set_int(decoder+I2D_POS,i2d_int(decoder+I2D_POS)+1);
    let count=i2d_int(draft+I2D_COUNT);
    if used+1==count {
        i2d_log(str("Inkling speculation committed position=",i2d_int(draft+I2D_BEGIN)," count=",count,"\n"));
        i2d_clear_bridge(decoder);
        i2d_set_int(draft+I2D_COUNT,0);
        i2d_set_int(draft+I2D_USED,0);
    }
}

export fn i2_adapter_run_step(decoder: u64,tokens: const i32[:],prompt: bool,logits: f32[:]) {
    let position=i2d_int(decoder+I2D_POS);
    let count=tokens.len as! i32;
    fg_cuda_inkling_expert_phase(prompt,position,count);
    if prompt {
        fg_i2_backend_prefill(decoder,i2d_address(unsafe_c_slice_data<c_ptr<const void>>(tokens)),
            position,count,logits.len!=0);
    } else { fg_i2_backend_token(decoder,tokens[0],position,logits.len!=0); }
    i2d_set_int(decoder+I2D_POS,i2d_int(decoder+I2D_POS)+count);
    if logits.len!=0 {
        i2d_transfer(i2d_address(unsafe_c_slice_data<c_ptr<const void>>(logits)),decoder+I2D_LOGITS,(logits.len as! u64)*4);
    }
}

export fn inkling_backend_eval(decoder: u64,tokens: u64,count: i64,prompt: bool,logits: u64,logits_count: i64) -> i64 {
    if decoder==0 || tokens==0 || count<=0 || count>4096 || (!prompt && count!=1) ||
        (logits_count!=0 && (logits_count!=I2D_VOCAB || logits==0)) { return -1; }
    let position=i2d_int(decoder+I2D_POS);
    let capacity=i2d_int(decoder+I2D_CAP);
    if count>((capacity-position) as! i64) { return -1; }
    for i in count {
        let token=i2d_int(tokens+(i as! u64)*4);
        if token<0 || (token as! i64)>=I2D_VOCAB { return -1; }
    }
    let retain=!prompt && !i2d_env_is("I2_RETAIN_DECODE_GRAPHS",48);
    i2d_set_byte(decoder+I2D_RETAIN,if retain { 1 } else { 0 });
    let ready=i2d_init(decoder);
    let draft=decoder+I2D_DRAFT;
    return i2_eval_prepared(decoder,i2d_span(tokens,count),prompt,i2d_span(logits,logits_count),
        i2d_span(i2d_read(draft+I2D_HISTORY),if ready { capacity as! i64 } else { 0 }),
        i2d_span(draft+I2D_TOKENS,16),position as! i64,i2d_int(draft+I2D_COUNT)!=0,i2d_int(draft+I2D_USED),retain);
}
