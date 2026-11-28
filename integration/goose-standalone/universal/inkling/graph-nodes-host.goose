const I2_NODE_OP: u64 = 80;
const I2_NODE_PARAMS: u64 = 84;
const I2_NODE_SRC: u64 = 152;
const I2_NODE_DATA: u64 = 248;
const I2_NODE_CUSTOM: i32 = 95;
const I2_ENTRY_PLAN: u64 = 8;
const I2_ENTRY_OUT: u64 = 16;
const I2_ENTRY_INPUTS: u64 = 24;
const I2_ENTRY_MESSAGES: u64 = 48;
const I2_ENTRY_RESIDUAL: u64 = 72;
const I2_ENTRY_ATTENTION: u64 = 80;
const I2_ENTRY_MOE_BASE: u64 = 904944;
const I2_ENTRY_ATTN_BASE: u64 = 887280;
const I2_ENTRY_BYTES: u64 = 368;

extern fn fg_i2_host_measured_moe(ctx: i64,input: i64,weights: I2DraftSpan,work: I2DraftSpan,nodes: I2DraftSpan) -> i64;
extern fn ggml_dup_tensor(ctx: u64,tensor: u64) -> u64;
extern "memset" fn i2_node_clear(dst: c_ptr<void>,byte: i32,bytes: u64) -> c_ptr<void>;

fn i2_node_source(node: u64,slot: i64,value: u64) {
    i2d_set(node+I2_NODE_SRC+(slot as! u64)*8,value);
}

fn i2_node_param(node: u64,slot: u64,value: i32) {
    i2d_set_int(node+I2_NODE_PARAMS+slot*4,value);
}

fn i2_node_entry(decoder: u64,base: u64,slot: i32,count: i32) -> u64 {
    if count!=1 || i2d_byte(decoder+I2D_RETAIN)==0 { return 0; }
    return decoder+base+(slot as! u64)*I2_ENTRY_BYTES;
}

export fn fg_i2_host_moe_node(ctx: u64,decoder: u64,core: u64,aux: u64,slot: i32,position: i32,count: i32,
        initial: u64,message: u64,residual: u64,entry: u64) -> u64 {
    var input=initial;
    if message!=0 {
        let memory=fg_i2_host_rows_input(ctx,message,2304,count);
        if entry!=0 { i2d_set(entry+I2_ENTRY_MESSAGES,memory); }
        input=fg_i2_host_controller_node(ctx,decoder,aux,27,input,memory,0);
    }
    let norm=ggml_rms_norm(ctx,input,0.000001 as! f32);
    let scaled=ggml_mul(ctx,norm,fg_i2_host_weight(ctx,decoder,i2_host_aux(core,9)));
    let order: i32[4]=[5,8,6,7];
    var weights: i64[4];
    var work: i64[16]=[0;16];
    var nodes: i64[8]=[0;8];
    for i in order.len { weights[i]=fg_i2_host_weight(ctx,decoder,i2_host_aux(core,order[i])) as! i64; }
    let moe=fg_i2_host_measured_moe(ctx as! i64,scaled as! i64,
        i2d_span(i2d_address(unsafe_c_address<c_ptr<const void>>(weights[0])),4),
        i2d_span(i2d_address(unsafe_c_address<c_ptr<const void>>(work[0])),16),
        i2d_span(i2d_address(unsafe_c_address<c_ptr<const void>>(nodes[0])),8)) as! u64;
    assert(moe!=0);
    if !i2d_env_is("I2_FUSED_MOE_SITU",48) {
        let hidden=nodes[6] as! u64;
        let gate_up=nodes[5] as! u64;
        assert(hidden!=0 && gate_up!=0);
        i2d_set_int(hidden+I2_NODE_OP,I2_NODE_CUSTOM);
        i2_node_clear(unsafe_c_ptr<c_ptr<void>>(hidden+I2_NODE_PARAMS),0,64);
        i2_node_clear(unsafe_c_ptr<c_ptr<void>>(hidden+I2_NODE_SRC),0,80);
        i2_node_param(hidden,0,0x49325354);
        i2_node_source(hidden,0,gate_up);
    }
    let epilogue=ggml_dup_tensor(ctx,moe);
    assert(epilogue!=0);
    i2d_set_int(epilogue+I2_NODE_OP,I2_NODE_CUSTOM);
    i2_node_param(epilogue,0,0x49324d45);
    i2_node_param(epilogue,1,slot);
    i2_node_param(epilogue,2,position);
    i2_node_source(epilogue,0,moe);
    i2_node_source(epilogue,1,residual);
    i2_node_source(epilogue,2,fg_i2_host_weight(ctx,decoder,i2_host_aux(aux,3)));
    return epilogue;
}

