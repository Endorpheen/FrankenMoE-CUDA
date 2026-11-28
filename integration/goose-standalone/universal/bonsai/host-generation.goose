extern fn fg_host_decode_tls_address() -> u64;
extern fn fg_host_create_free_key(key: u64) -> i32;
extern fn vocab_decode_setspecific(key: u32, data: u64) -> i32;

var decode_free_key: u32;
var decode_cleanup_attempted: bool;
var decode_cleanup_available: bool;

fn fg_cuda_decode_exchange_address() -> u64 { return fg_host_decode_tls_address(); }
fn fg_cuda_decode_plan_address() -> u64 { return fg_host_decode_tls_address() + 24; }

fn fg_cuda_decode_cleanup_ready() -> bool {
    if !bonsai_init_lock() { return false; }
    if !decode_cleanup_attempted {
        let key = vocab_address(unsafe_c_address<c_ptr<const void>>(decode_free_key));
        decode_cleanup_available = fg_host_create_free_key(key) == 0;
        decode_cleanup_attempted = true;
    }
    let available = decode_cleanup_available;
    return bonsai_init_unlock() && available;
}

fn fg_cuda_decode_track_buffer(address: u64) -> i32 {
    return vocab_decode_setspecific(decode_free_key, address);
}
var generation_ids: i64[262144];

export fn fg_cuda_generation_ids_address() -> u64 {
    return vocab_address(unsafe_c_address<c_ptr<const void>>(generation_ids[0]));
}
extern fn fg_goose_host_ensure_init();
extern fn fg_standalone_set_cpu_prefill(prefill: bool) -> bool;
extern fn fg_q35_checkpoint_enabled() -> bool;
extern fn q35_prefill_valid(batch: i64, microbatch: i64) -> bool;
extern fn bt_is_eog(ids: VocabSpan, count: i64, id: i64) -> i64;
extern fn q35_decode_plan_close(owner: VocabSpan, plan: VocabSpan) -> i64;
extern fn q35_model_step_cached(owner: VocabSpan, handles: VocabSpan, records: VocabSpan, acfg: VocabSpan,
    gcfg: VocabSpan, mcfg: VocabSpan, kv: VocabSpan, tokens: VocabSpan, logits: VocabSpan, plan: VocabSpan) -> i64;
extern fn q35_generate(prompt: VocabSpan, vocab: i64, capacity: i64, eog: VocabSpan,
    limit: i64, ignore_eos: bool, ids: VocabSpan, timing: VocabSpan) -> i64;
extern fn fg_q35_topk_scan_enabled() -> bool;

var model_prefill_batch: i64 = 128;
var model_prefill_microbatch: i64 = 128;
var model_expert_cache_request: i64;

const DECODE_PLAN_ENABLED: u64 = 4;
const DECODE_PLAN_WORDS: u64 = 8;
const GENERATION_MAX_BYTES4: u64 = 0x3fffffffffffffff;
const GENERATION_MAX_BYTES: u64 = 0xffffffffffffffff;

fn generation_copy(destination: u64, source: u64, bytes: u64) {
    vocab_memcpy(unsafe_c_ptr<c_ptr<void>>(destination),unsafe_c_ptr<c_ptr<const void>>(source),bytes);
}

fn generation_address(text: const i32[:]) -> u64 {
    return vocab_address(unsafe_c_slice_data<c_ptr<const void>>(text));
}

fn generation_output(text: f32[:]) -> u64 {
    return vocab_address(unsafe_c_slice_data<c_ptr<const void>>(text));
}

fn generation_alloc(bytes: u64) -> u64 {
    let pointer=vocab_malloc(bytes);
    var address: u64=0;
    vocab_memcpy(unsafe_c_address<c_ptr<void>>(address),unsafe_c_address<c_ptr<const void>>(pointer),8);
    return address;
}

