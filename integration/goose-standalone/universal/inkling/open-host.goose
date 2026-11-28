const I2_OPEN_BYTES: i64 = 940272;
const I2_OPEN_TENSOR: u64 = 8;
const I2_OPEN_GLOBALS: u64 = 73736;
const I2_OPEN_CORE: u64 = 73776;
const I2_OPEN_AUX: u64 = 75696;
const I2_OPEN_VISITS: u64 = 81456;

extern fn gr_parse(data: I2DraftSpan,bytes: i64,state: I2DraftSpan,meta: I2DraftSpan,tensors: I2DraftSpan,
    keys: I2DraftSpan,slots: I2DraftSpan) -> i64;
extern fn i2_read_layout(data: I2DraftSpan,state: I2DraftSpan,meta: I2DraftSpan,keys: I2DraftSpan,
    layout: I2DraftSpan,visits: I2DraftSpan) -> i64;
extern fn i2_bind_text_weights(data: I2DraftSpan,state: I2DraftSpan,meta: I2DraftSpan,keys: I2DraftSpan,
    tensors: I2DraftSpan,slots: I2DraftSpan,layout: I2DraftSpan,globals: I2DraftSpan,core: I2DraftSpan) -> i64;
extern fn i2_bind_text_aux(data: I2DraftSpan,state: I2DraftSpan,meta: I2DraftSpan,keys: I2DraftSpan,
    tensors: I2DraftSpan,slots: I2DraftSpan,layout: I2DraftSpan,aux: I2DraftSpan) -> i64;
extern fn i2_place_expert_bank(tensors: I2DraftSpan,core: I2DraftSpan,data: i64,budget: i64) -> i64;
extern fn i2_place_weights(tensors: I2DraftSpan,core: I2DraftSpan,data: i64,excluded: i64,budget: i64,
    adaptive: bool,keep_routers: bool,ranges: I2DraftSpan,totals: I2DraftSpan) -> i64;
extern fn i2_candidate_init(argc: i32,argv: u64);
extern fn fg_cuda_free_bytes() -> i64;
extern fn fg_cuda_enable_inkling_quants(enabled: bool);
extern fn fg_i2_close_logits_team(team: u64);
extern fn fg_standalone_graph_plan_free(plan: u64);

fn i2_open_drop_graph(entry: u64) {
    fg_standalone_graph_plan_free(i2d_read(entry + 8));
    let ctx = i2d_read(entry);
    if ctx != 0 { ggml_free(ctx); }
}

fn fg_i2_backend_open_cleanup(decoder: u64) {
    let team = i2d_read(decoder + 887272);
    if team != 0 {
        fg_i2_close_logits_team(team);
        i2d_free(unsafe_c_ptr<c_ptr<void>>(team));
    }
    for index in 48 {
        let offset = (index as! u64) * 368;
        i2_open_drop_graph(decoder + I2_ENTRY_ATTN_BASE + offset);
        i2_open_drop_graph(decoder + I2_ENTRY_MOE_BASE + offset);
        i2_open_drop_graph(decoder + I2D_JOINED + offset);
    }
}
extern fn fg637_fmt2(out: I2DraftSpan,value: f64) -> i64;
extern "calloc" fn i2_open_calloc(count: u64,size: u64) -> c_ptr<void>;
extern "gs_os_write_stdout" fn i2_open_print(text: const u8[:]);

fn i2_open_alloc(count: u64,size: u64) -> u64 {
    let pointer=i2_open_calloc(count,size);
    var address: u64=0;
    i2d_copy(unsafe_c_address<c_ptr<void>>(address),unsafe_c_address<c_ptr<const void>>(pointer),8);
    return address;
}

fn i2_open_free(decoder: u64) {
    fg_i2_backend_open_cleanup(decoder);
    let fields: u64[8]=[I2D_DRAFT+I2D_HISTORY,I2D_DRAFT+I2D_SCORES,I2D_DRAFT+I2D_SAVED,
        887144,I2D_HA,887128,887120,887112];
    for i in fields.len { i2d_free(unsafe_c_ptr<c_ptr<void>>(i2d_read(decoder+fields[i]))); }
    i2d_free(unsafe_c_ptr<c_ptr<void>>(decoder));
}

