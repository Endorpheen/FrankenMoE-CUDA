const GRAPH_RMSNORM: u32 = 0;
const GRAPH_Q4_0: u32 = 1;
const GRAPH_SWIGLU: u32 = 2;
const GRAPH_ROPE: u32 = 3;
const GRAPH_KV_STORE: u32 = 4;
const GRAPH_ATTENTION: u32 = 5;
const GRAPH_Q8_0: u32 = 6;
const GRAPH_GATHER_Q4_0: u32 = 7;
const GRAPH_BIAS: u32 = 8;
const GRAPH_ADD: u32 = 9;
const GRAPH_PQ2_0: u32 = 10;
const GRAPH_GATHER_PQ2_0: u32 = 11;
const GRAPH_FWHT: u32 = 12;
const GRAPH_CAUSAL_CONV: u32 = 13;
const GRAPH_QK_NORM: u32 = 14;
const GRAPH_GDN: u32 = 15;
const GRAPH_GDN_GATES: u32 = 16;
const GRAPH_GATED_RMS: u32 = 17;
const GRAPH_HEAD_SLICE: u32 = 18;
const GRAPH_SIGMOID_GATE: u32 = 19;
const GRAPH_KV_STORE_Q4: u32 = 20;
const GRAPH_ATTENTION_Q4: u32 = 21;
const GRAPH_Q4_K: u32 = 22;
const GRAPH_Q6_K: u32 = 23;
const GRAPH_GATHER_Q4_K: u32 = 24;
const GRAPH_IQ4_NL: u32 = 25;
const GRAPH_IQ4_XS: u32 = 26;
const GRAPH_GATHER_Q8_0: u32 = 27;
const GRAPH_F32_PROJECT: u32 = 28;
const GRAPH_MOE_ROUTE: u32 = 29;
const GRAPH_MOE_WEIGHTED: u32 = 30;
const GRAPH_APEX_QK_NORM: u32 = 31;
const GRAPH_GDN_APPLY: u32 = 32;
const GRAPH_SIGMOID_GATE_SCALAR: u32 = 33;
const GRAPH_KV_FWHT: u32 = 34;
const GRAPH_INKLING_ROUTE: u32 = 35;
const GRAPH_INKLING_SITU: u32 = 36;
const GRAPH_INKLING_ATTENTION: u32 = 37;

const GRAPH_MAX_U32: u64 = 0xffffffff;

fn graph_finite(value: f32) -> bool {
    var bits: u32 = 0;
    bank_copy(unsafe_c_address<c_ptr<void>>(bits), unsafe_c_address<c_ptr<const void>>(value), 4);
    return (bits & 0x7f800000) != 0x7f800000;
}

fn graph_packed_size(width: u64, rows: u64, op: u32) -> u64 {
    var block_width: u64 = 32;
    var block_size: u64 = 34;
    if op == GRAPH_Q4_K || op == GRAPH_GATHER_Q4_K { block_width = 256; block_size = 144; }
    else if op == GRAPH_Q6_K { block_width = 256; block_size = 210; }
    else if op == GRAPH_IQ4_XS { block_width = 256; block_size = 136; }
    else if op == GRAPH_Q4_0 || op == GRAPH_IQ4_NL || op == GRAPH_GATHER_Q4_0 { block_size = 18; }
    else if op == GRAPH_PQ2_0 || op == GRAPH_GATHER_PQ2_0 { block_width = 128; }
    if width % block_width != 0 || rows == 0 { return 0; }
    let blocks = width / block_width * rows;
    if blocks > (GRAPH_MAX_U32 - 3) / block_size { return 0; }
    return (blocks * block_size + 3) & 0xfffffffffffffffc;
}

