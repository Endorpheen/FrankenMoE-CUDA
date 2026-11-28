const VOCAB_BASE: u64 = 0;
const VOCAB_SIZE: u64 = 8;
const VOCAB_STATE: u64 = 128;
const VOCAB_KV: u64 = 192;
const VOCAB_KV_SLOTS: u64 = 200;
const VOCAB_TENSORS: u64 = 208;
const VOCAB_TENSOR_SLOTS: u64 = 216;
const VOCAB_BLOB: u64 = 224;
const VOCAB_TOKEN_OFFSETS: u64 = 232;
const VOCAB_TOKEN_LENGTHS: u64 = 240;
const VOCAB_TOKEN_SLOTS: u64 = 248;
const VOCAB_MERGE_OFFSETS: u64 = 256;
const VOCAB_MERGE_LENGTHS: u64 = 264;
const VOCAB_MERGE_SLOTS: u64 = 272;
const VOCAB_ATTR: u64 = 280;
const VOCAB_ORDER: u64 = 288;
const VOCAB_TOKENS: u64 = 296;
const VOCAB_MERGES: u64 = 304;
const VOCAB_SPECIALS: u64 = 312;
const VOCAB_TOKEN_SLOT_COUNT: u64 = 320;
const VOCAB_MERGE_SLOT_COUNT: u64 = 328;
const VOCAB_BLOB_BYTES: u64 = 336;
const VOCAB_EOG: u64 = 344;
const VOCAB_EOG_COUNT: u64 = 352;
const VOCAB_EOS: u64 = 360;
const VOCAB_BONSAI_LAYOUT: u64 = 688;
const VOCAB_INKLING: u64 = 689;
const VOCAB_CPTS: u64 = 704;
const VOCAB_SEG: u64 = 712;
const VOCAB_SYM: u64 = 720;
const VOCAB_HEAP: u64 = 728;
const VOCAB_HC: u64 = 736;
const VOCAB_INFO: u64 = 744;
const VOCAB_ENC: u64 = 752;

const VOCAB_REQUEST: i64 = 262144;
const VOCAB_SCRATCH: i64 = 262145;

struct VocabSpan { address: u64, len: i64 }


extern fn vocab_strlen(text: u64) -> u64;
extern "calloc" fn vocab_calloc(n: u64, size: u64) -> c_ptr<void>;
extern "malloc" fn vocab_malloc(size: u64) -> c_ptr<void>;
extern "free" fn vocab_free(pointer: c_ptr<void>);
extern "memset" fn vocab_memset(pointer: c_ptr<void>, value: i32, size: u64) -> c_ptr<void>;
extern "memcpy" fn vocab_memcpy(dst: c_ptr<void>, src: c_ptr<const void>, size: u64) -> c_ptr<void>;
extern "gs_os_write_stderr" fn vocab_log(text: const u8[:]);
extern "exit" fn vocab_exit(status: i32);
extern fn bonsai_goose_init(argc: i32, argv: u64);
extern fn inkling_tokenizer_init(argc: i32, argv: u64);
extern fn gr_parse(file: VocabSpan, size: i64, state: VocabSpan, meta: VocabSpan, tensor: VocabSpan, keys: VocabSpan, tensors: VocabSpan) -> i64;
extern fn bonsai_gr_parse(file: VocabSpan, size: i64, state: VocabSpan, meta: VocabSpan, tensor: VocabSpan, keys: VocabSpan, tensors: VocabSpan) -> i64;
extern fn gr_find_kv(file: VocabSpan, state: VocabSpan, meta: VocabSpan, keys: VocabSpan, key: VocabSpan) -> i64;
extern fn gr_kv_to_str(file: VocabSpan, state: VocabSpan, meta: VocabSpan, id: i64, limit: i64, out: VocabSpan) -> i64;
extern fn gr_arr_n(state: VocabSpan, meta: VocabSpan, id: i64) -> i64;
extern fn gr_array_int(file: VocabSpan, state: VocabSpan, meta: VocabSpan, id: i64, index: i64, out: VocabSpan) -> i64;
extern fn gr_meta_int(file: VocabSpan, state: VocabSpan, meta: VocabSpan, id: i64, out: VocabSpan) -> i64;
extern fn bt_prepare_need(file: VocabSpan, state: VocabSpan, meta: VocabSpan, tokens: i64, merges: i64, need: VocabSpan) -> i64;
extern fn bt_prepare_from_gguf(file: VocabSpan, state: VocabSpan, meta: VocabSpan, tokens: i64, merges: i64,
    blob: VocabSpan, offsets: VocabSpan, lengths: VocabSpan, ids: VocabSpan,
    merge_offsets: VocabSpan, merge_lengths: VocabSpan, merge_ids: VocabSpan, need: VocabSpan) -> i64;
