const GRAPH_VK: u64 = 0;
const GRAPH_SETS: u64 = 8;
const GRAPH_LAYOUT: u64 = 16;
const GRAPH_PIPELINES: u64 = 24;
const GRAPH_DESCRIPTORS: u64 = 328;
const GRAPH_POOL: u64 = 336;
const GRAPH_CMD: u64 = 344;
const GRAPH_DONE: u64 = 352;
const GRAPH_EPOCH: u64 = 360;
const GRAPH_CAPACITY: u64 = 368;
const GRAPH_USED: u64 = 372;
const GRAPH_RECORDING: u64 = 376;
const GRAPH_BYTES: u64 = 384;
const GRAPH_OPS: i64 = 38;
const GRAPH_BINDINGS: i64 = 4;
const GRAPH_DEVICE: u64 = 2016;
const GRAPH_ALIGNMENT: u64 = 664;
const GRAPH_RANGE: u64 = 364;
const GRAPH_GROUP_COUNT: u64 = 556;
const GRAPH_WHOLE_EPOCH: u64 = 0xffffffffffffffff;

type GraphCreate = c_fnptr(u64, u64, u64, u64) -> i32;
type GraphDestroy = c_fnptr(u64, u64, u64) -> void;
type GraphAlloc = c_fnptr(u64, u64, u64) -> i32;
type GraphPipelineCreate = c_fnptr(u64, u64, u32, u64, u64, u64) -> i32;
type GraphReset = c_fnptr(u64, u64, u32) -> i32;
type GraphBegin = c_fnptr(u64, u64) -> i32;
type GraphEnd = c_fnptr(u64) -> i32;
type GraphBarrier = c_fnptr(u64, u64) -> void;
type GraphUpdate = c_fnptr(u64, u32, u64, u32, u64) -> void;
type GraphBind = c_fnptr(u64, u32, u64) -> void;
type GraphBindSets = c_fnptr(u64, u32, u64, u32, u32, u64, u32, u64) -> void;
type GraphPush = c_fnptr(u64, u64, u32, u32, u32, u64) -> void;
type GraphDispatch = c_fnptr(u64, u32, u32, u32) -> void;

extern fn fg_vk_shader_info(op: u32, out: u64);
extern fn fg_vk_submit_wait(vk: u64, command: u64, done: u64, epoch: u64) -> i32;

fn graph_address(pointer: c_ptr<const void>) -> u64 {
    var address: u64 = 0;
    bank_copy(unsafe_c_address<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(pointer), 8);
    return address;
}

fn graph_store(address: u64, value: u64) {
    bank_copy(unsafe_c_ptr<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(value), 8);
}

fn graph_fail(message: const u8[:], status: i32) -> i32 {
    bank_error(str("Vulkan: ", message, " (", status, ")\n"));
    return 0;
}

fn graph_release(vk: u64, device: u64, callback: u64, resource: u64) {
    if resource != 0 { unsafe_c_fnptr<GraphDestroy>(bank_word(vk + callback))(device, resource, 0); }
}

export fn fg_vk_graph_close(graph: u64) {
    let vk = bank_word(graph);
    if vk == 0 { return; }
    let device = bank_word(vk + GRAPH_DEVICE);
    graph_release(vk, device, 2152, bank_word(graph + GRAPH_POOL));
    graph_release(vk, device, 2224, bank_word(graph + GRAPH_DONE));
    graph_release(vk, device, 2328, bank_word(graph + GRAPH_DESCRIPTORS));
    for i in GRAPH_OPS {
        graph_release(vk, device, 2312, bank_word(graph + GRAPH_PIPELINES + (i as! u64) * 8));
    }
    graph_release(vk, device, 2280, bank_word(graph + GRAPH_LAYOUT));
    graph_release(vk, device, 2264, bank_word(graph + GRAPH_SETS));
    bank_fill(unsafe_c_ptr<c_ptr<void>>(graph), 0, GRAPH_BYTES);
}

