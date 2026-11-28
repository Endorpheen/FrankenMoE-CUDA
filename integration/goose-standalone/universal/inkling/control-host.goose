const I2_HOST_WIDTH: i64 = 2304;
const I2_HOST_ROW: u64 = 9216;

extern fn fg_i2_host_measured_rms(source: I2DraftSpan,weight: I2DraftSpan,eps: f32,out: I2DraftSpan) -> i64;
extern fn fg_i2_host_measured_depth(states: I2DraftSpan,width: i64,query: I2DraftSpan,norm: I2DraftSpan,p: I2DraftSpan,out: I2DraftSpan) -> i64;
extern fn fg_i2_host_measured_blend(cur: I2DraftSpan,msg: I2DraftSpan,gain: I2DraftSpan,forget: I2DraftSpan,out: I2DraftSpan) -> i64;
extern fn i2_depth_mix_cached(history: I2DraftSpan,cur: I2DraftSpan,query: I2DraftSpan,p: I2DraftSpan,out: I2DraftSpan) -> i64;
extern fn fg_i2_host_context() -> u64;
extern fn fg_i2_host_weight(ctx: u64,decoder: u64,id: i64) -> u64;
extern fn fg_i2_host_evaluate_one(ctx: u64,tensor: u64,out: u64,count: i32);
extern fn fg_i2_host_project(decoder: u64,id: i64,in_addr: u64,n: i32,out: u64,rows: i32);
extern fn fg_i2_host_resident_plan() -> bool;
extern fn fg_i2_host_resident_control() -> bool;
extern fn ggml_new_tensor_2d(ctx: u64,kind: i32,width: i64,count: i64) -> u64;
extern fn ggml_mul_mat(ctx: u64,a: u64,b: u64) -> u64;
extern fn ggml_rms_norm(ctx: u64,input: u64,eps: f32) -> u64;
extern fn ggml_concat(ctx: u64,a: u64,b: u64,dim: i32) -> u64;
extern fn ggml_unary(ctx: u64,input: u64,op: i32) -> u64;
extern fn ggml_scale_bias(ctx: u64,input: u64,scale: f32,bias: f32) -> u64;
extern fn ggml_add(ctx: u64,a: u64,b: u64) -> u64;
extern fn ggml_mul(ctx: u64,a: u64,b: u64) -> u64;
extern fn ggml_free(ctx: u64);

var i2_history_enabled: i32 = -1;

export fn fg_i2_host_cached_history() -> bool {
    if i2_history_enabled<0 {
        i2_history_enabled=if i2d_env_is("I2_DEPTH_HISTORY_CACHE",48) { 0 } else { 1 };
    }
    return i2_history_enabled!=0;
}

fn i2_host_normalize(src: u64,dst: u64) {
    assert(fg_i2_host_measured_rms(i2d_span(src,I2_HOST_WIDTH),i2d_span(0,0),0.000001 as! f32,
        i2d_span(dst,I2_HOST_WIDTH))==I2_HOST_WIDTH);
}

export fn fg_i2_host_save_history(row: u64,current: u64) {
    if fg_i2_host_cached_history() { i2_host_normalize(current,row); }
    else { i2d_transfer(row,current,I2_HOST_ROW); }
}

export fn fg_i2_host_mix(history: u64,raw_count: i32,current: u64,query: I2DraftSpan,raw_queries: i32,out: u64) {
    let count=raw_count as! i64;
    let queries=raw_queries as! i64;
    assert(count>=0 && count<12);
    var probability: f32[12];
    let p=i2d_span(i2d_address(unsafe_c_address<c_ptr<const void>>(probability[0])),count+1);
    let output=i2d_span(out,queries*I2_HOST_WIDTH);
    if fg_i2_host_cached_history() {
        var normalized: f32[2304];
        let address=i2d_address(unsafe_c_address<c_ptr<const void>>(normalized[0]));
        i2_host_normalize(current,address);
        assert(i2_depth_mix_cached(i2d_span(history,count*I2_HOST_WIDTH),i2d_span(address,I2_HOST_WIDTH),
            query,p,output)==queries);
        return;
    }
    var states: f32[27648];
    var norm: f32[27648];
    let address=i2d_address(unsafe_c_address<c_ptr<const void>>(states[0]));
    for i in count {
        i2d_transfer(address+(i as! u64)*I2_HOST_ROW,history+((count-1-i) as! u64)*I2_HOST_ROW,I2_HOST_ROW);
    }
    i2d_transfer(address+(count as! u64)*I2_HOST_ROW,current,I2_HOST_ROW);
    let size=(count+1)*I2_HOST_WIDTH;
    assert(fg_i2_host_measured_depth(i2d_span(address,size),I2_HOST_WIDTH,query,
        i2d_span(i2d_address(unsafe_c_address<c_ptr<const void>>(norm[0])),size),p,output)==queries);
}