fn generation_borrow(bytes: u64) -> u64 {
    let state=fg_cuda_decode_exchange_address();
    if vocab_byte(state+16)!=0 { return 0; }
    var address=vocab_word(state);
    if vocab_word(state+8)<bytes {
        if !fg_cuda_decode_cleanup_ready() { return 0; }
        let replacement=generation_alloc(bytes);
        if replacement==0 { return 0; }
        if fg_cuda_decode_track_buffer(replacement)!=0 {
            vocab_free(unsafe_c_ptr<c_ptr<void>>(replacement));
            return 0;
        }
        vocab_free(unsafe_c_ptr<c_ptr<void>>(address));
        vocab_set(state,replacement);
        vocab_set(state+8,bytes);
        address=replacement;
    }
    vocab_set_byte(state+16,1);
    return address;
}

fn generation_read_u32(address: u64) -> u32 {
    var value: u32=0;
    vocab_memcpy(unsafe_c_address<c_ptr<void>>(value),unsafe_c_ptr<c_ptr<const void>>(address),4);
    return value;
}

fn generation_set_u32(address: u64,value: u32) {
    vocab_memcpy(unsafe_c_ptr<c_ptr<void>>(address),unsafe_c_address<c_ptr<const void>>(value),4);
}

fn generation_env_equal(key: const u8[:],byte: u8) -> bool {
    let value=model_env(key);
    return value!=0 && vocab_byte(value)==byte && vocab_byte(value+1)==0;
}

fn generation_eog(host: u64) -> VocabSpan {
    return vocab_table(host,VOCAB_EOG,model_i64(host+VOCAB_EOG_COUNT));
}

export fn hose_tokenize(text: u64,len: i64,ids: u64,capacity: i64) -> i64 {
    return tokenize(text,len,ids,capacity);
}

export fn hose_detokenize(ids: u64,count: i64,out: u64,capacity: i64) -> i64 {
    return detokenize(ids,count,out,capacity);
}

export fn hose_eog_is(id: i64) -> i64 {
    let eog=generation_eog(fg_cuda_model_host_address());
    return bt_is_eog(eog,eog.len,id);
}

export fn hose_stats(out: u64) {
    let host=fg_cuda_model_host_address();
    let fields: u64[5]=[VOCAB_TOKENS,VOCAB_SPECIALS,VOCAB_EOG_COUNT,VOCAB_EOS,MODEL_LAYER_COUNT];
    for i in fields.len { vocab_set(out+(i as! u64)*8,vocab_word(host+fields[i])); }
}

export fn hose_context_capacity() -> i64 { return model_i64(fg_cuda_model_host_address()+MODEL_OWNER+80); }

export fn hose_configure_prefill(batch: i64,microbatch: i64) -> i64 {
    fg_goose_host_ensure_init();
    if !q35_prefill_valid(batch,microbatch) { return -1; }
    model_prefill_batch=batch;
    model_prefill_microbatch=microbatch;
    return 0;
}

export fn hose_prefill_profile(profile: u64) {
    vocab_set(profile,model_prefill_batch as! u64);
    vocab_set(profile+8,model_prefill_microbatch as! u64);
}

export fn hose_configure_expert_cache(mib: i64) -> i64 {
    if mib<=0 || mib>0x7fffffffffffffff/1048576 { return -1; }
    model_expert_cache_request=mib*1048576;
    return 0;
}

fn fg_cuda_model_expert_request() -> i64 { return model_expert_cache_request; }

export fn hose_expert_cache_limit() -> i64 { return model_i64(fg_cuda_model_host_address()+MODEL_EXPERT_LIMIT); }

fn generation_apex(host: u64) -> bool {
    return vocab_word(host+MODEL_BONSAI_MODEL)==0 && vocab_word(host+MODEL_INKLING_MODEL)==0 &&
        vocab_byte(host+MODEL_GIGA_FLAG)==0;
}

export fn fg_q35_generation_profile(profile: i64[:]) {
    hose_prefill_profile(vocab_address(unsafe_c_slice_data<c_ptr<const void>>(profile)));
    if profile.len>=3 {
        profile[2]=if generation_apex(fg_cuda_model_host_address()) &&
            generation_env_equal("GOOSE_PREFILL_CHECKPOINT_TAIL",49) { 1 } else { 0 };
    }
}

export fn fg_http_eog(ids: i64[:]) -> i64 {
    let eog=generation_eog(fg_cuda_model_host_address());
    if ids.len<eog.len { return -1; }
    if eog.len!=0 {
        generation_copy(vocab_address(unsafe_c_slice_data<c_ptr<const void>>(ids)),eog.address,(eog.len as! u64)*8);
    }
    return eog.len;
}