export fn fg_vk_dispatch_plan(op: u32, shape: BankShape, aliased_state: i32, plan: u64) -> i32 {
    let width = shape.width as! u64;
    let rows = shape.rows as! u64;
    let batch = shape.batch as! u64;
    let position = shape.position as! u64;
    let context = shape.context as! u64;
    let kv_heads = shape.kv_heads as! u64;
    let rotary = shape.rotary as! u64;
    let elements = width * batch;
    if elements == 0 || elements > GRAPH_MAX_U32 { return 0; }
    let stride = width * rows;
    let end = position + batch;
    var input = elements * 4;
    var output = input;
    var weight: u64 = 4;
    var value: u64 = 4;
    var x: u64 = 0;
    var y: u64 = 1;
    if op == GRAPH_RMSNORM || op == GRAPH_GATED_RMS {
        if !graph_finite(shape.epsilon) || shape.epsilon <= 0.0 as! f32 { return 0; }
        x = batch;
        weight = width * 4;
        if op == GRAPH_GATED_RMS { value = input; }
    } else if op == GRAPH_Q4_0 || op == GRAPH_Q4_K || op == GRAPH_Q6_K || op == GRAPH_IQ4_NL ||
        op == GRAPH_IQ4_XS || op == GRAPH_F32_PROJECT || op == GRAPH_PQ2_0 || op == GRAPH_Q8_0 {
        let output_stride = if context == 0 { rows } else { context };
        if position + rows > output_stride || output_stride * batch > GRAPH_MAX_U32 ||
            (op == GRAPH_F32_PROJECT && stride > GRAPH_MAX_U32) { return 0; }
        weight = if op == GRAPH_F32_PROJECT { stride * 4 } else { graph_packed_size(width, rows, op) };
        if weight == 0 { return 0; }
        x = rows;
        y = batch;
        output = output_stride * batch * 4;
    } else if op == GRAPH_MOE_ROUTE || op == GRAPH_INKLING_ROUTE {
        if width > 4096 || rows > width || rows == 0 || rows > 64 || !graph_finite(shape.theta) ||
            shape.theta < 0.0 as! f32 || rows * batch > GRAPH_MAX_U32 / 2 { return 0; }
        if op == GRAPH_INKLING_ROUTE { weight = width * 4; }
        output = rows * batch * 8;
        x = batch;
    } else if op == GRAPH_MOE_WEIGHTED {
        if rows == 0 || rows > 64 || elements * rows > GRAPH_MAX_U32 { return 0; }
        input = elements * rows * 4;
        weight = rows * batch * 8;
        x = (elements + 63) / 64;
    } else if op == GRAPH_GATHER_Q4_0 || op == GRAPH_GATHER_Q4_K || op == GRAPH_GATHER_Q8_0 || op == GRAPH_GATHER_PQ2_0 {
        if position + rows > GRAPH_MAX_U32 { return 0; }
        weight = graph_packed_size(width, rows, op);
        if weight == 0 { return 0; }
        input = batch * 4;
        x = batch;
    } else if op == GRAPH_FWHT {
        if width % 1024 != 0 || position > 1 { return 0; }
        if context > 1 && (position != 0 || rows == 0 || kv_heads == 0 ||
            rows * kv_heads != width / context || width % context != 0) { return 0; }
        x = width / 1024;
        y = batch;
        weight = width * 4;
    } else if op == GRAPH_KV_FWHT {
        if (rows != 64 && rows != 256) || width % rows != 0 { return 0; }
        x = width / rows;
        y = batch;
    } else if op == GRAPH_CAUSAL_CONV {
        if batch != 1 || aliased_state != 0 { return 0; }
        x = (elements + 63) / 64;
        weight = elements * 16;
        value = elements * 12;
    } else if op == GRAPH_QK_NORM || op == GRAPH_APEX_QK_NORM {
        if batch != 1 || rows == 0 || kv_heads == 0 || kv_heads > rows || stride > GRAPH_MAX_U32 ||
            kv_heads > rows - kv_heads || !graph_finite(shape.epsilon) || shape.epsilon <= 0.0 as! f32 { return 0; }
        input = stride * 4;
        output = input;
        x = rows;
    } else if op == GRAPH_GDN {
        if batch != 1 || width > 256 || rows == 0 || kv_heads == 0 || rows % kv_heads != 0 ||
            stride > GRAPH_MAX_U32 / width || stride * 3 > GRAPH_MAX_U32 || aliased_state != 0 { return 0; }
        x = rows;
        y = width;
        input = width * (rows + 2 * kv_heads) * 4;
        weight = stride * width * 4;
        output = stride * 4;
        value = rows * 8;
    } else if op == GRAPH_GDN_GATES {
        if batch != 1 || rows == 0 || stride > GRAPH_MAX_U32 / 2 { return 0; }
        x = rows;
        weight = stride * 8;
        output = rows * 8;
        value = output;
    } else if op == GRAPH_GDN_APPLY {
        if batch != 1 || width > GRAPH_MAX_U32 / 2 { return 0; }
        x = (elements + 63) / 64;
        weight = input;
        output = input * 2;
        value = output;
    } else if op == GRAPH_BIAS {
        x = (elements + 63) / 64;
        weight = width * 4;
    } else if op == GRAPH_ADD || op == GRAPH_SIGMOID_GATE || op == GRAPH_SWIGLU || op == GRAPH_INKLING_SITU {
        x = (elements + 63) / 64;
        weight = input;
    } else if op == GRAPH_SIGMOID_GATE_SCALAR {
        x = (elements + 63) / 64;
        weight = batch * 4;
    } else if op == GRAPH_HEAD_SLICE {
        if position > 1 || stride == 0 || stride > GRAPH_MAX_U32 / 2 || stride * batch > GRAPH_MAX_U32 / 2 { return 0; }
        input = stride * batch * 8;
        output = stride * batch * 4;
        x = (stride * batch + 63) / 64;
    } else if op == GRAPH_ROPE {
        if width % 2 != 0 || stride == 0 || stride > GRAPH_MAX_U32 || stride * batch > GRAPH_MAX_U32 ||
            end > GRAPH_MAX_U32 || rotary > width || rotary % 2 != 0 ||
            !graph_finite(shape.theta) || shape.theta <= 0.0 as! f32 { return 0; }
        input = stride * batch * 4;
        output = input;
        x = rows;
        y = batch;
    } else if op == GRAPH_KV_STORE || op == GRAPH_KV_STORE_Q4 {
        if stride == 0 || stride > GRAPH_MAX_U32 || end > context || stride * context > GRAPH_MAX_U32 { return 0; }
        let count = stride * batch;
        input = count * 4;
        if op == GRAPH_KV_STORE_Q4 {
            if stride % 64 != 0 { return 0; }
            output = stride / 32 * context * 18;
            x = (count / 64 + 63) / 64;
        } else {
            output = stride * context * 4;
            x = (count + 63) / 64;
        }
    } else if op == GRAPH_ATTENTION || op == GRAPH_ATTENTION_Q4 {
        if width > 256 || rows == 0 || kv_heads == 0 || rows % kv_heads != 0 || stride > GRAPH_MAX_U32 ||
            stride * batch > GRAPH_MAX_U32 || end > context || width * kv_heads * context > GRAPH_MAX_U32 { return 0; }
        input = stride * batch * 4;
        output = input;
        let cache = width * kv_heads * context;
        if op == GRAPH_ATTENTION_Q4 && width % 64 != 0 { return 0; }
        weight = if op == GRAPH_ATTENTION_Q4 { cache / 32 * 18 } else { cache * 4 };
        value = weight;
        x = rows;
        y = batch;
    } else if op == GRAPH_INKLING_ATTENTION {
        if width != 128 || rows != 24 || kv_heads != 8 || batch != 1 || context == 0 || context > 1048576 ||
            position >= 1048576 || rotary > context || (rotary == 0 && end > context) { return 0; }
        input = (24 * 128 + 24 * 16 + 24 + 16 * 1024) * 4;
        output = 24 * 128 * 4;
        weight = context * 576;
        value = weight;
        x = rows;
    } else { return 0; }
    let dispatch: u64[6] = [input, weight, output, value, x, y];
    bank_copy(unsafe_c_ptr<c_ptr<void>>(plan), unsafe_c_address<c_ptr<const void>>(dispatch[0]), 48);
    return 1;
}
