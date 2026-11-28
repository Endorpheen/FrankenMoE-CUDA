const I2_JOINED_BRIDGE: u64 = 312;
const I2_JOINED_STARTED: u64 = 360;

extern fn fg_i2_host_coefficients(decoder: u64,id: i64,count: i64) -> u64;
extern fn ggml_view_1d(ctx: u64,tensor: u64,count: i64,offset: u64) -> u64;

fn i2_joined_bind(entry: u64,decoder: u64,aux: u64,slot: i32,position: i32,input: u64,
        rows: i32,add_residual: bool) {
    let bridge=entry+I2_JOINED_BRIDGE;
    i2d_set(bridge,fg_i2_host_coefficients(decoder,i2_host_aux(aux,2),9216));
    i2d_set(bridge+8,fg_i2_host_coefficients(decoder,i2_host_aux(aux,9),2304));
    i2d_set(bridge+16,i2d_read(decoder+I2D_HA)+(slot as! u64)*27648);
    i2d_set(bridge+24,input);
    i2d_set_int(bridge+32,slot);
    i2d_set_int(bridge+36,rows);
    i2d_set_int(bridge+40,position);
    i2d_set_byte(bridge+44,if i2d_byte(entry+I2_JOINED_STARTED)==0 { 1 } else { 0 });
    i2d_set_byte(bridge+45,if add_residual { 1 } else { 0 });
}

fn i2_joined_rebind(entry: u64,position: i32,input: u64,messages: u64) {
    i2d_set_int(entry+I2_ENTRY_ATTENTION+8,position);
    for i in 3 {
        let field=(i as! u64)*8;
        let tensor=i2d_read(entry+I2_ENTRY_INPUTS+field);
        if tensor!=0 { i2d_set(tensor+I2_NODE_DATA,i2d_read(input+field)); }
        if messages!=0 {
            let memory=i2d_read(entry+I2_ENTRY_MESSAGES+field);
            i2d_set(memory+I2_NODE_DATA,messages+(i as! u64)*I2_HOST_ROW);
        }
    }
    if messages!=0 {
        i2d_set(i2d_read(entry+I2_ENTRY_RESIDUAL)+I2_NODE_DATA,messages+3*I2_HOST_ROW);
    }
    i2_node_param(i2d_read(entry+I2_ENTRY_OUT),2,position);
}

fn i2_joined_create(entry: u64,decoder: u64,core: u64,aux: u64,slot: i32,position: i32,
        sliding: bool,input: u64,messages: u64) {
    let ctx=fg_i2_host_context();
    i2d_set(entry,ctx);
    let metadata=entry+I2_ENTRY_ATTENTION;
    fg_i2_host_bind_attention(decoder,core,aux,slot,position,1,sliding,metadata);
    var pointers: u64[3]=[0,0,0];
    if messages!=0 {
        for i in 3 { pointers[i]=messages+(i as! u64)*I2_HOST_ROW; }
    }
    let message_vector: u64=if messages!=0 {
        i2d_address(unsafe_c_address<c_ptr<const void>>(pointers[0]))
    } else { 0 };
    let attention=fg_i2_host_attention_node(ctx,decoder,aux,1,sliding,input,message_vector,metadata,entry);
    let bridge=ggml_new_tensor_2d(ctx,0,I2_HOST_WIDTH,2);
    assert(bridge!=0);
    i2d_set_int(bridge+I2_NODE_OP,I2_NODE_CUSTOM);
    i2_node_param(bridge,0,0x49324252);
    i2d_set(bridge+I2_NODE_PARAMS+8,entry+I2_JOINED_BRIDGE);
    i2_node_source(bridge,0,attention);
    let ffn=ggml_view_1d(ctx,bridge,I2_HOST_WIDTH,0);
    let next=ggml_view_1d(ctx,bridge,I2_HOST_WIDTH,I2_HOST_ROW);
    var scratch: u64[46]=[0;46];
    let moe=i2d_address(unsafe_c_address<c_ptr<const void>>(scratch[0]));
    let message: u64=if messages!=0 { messages+3*I2_HOST_ROW } else { 0 };
    let output=fg_i2_host_moe_node(ctx,decoder,core,aux,slot,position,1,ffn,message,next,moe);
    i2d_set(entry+I2_ENTRY_OUT,output);
    i2d_set(entry+I2_ENTRY_RESIDUAL,i2d_read(moe+I2_ENTRY_MESSAGES));
}

export fn fg_i2_host_joined_layer(decoder: u64,core: u64,aux: u64,slot: i32,position: i32,
        sliding: bool,add_residual: bool,input: u64,messages: u64,current: u64,history: u64,rows: i32) {
    let entry=decoder+I2D_JOINED+(slot as! u64)*I2_ENTRY_BYTES;
    assert(rows>=0 && rows<=11);
    var storage: f32[27648];
    let bridge_input=i2d_address(unsafe_c_address<c_ptr<const void>>(storage[0]));
    i2d_transfer(bridge_input,current,I2_HOST_ROW);
    i2d_transfer(bridge_input+I2_HOST_ROW,history,(rows as! u64)*I2_HOST_ROW);
    i2_joined_bind(entry,decoder,aux,slot,position,bridge_input,rows,add_residual);
    if i2d_read(entry+I2_ENTRY_PLAN)!=0 { i2_joined_rebind(entry,position,input,messages); }
    else { i2_joined_create(entry,decoder,core,aux,slot,position,sliding,input,messages); }
    fg_i2_host_execute_entry(entry,current);
    i2d_set_byte(entry+I2_JOINED_STARTED,1);
}