fn i2_host_aux(aux: u64,index: i32) -> i64 { return i2d_read(aux+(index as! u64)*8) as! i64; }

export fn fg_i2_host_rows_input(ctx: u64,values: u64,width: i32,count: i32) -> u64 {
    let tensor=ggml_new_tensor_2d(ctx,0,width as! i64,count as! i64);
    assert(tensor!=0);
    i2d_set(tensor+248,values);
    return tensor;
}

fn i2_host_controller_projection(ctx: u64,decoder: u64,aux: u64,at: i32,input: u64) -> u64 {
    return ggml_mul_mat(ctx,fg_i2_host_weight(ctx,decoder,i2_host_aux(aux,at)),input);
}

export fn fg_i2_host_control_group(decoder: u64,aux: u64,at: i32,norm: u64,count: i32,combined: u64) {
    let ctx=fg_i2_host_context();
    let input=fg_i2_host_rows_input(ctx,norm,4608,count);
    let hidden=i2_host_controller_projection(ctx,decoder,aux,at,input);
    let forget=i2_host_controller_projection(ctx,decoder,aux,at+1,hidden);
    let gain=i2_host_controller_projection(ctx,decoder,aux,at+2,hidden);
    fg_i2_host_evaluate_one(ctx,ggml_concat(ctx,gain,forget,0),combined,4608*count);
    ggml_free(ctx);
}

export fn fg_i2_host_controller_node(ctx: u64,decoder: u64,aux: u64,at: i32,signal: u64,memory: u64,
        normalized: u64) -> u64 {
    let eps=0.000001 as! f32;
    let x=if normalized!=0 { normalized } else { ggml_rms_norm(ctx,signal,eps) };
    let y=ggml_rms_norm(ctx,memory,eps);
    let hidden=i2_host_controller_projection(ctx,decoder,aux,at,ggml_concat(ctx,x,y,0));
    let forget=i2_host_controller_projection(ctx,decoder,aux,at+1,hidden);
    let gain=i2_host_controller_projection(ctx,decoder,aux,at+2,hidden);
    let keep=ggml_scale_bias(ctx,ggml_unary(ctx,forget,4),-1.0 as! f32,1.0 as! f32);
    let add=ggml_unary(ctx,gain,4);
    return ggml_add(ctx,ggml_mul(ctx,signal,keep),ggml_mul(ctx,memory,add));
}

export fn fg_i2_host_control_resident(decoder: u64,aux: u64,at: i32,current: u64,message: u64,count: i32) {
    let ctx=fg_i2_host_context();
    let signal=fg_i2_host_rows_input(ctx,current,2304,count);
    let memory=fg_i2_host_rows_input(ctx,message,2304,count);
    let result=fg_i2_host_controller_node(ctx,decoder,aux,at,signal,memory,0);
    fg_i2_host_evaluate_one(ctx,result,current,2304*count);
    ggml_free(ctx);
}

export fn fg_i2_host_control(decoder: u64,aux: u64,which: i32,current: u64,message: u64) {
    let at=18+which*3;
    if fg_i2_host_resident_control() {
        fg_i2_host_control_resident(decoder,aux,at,current,message,1);
        return;
    }
    var norm: f32[4608];
    var hidden: f32[64];
    var gain: f32[2304];
    var forget: f32[2304];
    let n=i2d_address(unsafe_c_address<c_ptr<const void>>(norm[0]));
    let g=i2d_address(unsafe_c_address<c_ptr<const void>>(gain[0]));
    let f=i2d_address(unsafe_c_address<c_ptr<const void>>(forget[0]));
    i2_host_normalize(current,n);
    i2_host_normalize(message,n+I2_HOST_ROW);
    if fg_i2_host_resident_plan() {
        var combined: f32[4608];
        let p=i2d_address(unsafe_c_address<c_ptr<const void>>(combined[0]));
        fg_i2_host_control_group(decoder,aux,at,n,1,p);
        i2d_transfer(g,p,I2_HOST_ROW);
        i2d_transfer(f,p+I2_HOST_ROW,I2_HOST_ROW);
    } else {
        let h=i2d_address(unsafe_c_address<c_ptr<const void>>(hidden[0]));
        fg_i2_host_project(decoder,i2_host_aux(aux,at),n,4608,h,64);
        fg_i2_host_project(decoder,i2_host_aux(aux,at+1),h,64,f,2304);
        fg_i2_host_project(decoder,i2_host_aux(aux,at+2),h,64,g,2304);
    }
    assert(fg_i2_host_measured_blend(i2d_span(current,I2_HOST_WIDTH),i2d_span(message,I2_HOST_WIDTH),
        i2d_span(g,I2_HOST_WIDTH),i2d_span(f,I2_HOST_WIDTH),i2d_span(current,I2_HOST_WIDTH))==I2_HOST_WIDTH);
}