export fn fg_q35_write_ids(address: u64,tokens: const i32[:]) {
    generation_copy(address,generation_address(tokens),(tokens.len as! u64)*4);
}

export fn fg_q35_read_logits(address: u64,output: f32[:]) {
    generation_copy(generation_output(output),address,(output.len as! u64)*4);
}

export fn fg_q35_release_graph(address: u64) { vocab_free(unsafe_c_ptr<c_ptr<void>>(address)); }

export fn fg_q35_generation_scope_enter() {
    if !generation_apex(fg_cuda_model_host_address()) { return; }
    let plan=fg_cuda_decode_plan_address();
    let old=generation_read_u32(plan);
    generation_set_u32(plan,old+1);
    if old==0 {
        let enabled=!generation_env_equal("GOOSE_DECODE_PLAN_REUSE",48) && !fg_q35_checkpoint_enabled();
        vocab_set_byte(plan+DECODE_PLAN_ENABLED,if enabled { 1 } else { 0 });
    }
}

export fn fg_q35_generation_scope_leave() {
    let host=fg_cuda_model_host_address();
    if !generation_apex(host) { return; }
    let plan=fg_cuda_decode_plan_address();
    let depth=generation_read_u32(plan)-1;
    generation_set_u32(plan,depth);
    if depth==0 && q35_decode_plan_close(model_owner(host),vocab_span(plan+DECODE_PLAN_WORDS,10))!=0 {
        vocab_fatal("decode plan release");
    }
}

fn generation_step(host: u64,tokens: VocabSpan,logits: VocabSpan,prefill: bool) -> i64 {
    if vocab_byte(host+MODEL_GIGA_FLAG)==0 && !prefill && tokens.len==1 && logits.len==model_i64(host+VOCAB_TOKENS) {
        let plan=fg_cuda_decode_plan_address();
        if generation_read_u32(plan)==1 && vocab_byte(plan+DECODE_PLAN_ENABLED)!=0 {
            let status=q35_model_step_cached(model_owner(host),vocab_span(host+MODEL_HANDLES,4),model_records(host),
                vocab_span(host+MODEL_ACFG,14),vocab_span(host+MODEL_GCFG,7),vocab_span(host+MODEL_MCFG,3),
                vocab_span(host+MODEL_KV_STATE,6),tokens,logits,vocab_span(plan+DECODE_PLAN_WORDS,10));
            if status==-2 { vocab_fatal("model step recovery"); }
            return status;
        }
    }
    return model_step_raw(host,tokens,if logits.len!=0 { 1 } else { 0 },logits);
}

export fn fg_q35_generation_eval(tokens: const i32[:],output: f32[:],prefill: bool) -> i64 {
    let source=generation_address(tokens);
    let destination=generation_output(output);
    let count=tokens.len;
    let rows=output.len;
    if source==0 || count<=0 || rows<0 || (rows!=0 && destination==0) ||
        (count as! u64)>GENERATION_MAX_BYTES4 || (rows as! u64)>GENERATION_MAX_BYTES4 { return -1; }
    let token_bytes=(count as! u64)*4;
    let logit_bytes=(rows as! u64)*4;
    if token_bytes>GENERATION_MAX_BYTES-logit_bytes { return -1; }

    // Keep caller-owned slices outside the runtime arena through nested exports.
    let bytes=token_bytes+logit_bytes;
    var exchange: u64=if !prefill && count==1 { generation_borrow(bytes) } else { 0 };
    let borrowed=exchange!=0;
    if !borrowed { exchange=generation_alloc(bytes); }
    if exchange==0 { return -1; }
    generation_copy(exchange,source,token_bytes);
    let host=fg_cuda_model_host_address();
    let output_address=exchange+token_bytes;
    let bonsai=vocab_word(host+MODEL_BONSAI_MODEL);
    let inkling=vocab_word(host+MODEL_INKLING_MODEL);
    var result: i64=-1;
    if bonsai!=0 {
        let row: u64=if rows==model_i64(host+VOCAB_TOKENS) { output_address } else { 0 };
        if (row!=0 || rows==0) && count<=0xffffffff &&
            bonsai_session_step_phase(bonsai,exchange,count as! u32,row,if prefill { 0 } else { 1 })==0 { result=rows; }
    } else if inkling!=0 {
        result=inkling_backend_eval(inkling,exchange,count,prefill,output_address,rows);
    } else {
        let previous=fg_standalone_set_cpu_prefill(prefill);
        result=generation_step(host,vocab_span(exchange,count),vocab_span(output_address,rows),prefill);
        fg_standalone_set_cpu_prefill(previous);
    }
    if result>0 { generation_copy(destination,output_address,logit_bytes); }
    if borrowed { vocab_set_byte(fg_cuda_decode_exchange_address()+16,0); }
    else { vocab_free(unsafe_c_ptr<c_ptr<void>>(exchange)); }
    return result;
}