fn graph_pipeline(graph: u64, vk: u64, device: u64, op: u32) -> i32 {
    var shader: u64[2];
    fg_vk_shader_info(op, graph_address(unsafe_c_address<c_ptr<const void>>(shader[0])));
    let info: u64[5] = [16, 0, 0, shader[1], shader[0]];
    var module: u64 = 0;
    let result = unsafe_c_fnptr<GraphCreate>(bank_word(vk + 2288))(device,
        graph_address(unsafe_c_address<c_ptr<const void>>(info[0])), 0,
        graph_address(unsafe_c_address<c_ptr<const void>>(module)));
    if result != 0 { return result; }
    let name: u8[5] = [109,97,105,110,0];
    let pipeline: u64[12] = [29, 0, 0, 18, 0, 0x2000000000, module,
        graph_address(unsafe_c_address<c_ptr<const void>>(name[0])), 0, bank_word(graph + GRAPH_LAYOUT), 0, 0];
    let status = unsafe_c_fnptr<GraphPipelineCreate>(bank_word(vk + 2304))(device, 0, 1,
        graph_address(unsafe_c_address<c_ptr<const void>>(pipeline[0])), 0,
        graph + GRAPH_PIPELINES + (op as! u64) * 8);
    graph_release(vk, device, 2296, module);
    return status;
}

fn graph_create_resources(graph: u64, vk: u64, capacity: u32) -> i32 {
    let device = bank_word(vk + GRAPH_DEVICE);
    var bindings: u64[12] = [0; 12];
    for i in GRAPH_BINDINGS {
        bindings[i * 3] = (0x700000000 as! u64) | (i as! u64);
        bindings[i * 3 + 1] = 0x2000000001;
    }
    let sets: u64[4] = [32, 0, 0x400000000, graph_address(unsafe_c_address<c_ptr<const void>>(bindings[0]))];
    var result = unsafe_c_fnptr<GraphCreate>(bank_word(vk + 2256))(device,
        graph_address(unsafe_c_address<c_ptr<const void>>(sets[0])), 0, graph + GRAPH_SETS);
    if result != 0 { return result; }

    let push: u32[3] = [32, 0, 36];
    let layout: u64[6] = [30, 0, 0x100000000, graph + GRAPH_SETS, 1,
        graph_address(unsafe_c_address<c_ptr<const void>>(push[0]))];
    result = unsafe_c_fnptr<GraphCreate>(bank_word(vk + 2272))(device,
        graph_address(unsafe_c_address<c_ptr<const void>>(layout[0])), 0, graph + GRAPH_LAYOUT);
    if result != 0 { return result; }

    let sizes: u32[2] = [7, capacity * 4];
    let descriptors: u64[5] = [33, 0, (capacity as! u64) << 32, 1,
        graph_address(unsafe_c_address<c_ptr<const void>>(sizes[0]))];
    result = unsafe_c_fnptr<GraphCreate>(bank_word(vk + 2320))(device,
        graph_address(unsafe_c_address<c_ptr<const void>>(descriptors[0])), 0, graph + GRAPH_DESCRIPTORS);
    if result != 0 { return result; }

    let family = bank_read_u32(vk + 2044) as! u64;
    let pool: u64[3] = [39, 0, 1 | (family << 32)];
    result = unsafe_c_fnptr<GraphCreate>(bank_word(vk + 2144))(device,
        graph_address(unsafe_c_address<c_ptr<const void>>(pool[0])), 0, graph + GRAPH_POOL);
    if result != 0 { return result; }
    let command: u64[4] = [40, 0, bank_word(graph + GRAPH_POOL), 0x100000000];
    result = unsafe_c_fnptr<GraphAlloc>(bank_word(vk + 2160))(device,
        graph_address(unsafe_c_address<c_ptr<const void>>(command[0])), graph + GRAPH_CMD);
    if result != 0 { return result; }

    let timeline: u64[4] = [1000207002, 0, 1, 0];
    let done: u64[3] = [9, graph_address(unsafe_c_address<c_ptr<const void>>(timeline[0])), 0];
    result = unsafe_c_fnptr<GraphCreate>(bank_word(vk + 2216))(device,
        graph_address(unsafe_c_address<c_ptr<const void>>(done[0])), 0, graph + GRAPH_DONE);
    if result != 0 { return result; }
    for i in GRAPH_OPS {
        result = graph_pipeline(graph, vk, device, i as! u32);
        if result != 0 { return result; }
    }
    return 0;
}

