const CPU_MASK_BITS: i64 = 512;

struct GraphHostSlice { address: u64, len: i64 }

extern "memcpy" fn graph_host_copy(dst: c_ptr<void>, src: c_ptr<const void>, size: u64) -> c_ptr<void>;
extern "gs_os_write_stderr" fn graph_host_log(text: const u8[:]);
extern fn graph_host_strlen(address: u64) -> u64;
extern fn graph_host_perror(address: u64);
extern fn graph_host_getaffinity(pid: i32, size: u64, out: u64) -> i32;
extern fn graph_host_setaffinity(pid: i32, size: u64, mask: u64) -> i32;
extern fn graph_host_getcpu() -> i32;

extern fn fg143_cpu_batch_supported(threads: i64) -> bool;
extern fn fg143_storage_allocate(graph: u64) -> u64;
extern fn fg143_graph_plan_alloc(graph: u64, output: u64) -> u64;
extern fn fg143_graph_execute(graph: u64, output: u64, all: bool, deadlines: GraphHostSlice,
    trace: bool, prefill: bool, affinity: u64) -> i32;
extern fn q35_cpumask_parse(text: GraphHostSlice, mask: GraphHostSlice) -> bool;
extern fn th_cpumask_next(mask: GraphHostSlice, strict: i64, first: i64, local: GraphHostSlice) -> i64;

var graph_affinity: u64[CPU_AFFINITY_WORDS * CPU_MASK_BITS * 2];
var graph_affinity_count: i64[2];

fn graph_host_address(pointer: c_ptr<const void>) -> u64 {
    var address: u64 = 0;
    graph_host_copy(unsafe_c_address<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(pointer), 8);
    return address;
}

fn graph_host_word(address: u64) -> u64 {
    var value: u64 = 0;
    graph_host_copy(unsafe_c_address<c_ptr<void>>(value),unsafe_c_ptr<c_ptr<const void>>(address),8);
    return value;
}

fn graph_host_i32(address: u64) -> i32 {
    var value: i32 = 0;
    graph_host_copy(unsafe_c_address<c_ptr<void>>(value),unsafe_c_ptr<c_ptr<const void>>(address),4);
    return value;
}

fn graph_host_error(text: const u8[:]) {
    var message: u8[40];
    for i in text.len { message[i]=text[i]; }
    message[text.len]=0;
    graph_host_perror(graph_host_address(unsafe_c_address<c_ptr<const void>>(message[0])));
}

fn configure_cpu_threads(prefill: bool, threads: i64) -> i64 {
    if threads <= 0 || threads > CPU_MASK_BITS { return -1; }
    fg_graph_native_cpu_init();
    if !fg143_cpu_batch_supported(threads) { return -1; }
    fg_graph_native_threads(prefill,threads);
    return 0;
}

export fn fg_standalone_configure_cpu_batch(threads: i64) -> i64 { return configure_cpu_threads(true,threads); }
export fn fg_standalone_configure_cpu_decode(threads: i64) -> i64 { return configure_cpu_threads(false,threads); }
export fn fg_standalone_cpu_batch_threads() -> i64 { return fg_graph_native_threads(true,0); }
export fn fg_standalone_cpu_decode_threads() -> i64 { return fg_graph_native_threads(false,0); }
export fn fg_standalone_set_cpu_prefill(prefill: bool) -> bool { return fg_graph_native_set_prefill(prefill); }
export fn fg143_cpu_prefill() -> bool { return fg_graph_native_prefill(); }
export fn fg143_cpu_gather_threads() -> i64 { return fg_graph_native_threads(fg_graph_native_prefill(),0); }

fn graph_profile(prefill: bool) -> i64 { return if prefill { 0 } else { 1 }; }
fn graph_mask_word(profile: i64, worker: i64) -> i64 { return (profile * CPU_MASK_BITS + worker)*CPU_AFFINITY_WORDS; }
fn graph_mask_address(profile: i64, worker: i64) -> u64 {
    return graph_host_address(unsafe_c_address<c_ptr<const void>>(graph_affinity[graph_mask_word(profile,worker)]));
}