export fn fg_i2_host_moe_resident(decoder: u64,core: u64,aux: u64,slot: i32,position: i32,count: i32,
        ffn: u64,message: u64,residual: u64,out: u64) {
    let entry=i2_node_entry(decoder,I2_ENTRY_MOE_BASE,slot,count);
    if entry!=0 && i2d_read(entry+I2_ENTRY_PLAN)!=0 {
        let memory=i2d_read(entry+I2_ENTRY_MESSAGES);
        assert((memory!=0)==(message!=0));
        i2d_set(i2d_read(entry+I2_ENTRY_INPUTS)+I2_NODE_DATA,ffn);
        if message!=0 { i2d_set(memory+I2_NODE_DATA,message); }
        i2d_set(i2d_read(entry+I2_ENTRY_RESIDUAL)+I2_NODE_DATA,residual);
        i2_node_param(i2d_read(entry+I2_ENTRY_OUT),2,position);
        fg_i2_host_execute_entry(entry,out);
        return;
    }
    let ctx=fg_i2_host_context();
    let input=fg_i2_host_rows_input(ctx,ffn,2304,count);
    if entry!=0 { i2d_set(entry+I2_ENTRY_INPUTS,input); }
    let skip=fg_i2_host_rows_input(ctx,residual,2304,count);
    let epilogue=fg_i2_host_moe_node(ctx,decoder,core,aux,slot,position,count,input,message,skip,entry);
    if entry!=0 {
        i2d_set(entry,ctx);
        i2d_set(entry+I2_ENTRY_OUT,epilogue);
        i2d_set(entry+I2_ENTRY_RESIDUAL,i2d_read(epilogue+I2_NODE_SRC+8));
        fg_i2_host_execute_entry(entry,out);
        return;
    }
    fg_i2_host_evaluate_one(ctx,epilogue,out,count*2304);
    ggml_free(ctx);
}

export fn fg_i2_host_share_attention(sliding: bool,aux: u64) -> bool {
    return (sliding || i2_host_aux(aux,10)<0) && !i2d_env_is("I2_SHARED_ATTENTION_INPUTS",48);
}

export fn fg_i2_host_attention_node(ctx: u64,decoder: u64,aux: u64,count: i32,sliding: bool,
        input: u64,message: u64,metadata: u64,entry: u64) -> u64 {
    let node=ggml_new_tensor_2d(ctx,0,2304,count as! i64);
    assert(node!=0);
    i2d_set_int(node+I2_NODE_OP,I2_NODE_CUSTOM);
    i2_node_param(node,0,0x49324147);
    i2d_set(node+I2_NODE_PARAMS+8,metadata);
    let shared=fg_i2_host_share_attention(sliding,aux);
    let eps=0.000001 as! f32;
    let signal: u64=if shared { fg_i2_host_rows_input(ctx,i2d_read(input),2304,count) } else { 0 };
    let normalized_signal: u64=if signal!=0 { ggml_rms_norm(ctx,signal,eps) } else { 0 };
    if entry!=0 && signal!=0 { i2d_set(entry+I2_ENTRY_INPUTS,signal); }
    for i in 3 {
        let index=(i as! u64)*8;
        if i>0 && shared && sliding && message==0 {
            i2_node_source(node,i,i2d_read(node+I2_NODE_SRC));
            continue;
        }
        var normalized=normalized_signal;
        if message!=0 || normalized==0 {
            var x=if signal!=0 { signal } else { fg_i2_host_rows_input(ctx,i2d_read(input+index),2304,count) };
            if entry!=0 && signal==0 { i2d_set(entry+I2_ENTRY_INPUTS+index,x); }
            if message!=0 {
                let memory=fg_i2_host_rows_input(ctx,i2d_read(message+index),2304,count);
                if entry!=0 { i2d_set(entry+I2_ENTRY_MESSAGES+index,memory); }
                x=fg_i2_host_controller_node(ctx,decoder,aux,18+(i as! i32)*3,x,memory,normalized_signal);
            }
            normalized=ggml_rms_norm(ctx,x,eps);
        }
        let gain=fg_i2_host_weight(ctx,decoder,i2_host_aux(aux,if sliding { 11 } else { 12+(i as! i32) }));
        i2_node_source(node,i,ggml_mul(ctx,normalized,gain));
    }
    return node;
}

export fn fg_i2_host_attention_graph(decoder: u64,core: u64,aux: u64,slot: i32,begin: i32,count: i32,
        sliding: bool,input: u64,message: u64,out: u64) {
    var raw: u64[29];
    let call=i2d_address(unsafe_c_address<c_ptr<const void>>(raw[0]));
    fg_i2_host_bind_attention(decoder,core,aux,slot,begin,count,sliding,call);
    let entry=i2_node_entry(decoder,I2_ENTRY_ATTN_BASE,slot,count);
    var metadata=call;
    if entry!=0 {
        metadata=entry+I2_ENTRY_ATTENTION;
        if i2d_read(entry+I2_ENTRY_PLAN)!=0 {
            assert((i2d_read(entry+I2_ENTRY_MESSAGES)!=0)==(message!=0));
            i2d_set_int(metadata+8,begin);
            for i in 3 {
                let at=(i as! u64)*8;
                let signal=i2d_read(entry+I2_ENTRY_INPUTS+at);
                if signal!=0 { i2d_set(signal+I2_NODE_DATA,i2d_read(input+at)); }
                if message!=0 { i2d_set(i2d_read(entry+I2_ENTRY_MESSAGES+at)+I2_NODE_DATA,i2d_read(message+at)); }
            }
            fg_i2_host_execute_entry(entry,out);
            return;
        }
        i2d_transfer(metadata,call,232);
    }
    let ctx=fg_i2_host_context();
    let node=fg_i2_host_attention_node(ctx,decoder,aux,count,sliding,input,message,metadata,entry);
    if entry!=0 {
        i2d_set(entry,ctx);
        i2d_set(entry+I2_ENTRY_OUT,node);
        fg_i2_host_execute_entry(entry,out);
        return;
    }
    fg_i2_host_evaluate_one(ctx,node,out,count*2304);
    ggml_free(ctx);
}