extern fn bt_attrs(blob: VocabSpan, offsets: VocabSpan, lengths: VocabSpan, types: VocabSpan, attrs: VocabSpan) -> i64;
extern fn bt_build(blob: VocabSpan, offsets: VocabSpan, lengths: VocabSpan, ids: VocabSpan,
    merge_offsets: VocabSpan, merge_lengths: VocabSpan, merge_ids: VocabSpan, stats: VocabSpan) -> i64;
extern fn bt_special_order(blob: VocabSpan, offsets: VocabSpan, lengths: VocabSpan, attrs: VocabSpan, out: VocabSpan) -> i64;
extern fn bt_eog_ids(blob: VocabSpan, offsets: VocabSpan, lengths: VocabSpan, eos: i64, eom: i64, eot: i64, out: VocabSpan) -> i64;
extern fn bt_token_to_piece(blob: VocabSpan, offsets: VocabSpan, lengths: VocabSpan, attrs: VocabSpan,
    id: i64, out: VocabSpan, at: i64, trim: i64, special: i64, attr: i64) -> i64;
extern fn bt_tokenize(blob: VocabSpan, offsets: VocabSpan, lengths: VocabSpan, ids: VocabSpan,
    merge_offsets: VocabSpan, merge_lengths: VocabSpan, merges: VocabSpan, order: VocabSpan,
    specials: i64, text: VocabSpan, result: VocabSpan, special: i64, cpts: VocabSpan, seg: VocabSpan,
    enc: VocabSpan, sym: VocabSpan, heap: VocabSpan, hc: VocabSpan, info: VocabSpan) -> i64;
extern fn bt_tokenize_inkling2(blob: VocabSpan, offsets: VocabSpan, lengths: VocabSpan, ids: VocabSpan,
    merge_offsets: VocabSpan, merge_lengths: VocabSpan, merges: VocabSpan, order: VocabSpan,
    specials: i64, text: VocabSpan, result: VocabSpan, special: i64, cpts: VocabSpan, seg: VocabSpan,
    enc: VocabSpan, sym: VocabSpan, heap: VocabSpan, hc: VocabSpan, info: VocabSpan) -> i64;

fn vocab_address(pointer: c_ptr<const void>) -> u64 {
    var address: u64=0;
    vocab_memcpy(unsafe_c_address<c_ptr<void>>(address),unsafe_c_address<c_ptr<const void>>(pointer),8);
    return address;
}

// One aligned backing store for the vocabulary, model handles and trunk state.
var model_host_storage: u64[95];

export fn fg_cuda_model_host_address() -> u64 {
    return vocab_address(unsafe_c_address<c_ptr<const void>>(model_host_storage[0]));
}

extern fn vocab_touch_copy(dst: c_ptr<void>, source: c_ptr<const void>, bytes: u64) -> c_ptr<void>;

export fn fg_model_touch_page(address: u64) {
    var byte: u8 = 0;
    // The out-of-line call preserves the page read even though its value is unused.
    vocab_touch_copy(unsafe_c_address<c_ptr<void>>(byte), unsafe_c_ptr<c_ptr<const void>>(address), 1);
}

fn vocab_word(address: u64) -> u64 {
    var value: u64=0;
    vocab_memcpy(unsafe_c_address<c_ptr<void>>(value),unsafe_c_ptr<c_ptr<const void>>(address),8);
    return value;
}

fn vocab_set(address: u64,value: u64) {
    vocab_memcpy(unsafe_c_ptr<c_ptr<void>>(address),unsafe_c_address<c_ptr<const void>>(value),8);
}

fn vocab_set_byte(address: u64,value: u8) {
    vocab_memcpy(unsafe_c_ptr<c_ptr<void>>(address),unsafe_c_address<c_ptr<const void>>(value),1);
}