export fn fg_vk_graph_open(graph: u64, vk: u64, capacity: u32) -> i32 {
    bank_fill(unsafe_c_ptr<c_ptr<void>>(graph), 0, GRAPH_BYTES);
    graph_store(graph, vk);
    bank_write_u32(graph + GRAPH_CAPACITY, capacity);
    if capacity == 0 || capacity > 0x3fffffff { return graph_fail("invalid dispatch capacity", -3); }
    let result = graph_create_resources(graph, vk, capacity);
    if result == 0 { return 1; }
    fg_vk_graph_close(graph);
    return graph_fail("initialize compute graph", result);
}

export fn fg_vk_graph_begin(graph: u64) -> i32 {
    let command = bank_word(graph + GRAPH_CMD);
    if command == 0 || bank_read_u32(graph + GRAPH_RECORDING) != 0 ||
        bank_word(graph + GRAPH_EPOCH) == GRAPH_WHOLE_EPOCH { return 0; }
    let vk = bank_word(graph);
    let device = bank_word(vk + GRAPH_DEVICE);
    var result = unsafe_c_fnptr<GraphReset>(bank_word(vk + 2168))(device, bank_word(graph + GRAPH_POOL), 0);
    if result == 0 {
        result = unsafe_c_fnptr<GraphReset>(bank_word(vk + 2336))(device, bank_word(graph + GRAPH_DESCRIPTORS), 0);
    }
    if result == 0 {
        let begin: u64[4] = [42, 0, 1, 0];
        result = unsafe_c_fnptr<GraphBegin>(bank_word(vk + 2176))(command,
            graph_address(unsafe_c_address<c_ptr<const void>>(begin[0])));
    }
    if result != 0 { return graph_fail("begin compute graph", result); }
    bank_write_u32(graph + GRAPH_RECORDING, 1);
    bank_write_u32(graph + GRAPH_USED, 0);
    return 1;
}

fn graph_valid(vk: u64, view: BankView, required: u64) -> bool {
    if view.buffer == 0 || bank_word(view.buffer) == 0 || (bank_read_u32(view.buffer + 20) & 32) == 0 ||
        view.offset % bank_word(vk + GRAPH_ALIGNMENT) != 0 || view.size < required ||
        view.size > (bank_read_u32(vk + GRAPH_RANGE) as! u64) { return false; }
    let capacity = bank_word(view.buffer + 24);
    return view.offset <= capacity && view.size <= capacity - view.offset;
}

fn graph_overlap(a: BankView, b: BankView) -> bool {
    return bank_word(a.buffer) == bank_word(b.buffer) &&
        a.offset < b.offset + b.size && b.offset < a.offset + a.size;
}

fn graph_barrier(graph: u64, vk: u64, stage: u64, access: u64) {
    let memory: u64[6] = [1000314000, 0, 2048, 64, stage, access];
    let deps: u64[8] = [1000314003, 0, 0x100000000,
        graph_address(unsafe_c_address<c_ptr<const void>>(memory[0])), 0, 0, 0, 0];
    unsafe_c_fnptr<GraphBarrier>(bank_word(vk + 2208))(bank_word(graph + GRAPH_CMD),
        graph_address(unsafe_c_address<c_ptr<const void>>(deps[0])));
}