export fn hose_generate_ids(prompt: u64,count: i64,max_tokens: i64,ignore_eos: bool,output: u64,timing: u64) -> i64 {
    if prompt==0 || count<=0 || output==0 || max_tokens<=0 { return -1; }
    let host=fg_cuda_model_host_address();
    return q35_generate(vocab_span(prompt,count),model_i64(host+VOCAB_TOKENS),model_i64(host+MODEL_OWNER+80),
        generation_eog(host),max_tokens,ignore_eos,vocab_span(output,max_tokens),vocab_span(timing,if timing!=0 { 3 } else { 0 }));
}

export fn hose_generate_text(text: u64,len: i64,max_tokens: i64,ignore_eos: bool,output: u64,timing: u64) -> i64 {
    if timing!=0 { for i in 3 { vocab_set(timing+(i as! u64)*8,0); } }
    if text==0 || len<=0 || len>VOCAB_REQUEST || max_tokens<=0 || output==0 { return -1; }
    let ids=fg_cuda_generation_ids_address();
    let count=tokenize(text,len,ids,VOCAB_REQUEST);
    return hose_generate_ids(ids,count,max_tokens,ignore_eos,output,timing);
}

export fn hose_generate(text: u64,len: i64,max_tokens: i64,ids: u64,out: u64,capacity: i64,timing: u64) -> i64 {
    let produced=hose_generate_text(text,len,max_tokens,false,ids,timing);
    if produced>=0 && out!=0 && detokenize(ids,produced,out,capacity)<0 { return -1; }
    return produced;
}

export fn fg_bonsai_choice_available(eog: const i64[:]) -> bool {
    let model=vocab_word(fg_cuda_model_host_address()+MODEL_BONSAI_MODEL);
    return model!=0 && bonsai_session_choice_available(model,
        vocab_address(unsafe_c_slice_data<c_ptr<const void>>(eog)),eog.len)!=0;
}

export fn fg_bonsai_choose_next(token: i32,output: f32[:],ignore_eos: bool,remaining: i64) -> i64 {
    let host=fg_cuda_model_host_address();
    let model=vocab_word(host+MODEL_BONSAI_MODEL);
    let address=generation_output(output);
    if model==0 || token<0 || address==0 || output.len!=model_i64(host+VOCAB_TOKENS) { return -1; }
    let id=bonsai_session_mtp_next(model,token as! u32,if ignore_eos { 1 } else { 0 },remaining);
    if id!=-2 { return id; }
    let bytes=(output.len as! u64)*4;
    let scratch=generation_alloc(bytes);
    if scratch==0 { return -1; }
    let status=bonsai_session_read_choice_logits(model,scratch);
    if status==0 { generation_copy(address,scratch,bytes); }
    vocab_free(unsafe_c_ptr<c_ptr<void>>(scratch));
    return if status==0 { -2 } else { -1 };
}

export fn fg_bonsai_host_scan_enabled() -> bool {
    return vocab_word(fg_cuda_model_host_address()+MODEL_BONSAI_MODEL)!=0 &&
        generation_env_equal("BONSAI_HTTP_HOST_SCAN",49) && fg_q35_topk_scan_enabled();
}

export fn fg_bonsai_mtp_prepare(allowed: bool) {
    let model=vocab_word(fg_cuda_model_host_address()+MODEL_BONSAI_MODEL);
    if model!=0 { bonsai_session_mtp_prepare(model,if allowed { 1 } else { 0 }); }
}