fn vocab_byte(address: u64) -> u8 {
    var value: u8=0;
    vocab_memcpy(unsafe_c_address<c_ptr<void>>(value),unsafe_c_ptr<c_ptr<const void>>(address),1);
    return value;
}

fn vocab_fatal(message: const u8[:]) {
    vocab_log(str("GENERATE ",message,"\n"));
    vocab_exit(1);
}

fn vocab_alloc(count: i64,size: u64) -> u64 {
    let pointer=vocab_calloc(count as! u64,size);
    if pointer==null && count!=0 { vocab_fatal("tokenizer allocation"); }
    var address: u64=0;
    vocab_memcpy(unsafe_c_address<c_ptr<void>>(address),unsafe_c_address<c_ptr<const void>>(pointer),8);
    return address;
}

fn vocab_span(address: u64,len: i64) -> VocabSpan { return VocabSpan {address:address,len:len}; }
fn vocab_table(host: u64,field: u64,count: i64) -> VocabSpan { return vocab_span(vocab_word(host+field),count); }
fn vocab_text(text: const u8[:]) -> VocabSpan {
    return vocab_span(vocab_address(unsafe_c_slice_data<c_ptr<const void>>(text)),text.len);
}
fn vocab_file(host: u64) -> VocabSpan {
    let size=vocab_word(host+VOCAB_SIZE);
    return vocab_table(host,VOCAB_BASE,(if size<67108864 { size } else { 67108864 }) as! i64);
}
fn vocab_find(host: u64,key: VocabSpan) -> i64 {
    return gr_find_kv(vocab_file(host),vocab_span(host+VOCAB_STATE,8),vocab_table(host,VOCAB_KV,2048),
        vocab_table(host,VOCAB_KV_SLOTS,512),key);
}

export fn find_key(key: u64) -> i64 {
    return vocab_find(fg_cuda_model_host_address(),vocab_span(key,vocab_strlen(key) as! i64));
}

fn vocab_meta_is(host: u64,key: VocabSpan,value: VocabSpan) -> bool {
    let index=vocab_find(host,key);
    if index<0 { return false; }
    var text: u8[32]=[0;32];
    let address=vocab_address(unsafe_c_address<c_ptr<const void>>(text[0]));
    let length=gr_kv_to_str(vocab_file(host),vocab_span(host+VOCAB_STATE,8),
        vocab_table(host,VOCAB_KV,2048),index,32,vocab_span(address,32));
    if length<0 || length!=value.len || length>32 { return false; }
    for i in length {
        if text[i]!=vocab_byte(value.address+(i as! u64)) { return false; }
    }
    return true;
}

export fn metadata_is(key: u64,value: u64) -> bool {
    return vocab_meta_is(fg_cuda_model_host_address(),vocab_span(key,vocab_strlen(key) as! i64),
        vocab_span(value,vocab_strlen(value) as! i64));
}

fn vocab_parse(host: u64) {
    let state=vocab_span(host+VOCAB_STATE,8);
    let kv=vocab_span(vocab_alloc(2048,8),2048);
    let keys=vocab_span(vocab_alloc(512,8),512);
    let tensors=vocab_span(vocab_alloc(24576,8),24576);
    let slots=vocab_span(vocab_alloc(8192,8),8192);
    vocab_set(host+VOCAB_KV,kv.address);
    vocab_set(host+VOCAB_KV_SLOTS,keys.address);
    vocab_set(host+VOCAB_TENSORS,tensors.address);
    vocab_set(host+VOCAB_TENSOR_SLOTS,slots.address);
    let file=vocab_file(host);
    let size=vocab_word(host+VOCAB_SIZE) as! i64;
    if gr_parse(file,size,state,kv,tensors,keys,slots)==0 { return; }

    let buffers: VocabSpan[5]=[state,kv,keys,tensors,slots];
    for i in buffers.len {
        vocab_memset(unsafe_c_ptr<c_ptr<void>>(buffers[i].address),0,(buffers[i].len as! u64)*8);
    }
    bonsai_goose_init(0,0);
    if bonsai_gr_parse(file,size,state,kv,tensors,keys,slots)!=0 { vocab_fatal("reader"); }
    vocab_set_byte(host+VOCAB_BONSAI_LAYOUT,1);
    if !vocab_meta_is(host,vocab_text("general.architecture"),vocab_text("qwen35")) { vocab_fatal("Bonsai architecture"); }
}