fn graph_record(graph: u64, op: u32, shape: BankShape, input: BankView, weight: BankView,
        output: BankView, values: BankView) -> i32 {
    let used = bank_read_u32(graph + GRAPH_USED);
    if bank_read_u32(graph + GRAPH_RECORDING) == 0 || used >= bank_read_u32(graph + GRAPH_CAPACITY) ||
        op >= (GRAPH_OPS as! u32) { return 0; }
    let alias = (op == GRAPH_CAUSAL_CONV && (graph_overlap(input, values) || graph_overlap(weight, values))) ||
        (op == GRAPH_GDN && (graph_overlap(input, weight) || graph_overlap(weight, values)));
    var plan: u64[6];
    if fg_vk_dispatch_plan(op, shape, if alias { 1 } else { 0 },
        graph_address(unsafe_c_address<c_ptr<const void>>(plan[0]))) == 0 { return 0; }
    let vk = bank_word(graph);
    let x = plan[4] as! u32;
    let y = plan[5] as! u32;
    if x > bank_read_u32(vk + GRAPH_GROUP_COUNT) || y > bank_read_u32(vk + GRAPH_GROUP_COUNT + 4) ||
        !graph_valid(vk, input, plan[0]) || !graph_valid(vk, weight, plan[1]) ||
        !graph_valid(vk, output, plan[2]) || !graph_valid(vk, values, plan[3]) ||
        graph_overlap(input, output) || graph_overlap(weight, output) || graph_overlap(values, output) { return 0; }

    let device = bank_word(vk + GRAPH_DEVICE);
    let alloc: u64[5] = [34, 0, bank_word(graph + GRAPH_DESCRIPTORS), 1, graph + GRAPH_SETS];
    var set: u64 = 0;
    let set_address = graph_address(unsafe_c_address<c_ptr<const void>>(set));
    let result = unsafe_c_fnptr<GraphAlloc>(bank_word(vk + 2344))(device,
        graph_address(unsafe_c_address<c_ptr<const void>>(alloc[0])), set_address);
    if result != 0 { return graph_fail("allocate dispatch descriptors", result); }

    let views: BankView[4] = [input, weight, output, values];
    var buffers: u64[12];
    var writes: u64[32] = [0; 32];
    for i in GRAPH_BINDINGS {
        buffers[i * 3] = bank_word(views[i].buffer);
        buffers[i * 3 + 1] = views[i].offset;
        buffers[i * 3 + 2] = views[i].size;
        writes[i * 8] = 35;
        writes[i * 8 + 2] = set;
        writes[i * 8 + 3] = i as! u64;
        writes[i * 8 + 4] = 0x700000001;
        writes[i * 8 + 6] = graph_address(unsafe_c_address<c_ptr<const void>>(buffers[i * 3]));
    }
    unsafe_c_fnptr<GraphUpdate>(bank_word(vk + 2352))(device, 4,
        graph_address(unsafe_c_address<c_ptr<const void>>(writes[0])), 0, 0);
    if used != 0 || bank_word(graph + GRAPH_EPOCH) != 0 { graph_barrier(graph, vk, 2048, 96); }

    let command = bank_word(graph + GRAPH_CMD);
    let layout = bank_word(graph + GRAPH_LAYOUT);
    unsafe_c_fnptr<GraphBind>(bank_word(vk + 2360))(command, 1,
        bank_word(graph + GRAPH_PIPELINES + (op as! u64) * 8));
    unsafe_c_fnptr<GraphBindSets>(bank_word(vk + 2368))(command, 1, layout, 0, 1, set_address, 0, 0);
    unsafe_c_fnptr<GraphPush>(bank_word(vk + 2376))(command, layout, 32, 0, 36,
        graph_address(unsafe_c_address<c_ptr<const void>>(shape)));
    unsafe_c_fnptr<GraphDispatch>(bank_word(vk + 2384))(command, x, y, 1);
    bank_write_u32(graph + GRAPH_USED, used + 1);
    return 1;
}

