extern fn fg_cuda_host_getenv(address: u64) -> u64;
extern fn ggml_op_name(op: i32) -> u64;

fn graph_host_byte(address: u64) -> u8 {
    var value: u8=0;
    graph_host_copy(unsafe_c_address<c_ptr<void>>(value),unsafe_c_ptr<c_ptr<const void>>(address),1);
    return value;
}

fn graph_host_text(address: u64) -> u8[>..] {
    var text: u8[>..]=[];
    if address==0 { return text; }
    var offset: u64=0;
    loop {
        let byte=graph_host_byte(address+offset);
        if byte==0 { break; }
        text.push(byte);
        offset+=1;
    }
    return text;
}

fn graph_env_set(name: const u8[:]) -> bool {
    var key: u8[32];
    for i in name.len { key[i]=name[i]; }
    key[name.len]=0;
    return fg_cuda_host_getenv(graph_host_address(unsafe_c_address<c_ptr<const void>>(key[0])))!=0;
}

export fn fg143_graph_log_enabled() -> bool { return graph_env_set("GOOSE_GRAPH_LOG"); }

export fn fg143_gather_witness(rows: i64,tasks: i64) {
    if fg143_graph_log_enabled() {
        graph_host_log(str("Goose CPU GET_ROWS: ",rows," rows over ",tasks," threads\n"));
    }
}

export fn fg143_storage_ram_error(required: u64,available: u64) {
    graph_host_log(str("Goose graph allocation refused: need ",required," bytes, RAM available ",available," bytes\n"));
}

export fn fg143_storage_report(bytes: u64,blocks: i64,reused: i64) {
    if graph_env_set("GOOSE_ARENA_LOG") {
        graph_host_log(str("Goose tensor arena: ",bytes," bytes, ",blocks," blocks, ",reused," reuses\n"));
    }
}

export fn fg143_graph_plan_log_replays(replays: u64) {
    graph_host_log(str("Goose decode plan reuses: ",replays,"\n"));
}

export fn fg143_graph_log_failure(index: i64,op: i32) {
    graph_host_log(str("Goose graph: node ",index," (",graph_host_text(ggml_op_name(op)),") failed\n"));
}

fn graph_log_field(text: const u8[:]) -> const u8[:] {
    var size: i64=0;
    while size<text.len && text[size]!=0 { size+=1; }
    return text[0..size];
}

export fn fg143_graph_log_count(label: const u8[:],count: u64,unit: const u8[:]) {
    graph_host_log(str(graph_log_field(label),": ",count,graph_log_field(unit),"\n"));
}

export fn fg143_graph_log_pair(label: const u8[:],count: u64,unit: const u8[:],next: u64,next_unit: const u8[:]) {
    graph_host_log(str(graph_log_field(label),": ",count,graph_log_field(unit),", ",next,graph_log_field(next_unit),"\n"));
}

export fn fg143_graph_name_unnamed(node: u64,leaf: bool,index: i32) {
    let name=node+256;
    if graph_host_byte(name)!=0 { return; }
    let prefix=if leaf { "leaf_" } else { "node_" };
    let text=str(prefix,index);
    graph_host_copy(unsafe_c_ptr<c_ptr<void>>(name),unsafe_c_slice_data<c_ptr<const void>>(text[0..text.len]),text.len as! u64);
    let nul: u8=0;
    graph_host_copy(unsafe_c_ptr<c_ptr<void>>(name+(text.len as! u64)),unsafe_c_address<c_ptr<const void>>(nul),1);
}