fn vocab_tables(host: u64) {
    let inkling=vocab_meta_is(host,vocab_text("tokenizer.ggml.pre"),vocab_text("inkling2"));
    vocab_set_byte(host+VOCAB_INKLING,if inkling { 1 } else { 0 });
    if inkling { inkling_tokenizer_init(0,0); }
    let tokens=vocab_find(host,vocab_text("tokenizer.ggml.tokens"));
    let merges=vocab_find(host,vocab_text("tokenizer.ggml.merges"));
    let types=vocab_find(host,vocab_text("tokenizer.ggml.token_type"));
    if tokens<0 || merges<0 || types<0 { vocab_fatal("tokenizer keys"); }
    let file=vocab_file(host);
    let state=vocab_span(host+VOCAB_STATE,8);
    let kv=vocab_table(host,VOCAB_KV,2048);
    var shape: i64[5]=[0;5];
    let need=vocab_span(vocab_address(unsafe_c_address<c_ptr<const void>>(shape[0])),5);
    if bt_prepare_need(file,state,kv,tokens,merges,need)!=0 { vocab_fatal("prepare_need"); }
    vocab_set(host+VOCAB_BLOB_BYTES,shape[0] as! u64);
    vocab_set(host+VOCAB_TOKENS,shape[1] as! u64);
    vocab_set(host+VOCAB_MERGES,shape[2] as! u64);
    vocab_set(host+VOCAB_TOKEN_SLOT_COUNT,shape[3] as! u64);
    vocab_set(host+VOCAB_MERGE_SLOT_COUNT,shape[4] as! u64);
    let blob_pointer=vocab_malloc(shape[0] as! u64);
    if blob_pointer==null { vocab_fatal("prepare"); }
    var blob_address: u64=0;
    vocab_memcpy(unsafe_c_address<c_ptr<void>>(blob_address),unsafe_c_address<c_ptr<const void>>(blob_pointer),8);
    let blob=vocab_span(blob_address,shape[0]);
    vocab_set(host+VOCAB_BLOB,blob.address);

    let count=shape[1];
    let counts: i64[8]=[count,count,shape[3],shape[2],shape[2],shape[4],count,count];
    var tables: VocabSpan[8];
    for i in counts.len {
        tables[i]=vocab_span(vocab_alloc(counts[i],8),counts[i]);
        vocab_set(host+VOCAB_TOKEN_OFFSETS+(i as! u64)*8,tables[i].address);
    }
    if bt_prepare_from_gguf(file,state,kv,tokens,merges,blob,tables[0],tables[1],tables[2],
        tables[3],tables[4],tables[5],need)!=0 { vocab_fatal("prepare"); }

    let token_type=vocab_span(vocab_alloc(count,8),count);
    if gr_arr_n(state,kv,types)!=count { vocab_fatal("token type count"); }
    for i in count {
        if gr_array_int(file,state,kv,types,i,vocab_span(token_type.address+(i as! u64)*8,1))!=0 {
            vocab_fatal("token type value");
        }
    }
    if bt_attrs(blob,tables[0],tables[1],token_type,tables[6])!=0 { vocab_fatal("attrs"); }
    vocab_free(unsafe_c_ptr<c_ptr<void>>(token_type.address));
    var stats: i64[4]=[0;4];
    if bt_build(blob,tables[0],tables[1],tables[2],tables[3],tables[4],tables[5],
        vocab_span(vocab_address(unsafe_c_address<c_ptr<const void>>(stats[0])),4))!=0 { vocab_fatal("bt_build"); }
    let specials=bt_special_order(blob,tables[0],tables[1],tables[6],tables[7]);
    vocab_set(host+VOCAB_SPECIALS,if specials>=0 { specials as! u64 } else { 0 });

    var eos: i64=-1;
    let key=vocab_find(host,vocab_text("tokenizer.ggml.eos_token_id"));
    if key>=0 {
        eos=0;
        gr_meta_int(file,state,kv,key,vocab_span(vocab_address(unsafe_c_address<c_ptr<const void>>(eos)),1));
    }
    vocab_set(host+VOCAB_EOS,eos as! u64);
    let eog=vocab_span(vocab_alloc(count,8),count);
    vocab_set(host+VOCAB_EOG,eog.address);
    let stops=bt_eog_ids(blob,tables[0],tables[1],eos,-1,-1,eog);
    vocab_set(host+VOCAB_EOG_COUNT,if stops>=0 { stops as! u64 } else { 0 });

    let scratch: i64[6]=[VOCAB_SCRATCH,4*VOCAB_SCRATCH,24*VOCAB_SCRATCH,15*VOCAB_SCRATCH+40,2,8];
    for i in scratch.len { vocab_set(host+VOCAB_CPTS+(i as! u64)*8,vocab_alloc(scratch[i],8)); }
    vocab_set(host+VOCAB_ENC,vocab_alloc(8*VOCAB_SCRATCH,1));
}