export fn fg_vk_graph_record(graph: u64, op: u32, shape: BankShape, input: BankView, weight: BankView, output: BankView) -> i32 {
    if op == GRAPH_ATTENTION || op == GRAPH_ATTENTION_Q4 || op == GRAPH_INKLING_ATTENTION ||
        op == GRAPH_CAUSAL_CONV || op == GRAPH_GDN || op == GRAPH_GDN_GATES || op == GRAPH_GDN_APPLY ||
        op == GRAPH_GATED_RMS { return 0; }
    return graph_record(graph, op, shape, input, weight, output, input);
}

export fn fg_vk_graph_conv(graph: u64, channels: u32, input: BankView, coefficients: BankView, history: BankView, output: BankView) -> i32 {
    if input.buffer == 0 || coefficients.buffer == 0 || history.buffer == 0 { return 0; }
    return graph_record(graph, GRAPH_CAUSAL_CONV, bank_shape(channels, 0), input, coefficients, output, history);
}

export fn fg_vk_graph_gdn(graph: u64, shape: BankShape, qkv: BankView, state: BankView, gates: BankView, output: BankView) -> i32 {
    if qkv.buffer == 0 || state.buffer == 0 || gates.buffer == 0 { return 0; }
    return graph_record(graph, GRAPH_GDN, shape, qkv, state, output, gates);
}

export fn fg_vk_graph_gdn_gates(graph: u64, shape: BankShape, input: BankView, weights: BankView, bias: BankView, output: BankView) -> i32 {
    return graph_record(graph, GRAPH_GDN_GATES, shape, input, weights, output, bias);
}

export fn fg_vk_graph_gdn_apply(graph: u64, heads: u32, alpha: BankView, beta: BankView, bias: BankView, output: BankView) -> i32 {
    return graph_record(graph, GRAPH_GDN_APPLY, bank_shape(heads, 0), alpha, beta, output, bias);
}

export fn fg_vk_graph_gated_rms(graph: u64, shape: BankShape, input: BankView, weights: BankView, gate: BankView, output: BankView) -> i32 {
    return graph_record(graph, GRAPH_GATED_RMS, shape, input, weights, output, gate);
}

export fn fg_vk_graph_attention(graph: u64, shape: BankShape, query: BankView, keys: BankView, values: BankView, output: BankView) -> i32 {
    return graph_record(graph, GRAPH_ATTENTION, shape, query, keys, output, values);
}

export fn fg_vk_graph_attention_q4(graph: u64, shape: BankShape, query: BankView, keys: BankView, values: BankView, output: BankView) -> i32 {
    return graph_record(graph, GRAPH_ATTENTION_Q4, shape, query, keys, output, values);
}

export fn fg_vk_graph_inkling_attention(graph: u64, shape: BankShape, query: BankView, keys: BankView, values: BankView, output: BankView) -> i32 {
    return graph_record(graph, GRAPH_INKLING_ATTENTION, shape, query, keys, output, values);
}

export fn fg_vk_graph_submit(graph: u64) -> i32 {
    if bank_read_u32(graph + GRAPH_RECORDING) == 0 || bank_read_u32(graph + GRAPH_USED) == 0 { return 0; }
    let vk = bank_word(graph);
    let cmd = bank_word(graph + GRAPH_CMD);
    graph_barrier(graph, vk, 16384, 8192);
    bank_write_u32(graph + GRAPH_RECORDING, 0);
    let result = unsafe_c_fnptr<GraphEnd>(bank_word(vk + 2184))(cmd);
    if result != 0 { return graph_fail("end compute graph", result); }
    let epoch = bank_word(graph + GRAPH_EPOCH) + 1;
    graph_store(graph + GRAPH_EPOCH, epoch);
    return fg_vk_submit_wait(vk, cmd, bank_word(graph + GRAPH_DONE), epoch);
}