fn i2_open_bind(decoder: u64,image: u64,bytes: i64,tensor_count: i64&) -> bool {
    var state: i64[8]=[0;8];
    var meta: i64[2048]=[0;2048];
    var keys: i64[512]=[0;512];
    var slots: i64[2048]=[0;2048];
    var layout: i64[9]=[0;9];
    let data=i2d_span(image,if bytes<67108864 { bytes } else { 67108864 });
    let st=i2d_span(i2d_address(unsafe_c_address<c_ptr<const void>>(state[0])),8);
    let kv=i2d_span(i2d_address(unsafe_c_address<c_ptr<const void>>(meta[0])),2048);
    let names=i2d_span(i2d_address(unsafe_c_address<c_ptr<const void>>(keys[0])),512);
    let table=i2d_span(i2d_address(unsafe_c_address<c_ptr<const void>>(slots[0])),2048);
    let view=i2d_span(i2d_address(unsafe_c_address<c_ptr<const void>>(layout[0])),9);
    let tensors=i2d_span(decoder+I2_OPEN_TENSOR,9216);
    if gr_parse(data,bytes,st,kv,tensors,names,table)!=0 ||
        i2_read_layout(data,st,kv,names,view,i2d_span(decoder+I2_OPEN_VISITS,192))!=48 ||
        i2_bind_text_weights(data,st,kv,names,tensors,table,view,i2d_span(decoder+I2_OPEN_GLOBALS,5),
            i2d_span(decoder+I2_OPEN_CORE,240))!=24 ||
        i2_bind_text_aux(data,st,kv,names,tensors,table,view,i2d_span(decoder+I2_OPEN_AUX,720))!=24 { return false; }
    tensor_count=state[3];
    return true;
}

fn i2_open_history(decoder: u64) -> bool {
    let fields: u64[4]=[887120,887128,I2D_HA,887144];
    let counts: u64[4]=[147456,147456,331776,331776];
    var ok=true;
    for i in fields.len {
        let address=i2_open_alloc(counts[i],4);
        i2d_set(decoder+fields[i],address);
        ok=ok && address!=0;
    }
    return ok;
}

fn i2_open_weights(decoder: u64,image: u64,count: i64,budget: i64) -> i64 {
    let capacity=(count+6144)*3;
    let ranges=i2_open_alloc(capacity as! u64,8);
    assert(ranges!=0);
    var totals: i64[2];
    let used=i2_place_weights(i2d_span(decoder+I2_OPEN_TENSOR,count*12),i2d_span(decoder+I2_OPEN_CORE,240),
        image as! i64,i2d_read(decoder+I2_OPEN_GLOBALS) as! i64,budget,true,!i2d_env_is("I2_RESIDENT_ROUTERS",48),
        i2d_span(ranges,capacity),i2d_span(i2d_address(unsafe_c_address<c_ptr<const void>>(totals[0])),2));
    i2d_free(unsafe_c_ptr<c_ptr<void>>(ranges));
    var text: u8[64];
    var dense: u8[64];
    let a=fg637_fmt2(i2d_span(i2d_address(unsafe_c_address<c_ptr<const void>>(text[0])),64),(used as! f64)/1048576.0);
    let b=fg637_fmt2(i2d_span(i2d_address(unsafe_c_address<c_ptr<const void>>(dense[0])),64),(totals[0] as! f64)/1048576.0);
    if a>=0 && a<64 && b>=0 && b<64 {
        i2_open_print(str("Resident weights: ",text[0..a]," MiB, dense ",dense[0..b]," MiB\n"));
    }
    i2_open_print(str("Resident router weights: ",totals[1]," bytes\n"));
    return used;
}

fn i2_open_place(decoder: u64,image: u64,count: i64,capacity: i64,limit: i64,format: i32) -> bool {
    i2_candidate_init(0,0);
    let available=fg_cuda_free_bytes();
    let local=if capacity<1024 { capacity } else { 1024 };
    let rows=8*capacity+40*local;
    let row_bytes: i64=if format==1 { 576 } else { 2048 };
    let reserve=1073741824+rows*(2*row_bytes+8);
    if available<=reserve || (limit>0 && limit>available-reserve) { return false; }
    let memory=available-reserve;
    let budget=if limit>0 || memory<7516192768 { memory } else { 7516192768 };
    fg_cuda_enable_inkling_quants(true);
    let resident=i2_open_weights(decoder,image,count,budget);
    let experts=if limit>0 { limit } else { budget-resident };
    return resident>0 && experts<=memory-resident &&
        i2_place_expert_bank(i2d_span(decoder+I2_OPEN_TENSOR,9216),i2d_span(decoder+I2_OPEN_CORE,240),
            image as! i64,experts)>0;
}

export fn inkling_backend_open(image: u64,bytes: u64,capacity: i64,limit: i64,format: i32) -> u64 {
    if image==0 || bytes>0x7fffffffffffffff || capacity<=0 || capacity>1048576 || limit<0 { return 0; }
    let decoder=i2_open_alloc(1,I2_OPEN_BYTES as! u64);
    if decoder==0 { return 0; }
    i2d_set(decoder,image);
    i2d_set_int(decoder+I2D_CAP,capacity as! i32);
    i2d_set_int(decoder+I2D_FMT,format);
    var count: i64=0;
    if i2_open_bind(decoder,image,bytes as! i64,count) && i2_open_history(decoder) &&
        i2_open_place(decoder,image,count,capacity,limit,format) { return decoder; }
    i2_open_free(decoder);
    return 0;
}