export fn load_vocab() {
    let host=fg_cuda_model_host_address();
    vocab_parse(host);
    vocab_tables(host);
}

export fn tokenize(text: u64,len: i64,ids: u64,capacity: i64) -> i64 {
    if text==0 || len<0 || len>VOCAB_REQUEST || ids==0 || capacity<0 { return -1; }
    let host=fg_cuda_model_host_address();
    let count=vocab_word(host+VOCAB_TOKENS) as! i64;
    let merges=vocab_word(host+VOCAB_MERGES) as! i64;
    let blob=vocab_table(host,VOCAB_BLOB,vocab_word(host+VOCAB_BLOB_BYTES) as! i64);
    let table0=vocab_table(host,VOCAB_TOKEN_OFFSETS,count);
    let table1=vocab_table(host,VOCAB_TOKEN_LENGTHS,count);
    let table2=vocab_table(host,VOCAB_TOKEN_SLOTS,vocab_word(host+VOCAB_TOKEN_SLOT_COUNT) as! i64);
    let table3=vocab_table(host,VOCAB_MERGE_OFFSETS,merges);
    let table4=vocab_table(host,VOCAB_MERGE_LENGTHS,merges);
    let table5=vocab_table(host,VOCAB_MERGE_SLOTS,vocab_word(host+VOCAB_MERGE_SLOT_COUNT) as! i64);
    let order=vocab_table(host,VOCAB_ORDER,count);
    let specials=vocab_word(host+VOCAB_SPECIALS) as! i64;
    let vocabulary_text=vocab_span(text,len);
    let result=vocab_span(ids,capacity);
    let cpts=vocab_table(host,VOCAB_CPTS,VOCAB_SCRATCH);
    let seg=vocab_table(host,VOCAB_SEG,4*VOCAB_SCRATCH);
    let enc=vocab_table(host,VOCAB_ENC,8*VOCAB_SCRATCH);
    let sym=vocab_table(host,VOCAB_SYM,24*VOCAB_SCRATCH);
    let heap=vocab_table(host,VOCAB_HEAP,15*VOCAB_SCRATCH+40);
    let hc=vocab_table(host,VOCAB_HC,2);
    let info=vocab_table(host,VOCAB_INFO,8);
    if vocab_byte(host+VOCAB_INKLING)!=0 {
        return bt_tokenize_inkling2(blob, table0, table1, table2, table3, table4, table5, order, specials,
            vocabulary_text, result, 1, cpts, seg, enc, sym, heap, hc, info);
    }
    return bt_tokenize(blob, table0, table1, table2, table3, table4, table5, order, specials,
            vocabulary_text, result, 1, cpts, seg, enc, sym, heap, hc, info);
}

export fn detokenize(ids: u64,count: i64,out: u64,capacity: i64) -> i64 {
    let host=fg_cuda_model_host_address();
    let tokens=vocab_word(host+VOCAB_TOKENS) as! i64;
    let blob=vocab_table(host,VOCAB_BLOB,vocab_word(host+VOCAB_BLOB_BYTES) as! i64);
    let offsets=vocab_table(host,VOCAB_TOKEN_OFFSETS,tokens);
    let lengths=vocab_table(host,VOCAB_TOKEN_LENGTHS,tokens);
    let attr=vocab_table(host,VOCAB_ATTR,tokens);
    var at: i64=0;
    for i in count {
        let id=vocab_word(ids+(i as! u64)*8) as! i64;
        if id<0 || id>=tokens || at>=capacity { return -1; }
        let got=bt_token_to_piece(blob,offsets,lengths,attr,id,vocab_span(out+(at as! u64),capacity-at),0,0,1,0);
        if got<0 { return -1; }
        at+=got;
    }
    return at;
}