export fn fg_standalone_configure_cpu_affinity(text: u64, strict: bool, prefill: bool) -> i64 {
    if text == 0 { return -1; }
    var allowed: u64[CPU_AFFINITY_WORDS];
    let allowed_address=graph_host_address(unsafe_c_address<c_ptr<const void>>(allowed[0]));
    if graph_host_getaffinity(0,(CPU_AFFINITY_WORDS*8) as! u64,allowed_address)!=0 { return -1; }
    fg_graph_native_cpu_init();
    var mask: i64[CPU_MASK_BITS];
    var local: i64[CPU_MASK_BITS];
    let span=GraphHostSlice {address:graph_host_address(unsafe_c_address<c_ptr<const void>>(mask[0])),len:CPU_MASK_BITS};
    let temporary=GraphHostSlice {address:graph_host_address(unsafe_c_address<c_ptr<const void>>(local[0])),len:CPU_MASK_BITS};
    if !q35_cpumask_parse(GraphHostSlice {address:text,len:graph_host_strlen(text) as! i64},span) { return -1; }
    for cpu in CPU_MASK_BITS {
        if mask[cpu]==0 { continue; }
        let word=cpu/64;
        let bit=cpu%64;
        if word>=CPU_AFFINITY_WORDS || ((allowed[word] >> (bit as! u64)) & 1)==0 { return -1; }
    }
    let profile=graph_profile(prefill);
    let count=fg_graph_native_threads(prefill,0);
    var next: i64=0;
    for worker in count {
        next=th_cpumask_next(span,if strict { 1 } else { 0 },next,temporary);
        let at=graph_mask_word(profile,worker);
        for word in CPU_AFFINITY_WORDS { graph_affinity[at+word]=0; }
        for cpu in CPU_MASK_BITS {
            if local[cpu]!=0 && cpu<CPU_AFFINITY_WORDS*64 {
                graph_affinity[at+cpu/64] |= (1 as! u64) << ((cpu%64) as! u64);
            }
        }
    }
    graph_affinity_count[profile]=count;
    return 0;
}

fn graph_bit_count(words: const u64[:]) -> i64 {
    var total: i64=0;
    for i in words.len {
        var word=words[i];
        while word!=0 { word &= word-1; total+=1; }
    }
    return total;
}

export fn fg143_gather_affinity(prefill: bool, worker: i64) -> i64 {
    let profile=graph_profile(prefill);
    let count=graph_affinity_count[profile];
    if count==0 { return 0; }
    let phase=if prefill { "prefill" } else { "decode" };
    if worker<0 || worker>=count || graph_host_setaffinity(0,(CPU_AFFINITY_WORDS*8) as! u64,
        graph_mask_address(profile,worker))!=0 {
        graph_host_log(str("Goose CPU affinity failed for ",phase," worker ",worker,"\n"));
        return -1;
    }
    if fg143_graph_log_enabled() {
        var mask: u64[CPU_AFFINITY_WORDS];
        if graph_host_getaffinity(0,(CPU_AFFINITY_WORDS*8) as! u64,
            graph_host_address(unsafe_c_address<c_ptr<const void>>(mask[0])))!=0 { return -1; }
        graph_host_log(str("Goose CPU affinity: ",phase," worker ",worker," cpu ",graph_host_getcpu(),
            " allowed ",graph_bit_count(mask),"\n"));
    }
    return 0;
}

export fn fg143_graph_restore_affinity(address: u64) -> bool {
    if address==0 || graph_host_setaffinity(0,(CPU_AFFINITY_WORDS*8) as! u64,address)==0 { return true; }
    graph_host_error("Goose CPU affinity restore");
    return false;
}

export fn fg_standalone_graph_alloc(graph: u64) -> u64 {
    fg_graph_native_cpu_init();
    return fg143_storage_allocate(graph);
}

export fn fg_standalone_graph_plan_alloc(graph: u64,output: u64) -> u64 {
    if graph==0 || graph_host_i32(graph+4)<0 { return 0; }
    fg_graph_native_cpu_init();
    return fg143_graph_plan_alloc(graph,output);
}

fn graph_host_compute(graph: u64,all: bool,output: u64,deadlines: GraphHostSlice) -> i32 {
    fg_graph_native_cpu_init();
    let prefill=fg_graph_native_prefill();
    let pin=graph_affinity_count[graph_profile(prefill)]!=0;
    var previous: u64[CPU_AFFINITY_WORDS];
    var address: u64=0;
    if pin {
        address=graph_host_address(unsafe_c_address<c_ptr<const void>>(previous[0]));
        if graph_host_getaffinity(0,(CPU_AFFINITY_WORDS*8) as! u64,address)!=0 {
            graph_host_error("Goose CPU affinity snapshot");
            return -1;
        }
    }
    return fg143_graph_execute(graph,output,all,deadlines,fg_graph_native_checkpoint(),prefill,address);
}

export fn fg_standalone_graph_compute(graph: u64) -> i32 {
    return graph_host_compute(graph,true,0,GraphHostSlice {address:0,len:0});
}

export fn fg_standalone_graph_compute_output(graph: u64,output: u64) -> i32 {
    return graph_host_compute(graph,false,output,GraphHostSlice {address:0,len:0});
}

export fn fg_standalone_graph_plan_compute(plan: u64) -> i32 {
    if plan==0 { return -1; }
    let graph=graph_host_word(plan);
    let nodes=graph_host_word(plan+24) as! i64;
    if nodes != (graph_host_i32(graph+4) as! i64) { return -1; }
    let deadlines=GraphHostSlice {address:plan+40,len:nodes};
    return graph_host_compute(graph,false,graph_host_word(plan+8),deadlines);
}
