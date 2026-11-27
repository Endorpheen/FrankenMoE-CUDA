import.codec.decode;
import.codec.half;

const CV_ADD: i64 = 1;
const CV_SUB: i64 = 2;
const CV_MUL: i64 = 3;
const CV_DIV: i64 = 4;
const CV_SCALE: i64 = 5;
const CV_CLAMP: i64 = 6;
const CV_SIGMOID: i64 = 7;
const CV_SILU: i64 = 8;
const CV_SOFTPLUS: i64 = 9;
const CV_SWIGLU: i64 = 10;
const CV_RMS: i64 = 11;
const CV_L2: i64 = 12;
const CV_SIGMOID_REFERENCE: i64 = 13;
const CV_SILU_REFERENCE: i64 = 14;
const CV_SWIGLU_REFERENCE: i64 = 15;
const CV_FLAT_PLAN: i64 = 8;
const VMM_POOL_CAPACITY: i64 = 34359738368;

fn vmm_map_growth(size: i64, used: i64, mapped: i64, granularity: i64) -> i64 {
    if size <= 0 || size > VMM_POOL_CAPACITY || used < 0 || mapped < used ||
       mapped > VMM_POOL_CAPACITY || granularity <= 0 || granularity > VMM_POOL_CAPACITY {
        return -1;
    }
    let aligned = 128 * ((size + 127) / 128);
    let need = aligned - (mapped - used);
    if need <= 0 { return 0; }
    let growth = granularity * (1 + (need - 1) / granularity);
    return if growth <= VMM_POOL_CAPACITY - mapped { growth } else { -1 };
}

export fn fg143_vmm_capacity() -> i64 { return VMM_POOL_CAPACITY; }

export fn fg143_vmm_scratch_growth(bytes: i64, mapped: i64, granularity: i64) -> i64 {
    return vmm_map_growth(bytes, 0, mapped, granularity);
}


export fn fg143_vmm_prepare(bytes: u64, mapped: u64, limit: u64, granularity: u64,
        failed: bool, has_address: bool, plan: i64[:]) -> bool {
    let capacity = VMM_POOL_CAPACITY as u64;
    if granularity == 0 || granularity > capacity || capacity % granularity != 0 { return false; }
    plan[0] = VMM_POOL_CAPACITY;
    if failed || bytes > limit || limit > capacity || mapped > capacity { return false; }
    let unit = granularity as i64;
    let growth = vmm_map_growth(bytes as i64, 0, mapped as i64, unit);
    if growth <= 0 { return false; }
    let reservation = if has_address { 0 } else { vmm_map_growth(limit as i64, 0, 0, unit) };
    if !has_address && reservation <= 0 { return false; }
    plan[1] = growth;
    plan[2] = reservation;
    return true;
}

export fn fg143_pinned_packet_slot(used: i64, capacity: i64, bytes: i64) -> i64 {
    if used < 0 || used > capacity || bytes <= 0 || bytes > 4096 { return -1; }
    let padding = (64 - used % 64) % 64;
    if padding > capacity - used || bytes > capacity - used - padding { return -1; }
    return used + padding;
}


const EXPERT_GROUP_WORDS: i64 = 9;
const EXPERT_MIN_PREFILL_COUNT: i64 = 64;
const EXPERT_MIB: u64 = 1048576;
const EXPERT_SIGNED_LIMIT: u64 = 9223372036854775807;

export fn fg143_expert_prefill_plan(groups: const i64[:], bank_bytes: u64, planes: u64,
        budget_mib: u64, plan: u64[:]) -> bool {
    let maximum: u64 = 0xffffffffffffffff;
    if groups.len % EXPERT_GROUP_WORDS != 0 || bank_bytes > maximum - 255 { return false; }
    var largest: u64 = 0;
    var group: i64 = 0;
    while group < groups.len {
        let experts = groups[group + 3];
        if groups[group + 5] > 0 && experts >= EXPERT_MIN_PREFILL_COUNT {
            for plane in 0..3 {
                let weight = groups[group + plane];
                if weight <= 0 || (weight as u64) > maximum / (experts as u64) { return false; }
                let bytes = (weight as u64) * (experts as u64);
                if bytes > largest { largest = bytes; }
            }
        }
        group += EXPERT_GROUP_WORDS;
    }
    if largest == 0 { return true; }
    let offset = (bank_bytes + 255) & (maximum - 255);
    if offset > EXPERT_SIGNED_LIMIT || largest > (EXPERT_SIGNED_LIMIT - offset) / planes { return false; }
    var workspace = largest * planes;
    if budget_mib != 0 {
        if budget_mib > (EXPERT_SIGNED_LIMIT - offset) / EXPERT_MIB { return false; }
        workspace = budget_mib * EXPERT_MIB;
        if workspace < largest { return false; }
    }
    plan[0] = offset;
    plan[1] = largest;
    plan[2] = workspace;
    return true;
}

export fn fg143_expert_balance_apex(groups: i64[:], bytes: i64, layer_words: i64) -> bool {
    if groups.len != 18 || layer_words != 160 || bytes != 6592856064 { return false; }
    let q4 = EXPERT_GROUP_WORDS;
    if groups[0] != 589824 || groups[1] != 589824 || groups[2] != 860160 ||
        groups[q4] != 589824 || groups[q4 + 1] != 589824 || groups[q4 + 2] != 589824 ||
        groups[3] != 256 || groups[q4 + 3] != 256 || groups[4] != 20 || groups[q4 + 4] != 20 ||
        groups[5] != 1616 || groups[q4 + 5] != 1863 || groups[6] != 0 { return false; }
    let q6_slots: i64 = 1904;
    let q4_slots: i64 = 1531;
    let q6_bytes = q6_slots * (groups[0] + groups[1] + groups[2]);
    let q4_bytes = q4_slots * (groups[q4] + groups[q4 + 1] + groups[q4 + 2]);
    if q6_bytes + q4_bytes > bytes { return false; }
    groups[q4 + 7] += 4 * (q6_slots - groups[5]);
    groups[5] = q6_slots;
    groups[q4 + 5] = q4_slots;
    groups[q4 + 6] = q6_bytes;
    return true;
}

export fn fg143_expert_source_pinned(spans: const i64[:], source: u64, bytes: u64) -> bool {
    var i: i64 = 0;
    while i + 1 < spans.len {
        let base = spans[i] as u64;
        let span = spans[i + 1] as u64;
        if source < base { return false; }
        if bytes <= span && source - base <= span - bytes { return true; }
        i += 2;
    }
    return false;
}

const EXPERT_UPLOAD_BYTES: i64 = 32;

fn expert_upload_word(jobs: const u8[:], word: i64) -> u64 {
    var value: u64 = 0;
    for byte in 8 {
        value |= (jobs[word * 8 + byte] as u64) << ((byte * 8) as u64);
    }
    return value;
}

export fn fg143_expert_pinned_batch(jobs: const u8[:]) -> bool {
    let count = jobs.len / EXPERT_UPLOAD_BYTES;
    for i in count {
        let bytes = expert_upload_word(jobs, 4 * i + 2);
        if jobs[i * EXPERT_UPLOAD_BYTES + 24] != 0 || bytes == 0 { return false; }
        let destination = expert_upload_word(jobs, 4 * i);
        for previous in i {
            let other = expert_upload_word(jobs, 4 * previous);
            if destination < other {
                if other - destination < bytes { return false; }
            } else if destination - other < expert_upload_word(jobs, 4 * previous + 2) {
                return false;
            }
        }
    }
    return true;
}

export fn fg143_expert_down_split(jobs: const u8[:], tensor: u64,
        plane_bytes: u64, experts: u64) -> i64 {
    var split = jobs.len / EXPERT_UPLOAD_BYTES;
    if plane_bytes == 0 { return split; }
    while split > 0 {
        let word = 4 * (split - 1);
        let source = expert_upload_word(jobs, word + 1);
        if source < tensor || expert_upload_word(jobs, word + 2) != plane_bytes { break; }
        let offset = source - tensor;
        if offset % plane_bytes != 0 || offset / plane_bytes >= experts { break; }
        split -= 1;
    }
    return split;
}

export fn fg143_expert_pageable_run(jobs: const u8[:], capacity: u64, total: u64[:]) -> i64 {
    var count: i64 = 0;
    var bytes: u64 = 0;
    while count < jobs.len / EXPERT_UPLOAD_BYTES && jobs[count * EXPERT_UPLOAD_BYTES + 24] != 0 {
        let size = expert_upload_word(jobs, 4 * count + 2);
        if size > capacity - bytes { break; }
        bytes += size;
        count += 1;
    }
    total[0] = bytes;
    return count;
}

export fn fg143_object_layout(previous_offset: u64, previous_size: u64, size: u64,
        alignment: u64, header: u64, capacity: u64, layout: u64[:]) -> i32 {
    let maximum: u64 = 0xffffffffffffffff;
    if layout.len != 3 || alignment == 0 || size > maximum - (alignment - 1) { return -1; }
    let padded = ((size + alignment - 1) / alignment) * alignment;
    layout[1] = padded;
    if previous_offset > maximum - previous_size { return -2; }
    let cursor = previous_offset + previous_size;
    layout[0] = cursor;
    if cursor > maximum - padded { return -2; }
    if cursor + padded > maximum - header { return -3; }
    layout[2] = cursor + padded + header;
    if layout[2] > capacity { return -4; }
    return 0;
}

export fn fg143_tensor_layout(kind: i64, shape: const i64[:], layout: u64[:]) -> bool {
    if !ab_range(kind) || shape.len < 1 || shape.len > 4 || layout.len != 5 { return false; }
    let quant_block = AB_BLCK[kind];
    if quant_block == 0 || shape[0] < 0 || shape[0] % quant_block != 0 { return false; }
    var elements = shape[0];
    let width = AB_SIZE[kind] as u64;
    let blocks = (shape[0] / quant_block) as u64;
    if width == 0 || blocks > 0xffffffffffffffff / width { return false; }
    var stride = width * blocks;
    layout[0] = width;
    layout[1] = stride;
    for axis in 1 .. 4 {
        let extent = if axis < shape.len { shape[axis] } else { 1 };
        if extent < 0 { return false; }
        if extent > 1 {
            if 9223372036854775807 / extent <= elements { return false; }
            elements *= extent;
        }
        if extent > 0 && stride > 0xffffffffffffffff / (extent as u64) { return false; }
        stride *= extent as u64;
        layout[axis + 1] = stride;
    }
    return true;
}

export fn fg143_graph_arena_layout(nodes: u64, hash_size: u64, base_size: u64, ptr_width: u64,
        bitset_bytes: u64, grads: bool, out: u64[:]) -> bool {
    if out.len != 8 || nodes > 2147483647 || hash_size > 4294967295 || ptr_width != 8 ||
       base_size > 1024 || bitset_bytes > 536870912 { return false; }
    let start = (base_size + 7) & ~7;
    out[0] = start;
    out[1] = out[0] + nodes * ptr_width;
    out[2] = out[1] + nodes * ptr_width;
    out[3] = (out[2] + hash_size * 4 + 7) & ~7;
    let hash_end = out[3] + hash_size * ptr_width;
    out[4] = if grads { hash_end } else { 0 };
    out[5] = if grads { hash_end + hash_size * ptr_width } else { 0 };
    out[6] = if grads { hash_end + hash_size * ptr_width * 2 } else { hash_end };
    out[7] = out[6] + bitset_bytes;
    return true;
}

extern "fg158_dup" fn ctor_duplicate(context: i64, input: i64) -> i64;
extern "fg158_pred" fn ctor_predicate(kind: i64, first: i64, second: i64) -> i64;
extern "fg158_set_op" fn ctor_set_op(tensor: i64, op: i64);
extern "fg158_set_src" fn ctor_set_source(tensor: i64, index: i64, source: i64);
extern "fg158_set_param" fn ctor_set_parameter(tensor: i64, index: i64, value: i64);
extern "fg158_new" fn ctor_tensor(context: i64, kind: i64, n0: i64, n1: i64, n2: i64, n3: i64) -> i64;
extern "fg158_ne" fn ctor_extent(tensor: i64, axis: i64) -> i64;

fn ctor_attach(result: i64, op: i64, first: i64, second: i64) -> i64 {
    ctor_set_op(result, op);
    ctor_set_source(result, 0, first);
    if second != 0 { ctor_set_source(result, 1, second); }
    return result;
}

export fn fg143_construct_elementwise(context: i64, first: i64, second: i64,
        op: i64, predicate: i64, params: const i64[:]) -> i64 {
    if predicate == 4 { assert(ctor_predicate(predicate, second, first) != 0); }
    if predicate == 2 || predicate == 1 { assert(ctor_predicate(predicate, first, 0) != 0); }
    let result = ctor_duplicate(context, first);
    for i in params.len { ctor_set_parameter(result, i, params[i]); }
    return ctor_attach(result, op, first, second);
}

export fn fg143_construct_row_sum(context: i64, input: i64, kind: i64, op: i64) -> i64 {
    let result = ctor_tensor(context, kind, 1, ctor_extent(input, 1),
        ctor_extent(input, 2), ctor_extent(input, 3));
    return ctor_attach(result, op, input, 0);
}

fn ctor_view_storage(context: i64, source: i64, shape: const i64[:], offset: u64) -> i64 {
    return tensor_create(context as u64, ctor_property(source, 0), shape, source as u64, offset) as i64;
}
extern "fg143_ctor_caption" fn ctor_caption(tensor: i64, source: i64, kind: i64);
extern "memcpy" fn tensor_copy_raw(destination: c_ptr<void>, source: c_ptr<const void>, bytes: u64) -> c_ptr<void>;

fn ctor_geometry(tensor: i64, geometry: u64[:], write: bool) {
    let address = (tensor as! u64) + 16;
    if write {
        tensor_copy_raw(unsafe_c_ptr<c_ptr<void>>(address),
                        unsafe_c_address<c_ptr<const void>>(geometry[0]), 64);
    } else {
        tensor_copy_raw(unsafe_c_address<c_ptr<void>>(geometry[0]),
                        unsafe_c_ptr<c_ptr<const void>>(address), 64);
    }
}
extern "fg158_view" fn ctor_native_view(context: i64, input: i64) -> i64;

export fn fg143_construct_reshape(context: i64, input: i64, shape: const i64[:], op: i64) -> i64 {
    assert(ctor_predicate(0, input, 0) != 0);
    var product: i64 = 1;
    for i in shape.len { product *= shape[i]; }
    assert(ctor_predicate(13, input, 0) == product);
    let output = ctor_view_storage(context, input, shape, 0);
    ctor_caption(output, input, 0);
    return ctor_attach(output, op, input, 0);
}

export fn fg143_construct_view(context: i64, input: i64, shape: const i64[:],
        strides: const u64[:], offset: u64, op: i64) -> i64 {
    let output = ctor_view_storage(context, input, shape, offset);
    ctor_caption(output, input, 1);
    ctor_set_parameter(output, 0, (offset & 0xffffffff) as i64);
    ctor_set_parameter(output, 1, (offset >> 32) as i64);
    if shape.len > 1 {
        var geometry: u64[8] = [0; 8];
        ctor_geometry(output, geometry[..], false);
        for axis in 1 .. 4 {
            geometry[4 + axis] = if axis < shape.len { strides[axis - 1] }
                else { geometry[3 + axis] * geometry[axis - 1] };
        }
        ctor_geometry(output, geometry[..], true);
    }
    return ctor_attach(output, op, input, 0);
}

export fn fg143_construct_axis_map(context: i64, input: i64, axes: const i64[:],
        op: i64, transpose: bool) -> i64 {
    assert(axes.len == 4);
    var seen: u64 = 0;
    var source: u64[8] = [0; 8];
    var target: u64[8] = [0; 8];
    ctor_geometry(input, source[..], false);
    for i in 4 {
        let axis = axes[i];
        assert(axis >= 0 && axis < 4);
        let flag = (1 as u64) << (axis as u32);
        assert((seen & flag) == 0);
        seen = seen | flag;
        target[axis] = source[i];
        target[4 + axis] = source[4 + i];
    }
    let output = ctor_native_view(context, input);
    ctor_caption(output, input, if transpose { 3 } else { 2 });
    ctor_geometry(output, target[..], true);
    if !transpose {
        for i in 4 { ctor_set_parameter(output, i, axes[i]); }
    }
    return ctor_attach(output, op, input, 0);
}

fn ctor_property(tensor: i64, field: i64) -> i64 { return graph_prop(tensor as! u64, field); }
extern "fg158_name" fn ctor_copy_name(output: i64, first: i64, second: i64, style: i64);

export fn fg143_construct_product(context: i64, first: i64, second: i64, ids: i64,
        output_type: i64, id_type: i64, op: i64) -> i64 {
    assert(ctor_predicate(10, first, 0) == 0);
    var columns = ctor_extent(second, 1);
    var fourth = ctor_extent(second, 3);
    if ids == 0 {
        assert(ctor_predicate(8, first, second) != 0);
    } else {
        assert(ctor_property(ids, 0) == id_type);
        assert(ctor_extent(first, 3) == 1 && fourth == 1);
        assert(ctor_extent(ids, 2) == 1 && ctor_extent(ids, 3) == 1);
        assert(ctor_extent(ids, 1) == ctor_extent(second, 2));
        assert(ctor_extent(first, 0) == ctor_extent(second, 0));
        assert(columns > 0 && ctor_extent(ids, 0) % columns == 0);
        columns = ctor_extent(ids, 0);
        fourth = 1;
    }
    let output = ctor_tensor(context, output_type, ctor_extent(first, 1),
        columns, ctor_extent(second, 2), fourth);
    if ids != 0 { ctor_set_source(output, 2, ids); }
    return ctor_attach(output, op, first, second);
}

export fn fg143_construct_copy(context: i64, input: i64, target: i64, kind: i64,
        op: i64, mode: i64) -> i64 {
    if mode == 0 {
        assert(ctor_predicate(13, input, 0) == ctor_predicate(13, target, 0));
        let output = ctor_native_view(context, target);
        ctor_copy_name(output, input, target, if ctor_property(target, 3) == 0 { 1 } else { 0 });
        return ctor_attach(output, op, input, target);
    }
    let output = if mode == 1 {
        ctor_tensor(context, kind, ctor_extent(input, 0), ctor_extent(input, 1),
            ctor_extent(input, 2), ctor_extent(input, 3))
    } else { ctor_duplicate(context, input) };
    ctor_copy_name(output, input, 0, if mode == 1 { 1 } else { 2 });
    return ctor_attach(output, op, input, if mode == 1 { output } else { 0 });
}

export fn fg143_construct_flash_attention(context: i64, q: i64, k: i64, v: i64, mask: i64,
        params: const i64[:], output_type: i64, mask_type: i64, op: i64) -> i64 {
    assert(ctor_predicate(8, k, q) != 0);
    assert(ctor_extent(q, 3) == ctor_extent(k, 3) && ctor_extent(q, 3) == ctor_extent(v, 3));
    assert(params.len == 3);
    if mask != 0 {
        assert(ctor_property(mask, 0) == mask_type && ctor_predicate(0, mask, 0) != 0);
        for axis in 2 .. 4 {
            let extent = ctor_extent(mask, axis);
            assert(extent > 0 && ctor_extent(q, axis) % extent == 0);
        }
    }
    assert(mask != 0 || !(from_bits(params[1] as! u32) > 0.0));
    let output = ctor_tensor(context, output_type, ctor_extent(v, 0),
        ctor_extent(q, 2), ctor_extent(q, 1), ctor_extent(q, 3));
    for i in 3 { ctor_set_parameter(output, i, params[i]); }
    ctor_set_source(output, 2, v);
    if mask != 0 { ctor_set_source(output, 3, mask); }
    return ctor_attach(output, op, q, k);
}

export fn fg143_construct_rope(context: i64, input: i64, positions: i64, factors: i64,
        config: const i64[:], scales: const i64[:], sections: const i64[:]) -> i64 {
    let dims = config[0];
    let mode = config[1];
    let multi = (mode & config[3]) != 0;
    assert((mode & 1) == 0);
    assert(ctor_predicate(14, positions, 0) != 0 && ctor_property(positions, 0) == config[4]);
    let per_token = if multi { 4 } else { 1 };
    assert(ctor_extent(input, 2) * per_token == ctor_extent(positions, 0));
    if factors != 0 {
        assert(ctor_property(factors, 0) == config[5] && ctor_extent(factors, 0) >= dims / 2);
    }
    let output = ctor_duplicate(context, input);
    ctor_set_parameter(output, 1, dims);
    ctor_set_parameter(output, 2, mode);
    ctor_set_parameter(output, 4, config[2]);
    for i in 6 { ctor_set_parameter(output, 5 + i, scales[i]); }
    if multi && sections.len == 4 {
        for i in 4 { ctor_set_parameter(output, 11 + i, sections[i]); }
    }
    if factors != 0 { ctor_set_source(output, 2, factors); }
    return ctor_attach(output, config[6], input, positions);
}

export fn fg143_construct_rows(context: i64, a: i64, b: i64, ids: i64,
        types: const i64[:], op: i64) -> i64 {
    if ids == 0 {
        assert(ctor_extent(a, 2) == ctor_extent(b, 1) && ctor_extent(a, 3) == ctor_extent(b, 2));
        assert(ctor_extent(b, 3) == 1 && ctor_property(b, 0) == types[2]);
        let kind = if ctor_property(a, 0) == types[2] { types[2] } else { types[0] };
        let output = ctor_tensor(context, kind, ctor_extent(a, 0), ctor_extent(b, 0),
            ctor_extent(b, 1), ctor_extent(b, 2));
        return ctor_attach(output, op, a, b);
    }
    assert(ctor_extent(a, 0) == ctor_extent(b, 0));
    for axis in 2 .. 4 { assert(ctor_extent(a, axis) == ctor_extent(b, axis)); }
    assert(ctor_extent(b, 1) == ctor_extent(ids, 0) && ctor_extent(ids, 3) == 1);
    for axis in 2 .. 4 {
        let divisor = ctor_extent(ids, axis - 1);
        assert(divisor > 0 && ctor_extent(b, axis) % divisor == 0);
    }
    let values_type = ctor_property(b, 0);
    let indices_type = ctor_property(ids, 0);
    assert(values_type == types[0] || values_type == types[1]);
    assert(indices_type == types[2] || indices_type == types[3]);
    assert(ctor_predicate(1, a, 0) != 0 && ctor_predicate(1, b, 0) != 0);
    let output = ctor_native_view(context, a);
    ctor_set_source(output, 2, a);
    return ctor_attach(output, op, b, ids);
}

export fn fg143_construct_glu(context: i64, a: i64, b: i64, op: i64,
        subtype: i64, swapped: bool, limit: const i64[:]) -> i64 {
    assert(ctor_predicate(7, a, 0) != 0);
    if b != 0 {
        assert(ctor_predicate(7, b, 0) != 0 && ctor_predicate(3, a, b) != 0);
        assert(ctor_property(a, 0) == ctor_property(b, 0));
    }
    let width = if b != 0 { ctor_extent(a, 0) } else { ctor_extent(a, 0) / 2 };
    let output = ctor_tensor(context, ctor_property(a, 0), width, ctor_extent(a, 1),
        ctor_extent(a, 2), ctor_extent(a, 3));
    ctor_set_parameter(output, 0, subtype);
    ctor_set_parameter(output, 1, if swapped { 1 } else { 0 });
    if limit.len != 0 { ctor_set_parameter(output, 3, limit[0]); }
    return ctor_attach(output, op, a, b);
}

export fn fg143_construct_tile(context: i64, a: i64, b: i64, axis: i64,
        shape: const i64[:], op: i64) -> i64 {
    var extents: i64[4] = [0; 4];
    let kind = ctor_property(a, 0);
    if b == 0 {
        assert(shape.len == 4);
        let empty = ctor_predicate(12, a, 0) != 0;
        for i in 4 {
            if !empty { assert(shape[i] % ctor_extent(a, i) == 0); }
            extents[i] = shape[i];
        }
    } else {
        assert(axis >= 0 && axis < 4 && kind == ctor_property(b, 0));
        for i in 4 {
            let extent = ctor_extent(a, i);
            let other = ctor_extent(b, i);
            if i == axis { extents[i] = extent + other; }
            else { assert(extent == other); extents[i] = extent; }
        }
    }
    let output = ctor_tensor(context, kind, extents[0], extents[1], extents[2], extents[3]);
    if b != 0 { ctor_set_parameter(output, 0, axis); }
    return ctor_attach(output, op, a, b);
}

fn ctor_attach_sources(output: i64, op: i64, inputs: const i64[:]) -> i64 {
    ctor_set_op(output, op);
    for i in inputs.len { ctor_set_source(output, i, inputs[i]); }
    return output;
}

export fn fg143_construct_ssm_conv(context: i64, x: i64, kernel: i64,
        output_type: i64, op: i64) -> i64 {
    assert(ctor_extent(x, 3) == 1 && ctor_predicate(11, kernel, 0) != 0);
    let channels = ctor_extent(kernel, 1);
    let tokens = ctor_extent(x, 0) - ctor_extent(kernel, 0) + 1;
    assert(ctor_extent(x, 1) == channels && tokens >= 0);
    let output = ctor_tensor(context, output_type, channels, tokens, ctor_extent(x, 2), 1);
    return ctor_attach(output, op, x, kernel);
}

fn ctor_scan_rows(tensor: i64) -> bool {
    var geometry: u64[8] = [0; 8];
    let kind = ctor_property(tensor, 0);
    if !ab_range(kind) { return false; }
    ctor_geometry(tensor, geometry[..], false);
    return geometry[4] == (AB_SIZE[kind] as u64) && geometry[5] == geometry[0] * geometry[4];
}

export fn fg143_construct_ssm_scan(context: i64, inputs: const i64[:], snapshots: i64,
        output_type: i64, indices_type: i64, op: i64) -> i64 {
    assert(inputs.len == 7 && snapshots >= 1 && snapshots <= 2147483647);
    let state = inputs[0];
    let x = inputs[1];
    let delta = inputs[2];
    let decay = inputs[3];
    let b = inputs[4];
    let c = inputs[5];
    let ids = inputs[6];
    assert(ctor_predicate(0, state, 0) != 0 && ctor_predicate(0, delta, 0) != 0 &&
        ctor_predicate(0, decay, 0) != 0);
    assert(ctor_scan_rows(x) && ctor_scan_rows(b) && ctor_scan_rows(c));
    assert(ctor_predicate(3, b, c) != 0 && ctor_property(ids, 0) == indices_type);
    let state_width = ctor_extent(state, 0);
    let head_width = ctor_extent(x, 0);
    let heads = ctor_extent(x, 1);
    let tokens = ctor_extent(x, 2);
    let sequences = ctor_extent(x, 3);
    assert(ctor_extent(delta, 0) == heads && ctor_extent(delta, 1) == tokens &&
        ctor_extent(delta, 2) == sequences && ctor_extent(delta, 3) == 1);
    assert(ctor_extent(state, 1) == head_width && ctor_extent(state, 2) == heads);
    assert(ctor_extent(b, 0) == state_width && ctor_extent(b, 2) == tokens &&
        ctor_extent(b, 3) == sequences);
    assert(ctor_extent(ids, 0) == sequences && ctor_predicate(14, ids, 0) != 0);
    assert(ctor_extent(decay, 1) == heads && ctor_predicate(11, decay, 0) != 0);
    let decay_width = ctor_extent(decay, 0);
    assert(decay_width == 1 || (decay_width == state_width && snapshots == 1));
    let size = ctor_predicate(13, x, 0) + snapshots * state_width * head_width * heads * sequences;
    let output = ctor_tensor(context, output_type, size, 1, 1, 1);
    ctor_set_parameter(output, 0, snapshots);
    return ctor_attach_sources(output, op, inputs);
}

export fn fg143_construct_gated_delta(context: i64, inputs: const i64[:], snapshots: i64,
        output_type: i64, op: i64) -> i64 {
    assert(inputs.len == 6 && snapshots >= 1);
    for i in inputs.len {
        let contiguous = if i < 3 { 1 } else { 0 };
        assert(ctor_predicate(contiguous, inputs[i], 0) != 0);
        assert(ctor_property(inputs[i], 0) == output_type);
    }
    let v = inputs[2];
    let g = inputs[3];
    let state = inputs[5];
    let width = ctor_extent(v, 0);
    let heads = ctor_extent(v, 1);
    let tokens = ctor_extent(v, 2);
    let sequences = ctor_extent(v, 3);
    assert((ctor_extent(g, 0) == 1 || ctor_extent(g, 0) == width) && ctor_extent(inputs[4], 0) == 1);
    assert(ctor_extent(state, 0) == width && ctor_extent(state, 1) == width &&
        ctor_extent(state, 2) == heads && ctor_extent(state, 3) == sequences);
    let output = ctor_tensor(context, output_type, width * heads,
        tokens * sequences + snapshots * width * sequences, 1, 1);
    ctor_set_parameter(output, 0, snapshots);
    return ctor_attach_sources(output, op, inputs);
}

export fn fg143_construct_indexer(context: i64, inputs: const i64[:], output_type: i64,
        mask_type: i64, op: i64) -> i64 {
    assert(inputs.len == 4);
    let q = inputs[0];
    let k = inputs[1];
    let weights = inputs[2];
    let mask = inputs[3];
    assert(ctor_property(q, 0) == output_type && ctor_property(weights, 0) == output_type &&
        ctor_property(mask, 0) == mask_type);
    let cells = ctor_extent(k, 2);
    let tokens = ctor_extent(q, 2);
    let streams = ctor_extent(q, 3);
    assert(ctor_extent(q, 0) == ctor_extent(k, 0) && ctor_extent(mask, 0) == cells);
    assert(ctor_extent(q, 1) == ctor_extent(weights, 0) && ctor_extent(k, 1) == 1);
    assert(ctor_extent(mask, 1) == tokens && ctor_extent(weights, 1) == tokens);
    assert(ctor_extent(weights, 2) == 1 && ctor_extent(mask, 2) == 1);
    assert(ctor_extent(k, 3) == streams && ctor_extent(weights, 3) == streams);
    let mask_streams = ctor_extent(mask, 3);
    assert(mask_streams > 0 && streams % mask_streams == 0);
    let output = ctor_tensor(context, output_type, cells, tokens, 1, streams);
    return ctor_attach_sources(output, op, inputs);
}

export fn fg143_construct_softmax(context: i64, input: i64, mask: i64, params: const i64[:],
        f32_type: i64, f16_type: i64, op: i64) -> i64 {
    assert(ctor_predicate(0, input, 0) != 0 && params.len == 2);
    if mask != 0 {
        let kind = ctor_property(mask, 0);
        assert((kind == f32_type || kind == f16_type) && ctor_predicate(0, mask, 0) != 0);
        assert(ctor_extent(mask, 0) == ctor_extent(input, 0));
        assert(ctor_extent(mask, 1) >= ctor_extent(input, 1));
        for axis in 2 .. 4 {
            let divisor = ctor_extent(mask, axis);
            assert(divisor > 0 && ctor_extent(input, axis) % divisor == 0);
        }
    }
    assert(mask != 0 || !(from_bits(params[1] as! u32) > 0.0));
    return fg143_construct_elementwise(context, input, mask, op, 0, params);
}

export fn fg143_construct_fill(context: i64, input: i64, value: i64,
        f32_type: i64, f16_type: i64, op: i64) -> i64 {
    let kind = ctor_property(input, 0);
    assert((kind == f32_type || kind == f16_type) && ctor_predicate(0, input, 0) != 0);
    var params: i64[1] = [value];
    return fg143_construct_elementwise(context, input, 0, op, 0, params[..]);
}

export fn fg143_construct_sort(context: i64, input: i64, limit: i64, order: i64,
        mode: i64, kind: i64, op: i64, view_op: i64) -> i64 {
    let width = ctor_extent(input, 0);
    assert(mode == 0 || (limit >= 0 && limit <= width));
    assert(mode == 2 || width <= 2147483647);
    let output_width = if mode == 2 { limit } else { width };
    let output = ctor_tensor(context, kind, output_width, ctor_extent(input, 1),
        ctor_extent(input, 2), ctor_extent(input, 3));
    if mode != 2 { ctor_set_parameter(output, 0, order); }
    let sorted = ctor_attach(output, op, input, 0);
    if mode != 1 { return sorted; }
    var shape: i64[4] = [limit, ctor_extent(input, 1), ctor_extent(input, 2), ctor_extent(input, 3)];
    var geometry: u64[8] = [0; 8];
    ctor_geometry(sorted, geometry[..], false);
    return fg143_construct_view(context, sorted, shape[..], geometry[5..8], 0, view_op);
}

export fn fg143_geometry_product(shape: const i64[:], first: i64) -> i64 {
    var count: i64 = 1;
    for axis in first .. shape.len { count *= shape[axis]; }
    return count;
}

export fn fg143_geometry_unit_from(shape: const i64[:], first: i64) -> bool {
    for axis in first .. shape.len { if shape[axis] != 1 { return false; } }
    return true;
}

export fn fg143_geometry_rank(shape: const i64[:]) -> i64 {
    var rank = shape.len;
    while rank > 1 {
        if shape[rank - 1] > 1 { break; }
        rank -= 1;
    }
    return rank;
}

export fn fg143_geometry_empty(shape: const i64[:]) -> bool {
    for axis in shape.len { if shape[axis] == 0 { return true; } }
    return false;
}

export fn fg143_geometry_equal(a: const i64[:], b: const i64[:]) -> bool {
    if a.len != b.len { return false; }
    for axis in a.len { if a[axis] != b[axis] { return false; } }
    return true;
}

export fn fg143_geometry_repeat(a: const i64[:], b: const i64[:]) -> bool {
    if a.len != b.len { return false; }
    if fg143_geometry_empty(a) { return fg143_geometry_empty(b); }
    for axis in a.len { if b[axis] % a[axis] != 0 { return false; } }
    return true;
}

export fn fg143_geometry_contiguous(kind: i64, layout: const i64[:], loose: i64) -> bool {
    if !ab_range(kind) || layout.len != 8 { return false; }
    let quant_block = AB_BLCK[kind];
    let unit = AB_SIZE[kind];
    if quant_block <= 0 || (layout[0] != quant_block && layout[4] != unit) { return false; }
    var expected = unit * (layout[0] / quant_block);
    for axis in 1 .. 4 {
        if axis > loose {
            if layout[axis] != 1 && layout[4 + axis] != expected { return false; }
            expected *= layout[axis];
        } else {
            expected = layout[axis] * layout[4 + axis];
        }
    }
    return true;
}

export fn fg143_geometry_rows(kind: i64, first_extent: i64, first_stride: i64) -> bool {
    return ab_range(kind) && (first_extent == AB_BLCK[kind] || first_stride == AB_SIZE[kind]);
}

const GRAPH_HASH_PRIMES: u64[32] = [
    2, 3, 5, 11, 17, 37, 67, 131, 257, 521, 1031,
    2053, 4099, 8209, 16411, 32771, 65537, 131101,
    262147, 524309, 1048583, 2097169, 4194319, 8388617,
    16777259, 33554467, 67108879, 134217757, 268435459,
    536870923, 1073741827, 2147483659
];

export fn fg143_graph_hash_size(minimum: u64) -> u64 {
    var low: i64 = 0;
    var high: i64 = GRAPH_HASH_PRIMES.len;
    while low < high {
        let mid = (low + high) / 2;
        if GRAPH_HASH_PRIMES[mid] < minimum { low = mid + 1; } else { high = mid; }
    }
    return if low < GRAPH_HASH_PRIMES.len { GRAPH_HASH_PRIMES[low] } else { minimum | 1 };
}


struct Tensor {
    ne: i64[4], nb: i64[4], kind: i64, bytes: i64
}
struct Node {
    x: Tensor, y: Tensor, z: Tensor, d: Tensor, op: i64, ith: i64, nth: i64, ip: i64[16], fp: f32[8]
}
extern "fg143_bits" fn bits(x: f32) -> u32;
extern "fg143_from_bits" fn from_bits(x: u32) -> f32;
fn half(x: u16) -> f32 { return half_codec :: expand_half(x); }
fn to_half(x: f32) -> u16 { return half_codec :: narrow_half(x); }
extern "fabsf" fn abs32(x: f32) -> f32;
extern "expf" fn exp32(x: f32) -> f32;
extern "expm1f" fn expm132(x: f32) -> f32;
extern "logf" fn log32(x: f32) -> f32;
extern "tanhf" fn tanh32(x: f32) -> f32;
extern "sqrtf" fn sqrt32(x: f32) -> f32;
extern "sinf" fn sin32(x: f32) -> f32;
extern "cosf" fn cos32(x: f32) -> f32;
extern "floorf" fn floor32(x: f32) -> f32;
extern "ceilf" fn ceil32(x: f32) -> f32;
extern "roundf" fn round32(x: f32) -> f32;
extern "truncf" fn trunc32(x: f32) -> f32;
extern "fminf" fn min32(a: f32, b: f32) -> f32;
extern "fmaxf" fn max32(a: f32, b: f32) -> f32;
extern "fmaf" fn fma32(a: f32, b: f32, c: f32) -> f32;
extern "erff" fn erf32(x: f32) -> f32;
extern "fg143_witness" fn witness(op: i64, values: i64);

fn low(a: i64, b: i64) -> i64 {
    return if a < b {
        a
    } else {
        b
    }
    ;
}

fn high(a: i64, b: i64) -> i64 {
    return if a > b {
        a
    } else {
        b
    }
    ;
}

fn add_size(a: i64, b: i64) -> i64 {
    assert(a >= 0 && b >= 0 && a <= 0x7fffffffffffffff - b);
    return a + b;
}

fn mul_size(a: i64, b: i64) -> i64 {
    assert(a >= 0 && b >= 0);
    assert(a == 0 || b <= 0x7fffffffffffffff / a);
    return a * b;
}

fn scalar_bytes(kind: i64) -> i64 {
    if kind == 0 || kind == 26 {
        return 4;
    }

    if kind == 1 || kind == 30 || kind == 25 {
        return 2;
    }

    if kind == 24 {
        return 1;
    }

    if kind == 27 || kind == 28 {
        return 8;
    }
    return 0;
}

fn scalar_type(t: const Tensor &) -> bool {
    return scalar_bytes(t.kind) > 0;
}

fn elements(t: const Tensor &) -> i64 {
    var n: i64 = 1;
    for axis in 4 {
        n = mul_size(n, t.ne[axis]);
    }
    return n;
}

fn rows(t: const Tensor &) -> i64 {
    return mul_size(mul_size(t.ne[1], t.ne[2]), t.ne[3]);
}

fn validate(t: const Tensor &, available: i64) {
    assert(t.bytes >= 0 && t.bytes <= available);
    let bytes = scalar_bytes(t.kind);
    assert(bytes > 0);
    var last: i64 = bytes;
    var empty: bool = false;
    for axis in 4 {
        assert(t.ne[axis] >= 0 && t.nb[axis] >= 0);

        if t.ne[axis] == 0 {
            empty = true;
        } else {
            last = add_size(last, mul_size(t.ne[axis] - 1, t.nb[axis]));
        }
    }
    assert(empty || last <= t.bytes);
}

fn offset(t: const Tensor &, index: i64) -> i64 {
    assert(index >= 0 && index < elements(t));
    var n = index;
    var result: i64 = 0;
    for axis in 4 {
        assert(t.ne[axis] > 0);
        result = add_size(result, mul_size(n % t.ne[axis], t.nb[axis]));
        n /= t.ne[axis];
    }
    return result;
}

fn broadcast(t: const Tensor &, d: const Tensor &, index: i64) -> i64 {
    var n = index;
    var result: i64 = 0;
    for axis in 4 {
        assert(t.ne[axis] > 0 && d.ne[axis] > 0 && d.ne[axis] % t.ne[axis] == 0);
        let c = n % d.ne[axis];
        n /= d.ne[axis];
        result = add_size(result, mul_size(c % t.ne[axis], t.nb[axis]));
    }
    return result;
}

fn read16(a: const u8[:], i: i64) -> u16 {
    assert(i >= 0 && i <= a.len - 2);
    return (a[i] as u16) | ( (a[i + 1] as u16) << 8);
}

fn read32(a: const u8[:], i: i64) -> u32 {
    assert(i >= 0 && i <= a.len - 4);
    return (a[i] as u32) | ( (a[i + 1] as u32) << 8) | ( (a[i + 2] as u32) << 16) | ( (a[i + 3] as u32) << 24);
}

fn write16(a: u8[:], i: i64, v: u16) {
    assert(i >= 0 && i <= a.len - 2);
    a[i] = v as !u8;
    a[i + 1] = (v >> 8) as !u8;
}

fn write32(a: u8[:], i: i64, v: u32) {
    assert(i >= 0 && i <= a.len - 4);
    a[i] = v as !u8;
    a[i + 1] = (v >> 8) as !u8;
    a[i + 2] = (v >> 16) as !u8;
    a[i + 3] = (v >> 24) as !u8;
}

fn load_at(t: const Tensor &, a: const u8[:], i: i64) -> f32 {
    if t.kind == 0 {
        return from_bits(read32(a, i));
    }

    if t.kind == 1 {
        return half(read16(a, i));
    }

    if t.kind == 30 {
        return from_bits( (read16(a, i) as u32) << 16);
    }

    if t.kind == 26 {
        return (read32(a, i) as !i32) as f32;
    }

    if t.kind == 25 {
        return (read16(a, i) as !i16) as f32;
    }
    assert(t.kind == 24 && i >= 0 && i < a.len);
    return (a[i] as !i8) as f32;
}

fn trunc_i32(x: f32) -> i32 {
    let u = bits(x);
    let magnitude = u & 0x7fffffff;
    let exponent = ( (magnitude >> 23) & 255) as i64;

    if exponent < 127 {
        return 0;
    }
    assert(exponent <= 158);
    var v: u32 = (magnitude & 0x7fffff) | 0x800000;

    if exponent >= 150 {
        v = v << ( (exponent - 150) as u32);
    } else {
        v = v >> ( (150 - exponent) as u32);
    }

    if u >> 31 != 0 {
        assert(v <= 0x80000000);
        return ( (0 as u32) - v) as !i32;
    }
    assert(v <= 0x7fffffff);
    return v as i32;
}

fn store_at(t: const Tensor &, a: u8[:], i: i64, x: f32) {
    if t.kind == 0 {
        write32(a, i, bits(x));
    } else

    if t.kind == 1 {
        write16(a, i, to_half(x));
    } else

    if t.kind == 30 {
        let u = bits(x);
        let v = if (u & 0x7fffffff) > 0x7f800000 {
            ( (u >> 16) | 64) as !u16
        } else {
            ( (u + (32767 + ( (u >> 16) & 1))) >> 16) as !u16
        }
        ;
        write16(a, i, v);
    } else

    if t.kind == 26 {
        write32(a, i, trunc_i32(x) as !u32);
    } else

    if t.kind == 25 {
        write16(a, i, trunc_i32(x) as !u16);
    } else {
        assert(t.kind == 24 && i >= 0 && i < a.len);
        a[i] = trunc_i32(x) as !u8;
    }
}

fn load(t: const Tensor &, a: const u8[:], index: i64) -> f32 {
    return load_at(t, a, offset(t, index));
}

fn store(t: const Tensor &, a: u8[:], index: i64, value: f32) {
    store_at(t, a, offset(t, index), value);
}

fn thread_first(p: const Node &) -> i64 {
    assert(p.nth > 0 && p.ith >= 0 && p.ith < p.nth);
    return p.ith;
}

fn same_shape(a: const Tensor &, b: const Tensor &) -> bool {
    for i in 4 {
        if a.ne[i] != b.ne[i] {
            return false;
        }
    }
    return true;
}


// Host span, byte count, device offset, host-current flag, last consumer.
const ACT_WORDS: i64 = 5;

extern "fg_cuda_activation_read" fn activation_read(data: const u8[:]) -> i64;
extern "fg_cuda_activation_read_address" fn activation_read_address(address: u64, bytes: i64) -> i64;
extern "fg_cuda_activation_read_f32" fn activation_read_f32(data: const f32[:]) -> i64;

fn activation_spans_overlap(a: u64, na: u64, b: u64, nb: u64) -> bool {
    if na == 0 || nb == 0 { return false; }
    return if a < b { b - a < na } else { a - b < nb };
}

export fn fg143_activation_lookup(ranges: const i64[:], host: u64, bytes: i64) -> i64 {
    if bytes <= 0 { return -1; }
    for i in ranges.len / ACT_WORDS {
        let at = i * ACT_WORDS;
        let base = ranges[at] as! u64;
        let size = ranges[at + 1];
        if size < bytes || host < base { continue; }
        let relative = host - base;
        if relative <= (size - bytes) as u64 {
            return ranges[at + 2] + (relative as i64);
        }
    }
    return -1;
}

fn activation_host_overlap(ranges: const i64[:], host: u64, bytes: i64, first: i64) -> i64 {
    if bytes <= 0 || first < 0 { return -1; }
    for i in first .. ranges.len / ACT_WORDS {
        let at = i * ACT_WORDS;
        let size = ranges[at + 1];
        if size <= 0 { continue; }
        let base = ranges[at] as! u64;
        if activation_spans_overlap(host, bytes as u64, base, size as u64) { return i; }
    }
    return -1;
}

export fn fg143_activation_overlap(ranges: const i64[:], host: u64, bytes: i64, first: i64) -> i64 {
    return activation_host_overlap(ranges, host, bytes, first);
}

// A fully covered record will be replaced by a disjoint full-output operation.
// Partially covered records must still be published to preserve their tails.
export fn fg143_activation_discard_overwritten(ranges: i64[:], host: u64, bytes: i64) -> u64 {
    if bytes <= 0 { return 0; }
    var skipped: u64 = 0;
    for i in ranges.len / ACT_WORDS {
        let at = i * ACT_WORDS;
        let base = ranges[at] as! u64;
        let size = ranges[at + 1];
        if size <= 0 || size > bytes || base < host || base - host > (bytes - size) as u64 { continue; }
        if ranges[at + 3] == 0 { skipped += size as u64; }
        ranges[at + 1] = 0;
    }
    return skipped;
}

export fn fg143_activation_expire(ranges: i64[:], step: i64) -> u64 {
    var skipped: u64 = 0;
    for i in ranges.len / ACT_WORDS {
        let at = i * ACT_WORDS;
        let size = ranges[at + 1];
        let deadline = ranges[at + 4];
        if size <= 0 || deadline < 0 || deadline >= step { continue; }
        if ranges[at + 3] == 0 { skipped += size as u64; }
        ranges[at + 1] = 0;
    }
    return skipped;
}

// Find the first aligned gap between live allocations. -2 asks the adapter
// to publish live records before reusing the arena.
export fn fg143_activation_slot(ranges: const i64[:], capacity: i64, bytes: i64) -> i64 {
    if bytes <= 0 || capacity <= 0 || bytes > capacity { return -1; }
    var start: i64 = 0;
    while start <= capacity - bytes {
        var next = start;
        for i in ranges.len / ACT_WORDS {
            let at = i * ACT_WORDS;
            let size = ranges[at + 1];
            if size <= 0 { continue; }
            let offset = ranges[at + 2];
            if offset < 0 || offset > capacity || size > capacity - offset { return -1; }
            if offset < start + bytes && offset + size > start { next = high(next, offset + size); }
        }
        if next == start { return start; }
        let padding = (128 - next % 128) % 128;
        if padding > capacity - next { return -2; }
        start = next + padding;
    }
    return -2;
}

export fn fg143_activation_record_slot(ranges: const i64[:]) -> i64 {
    let count = ranges.len / ACT_WORDS;
    for i in count {
        if ranges[i * ACT_WORDS + 1] == 0 { return i; }
    }
    return count;
}

fn activation_device_overlap(ranges: const i64[:], offset: i64, bytes: i64, first: i64) -> i64 {
    if offset < 0 || bytes <= 0 || first < 0 { return -1; }
    for i in first .. ranges.len / ACT_WORDS {
        let at = i * ACT_WORDS;
        let size = ranges[at + 1];
        let base = ranges[at + 2];
        if size > 0 && base >= 0 && activation_spans_overlap(offset as u64, bytes as u64, base as u64, size as u64) {
            return i;
        }
    }
    return -1;
}

export fn fg143_activation_device_overlap(ranges: const i64[:], offset: i64, bytes: i64, first: i64) -> i64 {
    return activation_device_overlap(ranges, offset, bytes, first);
}

fn activation_prepare_records(ranges: i64[:], host: u64, bytes: i64,
        step: i64, overwrite: bool, counters: u64[:]) -> bool {
    var overlap = false;
    for i in ranges.len / ACT_WORDS {
        let at = i * ACT_WORDS;
        let size = ranges[at + 1];
        if size <= 0 { continue; }
        let deadline = ranges[at + 4];
        let base = ranges[at] as! u64;
        if step >= 0 && deadline >= 0 && deadline < step {
            if ranges[at + 3] == 0 { counters[0] += size as u64; }
            ranges[at + 1] = 0;
        } else if overwrite && size <= bytes && base >= host && base - host <= (bytes - size) as u64 {
            if ranges[at + 3] == 0 { counters[1] += size as u64; }
            ranges[at + 1] = 0;
        } else if bytes > 0 && activation_spans_overlap(host, bytes as u64, base, size as u64) {
            overlap = true;
        }
    }
    return overlap;
}

export fn fg143_activation_prepare_records(ranges: i64[:], large: i64[:], host: u64,
        bytes: i64, step: i64, overwrite: bool, counters: u64[:]) -> bool {
    let ordinary_overlap = activation_prepare_records(ranges, host, bytes, step, overwrite, counters);
    let large_overlap = activation_prepare_records(large, host, bytes, step, overwrite, counters);
    return ordinary_overlap || large_overlap;
}

struct ActivationPlacement {
    offset: i64, evict: bool, flush: bool
}

fn activation_place(ranges: const i64[:], capacity: i64, bytes: i64, used: i64,
        limit: i64, reuse: bool, selective: bool) -> ActivationPlacement {
    let count = ranges.len / ACT_WORDS;
    var offset: i64 = -2;
    if reuse {
        offset = fg143_activation_slot(ranges, capacity, bytes);
    } else {
        let aligned = (used + 127) & ~127;
        if aligned <= capacity - bytes { offset = aligned; }
    }
    let evict = offset == -2 && reuse && selective;
    if evict {
        offset = fg143_activation_eviction_slot(ranges, capacity, bytes);
        if offset < 0 { return ActivationPlacement {-1, false, false}; }
    }
    let full = count == limit && (!reuse || fg143_activation_record_slot(ranges) == count);
    let freed_record = evict && activation_device_overlap(ranges, offset, bytes, 0) >= 0;
    return ActivationPlacement {offset, evict, offset == -2 || (full && !freed_record)};
}

export fn fg143_activation_output_plan(ranges: const i64[:], capacity: i64, bytes: i64,
        used: i64, limit: i64, reuse: bool, selective: bool, plan: i64[:]) {
    let placement = activation_place(ranges, capacity, bytes, used, limit, reuse, selective);
    plan[0] = placement.offset;
    plan[1] = if placement.evict { 1 } else { 0 };
    plan[2] = if placement.flush { 1 } else { 0 };
}

export fn fg143_activation_publish_record(ranges: i64[:], count: i64, host: u64,
        bytes: i64, offset: i64, deadline: i64, reuse: bool) -> i64 {
    let slot = if reuse { fg143_activation_record_slot(ranges[0..count * ACT_WORDS]) } else { count };
    if slot >= ranges.len / ACT_WORDS { return -1; }
    let at = slot * ACT_WORDS;
    ranges[at] = host as! i64;
    ranges[at + 1] = bytes;
    ranges[at + 2] = offset;
    ranges[at + 3] = 0;
    ranges[at + 4] = deadline;
    return slot;
}

extern "fg_cuda_activation_copy_to_host" fn activation_copy_to_host(host: u64, source: u64,
    bytes: i64) -> i32;

fn activation_download_at(ranges: i64[:], at: i64, base: u64) -> i32 {
    if ranges[at + 1] == 0 || ranges[at + 3] != 0 { return 0; }
    let error = activation_copy_to_host(ranges[at] as! u64, base + (ranges[at + 2] as! u64),
                                        ranges[at + 1]);
    if error == 0 { ranges[at + 3] = 1; }
    return error;
}

export fn fg143_activation_download_all(ranges: i64[:], base: u64) -> i32 {
    for i in ranges.len / ACT_WORDS {
        let error = activation_download_at(ranges, i * ACT_WORDS, base);
        if error != 0 { return error; }
    }
    return 0;
}

export fn fg143_activation_sync(ranges: i64[:], base: u64, address: u64, bytes: i64,
        device: bool, invalidate: bool) -> i32 {
    var start: i64 = 0;
    while start < ranges.len / ACT_WORDS {
        let hit = if device {
            activation_device_overlap(ranges, address as! i64, bytes, start)
        } else { activation_host_overlap(ranges, address, bytes, start) };
        if hit < 0 { break; }
        let at = hit * ACT_WORDS;
        let error = activation_download_at(ranges, at, base);
        if error != 0 { return error; }
        if invalidate { ranges[at + 1] = 0; }
        start = hit + 1;
    }
    return 0;
}

export fn fg143_activation_pending(ranges: const i64[:], host: u64, bytes: i64) -> bool {
    var start: i64 = 0;
    while start < ranges.len / ACT_WORDS {
        let hit = activation_host_overlap(ranges, host, bytes, start);
        if hit < 0 { break; }
        if ranges[hit * ACT_WORDS + 3] == 0 { return true; }
        start = hit + 1;
    }
    return false;
}

// Relocate an entire stale input on its last use if allocation would evict it.
export fn fg143_activation_last_input(ranges: const i64[:], host: u64, bytes: i64,
                                      step: i64, window: i64, output_bytes: i64) -> i64 {
    if step < 0 || bytes <= 0 || window < 0 || output_bytes <= 0 { return -1; }
    let found = activation_host_overlap(ranges, host, bytes, 0);
    if found < 0 { return -1; }
    let at = found * ACT_WORDS;
    let offset = ranges[at + 2];
    if offset < 0 || ranges[at] as! u64 != host || ranges[at + 1] != bytes ||
       ranges[at + 3] != 0 || ranges[at + 4] != step { return -1; }
    if !activation_spans_overlap(offset as u64, bytes as u64, window as u64, output_bytes as u64) {
        return -1;
    }
    return found;
}

// An eviction window costs one potential upload per victim plus a download
// when its host copy is stale. Only zero and aligned ends of allocations can
// start a cheaper window: moving right otherwise cannot remove a victim.
export fn fg143_activation_eviction_slot(ranges: const i64[:], capacity: i64, bytes: i64) -> i64 {
    if bytes <= 0 || capacity <= 0 || bytes > capacity || capacity > 0x3fffffffffffffff { return -1; }
    let count = ranges.len / ACT_WORDS;
    var best: i64 = -1;
    var best_cost = 2 * capacity + 1;
    for candidate in count + 1 {
        var offset: i64 = 0;
        if candidate != 0 {
            let at = (candidate - 1) * ACT_WORDS;
            let size = ranges[at + 1];
            let base = ranges[at + 2];
            if size <= 0 { continue; }
            if base < 0 || base > capacity || size > capacity - base { return -1; }
            let end = base + size;
            offset = end + (128 - end % 128) % 128;
            if offset > capacity - bytes { continue; }
        }
        var cost: i64 = 0;
        var first: i64 = 0;
        while cost < best_cost {
            let found = activation_device_overlap(ranges, offset, bytes, first);
            if found < 0 { break; }
            let at = found * ACT_WORDS;
            let size = ranges[at + 1];
            cost += if ranges[at + 3] == 0 { 2 * size } else { size };
            first = found + 1;
        }
        if cost < best_cost {
            best = offset;
            best_cost = cost;
        }
    }
    return best;
}

extern "fg_cuda_vector" fn cuda_vector(config: const i64[:], x: const u8[:], y: const u8[:],
    dst: u8[:], param0: f32, param1: f32) -> i64;
extern "fg_cuda_wide_vector_ready" fn cv_wide_ready(x: u64, y: u64, output: u64, bytes: i64) -> bool;

fn cv_operation(p: const Node &) -> i64 {
    if p.op == 2 || p.op == 4 { return CV_ADD; }
    if p.op == 6 { return CV_SUB; }
    if p.op == 7 { return CV_MUL; }
    if p.op == 8 { return CV_DIV; }
    if p.op == 32 { return CV_SCALE; }
    if p.op == 20 && p.ip[13] == 2143 && p.x.ne[0] == 128 && p.x.ne[1] == 1 &&
       p.x.ne[2] == 16 && (p.x.ne[3] == 1 || p.x.ne[3] >= 16) && p.d.ne[0] == 128 &&
       p.d.ne[1] == 2 && p.d.ne[2] == 16 && p.d.ne[3] == p.x.ne[3] {
        return CV_SCALE;
    }
    if p.op == 50 { return CV_CLAMP; }
    if p.op == 25 { return CV_RMS; }
    if p.op == 28 { return CV_L2; }
    if p.op == 100 && p.ip[0] == 2 && p.ip[12] == 0 { return CV_SWIGLU; }
    if p.op == 100 && p.ip[0] == 6 && p.ip[13] == 1 && p.ip[2] == 1 &&
       p.fp[1] > 0.0 && p.fp[1] <= 3.4028234663852886e38 {
        return if p.d.ne[2] > 1 || p.d.ne[1] >= 16 { 17 } else { 16 };
    }
    if p.op == 91 {
        if p.ip[0] == 7 { return CV_SIGMOID; }
        if p.ip[0] == 10 { return CV_SILU; }
        if p.ip[0] == 15 { return CV_SOFTPLUS; }
    }
    return 0;
}

fn cv_layout(t: const Tensor &, available: i64) -> bool {
    if t.kind != 0 || t.bytes <= 0 || t.bytes > available { return false; }
    for axis in 4 {
        if t.ne[axis] <= 0 || t.nb[axis] < 0 || t.nb[axis] % 4 != 0 { return false; }
    }
    validate(t, available);
    return true;
}

fn cv_multiply_windows(p: const Node &, x: const u8[:], y: const u8[:], dst: u8[:], plan: i64[:]) -> i64 {
    let capacity = high(1, (16 * 1024 * 1024) / p.d.nb[2]);
    plan[23] = 4;
    var offset: i64 = 0;
    while offset < p.d.ne[2] {
        let count = low(capacity, p.d.ne[2] - offset);
        let first_x = offset * p.x.nb[2];
        let first_y = offset * p.y.nb[2];
        let first_d = offset * p.d.nb[2];
        plan[1] = count * (p.d.nb[2] / 4);
        plan[4] = count;
        plan[8] = count;
        plan[16] = count;
        plan[24] = 1 + (plan[1] - 1) / 128;
        let result = cuda_vector(plan, x[first_x .. first_x + count * p.x.nb[2]],
            y[first_y .. first_y + count * p.y.nb[2]], dst[first_d .. first_d + count * p.d.nb[2]],
            p.fp[0], p.fp[1]);
        if result <= 0 { return if offset == 0 { result } else { -1 }; }
        offset += count;
    }
    return 1;
}

fn cv_try(p: const Node &, x: const u8[:], y: const u8[:], dst: u8[:], addresses: const i64[3] &) -> i64 {
    let op = cv_operation(p);
    if op == 0 || p.nth != 1 || !cv_layout(p.d, dst.len) || !dense_scalar(p.d, 4) ||
       !cv_layout(p.x, x.len) {
        return 0;
    }
    let norm = op == CV_RMS || op == CV_L2;
    let packed = op == CV_SWIGLU && p.ip[2] == 0;
    let clamped = op == 16 || op == 17;
    let pair = clamped || op <= CV_DIV || (op == CV_SWIGLU && !packed) || (op == CV_RMS && p.ip[14] != 0);
    if pair && !cv_layout(p.y, y.len) { return 0; }
    if norm && !same_shape(p.x, p.d) { return 0; }
    if (clamped || (op == CV_SWIGLU && !packed)) && (!same_shape(p.x, p.d) || !same_shape(p.y, p.d)) { return 0; }
    for axis in 4 {
        if packed {
            let extent = if axis == 0 { mul_size(p.d.ne[axis], 2) } else { p.d.ne[axis] };
            if p.x.ne[axis] != extent { return 0; }
        } else if p.d.ne[axis] % p.x.ne[axis] != 0 { return 0; }
        if pair && p.d.ne[axis] % p.y.ne[axis] != 0 { return 0; }
    }
    let count = elements(p.d);
    let groups = if norm { rows(p.d) } else { 1 + (count - 1) / 128 };
    if groups > 2147483647 { return 0; }
    var config: i64[26] = [0; 26];
    config[0] = op;
    config[1] = count;
    for axis in 4 {
        config[2 + axis] = p.d.ne[axis];
        config[6 + axis] = p.x.ne[axis];
        config[10 + axis] = p.x.nb[axis];
        config[14 + axis] = p.y.ne[axis];
        config[18 + axis] = p.y.nb[axis];
    }
    config[22] = if !packed { 0 } else if p.ip[1] != 0 { 2 } else { 1 };
    let pack_x = p.x.nb[0] == 4 && p.x.bytes > mul_size(elements(p.x), 4);
    let pack_y = pair && p.y.nb[0] == 4 && p.y.bytes > mul_size(elements(p.y), 4);
    let flat = !norm && !packed && same_shape(p.x, p.d) && dense_scalar(p.x, 4) &&
        (!pair || (same_shape(p.y, p.d) && dense_scalar(p.y, 4)));
    config[23] = (if pack_x { 1 } else { 0 }) | (if pack_y { 2 } else { 0 }) | (if flat { CV_FLAT_PLAN } else { 0 });
    config[24] = groups;
    config[25] = if norm { 32 } else { 128 };
    let y_bytes = if pair { p.y.bytes } else { 0 };
    // The full MoE product can exceed the available staging memory. Its
    // token planes are independent and can be published directly to RAM.
    if op == CV_MUL && p.d.bytes > 32 * 1024 * 1024 &&
       p.d.ne[2] > 1 && p.d.ne[3] == 1 && same_shape(p.x, p.d) &&
       p.y.ne[2] == p.d.ne[2] && p.y.ne[3] == 1 &&
       dense_scalar(p.x, 4) && dense_scalar(p.y, 4) &&
       addresses[0] != 0 && addresses[1] != 0 && addresses[2] != 0 &&
       blit_disjoint(addresses[0] as! u64, p.x.bytes, addresses[2] as! u64, p.d.bytes) &&
       blit_disjoint(addresses[1] as! u64, p.y.bytes, addresses[2] as! u64, p.d.bytes) &&
       !cv_wide_ready(addresses[0] as! u64, addresses[1] as! u64, addresses[2] as! u64, p.d.bytes) {
        return cv_multiply_windows(p, x, y, dst, config[..]);
    }
    let scale = if p.op == 20 { 1.0 } else { p.fp[0] };
    let bias = if p.op == 20 { 0.0 } else { p.fp[1] };
    return cuda_vector(config, x[0 .. p.x.bytes], y[0 .. y_bytes], dst[0 .. p.d.bytes], scale, bias);
}

extern "fg_cuda_position" fn cuda_position(config: const i64[:], x: const u8[:], y: const u8[:],
    z: const u8[:], dst: u8[:]) -> i64;

fn rope_cuda(p: const Node &, x: const u8[:], positions: const u8[:], factors: const u8[:], dst: u8[:]) -> i64 {
    let dims = p.ip[0];
    let mode = p.ip[1];
    let offs = p.ip[3];
    let width = p.x.ne[0];
    let size = scalar_bytes(p.x.kind);
    if p.nth != 1 || (p.x.kind != 0 && p.x.kind != 1) || p.x.kind != p.d.kind ||
       !same_shape(p.x, p.d) || !dense_scalar(p.d, size) || p.x.nb[0] != size ||
       (p.x.nb[1] | p.x.nb[2] | p.x.nb[3]) % size != 0 ||
       (mode != 0 && mode != 2 && mode != 8 && mode != 24 && mode != 40) ||
       width <= 0 || width % 2 != 0 || dims <= 0 || dims % 2 != 0 ||
       offs < 0 || offs % 2 != 0 || offs > width || dims > width - offs ||
       (mode == 24 && (offs != 0 || dims != width / 2)) ||
       !(p.fp[0] >= 1.0) || !(p.fp[1] > 0.0 && p.fp[1] <= 1.0) ||
       !(p.fp[2] >= 0.0 && p.fp[2] <= 1.0) {
        return 0;
    }
    let multi = (mode & 8) != 0;
    if multi {
        var total: i64 = 0;
        for i in 4 {
            let segment = p.ip[4 + i];
            if segment < 0 || segment > width - total { return 0; }
            total += segment;
        }
        if total <= 0 { return 0; }
    }
    let tokens = p.x.ne[2];
    let positions_needed = mul_size(tokens, if multi { 4 } else { 1 });
    if tokens <= 0 || p.y.kind != 26 || !dense_scalar(p.y, 4) ||
       p.y.bytes < mul_size(4, positions_needed) { return 0; }
    validate(p.x, x.len);
    validate(p.d, dst.len);
    validate(p.y, positions.len);
    let has_factors = p.ip[8] != 0;
    if has_factors {
        if !cv_layout(p.z, factors.len) || !dense_scalar(p.z, 4) ||
           elements(p.z) < width / 2 { return 0; }
        if activation_read(factors) != 0 { return -1; }
        for i in width / 2 {
            if !(load_at(p.z, factors, 4 * i) >= 0.001) { return 0; }
        }
    }
    let count = mul_size(rows(p.x), width / 2);
    if count <= 0 { return 0; }
    let blocks = 1 + (count - 1) / 128;
    let phases = 1 + (tokens - 1) / 128;
    if blocks > 2147483647 || phases > 2147483647 { return 0; }
    let step = pow32(p.fp[0], -2.0 / (dims as f32));
    let corr = rope_correction(p, dims);
    let forward = if p.op == 48 { 1 } else { 0 };
    let workspace = mul_size(mul_size(tokens, width), 4);
    let decode = mode == 40 && tokens == 1 && p.x.ne[3] == 1 && width <= 512 && dims <= 64;
    let config: i64[27] = [if decode { 2 } else { 0 }, blocks, 1, phases, workspace,
        width, p.x.ne[1], tokens, p.x.ne[3], dims, mode, offs,
        p.ip[4], p.ip[5], p.ip[6], p.ip[7], size, p.x.nb[1], p.x.nb[2], p.x.nb[3],
        forward, bits(step) as i64, bits(p.fp[1]) as i64, bits(p.fp[2]) as i64,
        bits(p.fp[3]) as i64, bits(corr[0]) as i64, bits(corr[1]) as i64];
    let factor_bytes = if has_factors { p.z.bytes } else { 0 };
    return cuda_position(config, x[0 .. p.x.bytes], positions[0 .. p.y.bytes],
        factors[0 .. factor_bytes], dst[0 .. p.d.bytes]);
}

fn conv_cuda(p: const Node &, x: const u8[:], weights: const u8[:], dst: u8[:]) -> i64 {
    if p.nth != 1 || !cv_layout(p.x, x.len) || !cv_layout(p.y, weights.len) ||
       !cv_layout(p.d, dst.len) || !dense_scalar(p.d, 4) ||
       p.x.nb[0] != 4 || p.y.nb[0] != 4 || p.x.ne[3] != 1 ||
       p.y.ne[2] != 1 || p.y.ne[3] != 1 || p.d.ne[3] != 1 {
        return 0;
    }
    let kernel = p.y.ne[0];
    let channels = p.x.ne[1];
    let tokens = p.d.ne[1];
    let sequences = p.d.ne[2];
    if (kernel != 3 && kernel != 4 && kernel != 5 && kernel != 7 && kernel != 9 && kernel != 15) ||
       p.x.ne[0] != add_size(kernel - 1, tokens) || p.y.ne[1] != channels ||
       p.d.ne[0] != channels || p.x.ne[2] != sequences || tokens > 2147483647 - kernel ||
       p.x.nb[1] > 2147483647 || p.x.nb[2] > 2147483647 || p.y.nb[1] > 2147483647 ||
       p.d.nb[1] > 2147483647 || p.d.nb[2] > 2147483647 {
        return 0;
    }
    let groups = 1 + (channels - 1) / 128;
    if sequences > 2147483647 || groups > 65535 { return 0; }
    var config: i64[27] = [0; 27];
    config[0] = 1;
    config[1] = sequences;
    config[2] = groups;
    config[5] = channels;
    config[6] = kernel;
    config[7] = tokens;
    config[8] = p.x.nb[0];
    config[9] = p.x.nb[1];
    config[10] = p.x.nb[2];
    config[11] = p.y.nb[1];
    config[12] = p.d.nb[0];
    config[13] = p.d.nb[1];
    config[14] = p.d.nb[2];
    config[15] = if p.ip[0] == 1 { 1 } else { 0 };
    return cuda_position(config, x[0 .. p.x.bytes], weights[0 .. p.y.bytes], weights[0 .. 0], dst[0 .. p.d.bytes]);
}

extern "fg_cuda_routing" fn cuda_routing(config: const i64[:], x: const u8[:], y: const u8[:], dst: u8[:]) -> i64;

fn moe_normalize(p: const Node &, probs: const u8[:], ids: const u8[:], dst: u8[:]) -> bool {
    let experts = p.x.ne[0]; let used = p.y.ne[0]; let tokens = p.x.ne[1];
    if experts <= 0 || used <= 0 || used > experts || used > 64 || tokens <= 0 || tokens > 2147483647 ||
       p.x.kind != 0 || p.y.kind != 26 || p.d.kind != 0 || p.x.ne[2] != 1 || p.x.ne[3] != 1 ||
       p.y.ne[1] != tokens || p.y.ne[2] != 1 || p.y.ne[3] != 1 ||
       p.d.ne[0] != 1 || p.d.ne[1] != used || p.d.ne[2] != tokens || p.d.ne[3] != 1 ||
       !dense_scalar(p.x, 4) || !dense_scalar(p.y, 4) || !dense_scalar(p.d, 4) ||
       p.x.bytes > probs.len || p.y.bytes > ids.len || p.d.bytes > dst.len { return false; }
    let scale = p.fp[1];
    if !(scale >= 0.0 && scale <= 3.4028234663852886e38) { return false; }
    var cfg: i64[22] = [0; 22];
    cfg[0] = 4; cfg[1] = tokens; cfg[2] = 32;
    cfg[4] = experts; cfg[12] = used; cfg[21] = p.ip[1];
    let cuda = cuda_routing(cfg, probs[0 .. p.x.bytes], ids[0 .. p.y.bytes], dst[0 .. p.d.bytes]);
    if cuda != 0 { return cuda > 0; }
    if activation_read(probs) != 0 || activation_read(ids) != 0 { return false; }
    for row in tokens {
        var picked: f32[64] = default <f32[64]>();
        var sum: f64 = 0.0;
        for slot in used {
            let id = read32(ids, 4 * (row * used + slot)) as! i32;
            if id < 0 || (id as i64) >= experts { return false; }
            picked[slot] = from_bits(read32(probs, 4 * (row * experts + (id as i64))));
            sum += picked[slot] as f64;
        }
        let denominator = max32(sum as f32, 0.00006103515625);
        for slot in used {
            let weight = picked[slot] / denominator;
            let result = if scale == 0.0 || scale == 1.0 { weight } else { weight * scale + 0.0 };
            write32(dst, 4 * (row * used + slot), bits(result));
        }
    }
    return true;
}

fn routing_cuda(p: const Node &, x: const u8[:], y: const u8[:], dst: u8[:]) -> i64 {
    let kind = if p.op == 46 { 0 } else if p.op == 69 || p.op == 70 { 1 }
        else if p.op == 40 { 2 } else if p.op == 15 { 3 } else { -1 };
    if kind < 0 || p.nth != 1 || !cv_layout(p.x, x.len) || p.d.bytes <= 0 ||
       (if kind == 1 { p.d.kind != 26 } else { p.d.kind != 0 }) || !dense_scalar(p.d, 4) { return 0; }
    validate(p.d, dst.len);
    let width = p.x.ne[0];
    let mutates_rows = kind == 1 || kind == 3;
    if mutates_rows {
        for axis in 3 {
            if p.d.ne[axis + 1] != p.x.ne[axis + 1] { return 0; }
        }
    }
    var detail: i64 = 0;
    var param: i64 = 0;
    var groups = rows(p.x);
    var lanes: i64 = 32;
    let compact_topk = p.op == 70 && p.ip[14] == 1 && width == 256 && p.d.ne[0] == 8 &&
        p.x.ne[2] == 1 && p.x.ne[3] == 1;
    if kind == 0 {
        if width > 4096 || !same_shape(p.x, p.d) || p.ip[0] != 0 || p.ip[1] != 0 ||
           !(p.fp[0] > 0.0 && p.fp[0] <= 1.0) { return 0; }
        param = bits(p.fp[0]) as i64;
    } else if kind == 1 {
        if width > 4096 || p.x.nb[0] != 4 || p.d.ne[0] > width ||
           (p.ip[0] != 0 && p.ip[0] != 1) { return 0; }
        detail = 1;
        while detail < width { detail *= 2; }
        lanes = if detail <= 32 { 32 } else { 128 };
        param = p.ip[0];
    } else if kind == 2 {
        if p.y.kind != 26 || p.y.ne[3] != 1 || p.d.ne[0] != width ||
           p.y.ne[0] != p.d.ne[1] || p.y.ne[1] != p.d.ne[2] || p.y.ne[2] != p.d.ne[3] ||
           p.y.ne[1] != p.x.ne[2] || p.y.ne[2] != p.x.ne[3] { return 0; }
        for axis in 3 {
            if p.y.nb[axis] < 0 || p.y.nb[axis] % 4 != 0 { return 0; }
        }
        validate(p.y, y.len);
        detail = elements(p.d);
        lanes = 128;
        groups = 1 + (detail - 1) / lanes;
    } else if p.d.ne[0] != 1 || width > 4096 { return 0; }
    if compact_topk { groups = 1 + (groups - 1) / 4; }
    if groups <= 0 || groups > 2147483647 { return 0; }
    var config: i64[22] = [0; 22];
    config[0] = kind;
    config[1] = groups;
    config[2] = lanes;
    config[3] = if compact_topk { 1 } else { 0 };
    for axis in 4 {
        config[4 + axis] = p.x.ne[axis];
        config[8 + axis] = p.x.nb[axis];
        config[12 + axis] = p.d.ne[axis];
        config[16 + axis] = p.y.nb[axis];
    }
    config[20] = detail;
    config[21] = param;
    let ids = if kind == 2 { p.y.bytes } else { 0 };
    return cuda_routing(config, x[0 .. p.x.bytes], y[0 .. ids], dst[0 .. p.d.bytes]);
}

extern "fg_cuda_blit" fn cuda_blit(config: const i64[:], x: const u8[:], y: const u8[:], dst: u8[:]) -> i64;

fn blit_disjoint(a: u64, a_bytes: i64, b: u64, b_bytes: i64) -> bool {
    return if a <= b { b - a >= a_bytes as u64 } else { a - b >= b_bytes as u64 };
}

fn blit_cuda(p: const Node &, x: const u8[:], y: const u8[:], dst: u8[:], addresses: const i64[3] &) -> i64 {
    let op = p.op;
    let mode = if op == 22 { 0 } else if op == 35 || op == 1 { 1 }
        else if op == 34 { 2 } else { -1 };
    if mode < 0 || p.nth != 1 || !cv_layout(p.x, x.len) || !cv_layout(p.d, dst.len) ||
       !dense_scalar(p.d, 4) || addresses[0] == 0 || addresses[2] == 0 { return 0; }
    let target = addresses[2] as! u64;
    if !blit_disjoint(addresses[0] as! u64, p.x.bytes, target, p.d.bytes) { return 0; }
    let count = elements(p.d);
    let groups = 1 + (count - 1) / 128;
    if groups > 2147483647 { return 0; }
    var axis: i64 = 0;
    var left: i64 = 0;
    if mode == 0 {
        axis = p.ip[0];
        if axis < 0 || axis >= 4 || addresses[1] == 0 || !cv_layout(p.y, y.len) ||
           !blit_disjoint(addresses[1] as! u64, p.y.bytes, target, p.d.bytes) { return 0; }
        left = p.x.ne[axis];
        for i in 4 {
            if i == axis {
                if p.d.ne[i] != add_size(left, p.y.ne[i]) { return 0; }
            } else if p.d.ne[i] != p.x.ne[i] || p.d.ne[i] != p.y.ne[i] { return 0; }
        }
    } else if count != elements(p.x) { return 0; }
    var config: i64[17] = [0; 17];
    config[0] = mode;
    config[1] = groups;
    config[2] = count;
    config[3] = axis;
    config[4] = left;
    for i in 4 {
        config[5 + i] = if mode == 0 { p.d.ne[i] } else { p.x.ne[i] };
        config[9 + i] = p.x.nb[i];
        config[13 + i] = p.y.nb[i];
    }
    let y_bytes = if mode == 0 { p.y.bytes } else { 0 };
    return cuda_blit(config, x[0 .. p.x.bytes], y[0 .. y_bytes], dst[0 .. p.d.bytes]);
}

fn gelu(x: f32) -> f32 {
    return (0.5 as f32) * x * ( (1 as f32) + tanh32( (0.7978845608028654 as f32) * x * ( (1 as f32) + (0.044715 as f32) * x * x)));
}

fn gelu_compat(p: const Node &, x: f32) -> f32 {
    if p.x.kind == 0 && p.ip[13] != 0 {
        if x <= -(10 as f32) {
            return (0 as f32);
        }

        if x >= (10 as f32) {
            return x;
        }
        return half(to_half(gelu(half(to_half(x)))));
    }

    if p.x.kind == 1 {
        return half(to_half(gelu(x)));
    }
    return gelu(x);
}

fn gelu_erf(x: f32) -> f32 {
    return (0.5 as f32) * x * ( (1 as f32) + erf32(x * (0.7071067811865475 as f32)));
}

fn gelu_quick(x: f32) -> f32 {
    return x / ( (1 as f32) + exp32(-(1.702 as f32) * x));
}

fn silu(x: f32) -> f32 {
    return x / ( (1 as f32) + exp32(-x));
}

fn sigmoid(x: f32) -> f32 {
    return (1.0 as f32) / ( (1.0 as f32) + exp32(-x));
}

fn softplus(x: f32) -> f32 {
    return if x > (20.0 as f32) { x } else { log32( (1.0 as f32) + exp32(x)) };
}

fn avx2_exp_compat(x: f32) -> f32 {
    let z = fma32(x, from_bits(1069066811), 12582912 as f32);
    let n = z - (12582912 as f32);
    let b = fma32(-n, from_bits(901758606), fma32(-n, from_bits(1060205056), x));
    let e = bits(z) << 23;
    let k = from_bits( ( (e as u64) + (0x3f800000 as u64)) as !u32);
    let u = b * b;
    let j = fma32(
        fma32(fma32(from_bits(1007099920), b, from_bits(1026268951)), u, fma32(from_bits(1042984755), b, from_bits(1056964315))),
        u,
        from_bits(1065353206) * b);

    if ! (abs32(n) > (126 as f32)) {
        return fma32(j, k, k);
    }
    let g: u32 = if n <= (0 as f32) {
        0x82000000 as u32
    } else {
        0 as u32
    }
    ;
    let s1 = from_bits( ( (g as u64) + (0x7f000000 as u64)) as !u32);
    let s2 = from_bits( ( (e as u64) + (0x100000000 as u64) - (g as u64)) as !u32);

    if abs32(n) > (192 as f32) {
        return s1 * s1;
    }
    return fma32(s2, j, s2) * s1;
}

fn silu_compat(p: const Node &, x: f32, col: i64, width: i64) -> f32 {
    if p.ip[12] != 0 && p.x.kind == 0 && p.d.kind == 0 && col < (width / 8) * 8 {
        return x / ( (1 as f32) + avx2_exp_compat( (0 as f32) - x));
    }
    return silu(x);
}

fn unary_value(p: const Node &, op: i64, x: f32) -> f32 {
    if op == 0 {
        return abs32(x);
    }

    if op == 1 {
        return if x > (0.0 as f32) {
            (1.0 as f32)
        } else

        if x < (0.0 as f32) {
            -(1.0 as f32)
        } else {
            (0.0 as f32)
        }
        ;
    }

    if op == 2 {
        return -x;
    }

    if op == 3 {
        return if x > (0.0 as f32) {
            (1.0 as f32)
        } else {
            (0.0 as f32)
        }
        ;
    }

    if op == 4 {
        return tanh32(x);
    }

    if op == 5 {
        return if x > (0.0 as f32) {
            x
        } else {
            expm132(x)
        }
        ;
    }

    if op == 6 {
        return if x > (0.0 as f32) {
            x
        } else {
            (0.0 as f32)
        }
        ;
    }

    if op == 7 {
        return sigmoid(x);
    }

    if op == 12 {
        return min32( (1.0 as f32), max32( (0.0 as f32), (x + (3.0 as f32)) / (6.0 as f32)));
    }

    if op == 13 {
        return exp32(x);
    }

    if op == 11 {
        return x * min32( (1.0 as f32), max32( (0.0 as f32), (x + (3.0 as f32)) / (6.0 as f32)));
    }

    if op == 14 {
        return exp32(x) - (1.0 as f32);
    }

    if op == 15 {
        return softplus(x);
    }

    if op == 18 {
        return floor32(x);
    }

    if op == 19 {
        return ceil32(x);
    }

    if op == 20 {
        return round32(x);
    }

    if op == 21 {
        return trunc32(x);
    }

    if op == 17 {
        return if x > (0.0 as f32) {
            fma32(p.fp[1] * x, x, p.fp[2] * x)
        } else {
            fma32(expm132(min32(x, p.fp[3])) - x, p.fp[0], p.fp[2] * x)
        }
        ;
    }

    if op == 8 {
        return gelu_compat(p, x);
    }

    if op == 16 {
        return gelu_erf(x);
    }

    if op == 9 {
        return gelu_quick(x);
    }
    assert(op == 10);
    return silu(x);
}

fn pointwise(p: const Node &, x: const u8[:], y: const u8[:], dst: u8[:]) -> bool {
    if p.op != 2 && p.op != 4 && p.op != 6 && p.op != 7 && p.op != 8 && p.op != 32 && p.op != 9 && p.op != 10 && p.op != 12 && p.op != 13 && p.op != 11 && p.op != 50 && p.op != 71 && p.op != 91 && p.op != 23 {
        return false;
    }
    var i = thread_first(p);
    let count = elements(p.d);

    while i < count {
        let a = load_at(p.x, x, broadcast(p.x, p.d, i));
        var value: f32 = (0 as f32);

        if p.op == 2 || p.op == 4 || p.op == 6 || p.op == 7 || p.op == 8 || p.op == 23 {
            let b = load_at(p.y, y, broadcast(p.y, p.d, i));

            if p.op == 2 || p.op == 4 {
                value = a + b;
            } else

            if p.op == 6 {
                value = a - b;
            } else

            if p.op == 7 {
                value = a * b;
            } else

            if p.op == 8 {
                value = a / b;
            } else {
                let sig = (1 as f32) / ( (1 as f32) + exp32(-b));
                value = a * sig * ( (1 as f32) + b * ( (1 as f32) - sig));
            }
        } else

        if p.op == 32 {
            value = a * p.fp[0] + p.fp[1];
        } else

        if p.op == 9 {
            value = a * a;
        } else

        if p.op == 10 {
            value = sqrt32(a);
        } else

        if p.op == 12 {
            value = sin32(a);
        } else

        if p.op == 13 {
            value = cos32(a);
        } else

        if p.op == 11 {
            value = log32(a);
        } else

        if p.op == 50 {
            value = min32(max32(a, p.fp[0]), p.fp[1]);
        } else

        if p.op == 71 {
            value = if a > (0 as f32) {
                a
            } else {
                a * p.fp[0]
            }
            ;
        } else {
            value = unary_value(p, p.ip[0], a);
        }
        store(p.d, dst, i, value);
        i += p.nth;
    }
    return true;
}

fn cpp_min(a: f32, b: f32) -> f32 {
    return if b < a {
        b
    } else {
        a
    }
    ;
}

fn cpp_max(a: f32, b: f32) -> f32 {
    return if a < b {
        b
    } else {
        a
    }
    ;
}

fn glu(p: const Node &, x: const u8[:], y: const u8[:], dst: u8[:]) -> bool {
    if p.op != 100 {
        return false;
    }
    let width = p.d.ne[0];
    assert(width > 0 && rows(p.x) == rows(p.d));
    let separate = p.ip[2] != 0;
    let swapped = p.ip[1] != 0;

    if separate {
        assert(same_shape(p.x, p.y) && same_shape(p.x, p.d));
    } else {
        assert(p.x.ne[0] == 2 * width);
    }
    var row = thread_first(p);

    while row < rows(p.d) {
        for col in width {
            let a = load(p.x, x, row * p.x.ne[0] + col + (if separate || !swapped {
                0
            } else {
                width
            }
            ));
            let b = if separate {
                load(p.y, y, row * p.y.ne[0] + col)
            } else {
                load(p.x, x, row * p.x.ne[0] + col + (if swapped {
                    0
                } else {
                    width
                }
                ))
            }
            ;
            let op = p.ip[0];
            var value: f32 = (0 as f32);

            if op == 0 {
                value = (if a > (0 as f32) {
                    a
                } else {
                    (0 as f32)
                }
                ) * b;
            } else

            if op == 1 {
                value = gelu_compat(p, a) * b;
            } else

            if op == 4 {
                value = gelu_erf(a) * b;
            } else

            if op == 5 {
                value = gelu_quick(a) * b;
            } else

            if op == 2 {
                value = silu_compat(p, a, col, width) * b;
            } else

            if op == 3 {
                let aa = cpp_min(a, p.fp[1]);
                let bb = cpp_max(cpp_min(b, p.fp[1]), -p.fp[1]);
                value = (aa / ( (1 as f32) + exp32(-p.fp[0] * aa))) * ( (1 as f32) + bb);
            } else {
                assert(op == 6);
                let aa = cpp_min(a, p.fp[1]);
                let bb = cpp_min(cpp_max(b, -p.fp[1]), p.fp[1]);
                value = silu(aa) * bb;
            }
            store(p.d, dst, row * width + col, value);
        }
        row += p.nth;
    }
    return true;
}


fn reduction(p: const Node &, x: const u8[:], y: const u8[:], dst: u8[:]) -> bool {
    if p.op != 14 && p.op != 15 && p.op != 17 && p.op != 16 && p.op != 18 && p.op != 19 {
        return false;
    }

    if p.op == 14 {
        if p.ith == 0 {
            var total: f64 = (0 as f64);
            for row in rows(p.x) {
                var value: f64 = (0 as f64);
                for col in p.x.ne[0] {
                    value += load(p.x, x, row * p.x.ne[0] + col) as f64;
                }
                total += value;
            }
            store(p.d, dst, 0, total as f32);
        }
        return true;
    }

    if p.op == 19 {
        assert(p.x.kind == 26 && p.y.kind == 26 && same_shape(p.x, p.y));

        if p.ith == 0 {
            var total: i64 = 0;
            for i in elements(p.x) {
                if read32(x, offset(p.x, i)) == read32(y, offset(p.y, i)) {
                    total += 1;
                }
            }
            assert(p.d.bytes >= 8);
            let u = total as u64;
            for i in 8 {
                dst[i] = (u >> ( (8 * i) as u64)) as !u8;
            }
        }
        return true;
    }
    var row = thread_first(p);
    let width = p.x.ne[0];

    while row < rows(p.x) {
        if p.op == 18 {
            assert(width > 0);
            var at: i64 = 0;
            var best = load(p.x, x, row * width);
            for col in width {
                let v = load(p.x, x, row * width + col);

                if v > best {
                    best = v;
                    at = col;
                }
            }
            assert(p.d.kind == 26 && at <= 0x7fffffff);
            write32(dst, offset(p.d, row), at as u32);
        } else

        if p.op == 16 {
            var value: f32 = (0 as f32);
            for col in width {
                value += load(p.x, x, row * width + col);
                store(p.d, dst, row * width + col, value);
            }
        } else {
            var total: f64 = (0 as f64);
            for col in width {
                total += load(p.x, x, row * width + col) as f64;
            }
            let value = if p.op == 17 {
                (total / (width as f64)) as f32
            } else {
                total as f32
            }
            ;
            store(p.d, dst, row, value);
        }
        row += p.nth;
    }
    return true;
}

fn normalize(p: const Node &, x: const u8[:], y: const u8[:], dst: u8[:]) -> bool {
    if p.op != 24 && p.op != 25 && p.op != 28 && p.op != 26 {
        return false;
    }
    assert(same_shape(p.x, p.d));
    let width = p.x.ne[0];
    assert(width > 0);
    var row = thread_first(p);

    while row < rows(p.x) {
        var sum: f64 = (0 as f64);
        var sq: f64 = (0 as f64);
        var cross: f64 = (0 as f64);
        for col in width {
            let v = load(p.x, x, row * width + col);
            sum += v as f64;

            if p.op == 26 {
                let a = load(p.y, y, row * width + col);
                sq += (a * a) as f64;
                cross += (a * v) as f64;
            }
        }
        let mean = if p.op == 24 {
            (sum / (width as f64)) as f32
        } else {
            (0 as f32)
        }
        ;

        if p.op != 26 {
            for col in width {
                let v = load(p.x, x, row * width + col) - mean;
                sq += (v * v) as f64;
            }
        }
        let variance = if p.op == 28 {
            sq as f32
        } else

        if p.op == 26 {
            (sq as f32) / (width as f32)
        } else {
            (sq / (width as f64)) as f32
        }
        ;
        let scale = if p.op == 28 {
            (1 as f32) / max32(sqrt32(variance), p.fp[0])
        } else {
            (1 as f32) / sqrt32(variance + p.fp[0])
        }
        ;
        for col in width {
            let v = load(p.x, x, row * width + col);
            let value = if p.op == 26 {
                (v - load(p.y, y, row * width + col) * (cross as f32) / ( (sq as f32) + p.fp[0] * (width as f32))) * scale
            } else {
                (v - mean) * scale
            }
            ;
            let result = if p.ip[14] != 0 && p.op == 25 {
                value * load_at(p.y, y, broadcast(p.y, p.d, row * width + col))
            } else {
                value
            }
            ;
            store(p.d, dst, row * width + col, result);
        }
        row += p.nth;
    }
    return true;
}

fn topk_key(data: const u8[:], base: i64, index: i64, descending: bool) -> i64 {
    var bits = read32(data, base + 4 * index);
    if bits & 0x7fffffff == 0 { bits = 0; }
    var order: u32 = if bits & 0x80000000 != 0 { bits ^ 0xffffffff } else { bits ^ 0x80000000 };
    if !descending { order = order ^ 0xffffffff; }
    let key = ((order as u64) << 32) | (0xffffffff - (index as u64));
    return (key ^ 0x8000000000000000) as! i64;
}

fn topk_key_sift(heap: i64[:], size: i64, root: i64, value: i64) {
    var position = root;
    while 2 * position + 1 < size {
        var child = 2 * position + 1;
        if child + 1 < size && heap[child + 1] < heap[child] { child += 1; }
        if heap[child] >= value { break; }
        heap[position] = heap[child];
        position = child;
    }
    heap[position] = value;
}

fn topk_wide(p: const Node &, x: const u8[:], dst: u8[:], indices: i64[:]) -> bool {
    let n = p.x.ne[0];
    let count = p.d.ne[0];
    let start = p.ith * n;
    let heap = indices[start .. start + count];
    let descending = p.ip[0] != 0;
    var row = thread_first(p);
    while row < rows(p.x) {
        let base = offset(p.x, row * n);
        for i in count { heap[i] = topk_key(x, base, i, descending); }
        var parent = count / 2;
        while parent > 0 {
            parent -= 1;
            topk_key_sift(heap, count, parent, heap[parent]);
        }
        for i in count .. n {
            let key = topk_key(x, base, i, descending);
            if key > heap[0] { topk_key_sift(heap, count, 0, key); }
        }
        let output = offset(p.d, row * count);
        var remaining = count;
        while remaining > 0 {
            remaining -= 1;
            let index = 0xffffffff - ((heap[0] as! u64) & 0xffffffff);
            write32(dst, output + 4 * remaining, index as u32);
            if remaining > 0 { topk_key_sift(heap, remaining, 0, heap[remaining]); }
        }
        row += p.nth;
    }
    return true;
}

fn sort_rows(p: const Node &, x: const u8[:], dst: u8[:], indices: i64[:]) -> bool {
    if p.op != 69 && p.op != 70 {
        return false;
    }
    let n = p.x.ne[0];
    assert(p.d.kind == 26 && p.d.ne[0] <= n);
    assert(indices.len >= mul_size(n, p.nth));
    if p.op == 70 && p.ip[13] == 1 && n > 4096 && p.x.kind == 0 &&
       p.x.nb[0] == 4 && p.d.nb[0] == 4 && p.d.ne[0] > 0 {
        return topk_wide(p, x, dst, indices);
    }
    var row = thread_first(p);
    let base = p.ith * n;

    while row < rows(p.x) {
        for i in n {
            indices[base + i] = i;
        }
        for i in n {
            var j = i;

            while j > 0 {
                let ia = indices[base + j - 1];
                let ib = indices[base + j];
                let a = load(p.x, x, row * n + ia);
                let b = load(p.x, x, row * n + ib);
                let ordered = if p.ip[0] == 0 {
                    a <= b
                } else {
                    a >= b
                }
                ;

                if ordered {
                    break;
                }
                indices[base + j - 1] = ib;
                indices[base + j] = ia;
                j -= 1;
            }
        }
        for i in p.d.ne[0] {
            assert(indices[base + i] <= 0x7fffffff);
            write32(dst, offset(p.d, row * p.d.ne[0] + i), indices[base + i] as u32);
        }
        row += p.nth;
    }
    return true;
}


extern "powf" fn pow32(a: f32, b: f32) -> f32;

fn group_normalize(p: const Node &, x: const u8[:], dst: u8[:]) -> bool {
    if p.op != 27 {
        return false;
    }
    assert(same_shape(p.x, p.d));
    let groups = p.ip[0];
    assert(groups > 0);
    let channels = p.x.ne[2];
    let per = (channels + groups - 1) / groups;
    var group = thread_first(p);

    while group < groups {
        let first = group * per;
        let last = low(first + per, channels);
        let count = (last - first) * p.x.ne[0] * p.x.ne[1];
        assert(count > 0);
        for batch in p.x.ne[3] {
            var sum: f64 = (0 as f64);
            for ch in last - first {
                for row in p.x.ne[1] {
                    var part: f64 = (0 as f64);
                    for col in p.x.ne[0] {
                        let index = col + p.x.ne[0] * (row + p.x.ne[1] * (ch + first + channels * batch));
                        part += load(p.x, x, index) as f64;
                    }
                    sum += part;
                }
            }
            let mean = (sum / (count as f64)) as f32;
            var sq: f64 = (0 as f64);
            for ch in last - first {
                for row in p.x.ne[1] {
                    var part: f64 = (0 as f64);
                    for col in p.x.ne[0] {
                        let index = col + p.x.ne[0] * (row + p.x.ne[1] * (ch + first + channels * batch));
                        let v = load(p.x, x, index) - mean;
                        part += (v * v) as f64;
                        store(p.d, dst, index, v);
                    }
                    sq += part;
                }
            }
            let scale = (1 as f32) / sqrt32( (sq / (count as f64)) as f32 + p.fp[0]);
            for ch in last - first {
                for row in p.x.ne[1] {
                    for col in p.x.ne[0] {
                        let index = col + p.x.ne[0] * (row + p.x.ne[1] * (ch + first + channels * batch));
                        store(p.d, dst, index, load(p.d, dst, index) * scale);
                    }
                }
            }
        }
        group += p.nth;
    }
    return true;
}

fn softmax(
    p: const Node &,
    x: const u8[:],
    mask: const u8[:],
    sinks: const u8[:],
    dst: u8[:],
    scratch: f32[:]) -> bool {
    if p.op != 46 && p.op != 47 {
        return false;
    }
    let width = p.x.ne[0];
    assert(width > 0 && same_shape(p.x, p.d));
    assert(scratch.len >= mul_size(width, p.nth));
    let base = p.ith * width;
    var row = thread_first(p);

    while row < rows(p.x) {
        if p.op == 47 {
            assert(same_shape(p.x, p.y));
            var dot: f64 = (0 as f64);
            for col in width {
                let index = row * width + col;
                dot += (load(p.x, x, index) * load(p.y, mask, index)) as f64;
            }
            for col in width {
                let index = row * width + col;
                let grad = load(p.x, x, index);
                let prob = load(p.y, mask, index);
                store(p.d, dst, index, p.fp[0] * prob * (grad - dot as f32));
            }
        } else {
            let r1 = row % p.x.ne[1];
            let head = (row / p.x.ne[1]) % p.x.ne[2];
            let batch = row / (p.x.ne[1] * p.x.ne[2]);
            var head_power: i64 = 1;

            while head_power <= p.x.ne[2] / 2 {
                head_power *= 2;
            }
            var slope: f32 = (1 as f32);

            if p.fp[1] > (0 as f32) {
                let m = pow32( (2 as f32), -p.fp[1] / ( (if head < head_power {
                    head_power
                } else {
                    2 * head_power
                }
                ) as f32));
                slope = pow32(m, (if head < head_power {
                    head + 1
                } else {
                    2 * (head - head_power) + 1
                }
                ) as f32);
            }
            var maximum = from_bits(0xff800000);
            for col in width {
                var v = load(p.x, x, row * width + col) * p.fp[0];

                if p.ip[0] != 0 {
                    assert(p.y.ne[0] >= width && p.y.ne[1] > r1);
                    let off = col * p.y.nb[0] + r1 * p.y.nb[1] + (head % p.y.ne[2]) * p.y.nb[2] + (batch % p.y.ne[3]) * p.y.nb[3];
                    v += slope * load_at(p.y, mask, off);
                }
                scratch[base + col] = v;

                if v > maximum {
                    maximum = v;
                }
            }

            if p.ip[1] != 0 {
                let v = load(p.z, sinks, head);

                if v > maximum {
                    maximum = v;
                }
            }
            var sum: f64 = (0 as f64);
            for col in width {
                let v = exp32(scratch[base + col] - maximum);
                scratch[base + col] = v;
                sum += v as f64;
            }

            if p.ip[1] != 0 {
                sum += exp32(load(p.z, sinks, head) - maximum) as f64;
            }
            assert(sum > (0 as f64));
            let scale = ( (1 as f64) / sum) as f32;
            for col in width {
                store(p.d, dst, row * width + col, scratch[base + col] * scale);
            }
        }
        row += p.nth;
    }
    return true;
}


extern "fg_cuda_state_read" fn cuda_state_read(src: u64, count: i64) -> i64;
extern "fg_cuda_state_write" fn cuda_state_write(dst: u64, count: i64) -> i64;
extern "fg_cuda_conv_read" fn cuda_conv_read(src: u64, count: i64) -> i64;
extern "fg_cuda_match_published_state" fn cuda_match_state(dst: u64, src: u64, count: i64) -> bool;

fn dense_scalar(t: const Tensor &, size: i64) -> bool {
    var stride = size;
    for axis in 4 {
        if t.ne[axis] > 1 && t.nb[axis] != stride { return false; }
        stride = mul_size(stride, t.ne[axis]);
    }
    return stride == t.bytes;
}

fn transforms(p: const Node &, x: const u8[:], y: const u8[:], dst: u8[:],
              addresses: const i64[3] &) -> bool {
    let op = p.op;

    if op != 1 && op != 34 && op != 35 && op != 20 && op != 22 && op != 64 && op != 65 && op != 66 && op != 67 && op != 73 && op != 43 && op != 44 && op != 45 && op != 40 && op != 42 {
        return false;
    }
    var index = thread_first(p);
    let count = elements(p.d);

    if (op == 1 || op == 34 || op == 35) && p.x.kind == p.d.kind {
        assert(count == elements(p.x));
        let bytes = scalar_bytes(p.x.kind);
        if p.nth == 1 && addresses[0] != 0 && addresses[2] != 0 &&
           dense_scalar(p.x, bytes) && dense_scalar(p.d, bytes) {
            let src = addresses[0] as! u64;
            let dest = addresses[2] as! u64;
            if op == 34 && cuda_match_state(dest, src, p.x.bytes) { return true; }
            if cuda_state_read(src, p.x.bytes) != 0 { return false; }
            fg143_copy_bytes(dest, src, p.x.bytes);
            if op == 34 && cuda_state_write(dest, p.x.bytes) != 0 { return false; }
            return true;
        }

        if p.nth == 1 && addresses[0] != 0 && cuda_conv_read(addresses[0] as! u64, p.x.bytes) != 0 { return false; }
        while index < count {
            let source = offset(p.x, index);
            let target = offset(p.d, index);
            for byte in bytes {
                dst[target + byte] = x[source + byte];
            }
            index += p.nth;
        }
        if op == 34 && p.nth == 1 && addresses[2] != 0 &&
           cuda_state_write(addresses[2] as! u64, p.d.bytes) != 0 { return false; }
        return true;
    }

    if op == 22 && p.nth == 1 && (cuda_conv_read(addresses[0] as! u64, p.x.bytes) != 0 ||
       cuda_conv_read(addresses[1] as! u64, p.y.bytes) != 0) { return false; }
    while index < count {
        var n = index;
        var c: i64[4] = default < i64[4] > ();
        for axis in 4 {
            c[axis] = n % p.d.ne[axis];
            n /= p.d.ne[axis];
        }
        var value: f32 = (0 as f32);
        var target = index;

        if op == 73 {
            value = p.fp[0];
        } else

        if op == 67 {
            value = p.fp[0] + p.fp[2] * (index as f32);
        } else

        if op == 20 {
            value = load_at(p.x, x, broadcast(p.x, p.d, index));
        } else

        if op == 22 {
            let axis = p.ip[0];
            assert(axis >= 0 && axis < 4);
            var source = c;

            if c[axis] < p.x.ne[axis] {
                var offset: i64 = 0;
                for k in 4 {
                    offset = add_size(offset, mul_size(source[k], p.x.nb[k]));
                }
                value = load_at(p.x, x, offset);
            } else {
                source[axis] -= p.x.ne[axis];
                var offset: i64 = 0;
                for k in 4 {
                    offset = add_size(offset, mul_size(source[k], p.y.nb[k]));
                }
                value = load_at(p.y, y, offset);
            }
        } else

        if op == 64 {
            var valid: bool = true;
            var offset: i64 = 0;
            for axis in 4 {
                var at = c[axis] - p.ip[2 * axis];

                if p.ip[8] != 0 {
                    assert(p.x.ne[axis] > 0);
                    at = ( (at % p.x.ne[axis]) + p.x.ne[axis]) % p.x.ne[axis];
                } else

                if at < 0 || at >= p.x.ne[axis] {
                    valid = false;
                }

                if at >= 0 && at < p.x.ne[axis] {
                    offset = add_size(offset, mul_size(at, p.x.nb[axis]));
                }
            }

            if valid {
                value = load_at(p.x, x, offset);
            }
        } else

        if op == 65 {
            let left = p.ip[0];
            let at = c[0] - left;
            let reflected = if at < 0 {
                -at
            } else

            if at >= p.x.ne[0] {
                2 * (p.x.ne[0] - 1) - at
            } else {
                at
            }
            ;
            assert(reflected >= 0 && reflected < p.x.ne[0]);
            value = load_at(p.x, x, reflected * p.x.nb[0] + c[1] * p.x.nb[1] + c[2] * p.x.nb[2] + c[3] * p.x.nb[3]);
        } else

        if op == 66 {
            var offset: i64 = 0;
            for axis in 4 {
                let size = p.x.ne[axis];
                assert(size > 0);
                let at = ( (c[axis] - p.ip[axis]) % size + size) % size;
                offset = add_size(offset, mul_size(at, p.x.nb[axis]));
            }
            value = load_at(p.x, x, offset);
        } else

        if op == 43 {
            if c[0] == c[1] {
                value = load(p.x, x, c[0] + p.x.ne[0] * (c[2] + p.x.ne[2] * c[3]));
            }
        } else

        if op == 44 || op == 45 {
            value = load(p.x, x, index);

            if c[0] > p.ip[0] + c[1] {
                value = if op == 44 {
                    from_bits(0xff800000)
                } else {
                    (0 as f32)
                }
                ;
            }
        } else

        if op == 40 {
            let row = index / p.d.ne[0];
            let id = read32(y, offset(p.y, row)) as !i32;
            assert(id >= 0 && (id as i64) < p.x.ne[1]);
            let ix = c[0] * p.x.nb[0] + (id as i64) * p.x.nb[1] + c[2] * p.x.nb[2] + c[3] * p.x.nb[3];
            value = load_at(p.x, x, ix);
        } else

        if op == 42 {
            return false;
        } else {
            assert(count == elements(p.x));
            value = load(p.x, x, index);
        }
        store(p.d, dst, target, value);
        index += p.nth;
    }
    return true;
}


fn vector_dot(
    a: const Tensor &,
    x: const u8[:],
    ax: i64,
    as0: i64,
    b: const Tensor &,
    y: const u8[:],
    by: i64,
    bs0: i64,
    count: i64) -> f32 {
    assert(count >= 0);
    var acc: f32[8][4] = default < f32[8][4] > ();
    let end = (count / 32) * 32;
    var k: i64 = 0;

    while k < end {
        for group in 4 {
            for lane in 8 {
                let i = k + 8 * group + lane;
                let va = load_at(a, x, ax + i * as0);
                let vb = load_at(b, y, by + i * bs0);
                acc[group][lane] = if a.kind == 30 {
                    acc[group][lane] + va * vb
                } else {
                    fma32(va, vb, acc[group][lane])
                }
                ;
            }
        }
        k += 32;
    }
    let s = dot_lanes(acc);

    if a.kind == 0 {
        var result = (s[0] + s[1]) + (s[2] + s[3]);

        while k < count {
            result += load_at(a, x, ax + k * as0) * load_at(b, y, by + k * bs0);
            k += 1;
        }
        return result;
    }
    var result = if a.kind == 30 {
        ((s[0] + s[2]) + (s[1] + s[3])) as f64
    } else {
        ((s[0] + s[1]) + (s[2] + s[3])) as f64
    }
    ;

    while k < count {
        result += (load_at(a, x, ax + k * as0) * load_at(b, y, by + k * bs0)) as f64;
        k += 1;
    }
    return result as f32;
}

fn dot_lanes(acc: const f32[8][4] &) -> f32[4] {
    var v: f32[8] = default <f32[8]>();
    for lane in 8 {
        v[lane] = (acc[0][lane] + acc[2][lane]) + (acc[1][lane] + acc[3][lane]);
    }
    return [v[0] + v[4], v[1] + v[5], v[2] + v[6], v[3] + v[7]];
}

fn f32_row_dot(row: const f32[:], other: const f32[:], stride: i64) -> f32 {
    var acc: f32[8][4] = default <f32[8][4]>();
    let end = row.len / 32 * 32;
    var k: i64 = 0;
    while k < end {
        for group in 4 {
            for lane in 8 {
                let i = k + 8 * group + lane;
                acc[group][lane] = fma32(row[i], other[i * stride], acc[group][lane]);
            }
        }
        k += 32;
    }
    let s = dot_lanes(acc);
    var result = (s[0] + s[1]) + (s[2] + s[3]);
    while k < row.len {
        result += row[k] * other[k * stride];
        k += 1;
    }
    return result;
}



extern "fg_cuda_begin" fn cuda_begin(input: u64, input_bytes: i64, output: u64, output_bytes: i64) -> i64;
extern "fg_cuda_prepare_experts" fn cuda_prepare_experts(tensor: u64, bytes: i64,
    active: const i64[:], offsets: i64[:]) -> i64;
extern "fg_cuda_plane" fn cuda_plane(weights: u64, weight_bytes: i64, stride: i64,
                                    extent: i64, kind: i64, columns: i64, jobs: const i64[:]) -> i64;
extern "fg_cuda_expert_batch" fn cuda_expert_batch(weight_bytes: i64, stride: i64,
    extent: i64, kind: i64, columns: i64, jobs: const i64[:], shared_rows: i64, input_stride: i64) -> i64;
extern "fg_cuda_finish" fn cuda_finish(output: u64, output_bytes: i64) -> i64;

fn matrix_index_layout(p: const Node &) -> bool {
    return p.z.kind == 26 && p.x.ne[3] == 1 && p.y.ne[3] == 1 &&
        p.z.ne[0] > 0 && p.y.ne[1] > 0 && p.z.ne[1] == p.y.ne[2] &&
        p.z.ne[0] % p.y.ne[1] == 0 && p.z.ne[2] == 1 && p.z.ne[3] == 1 &&
        p.d.ne[1] == p.z.ne[0] && p.d.ne[2] == p.z.ne[1] && p.d.ne[3] == 1;
}

extern "fg_cuda_inkling_quants_enabled" fn cuda_inkling_quants_enabled() -> bool;

fn cuda_matrix_layout(p: const Node &) -> bool {
    let inkling = (p.x.kind == 8 || p.x.kind == 20 || p.x.kind == 23) &&
        p.x.ne[0] <= 2147483647 && (p.op != 30 || matrix_index_layout(p)) &&
        cuda_inkling_quants_enabled();
    let f32_layout = p.x.kind == 0 && p.x.nb[0] == 4 && p.x.ne[0] <= 2147483647 &&
        (p.x.nb[1] | p.x.nb[2] | p.x.nb[3]) % 4 == 0 &&
        (p.op != 30 || matrix_index_layout(p));
    return p.nth == 1 && (p.x.kind == 12 || p.x.kind == 14 || f32_layout || inkling) &&
        p.y.kind == 0 && p.y.nb[0] == 4 && p.d.kind == 0 &&
        (p.y.nb[1] | p.y.nb[2] | p.y.nb[3]) % 4 == 0 &&
        p.x.nb[1] >= 0 && p.x.nb[1] % 2 == 0 &&
        p.d.nb[0] == 4 && p.d.nb[1] == mul_size(p.d.ne[0], 4) &&
        p.d.nb[2] == mul_size(p.d.nb[1], p.d.ne[1]) &&
        p.d.nb[3] == mul_size(p.d.nb[2], p.d.ne[2]) &&
        p.d.bytes == mul_size(p.d.nb[3], p.d.ne[3]);
}

fn matrix(p: const Node &, x: const u8[:], y: const u8[:], ids: const u8[:], dst: u8[:]) -> bool {
    if p.op != 29 && p.op != 30 && p.op != 31 {
        return false;
    }

    if p.y.kind != 0 && p.y.kind != 1 && p.y.kind != 30 {
        return false;
    }
    if p.x.kind != 0 && p.x.kind != 1 && p.x.kind != 30 {
        return false;
    }
    let d0 = p.d.ne[0];
    assert(d0 > 0 && p.d.kind == 0);
    var row = thread_first(p);

    while row < rows(p.d) {
        let i1 = row % p.d.ne[1];
        let i2 = (row / p.d.ne[1]) % p.d.ne[2];
        let i3 = row / (p.d.ne[1] * p.d.ne[2]);
        for i0 in d0 {
            var xa: i64 = 0;
            var ya: i64 = 0;
            var xs: i64 = 0;
            var ys: i64 = 0;
            var count: i64 = 0;

            if p.op == 29 {
                assert(p.x.ne[0] == p.y.ne[0] && p.x.ne[1] == d0 && p.x.ne[2] > 0 && p.x.ne[3] > 0 && p.y.ne[2] % p.x.ne[2] == 0 && p.y.ne[3] % p.x.ne[3] == 0);
                let x2 = i2 / (p.y.ne[2] / p.x.ne[2]);
                let x3 = i3 / (p.y.ne[3] / p.x.ne[3]);
                xa = i0 * p.x.nb[1] + x2 * p.x.nb[2] + x3 * p.x.nb[3];
                ya = i1 * p.y.nb[1] + i2 * p.y.nb[2] + i3 * p.y.nb[3];
                xs = p.x.nb[0];
                ys = p.y.nb[0];
                count = p.x.ne[0];
            } else

            if p.op == 30 {
                assert(p.z.kind == 26 && p.x.ne[0] == p.y.ne[0] && p.x.ne[1] == d0 && p.y.ne[1] > 0);
                let id = read32(ids, offset(p.z, i1 + p.z.ne[0] * i2)) as !i32;
                assert(id >= 0 && (id as i64) < p.x.ne[2]);
                xa = i0 * p.x.nb[1] + (id as i64) * p.x.nb[2];
                ya = (i1 % p.y.ne[1]) * p.y.nb[1] + i2 * p.y.nb[2];
                xs = p.x.nb[0];
                ys = p.y.nb[0];
                count = p.x.ne[0];
            } else {
                assert(p.x.ne[1] == p.y.ne[1] && p.x.ne[0] == d0 && p.y.ne[0] == p.d.ne[1]);
                let x2 = i2 % p.x.ne[2];
                let x3 = i3 % p.x.ne[3];
                let y2 = i2 % p.y.ne[2];
                let y3 = i3 % p.y.ne[3];
                xa = i0 * p.x.nb[0] + x2 * p.x.nb[2] + x3 * p.x.nb[3];
                ya = i1 * p.y.nb[0] + y2 * p.y.nb[2] + y3 * p.y.nb[3];
                xs = p.x.nb[1];
                ys = p.y.nb[1];
                count = p.x.ne[1];
            }
            store(p.d, dst, row * d0 + i0, vector_dot(p.x, x, xa, xs, p.y, y, ya, ys, count));
        }
        row += p.nth;
    }
    return true;
}


fn quant_route(p: const Node &, route: i64, plane2: i64, plane3: i64,
               spread2: i64, spread3: i64) -> i64, i64 {
    if p.op == 30 {
        let token = route / p.z.ne[0];
        let base = token * p.y.nb[2];
        if p.y.ne[1] == 1 { return base, route; }
        let slot = route - token * p.z.ne[0];
        let input_slot = if p.y.ne[1] == p.z.ne[0] { slot } else { slot % p.y.ne[1] };
        return base + input_slot * p.y.nb[1], route;
    }
    let i1 = route % p.y.ne[1];
    let relative_plane = route / p.y.ne[1];
    let i2 = plane2 * spread2 + relative_plane % spread2;
    let i3 = plane3 * spread3 + relative_plane / spread2;
    return i1 * p.y.nb[1] + i2 * p.y.nb[2] + i3 * p.y.nb[3],
        i1 + p.d.ne[1] * (i2 + p.d.ne[2] * i3);
}

fn next_quant_route(indexed: bool, indices: const i64[:], next: i64, route: i64, count: i64) -> i64 {
    if indexed { return indices[next + route]; }
    return if route + 1 < count { route + 1 } else { -1 };
}

fn cuda_matrix_job_words(p: const Node &) -> i64 {
    let words = if p.op == 30 && p.x.kind != 0 { 3 } else { 2 };
    return mul_size(words, rows(p.d));
}

export fn fg143_mmq_linear_jobs(jobs: const i64[:], stride: i64, bytes: i64) -> bool {
    if stride <= 0 || bytes <= 0 || bytes % stride != 0 ||
       jobs.len % 2 != 0 || jobs.len / 2 != bytes / stride { return false; }
    var address: i64 = 0;
    for i in jobs.len / 2 {
        if jobs[2 * i] != address { return false; }
        address += stride;
    }
    return true;
}

// Jobs for each bank plane are contiguous. No tile may cross a plane boundary.
export fn fg143_mmq_tiles(jobs: const i64[:], words: i64, tile_width: i64, tiles: i64[:]) -> i64 {
    if (words != 2 && words != 3) || jobs.len <= 0 || jobs.len % words != 0 ||
       (tile_width != 8 && tile_width != 32 && tile_width != 64 && tile_width != 128) { return -1; }
    let count = jobs.len / words;
    var first: i64 = 0;
    var tile: i64 = 0;
    while first < count {
        let limit = low(tile_width, count - first);
        let weight = if words == 3 { jobs[3 * first + 2] } else { 0 };
        var size = limit;
        if words == 3 {
            size = 1;
            while size < limit && jobs[3 * (first + size) + 2] == weight { size += 1; }
        }
        if tile >= tiles.len / 3 || weight < 0 { return -1; }
        tiles[tile * 3] = first;
        tiles[tile * 3 + 1] = size;
        tiles[tile * 3 + 2] = weight;
        tile += 1;
        first += size;
    }
    return tile;
}

export fn fg143_mmq_window(extent: i64, tiles: i64, tile_width: i64, budget: i64) -> i64 {
    if extent <= 0 || extent > 2147483647 / 256 || tiles <= 0 || budget <= 0 ||
       (tile_width != 8 && tile_width != 32 && tile_width != 64 && tile_width != 128) { return -1; }
    let bytes_per_tile = extent * 288 * tile_width;
    // Packing launches one CUDA grid-Y block per route.
    return low(tiles, low(65535 / tile_width, high(1, budget / bytes_per_tile)));
}

// Group the wide descriptors within one already bounded packing window.
// Route indices and their packed inputs stay in the original order.
export fn fg143_mmq_wide_first(tiles: i64[:]) -> i64 {
    var wide: i64 = 0;
    for i in tiles.len / 3 {
        let at = 3 * i;
        if tiles[at + 1] <= 64 { continue; }
        for j in 3 {
            let value = tiles[at + j];
            tiles[at + j] = tiles[3 * wide + j];
            tiles[3 * wide + j] = value;
        }
        wide += 1;
    }
    return wide;
}

// Consecutive eight-route tiles form a dense shared launch.
// Round down so the packed workspace never exceeds the approved window.
export fn fg143_mmq_dense_window(tiles: i64, window: i64, group: i64) -> i64 {
    if tiles <= 0 || window <= 0 || window > tiles || (group != 4 && group != 8 && group != 16) { return -1; }
    return if window < tiles && window >= group { window - window % group } else { window };
}

export fn fg143_mmq_stream_workers(rows: i64, tiles: i64, extent: i64, sms: i64) -> i64 {
    if rows <= 0 || rows > 2147483647 || tiles <= 0 || tiles > 8191 || extent <= 1 || sms <= 0 { return 0; }
    let outputs = (1 + (rows - 1) / 64) * tiles;
    if outputs > 1073741823 / extent { return 0; }
    let waves = 1 + (outputs - 1) / sms;
    if 100 * outputs / (waves * sms) >= 90 { return 0; }
    let workers = low(sms, outputs * extent);
    return if workers != outputs { workers } else { 0 };
}

fn cuda_matrix_planes(p: const Node &, weights: u64, weight_bytes: i64,
                     planes: i64, row_bytes: i64, row_count: i64,
                     spread2: i64, spread3: i64, heads: i64, next: i64,
                     routes: const i64[:], jobs: i64[:], offsets: i64[:]) -> bool {
    let indexed = p.op == 30;
    let span = add_size(mul_size(p.d.ne[0] - 1, p.x.nb[1]), row_bytes);
    let k_quant = p.x.kind == 12 || p.x.kind == 14;
    let cacheable = k_quant || ((p.x.kind == 20 || p.x.kind == 23) && cuda_inkling_quants_enabled());
    let cached = if indexed && cacheable && p.x.nb[2] == span {
        cuda_prepare_experts(weights, span, routes[heads .. heads + planes], offsets)
    } else { 0 };
    if cached < 0 { return false; }
    let extent = if k_quant { p.x.ne[0] / 256 } else { p.x.ne[0] };
    var words: i64 = 0;
    for plane in planes {
        var route = if indexed { routes[heads + plane] } else { 0 };
        if route < 0 { continue; }
        let plane2 = plane % p.x.ne[2];
        let plane3 = plane / p.x.ne[2];
        let at = add_size(mul_size(plane2, p.x.nb[2]), mul_size(plane3, p.x.nb[3]));
        if at > weight_bytes || span > weight_bytes - at { return false; }
        let expert_offset = if cached == 1 { offsets[plane] } else { -1 };
        if cached == 1 && expert_offset < 0 { return false; }
        while route >= 0 {
            let ya, dst_row = quant_route(p, route, plane2, plane3, spread2, spread3);
            jobs[words] = ya;
            jobs[words + 1] = dst_row * p.d.nb[1];
            words += 2;
            if cached == 1 {
                jobs[words] = expert_offset;
                words += 1;
            }
            route = next_quant_route(indexed, routes, next, route, row_count);
        }
        if cached == 0 {
            if cuda_plane(weights + (at as u64), span, p.x.nb[1], extent, p.x.kind,
                          p.d.ne[0], jobs[0 .. words]) != 0 { return false; }
            words = 0;
        }
    }
    if cached == 1 {
        // With one input slot per token, all of its expert routes share this row.
        let shared_rows = if p.y.ne[1] == 1 && p.z.ne[0] > 1 && p.y.nb[2] > 0 { p.y.ne[2] } else { 0 };
        return cuda_expert_batch(span, p.x.nb[1], extent, p.x.kind, p.d.ne[0], jobs[0 .. words],
            shared_rows, p.y.nb[2]) == 0;
    }
    return true;
}

// Each worker owns output columns and decodes a weight row once for all
// input vectors using that row. Indexed multiplication groups only routes
// that exist, so weights of inactive experts are never touched.
fn packed_matrix(p: const Node &, x: const u8[:], y: const u8[:], ids: const u8[:], dst: u8[:],
                    scratch: f32[:], indices: i64[:], addresses: const i64[3] &,
                    gxx: const u8[8][:], gxs: const u8[8][:], gs: const u8[8][:],
                    g3x: const u8[4][:], g3s: const u8[4][:], g1: const i8[8][:],
                    signs: const u8[:], palette: const i8[:]) -> bool {
    let indexed = p.op == 30;
    if (p.op != 29 && !indexed) || p.d.kind != 0 ||
       (p.y.kind != 0 && p.y.kind != 1 && p.y.kind != 30) {
        return false;
    }
    let width = p.x.ne[0];
    let f32_input = p.x.kind == 0;
    let kind = decode :: super_kind(p.x.kind);
    let block_width = if f32_input { 1 } else if kind >= 0 { 256 } else { decode :: compact_width(p.x.kind) };
    let block_bytes = if f32_input { 4 } else if kind >= 0 { decode :: tile_traits(kind).bytes } else { decode :: compact_bytes(p.x.kind) };
    if width <= 0 || block_bytes == 0 || width % block_width != 0 || p.x.nb[0] != block_bytes ||
       p.x.ne[0] != p.y.ne[0] || p.d.ne[0] != p.x.ne[1] || p.d.ne[0] <= 0 ||
       p.x.ne[2] <= 0 || p.x.ne[3] <= 0 || p.y.ne[1] <= 0 || p.y.ne[2] <= 0 || p.y.ne[3] <= 0 ||
       (!f32_input && scratch.len < mul_size(width, p.nth)) {
        return false;
    }

    validate(p.y, y.len);
    validate(p.d, dst.len);
    if f32_input { validate(p.x, x.len); }
    let row_bytes = mul_size(width / block_width, block_bytes);
    let planes = mul_size(p.x.ne[2], p.x.ne[3]);
    var spread2: i64 = 1;
    var spread3: i64 = 1;
    var heads: i64 = 0;
    var next: i64 = 0;
    var route_words: i64 = 0;

    if indexed {
        if !matrix_index_layout(p) { return false; }
        if activation_read(ids) != 0 { return false; }
        validate(p.z, ids.len);
        let routes = mul_size(p.z.ne[0], p.z.ne[1]);
        let words = add_size(planes, routes);
        route_words = mul_size(words, p.nth);
        if indices.len < route_words { return false; }
        heads = p.ith * words;
        next = heads + planes;
        for expert in planes { indices[heads + expert] = -1; }
        var route: i64 = 0;
        for token in p.z.ne[1] {
            let row = token * p.z.nb[1];
            for slot in p.z.ne[0] {
                let expert = (read32(ids, row + slot * p.z.nb[0]) as! i32) as i64;
                if expert < 0 || expert >= planes { return false; }
                indices[next + route] = indices[heads + expert];
                indices[heads + expert] = route;
                route += 1;
            }
        }
    } else {
        if p.y.ne[2] % p.x.ne[2] != 0 || p.y.ne[3] % p.x.ne[3] != 0 ||
           p.d.ne[1] != p.y.ne[1] || p.d.ne[2] != p.y.ne[2] || p.d.ne[3] != p.y.ne[3] {
            return false;
        }
        spread2 = p.y.ne[2] / p.x.ne[2];
        spread3 = p.y.ne[3] / p.x.ne[3];
    }

    let row_count = mul_size(mul_size(p.y.ne[1], spread2), spread3);
    if addresses[0] != 0 && addresses[1] != 0 && addresses[2] != 0 && cuda_matrix_layout(p) {
        let job_words = cuda_matrix_job_words(p);
        let offset_words = if indexed { planes } else { 0 };
        let offset_start = add_size(route_words, job_words);
        if indices.len < add_size(offset_start, offset_words) || p.x.nb[2] < 0 || p.x.nb[3] < 0 { return false; }
        let status = cuda_begin(addresses[1] as! u64, y.len, addresses[2] as! u64, dst.len);
        if status < 0 { return false; }
        if status > 0 {
            let ok = cuda_matrix_planes(p, addresses[0] as! u64, x.len, planes, row_bytes, row_count,
                spread2, spread3, heads, next, indices[0 .. route_words],
                indices[route_words .. offset_start], indices[offset_start .. offset_start + offset_words]);
            let output = if ok { addresses[2] as! u64 } else { 0 };
            return cuda_finish(output, dst.len) == 0 && ok;
        }
    }
    if activation_read(x) != 0 || activation_read(y) != 0 { return false; }
    if f32_input { return matrix(p, x, y, ids, dst); }
    let decoded = scratch[p.ith * width .. (p.ith + 1) * width];
    var decoder: decode :: WeightTile = default < decode :: WeightTile > ();
    for plane in planes {
        let first = if indexed { indices[heads + plane] } else { 0 };
        if first < 0 { continue; }
        let plane2 = plane % p.x.ne[2];
        let plane3 = plane / p.x.ne[2];
        let xb = plane2 * p.x.nb[2] + plane3 * p.x.nb[3];
        var col = p.ith;
        while col < p.d.ne[0] {
            let xa = xb + col * p.x.nb[1];
            if xa < 0 || xa > x.len || row_bytes > x.len - xa { return false; }
            if !decode :: row_entry_with_workspace(p.x.kind, x[xa .. xa + row_bytes], decoded,
                                                  gxx, gxs, gs, g3x, g3s, g1, signs, palette, decoder) {
                return false;
            }

            var route = first;
            while route >= 0 {
                let ya, dst_row = quant_route(p, route, plane2, plane3, spread2, spread3);
                var sum: f32 = 0.0;
                for j in width {
                    sum = fma32(decoded[j], load_at(p.y, y, ya + j * p.y.nb[0]), sum);
                }
                store(p.d, dst, dst_row * p.d.ne[0] + col, sum);
                route = next_quant_route(indexed, indices, next, route, row_count);
            }
            col += p.nth;
        }
    }
    return true;
}

fn ssm_conv(p: const Node &, x: const u8[:], weight: const u8[:], dst: u8[:]) -> bool {
    if p.op != 76 {
        return false;
    }
    let kernel = p.y.ne[0];
    let channels = p.x.ne[1];
    let tokens = p.d.ne[1];
    let seqs = p.d.ne[2];
    assert(kernel > 0 && p.x.ne[0] == kernel - 1 + tokens && p.y.ne[1] == channels && p.d.ne[0] == channels);
    var channel = thread_first(p);

    while channel < channels {
        for seq in seqs {
            for token in tokens {
                var sum: f32 = (0 as f32);
                for tap in kernel {
                    let v = load_at(p.x, x, (token + tap) * p.x.nb[0] + channel * p.x.nb[1] + seq * p.x.nb[2]);
                    let w = load_at(p.y, weight, tap * p.y.nb[0] + channel * p.y.nb[1]);
                    sum += v * w;
                }
                if p.ip[0] == 1 { sum = silu(sum); }
                store(p.d, dst, channel + channels * (token + tokens * seq), sum);
            }
        }
        channel += p.nth;
    }
    return true;
}


fn yarn_pair(theta: f32, scale: f32, corr: f32[2], i: i64, ext: f32, amplitude: f32) -> f32[2] {
    var angle = theta * scale;
    var magnitude = amplitude;

    if ext != (0 as f32) {
        let ramp = (1 as f32) - min32( (1 as f32), max32( (0 as f32), ( (i / 2) as f32 - corr[0]) / max32( (0.001 as f32), corr[1] - corr[0])));
        let mix = ramp * ext;
        angle = angle * ( (1 as f32) - mix) + theta * mix;
        magnitude *= (1 as f32) + (0.1 as f32) * log32( (1 as f32) / scale);
    }
    var out: f32[2] = default < f32[2] > ();
    out[0] = cos32(angle) * magnitude;
    out[1] = sin32(angle) * magnitude;
    return out;
}

fn rope_sector_axis(mode: i64, sector: i64, first: i64, second: i64, third: i64) -> i64 {
    if mode == 40 {
        let lane = sector % 3;
        if lane == 0 && sector < 3 * first { return 0; }
        if lane == 1 && sector < 3 * second { return 1; }
        if lane == 2 && sector < 3 * third { return 2; }
        return 3;
    }
    if sector < first { return 0; }
    if sector < first + second { return 1; }
    if sector < first + second + third { return 2; }
    return 3;
}

fn rope_reset_axis(sector: i64, first: i64, second: i64, third: i64) -> i64 {
    if sector == 0 { return 0; }
    if sector == first { return 1; }
    if sector == first + second { return 2; }
    if sector == first + second + third { return 3; }
    return -1;
}

fn rope_correction(p: const Node &, dims: i64) -> f32[2] {
    let pi2 = (6.283185307179586 as f32);
    let original = p.ip[2] as f32;
    let divisor = (2 as f32) * log32(p.fp[0]);
    return [max32(0.0, floor32((dims as f32) * log32(original / (p.fp[4] * pi2)) / divisor)),
        min32((dims - 1) as f32, ceil32((dims as f32) * log32(original / (p.fp[5] * pi2)) / divisor))];
}

fn rope(
    p: const Node &,
    x: const u8[:],
    positions: const u8[:],
    factors: const u8[:],
    dst: u8[:],
    cache: f32[:]) -> bool {
    if p.op != 48 && p.op != 49 {
        return false;
    }
    assert(same_shape(p.x, p.d) && p.y.kind == 26 && (p.x.kind == 0 || p.x.kind == 1));
    let dims = p.ip[0];
    let mode = p.ip[1];
    let offs = p.ip[3];
    let width = p.x.ne[0];
    assert(dims > 0 && dims % 2 == 0 && offs >= 0 && offs % 2 == 0 && offs + dims <= width && width % 2 == 0);
    assert(cache.len >= mul_size(width, p.nth));
    assert(p.fp[0] > (0 as f32) && p.fp[1] > (0 as f32));
    let vision = mode == 24;
    let multiple = (mode & 8) != 0;

    if vision {
        assert(dims == width / 2 && offs == 0);
    }
    assert(mode == 0 || mode == 2 || mode == 8 || mode == 24 || mode == 40);
    let theta_scale = pow32(p.fp[0], -(2 as f32) / (dims as f32));
    let sign = if p.op == 48 {
        (1 as f32)
    } else {
        -(1 as f32)
    }
    ;
    let corr = rope_correction(p, dims);
    let base = p.ith * width;
    var row = thread_first(p);

    while row < rows(p.x) {
        let token = (row / p.x.ne[1]) % p.x.ne[2];
        var initial: f32[4] = default < f32[4] > ();
        var theta: f32[4] = default < f32[4] > ();
        for axis in 4 {
            let at = token + (if multiple {
                axis * p.x.ne[2]
            } else {
                0
            }
            );
            initial[axis] = (read32(positions, offset(p.y, at)) as !i32) as f32;
            theta[axis] = initial[axis];
        }
        let total = p.ip[4] + p.ip[5] + p.ip[6] + p.ip[7];

        if multiple {
            assert(total > 0 && total <= width);
        }
        for pair in width / 2 {
            let i = 2 * pair;
            var angle = theta[0];

            if multiple {
                let sector = pair % total;
                if vision {
                    let reset = rope_reset_axis(sector, p.ip[4], p.ip[5], p.ip[6]);
                    if reset >= 0 { theta[reset] = initial[reset]; }
                }
                angle = theta[rope_sector_axis(mode, sector, p.ip[4], p.ip[5], p.ip[6])];
            }
            let ff = if p.ip[8] != 0 {
                load(p.z, factors, pair)
            } else {
                (1 as f32)
            }
            ;
            let rotation = yarn_pair(angle / ff, p.fp[1], corr, i, p.fp[2], p.fp[3]);
            cache[base + i] = rotation[0];
            cache[base + i + 1] = rotation[1] * sign;
            for axis in 4 {
                theta[axis] *= theta_scale;
            }
        }
        let rotated = if vision {
            width
        } else {
            dims
        }
        ;
        let distance = if mode == 0 {
            1
        } else

        if vision {
            dims
        } else {
            dims / 2
        }
        ;
        let stride = if mode == 0 {
            1
        } else {
            2
        }
        ;
        for pair in rotated / 2 {
            let i = 2 * pair;
            let index = offs + i / stride;
            let a = load(p.x, x, row * width + index);
            let b = load(p.x, x, row * width + index + distance);
            let c = cache[base + i];
            let s = cache[base + i + 1];
            store(p.d, dst, row * width + index, a * c - b * s);
            store(p.d, dst, row * width + index + distance, a * s + b * c);
        }

        if !vision {
            let bytes = scalar_bytes(p.x.kind);
            for col in width {
                if col < offs || col >= offs + dims {
                    let source = offset(p.x, row * width + col);
                    let target = offset(p.d, row * width + col);
                    for byte in bytes {
                        dst[target + byte] = x[source + byte];
                    }
                }
            }
        }
        row += p.nth;
    }
    return true;
}


fn read_index(t: const Tensor &, a: const u8[:], i: i64) -> i64 {
    let at = offset(t, i);

    if t.kind == 26 {
        return (read32(a, at) as !i32) as i64;
    }
    assert(t.kind == 27 && at >= 0 && at <= a.len - 8);
    var u: u64 = 0;
    for b in 8 {
        u = u | ( (a[at + b] as u64) << ( (8 * b) as u64));
    }
    return u as !i64;
}

fn indexed(p: const Node &, x: const u8[:], y: const u8[:], dst: u8[:]) -> bool {
    if p.op != 42 && p.op != 41 && p.op != 21 && p.op != 3 && p.op != 5 && p.op != 33 && p.op != 72 {
        return false;
    }

    if p.op == 72 {
        var index = thread_first(p);

        while index < elements(p.d) {
            let col = index % p.d.ne[0];
            let row = (index / p.d.ne[0]) % p.d.ne[1];
            let keep = if p.ip[0] != 0 {
                col >= row + p.ip[1]
            } else {
                col <= row + p.ip[1]
            }
            ;
            store(p.d, dst, index, if keep {
                load(p.x, x, index)
            } else {
                (0 as f32)
            }
            );
            index += p.nth;
        }
        return true;
    }

    if p.op == 21 {
        if p.ith != 0 {
            return true;
        }
        for i in elements(p.d) {
            store(p.d, dst, i, (0 as f32));
        }
        for i in elements(p.x) {
            var n = i;
            var target: i64 = 0;
            var stride: i64 = 1;
            for axis in 4 {
                let at = n % p.x.ne[axis];
                n /= p.x.ne[axis];
                target += (at % p.d.ne[axis]) * stride;
                stride *= p.d.ne[axis];
            }
            store(p.d, dst, target, load(p.d, dst, target) + load(p.x, x, i));
        }
        return true;
    }

    if p.op == 41 {
        if p.ith != 0 {
            return true;
        }
        for i in elements(p.d) {
            store(p.d, dst, i, (0 as f32));
        }
        let width = p.x.ne[0];
        for row in rows(p.x) {
            let id = read_index(p.y, y, row);
            assert(id >= 0 && id < p.d.ne[1]);
            for col in width {
                let index = col + id * width;
                store(p.d, dst, index, load(p.d, dst, index) + load(p.x, x, row * width + col));
            }
        }
        return true;
    }

    if p.op == 42 {
        if p.ith != 0 {
            return true;
        }
        let width = p.x.ne[0];
        assert(width == p.d.ne[0]);
        for row in rows(p.x) {
            let i1 = row % p.x.ne[1];
            let i2 = (row / p.x.ne[1]) % p.x.ne[2];
            let i3 = row / (p.x.ne[1] * p.x.ne[2]);
            let id = read_index(p.y, y, i1 + p.y.ne[0] * ( (i2 % p.y.ne[1]) + p.y.ne[1] * (i3 % p.y.ne[2])));
            assert(id >= 0 && id < p.d.ne[1]);
            for col in width {
                let target = col * p.d.nb[0] + id * p.d.nb[1] + i2 * p.d.nb[2] + i3 * p.d.nb[3];
                store_at(p.d, dst, target, load(p.x, x, row * width + col));
            }
        }
        return true;
    }

    if p.op == 3 {
        return false;
    }

    if p.ith != 0 {
        return true;
    }
    assert(elements(p.x) == elements(p.d));

    if p.ip[4] == 0 {
        for i in elements(p.x) {
            let a = offset(p.x, i);
            let b = offset(p.d, i);
            for byte in scalar_bytes(p.x.kind) {
                dst[b + byte] = x[a + byte];
            }
        }
    }
    for i in elements(p.y) {
        var n = i;
        var target = p.ip[3];
        for axis in 4 {
            let at = n % p.y.ne[axis];
            n /= p.y.ne[axis];
            let stride = if axis == 0 {
                p.d.nb[0]
            } else {
                p.ip[axis - 1]
            }
            ;
            target = add_size(target, mul_size(at, stride));
        }
        let value = load(p.y, y, i);
        store_at(p.d, dst, target, if p.op == 5 {
            load_at(p.d, dst, target) + value
        } else {
            value
        }
        );
    }
    return true;
}


fn image_ops(p: const Node &, x: const u8[:], y: const u8[:], dst: u8[:]) -> bool {
    if p.op != 52 && p.op != 60 && p.op != 61 && p.op != 78 && p.op != 79 {
        return false;
    }
    var i = thread_first(p);
    let count = elements(p.d);

    while i < count {
        let o0 = i % p.d.ne[0];
        let o1 = (i / p.d.ne[0]) % p.d.ne[1];
        let o2 = (i / (p.d.ne[0] * p.d.ne[1])) % p.d.ne[2];
        let o3 = i / (p.d.ne[0] * p.d.ne[1] * p.d.ne[2]);
        var value: f32 = (0 as f32);

        if p.op == 52 {
            let is2 = p.ip[6] != 0;
            let kw = p.x.ne[0];
            let kh = if is2 {
                p.x.ne[1]
            } else {
                1
            }
            ;
            let channel = o0 / (kw * kh);
            let tap = o0 % (kw * kh);
            let xx = o1 * p.ip[0] + (tap % kw) * p.ip[4] - p.ip[2];
            let yy = if is2 {
                o2 * p.ip[1] + (tap / kw) * p.ip[5] - p.ip[3]
            } else {
                0
            }
            ;
            let input_h = if is2 {
                p.y.ne[1]
            } else {
                1
            }
            ;
            let batch = if is2 {
                o3
            } else {
                o2
            }
            ;

            if xx >= 0 && xx < p.y.ne[0] && yy >= 0 && yy < input_h {
                let off = xx * p.y.nb[0] + (if is2 {
                    yy * p.y.nb[1] + channel * p.y.nb[2] + batch * p.y.nb[3]
                } else {
                    channel * p.y.nb[1] + batch * p.y.nb[2]
                }
                );
                value = load_at(p.y, y, off);
            }
        } else

        if p.op == 60 || p.op == 61 {
            let is2 = p.op == 61;
            let k0 = p.ip[1];
            let k1 = if is2 {
                p.ip[2]
            } else {
                1
            }
            ;
            let s0 = if is2 {
                p.ip[3]
            } else {
                p.ip[2]
            }
            ;
            let s1 = if is2 {
                p.ip[4]
            } else {
                1
            }
            ;
            let pad0 = if is2 {
                p.ip[5]
            } else {
                p.ip[3]
            }
            ;
            let pad1 = if is2 {
                p.ip[6]
            } else {
                0
            }
            ;
            assert(k0 > 0 && k1 > 0);
            let average = p.ip[0] == 0;
            value = if average {
                (0 as f32)
            } else {
                from_bits(0xff7fffff)
            }
            ;
            for ky in k1 {
                for kx in k0 {
                    let xx = o0 * s0 - pad0 + kx;
                    let yy = if is2 {
                        o1 * s1 - pad1 + ky
                    } else {
                        o1
                    }
                    ;

                    if xx >= 0 && xx < p.x.ne[0] && yy >= 0 && yy < p.x.ne[1] {
                        let v = load_at(p.x, x, xx * p.x.nb[0] + yy * p.x.nb[1] + o2 * p.x.nb[2] + o3 * p.x.nb[3]);
                        value = if average {
                            value + v
                        } else {
                            max32(v, value)
                        }
                        ;
                    }
                }
            }

            if average {
                value /= (k0 * k1) as f32;
            }
        } else

        if p.op == 78 {
            let window = p.ip[0];
            let nx = p.ip[1];
            assert(window > 0 && nx > 0);
            let wx = o3 % nx;
            let wy = o3 / nx;
            let xx = wx * window + o1;
            let yy = wy * window + o2;

            if xx < p.x.ne[1] && yy < p.x.ne[2] {
                value = load_at(p.x, x, o0 * p.x.nb[0] + xx * p.x.nb[1] + yy * p.x.nb[2]);
            }
        } else {
            let window = p.ip[0];
            let nx = (p.d.ne[1] + window - 1) / window;
            let which = (o2 / window) * nx + (o1 / window);
            value = load_at(
                p.x,
                x,
                o0 * p.x.nb[0] + (o1 % window) * p.x.nb[1] + (o2 % window) * p.x.nb[2] + which * p.x.nb[3]);
        }
        store(p.d, dst, i, value);
        i += p.nth;
    }
    return true;
}




fn codec_transform(
    p: const Node &,
    x: const u8[:],
    ids: const u8[:],
    dst: u8[:],
    scratch: f32[:],
    gxx: const u8[8][:],
    gxs: const u8[8][:],
    gs: const u8[8][:],
    g3x: const u8[4][:],
    g3s: const u8[4][:],
    g1: const i8[8][:],
    signs: const u8[:],
    palette: const i8[:]) -> bool {
    if p.op != 1 && p.op != 34 && p.op != 35 && p.op != 40 {
        return false;
    }

    if p.d.kind != 0 || scalar_type(p.x) {
        return false;
    }
    let kind = decode :: super_kind(p.x.kind);
    let block_width = if kind >= 0 {
        256
    } else {
        decode :: compact_width(p.x.kind)
    }
    ;
    let bytes = if kind >= 0 {
        decode :: tile_traits(kind).bytes
    } else {
        decode :: compact_bytes(p.x.kind)
    }
    ;

    if bytes == 0 {
        return false;
    }
    let width = p.x.ne[0];
    assert(width > 0 && width % block_width == 0);
    let rowbytes = mul_size(width / block_width, bytes);
    assert(p.x.bytes <= x.len && scratch.len >= mul_size(width, p.nth));
    validate(p.d, dst.len);
    let temp = p.ith * width;

    if p.op == 40 {
        assert(p.y.kind == 26 && p.d.ne[0] == width);
        validate(p.y, ids.len);
    } else {
        assert(elements(p.x) == elements(p.d));
    }
    let nrows = if p.op == 40 {
        rows(p.d)
    } else {
        rows(p.x)
    }
    ;
    var row = thread_first(p);

    while row < nrows {
        var source: i64 = 0;

        if p.op == 40 {
            let i0 = row % p.y.ne[0];
            let i1 = (row / p.y.ne[0]) % p.y.ne[1];
            let i2 = row / (p.y.ne[0] * p.y.ne[1]);
            let id = read_index(p.y, ids, row);
            assert(id >= 0 && id < p.x.ne[1]);
            source = id * p.x.nb[1] + i1 * p.x.nb[2] + i2 * p.x.nb[3];
        } else {
            let i1 = row % p.x.ne[1];
            let i2 = (row / p.x.ne[1]) % p.x.ne[2];
            let i3 = row / (p.x.ne[1] * p.x.ne[2]);
            source = i1 * p.x.nb[1] + i2 * p.x.nb[2] + i3 * p.x.nb[3];
        }
        assert(source >= 0 && source <= p.x.bytes && rowbytes <= p.x.bytes - source);
        let output = scratch[temp .. temp + width];
        assert(decode :: row_entry(p.x.kind, x[source .. source + rowbytes], output, gxx, gxs, gs, g3x, g3s, g1, signs, palette));
        for col in width {
            store(p.d, dst, row * width + col, output[col]);
        }
        row += p.nth;
    }
    return true;
}

extern "fg143_cpu_gather_threads" fn cpu_gather_threads() -> i64;
extern "fg143_cpu_prefill" fn cpu_prefill() -> bool;
extern "fg143_gather_affinity" fn gather_affinity(prefill: bool, task: i64) -> i64;
extern "fg143_gather_index" fn gather_index(address: u64) -> i64;
extern "fg143_gather_fetch" fn gather_fetch(address: u64, row: u8[:]);
extern "fg143_gather_store" fn gather_store(address: u64, row: const f32[:]);
extern "fg143_gather_witness" fn gather_witness(rows: i64, tasks: i64);

struct CodecGather {
    weights: u64, ids: u64, dst: u64, width: i64, rows: i64,
    weight_rows: i64, stride: i64, row_bytes: i64, kind: i64, tasks: i64, prefill: bool
}

fn codec_gather_partition(job: CodecGather, task: i64) -> bool {
    var raw: u8[>..] = [];
    raw.resize(job.row_bytes, 0);
    var values: f32[>..] = [];
    values.resize(job.width, 0.0);
    var tile: decode :: WeightTile = default <decode :: WeightTile>();
    let g8: u8[8][0] = [];
    let g4: u8[4][0] = [];
    let g1: i8[8][0] = [];
    let signs: u8[0] = [];
    let palette: i8[0] = [];
    let first = job.rows * task / job.tasks;
    let last = job.rows * (task + 1) / job.tasks;
    for row in first .. last {
        let id = gather_index(job.ids + ((4 * row) as u64));
        if id < 0 || id >= job.weight_rows { return false; }
        gather_fetch(job.weights + ((id * job.stride) as u64), raw[..]);
        if !decode :: row_entry_with_workspace(job.kind, raw[..], values[..],
            g8, g8, g8, g4, g4, g1, signs, palette, tile) { return false; }
        gather_store(job.dst + ((4 * job.width * row) as u64), values[..]);
    }
    return true;
}

thread_fn codec_gather_worker(job: CodecGather, task: i64) {
    assert(gather_affinity(job.prefill, task) == 0);
    assert(codec_gather_partition(job, task));
}

export fn fg143_cpu_batch_supported(tasks: i64) -> bool {
    return tasks > 0 && tasks - 1 <= hardware_threads();
}

fn codec_parallel_gather(p: const Node &, addresses: const i64[3] &) -> i64 {
    if p.op != 40 || p.nth != 1 || (p.x.kind != 12 && p.x.kind != 14) ||
       p.d.kind != 0 || p.y.kind != 26 || !dense_scalar(p.d, 4) || !dense_scalar(p.y, 4) ||
       p.x.ne[2] != 1 || p.x.ne[3] != 1 || p.y.ne[1] != 1 || p.y.ne[2] != 1 ||
       p.y.ne[3] != 1 || p.d.ne[0] != p.x.ne[0] || p.y.ne[0] != rows(p.d) ||
       p.x.ne[0] <= 0 || p.x.ne[0] % 256 != 0 || p.x.ne[1] <= 0 ||
       addresses[0] == 0 || addresses[1] == 0 || addresses[2] == 0 { return 0; }
    let tasks = cpu_gather_threads();
    let count = p.y.ne[0];
    if tasks <= 1 || count < tasks || count > 2147483647 { return 0; }
    let weights = addresses[0] as! u64;
    let ids = addresses[1] as! u64;
    let dst = addresses[2] as! u64;
    let traits = decode :: tile_traits(decode :: super_kind(p.x.kind));
    let row_bytes = mul_size(p.x.ne[0] / 256, traits.bytes);
    if p.x.nb[0] != traits.bytes || p.x.nb[1] < row_bytes || p.x.bytes < row_bytes ||
       p.x.ne[1] - 1 > (p.x.bytes - row_bytes) / p.x.nb[1] ||
       p.y.bytes != 4 * count || p.d.bytes != mul_size(mul_size(p.x.ne[0], count), 4) ||
       !blit_disjoint(weights, p.x.bytes, dst, p.d.bytes) ||
       !blit_disjoint(ids, p.y.bytes, dst, p.d.bytes) { return 0; }

    let job = CodecGather { weights, ids, dst, p.x.ne[0], count, p.x.ne[1], p.x.nb[1], row_bytes, p.x.kind, tasks, cpu_prefill() };
    var workers: i64[>..] = [];
    for task in 1 .. tasks { workers.push(thread_spawn_contained(codec_gather_worker, job, task)); }
    var ok = codec_gather_partition(job, 0);
    for id in workers {
        if !thread_wait(id) { ok = false; }
    }
    if !ok { return -1; }
    gather_witness(count, tasks);
    return 1;
}


fn entry(
    p: const Node &,
    x: const u8[:],
    y: const u8[:],
    z: const u8[:],
    dst: u8[:],
    scratch: f32[:],
    indices: i64[:],
    addresses: const i64[3] &,
    gxx: const u8[8][:],
    gxs: const u8[8][:],
    gs: const u8[8][:],
    g3x: const u8[4][:],
    g3s: const u8[4][:],
    g1: const i8[8][:],
    signs: const u8[:],
    palette: const i8[:]) -> bool {
    assert(p.nth > 0 && p.ith >= 0 && p.ith < p.nth);

    if p.ip[15] == 1464816203 {
        assert(indices.len >= 4);
        var nf: i64 = 0;
        var ni: i64 = 0;

        if p.op == 46 || p.op == 47 || p.op == 48 || p.op == 49 {
            nf = mul_size(p.x.ne[0], p.nth);
        }

        if p.op == 69 || p.op == 70 {
            ni = mul_size(p.x.ne[0], p.nth);
        }
        if p.op == 42 && p.d.kind == 2 {
            ni = mul_size(2, rows(p.x));
        }

        if (p.op == 1 || p.op == 34 || p.op == 35 || p.op == 40) && !scalar_type(p.x) && p.d.kind == 0 {
            nf = mul_size(p.x.ne[0], p.nth);
        }
        let routed_matrix = p.op == 29 || p.op == 30;
        let offload = routed_matrix && addresses[0] != 0 && cuda_matrix_layout(p);
        if routed_matrix && (!scalar_type(p.x) || offload) {
            if !scalar_type(p.x) { nf = mul_size(p.x.ne[0], p.nth); }
            if p.op == 30 {
                let routes = mul_size(p.z.ne[0], p.z.ne[1]);
                ni = mul_size(add_size(p.x.ne[2], routes), p.nth);
            }
            if offload {
                ni = add_size(ni, cuda_matrix_job_words(p));
                if p.op == 30 { ni = add_size(ni, p.x.ne[2]); }
            }
        }
        let byte = mul_size(nf, 4);
        let aligned = mul_size(add_size(byte, 7) / 8, 8);
        indices[0] = nf;
        indices[1] = ni;
        indices[2] = aligned;
        indices[3] = add_size(aligned, mul_size(ni, 8));
        return true;
    }

    let routed = p.op == 29 || p.op == 30;
    if routed && (!scalar_type(p.x) || (p.x.kind == 0 && addresses[0] != 0 && cuda_matrix_layout(p))) {
        let handled = packed_matrix(p, x, y, z, dst, scratch, indices, addresses, gxx, gxs, gs, g3x, g3s, g1, signs, palette);
        if handled { witness(p.op, elements(p.d)); }
        return handled;
    }
    let vector = cv_try(p, x, y, dst, addresses);
    if vector != 0 {
        if vector > 0 { witness(p.op, elements(p.d)); }
        return vector > 0;
    }
    let position = if p.op == 48 || p.op == 49 {
        rope_cuda(p, x, y, z, dst)
    } else if p.op == 76 { conv_cuda(p, x, y, dst) } else { 0 };
    if position != 0 {
        if position > 0 { witness(p.op, elements(p.d)); }
        return position > 0;
    }
    let route = routing_cuda(p, x, y, dst);
    if route != 0 {
        if route > 0 { witness(p.op, elements(p.d)); }
        return route > 0;
    }
    let blit = blit_cuda(p, x, y, dst, addresses);
    if blit != 0 {
        if blit > 0 { witness(p.op, elements(p.d)); }
        return blit > 0;
    }
    if activation_read(x) != 0 || activation_read(y) != 0 || activation_read(z) != 0 { return false; }
    let gather = codec_parallel_gather(p, addresses);
    if gather != 0 {
        if gather > 0 { witness(p.op, elements(p.d)); }
        return gather > 0;
    }
    if codec_transform(p, x, y, dst, scratch, gxx, gxs, gs, g3x, g3s, g1, signs, palette) {
        witness(p.op, elements(p.d));
        return true;
    }
    if !scalar_type(p.x) { return false; }
    if !scalar_type(p.d) {
        return false;
    }
    validate(p.d, dst.len);
    validate(p.x, x.len);

    if p.y.bytes > 0 {
        validate(p.y, y.len);
    }

    if p.z.bytes > 0 {
        validate(p.z, z.len);
    }
    if transforms(p, x, y, dst, addresses) {
        witness(p.op, elements(p.d));
        return true;
    }

    if p.x.kind == 27 || p.x.kind == 28 || p.d.kind == 27 || p.d.kind == 28 {
        if p.op != 19 {
            return false;
        }
    }
    let handled = indexed(
        p,
        x,
        y,
        dst) || image_ops(p,
        x,
        y,
        dst) || rope(p,
        x,
        y,
        z,
        dst,
        scratch) || matrix(p, x, y, z, dst) || ssm_conv(p,
        x,
        y,
        dst) || pointwise(p,
        x,
        y,
        dst) || glu(p,
        x,
        y,
        dst) || reduction(p,
        x,
        y,
        dst) || normalize(p,
        x,
        y,
        dst) || group_normalize(p,
        x,
        dst) || softmax(p,
        x,
        y,
        z,
        dst,
        scratch) || sort_rows(p,
        x,
        dst,
        indices);

    if handled {
        witness(p.op, elements(p.d));
    }
    return handled;
}

fn flash_value_type(t: const Tensor &) -> bool {
    return t.kind == 0 || t.kind == 1 || t.kind == 30 ||
        (t.kind == 2 && t.ne[0] % 32 == 0 && t.nb[0] == 18);
}

fn flash_cache_value(t: const Tensor &, data: const u8[:], base: i64, column: i64,
                     palette: const i8[:]) -> f32 {
    if t.kind == 2 {
        return decode :: compact_value(t.kind, data, base + (column / 32) * t.nb[0], column % 32, palette);
    }
    return load_at(t, data, base + column * t.nb[0]);
}

extern "fg_cuda_q4_attention" fn cuda_q4_attention(config: const i64[:], q: const u8[:], k: const u8[:],
    v: const u8[:], mask: const u8[:], out: u8[:], scale: f32) -> i64;
extern "fg_cuda_kv_read" fn cuda_kv_read(k: const u8[:], v: const u8[:]) -> i64;

export fn fg143_flash_vector_parts(keys: i64) -> i64 {
    return if keys > 0 { low(64, 1 + (keys - 1) / 128) } else { 0 };
}

export fn fg143_flash_stream_workers(queries: i64, groups: i64, keys: i64, sms: i64) -> i64 {
    if queries < 4 || queries > 2147483647 || groups <= 0 || groups > 65535 ||
       keys <= 0 || keys > 2147483647 || sms <= 0 || sms > 1024 { return 0; }
    let tiles = ((queries + 7) / 8) * groups;
    let key_tiles = (keys + 31) / 32;
    if tiles > 2147483647 / key_tiles || key_tiles == 1 { return 0; }
    let raw = low(2 * sms, tiles * key_tiles);
    let rounded = (raw / tiles) * tiles;
    let workers = if rounded > 0 && 100 * (raw - rounded) <= 5 * raw { rounded } else { raw };
    return if workers > 1 && workers != tiles { workers } else { 0 };
}

fn cuda_q4_kv_layout(t: const Tensor &, available: i64) -> bool {
    if t.kind != 2 || t.ne[0] > 256 || t.ne[3] != 1 ||
       t.nb[0] != 18 || t.nb[1] <= 0 || t.nb[1] % 2 != 0 || t.nb[2] < 0 || t.nb[2] % 2 != 0 {
        return false;
    }
    let last = add_size(mul_size(t.ne[0] / 32, 18),
        add_size(mul_size(t.ne[1] - 1, t.nb[1]), mul_size(t.ne[2] - 1, t.nb[2])));
    return last <= t.bytes && t.bytes <= available;
}

extern "fg_cuda_giga_mla_attention" fn giga_mla_attention(config: i64[:], q: const u8[:],
    k: const u8[:], v: const u8[:], masks: const u8[:], dst: u8[:], scale: f32) -> i64;

fn mla_f16_layout(t: const Tensor &, width: i64, available: i64) -> bool {
    if t.kind != 1 || t.ne[0] != width || t.ne[2] != 1 || t.ne[3] != 1 ||
       t.nb[0] != 2 || t.nb[1] < width * 2 || t.nb[1] % 2 != 0 { return false; }
    let last = (t.ne[1] - 1) * t.nb[1] + width * 2;
    return last <= t.bytes && t.bytes <= available;
}

fn giga_flash_cuda(p: const Node &, mask: const Tensor &, q: const u8[:], k: const u8[:],
              v: const u8[:], masks: const u8[:], dst: u8[:]) -> i64 {
    let masked = mask.bytes > 0;
    let mla = p.x.ne[0] == 576 && mla_f16_layout(p.y, 576, k.len) && mla_f16_layout(p.z, 512, v.len);
    if !mla { return 0; }
    if p.x.ne[3] != 1 || p.fp[1] != 0.0 || p.fp[2] != 0.0 ||
       p.x.nb[0] != 4 || p.x.nb[1] % 4 != 0 || p.x.nb[2] % 4 != 0 || !dense_scalar(p.d, 4) ||
       (!mla && (!cuda_q4_kv_layout(p.y, k.len) || !cuda_q4_kv_layout(p.z, v.len))) ||
       (masked && (mask.ne[2] != 1 || mask.ne[3] != 1 || mask.nb[0] != 2 || mask.nb[1] % 2 != 0)) {
        return 0;
    }
    let positions = p.x.ne[1];
    let heads = p.x.ne[2];
    let tasks = mul_size(positions, heads);
    if tasks > 2147483647 { return 0; }
    let segment: i64 = if mla && positions == 1 { 64 } else { 256 };
    let tiles = 1 + (p.y.ne[1] - 1) / segment;
    if tiles > 65535 { return 0; }
    let per_task = mul_size(tiles, (p.z.ne[0] + 2) * 4);
    let batch = low(tasks, high(1, (32 * 1024 * 1024) / per_task));
    // Words 18..20 size the launches and bound the partial softmax buffer.
    // Word 21 gives the first query's causal key count, or zero for an explicit
    // mask or ordinary unmasked attention.
    let config: i64[23] = [p.x.ne[0], p.z.ne[0], positions, heads, p.y.ne[2], p.z.ne[2], p.y.ne[1],
        p.x.nb[1], p.x.nb[2], p.y.nb[1], p.y.nb[2], p.z.nb[1], p.z.nb[2], mask.nb[1],
        p.d.nb[1], p.d.nb[2], segment, tiles, tasks, batch, mul_size(batch, per_task), p.ip[5], if mla { 1 } else { 0 }];
    return giga_mla_attention(config, q[0 .. p.x.bytes], k[0 .. p.y.bytes], v[0 .. p.z.bytes],
        masks[0 .. mask.bytes], dst[0 .. p.d.bytes], p.fp[0]);
}

fn flash_cuda(p: const Node &, mask: const Tensor &, q: const u8[:], k: const u8[:],
              v: const u8[:], masks: const u8[:], dst: u8[:]) -> i64 {
    if p.x.ne[0] == 576 && p.y.kind == 1 && p.z.kind == 1 {
        return giga_flash_cuda(p, mask, q, k, v, masks, dst);
    }
    let masked = mask.bytes > 0;
    if p.x.ne[3] != 1 || p.fp[1] != 0.0 || p.fp[2] != 0.0 ||
       p.x.nb[0] != 4 || p.x.nb[1] % 4 != 0 || p.x.nb[2] % 4 != 0 || !dense_scalar(p.d, 4) ||
       !cuda_q4_kv_layout(p.y, k.len) || !cuda_q4_kv_layout(p.z, v.len) ||
       (masked && (mask.ne[2] != 1 || mask.ne[3] != 1 || mask.nb[0] != 2 || mask.nb[1] % 2 != 0)) {
        return 0;
    }
    let positions = p.x.ne[1];
    let heads = p.x.ne[2];
    let tasks = mul_size(positions, heads);
    if tasks > 2147483647 { return 0; }
    let segment: i64 = 256;
    let tiles = 1 + (p.y.ne[1] - 1) / segment;
    if tiles > 65535 { return 0; }
    let per_task = mul_size(tiles, (p.z.ne[0] + 2) * 4);
    let batch = low(tasks, high(1, (32 * 1024 * 1024) / per_task));
    // Words 18..20 size the launches and bound the partial softmax buffer.
    // Word 21 gives the first query's causal key count, or zero for an explicit
    // mask or ordinary unmasked attention.
    let config: i64[22] = [p.x.ne[0], p.z.ne[0], positions, heads, p.y.ne[2], p.z.ne[2], p.y.ne[1],
        p.x.nb[1], p.x.nb[2], p.y.nb[1], p.y.nb[2], p.z.nb[1], p.z.nb[2], mask.nb[1],
        p.d.nb[1], p.d.nb[2], segment, tiles, tasks, batch, mul_size(batch, per_task), p.ip[5]];
    return cuda_q4_attention(config, q[0 .. p.x.bytes], k[0 .. p.y.bytes], v[0 .. p.z.bytes],
        masks[0 .. mask.bytes], dst[0 .. p.d.bytes], p.fp[0]);
}

fn flash_attention(p: const Node &, mask: const Tensor &, q: const u8[:], k: const u8[:],
                   v: const u8[:], masks: const u8[:], dst: u8[:], palette: const i8[:]) -> bool {
    if p.x.kind != 0 || p.d.kind != 0 || !flash_value_type(p.y) || !flash_value_type(p.z) ||
       p.x.ne[0] != p.y.ne[0] || p.x.ne[0] <= 0 || p.z.ne[0] <= 0 ||
       p.y.ne[1] != p.z.ne[1] || p.y.ne[1] <= 0 ||
       p.x.ne[1] <= 0 || p.x.ne[2] <= 0 || p.x.ne[3] <= 0 ||
       p.y.ne[2] <= 0 || p.y.ne[3] <= 0 || p.z.ne[2] <= 0 || p.z.ne[3] <= 0 ||
       p.x.ne[2] % p.y.ne[2] != 0 || p.x.ne[2] % p.z.ne[2] != 0 ||
       p.x.ne[3] % p.y.ne[3] != 0 || p.x.ne[3] % p.z.ne[3] != 0 ||
       p.d.ne[0] != p.z.ne[0] || p.d.ne[1] != p.x.ne[2] ||
       p.d.ne[2] != p.x.ne[1] || p.d.ne[3] != p.x.ne[3] ||
       (p.fp[1] > 0.0 && mask.bytes == 0) {
        return false;
    }
    let has_mask = mask.bytes > 0;
    let first_query_keys = p.ip[5];
    if first_query_keys < 0 || (first_query_keys != 0 &&
        (has_mask || first_query_keys > p.y.ne[1] - p.x.ne[1] + 1)) { return false; }
    if has_mask {
        if mask.kind != 1 || mask.ne[0] < p.y.ne[1] || mask.ne[1] < p.x.ne[1] ||
           mask.ne[2] <= 0 || mask.ne[3] <= 0 ||
           p.x.ne[2] % mask.ne[2] != 0 || p.x.ne[3] % mask.ne[3] != 0 {
            return false;
        }
        validate(mask, masks.len);
    }
    validate(p.x, q.len);
    validate(p.d, dst.len);
    let device = flash_cuda(p, mask, q, k, v, masks, dst);
    if device != 0 { return device > 0; }
    if activation_read(q) != 0 || activation_read(k) != 0 ||
       activation_read(v) != 0 || activation_read(masks) != 0 { return false; }
    if cuda_kv_read(k, v) != 0 { return false; }
    let heads = p.x.ne[2];
    let positions = p.x.ne[1];
    var head_power: i64 = 1;
    while head_power <= heads / 2 { head_power *= 2; }
    let neg_inf = from_bits(0xff800000);
    for batch in p.x.ne[3] {
        let kb = (batch / (p.x.ne[3] / p.y.ne[3])) * p.y.nb[3];
        let vb = (batch / (p.x.ne[3] / p.z.ne[3])) * p.z.nb[3];
        for head in heads {
            let kh = kb + (head / (heads / p.y.ne[2])) * p.y.nb[2];
            let vh = vb + (head / (heads / p.z.ne[2])) * p.z.nb[2];
            var slope: f32 = 1.0;
            if p.fp[1] > 0.0 {
                let exponent: f32 = if head < head_power { (head + 1) as f32 } else { ((head - head_power) as f32) + 0.5 };
                slope = pow32(2.0, -p.fp[1] * (exponent as f32) / (head_power as f32));
            }
            for pos in positions {
                let qb = pos * p.x.nb[1] + head * p.x.nb[2] + batch * p.x.nb[3];
                let db = head * p.d.nb[1] + pos * p.d.nb[2] + batch * p.d.nb[3];
                var maximum: f32 = neg_inf;
                var total: f32 = 0.0;
                for column in p.d.ne[0] {
                    store_at(p.d, dst, db + column * p.d.nb[0], 0.0);
                }
                let count = if first_query_keys != 0 { first_query_keys + pos } else { p.y.ne[1] };
                for key in count {
                    var bias: f32 = 0.0;
                    if has_mask {
                        let at = key * mask.nb[0] + pos * mask.nb[1] +
                            (head % mask.ne[2]) * mask.nb[2] + (batch % mask.ne[3]) * mask.nb[3];
                        bias = slope * load_at(mask, masks, at);
                    }
                    if bias == neg_inf { continue; }
                    let kp = kh + key * p.y.nb[1];
                    var score: f32 = 0.0;
                    for column in p.x.ne[0] {
                        score = fma32(load_at(p.x, q, qb + column * p.x.nb[0]),
                            flash_cache_value(p.y, k, kp, column, palette), score);
                    }
                    score *= p.fp[0];
                    if p.fp[2] != 0.0 { score = p.fp[2] * tanh32(score / p.fp[2]); }
                    score += bias;
                    let updated = max32(maximum, score);
                    let previous = exp32(maximum - updated);
                    let weight = exp32(score - updated);
                    total = total * previous + weight;
                    maximum = updated;
                    let vp = vh + key * p.z.nb[1];
                    for column in p.d.ne[0] {
                        let at = db + column * p.d.nb[0];
                        store_at(p.d, dst, at, load_at(p.d, dst, at) * previous +
                            weight * flash_cache_value(p.z, v, vp, column, palette));
                    }
                }
                if total > 0.0 {
                    for column in p.d.ne[0] {
                        let at = db + column * p.d.nb[0];
                        store_at(p.d, dst, at, load_at(p.d, dst, at) / total);
                    }
                }
            }
        }
    }
    return true;
}

struct ScanPlan {
    source: Tensor[7], ith: i64, nth: i64, snapshots: i64
}

fn softplus_state(x: f32) -> f32 {
    return if x > (20 as f32) {
        x
    } else {
        log32( (1 as f32) + exp32(x))
    }
    ;
}

fn scan_entry(
    p: const ScanPlan &,
    initial: const u8[:],
    input: const u8[:],
    dt: const u8[:],
    decay: const u8[:],
    b: const u8[:],
    c: const u8[:],
    ids: const u8[:],
    dst: u8[:]) -> bool {
    assert(p.nth > 0 && p.ith >= 0 && p.ith < p.nth && p.snapshots > 0);
    for i in 6 {
        assert(p.source[i].kind == 0);
    }
    assert(p.source[6].kind == 26);
    let state_size = p.source[0].ne[0];
    let width = p.source[0].ne[1];
    let heads = p.source[1].ne[1];
    let tokens = p.source[1].ne[2];
    let seqs = p.source[1].ne[3];
    let groups = p.source[4].ne[1];
    assert(state_size > 0 && width > 0 && groups > 0 && heads % groups == 0);
    let state_stride = p.source[0].nb[3] / 4;
    let output = mul_size(mul_size(mul_size(width, heads), tokens), seqs);
    assert(dst.len >= mul_size(add_size(output, mul_size(mul_size(state_stride, seqs), p.snapshots)), 4));
    let first = (heads * p.ith) / p.nth;
    let last = (heads * (p.ith + 1)) / p.nth;
    for seq in seqs {
        let id = read32(ids, offset(p.source[6], seq)) as !i32;
        assert(id >= 0 && (id as i64) < p.source[0].ne[3]);
        for token in tokens {
            for head_delta in last - first {
                let head = first + head_delta;
                let group = head / (heads / groups);
                let timestep = softplus_state(load_at(p.source[2], dt, head * p.source[2].nb[0] + token * p.source[2].nb[1] + seq * p.source[2].nb[2]));
                let a0 = load_at(p.source[3], decay, head * p.source[3].nb[1]);
                let shared = p.source[3].ne[0] == 1;
                let factor = exp32(timestep * a0);
                for lane in width {
                    let x = load_at(
                        p.source[1],
                        input,
                        lane * p.source[1].nb[0] + head * p.source[1].nb[1] + token * p.source[1].nb[2] + seq * p.source[1].nb[3]);
                    let scaled = x * timestep;
                    var sum: f32 = (0 as f32);
                    for state in state_size {
                        let position = state + (lane + head * width) * state_size;
                        let target = output + seq * state_stride + position;
                        let previous = if token == 0 {
                            load_at(
                                p.source[0],
                                initial,
                                state * p.source[0].nb[0] + lane * p.source[0].nb[1] + head * p.source[0].nb[2] + (id as i64) * p.source[0].nb[3])
                        } else {
                            value_at(dst, target)
                        }
                        ;
                        let ba = load_at(
                            p.source[4],
                            b,
                            state * p.source[4].nb[0] + group * p.source[4].nb[1] + token * p.source[4].nb[2] + seq * p.source[4].nb[3]);
                        let ca = load_at(
                            p.source[5],
                            c,
                            state * p.source[5].nb[0] + group * p.source[5].nb[1] + token * p.source[5].nb[2] + seq * p.source[5].nb[3]);
                        let rate = if shared {
                            factor
                        } else {
                            exp32(timestep * load_at(p.source[3], decay, state * p.source[3].nb[0] + head * p.source[3].nb[1]))
                        }
                        ;
                        let value = previous * rate + ba * scaled;
                        set_value(dst, target, value);
                        sum += value * ca;
                    }
                    set_value(dst, lane + width * (head + heads * (token + tokens * seq)), sum);
                }
                let slot = tokens - 1 - token;

                if p.snapshots > 1 && slot > 0 && slot < p.snapshots {
                    for i in state_size * width {
                        let pos = i + head * state_size * width;
                        set_value(
                            dst,
                            output + (slot * seqs + seq) * state_stride + pos,
                            value_at(dst, output + seq * state_stride + pos));
                    }
                }
            }
        }
    }
    witness(77, output);
    return true;
}


struct StatePlan {
    source: Tensor[7], dst: Tensor, ith: i64, nth: i64, op: i64, snapshots: i64, fp: f32[4], publish: bool
}

fn value_at(a: const u8[:], index: i64) -> f32 {
    return from_bits(read32(a, mul_size(index, 4)));
}

fn set_value(a: u8[:], index: i64, v: f32) {
    write32(a, mul_size(index, 4), bits(v));
}

extern "fg_cuda_gdn" fn cuda_gdn(plan: const i64[:], q: const f32[:], k: const f32[:],
    v: const f32[:], gate: const f32[:], beta: const f32[:], initial: const f32[:], dst: f32[:]) -> i64;
extern "fg_cuda_gdn_state_read" fn cuda_gdn_state_read(initial: const f32[:]) -> i64;

// The scalar device kernel uses the fast division descriptor from its
// original host plan. Both divisors are positive and fit in 32 bits.
fn gdn_divisor(divisor: i64, words: i64[:]) {
    let d = divisor as u64;
    var shift: i64 = 0;
    while ((1 as u64) << shift) < d { shift += 1; }
    let multiplier = ((1 as u64) << 32) * (((1 as u64) << shift) - d) / d + 1;
    words[0] = multiplier as i64;
    words[1] = shift;
    words[2] = divisor;
}

fn gdn_on_cuda(p: const StatePlan &, strides: const i64[4][6] &,
               q: const f32[:], k: const f32[:], v: const f32[:], gate: const f32[:],
               beta: const f32[:], initial: const f32[:], dst: f32[:],
               scale: f32, scores: i64, state_values: i64) -> i64 {
    let width = p.source[2].ne[0];
    let heads = p.source[2].ne[1];
    let tokens = p.source[2].ne[2];
    let sequences = p.source[2].ne[3];
    let key_heads = p.source[0].ne[1];
    let q_sequences = p.source[0].ne[3];
    let snapshots = p.snapshots;
    if p.nth != 1 || (width != 16 && width != 32 && width != 64 && width != 128) ||
       sequences > 65535 || heads > 2147483647 || tokens <= 0 || snapshots > tokens ||
       p.source[1].ne[1] != key_heads || p.source[1].ne[3] != q_sequences ||
       strides[0][0] != 1 || strides[1][0] != 1 || strides[2][0] != 1 ||
       dst.len != scores + snapshots * state_values {
        return 0;
    }
    for axis in 4 {
        if strides[0][axis] != strides[1][axis] { return 0; }
    }
    for i in 3 .. 6 {
        if !dense_scalar(p.source[i], 4) { return 0; }
    }
    for axis in 1 .. 4 {
        if p.source[3].ne[axis] != p.source[2].ne[axis] ||
           p.source[4].ne[axis] != p.source[2].ne[axis] {
            return 0;
        }
    }
    if p.source[5].ne[3] != sequences { return 0; }
    // Device packet: strides, kernel geometry, F32 scale bits, fast
    // division descriptors, and the state offset following the scores.
    var config: i64[50] = [0; 50];
    for i in 6 {
        for axis in 4 { config[4 * i + axis] = strides[i][axis]; }
    }
    config[24] = width;
    config[25] = heads;
    config[26] = tokens;
    config[27] = sequences;
    config[30] = snapshots;
    config[31] = if snapshots > 1 { 1 } else { 0 };
    config[34] = width / 16;
    config[35] = low(32, width);
    config[36] = 4;
    config[37] = 32;
    config[38] = if width == 128 && tokens == 1 && snapshots == 1 && p.source[3].ne[0] == 1 { 1 } else { 0 };
    config[39] = state_values;
    config[40] = if p.source[3].ne[0] == width { 1 } else { 0 };
    config[41] = bits(scale) as i64;
    gdn_divisor(key_heads, config[42 .. 45]);
    gdn_divisor(sequences / q_sequences, config[45 .. 48]);
    config[48] = scores;
    config[49] = if p.publish { 1 } else { 0 };
    return cuda_gdn(config, q, k, v, gate, beta, initial, dst);
}

fn gdn(
    p: const StatePlan &,
    q: const f32[:],
    k: const f32[:],
    v: const f32[:],
    g: const f32[:],
    beta: const f32[:],
    initial: const f32[:],
    dst: f32[:]) -> bool {
    let width = p.source[2].ne[0];
    let heads = p.source[2].ne[1];
    let tokens = p.source[2].ne[2];
    let seqs = p.source[2].ne[3];
    let snapshots = p.snapshots;
    assert(width > 0 && heads > 0 && tokens >= 0 && seqs > 0 && snapshots >= 1 && p.nth > 0 && p.ith >= 0 && p.ith < p.nth);
    assert(p.dst.kind == 0 && dense_scalar(p.dst, 4));
    validate(p.dst, mul_size(dst.len, 4));
    validate(p.source[0], mul_size(q.len, 4));
    validate(p.source[1], mul_size(k.len, 4));
    validate(p.source[2], mul_size(v.len, 4));
    validate(p.source[3], mul_size(g.len, 4));
    validate(p.source[4], mul_size(beta.len, 4));
    validate(p.source[5], mul_size(initial.len, 4));

    var strides: i64[4][6] = default <i64[4][6]>();
    for source in 6 {
        assert(p.source[source].kind == 0);
        for axis in 4 {
            assert(p.source[source].nb[axis] % 4 == 0);
            strides[source][axis] = p.source[source].nb[axis] / 4;
        }
    }
    for source in 2 {
        let t = p.source[source];
        assert(t.ne[0] == width && t.ne[1] > 0 && heads % t.ne[1] == 0 &&
            t.ne[2] >= tokens && t.ne[3] > 0 && seqs % t.ne[3] == 0);
    }
    assert((p.source[3].ne[0] == 1 || p.source[3].ne[0] == width) && p.source[4].ne[0] == 1);
    for source in 3 .. 5 {
        let t = p.source[source];
        assert(t.ne[1] >= heads && t.ne[2] >= tokens && t.ne[3] >= seqs);
    }
    assert(p.source[5].ne[0] == width && p.source[5].ne[1] == width && p.source[5].ne[2] == heads && p.source[5].ne[3] >= seqs);

    let square = mul_size(width, width);
    let tasks = mul_size(heads, seqs);
    let scores = mul_size(mul_size(width, tokens), tasks);
    let snapshot_values = mul_size(square, tasks);
    assert(add_size(scores, mul_size(snapshot_values, snapshots)) <= dst.len);
    let scale = (1 as f32) / sqrt32(width as f32);
    let device = gdn_on_cuda(p, strides, q, k, v, g, beta, initial, dst, scale, scores, snapshot_values);
    if device != 0 {
        if device > 0 { witness(86, scores); }
        return device > 0;
    }
    if activation_read_f32(q) != 0 || activation_read_f32(k) != 0 ||
       activation_read_f32(v) != 0 || activation_read_f32(g) != 0 ||
       activation_read_f32(beta) != 0 || activation_read_f32(initial) != 0 { return false; }
    if cuda_gdn_state_read(initial) != 0 { return false; }
    let gate_width = p.source[3].ne[0];
    var decay: f32[>..] = [];
    decay.resize(gate_width, 0.0);
    var task = p.ith;

    while task < tasks {
        let head = task % heads;
        let seq = task / heads;
        let qhead = head % p.source[0].ne[1];
        let khead = head % p.source[1].ne[1];
        let qseq = seq / (seqs / p.source[0].ne[3]);
        let kseq = seq / (seqs / p.source[1].ne[3]);
        let statebase = scores + task * square;
        let state = dst[statebase .. statebase + square];

        if tokens > 0 || snapshots == 1 {
            let init = head * strides[5][2] + seq * strides[5][3];
            for j in width {
                let row = state[j * width .. (j + 1) * width];
                for i in row.len {
                    row[i] = initial[init + j * strides[5][1] + i * strides[5][0]];
                }
            }
        }
        for token in tokens {
            let qb = qhead * strides[0][1] + token * strides[0][2] + qseq * strides[0][3];
            let kb = khead * strides[1][1] + token * strides[1][2] + kseq * strides[1][3];
            let vb = head * strides[2][1] + token * strides[2][2] + seq * strides[2][3];
            let gb = head * strides[3][1] + token * strides[3][2] + seq * strides[3][3];
            let bb = head * strides[4][1] + token * strides[4][2] + seq * strides[4][3];
            let query = q[qb .. qb + (width - 1) * strides[0][0] + 1];
            let key = k[kb .. kb + (width - 1) * strides[1][0] + 1];
            let rate = beta[bb];
            let out = ((seq * tokens + token) * heads + head) * width;
            let score = dst[out .. out + width];
            for lane in gate_width {
                decay[lane] = exp32(g[gb + lane * strides[3][0]]);
            }

            for j in width {
                let row = state[j * width .. (j + 1) * width];
                if gate_width == 1 {
                    let factor = decay[0];
                    for i in row.len { row[i] *= factor; }
                } else {
                    for i in row.len { row[i] *= decay[i]; }
                }
                let predicted = f32_row_dot(row, key, strides[1][0]);
                let delta = (v[vb + j * strides[2][0]] - predicted) * rate;
                for i in row.len {
                    row[i] = fma32(key[i * strides[1][0]], delta, row[i]);
                }
                score[j] = f32_row_dot(row, query, strides[0][0]) * scale;
            }

            let slot = tokens - 1 - token;
            if snapshots > 1 && slot > 0 && slot < snapshots {
                let at = scores + slot * snapshot_values + task * square;
                let saved = dst[at .. at + square];
                for i in saved.len { saved[i] = state[i]; }
            }
        }
        task += p.nth;
    }
    witness(86, scores);
    return true;
}

fn recurrent_attention(
    p: const StatePlan &,
    a0: const u8[:],
    a1: const u8[:],
    a2: const u8[:],
    a3: const u8[:],
    a4: const u8[:],
    a5: const u8[:],
    a6: const u8[:],
    dst: u8[:]) -> bool {
    let v7 = p.op == 84;
    let linear = p.op == 83;

    if !v7 && !linear && p.op != 82 {
        return false;
    }
    let state_index = if v7 {
        6
    } else {
        if linear {
            4
        } else {
            5
        }
    }
    ;
    let width = p.source[1].ne[0];
    let heads = p.source[1].ne[1];
    let tokens = p.source[1].ne[2];
    let seqs = p.source[state_index].ne[1];
    assert(width > 0 && heads > 0 && tokens >= 0 && seqs > 0 && tokens % seqs == 0 && p.nth > 0 && p.ith >= 0 && p.ith < p.nth);
    assert(p.dst.kind == 0);
    validate(p.dst, dst.len);
    for input in state_index + 1 {
        let shape = p.source[input];
        assert(shape.kind == 0);
        var stride: i64 = 4;
        for axis in 4 {
            assert(shape.nb[axis] == stride);
            stride = mul_size(stride, shape.ne[axis]);
        }
        assert(shape.bytes >= stride);
    }
    validate(p.source[0], a0.len);
    validate(p.source[1], a1.len);
    validate(p.source[2], a2.len);
    validate(p.source[3], a3.len);
    validate(p.source[4], a4.len);

    if !linear {
        validate(p.source[5], a5.len);
    }

    if v7 {
        validate(p.source[6], a6.len);
        assert(width % 32 == 0);
    }
    let channels = mul_size(width, heads);
    let scores = mul_size(channels, tokens);
    let square = mul_size(width, width);
    let state_values = mul_size(mul_size(square, heads), seqs);
    assert(elements(p.source[state_index]) == state_values && mul_size(add_size(scores, state_values), 4) <= dst.len);
    for input in state_index {
        if !v7 && input == 3 && !linear {
            assert(elements(p.source[input]) >= channels);
        } else {
            assert(elements(p.source[input]) >= scores);
        }
    }
    let begin = heads * p.ith / p.nth;
    let finish = heads * (p.ith + 1) / p.nth;
    let simd_end = width / 8 * 8;
    for head in finish - begin {
        let h = begin + head;
        for token in tokens {
            let seq = token / (tokens / seqs);
            let output = token * channels + h * width;
            let state_base = scores + (seq * heads + h) * square;
            let initial = (seq * heads + h) * square;

            if token % (tokens / seqs) == 0 {
                for i in square {
                    let value = if v7 {
                        value_at(a6, initial + i)
                    } else {
                        if linear {
                            value_at(a4, initial + i)
                        } else {
                            value_at(a5, initial + i)
                        }
                    }
                    ;
                    set_value(dst, state_base + i, value);
                }
            }

            if v7 {
                for row in width {
                    let base = state_base + row * width;
                    let sa = vector_dot(p.source[4], a4, output * 4, 4, p.dst, dst, base * 4, 4, width);
                    let value = value_at(a3, output + row);
                    for col in width {
                        let at = output + col;
                        let kv = value * value_at(a2, at);
                        let decayed = fma32(value_at(dst, base + col), value_at(a1, at), kv);
                        set_value(dst, base + col, fma32(sa, value_at(a5, at), decayed));
                    }
                    set_value(
                        dst,
                        output + row,
                        vector_dot(p.dst, dst, base * 4, 4, p.source[0], a0, output * 4, 4, width));
                }
            } else {
                for col in width {
                    set_value(dst, output + col, 0 as f32);
                }
                for row in width {
                    let key = value_at(a0, output + row);
                    let query = value_at(a2, output + row) * (if linear {
                        p.fp[0]
                    } else {
                        1 as f32
                    }
                    );
                    let decay = if linear {
                        value_at(a3, output + row)
                    } else {
                        value_at(a4, output + row)
                    }
                    ;
                    let factor = if linear {
                        0 as f32
                    } else {
                        value_at(a3, h * width + row)
                    }
                    ;
                    for col in width {
                        let at = state_base + row * width + col;
                        let kv = value_at(a1, output + col) * key;
                        let previous = value_at(dst, at);
                        let next = if col < simd_end {
                            fma32(previous, decay, kv)
                        } else {
                            previous * decay + kv
                        }
                        ;
                        let weighted = if linear {
                            next
                        } else {
                            if col < simd_end {
                                fma32(kv, factor, previous)
                            } else {
                                kv * factor + previous
                            }
                        }
                        ;
                        let result = if col < simd_end {
                            fma32(weighted, query, value_at(dst, output + col))
                        } else {
                            value_at(dst, output + col) + weighted * query
                        }
                        ;
                        set_value(dst, at, next);
                        set_value(dst, output + col, result);
                    }
                }
            }
        }
    }
    witness(p.op, scores);
    return true;
}

fn state_entry(
    p: const StatePlan &,
    a0: const u8[:],
    a1: const u8[:],
    a2: const u8[:],
    a3: const u8[:],
    a4: const u8[:],
    a5: const u8[:],
    a6: const u8[:],
    dst: u8[:]) -> bool {
    if recurrent_attention(p, a0, a1, a2, a3, a4, a5, a6, dst) {
        return true;
    }
    validate(p.dst, dst.len);
    validate(p.source[0], a0.len);
    validate(p.source[1], a1.len);
    validate(p.source[2], a2.len);
    validate(p.source[3], a3.len);
    validate(p.source[4], a4.len);
    validate(p.source[5], a5.len);

    if p.op == 77 {
        validate(p.source[6], a6.len);
        var scan: ScanPlan = default < ScanPlan > ();
        scan.source = p.source;
        scan.ith = p.ith;
        scan.nth = p.nth;
        scan.snapshots = p.snapshots;
        assert(p.source[3].ne[0] == 1 || p.snapshots == 1);
        return scan_entry(scan, a0, a1, a2, a3, a4, a5, a6, dst);
    }
    return false;
}

extern "ggml_nbytes" fn storage_bytes(tensor: u64) -> u64;
extern "fg156_tget" fn storage_get(tensor: u64, field: i64, axis: i64) -> u64;
extern "fg156_tset" fn storage_set(tensor: u64, field: i64, axis: i64, value: u64);
extern "fg143_storage_alloc" fn storage_alloc(bytes: u64) -> u64;

const STORAGE_VIEW: i64 = 2;
const STORAGE_VIEW_OFFSET: i64 = 3;
const STORAGE_DATA: i64 = 4;
const STORAGE_MAX: u64 = 18446744073709551615;

fn storage_member(graph: u64, tensor: u64, count: i64) -> bool {
    let indexed = fg143_storage_contains(graph, tensor);
    if indexed >= 0 { return indexed != 0; }
    // A GGML subgraph borrows the original hash, including tensors outside
    // the selected range. Check the actual members in this case.
    for i in count {
        if fg143_storage_tensor(graph, i) == tensor { return true; }
    }
    return false;
}

fn storage_view_valid(graph: u64, tensor: u64, parent: u64, count: i64) -> bool {
    let parent_bytes = storage_bytes(parent);
    let at = storage_get(tensor, STORAGE_VIEW_OFFSET, 0);
    if at > parent_bytes || storage_bytes(tensor) > parent_bytes - at { return false; }
    if storage_get(tensor, STORAGE_DATA, 0) != 0 { return true; }
    let base = storage_get(parent, STORAGE_DATA, 0);
    if base != 0 { return base <= STORAGE_MAX - at; }
    // GGML flattens views to their base when constructing the tensor.
    return storage_get(parent, STORAGE_VIEW, 0) == 0 && storage_member(graph, parent, count);
}

export fn fg143_storage_allocate(graph: u64) -> u64 {
    if graph == 0 { return 0; }
    let count = fg143_storage_count(graph);
    if count < 0 { return 0; }
    let align = fg143_storage_alignment();
    let mask = align - 1;
    let aligned = STORAGE_MAX - mask;
    var needed: u64 = 0;
    for i in count {
        let t = fg143_storage_tensor(graph, i);
        if t == 0 { return 0; }
        let parent = storage_get(t, STORAGE_VIEW, 0);
        if parent != 0 {
            if !storage_view_valid(graph, t, parent, count) { return 0; }
        } else if storage_get(t, STORAGE_DATA, 0) == 0 {
            let bytes = storage_bytes(t);
            if bytes > aligned { return 0; }
            let padded = (bytes + mask) & aligned;
            if needed > STORAGE_MAX - padded { return 0; }
            needed += padded;
        }
    }
    let storage = storage_alloc(if needed == 0 { align } else { needed });
    if storage == 0 { return 0; }
    var cursor: u64 = 0;
    for i in count {
        let t = fg143_storage_tensor(graph, i);
        if storage_get(t, STORAGE_VIEW, 0) == 0 && storage_get(t, STORAGE_DATA, 0) == 0 {
            let bytes = storage_bytes(t);
            storage_set(t, STORAGE_DATA, 0, storage + cursor);
            cursor += (bytes + mask) & aligned;
        }
    }
    for i in count {
        let t = fg143_storage_tensor(graph, i);
        let parent = storage_get(t, STORAGE_VIEW, 0);
        if parent != 0 && storage_get(t, STORAGE_DATA, 0) == 0 {
            let base = storage_get(parent, STORAGE_DATA, 0);
            storage_set(t, STORAGE_DATA, 0, base + storage_get(t, STORAGE_VIEW_OFFSET, 0));
        }
    }
    return storage;
}

struct GraphNodes {
    address: u64,
    count: i64,
}

fn graph_node(nodes: GraphNodes, index: i64) -> u64 {
    let position = if index < 0 { nodes.count + index } else { index };
    if position < 0 || position >= nodes.count { return 0; }
    return tensor_read_word(nodes.address + (position as! u64) * 8);
}

fn graph_pointer_batch(nodes: GraphNodes, first: i64, output: u64[:]) {
    let remaining = nodes.count - first;
    let count = if remaining < output.len { remaining } else { output.len };
    if count <= 0 { return; }
    tensor_copy_raw(unsafe_c_address<c_ptr<void>>(output[0]),
        unsafe_c_ptr<c_ptr<const void>>(nodes.address + (first as! u64) * 8), (count as! u64) * 8);
}

extern "fg158_prop" fn graph_prop(tensor: u64, field: i64) -> i64;
extern "ggml_is_empty" fn graph_empty(tensor: u64) -> bool;
extern "fg_cuda_activation_prepare" fn graph_output_prepare(address: u64, bytes: i64, fresh: bool,
    overwrite: bool, step: i64, deadline: i64) -> i32;

fn graph_describe(tensor: u64, packet: i64[:]) {
    let present = fg143_graph_layout(tensor, packet);
    if present & 1 != 0 { packet[9] = goose_tensor_span(packet[8], packet[0 .. 8]); }
    if present & 2 != 0 { packet[19] = goose_tensor_span(packet[18], packet[10 .. 18]); }
    if present & 4 != 0 { packet[29] = goose_tensor_span(packet[28], packet[20 .. 28]); }
    packet[39] = goose_tensor_span(packet[38], packet[30 .. 38]);
    if present & 16 != 0 { packet[68] = goose_tensor_span(packet[67], packet[59 .. 67]); }
}
extern "fg_cuda_host_view_u8" fn graph_borrow_u8(address: u64, length: i64) -> u8[:];
extern "fg_cuda_host_view_f32" fn graph_borrow_f32(address: u64, length: i64) -> f32[:];
extern "fg_cuda_host_view_i64" fn graph_borrow_i64(address: u64, length: i64) -> i64[:];

fn graph_borrow_tensor(tensor: u64, bytes: i64) -> u8[:] {
    let data = if tensor == 0 { 0 } else { tensor_read_word(tensor + TENSOR_DATA) };
    return graph_borrow_u8(data, bytes);
}

fn graph_regular(tensor: u64, packet: const i64[:], mode: i64, sizes: i64[:], workspace: u64) -> bool {
    let op = packet[40];
    if op == 95 || (mode == 0 && op == TENSOR_OP_UNARY) {
        let special = inkling_graph_special(tensor, packet, mode, sizes);
        if special >= 0 { return special != 0; }
    }
    let x = graph_borrow_tensor(decode_source(tensor, 0), packet[9]);
    let y = graph_borrow_tensor(decode_source(tensor, 1), packet[19]);
    let z = graph_borrow_tensor(decode_source(tensor, 2), packet[29]);
    let mask = graph_borrow_tensor(decode_source(tensor, 3), packet[68]);
    let output = graph_borrow_tensor(tensor, packet[39]);
    if mode == 1 { return fg143_node_run(packet, mode, x, y, z, mask, output, graph_borrow_f32(0, 0), sizes); }
    let scratch = graph_borrow_f32(workspace, sizes[0]);
    let indices = graph_borrow_i64(workspace + (sizes[2] as! u64), sizes[1]);
    return fg143_node_run(packet, mode, x, y, z, mask, output, scratch, indices);
}

extern "fg143_graph_resize" fn graph_resize(workspace: u64, bytes: i64) -> u64;
extern "fg143_graph_observe" fn graph_observe(tensor: u64) -> bool;
extern "fg_cuda_routed_graph_enabled" fn routed_graph_enabled() -> bool;
extern "fg_cuda_full_ffn_enabled" fn full_ffn_enabled() -> bool;
extern "fg_cuda_direct_ssm_enabled" fn direct_ssm_enabled() -> bool;
extern "fg_cuda_direct_ssm" fn direct_ssm(plan: const i64[:], step: i64, deadline: i64) -> i64;
extern "fg_cuda_glu_pack_offer" fn offer_glu_pack(plan: const i64[:]) -> bool;
extern "fg_cuda_glu_pack_close" fn close_glu_pack() -> bool;
extern "fg_cuda_rms_scale" fn rms_scale(plan: const i64[:], input: u64, input_bytes: i64,
    output: u64, bytes: i64, step: i64, deadline: i64, eps: f32, scale: f32) -> i64;
extern "fg_cuda_fwht_q4_enabled" fn fwht_q4_enabled() -> bool;
extern "fg_cuda_fwht_q4_rows" fn fwht_q4_rows(input: u64, target: u64, width: i64, rows: i64,
    rotation: i32, scale: f32) -> i64;
extern "fg_cuda_routed_graph" fn routed_graph(plan: const i64[:], glu: const i64[:], norm: const i64[:],
    shared: const i64[:], step: i64, deadline: i64) -> i64;

export fn fg143_routed_layer_matches(layers: const i64[:], plan: const i64[:]) -> bool {
    for i in layers.len / 4 {
        let j = i * 4;
        if layers[j] >= 0 && layers[j + 1] == plan[0] && layers[j + 2] == plan[1] && layers[j + 3] == plan[2] {
            return true;
        }
    }
    return false;
}

struct GraphWorkspace { address: u64, capacity: i64 }

fn routed_dense(t: Tensor, n0: i64, n1: i64, n2: i64, kind: i64) -> bool {
    return t.kind == kind && t.ne[0] == n0 && t.ne[1] == n1 && t.ne[2] == n2 && t.ne[3] == 1 &&
        dense_scalar(t, 4) && t.bytes == n0 * n1 * n2 * 4;
}

fn routed_shape(p: const i64[:], width: i64, rows: i64, experts: i64, inputs: i64, kind: i64) -> bool {
    let w = packet_tensor(p, 0);
    let row = width / 256 * (if kind == 12 { 144 } else { 210 });
    let plane_bytes = row * rows;
    return w.kind == kind && w.ne[0] == width && w.ne[1] == rows && w.ne[2] == experts && w.ne[3] == 1 &&
        w.nb[0] == (if kind == 12 { 144 } else { 210 }) && w.nb[1] == row && w.nb[2] == plane_bytes &&
        w.bytes == plane_bytes * experts &&
        routed_dense(packet_tensor(p, 10), width, inputs, 1, 0) &&
        routed_dense(packet_tensor(p, 20), 8, 1, 1, 26) &&
        routed_dense(packet_tensor(p, 30), rows, 8, 1, 0);
}

fn graph_vector_plan(p: const i64[:], op: i64, config: i64[:]) {
    config[0] = op;
    config[1] = p[30] * p[31];
    for axis in 4 {
        config[2 + axis] = p[30 + axis];
        config[6 + axis] = p[axis];
        config[10 + axis] = p[4 + axis];
        config[14 + axis] = p[10 + axis];
        config[18 + axis] = p[14 + axis];
    }
    config[24] = (config[1] + 127) / 128;
    config[25] = 128;
}

fn shared_projection(p: const i64[:], width: i64, rows: i64, kind: i64) -> bool {
    let w = packet_tensor(p, 0);
    let block_bytes = if kind == 12 { 144 } else { 210 };
    let row_bytes = width / 256 * block_bytes;
    return w.kind == kind && w.ne[0] == width && w.ne[1] == rows && w.ne[2] == 1 && w.ne[3] == 1 &&
        w.nb[0] == block_bytes && w.nb[1] == row_bytes && w.bytes == rows * row_bytes &&
        routed_dense(packet_tensor(p, 10), width, 1, 1, 0) &&
        routed_dense(packet_tensor(p, 30), rows, 1, 1, 0);
}

// Match the complete shared expert and residual before extending the routed graph.
// Each matched intermediate must have its final consumer inside this group.
fn shared_ffn_plan(nodes: GraphNodes, step: i64, deadlines: const i64[:], input: u64, routed: u64,
                   width: i64, shared: i64[:]) -> bool {
    if step + 14 >= deadlines.len || deadlines[step + 5] != step + 13 ||
       deadlines[step + 6] != step + 8 || deadlines[step + 7] != step + 8 ||
       deadlines[step + 8] != step + 9 || deadlines[step + 9] != step + 12 ||
       deadlines[step + 10] != step + 11 || deadlines[step + 11] != step + 12 ||
       deadlines[step + 12] != step + 13 || deadlines[step + 13] != step + 14 { return false; }
    let a = graph_node(nodes, step + 6);
    let b = graph_node(nodes, step + 7);
    let glu = graph_node(nodes, step + 8);
    let down = graph_node(nodes, step + 9);
    let router = graph_node(nodes, step + 10);
    let sigmoid = graph_node(nodes, step + 11);
    let product = graph_node(nodes, step + 12);
    let added = graph_node(nodes, step + 13);
    let output = graph_node(nodes, step + 14);
    if graph_prop(a, 2) != 29 || graph_prop(b, 2) != 29 ||
       graph_prop(glu, 2) != 100 || graph_prop(down, 2) != 29 ||
       graph_prop(router, 2) != 29 || graph_prop(sigmoid, 2) != 91 ||
       graph_prop(product, 2) != 7 || graph_prop(added, 2) != 2 ||
       graph_prop(output, 2) != 2 || storage_get(output, STORAGE_VIEW, 0) != 0 { return false; }

    var s: u64[10] = [0; 10];
    var packet: i64[71] = [0; 71];
    fg143_graph_inputs(glu, s[..]);
    let gate = s[0]; let up = s[1];
    if !((gate == a && up == b) || (gate == b && up == a)) { return false; }
    graph_describe(up, packet[..]);
    let hidden = packet[1];
    if hidden <= 0 || hidden > 8192 || hidden % 256 != 0 ||
       !shared_projection(packet[..], width, hidden, 12) { return false; }
    fg143_graph_inputs(up, s[..]);
    if s[1] == 0 || storage_get(s[1], STORAGE_DATA, 0) != input { return false; }
    let up_weight = s[0];
    fg143_graph_inputs(gate, s[..]);
    if s[1] == 0 || storage_get(s[1], STORAGE_DATA, 0) != input { return false; }
    let gate_weight = s[0];
    graph_describe(gate, packet[..]);
    if !shared_projection(packet[..], width, hidden, 12) { return false; }
    fg143_graph_inputs(down, s[..]);
    let down_weight = s[0];
    if s[1] != glu { return false; }
    graph_describe(down, packet[..]);
    let kind = packet[8];
    if (kind != 12 && kind != 14) || !shared_projection(packet[..], hidden, width, kind) { return false; }
    graph_describe(glu, packet[..]);
    if packet[41] != 2 || packet[42] != 0 || packet[53] != 0 ||
       !routed_dense(packet_tensor(packet[..], 30), hidden, 1, 1, 0) { return false; }
    graph_vector_plan(packet[..], CV_SWIGLU, shared[8 .. 34]);

    fg143_graph_inputs(router, s[..]);
    let router_weight = s[0];
    if s[1] == 0 || storage_get(s[1], STORAGE_DATA, 0) != input { return false; }
    graph_describe(router, packet[..]);
    if !routed_dense(packet_tensor(packet[..], 0), width, 1, 1, 0) ||
       !routed_dense(packet_tensor(packet[..], 10), width, 1, 1, 0) ||
       !routed_dense(packet_tensor(packet[..], 30), 1, 1, 1, 0) { return false; }
    fg143_graph_inputs(sigmoid, s[..]);
    if s[0] != router { return false; }
    graph_describe(sigmoid, packet[..]);
    if packet[41] != 7 || !routed_dense(packet_tensor(packet[..], 30), 1, 1, 1, 0) { return false; }
    graph_vector_plan(packet[..], CV_SIGMOID, shared[34 .. 60]);
    fg143_graph_inputs(product, s[..]);
    if s[0] != down || s[1] != sigmoid { return false; }
    graph_describe(product, packet[..]);
    if !routed_dense(packet_tensor(packet[..], 30), width, 1, 1, 0) { return false; }
    graph_vector_plan(packet[..], CV_MUL, shared[60 .. 86]);
    fg143_graph_inputs(added, s[..]);
    if s[0] != routed || s[1] != product { return false; }
    graph_describe(added, packet[..]);
    if !routed_dense(packet_tensor(packet[..], 30), width, 1, 1, 0) { return false; }
    graph_vector_plan(packet[..], CV_ADD, shared[86 .. 112]);
    fg143_graph_inputs(output, s[..]);
    if s[0] != added { return false; }
    let residual = s[1];
    graph_describe(output, packet[..]);
    if !routed_dense(packet_tensor(packet[..], 10), width, 1, 1, 0) ||
       !routed_dense(packet_tensor(packet[..], 30), width, 1, 1, 0) { return false; }
    graph_vector_plan(packet[..], CV_ADD, shared[112 .. 138]);

    let tensors: u64[6] = [up_weight, gate_weight, down_weight, router_weight, residual, output];
    for i in tensors.len {
        let address = storage_get(tensors[i], STORAGE_DATA, 0);
        if address == 0 { return false; }
        shared[i] = address as! i64;
    }
    shared[6] = hidden; shared[7] = kind;
    return true;
}

fn try_routed_graph(nodes: GraphNodes, step: i64, deadlines: const i64[:]) -> i64 {
    if step + 5 >= deadlines.len || deadlines[step] != step + 2 || deadlines[step + 1] != step + 2 ||
       deadlines[step + 2] != step + 3 || deadlines[step + 3] != step + 5 || deadlines[step + 4] != step + 5 {
        return 0;
    }
    let first = graph_node(nodes, step);
    let second = graph_node(nodes, step + 1);
    let glu_node = graph_node(nodes, step + 2);
    let down_node = graph_node(nodes, step + 3);
    let norm_node = graph_node(nodes, step + 4);
    let output = graph_node(nodes, step + 5);
    if graph_prop(first, 2) != 30 || graph_prop(second, 2) != 30 ||
       graph_prop(glu_node, 2) != 100 || graph_prop(down_node, 2) != 30 ||
       graph_prop(norm_node, 2) != 95 || graph_prop(output, 2) != 95 { return 0; }
    var s: u64[10] = [0; 10];
    fg143_graph_inputs(glu_node, s[..]);
    let gate_node = s[0];
    let up_node = s[1];
    if !((gate_node == first && up_node == second) || (gate_node == second && up_node == first)) { return 0; }
    fg143_graph_inputs(up_node, s[..]);
    let up = s[0]; let input = s[1]; let ids = s[2];
    fg143_graph_inputs(gate_node, s[..]);
    let gate = s[0];
    if s[1] != input || s[2] != ids { return 0; }
    fg143_graph_inputs(down_node, s[..]);
    let down = s[0];
    if s[1] != glu_node || s[2] != ids { return 0; }
    fg143_graph_inputs(norm_node, s[..]);
    let probs = s[0];
    if s[1] != ids { return 0; }
    fg143_graph_inputs(output, s[..]);
    if s[0] != down_node || s[1] != norm_node { return 0; }

    var matrix: i64[71] = [0; 71];
    graph_describe(up_node, matrix[..]);
    let width = matrix[0]; let hidden = matrix[1]; let experts = matrix[2];
    if width <= 0 || width > 8192 || width % 256 != 0 ||
       hidden <= 0 || hidden > 8192 || hidden % 256 != 0 || experts != 256 ||
       !routed_shape(matrix[..], width, hidden, experts, 1, 12) { return 0; }
    graph_describe(gate_node, matrix[..]);
    if !routed_shape(matrix[..], width, hidden, experts, 1, 12) { return 0; }
    graph_describe(down_node, matrix[..]);
    let down_kind = matrix[8];
    if (down_kind != 12 && down_kind != 14) || !routed_shape(matrix[..], hidden, width, experts, 8, down_kind) { return 0; }

    graph_describe(glu_node, matrix[..]);
    if matrix[41] != 2 || matrix[42] != 0 || matrix[53] != 0 ||
       !routed_dense(packet_tensor(matrix[..], 30), hidden, 8, 1, 0) { return 0; }
    var glu: i64[26] = [0; 26];
    graph_vector_plan(matrix[..], CV_SWIGLU, glu[..]);

    graph_describe(norm_node, matrix[..]);
    if matrix[41] != 0x4d4f454e || !routed_dense(packet_tensor(matrix[..], 0), experts, 1, 1, 0) ||
       !routed_dense(packet_tensor(matrix[..], 30), 1, 8, 1, 0) { return 0; }
    let scale = from_bits(matrix[42] as! u32);
    if !(scale >= 0.0 && scale <= 3.4028234663852886e38) { return 0; }
    var norm: i64[22] = [0; 22];
    norm[0] = 4; norm[1] = 1; norm[2] = 32; norm[4] = experts; norm[12] = 8; norm[21] = matrix[42];

    graph_describe(output, matrix[..]);
    if matrix[41] != 0x4d4f4557 || !routed_dense(packet_tensor(matrix[..], 30), width, 1, 1, 0) ||
       storage_get(output, STORAGE_VIEW, 0) != 0 { return 0; }
    let tensors: u64[7] = [up, gate, down, input, probs, ids, output];
    var plan: i64[10] = [0; 10];
    for j in tensors.len {
        let address = storage_get(tensors[j], STORAGE_DATA, 0);
        if address == 0 { return 0; }
        plan[j] = address as! i64;
    }
    plan[7] = width; plan[8] = hidden; plan[9] = down_kind;
    if full_ffn_enabled() {
        var shared: i64[138] = [0; 138];
        if shared_ffn_plan(nodes, step, deadlines, plan[3] as! u64, output, width, shared[..]) &&
           !activation_spans_overlap(shared[5] as! u64, (width * 4) as u64, plan[4] as! u64, (experts * 4) as u64) &&
           !activation_spans_overlap(shared[5] as! u64, (width * 4) as u64, plan[5] as! u64, 32) &&
           !activation_spans_overlap(shared[5] as! u64, (width * 4) as u64, shared[4] as! u64, (width * 4) as u64) &&
           (shared[5] == plan[3] || !activation_spans_overlap(shared[5] as! u64, (width * 4) as u64, plan[3] as! u64, (width * 4) as u64)) {
            let grouped = routed_graph(plan[..], glu[..], norm[..], shared[..], step, deadlines[step + 14]);
            if grouped != 0 { return if grouped > 0 { 15 } else { grouped }; }
        }
    }
    let destination = plan[6] as! u64;
    if activation_spans_overlap(destination, (width * 4) as u64, plan[3] as! u64, (width * 4) as u64) ||
       activation_spans_overlap(destination, (width * 4) as u64, plan[4] as! u64, (experts * 4) as u64) ||
       activation_spans_overlap(destination, (width * 4) as u64, plan[5] as! u64, 32) { return 0; }
    var empty: i64[0] = [];
    let grouped = routed_graph(plan[..], glu[..], norm[..], empty[..], step, deadlines[step + 5]);
    return if grouped > 0 { 6 } else { grouped };
}

fn try_direct_ssm(nodes: GraphNodes, step: i64, deadlines: const i64[:], concat: u64) -> i64 {
    if step + 3 >= deadlines.len || deadlines[step] != step + 3 || deadlines[step + 1] > step + 2 ||
       deadlines[step + 2] > step + 3 { return 0; }
    let tail = graph_node(nodes, step + 1);
    let copy = graph_node(nodes, step + 2);
    let conv = graph_node(nodes, step + 3);
    if graph_prop(tail, 2) != 37 || graph_prop(copy, 2) != 34 ||
       graph_prop(conv, 2) != 76 || storage_get(tail, STORAGE_VIEW, 0) != concat ||
       storage_get(conv, STORAGE_VIEW, 0) != 0 { return 0; }
    var inputs: u64[10] = [0; 10];
    fg143_graph_inputs(concat, inputs[..]);
    let history = inputs[0];
    let transposed = inputs[1];
    if history == 0 || transposed == 0 || graph_prop(transposed, 2) != 39 { return 0; }
    fg143_graph_inputs(transposed, inputs[..]);
    let mixed = inputs[0];
    fg143_graph_inputs(copy, inputs[..]);
    if mixed == 0 || inputs[0] != tail || inputs[1] != history { return 0; }
    fg143_graph_inputs(conv, inputs[..]);
    let weights = inputs[1];
    if inputs[0] != concat || weights == 0 { return 0; }
    var p: i64[71] = [0; 71];
    graph_describe(conv, p[..]);
    let channels = p[30]; let tokens = p[31]; let silu = p[41];
    if channels < 128 || channels > 65535 * 128 || tokens <= 32 || tokens > 65535 * 32 ||
       (silu != 0 && silu != 1) || !routed_dense(packet_tensor(p[..], 30), channels, tokens, 1, 0) ||
       !routed_dense(packet_tensor(p[..], 10), 4, channels, 1, 0) ||
       !routed_dense(packet_tensor(p[..], 0), tokens + 3, channels, 1, 0) ||
       storage_get(tail, STORAGE_VIEW_OFFSET, 0) != (4 * tokens) as u64 { return 0; }
    graph_describe(copy, p[..]);
    let v = packet_tensor(p[..], 0);
    if v.kind != 0 || v.ne[0] != 3 || v.ne[1] != channels || v.ne[2] != 1 || v.ne[3] != 1 ||
       v.nb[0] != 4 || v.nb[1] != 4 * (tokens + 3) ||
       !routed_dense(packet_tensor(p[..], 30), 3, channels, 1, 0) { return 0; }
    graph_describe(concat, p[..]);
    let t = packet_tensor(p[..], 10);
    if p[41] != 0 || !routed_dense(packet_tensor(p[..], 0), 3, channels, 1, 0) ||
       t.kind != 0 || t.ne[0] != tokens || t.ne[1] != channels || t.ne[2] != 1 || t.ne[3] != 1 ||
       t.nb[0] != 4 * channels || t.nb[1] != 4 { return 0; }
    let x = storage_get(mixed, STORAGE_DATA, 0);
    let h = storage_get(history, STORAGE_DATA, 0);
    let w = storage_get(weights, STORAGE_DATA, 0);
    let y = storage_get(conv, STORAGE_DATA, 0);
    if x == 0 || h == 0 || w == 0 || y == 0 || x != storage_get(transposed, STORAGE_DATA, 0) ||
       h != storage_get(copy, STORAGE_DATA, 0) { return 0; }
    let plan: i64[7] = [h as! i64, x as! i64, w as! i64, y as! i64, channels, tokens, silu];
    // Use the earlier step to keep the projection live until both launches.
    return direct_ssm(plan[..], step, deadlines[step + 3]);
}

fn try_glu_pack(nodes: GraphNodes, step: i64, deadlines: const i64[:], glu: u64) -> bool {
    if step + 1 >= deadlines.len || deadlines[step] != step + 1 ||
       storage_get(glu, STORAGE_VIEW, 0) != 0 { return false; }
    let down = graph_node(nodes, step + 1);
    if graph_prop(down, 2) != 30 { return false; }
    var p: i64[71] = [0; 71];
    graph_describe(down, p[..]);
    let tokens = p[12]; let kind = p[8];
    if tokens < 512 || tokens > 4096 || (kind != 12 && kind != 14) ||
       !routed_dense(packet_tensor(p[..], 10), 512, 8, tokens, 0) ||
       !routed_dense(packet_tensor(p[..], 30), 2048, 8, tokens, 0) ||
       !routed_dense(packet_tensor(p[..], 20), 8, tokens, 1, 26) ||
       p[0] != 512 || p[1] != 2048 || p[2] != 256 || p[3] != 1 { return false; }
    var s: u64[10] = [0; 10];
    fg143_graph_inputs(down, s[..]);
    let down_w = s[0]; let ids = s[2];
    if down_w == 0 || ids == 0 || s[1] != glu { return false; }
    let out = storage_get(down, STORAGE_DATA, 0);
    graph_describe(glu, p[..]);
    if p[41] != 2 || p[42] != 0 || p[53] != 0 ||
       !routed_dense(packet_tensor(p[..], 0), 512, 8, tokens, 0) ||
       !routed_dense(packet_tensor(p[..], 10), 512, 8, tokens, 0) { return false; }
    fg143_graph_inputs(glu, s[..]);
    let gate = s[0]; let up = s[1];
    if gate == 0 || up == 0 || graph_prop(gate, 2) != 30 ||
       graph_prop(up, 2) != 30 { return false; }
    if step < 2 || deadlines[step - 2] != step || deadlines[step - 1] != step { return false; }
    let first = graph_node(nodes, step - 2); let second = graph_node(nodes, step - 1);
    if !((first == gate && second == up) || (first == up && second == gate)) { return false; }
    let a = storage_get(gate, STORAGE_DATA, 0);
    let b = storage_get(up, STORAGE_DATA, 0);
    let input = storage_get(glu, STORAGE_DATA, 0);
    let size = 4 * 512 * 8 * tokens;
    if out == 0 || a == 0 || b == 0 || input == 0 ||
       activation_spans_overlap(out, (4 * 2048 * 8 * tokens) as u64, a, size as u64) ||
       activation_spans_overlap(out, (4 * 2048 * 8 * tokens) as u64, b, size as u64) { return false; }
    fg143_graph_inputs(gate, s[..]);
    let gate_w = s[0];
    if gate_w == 0 || s[2] != ids { return false; }
    fg143_graph_inputs(up, s[..]);
    let up_w = s[0];
    if up_w == 0 || s[2] != ids { return false; }
    let plan: i64[8] = [storage_get(up_w, STORAGE_DATA, 0) as! i64,
        storage_get(gate_w, STORAGE_DATA, 0) as! i64, storage_get(down_w, STORAGE_DATA, 0) as! i64,
        a as! i64, b as! i64, input as! i64, size, kind];
    return offer_glu_pack(plan[..]);
}

fn try_rms_scale(nodes: GraphNodes, step: i64, deadlines: const i64[:], rms: u64) -> i64 {
    if step + 1 >= deadlines.len || deadlines[step] != step + 1 ||
       storage_get(rms, STORAGE_VIEW, 0) != 0 { return 0; }
    let scale = graph_node(nodes, step + 1);
    if graph_prop(scale, 2) != 32 || storage_get(scale, STORAGE_VIEW, 0) != 0 { return 0; }
    var inputs: u64[10] = [0; 10];
    fg143_graph_inputs(scale, inputs[..]);
    if inputs[0] != rms { return 0; }
    var p: i64[71] = [0; 71];
    graph_describe(scale, p[..]);
    let width = p[30]; let heads = p[31]; let tokens = p[32];
    let factor = from_bits(p[41] as! u32);
    if p[42] != 0 || !(factor > 0.0 && factor <= 1.0) ||
       width != 128 || heads < 1 || heads > 64 || tokens < 64 || tokens > 4096 ||
       !routed_dense(packet_tensor(p[..], 30), width, heads, tokens, 0) ||
       !routed_dense(packet_tensor(p[..], 0), width, heads, tokens, 0) { return 0; }
    let out = storage_get(scale, STORAGE_DATA, 0);
    graph_describe(rms, p[..]);
    let inp = p[57] as! u64;
    let x = packet_tensor(p[..], 0);
    let eps = from_bits(p[41] as! u32);
    let words = width * heads * tokens;
    if p[55] != 0 || inp == 0 || out == 0 || (inp | out) % 4 != 0 ||
       !(eps >= 0.0 && eps <= 1.0) ||
       x.ne[0] != width || x.ne[1] != heads || x.ne[2] != tokens || x.ne[3] != 1 ||
       x.nb[0] != 4 || !cv_layout(x, x.bytes) ||
       activation_spans_overlap(inp, x.bytes as u64, out, (words * 4) as u64) { return 0; }
    var cfg: i64[26] = [0; 26];
    cfg[0] = CV_RMS; cfg[1] = words;
    for i in 4 {
        cfg[2 + i] = x.ne[i]; cfg[6 + i] = x.ne[i]; cfg[10 + i] = x.nb[i];
    }
    cfg[23] = 16 | (if x.bytes > words * 4 { 1 } else { 0 });
    cfg[24] = heads * tokens; cfg[25] = 32;
    return rms_scale(cfg[..], inp, x.bytes, out, words * 4, step, deadlines[step + 1], eps, factor);
}

fn try_fwht_q4(nodes: GraphNodes, step: i64, deadlines: const i64[:], fwht: u64) -> i64 {
    if step + 2 >= deadlines.len || deadlines[step] != step + 2 ||
       deadlines[step + 1] > step + 2 || storage_get(fwht, STORAGE_VIEW, 0) != 0 { return 0; }
    let view = graph_node(nodes, step + 1);
    let update = graph_node(nodes, step + 2);
    if graph_prop(view, 2) != 37 || graph_prop(update, 2) != 42 ||
       storage_get(view, STORAGE_VIEW, 0) != fwht || storage_get(view, STORAGE_VIEW_OFFSET, 0) != 0 { return 0; }
    var inputs: u64[10] = [0; 10];
    fg143_graph_inputs(update, inputs[..]);
    if inputs[0] != view || inputs[1] == 0 { return 0; }
    let ids_tensor = inputs[1];
    let target = storage_get(update, STORAGE_DATA, 0);
    var p: i64[71] = [0; 71];
    graph_describe(update, p[..]);
    let width = p[0]; let rows = p[1]; let capacity = p[31];
    if width <= 0 || width > 65536 || width % 32 != 0 || rows < 64 || rows > 4096 ||
       !routed_dense(packet_tensor(p[..], 0), width, rows, 1, 0) ||
       p[38] != 2 || p[30] != width || p[32] != 1 || p[33] != 1 ||
       p[34] != 18 || p[35] != width / 32 * 18 || capacity < rows || target == 0 { return 0; }
    let stride = p[35];
    let target_bytes = p[39];
    let id_shape = packet_tensor(p[..], 10);
    if (id_shape.kind != 26 && id_shape.kind != 27) || id_shape.ne[0] != rows ||
       id_shape.ne[1] != 1 || id_shape.ne[2] != 1 || id_shape.ne[3] != 1 ||
       !dense_scalar(id_shape, if id_shape.kind == 27 { 8 } else { 4 }) { return 0; }
    graph_describe(fwht, p[..]);
    let rotation = p[42];
    let x = packet_tensor(p[..], 0);
    let input = p[57] as! u64;
    if p[41] != 0x46574854 || (rotation != 64 && rotation != 256) || width % rotation != 0 ||
       x.kind != 0 || x.ne[0] % rotation != 0 || !dense_scalar(x, 4) ||
       x.bytes != 4 * width * rows || p[39] != x.bytes || input == 0 { return 0; }
    let ids = storage_get(ids_tensor, STORAGE_DATA, 0);
    if ids == 0 || id_shape.bytes != rows * (if id_shape.kind == 27 { 8 } else { 4 }) { return 0; }
    if activation_read_address(ids, id_shape.bytes) != 0 { return -1; }
    let first = fg143_read_contiguous_id(ids, 0, id_shape.kind == 27);
    if first < 0 || first > capacity - rows || (first + rows) > target_bytes / stride { return 0; }
    for i in rows {
        if fg143_read_contiguous_id(ids, i, id_shape.kind == 27) != first + i { return 0; }
    }
    return fwht_q4_rows(input, target + ((first * stride) as u64), width, rows, rotation as i32, 1.0 / sqrt32(rotation as f32));
}

fn execute_graph_node(tensor: u64, work: GraphWorkspace &, selective: bool, step: i64, deadline: i64) -> bool {
    if tensor == 0 { return false; }
    let op = graph_prop(tensor, 2);
    if op == 0 || op == 36 || op == 37 || op == 38 || op == 39 {
        return true;
    }
    let address = storage_get(tensor, STORAGE_DATA, 0);
    if address == 0 { return false; }
    var sources: u64[10] = [0; 10];
    fg143_graph_inputs(tensor, sources[..]);
    let fresh = storage_get(tensor, STORAGE_VIEW, 0) == 0;
    let candidate = selective && fresh &&
        (op == 29 || op == 30 || op == 25 || op == 7 ||
         op == 2 || op == 8 || op == 35 || op == 32 || op == 91 || op == 48);
    var packet: i64[71] = [0; 71];
    if candidate { graph_describe(tensor, packet[..]); }
    let bytes = if candidate { packet[39] as! u64 } else { storage_bytes(tensor) };
    if bytes > 0x7fffffffffffffff { return false; }
    var overwrite = candidate && goose_tensor_dense(packet[38], packet[30 .. 38]);
    for i in sources.len {
        let t = sources[i];
        if t == 0 { continue; }
        let src = storage_get(t, STORAGE_DATA, 0);
        if src == 0 && !graph_empty(t) { return false; }
        if overwrite {
            let source_bytes = if i < 3 { packet[9 + 10 * i] as! u64 }
                else if i == 3 { packet[68] as! u64 } else { storage_bytes(t) };
            if activation_spans_overlap(address, bytes, src, source_bytes) { overwrite = false; }
        }
    }
    if graph_output_prepare(address, bytes as i64, fresh, overwrite, step, deadline) != 0 { return false; }
    if op == 86 { return graph_gdn(tensor); }
    if op == 77 { return graph_scan(tensor); }

    var sizes: i64[4] = [0; 4];
    if !candidate { graph_describe(tensor, packet[..]); }
    if !graph_regular(tensor, packet[..], 1, sizes[..], 0) { return false; }
    let required = sizes[3];
    if required > work.capacity {
        let next = graph_resize(work.address, required);
        if next == 0 { return false; }
        work.address = next;
        work.capacity = required;
    }
    return graph_regular(tensor, packet[..], 0, sizes[..], work.address);
}

// Return one plus the first failing node index, or zero after the last node.
extern "fg_cuda_moe_epilogue_enabled" fn moe_epilogue_enabled() -> bool;
extern "fg_cuda_moe_epilogue" fn moe_epilogue(routed: u64, shared: u64, gate: u64, residual: u64,
    output: u64, rows: i64, step: i64, deadline: i64) -> i64;

fn try_moe_epilogue(nodes: GraphNodes, step: i64, deadlines: const i64[:], mul: u64) -> i64 {
    if step + 2 >= deadlines.len || deadlines[step] != step + 1 || deadlines[step + 1] != step + 2 {
        return 0;
    }
    let mix = graph_node(nodes, step + 1);
    let sum = graph_node(nodes, step + 2);
    if graph_prop(mix, 2) != 2 || graph_prop(sum, 2) != 2 ||
       storage_get(mul, STORAGE_VIEW, 0) != 0 || storage_get(mix, STORAGE_VIEW, 0) != 0 ||
       storage_get(sum, STORAGE_VIEW, 0) != 0 { return 0; }

    var inputs: u64[10] = [0; 10];
    fg143_graph_inputs(mix, inputs[..]);
    if inputs[1] != mul { return 0; }
    fg143_graph_inputs(sum, inputs[..]);
    if inputs[0] != mix { return 0; }

    var p: i64[71] = [0; 71];
    graph_describe(mul, p[..]);
    let rows = p[31];
    if rows < 2 || rows > 4096 ||
       !routed_dense(packet_tensor(p[..], 0), 2048, rows, 1, 0) ||
       !routed_dense(packet_tensor(p[..], 10), 1, rows, 1, 0) ||
       !routed_dense(packet_tensor(p[..], 30), 2048, rows, 1, 0) { return 0; }
    let shared = p[57] as! u64;
    let gate = p[70] as! u64;

    graph_describe(mix, p[..]);
    if !routed_dense(packet_tensor(p[..], 0), 2048, rows, 1, 0) ||
       !routed_dense(packet_tensor(p[..], 30), 2048, rows, 1, 0) { return 0; }
    let routed = p[57] as! u64;

    graph_describe(sum, p[..]);
    if !routed_dense(packet_tensor(p[..], 10), 2048, rows, 1, 0) ||
       !routed_dense(packet_tensor(p[..], 30), 2048, rows, 1, 0) { return 0; }
    let output = p[58] as! u64;
    let residual = p[70] as! u64;
    let bytes = (rows * 8192) as u64;
    if (routed | shared | residual | output) % 16 != 0 ||
       routed == 0 || shared == 0 || gate == 0 || residual == 0 || output == 0 ||
       activation_spans_overlap(output, bytes, routed, bytes) ||
       activation_spans_overlap(output, bytes, shared, bytes) ||
       activation_spans_overlap(output, bytes, residual, bytes) ||
       activation_spans_overlap(output, bytes, gate, (rows * 4) as u64) { return 0; }
    return moe_epilogue(routed, shared, gate, residual, output, rows, step, deadlines[step + 2]);
}

extern "fg_cuda_activation_graph_begin" fn graph_activation_begin(planned: bool) -> i32;
extern "fg143_graph_restore_affinity" fn graph_restore_affinity(previous: u64) -> bool;
extern "fg143_graph_collect" fn graph_collect(graph: u64, failed: i64, counters: u64[:]);
extern "fg_cuda_state_graph_done" fn graph_state_done() -> i32;
extern "fg143_graph_publish" fn graph_publish(output: u64, valid: bool, all: bool, counters: u64[:]) -> i32;
extern "fg143_graph_log_enabled" fn graph_log_enabled() -> bool;
extern "fg143_graph_take_cpu_values" fn graph_take_cpu_values() -> u64;
extern "fg143_graph_log_count" fn graph_log_count(label: const u8[:], count: u64, unit: const u8[:]);
extern "fg143_graph_log_pair" fn graph_log_pair(label: const u8[:], count: u64,
    unit: const u8[:], next: u64, next_unit: const u8[:]);

fn graph_log_nonzero(label: const u8[:], count: u64, unit: const u8[:]) {
    if count != 0 { graph_log_count(label, count, unit); }
}

fn graph_log_counters(counters: const u64[:]) {
    graph_log_count("Goose CPU graph", graph_take_cpu_values(), " values");
    graph_log_nonzero("Goose CUDA graph", counters[0], " matrices");
    graph_log_nonzero("Goose CUDA F32 matrix", counters[1], " nodes");
    graph_log_nonzero("Goose CUDA matrix launches", counters[2], "");
    graph_log_nonzero("Goose CUDA cached expert projections", counters[3], "");
    if counters[4] != 0 {
        graph_log_pair("Goose CUDA routed FFN", counters[4], " groups", counters[5], " graph replays");
    }
    if counters[6] != 0 {
        graph_log_pair("Goose CUDA full FFN", counters[6], " groups", counters[7], " graph replays");
    }
    graph_log_nonzero("Goose CUDA MMQ projections", counters[8], " nodes");
    if counters[9] != 0 {
        graph_log_pair("Goose CUDA MMQ Stream-K", counters[9], " windows", counters[10], " fixups");
    }
    graph_log_nonzero("Goose CUDA dense MMA", counters[11], " nodes");
    graph_log_nonzero("Goose CUDA attention", counters[12], " nodes");
    graph_log_nonzero("Goose CUDA Flash MMA", counters[13], " nodes");
    graph_log_nonzero("Goose CUDA causal attention", counters[14], " nodes");
    graph_log_nonzero("Goose CUDA GDN", counters[15], " nodes");
    graph_log_nonzero("Goose CUDA GDN MMA", counters[16], " nodes");
    graph_log_nonzero("Goose CUDA resident KV", counters[17], " nodes");
    graph_log_nonzero("Goose CUDA resident GDN", counters[18], " nodes");
    graph_log_nonzero("Goose CUDA resident Q4 writes", counters[19], " nodes");
    graph_log_nonzero("Goose CUDA vector", counters[20], " nodes");
    graph_log_nonzero("Goose CUDA KV FWHT", counters[21], " nodes");
    graph_log_nonzero("Goose CUDA MoE weighted reduction", counters[22], " tiles");
    graph_log_nonzero("Goose CUDA overwritten downloads skipped", counters[34], " bytes");
    graph_log_nonzero("Goose CUDA dirty activations retired", counters[35], " bytes");
    graph_log_nonzero("Goose CUDA RoPE", counters[23], " nodes");
    graph_log_nonzero("Goose CUDA SSM convolution", counters[24], " nodes");
    graph_log_nonzero("Goose CUDA softmax", counters[25], " nodes");
    graph_log_nonzero("Goose CUDA sort", counters[26], " nodes");
    graph_log_nonzero("Goose CUDA gather", counters[27], " nodes");
    graph_log_nonzero("Goose CUDA row sum", counters[28], " nodes");
    graph_log_nonzero("Goose CUDA normalized gather", counters[29], " nodes");
    graph_log_nonzero("Goose CUDA concat", counters[30], " nodes");
    graph_log_nonzero("Goose CUDA contiguous copy", counters[31], " nodes");
    graph_log_nonzero("Goose CUDA resident SSM writes", counters[32], " nodes");
    graph_log_nonzero("Goose CUDA activation reuse", counters[33], " inputs");
}

export fn fg143_graph_execute(graph: u64, output: u64, all: bool, deadlines: const i64[:],
        observe: bool, prefill: bool, previous_affinity: u64) -> i32 {
    let started = gather_affinity(prefill, 0) == 0 && graph_activation_begin(deadlines.len != 0) == 0;
    let failed = if started { fg143_graph_run(graph, !all, deadlines, observe) } else { -1 };
    let restored = if previous_affinity == 0 { true } else { graph_restore_affinity(previous_affinity) };
    if !started { return -1; }
    var counters: u64[36] = [0; 36];
    graph_collect(graph, failed, counters[..]);
    let valid = failed == 0 && graph_state_done() == 0 && restored;
    let published = graph_publish(output, valid, all, counters[..]);
    if graph_log_enabled() { graph_log_counters(counters[..]); }
    return if valid && published == 0 { 0 } else { -1 };
}

export fn fg143_graph_run(graph: u64, selective: bool, deadlines: const i64[:], observe: bool) -> i64 {
    if graph == 0 { return -1; }
    let count = fg143_graph_nodes(graph);
    let nodes = GraphNodes { address: tensor_read_word(graph + GRAPH_NODES), count: count };
    if deadlines.len != 0 && (deadlines.len != count || !selective) { return -1; }
    var work = GraphWorkspace { address: 0, capacity: 0 };
    var outcome: i64 = 0;
    let use_routed_graph = !observe && deadlines.len != 0 && routed_graph_enabled();
    let use_certified_dsa = !observe && deadlines.len != 0 && certified_dsa_enabled();
    let use_moe_epilogue = !observe && deadlines.len != 0 && cpu_prefill() && moe_epilogue_enabled();
    var i: i64 = 0;
    var pointers: u64[16] = [0; 16];
    var batch_start: i64 = -16;
    while i < count {
        if i >= batch_start + 16 {
            batch_start = i;
            graph_pointer_batch(nodes, batch_start, pointers[..]);
        }
        let step = if deadlines.len == 0 { -1 } else { i };
        let deadline = if deadlines.len == 0 { -1 } else { deadlines[i] };
        let tensor = pointers[i - batch_start];
        if use_certified_dsa && graph_prop(tensor, 2) == 87 {
            let fused = try_giga_dsa_graph(nodes, i, deadlines, tensor);
            if fused > 0 { i += fused; continue; }
            if fused < 0 { outcome = i + 1; break; }
        }
        if use_moe_epilogue && graph_prop(tensor, 2) == 7 {
            let fused = try_moe_epilogue(nodes, i, deadlines, tensor);
            if fused > 0 { i += 3; continue; }
            if fused < 0 { outcome = i + 1; break; }
        }
        if !observe && deadlines.len != 0 && graph_prop(tensor, 2) == 95 && fwht_q4_enabled() {
            let fused = try_fwht_q4(nodes, i, deadlines, tensor);
            if fused > 0 { i += 3; continue; }
            if fused < 0 { outcome = i + 1; break; }
        }
        if !observe && deadlines.len != 0 && graph_prop(tensor, 2) == 25 {
            let fused = try_rms_scale(nodes, i, deadlines, tensor);
            if fused > 0 { i += 2; continue; }
            if fused < 0 { outcome = i + 1; break; }
        }
        if !observe && deadlines.len != 0 && graph_prop(tensor, 2) == 100 &&
           try_glu_pack(nodes, i, deadlines, tensor) {
            // Keep both operands alive at the SwiGLU step until packing finishes.
            let ok = execute_graph_node(graph_node(nodes, i + 1), work, selective, i, deadlines[i + 1]);
            let used = close_glu_pack();
            if !ok || !used { outcome = i + 1; break; }
            i += 2;
            continue;
        }
        if !observe && deadlines.len != 0 && graph_prop(tensor, 2) == 22 && direct_ssm_enabled() {
            let grouped = try_direct_ssm(nodes, i, deadlines, tensor);
            if grouped > 0 { i += 4; continue; }
            if grouped < 0 { outcome = i + 1; break; }
        }
        if use_routed_graph && graph_prop(tensor, 2) == 30 {
            let grouped = try_routed_graph(nodes, i, deadlines);
            if grouped > 0 { i += grouped; continue; }
            if grouped < 0 { outcome = i + 1; break; }
        }
        if !execute_graph_node(tensor, work, selective, step, deadline) || (observe && !graph_observe(tensor)) {
            outcome = i + 1;
            break;
        }
        i += 1;
    }
    if work.address != 0 { graph_resize(work.address, 0); }
    return outcome;
}

extern "fg143_storage_slot" fn storage_slot(graph: u64, tensor: u64) -> i64;
extern "fg143_storage_ram_available" fn storage_ram_available() -> u64;
extern "fg143_storage_ram_error" fn storage_ram_error(required: u64, available: u64);
extern "fg143_storage_report" fn storage_report(bytes: u64, blocks: i64, reused: i64);

fn storage_keep(graph: u64, tensor: u64, until: i64, lifetimes: i64[:]) -> bool {
    if tensor == 0 { return true; }
    let parent = storage_get(tensor, STORAGE_VIEW, 0);
    let owner = if parent == 0 { tensor } else { parent };
    if storage_get(owner, STORAGE_DATA, 0) != 0 { return true; }
    let slot = storage_slot(graph, owner);
    if slot < 0 || slot >= lifetimes.len || lifetimes[slot] < 0 { return false; }
    lifetimes[slot] = high(lifetimes[slot], until);
    return true;
}

// A buffer can be reused only after the last node that reads any of its
// views. Outputs and inputs that meet in the same node use disjoint blocks.
export fn fg143_storage_allocate_output(graph: u64, output: u64, deadlines: i64[:]) -> u64 {
    if graph == 0 { return 0; }
    let count = fg143_storage_count(graph);
    let slots = fg143_storage_hash_size(graph);
    let leaves = count - fg143_graph_nodes(graph);
    if count < 0 || leaves < 0 || slots <= 0 || deadlines.len != count - leaves { return 0; }
    let align = fg143_storage_alignment();
    let mask = align - 1;
    let aligned = STORAGE_MAX - mask;
    var lifetimes: i64[>..] = [];
    var placements: u64[>..] = [];
    lifetimes.resize(slots, -1);
    placements.resize(slots, 0);
    for i in count {
        let t = fg143_storage_tensor(graph, i);
        if t == 0 { return 0; }
        let parent = storage_get(t, STORAGE_VIEW, 0);
        if parent != 0 {
            if !storage_view_valid(graph, t, parent, count) { return 0; }
        } else if storage_get(t, STORAGE_DATA, 0) == 0 {
            let slot = storage_slot(graph, t);
            if slot < 0 || slot >= slots { return 0; }
            lifetimes[slot] = i;
        }
    }
    var sources: u64[10] = [0; 10];
    for i in count {
        let t = fg143_storage_tensor(graph, i);
        if !storage_keep(graph, t, i, lifetimes[..]) { return 0; }
        if graph_prop(t, 2) == 0 { continue; }
        fg143_graph_inputs(t, sources[..]);
        for j in 10 {
            if !storage_keep(graph, sources[j], i, lifetimes[..]) { return 0; }
        }
    }
    if !storage_keep(graph, output, count, lifetimes[..]) { return 0; }

    var block_size: u64[>..] = [];
    var block_offset: u64[>..] = [];
    var block_until: i64[>..] = [];
    block_size.resize(count, 0);
    block_offset.resize(count, 0);
    block_until.resize(count, 0);
    var blocks: i64 = 0;
    var reused: i64 = 0;
    var needed: u64 = 0;
    for i in count {
        let t = fg143_storage_tensor(graph, i);
        if storage_get(t, STORAGE_VIEW, 0) != 0 || storage_get(t, STORAGE_DATA, 0) != 0 { continue; }
        let bytes = storage_bytes(t);
        if bytes > aligned { return 0; }
        let padded = (bytes + mask) & aligned;
        var fit: i64 = -1;
        for j in blocks {
            if block_until[j] < i && block_size[j] >= padded &&
               (fit < 0 || block_size[j] < block_size[fit]) {
                fit = j;
                if block_size[j] == padded { break; }
            }
        }
        if fit < 0 {
            if needed > STORAGE_MAX - padded { return 0; }
            fit = blocks;
            blocks += 1;
            block_size[fit] = padded;
            block_offset[fit] = needed;
            needed += padded;
        } else { reused += 1; }
        let slot = storage_slot(graph, t);
        block_until[fit] = lifetimes[slot];
        placements[slot] = block_offset[fit];
    }
    let available = storage_ram_available();
    if available == 0 || needed > available {
        storage_ram_error(needed, available);
        return 0;
    }
    let storage = storage_alloc(if needed == 0 { align } else { needed });
    if storage == 0 {
        storage_ram_error(needed, available);
        return 0;
    }
    storage_report(needed, blocks, reused);
    for i in count {
        let t = fg143_storage_tensor(graph, i);
        if storage_get(t, STORAGE_VIEW, 0) == 0 && storage_get(t, STORAGE_DATA, 0) == 0 {
            let slot = storage_slot(graph, t);
            storage_set(t, STORAGE_DATA, 0, storage + placements[slot]);
            if i >= leaves { deadlines[i - leaves] = lifetimes[slot] - leaves; }
        }
    }
    for i in count {
        let t = fg143_storage_tensor(graph, i);
        let parent = storage_get(t, STORAGE_VIEW, 0);
        if parent != 0 && storage_get(t, STORAGE_DATA, 0) == 0 {
            storage_set(t, STORAGE_DATA, 0,
                storage_get(parent, STORAGE_DATA, 0) + storage_get(t, STORAGE_VIEW_OFFSET, 0));
        }
    }
    return storage;
}

fn packet_tensor(packet: const i64[:], at: i64) -> Tensor {
    var t: Tensor = default <Tensor>();
    for axis in 4 {
        t.ne[axis] = packet[at + axis];
        t.nb[axis] = packet[at + 4 + axis];
    }
    t.kind = packet[at + 8];
    t.bytes = packet[at + 9];
    return t;
}

extern "fg_cuda_giga_dsa_mask" fn cuda_giga_dsa_mask(mask: const u8[:], picks: const u8[:],
    out: u8[:], cells: i64, count: i64, rows: i64, prefix: i64) -> i64;

extern "fg_cuda_dsa_score" fn cuda_dsa_score(config: i64[:], q: const u8[:], k: const u8[:],
    weights: const u8[:], masks: const u8[:], dst: u8[:]) -> i64;

fn dispatch_dsa_score(p: const Node &, mask: const Tensor &, q: const u8[:], k: const u8[:],
                      weights: const u8[:], masks: const u8[:], dst: u8[:]) -> bool {
    let queries = p.x.ne[2];
    let cells = p.y.ne[2];
    if queries < 1 || queries > 4096 || cells < 1 || cells > 1048576 ||
       p.x.kind != 0 || p.y.kind != 1 || p.z.kind != 0 || mask.kind != 1 || p.d.kind != 0 ||
       p.x.ne[0] != 128 || p.x.ne[1] != 64 || p.x.ne[3] != 1 ||
       p.x.nb[0] != 4 || p.x.nb[1] != 512 || p.x.nb[2] != 32768 ||
       p.y.ne[0] != 128 || p.y.ne[1] != 1 || p.y.ne[3] != 1 ||
       p.y.nb[0] != 2 || p.y.nb[2] < 256 || p.y.nb[2] % 2 != 0 ||
       p.z.ne[0] != 64 || p.z.ne[1] != queries || p.z.ne[2] != 1 || p.z.ne[3] != 1 ||
       p.z.nb[0] != 4 || p.z.nb[1] != 256 ||
       mask.ne[0] != cells || mask.ne[1] != queries || mask.ne[2] != 1 || mask.ne[3] != 1 ||
       mask.nb[0] != 2 || mask.nb[1] < 2 * cells || mask.nb[1] % 2 != 0 ||
       p.d.ne[0] != cells || p.d.ne[1] != queries || p.d.bytes != 4 * cells * queries ||
       p.x.bytes > q.len || p.y.bytes > k.len || p.z.bytes > weights.len ||
       mask.bytes > masks.len || p.d.bytes > dst.len { return false; }
    var config: i64[6] = [cells, queries, p.y.nb[2], p.x.nb[2], p.z.nb[1], mask.nb[1]];
    return cuda_dsa_score(config, q[0 .. p.x.bytes], k[0 .. p.y.bytes],
        weights[0 .. p.z.bytes], masks[0 .. mask.bytes], dst[0 .. p.d.bytes]) == 1;
}

extern "fg_cuda_giga_dsa_top_wide" fn cuda_giga_dsa_top_wide(x: const u8[:], dst: u8[:], cells: i64, rows: i64) -> i64;


extern "fg_cuda_giga_kv_write_enabled" fn giga_kv_write_enabled() -> bool;

export fn fg143_node_run(packet: const i64[:], mode: i64,
    x: const u8[:], y: const u8[:], z: const u8[:], mask: const u8[:], dst: u8[:],
    scratch: f32[:], indices: i64[:]) -> bool {
    let gxx = CPU_IQ2_XXS[..];
    let gxs = CPU_IQ2_XS[..];
    let gs = CPU_IQ2_S[..];
    let g3x = CPU_IQ3_XXS[..];
    let g3s = CPU_IQ3_S[..];
    let g1 = CPU_IQ1_S[..];
    let signs = CPU_IQ_SIGNS[..];
    let palette = CPU_IQ4_PALETTE[..];
    if packet.len != 71 { return false; }
    var p: Node = default <Node>();
    p.x = packet_tensor(packet, 0);
    p.y = packet_tensor(packet, 10);
    p.z = packet_tensor(packet, 20);
    p.d = packet_tensor(packet, 30);
    p.op = packet[40];
    p.nth = 1;
    for i in 16 { p.ip[i] = packet[41 + i]; }
    for i in 8 { p.fp[i] = from_bits(p.ip[i] as! u32); }
    // GGML stores the original kernel parameters, while the CPU kernels take
    // normalized dimensions, operand flags and floating-point parameters.
    if p.op == 48 || p.op == 49 {
        p.ip[0] = packet[42];
        p.ip[1] = packet[43];
        p.ip[2] = packet[45];
        p.ip[3] = packet[56];
        for i in 4 { p.ip[4 + i] = packet[52 + i]; }
        p.ip[8] = if p.z.bytes > 0 { 1 } else { 0 };
        for i in 6 { p.fp[i] = from_bits(packet[46 + i] as! u32); }
    } else if p.op == 46 {
        p.ip[0] = if p.y.bytes > 0 { 1 } else { 0 };
        p.ip[1] = if p.z.bytes > 0 { 1 } else { 0 };
    } else if p.op == 70 {
        p.ip[0] = 1;
    } else if p.op == 100 {
        p.ip[2] = if p.y.bytes > 0 { 1 } else { 0 };
        p.fp[0] = from_bits(packet[43] as! u32);
        p.fp[1] = from_bits(packet[44] as! u32);
    } else if p.op == 91 {
        for i in 4 { p.fp[i] = from_bits(packet[42 + i] as! u32); }
    }
    if mode == 1 { p.ip[15] = 1464816203; }
    if p.op == 95 && p.ip[0] == 0x46574854 {
        return mode == 1 || kv_fwht(p, x, dst);
    }
    if p.op == 95 && p.ip[0] == 0x4453414d {
        if mode == 1 { return true; }
        let cells = p.x.ne[0];
        let count = p.y.ne[0];
        let rows = p.x.ne[1];
        let prefix = rows - p.y.ne[1];
        if p.x.kind != 1 || p.y.kind != 26 || p.d.kind != 1 ||
            !dense_scalar(p.x, 2) || !dense_scalar(p.y, 4) || !dense_scalar(p.d, 2) ||
            !same_shape(p.x, p.d) || prefix < 0 || p.y.ne[1] < 1 ||
            p.x.ne[2] != 1 || p.x.ne[3] != 1 || p.y.ne[2] != 1 || p.y.ne[3] != 1 ||
            cells <= 0 || count <= 0 || count > cells || rows <= 0 ||
            p.x.bytes > x.len || p.y.bytes > y.len || p.d.bytes > dst.len { return false; }
        let device = cuda_giga_dsa_mask(x[0..p.x.bytes], y[0..p.y.bytes], dst[0..p.d.bytes], cells, count, rows, prefix);
        if device != 0 { return device > 0; }
        if activation_read(x) != 0 || activation_read(y) != 0 { return false; }
        for word in mul_size(cells, rows) {
            let at = 2 * word;
            write16(dst, at, if word < prefix * cells { read16(x, at) } else { 0xfc00 });
        }
        for row in p.y.ne[1] {
            for j in count {
                let key = (read32(y, 4 * (row * count + j)) as! i32) as i64;
                if key < 0 || key >= cells { return false; }
                let at = 2 * ((row + prefix) * cells + key);
                write16(dst, at, read16(x, at));
            }
        }
        return true;
    }
    if p.op == 87 {
        if mode == 1 { return true; }
        let mask_shape = packet_tensor(packet, 59);
        return dispatch_dsa_score(p, mask_shape, x, y, z, mask, dst);
    }
    if mode == 0 && p.op == 70 && p.ip[13] == 1 && p.nth == 1 && p.x.kind == 0 && p.d.kind == 26 &&
        p.x.ne[0] > 4096 && p.d.ne[0] == 2048 && p.x.ne[1] == p.d.ne[1] &&
        p.x.ne[2] == 1 && p.x.ne[3] == 1 && p.d.ne[2] == 1 && p.d.ne[3] == 1 &&
        dense_scalar(p.x, 4) && dense_scalar(p.d, 4) && p.x.bytes <= x.len && p.d.bytes <= dst.len {
        let wide = cuda_giga_dsa_top_wide(x[0..p.x.bytes],dst[0..p.d.bytes],p.x.ne[0],p.x.ne[1]);
        if wide != 0 { return wide > 0; }
    }
    if p.op == 95 && p.ip[0] == 0x4d4f4557 {
        return mode == 1 || moe_weighted(p, x, y, dst);
    }
    if p.op == 95 && p.ip[0] == 0x4d4f454e {
        return mode == 1 || moe_normalize(p, x, y, dst);
    }
    if mode == 0 && p.op == 74 {
        if packet[69] != 0 { return false; }
        let mask_shape = packet_tensor(packet, 59);
        return flash_attention(p, mask_shape, x, y, z, mask, dst, palette);
    }
    if mode == 0 && p.op == 42 && p.d.kind == 2 {
        return set_quantized_rows(p, packet[57] as! u64, packet[58] as! u64, y, indices);
    }
    let addresses: i64[3] = [packet[57], packet[70], packet[58]];
    let ok = entry(p, x, y, z, dst, scratch, indices, addresses, gxx, gxs, gs, g3x, g3s, g1, signs, palette);
    if !ok || mode != 0 || p.op != 42 || p.d.kind != 1 || !giga_kv_write_enabled() {
        return ok;
    }
    let width = p.d.ne[0];
    if (width != 576 && width != 128) || p.d.ne[2] != 1 || p.d.ne[3] != 1 ||
       p.x.ne[2] != 1 || p.x.ne[3] != 1 || p.d.nb[0] != 2 || p.d.nb[1] != width * 2 ||
       p.x.ne[1] <= 0 || addresses[2] == 0 { return false; }
    var first = p.d.ne[1];
    var last: i64 = 0;
    for row in p.x.ne[1] {
        let id = read_index(p.y, y, row);
        if id < 0 || id >= p.d.ne[1] { return false; }
        first = low(first, id);
        last = high(last, id + 1);
    }
    let at = first * p.d.nb[1];
    return cuda_state_write((addresses[2] as! u64) + (at as u64), (last - first) * p.d.nb[1]) == 0;

}

extern "fg_cuda_fwht" fn cuda_fwht(src: const u8[:], dst: u8[:], width: i32, rows: i64, scale: f32) -> i64;

fn kv_fwht(p: const Node &, src: const u8[:], dst: u8[:]) -> bool {
    let width = p.ip[1];
    if width < 64 || width > 0x7fffffff || width > p.x.ne[0] || (width & (width - 1)) != 0 || p.x.ne[0] % width != 0 ||
       p.x.kind != 0 || p.d.kind != 0 || !dense_scalar(p.x, 4) || !dense_scalar(p.d, 4) ||
       src.len != p.x.bytes || src.len != dst.len || src.len <= 0 || src.len % (4 * width) != 0 { return false; }
    let count = src.len / (4 * width);
    let scale = 1.0 / sqrt32(width as f32);
    let device = cuda_fwht(src, dst, width as i32, count, scale);
    if device != 0 { return device > 0; }
    if activation_read(src) != 0 { return false; }
    for i in src.len / 4 {
        let byte = i * 4;
        write32(dst, byte, bits(from_bits(read32(src, byte)) * scale));
    }
    for row in count {
        let start = row * width * 4;
        var step: i64 = 1;
        while step < width {
            for group in width / (2 * step) {
                for lane in step {
                    let a = start + (group * 2 * step + lane) * 4;
                    let b = a + step * 4;
                    let av = from_bits(read32(dst, a));
                    let bv = from_bits(read32(dst, b));
                    write32(dst, a, bits(av + bv));
                    write32(dst, b, bits(av - bv));
                }
            }
            step *= 2;
        }
    }
    return true;
}

extern "fg_cuda_moe_weighted" fn cuda_moe_weighted(x: const u8[:], weights: const u8[:], dst: u8[:],
    width: i64, used: i64, count: i64, tile: i64) -> i64;

fn moe_weighted(p: const Node &, x: const u8[:], w: const u8[:], dst: u8[:]) -> bool {
    let width = p.x.ne[0];
    let used = p.x.ne[1];
    let count = p.x.ne[2];
    if width <= 0 || width > 65535 * 256 || used <= 0 || used > 64 || count <= 0 || count > 0x7fffffff ||
       p.x.ne[3] != 1 || p.y.ne[0] != 1 || p.y.ne[1] != used || p.y.ne[2] != count || p.y.ne[3] != 1 ||
       p.d.ne[0] != width || p.d.ne[1] != count || p.d.ne[2] != 1 || p.d.ne[3] != 1 ||
       p.x.kind != 0 || p.y.kind != 0 || p.d.kind != 0 || !dense_scalar(p.x, 4) || !dense_scalar(p.y, 4) ||
       !dense_scalar(p.d, 4) || x.len != p.x.bytes || w.len != p.y.bytes || dst.len != p.d.bytes { return false; }
    let input_row = width * used * 4;
    let tile = high(1, (16 * 1024 * 1024) / input_row);
    let device = cuda_moe_weighted(x, w, dst, width, used, count, low(count, tile));
    if device != 0 { return device > 0; }
    if activation_read(x) != 0 || activation_read(w) != 0 { return false; }
    for row in count {
        let xbase = row * input_row;
        let wbase = row * used * 4;
        for col in width {
            let byte = col * 4;
            var sum = from_bits(read32(x, xbase + byte)) * from_bits(read32(w, wbase));
            for e in 1 .. used {
                let product = from_bits(read32(x, xbase + e * width * 4 + byte)) * from_bits(read32(w, wbase + e * 4));
                sum += product;
            }
            write32(dst, row * width * 4 + byte, bits(sum));
        }
    }
    return true;
}

fn state_plan(packet: const i64[:]) -> StatePlan {
    var p: StatePlan = default <StatePlan>();
    for i in 7 { p.source[i] = packet_tensor(packet, i * 10); }
    p.dst = packet_tensor(packet, 70);
    p.nth = 1;
    p.op = packet[80];
    p.snapshots = packet[81];
    return p;
}

export fn fg143_gdn_run(packet: const i64[:],
    q: const f32[:], k: const f32[:], v: const f32[:], gate: const f32[:],
    beta: const f32[:], initial: const f32[:], dst: f32[:]) -> bool {
    if (packet.len != 82 && packet.len != 83) || packet[80] != 86 { return false; }
    var p = state_plan(packet);
    p.publish = packet.len == 83 && packet[82] == 1;
    return gdn(p, q, k, v, gate, beta, initial, dst);
}

export fn fg143_state_run(packet: const i64[:],
    a0: const u8[:], a1: const u8[:], a2: const u8[:], a3: const u8[:],
    a4: const u8[:], a5: const u8[:], a6: const u8[:], dst: u8[:]) -> bool {
    if packet.len != 82 { return false; }
    if activation_read(a0) != 0 || activation_read(a1) != 0 || activation_read(a2) != 0 ||
       activation_read(a3) != 0 || activation_read(a4) != 0 || activation_read(a5) != 0 ||
       activation_read(a6) != 0 { return false; }
    let p = state_plan(packet);
    return state_entry(p, a0, a1, a2, a3, a4, a5, a6, dst);
}

extern "qz_quantize_row" fn quantize_row(kind: i64, source: u64, destination: u64, width: i64) -> i64;
extern "fg_cuda_set_q4_rows" fn cuda_set_q4_rows(source: u64, source_bytes: i64, stride: i64,
    destination: u64, destination_bytes: i64, width: i64, count: i64) -> i64;

fn set_quantized_rows(p: const Node &, source: u64, destination: u64, ids: const u8[:], work: i64[:]) -> bool {
    let width = p.x.ne[0];
    if p.d.kind != 2 || p.x.kind != 0 || width <= 0 || width % 32 != 0 ||
       p.d.ne[0] != width || p.x.nb[0] != 4 || p.d.nb[0] != 18 ||
       p.x.ne[2] != p.d.ne[2] || p.x.ne[3] != p.d.ne[3] ||
       (p.y.kind != 26 && p.y.kind != 27) || p.y.ne[0] != p.x.ne[1] ||
       p.y.ne[1] <= 0 || p.y.ne[2] <= 0 || source == 0 || destination == 0 {
        return false;
    }
    validate(p.y, ids.len);
    if activation_read(ids) != 0 { return false; }
    let row_bytes = (width / 32) * 18;
    let count = rows(p.x);
    if work.len / 2 < count { return false; }
    if count == 0 { return true; }
    var packed = p.x.ne[2] == 1 && p.x.ne[3] == 1 && p.x.nb[1] >= width * 4 && p.x.nb[1] % 4 == 0;
    var first = p.d.bytes;
    var last: i64 = 0;
    for row in count {
        let r = row % p.x.ne[1];
        let head = (row / p.x.ne[1]) % p.x.ne[2];
        let batch = row / (p.x.ne[1] * p.x.ne[2]);
        let id = read_index(p.y, ids, r + p.y.ne[0] * ((head % p.y.ne[1]) + p.y.ne[1] * (batch % p.y.ne[2])));
        if id < 0 || id >= p.d.ne[1] { return false; }
        let from = r * p.x.nb[1] + head * p.x.nb[2] + batch * p.x.nb[3];
        let to = id * p.d.nb[1] + head * p.d.nb[2] + batch * p.d.nb[3];
        if from < 0 || width * 4 > p.x.bytes - from || to < 0 || row_bytes > p.d.bytes - to {
            return false;
        }
        if row != 0 && to != last { packed = false; }
        first = low(first, to);
        last = high(last, to + row_bytes);
        work[2 * row] = from;
        work[2 * row + 1] = to;
    }
    let target = destination + (first as u64);
    let touched = last - first;
    if packed {
        let source_bytes = work[2 * (count - 1)] + width * 4;
        let device = cuda_set_q4_rows(source, source_bytes, p.x.nb[1], target, touched, width, count);
        if device != 0 { return device > 0; }
    } else if cuda_state_read(target, touched) != 0 {
        // Preserve the previous GPU bytes in the holes of a sparse update.
        return false;
    }
    if activation_read_address(source, p.x.bytes) != 0 { return false; }
    for row in count {
        let from = source + (work[2 * row] as u64);
        let to = destination + (work[2 * row + 1] as u64);
        if quantize_row(p.d.kind, from, to, width) != row_bytes { return false; }
    }
    return cuda_state_write(target, touched) == 0;
}

extern "fg_cuda_giga_dsa_certified_enabled" fn certified_dsa_enabled() -> bool;
extern "fg_cuda_giga_dsa_certified_address" fn certified_dsa(config: i64[:],
    inputs: const u64[:], sizes: const i64[:], out: u64, out_bytes: i64) -> i64;

fn try_giga_dsa_graph(nodes: GraphNodes, step: i64, deadlines: const i64[:], score: u64) -> i64 {
    if step + 2 >= deadlines.len || deadlines[step] != step + 1 ||
       deadlines[step + 1] != step + 2 || storage_get(score, STORAGE_VIEW, 0) != 0 { return 0; }
    let top = graph_node(nodes, step + 1);
    let scatter = graph_node(nodes, step + 2);
    if graph_prop(top, 2) != 70 || graph_prop(scatter, 2) != 95 ||
       storage_get(top, STORAGE_VIEW, 0) != 0 { return 0; }

    var p: i64[71] = [0; 71];
    var sources: u64[10] = [0; 10];
    fg143_graph_inputs(scatter, sources[..]);
    if sources[1] != top { return 0; }
    graph_describe(scatter, p[..]);
    if p[41] != 0x4453414d { return 0; }

    fg143_graph_inputs(top, sources[..]);
    if sources[0] != score { return 0; }
    graph_describe(top, p[..]);
    if p[54] != 1 { return 0; }
    let out = storage_get(top, STORAGE_DATA, 0);
    let ids = packet_tensor(p[..], 30);
    let rows = ids.ne[1];
    if out == 0 || !routed_dense(ids, 2048, rows, 1, 26) { return 0; }

    fg143_graph_inputs(score, sources[..]);
    graph_describe(score, p[..]);
    let q = packet_tensor(p[..], 0);
    let k = packet_tensor(p[..], 10);
    let weights = packet_tensor(p[..], 20);
    let mask = packet_tensor(p[..], 59);
    let cells = k.ne[2];
    if cells <= 4096 || cells > 32768 || cells % 16 != 0 || rows < 128 || rows > 4096 ||
       !routed_dense(q, 128, 64, rows, 0) || !routed_dense(weights, 64, rows, 1, 0) ||
       k.kind != 1 || k.ne[0] != 128 || k.ne[1] != 1 || k.ne[3] != 1 ||
       k.nb[0] != 2 || k.nb[2] < 256 || k.nb[2] % 2 != 0 ||
       mask.kind != 1 || mask.ne[0] != cells || mask.ne[1] != rows ||
       mask.ne[2] != 1 || mask.ne[3] != 1 || mask.nb[0] != 2 || mask.nb[1] != 2 * cells ||
       mask.bytes != 2 * cells * rows ||
       !routed_dense(packet_tensor(p[..], 30), cells, rows, 1, 0) { return 0; }
    var ptr: u64[4] = [0; 4];
    var span: i64[4] = [q.bytes, k.bytes, weights.bytes, mask.bytes];
    for i in 4 {
        ptr[i] = storage_get(sources[i], STORAGE_DATA, 0);
        if ptr[i] == 0 || span[i] <= 0 ||
           activation_spans_overlap(out, ids.bytes as u64, ptr[i], span[i] as u64) { return 0; }
    }
    if graph_output_prepare(out, ids.bytes, true, false, step, deadlines[step + 1]) != 0 { return -1; }
    var config: i64[6] = [cells, rows, k.nb[2], q.nb[2], weights.nb[1], mask.nb[1]];
    let status = certified_dsa(config[..], ptr[..], span[..], out, ids.bytes);
    return if status > 0 { 2 } else { status };
}

extern fn fg143_graph_name_unnamed(node: u64, leaf: bool, index: i32);

const TENSOR_SOURCES: u64 = 152;
const TENSOR_PARAM_FLAG: i32 = 4;
const TENSOR_COMPUTE_FLAG: i32 = 16;
const GRAPH_ORDER: u64 = 80;

fn graph_visit_info(graph: u64, node: u64, count_use: bool, info: u64[:]) -> i64 {
    let slot = goose_abi_ggml_hash_find(graph + GRAPH_HASH_SIZE, node);
    if slot == 0xffffffffffffffff { return -1; }
    let op = tensor_read_i32(node + TENSOR_OPERATION, 0);
    var flags = tensor_read_i32(node + TENSOR_FLAGS, 0) as! i32;
    if op != 0 {
        flags |= TENSOR_COMPUTE_FLAG;
        tensor_write_i32(node + TENSOR_FLAGS, flags);
    }
    let used = tensor_read_word(graph + GRAPH_HASH_USED) + (slot / 32) * 4;
    let mask = (1 as! u32) << ((slot % 32) as! u32);
    let bits = tensor_read_i32(used, 0) as! u32;
    let first = (bits & mask) == 0;
    info[10] = if first { 1 } else { 0 };
    info[11] = if op == 0 && (flags & TENSOR_PARAM_FLAG) == 0 { 1 } else { 0 };
    let uses = tensor_read_word(graph + GRAPH_USE_COUNTS) + slot * 4;
    if first {
        let keys = tensor_read_word(graph + GRAPH_HASH_KEYS);
        tensor_write_word(keys + slot * 8, node);
        tensor_write_i32(used, (bits | mask) as! i32);
        tensor_write_i32(uses, if count_use { 1 } else { 0 });
    } else if count_use {
        tensor_write_i32(uses, (tensor_read_i32(uses, 0) as! i32) + 1);
    }

    let reverse = first && tensor_read_i32(graph + GRAPH_ORDER, 0) == 1;
    var parents: i64 = 0;
    for i in 10 {
        let index = if reverse { 9 - i } else { i };
        let parent = tensor_read_word(node + TENSOR_SOURCES + (index as! u64) * 8);
        if parent != 0 && (first || (tensor_read_i32(parent + TENSOR_FLAGS, 0) & (TENSOR_COMPUTE_FLAG as! i64)) == 0) {
            info[parents] = parent;
            parents += 1;
        }
    }
    return parents;
}

fn graph_visit_append(graph: u64, node: u64, leaf: bool) -> bool {
    let count_address = graph + (if leaf { 8 as! u64 } else { 4 as! u64 });
    let count = tensor_read_i32(count_address, 0);
    if count >= tensor_read_i32(graph, 0) { return false; }
    fg143_graph_name_unnamed(node, leaf, count as! i32);
    let list = tensor_read_word(graph + (if leaf { GRAPH_LEAFS } else { GRAPH_NODES }));
    tensor_write_word(list + (count as! u64) * 8, node);
    tensor_write_i32(count_address, (count as! i32) + 1);
    return true;
}

fn graph_visit_is_tail(graph: u64, node: u64) -> bool {
    let count = tensor_read_i32(graph + 4, 0);
    if count <= 0 { return false; }
    let nodes = tensor_read_word(graph + GRAPH_NODES);
    return tensor_read_word(nodes + ((count - 1) as! u64) * 8) == node;
}

recursive fn graph_visit_forward(graph: u64, node: u64, count_use: bool) -> bool {
    var info = default<u64[12]>();
    let parents = graph_visit_info(graph, node, count_use, info[..]);
    if parents < 0 { return false; }
    let first_visit = info[10] != 0;
    for i in parents {
        if !graph_visit_forward(graph, info[i], first_visit) { return false; }
    }
    return !first_visit || graph_visit_append(graph, node, info[11] != 0);
}

export fn fg143_graph_expand_forward(graph: u64, node: u64) {
    let old_nodes = fg143_graph_nodes(graph);
    assert(graph_visit_forward(graph, node, false));
    assert(fg143_graph_nodes(graph) == old_nodes || graph_visit_is_tail(graph, node));
}

extern "abort" fn tensor_geometry_abort();

fn tensor_shape(tensor: u64) -> i64[4] {
    return [ctor_extent(tensor as i64, 0), ctor_extent(tensor as i64, 1), ctor_extent(tensor as i64, 2), ctor_extent(tensor as i64, 3)];
}

fn tensor_layout(tensor: u64) -> i64[8] {
    var geometry: u64[8];
    ctor_geometry(tensor as i64, geometry, false);
    var layout: i64[8];
    for i in 8 { layout[i] = geometry[i] as i64; }
    return layout;
}

export fn goose_abi_ggml_nelements(tensor: u64) -> i64 { return fg143_geometry_product(tensor_shape(tensor), 0); }
export fn goose_abi_ggml_nrows(tensor: u64) -> i64 { return fg143_geometry_product(tensor_shape(tensor), 1); }
export fn goose_abi_ggml_n_dims(tensor: u64) -> i32 { return fg143_geometry_rank(tensor_shape(tensor)) as i32; }
export fn goose_abi_ggml_is_scalar(tensor: u64) -> bool { return fg143_geometry_unit_from(tensor_shape(tensor), 0); }
export fn goose_abi_ggml_is_vector(tensor: u64) -> bool { return fg143_geometry_unit_from(tensor_shape(tensor), 1); }
export fn goose_abi_ggml_is_matrix(tensor: u64) -> bool { return fg143_geometry_unit_from(tensor_shape(tensor), 2); }
export fn goose_abi_ggml_is_3d(tensor: u64) -> bool { return fg143_geometry_unit_from(tensor_shape(tensor), 3); }
export fn goose_abi_ggml_is_empty(tensor: u64) -> bool { return fg143_geometry_empty(tensor_shape(tensor)); }

export fn goose_abi_ggml_nbytes(tensor: u64) -> u64 {
    let bytes = goose_tensor_span(ctor_property(tensor as i64, 0), tensor_layout(tensor));
    if bytes < 0 { tensor_geometry_abort(); }
    return bytes as u64;
}

fn tensor_is_contiguous(tensor: u64, loose: i64) -> bool {
    return fg143_geometry_contiguous(ctor_property(tensor as i64, 0), tensor_layout(tensor), loose);
}

export fn goose_abi_ggml_is_contiguous(tensor: u64) -> bool { return tensor_is_contiguous(tensor, 0); }
export fn goose_abi_ggml_is_contiguous_0(tensor: u64) -> bool { return tensor_is_contiguous(tensor, 0); }
export fn goose_abi_ggml_is_contiguous_1(tensor: u64) -> bool { return tensor_is_contiguous(tensor, 1); }
export fn goose_abi_ggml_is_contiguous_rows(tensor: u64) -> bool {
    let layout = tensor_layout(tensor);
    return fg143_geometry_rows(ctor_property(tensor as i64, 0), layout[0], layout[4]);
}
export fn goose_abi_ggml_are_same_shape(a: u64, b: u64) -> bool {
    return fg143_geometry_equal(tensor_shape(a), tensor_shape(b));
}
export fn goose_abi_ggml_are_same_stride(a: u64, b: u64) -> bool {
    let first = tensor_layout(a);
    let second = tensor_layout(b);
    for i in 4 .. 8 { if first[i] != second[i] { return false; } }
    return true;
}
export fn goose_abi_ggml_can_repeat(a: u64, b: u64) -> bool {
    return fg143_geometry_repeat(tensor_shape(a), tensor_shape(b));
}

const TENSOR_OP_ARGSORT: i64 = 69;
const TENSOR_OP_CONT: i64 = 35;
const TENSOR_OP_CPY: i64 = 34;
const TENSOR_OP_GATED_DELTA_NET: i64 = 86;
const TENSOR_OP_LIGHTNING_INDEXER: i64 = 87;
const TENSOR_OP_MUL_MAT: i64 = 29;
const TENSOR_OP_MUL_MAT_ID: i64 = 30;
const TENSOR_OP_PERMUTE: i64 = 38;
const TENSOR_OP_RESHAPE: i64 = 36;
const TENSOR_OP_SSM_CONV: i64 = 76;
const TENSOR_OP_SSM_SCAN: i64 = 77;
const TENSOR_OP_TOP_K: i64 = 70;
const TENSOR_OP_TRANSPOSE: i64 = 39;
const TENSOR_OP_VIEW: i64 = 37;
const TENSOR_SORT_ORDER_DESC: i64 = 1;
const TENSOR_TYPE_F16: i64 = 1;
const TENSOR_TYPE_F32: i64 = 0;
const TENSOR_TYPE_I32: i64 = 26;

export fn goose_abi_ggml_reshape_2d(ctx: u64, input: u64, n0: i64, n1: i64) -> u64 {
    return fg143_construct_reshape(ctx as i64, input as i64, [n0, n1], TENSOR_OP_RESHAPE) as u64;
}

export fn goose_abi_ggml_reshape_3d(ctx: u64, input: u64, n0: i64, n1: i64, n2: i64) -> u64 {
    return fg143_construct_reshape(ctx as i64, input as i64, [n0, n1, n2], TENSOR_OP_RESHAPE) as u64;
}

export fn goose_abi_ggml_reshape_4d(ctx: u64, input: u64, n0: i64, n1: i64, n2: i64, n3: i64) -> u64 {
    return fg143_construct_reshape(ctx as i64, input as i64, [n0, n1, n2, n3], TENSOR_OP_RESHAPE) as u64;
}

export fn goose_abi_ggml_view_2d(ctx: u64, input: u64, n0: i64, n1: i64, s1: u64, offset: u64) -> u64 {
    var strides: u64[1] = [s1];
    return fg143_construct_view(ctx as i64, input as i64, [n0, n1], strides, offset, TENSOR_OP_VIEW) as u64;
}

export fn goose_abi_ggml_view_3d(ctx: u64, input: u64, n0: i64, n1: i64, n2: i64, s1: u64, s2: u64, offset: u64) -> u64 {
    var strides: u64[2] = [s1, s2];
    return fg143_construct_view(ctx as i64, input as i64, [n0, n1, n2], strides, offset, TENSOR_OP_VIEW) as u64;
}

export fn goose_abi_ggml_view_4d(ctx: u64, input: u64, n0: i64, n1: i64, n2: i64, n3: i64, s1: u64, s2: u64, s3: u64, offset: u64) -> u64 {
    var strides: u64[3] = [s1, s2, s3];
    return fg143_construct_view(ctx as i64, input as i64, [n0, n1, n2, n3], strides, offset, TENSOR_OP_VIEW) as u64;
}

export fn goose_abi_ggml_permute(ctx: u64, input: u64, ax0: i32, ax1: i32, ax2: i32, ax3: i32) -> u64 {
    let axes: i64[4] = [ax0 as i64, ax1 as i64, ax2 as i64, ax3 as i64];
    return fg143_construct_axis_map(ctx as i64, input as i64, axes, TENSOR_OP_PERMUTE, false) as u64;
}

export fn goose_abi_ggml_transpose(ctx: u64, input: u64) -> u64 {
    return fg143_construct_axis_map(ctx as i64, input as i64, [1, 0, 2, 3], TENSOR_OP_TRANSPOSE, true) as u64;
}

export fn goose_abi_ggml_mul_mat(ctx: u64, a: u64, b: u64) -> u64 {
    return fg143_construct_product(ctx as i64, a as i64, b as i64, 0, TENSOR_TYPE_F32, TENSOR_TYPE_I32, TENSOR_OP_MUL_MAT) as u64;
}

export fn goose_abi_ggml_mul_mat_id(ctx: u64, a: u64, b: u64, ids: u64) -> u64 {
    return fg143_construct_product(ctx as i64, a as i64, b as i64, ids as i64, TENSOR_TYPE_F32, TENSOR_TYPE_I32, TENSOR_OP_MUL_MAT_ID) as u64;
}

export fn goose_abi_ggml_cpy(ctx: u64, input: u64, b: u64) -> u64 {
    return fg143_construct_copy(ctx as i64, input as i64, b as i64, 0, TENSOR_OP_CPY, 0) as u64;
}

export fn goose_abi_ggml_cast(ctx: u64, input: u64, kind: i32) -> u64 {
    return fg143_construct_copy(ctx as i64, input as i64, 0, kind as i64, TENSOR_OP_CPY, 1) as u64;
}

export fn goose_abi_ggml_cont(ctx: u64, input: u64) -> u64 {
    return fg143_construct_copy(ctx as i64, input as i64, 0, 0, TENSOR_OP_CONT, 2) as u64;
}

export fn goose_abi_ggml_ssm_conv(ctx: u64, input: u64, kernel: u64) -> u64 {
    return fg143_construct_ssm_conv(ctx as i64, input as i64, kernel as i64, TENSOR_TYPE_F32, TENSOR_OP_SSM_CONV) as u64;
}

export fn goose_abi_ggml_ssm_scan(ctx: u64, state: u64, x: u64, dt: u64, a: u64, b: u64, c: u64, ids: u64, snapshots: i64) -> u64 {
    let inputs: i64[7] = [state as i64, x as i64, dt as i64, a as i64, b as i64, c as i64, ids as i64];
    return fg143_construct_ssm_scan(ctx as i64, inputs, snapshots, TENSOR_TYPE_F32, TENSOR_TYPE_I32, TENSOR_OP_SSM_SCAN) as u64;
}

export fn goose_abi_ggml_gated_delta_net(ctx: u64, q: u64, k: u64, v: u64, g: u64, beta: u64, state: u64, snapshots: i64) -> u64 {
    let inputs: i64[6] = [q as i64, k as i64, v as i64, g as i64, beta as i64, state as i64];
    return fg143_construct_gated_delta(ctx as i64, inputs, snapshots, TENSOR_TYPE_F32, TENSOR_OP_GATED_DELTA_NET) as u64;
}

export fn goose_abi_ggml_lightning_indexer(ctx: u64, q: u64, k: u64, weights: u64, mask: u64) -> u64 {
    let inputs: i64[4] = [q as i64, k as i64, weights as i64, mask as i64];
    return fg143_construct_indexer(ctx as i64, inputs, TENSOR_TYPE_F32, TENSOR_TYPE_F16, TENSOR_OP_LIGHTNING_INDEXER) as u64;
}

export fn goose_abi_ggml_argsort(ctx: u64, input: u64, order: i32) -> u64 {
    return fg143_construct_sort(ctx as i64, input as i64, 0, order as i64, 0, TENSOR_TYPE_I32, TENSOR_OP_ARGSORT, TENSOR_OP_VIEW) as u64;
}

export fn goose_abi_ggml_argsort_top_k(ctx: u64, input: u64, k: i32) -> u64 {
    return fg143_construct_sort(ctx as i64, input as i64, k as i64, TENSOR_SORT_ORDER_DESC, 1, TENSOR_TYPE_I32, TENSOR_OP_ARGSORT, TENSOR_OP_VIEW) as u64;
}

export fn goose_abi_ggml_top_k(ctx: u64, input: u64, k: i32) -> u64 {
    return fg143_construct_sort(ctx as i64, input as i64, k as i64, 0, 2, TENSOR_TYPE_I32, TENSOR_OP_TOP_K, TENSOR_OP_VIEW) as u64;
}

const TENSOR_GLU_OP_SWIGLU: i64 = 2;
const TENSOR_GLU_OP_SWIGLU_CLAMP: i64 = 6;
const TENSOR_OP_ADD: i64 = 2;
const TENSOR_OP_CLAMP: i64 = 50;
const TENSOR_OP_CONCAT: i64 = 22;
const TENSOR_OP_DIV: i64 = 8;
const TENSOR_OP_FILL: i64 = 73;
const TENSOR_OP_FLASH_ATTN_EXT: i64 = 74;
const TENSOR_OP_GET_ROWS: i64 = 40;
const TENSOR_OP_GLU: i64 = 100;
const TENSOR_OP_L2_NORM: i64 = 28;
const TENSOR_OP_LOG: i64 = 11;
const TENSOR_OP_MUL: i64 = 7;
const TENSOR_OP_NORM: i64 = 24;
const TENSOR_OP_REPEAT: i64 = 20;
const TENSOR_OP_RMS_NORM: i64 = 25;
const TENSOR_OP_SCALE: i64 = 32;
const TENSOR_OP_SET_ROWS: i64 = 42;
const TENSOR_OP_SOFT_MAX: i64 = 46;
const TENSOR_OP_SQR: i64 = 9;
const TENSOR_OP_SQRT: i64 = 10;
const TENSOR_OP_SUB: i64 = 6;
const TENSOR_OP_SUM_ROWS: i64 = 15;
const TENSOR_OP_UNARY: i64 = 91;
const TENSOR_TYPE_I64: i64 = 27;

fn tensor_float_word(value: f32) -> i64 { return (bits(value) as! i32) as i64; }

fn tensor_binary(ctx: u64, a: u64, b: u64, op: i64) -> u64 {
    return fg143_construct_elementwise(ctx as i64, a as i64, b as i64, op, 4, []) as u64;
}

fn tensor_unary(ctx: u64, a: u64, op: i64) -> u64 {
    return fg143_construct_elementwise(ctx as i64, a as i64, 0, op, 0, []) as u64;
}

fn tensor_norm(ctx: u64, a: u64, op: i64, eps: f32) -> u64 {
    return fg143_construct_elementwise(ctx as i64, a as i64, 0, op, 0, [tensor_float_word(eps)]) as u64;
}

export fn goose_abi_ggml_add(ctx: u64, a: u64, b: u64) -> u64 { return tensor_binary(ctx, a, b, TENSOR_OP_ADD); }
export fn goose_abi_ggml_sub(ctx: u64, a: u64, b: u64) -> u64 { return tensor_binary(ctx, a, b, TENSOR_OP_SUB); }
export fn goose_abi_ggml_mul(ctx: u64, a: u64, b: u64) -> u64 { return tensor_binary(ctx, a, b, TENSOR_OP_MUL); }
export fn goose_abi_ggml_div(ctx: u64, a: u64, b: u64) -> u64 { return tensor_binary(ctx, a, b, TENSOR_OP_DIV); }
export fn goose_abi_ggml_sqr(ctx: u64, a: u64) -> u64 { return tensor_unary(ctx, a, TENSOR_OP_SQR); }
export fn goose_abi_ggml_sqrt(ctx: u64, a: u64) -> u64 { return tensor_unary(ctx, a, TENSOR_OP_SQRT); }
export fn goose_abi_ggml_log(ctx: u64, a: u64) -> u64 { return tensor_unary(ctx, a, TENSOR_OP_LOG); }
export fn goose_abi_ggml_norm(ctx: u64, a: u64, eps: f32) -> u64 { return tensor_norm(ctx, a, TENSOR_OP_NORM, eps); }
export fn goose_abi_ggml_rms_norm(ctx: u64, a: u64, eps: f32) -> u64 { return tensor_norm(ctx, a, TENSOR_OP_RMS_NORM, eps); }
export fn goose_abi_ggml_l2_norm(ctx: u64, a: u64, eps: f32) -> u64 { return tensor_norm(ctx, a, TENSOR_OP_L2_NORM, eps); }

export fn goose_abi_ggml_scale_bias(ctx: u64, a: u64, scale: f32, bias: f32) -> u64 {
    return fg143_construct_elementwise(ctx as i64, a as i64, 0, TENSOR_OP_SCALE, 2,
        [tensor_float_word(scale), tensor_float_word(bias)]) as u64;
}

export fn goose_abi_ggml_scale(ctx: u64, a: u64, scale: f32) -> u64 {
    return goose_abi_ggml_scale_bias(ctx, a, scale, 0.0 as f32);
}

export fn goose_abi_ggml_clamp(ctx: u64, a: u64, low: f32, high: f32) -> u64 {
    return fg143_construct_elementwise(ctx as i64, a as i64, 0, TENSOR_OP_CLAMP, 0,
        [tensor_float_word(low), tensor_float_word(high)]) as u64;
}

export fn goose_abi_ggml_unary(ctx: u64, a: u64, op: i32) -> u64 {
    return fg143_construct_elementwise(ctx as i64, a as i64, 0, TENSOR_OP_UNARY, 1, [op as i64]) as u64;
}

export fn goose_abi_ggml_sum_rows(ctx: u64, a: u64) -> u64 {
    return fg143_construct_row_sum(ctx as i64, a as i64, ctor_property(a as i64, 0), TENSOR_OP_SUM_ROWS) as u64;
}

export fn goose_abi_ggml_view_1d(ctx: u64, a: u64, n0: i64, offset: u64) -> u64 {
    var stride_storage: u64[1];
    return fg143_construct_view(ctx as i64, a as i64, [n0], stride_storage[0 .. 0], offset, TENSOR_OP_VIEW) as u64;
}

export fn goose_abi_ggml_flash_attn_ext(ctx: u64, q: u64, k: u64, v: u64, mask: u64,
        scale: f32, max_bias: f32, soft_cap: f32) -> u64 {
    return fg143_construct_flash_attention(ctx as i64, q as i64, k as i64, v as i64, mask as i64,
        [tensor_float_word(scale), tensor_float_word(max_bias), tensor_float_word(soft_cap)],
        TENSOR_TYPE_F32, TENSOR_TYPE_F16, TENSOR_OP_FLASH_ATTN_EXT) as u64;
}

export fn goose_abi_ggml_get_rows(ctx: u64, a: u64, b: u64) -> u64 {
    return fg143_construct_rows(ctx as i64, a as i64, b as i64, 0,
        [TENSOR_TYPE_F32, TENSOR_TYPE_F16, TENSOR_TYPE_I32, TENSOR_TYPE_I64], TENSOR_OP_GET_ROWS) as u64;
}

export fn goose_abi_ggml_set_rows(ctx: u64, a: u64, b: u64, ids: u64) -> u64 {
    return fg143_construct_rows(ctx as i64, a as i64, b as i64, ids as i64,
        [TENSOR_TYPE_F32, TENSOR_TYPE_F16, TENSOR_TYPE_I32, TENSOR_TYPE_I64], TENSOR_OP_SET_ROWS) as u64;
}

export fn goose_abi_ggml_glu(ctx: u64, a: u64, op: i32, swapped: bool) -> u64 {
    return fg143_construct_glu(ctx as i64, a as i64, 0, TENSOR_OP_GLU, op as i64, swapped, []) as u64;
}
export fn goose_abi_ggml_swiglu(ctx: u64, a: u64) -> u64 {
    return goose_abi_ggml_glu(ctx, a, TENSOR_GLU_OP_SWIGLU as i32, false);
}
export fn goose_abi_ggml_swiglu_swapped(ctx: u64, a: u64) -> u64 {
    return goose_abi_ggml_glu(ctx, a, TENSOR_GLU_OP_SWIGLU as i32, true);
}
export fn goose_abi_ggml_swiglu_split(ctx: u64, a: u64, b: u64) -> u64 {
    return fg143_construct_glu(ctx as i64, a as i64, b as i64, TENSOR_OP_GLU,
        TENSOR_GLU_OP_SWIGLU, false, []) as u64;
}
export fn goose_abi_ggml_swiglu_clamp(ctx: u64, a: u64, b: u64, limit: f32) -> u64 {
    return fg143_construct_glu(ctx as i64, a as i64, b as i64, TENSOR_OP_GLU,
        TENSOR_GLU_OP_SWIGLU_CLAMP, false, [tensor_float_word(limit)]) as u64;
}

export fn goose_abi_ggml_repeat_4d(ctx: u64, a: u64, n0: i64, n1: i64, n2: i64, n3: i64) -> u64 {
    return fg143_construct_tile(ctx as i64, a as i64, 0, 0, [n0, n1, n2, n3], TENSOR_OP_REPEAT) as u64;
}
export fn goose_abi_ggml_concat(ctx: u64, a: u64, b: u64, axis: i32) -> u64 {
    return fg143_construct_tile(ctx as i64, a as i64, b as i64, axis as i64, [], TENSOR_OP_CONCAT) as u64;
}

export fn goose_abi_ggml_soft_max_ext(ctx: u64, a: u64, mask: u64, scale: f32, max_bias: f32) -> u64 {
    return fg143_construct_softmax(ctx as i64, a as i64, mask as i64,
        [tensor_float_word(scale), tensor_float_word(max_bias)], TENSOR_TYPE_F32, TENSOR_TYPE_F16, TENSOR_OP_SOFT_MAX) as u64;
}
export fn goose_abi_ggml_soft_max(ctx: u64, a: u64) -> u64 {
    return goose_abi_ggml_soft_max_ext(ctx, a, 0, 1.0 as f32, 0.0 as f32);
}
export fn goose_abi_ggml_fill(ctx: u64, a: u64, value: f32) -> u64 {
    return fg143_construct_fill(ctx as i64, a as i64, tensor_float_word(value), TENSOR_TYPE_F32,
        TENSOR_TYPE_F16, TENSOR_OP_FILL) as u64;
}

const TENSOR_OP_ROPE: i64 = 48;
const TENSOR_ROPE_MROPE: i64 = 8;
extern "fg158_i32" fn tensor_read_i32(address: u64, index: i64) -> i64;

export fn goose_abi_ggml_rope_multi(ctx: u64, input: u64, positions: u64, factors: u64, dims: i32,
        sections: u64, mode: i32, original_context: i32, base: f32, scale: f32,
        extension: f32, attention: f32, beta_fast: f32, beta_slow: f32) -> u64 {
    let config: i64[7] = [dims as i64, mode as i64, original_context as i64, TENSOR_ROPE_MROPE,
                         TENSOR_TYPE_I32, TENSOR_TYPE_F32, TENSOR_OP_ROPE];
    let scales: i64[6] = [tensor_float_word(base), tensor_float_word(scale), tensor_float_word(extension),
                         tensor_float_word(attention), tensor_float_word(beta_fast), tensor_float_word(beta_slow)];
    var axes: i64[4];
    var section_count: i64 = 0;
    if sections != 0 && (config[1] & TENSOR_ROPE_MROPE) != 0 {
        section_count = axes.len;
        for i in section_count { axes[i] = tensor_read_i32(sections, i); }
    }
    return fg143_construct_rope(ctx as i64, input as i64, positions as i64, factors as i64,
                                config, scales, axes[0 .. section_count]) as u64;
}

export fn goose_abi_ggml_rope_ext(ctx: u64, input: u64, positions: u64, factors: u64, dims: i32,
        mode: i32, original_context: i32, base: f32, scale: f32,
        extension: f32, attention: f32, beta_fast: f32, beta_slow: f32) -> u64 {
    return goose_abi_ggml_rope_multi(ctx, input, positions, factors, dims, 0, mode, original_context,
                          base, scale, extension, attention, beta_fast, beta_slow);
}

export fn goose_abi_ggml_new_tensor_1d(ctx: u64, kind: i32, n0: i64) -> u64 {
    return tensor_create(ctx, kind as i64, [n0], 0, 0);
}
export fn goose_abi_ggml_new_tensor_2d(ctx: u64, kind: i32, n0: i64, n1: i64) -> u64 {
    return tensor_create(ctx, kind as i64, [n0, n1], 0, 0);
}
export fn goose_abi_ggml_new_tensor_3d(ctx: u64, kind: i32, n0: i64, n1: i64, n2: i64) -> u64 {
    return tensor_create(ctx, kind as i64, [n0, n1, n2], 0, 0);
}
export fn goose_abi_ggml_new_tensor_4d(ctx: u64, kind: i32, n0: i64, n1: i64, n2: i64, n3: i64) -> u64 {
    return tensor_create(ctx, kind as i64, [n0, n1, n2, n3], 0, 0);
}

export fn goose_abi_ggml_dup_tensor(ctx: u64, source: u64) -> u64 {
    let shape = tensor_shape(source);
    return ctor_tensor(ctx as i64, ctor_property(source as i64, 0),
                       shape[0], shape[1], shape[2], shape[3]) as u64;
}

export fn goose_abi_ggml_view_tensor(ctx: u64, source: u64) -> u64 {
    let shape = tensor_shape(source);
    let result = ctor_view_storage(ctx as i64, source as i64, shape, 0);
    ctor_caption(result, source as i64, 1);
    var geometry: u64[8];
    ctor_geometry(source as i64, geometry, false);
    ctor_geometry(result, geometry, true);
    return result as u64;
}


const TENSOR_HEADER_BYTES: u64 = 336;
const TENSOR_VIEW_SOURCE: u64 = 232;
const TENSOR_VIEW_OFFSET: u64 = 240;
const TENSOR_DATA: u64 = 248;

extern fn fg143_arena_failure(status: i32, needed: u64, capacity: u64);
extern "memset" fn tensor_set_raw(dst: c_ptr<void>, value: i32, bytes: u64) -> c_ptr<void>;

fn tensor_read_word(address: u64) -> u64 {
    var word: u64 = 0;
    tensor_copy_raw(unsafe_c_address<c_ptr<void>>(word), unsafe_c_ptr<c_ptr<const void>>(address), 8);
    return word;
}

fn tensor_write_word(address: u64, word: u64) {
    tensor_copy_raw(unsafe_c_ptr<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(word), 8);
}

fn tensor_create(context: u64, kind: i64, shape: const i64[:], source: u64, offset: u64) -> u64 {
    var view = source;
    var view_offset = offset;
    if view != 0 {
        let base = tensor_read_word(view + TENSOR_VIEW_SOURCE);
        if base != 0 {
            view_offset += tensor_read_word(view + TENSOR_VIEW_OFFSET);
            view = base;
        }
    }
    var layout: u64[5];
    if !fg143_tensor_layout(kind, shape, layout) { tensor_geometry_abort(); return 0; }
    let bytes = layout[4];
    let max_size: u64 = 18446744073709551615;
    if view != 0 && bytes != 0 {
        if view_offset > max_size - bytes || bytes + view_offset > goose_abi_ggml_nbytes(view) {
            tensor_geometry_abort();
            return 0;
        }
    }
    let allocate: u64 = if view == 0 && !goose_abi_ggml_get_no_alloc(context) { bytes } else { 0 };
    if allocate > max_size - TENSOR_HEADER_BYTES { tensor_geometry_abort(); return 0; }
    let object = goose_abi_ggml_new_object(context, 0, TENSOR_HEADER_BYTES + allocate);
    if object == 0 { tensor_geometry_abort(); return 0; }
    let result = object + 32;
    let count = tensor_read_i32(context + 20, 0) as! i32;
    tensor_write_i32(context + 20, count + 1);
    tensor_set_raw(unsafe_c_ptr<c_ptr<void>>(result), 0, TENSOR_HEADER_BYTES);
    let kind_word = kind as! i32;
    tensor_copy_raw(unsafe_c_ptr<c_ptr<void>>(result), unsafe_c_address<c_ptr<const void>>(kind_word), 4);
    var geometry: u64[8] = [1, 1, 1, 1, layout[0], layout[1], layout[2], layout[3]];
    for i in shape.len { geometry[i] = shape[i] as! u64; }
    ctor_geometry(result as i64, geometry, true);
    var data: u64 = 0;
    if allocate != 0 { data = result + TENSOR_HEADER_BYTES; }
    else if view != 0 {
        let source_data = tensor_read_word(view + TENSOR_DATA);
        if source_data != 0 { data = source_data + view_offset; }
    }
    tensor_write_word(result + TENSOR_VIEW_SOURCE, view);
    tensor_write_word(result + TENSOR_VIEW_OFFSET, view_offset);
    tensor_write_word(result + TENSOR_DATA, data);
    return result;
}

export fn goose_abi_ggml_new_tensor(context: u64, kind: i32, dims: i32, shape: u64) -> u64 {
    if dims < 1 || dims > 4 { tensor_geometry_abort(); return 0; }
    var extents: i64[4];
    tensor_copy_raw(unsafe_c_address<c_ptr<void>>(extents[0]), unsafe_c_ptr<c_ptr<const void>>(shape),
                    (dims as! u64) * 8);
    return tensor_create(context, kind as i64, extents[0 .. (dims as! i64)], 0, 0);
}

fn tensor_write_i32(address: u64, word: i32) {
    tensor_copy_raw(unsafe_c_ptr<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(word), 4);
}

export fn goose_abi_ggml_new_object(context: u64, kind: i32, bytes: u64) -> u64 {
    let previous = tensor_read_word(context + 32);
    let offset: u64 = if previous != 0 { tensor_read_word(previous) } else { 0 };
    let size: u64 = if previous != 0 { tensor_read_word(previous + 8) } else { 0 };
    let capacity = tensor_read_word(context);
    var layout: u64[3];
    let status = fg143_object_layout(offset, size, bytes, 16, 32, capacity, layout);
    if status != 0 { fg143_arena_failure(status, layout[2], capacity); return 0; }

    let object = tensor_read_word(context + 8) + layout[0];
    if object % 16 != 0 { tensor_geometry_abort(); return 0; }
    tensor_write_word(object, layout[0] + 32);
    tensor_write_word(object + 8, layout[1]);
    tensor_write_word(object + 16, 0);
    tensor_write_word(object + 24, 0);
    tensor_write_i32(object + 24, kind);
    if previous != 0 { tensor_write_word(previous + 16, object); }
    else { tensor_write_word(context + 24, object); }
    tensor_write_word(context + 32, object);
    return object;
}

const GRAPH_HEADER_BYTES: u64 = 96;
const GRAPH_OBJECT_BYTES: u64 = 32;
const GRAPH_NODES: u64 = 16;
const GRAPH_GRADS: u64 = 24;
const GRAPH_GRAD_ACCS: u64 = 32;
const GRAPH_LEAFS: u64 = 40;
const GRAPH_USE_COUNTS: u64 = 48;
const GRAPH_HASH_SIZE: u64 = 56;
const GRAPH_HASH_USED: u64 = 64;
const GRAPH_HASH_KEYS: u64 = 72;

fn graph_bitset_bytes(size: u64) -> u64 { return ((size + 31) / 32) * 4; }

fn tensor_graph_layout(size: u64, grads: bool, offsets: u64[:]) -> u64 {
    let hash = fg143_graph_hash_size(size * 2);
    if !fg143_graph_arena_layout(size, hash, GRAPH_HEADER_BYTES, 8, graph_bitset_bytes(hash), grads, offsets) {
        tensor_geometry_abort();
    }
    return hash;
}

export fn fg143_graph_overhead_custom(size: u64, grads: bool) -> u64 {
    var offsets: u64[8];
    tensor_graph_layout(size, grads, offsets);
    return GRAPH_OBJECT_BYTES + ((offsets[7] + 15) & ~15);
}

export fn goose_abi_ggml_new_graph_custom(context: u64, size: u64, grads: bool) -> u64 {
    var offsets: u64[8];
    let hash = tensor_graph_layout(size, grads, offsets);
    let object = goose_abi_ggml_new_object(context, 1, offsets[7]);
    if object == 0 { tensor_geometry_abort(); return 0; }
    let graph = object + GRAPH_OBJECT_BYTES;
    tensor_set_raw(unsafe_c_ptr<c_ptr<void>>(graph), 0, GRAPH_HEADER_BYTES);
    tensor_write_i32(graph, size as! i32);
    tensor_write_word(graph + GRAPH_NODES, graph + offsets[0]);
    tensor_write_word(graph + GRAPH_LEAFS, graph + offsets[1]);
    tensor_write_word(graph + GRAPH_USE_COUNTS, graph + offsets[2]);
    tensor_write_word(graph + GRAPH_HASH_SIZE, hash);
    tensor_write_word(graph + GRAPH_HASH_USED, graph + offsets[6]);
    tensor_write_word(graph + GRAPH_HASH_KEYS, graph + offsets[3]);
    tensor_set_raw(unsafe_c_ptr<c_ptr<void>>(graph + offsets[6]), 0, graph_bitset_bytes(hash));
    if grads {
        let grad = graph + offsets[4];
        let accumulator = graph + offsets[5];
        tensor_write_word(graph + GRAPH_GRADS, grad);
        tensor_write_word(graph + GRAPH_GRAD_ACCS, accumulator);
        tensor_set_raw(unsafe_c_ptr<c_ptr<void>>(grad), 0, hash * 8);
        tensor_set_raw(unsafe_c_ptr<c_ptr<void>>(accumulator), 0, hash * 8);
    }
    return graph;
}

export fn goose_abi_ggml_graph_clear(graph: u64) {
    tensor_write_i32(graph + 4, 0);
    tensor_write_i32(graph + 8, 0);
    goose_abi_ggml_hash_set_reset(graph + GRAPH_HASH_SIZE);
}

const TENSOR_BUFFER: u64 = 8;
const TENSOR_OPERATION: u64 = 80;
const TENSOR_FLAGS: u64 = 148;
const TENSOR_INPUT_FLAG: i32 = 1;
const TENSOR_OUTPUT_FLAG: i32 = 2;

export fn goose_abi_ggml_set_zero(tensor: u64) -> u64 {
    if fg143_geometry_empty(tensor_shape(tensor)) { return tensor; }
    let data = tensor_read_word(tensor + TENSOR_DATA);
    if data == 0 || tensor_read_word(tensor + TENSOR_BUFFER) != 0 { tensor_geometry_abort(); return 0; }
    tensor_set_raw(unsafe_c_ptr<c_ptr<void>>(data), 0, goose_abi_ggml_nbytes(tensor));
    return tensor;
}

export fn goose_abi_ggml_set_input(tensor: u64) {
    if tensor_read_i32(tensor + TENSOR_OPERATION, 0) != 0 { tensor_geometry_abort(); return; }
    let flags = tensor + TENSOR_FLAGS;
    tensor_write_i32(flags, (tensor_read_i32(flags, 0) as! i32) | TENSOR_INPUT_FLAG);
}

export fn goose_abi_ggml_set_output(tensor: u64) {
    var current = tensor;
    while current != 0 {
        let flags = current + TENSOR_FLAGS;
        tensor_write_i32(flags, (tensor_read_i32(flags, 0) as! i32) | TENSOR_OUTPUT_FLAG);
        current = tensor_read_word(current + TENSOR_VIEW_SOURCE);
    }
}

fn arena_find_tensor(pool: u64, start: u64) -> u64 {
    var object = start;
    while object != 0 {
        if tensor_read_i32(object + 24, 0) == 0 { return pool + tensor_read_word(object); }
        object = tensor_read_word(object + 16);
    }
    return 0;
}

export fn goose_abi_ggml_get_first_tensor(context: u64) -> u64 {
    return arena_find_tensor(tensor_read_word(context + 8), tensor_read_word(context + 24));
}

export fn goose_abi_ggml_get_next_tensor(context: u64, tensor: u64) -> u64 {
    let object = tensor - 32;
    return arena_find_tensor(tensor_read_word(context + 8), tensor_read_word(object + 16));
}

extern "malloc" fn tensor_allocate_context(bytes: u64) -> c_ptr<void>;
extern "free" fn tensor_free_context(pointer: c_ptr<void>);
extern "ggml_aligned_malloc" fn tensor_allocate_pool(bytes: u64) -> u64;
extern "ggml_aligned_free" fn tensor_release_pool(pointer: u64, bytes: u64);

const ARENA_CONTEXT_BYTES: u64 = 40;
const ARENA_CONTEXT_FLAGS: u64 = 16;
const ARENA_OWNED: i32 = 1;
const ARENA_NO_ALLOC: i32 = 256;

export fn fg143_arena_init(requested: u64, buffer: u64, no_alloc: bool) -> u64 {
    let capacity = if requested == 0 { 16 } else { requested };
    let owned = buffer == 0;
    if owned && capacity > 0xfffffffffffffff0 { tensor_geometry_abort(); return 0; }
    let bytes = if owned { (capacity + 15) & ~15 } else { capacity };
    let pool = if owned { tensor_allocate_pool(bytes) } else { buffer };
    if pool == 0 || pool % 16 != 0 { tensor_geometry_abort(); return 0; }
    let allocated = tensor_allocate_context(ARENA_CONTEXT_BYTES);
    if allocated == null {
        if owned { tensor_release_pool(pool, bytes); }
        tensor_geometry_abort();
        return 0;
    }
    var context: u64 = 0;
    tensor_copy_raw(unsafe_c_address<c_ptr<void>>(context), unsafe_c_address<c_ptr<const void>>(allocated), 8);
    tensor_set_raw(allocated, 0, ARENA_CONTEXT_BYTES);
    tensor_write_word(context, bytes);
    tensor_write_word(context + 8, pool);
    let allocated_flag: i32 = if owned { ARENA_OWNED } else { 0 };
    let no_alloc_flag: i32 = if no_alloc { ARENA_NO_ALLOC } else { 0 };
    tensor_write_i32(context + ARENA_CONTEXT_FLAGS, allocated_flag | no_alloc_flag);
    return context;
}

export fn goose_abi_ggml_get_no_alloc(context: u64) -> bool {
    return (tensor_read_i32(context + ARENA_CONTEXT_FLAGS, 0) & (ARENA_NO_ALLOC as i64)) != 0;
}

export fn goose_abi_ggml_free(context: u64) {
    if context == 0 { return; }
    if (tensor_read_i32(context + ARENA_CONTEXT_FLAGS, 0) & (ARENA_OWNED as i64)) != 0 {
        tensor_release_pool(tensor_read_word(context + 8), tensor_read_word(context));
    }
    tensor_free_context(unsafe_c_ptr<c_ptr<void>>(context));
}

const GRAPH_HASH_FULL: u64 = 0xffffffffffffffff;

fn graph_hash_cell_used(used: u64, slot: u64) -> bool {
    let word = tensor_read_i32(used + (slot / 32) * 4, 0) as! u32;
    let bit = (1 as! u32) << ((slot % 32) as! u32);
    return (word & bit) != 0;
}

export fn goose_abi_ggml_hash_find(table: u64, key: u64) -> u64 {
    let slots = tensor_read_word(table);
    if slots == 0 { return GRAPH_HASH_FULL; }
    let used = tensor_read_word(table + 8);
    let keys = tensor_read_word(table + 16);
    let home = (key >> 4) % slots;
    var slot = home;
    loop {
        if !graph_hash_cell_used(used, slot) || tensor_read_word(keys + slot * 8) == key { return slot; }
        slot += 1;
        if slot == slots { slot = 0; }
        if slot == home { return GRAPH_HASH_FULL; }
    }
}

export fn goose_abi_ggml_hash_set_reset(table: u64) {
    let slots = tensor_read_word(table);
    let used = tensor_read_word(table + 8);
    tensor_set_raw(unsafe_c_ptr<c_ptr<void>>(used), 0, graph_bitset_bytes(slots));
}

export fn fg143_storage_slot(graph: u64, tensor: u64) -> i64 {
    let slot = goose_abi_ggml_hash_find(graph + GRAPH_HASH_SIZE, tensor);
    if slot == GRAPH_HASH_FULL || !graph_hash_cell_used(tensor_read_word(graph + GRAPH_HASH_USED), slot) {
        return -1;
    }
    return slot as! i64;
}

const TENSOR_PARAMETERS: u64 = 84;

export fn goose_abi_ggml_element_size(tensor: u64) -> u64 {
    let kind = tensor_read_i32(tensor, 0);
    assert(ab_range(kind));
    return AB_SIZE[kind] as! u64;
}

export fn fg143_tensor_overhead() -> u64 {
    return 32 + TENSOR_HEADER_BYTES;
}

export fn goose_abi_ggml_is_transposed(tensor: u64) -> bool {
    return tensor_read_word(tensor + 48) > tensor_read_word(tensor + 56);
}

export fn goose_abi_ggml_get_unary_op(tensor: u64) -> i32 {
    assert(tensor_read_i32(tensor + TENSOR_OPERATION, 0) == TENSOR_OP_UNARY);
    return tensor_read_i32(tensor + TENSOR_PARAMETERS, 0) as! i32;
}

export fn goose_abi_ggml_get_glu_op(tensor: u64) -> i32 {
    assert(tensor_read_i32(tensor + TENSOR_OPERATION, 0) == TENSOR_OP_GLU);
    return tensor_read_i32(tensor + TENSOR_PARAMETERS, 0) as! i32;
}

extern "memmove" fn tensor_move_raw(destination: c_ptr<void>, source: c_ptr<const void>, bytes: u64) -> c_ptr<void>;

export fn fg143_copy_bytes(destination: u64, source: u64, bytes: i64) {
    if bytes > 0 {
        tensor_move_raw(unsafe_c_ptr<c_ptr<void>>(destination), unsafe_c_ptr<c_ptr<const void>>(source), bytes as! u64);
    }
}

export fn fg143_storage_count(graph: u64) -> i64 {
    return tensor_read_i32(graph + 8, 0) + tensor_read_i32(graph + 4, 0);
}

export fn fg143_storage_tensor(graph: u64, index: i64) -> u64 {
    let leaves = tensor_read_i32(graph + 8, 0);
    if index < leaves {
        return tensor_read_word(tensor_read_word(graph + GRAPH_LEAFS) + (index as! u64) * 8);
    }
    return tensor_read_word(tensor_read_word(graph + GRAPH_NODES) + ((index - leaves) as! u64) * 8);
}

export fn fg143_storage_contains(graph: u64, tensor: u64) -> i64 {
    if tensor_read_i32(graph, 0) == 0 || tensor_read_word(graph + GRAPH_HASH_SIZE) == 0 { return -1; }
    return if fg143_storage_slot(graph, tensor) >= 0 { 1 } else { 0 };
}

export fn fg143_storage_alignment() -> u64 { return 16; }

export fn fg143_storage_hash_size(graph: u64) -> i64 {
    return if tensor_read_i32(graph, 0) > 0 { tensor_read_word(graph + GRAPH_HASH_SIZE) as! i64 } else { 0 };
}

const DECODE_PLAN_BINDINGS: u64 = 32;
const DECODE_BINDING_BYTES: u64 = 40;
const DECODE_BINDING_LIMIT: u64 = 128;
const DECODE_BINDINGS_HEADER: u64 = 16;
const DECODE_ATTENTION_KEY_COUNT: u64 = 20;
const DECODE_Q4_TYPE: i64 = 2;
const DECODE_SET_ROWS_OP: i64 = 42;

fn decode_source(tensor: u64, index: u64) -> u64 {
    return tensor_read_word(tensor + TENSOR_SOURCES + index * 8);
}

fn decode_raw_kv(view: u64, rows: i64) -> u64 {
    if view == 0 || tensor_read_i32(view + TENSOR_OPERATION, 0) != TENSOR_OP_PERMUTE ||
        tensor_read_i32(view, 0) != DECODE_Q4_TYPE || ctor_extent(view as i64, 1) != rows ||
        tensor_read_i32(view + TENSOR_PARAMETERS, 2) != 1 { return 0; }
    let raw = decode_source(view, 0);
    if raw == 0 || tensor_read_i32(raw, 0) != DECODE_Q4_TYPE ||
        tensor_read_i32(raw + TENSOR_OPERATION, 0) != TENSOR_OP_VIEW ||
        ctor_extent(raw as i64, 2) != rows ||
        ctor_extent(raw as i64, 0) != ctor_extent(view as i64, 0) ||
        ctor_extent(raw as i64, 1) != ctor_extent(view as i64, 2) ||
        tensor_read_word(raw + 64) != tensor_read_word(view + 56) { return 0; }
    let writer = decode_source(raw, 0);
    return if writer != 0 && tensor_read_i32(writer + TENSOR_OPERATION, 0) == DECODE_SET_ROWS_OP { raw } else { 0 };
}

export fn goose_abi_fg_standalone_graph_bind_decode(plan: u64, rows: i64) -> bool {
    if plan == 0 || rows <= 0 || rows > 2147483647 || tensor_read_word(plan + DECODE_PLAN_BINDINGS) != 0 { return false; }
    let allocated = tensor_allocate_context(DECODE_BINDINGS_HEADER + DECODE_BINDING_LIMIT * DECODE_BINDING_BYTES);
    if allocated == null { return false; }
    var bindings: u64 = 0;
    tensor_copy_raw(unsafe_c_address<c_ptr<void>>(bindings), unsafe_c_address<c_ptr<const void>>(allocated), 8);
    let graph = tensor_read_word(plan);
    let nodes = tensor_read_word(graph + GRAPH_NODES);
    var count: u64 = 0;
    for i in tensor_read_i32(graph + 4, 0) {
        let node = tensor_read_word(nodes + (i as! u64) * 8);
        if tensor_read_i32(node + TENSOR_OPERATION, 0) != TENSOR_OP_FLASH_ATTN_EXT { continue; }
        let key = decode_source(node, 1);
        let value = decode_source(node, 2);
        let raw_key = decode_raw_kv(key, rows);
        let raw_value = decode_raw_kv(value, rows);
        if decode_source(node, 3) != 0 ||
            tensor_read_i32(node + TENSOR_PARAMETERS + DECODE_ATTENTION_KEY_COUNT, 0) != rows ||
            raw_key == 0 || raw_value == 0 || count == DECODE_BINDING_LIMIT {
            tensor_free_context(allocated);
            return false;
        }
        let layer = bindings + DECODE_BINDINGS_HEADER + count * DECODE_BINDING_BYTES;
        tensor_write_word(layer, node);
        tensor_write_word(layer + 8, key);
        tensor_write_word(layer + 16, value);
        tensor_write_word(layer + 24, raw_key);
        tensor_write_word(layer + 32, raw_value);
        count += 1;
    }
    if count == 0 {
        tensor_free_context(allocated);
        return false;
    }
    tensor_write_word(bindings, count);
    tensor_write_word(bindings + 8, 0);
    tensor_write_word(plan + DECODE_PLAN_BINDINGS, bindings);
    return true;
}

export fn goose_abi_fg_standalone_graph_update_decode(plan: u64, rows: i64) {
    let bindings = tensor_read_word(plan + DECODE_PLAN_BINDINGS);
    let count = tensor_read_word(bindings);
    for i in count as! i64 {
        let layer = bindings + DECODE_BINDINGS_HEADER + (i as! u64) * DECODE_BINDING_BYTES;
        let node = tensor_read_word(layer);
        tensor_write_word(tensor_read_word(layer + 8) + 24, rows as! u64);
        tensor_write_word(tensor_read_word(layer + 16) + 24, rows as! u64);
        tensor_write_word(tensor_read_word(layer + 24) + 32, rows as! u64);
        tensor_write_word(tensor_read_word(layer + 32) + 32, rows as! u64);
        tensor_write_i32(node + TENSOR_PARAMETERS + DECODE_ATTENTION_KEY_COUNT, rows as! i32);
    }
    tensor_write_word(bindings + 8, tensor_read_word(bindings + 8) + 1);
}

export fn fg143_graph_nodes(graph: u64) -> i64 {
    return tensor_read_i32(graph + 4, 0);
}

export fn fg143_graph_inputs(tensor: u64, sources: u64[:]) {
    tensor_copy_raw(unsafe_c_address<c_ptr<void>>(sources[0]),
                    unsafe_c_ptr<c_ptr<const void>>(tensor + TENSOR_SOURCES), 80);
}

fn graph_write_layout(tensor: u64, packet: i64[:], offset: i64) {
    tensor_copy_raw(unsafe_c_address<c_ptr<void>>(packet[offset]),
                    unsafe_c_ptr<c_ptr<const void>>(tensor + 16), 64);
    packet[offset + 8] = tensor_read_i32(tensor, 0);
}

export fn fg143_graph_layout(tensor: u64, packet: i64[:]) -> i64 {
    let first = decode_source(tensor, 0);
    let second = decode_source(tensor, 1);
    let third = decode_source(tensor, 2);
    let fourth = decode_source(tensor, 3);
    var present: i64 = 8;
    if first != 0 { graph_write_layout(first, packet, 0); present |= 1; }
    if second != 0 { graph_write_layout(second, packet, 10); present |= 2; }
    if third != 0 { graph_write_layout(third, packet, 20); present |= 4; }
    graph_write_layout(tensor, packet, 30);
    if fourth != 0 { graph_write_layout(fourth, packet, 59); present |= 16; }
    let op = tensor_read_i32(tensor + TENSOR_OPERATION, 0);
    packet[40] = op;
    for i in 16 { packet[41 + i] = tensor_read_i32(tensor + TENSOR_PARAMETERS, i); }
    if op == TENSOR_OP_TOP_K {
        let plain_softmax = first != 0 && tensor_read_i32(first + TENSOR_OPERATION, 0) == TENSOR_OP_SOFT_MAX &&
            decode_source(first, 1) == 0 && decode_source(first, 2) == 0;
        packet[55] = if plain_softmax { 1 } else { 0 };
    }
    packet[57] = if first != 0 { tensor_read_word(first + TENSOR_DATA) as! i64 } else { 0 };
    packet[58] = tensor_read_word(tensor + TENSOR_DATA) as! i64;
    packet[69] = if decode_source(tensor, 4) != 0 { 1 } else { 0 };
    packet[70] = if second != 0 { tensor_read_word(second + TENSOR_DATA) as! i64 } else { 0 };
    return present;
}

export fn fg143_read_contiguous_id(address: u64, index: i64, wide: bool) -> i64 {
    if wide { return tensor_read_word(address + (index as! u64) * 8) as! i64; }
    var value: i32 = 0;
    tensor_copy_raw(unsafe_c_address<c_ptr<void>>(value),
                    unsafe_c_ptr<c_ptr<const void>>(address + (index as! u64) * 4), 4);
    return value as i64;
}

const INKLING_GRAPH_BRIDGE: i64 = 0x49324252;
const INKLING_GRAPH_ATTENTION: i64 = 0x49324147;
const INKLING_GRAPH_EPILOGUE: i64 = 0x49324d45;
const INKLING_GRAPH_SITU: i64 = 0x49325354;
const INKLING_TANH_OP: i64 = 4;
const INKLING_GRAPH_CALL_WORDS: i64 = 29;

extern fn inkling_bridge_execute(bridge: u64, attention: u64, output: u64) -> i32;
extern fn inkling_attention_graph_execute(call: u64) -> i32;
extern fn fg_cuda_inkling_moe_epilogue(input: u8[:], residual: u8[:], weights: u8[:],
        output: u8[:], slot: i32, position: i32, rows: i32) -> i64;
extern fn fg_cuda_host_getenv(name: u64) -> u64;
extern fn fg_cuda_host_env_equal(value: u64, expected: u64) -> bool;

fn graph_pointer_address(pointer: c_ptr<const void>) -> u64 {
    var address: u64 = 0;
    tensor_copy_raw(unsafe_c_address<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(pointer), 8);
    return address;
}

fn inkling_tanh_enabled() -> bool {
    let name: u8[13] = [73, 50, 95, 67, 85, 68, 65, 95, 84, 65, 78, 72, 0];
    let zero: u8[2] = [48, 0];
    let setting = fg_cuda_host_getenv(graph_pointer_address(unsafe_c_address<c_ptr<const void>>(name[0])));
    return !fg_cuda_host_env_equal(setting, graph_pointer_address(unsafe_c_address<c_ptr<const void>>(zero[0])));
}

fn inkling_graph_attention(tensor: u64, metadata: u64) -> i32 {
    if metadata == 0 || ctor_extent(tensor as i64, 0) != tensor_read_i32(metadata + 224, 0) ||
        ctor_extent(tensor as i64, 1) != tensor_read_i32(metadata + 12, 0) { return 0; }
    let batch = tensor_read_i32(metadata + 12, 0);
    var call: u64[INKLING_GRAPH_CALL_WORDS];
    tensor_copy_raw(unsafe_c_address<c_ptr<void>>(call[0]), unsafe_c_ptr<c_ptr<const void>>(metadata), 232);
    for i in 3 {
        let input = decode_source(tensor, i as! u64);
        if input == 0 || tensor_read_i32(input, 0) != TENSOR_TYPE_F32 ||
            ctor_extent(input as i64, 0) != 2304 || ctor_extent(input as i64, 1) != batch ||
            !goose_abi_ggml_is_contiguous(input) { return 0; }
        let data = tensor_read_word(input + TENSOR_DATA);
        if data == 0 { return 0; }
        call[10 + i] = data;
    }
    call[9] = tensor_read_word(tensor + TENSOR_DATA);
    let address = graph_pointer_address(unsafe_c_address<c_ptr<const void>>(call[0]));
    return if inkling_attention_graph_execute(address) == 0 { 1 } else { 0 };
}

fn inkling_graph_situ_valid(tensor: u64, input: u64) -> bool {
    return input != 0 && tensor_read_i32(input, 0) == TENSOR_TYPE_F32 &&
        tensor_read_i32(tensor, 0) == TENSOR_TYPE_F32 &&
        ctor_extent(input as i64, 0) == 2304 && ctor_extent(tensor as i64, 0) == 1152 &&
        ctor_extent(input as i64, 1) == 8 && ctor_extent(tensor as i64, 1) == 8 &&
        ctor_extent(input as i64, 2) == ctor_extent(tensor as i64, 2) &&
        ctor_extent(input as i64, 3) == 1 && ctor_extent(tensor as i64, 3) == 1 &&
        goose_abi_ggml_is_contiguous(input) && goose_abi_ggml_is_contiguous(tensor);
}

fn inkling_graph_special(tensor: u64, packet: const i64[:], mode: i64, sizes: i64[:]) -> i32 {
    if !cuda_inkling_quants_enabled() { return -1; }
    let op = packet[40];
    let tag = packet[41];
    let input = decode_source(tensor, 0);
    if op == TENSOR_OP_UNARY {
        if mode != 0 || tag != INKLING_TANH_OP || tensor_read_i32(tensor, 0) != TENSOR_TYPE_F32 ||
            input == 0 || tensor_read_i32(input, 0) != TENSOR_TYPE_F32 ||
            !goose_abi_ggml_is_contiguous(tensor) || !goose_abi_ggml_is_contiguous(input) ||
            !inkling_tanh_enabled() { return -1; }
        let status = fg_cuda_tanh_f32(graph_borrow_tensor(input, goose_abi_ggml_nbytes(input) as! i64),
                                      graph_borrow_tensor(tensor, goose_abi_ggml_nbytes(tensor) as! i64));
        return if status == 0 { -1 } else if status > 0 { 1 } else { 0 };
    }
    if tag != INKLING_GRAPH_BRIDGE && tag != INKLING_GRAPH_ATTENTION &&
        tag != INKLING_GRAPH_EPILOGUE && tag != INKLING_GRAPH_SITU { return -1; }
    if tag == INKLING_GRAPH_SITU && !inkling_graph_situ_valid(tensor, input) { return 0; }
    if mode == 1 {
        for i in sizes.len { sizes[i] = 0; }
        return 1;
    }
    if tag == INKLING_GRAPH_BRIDGE {
        let bridge = tensor_read_word(tensor + TENSOR_PARAMETERS + 8);
        if bridge == 0 || input == 0 { return 0; }
        let source = tensor_read_word(input + TENSOR_DATA);
        let output = tensor_read_word(tensor + TENSOR_DATA);
        if source == 0 || output == 0 { return 0; }
        return if inkling_bridge_execute(bridge, source, output) == 0 { 1 } else { 0 };
    }
    if tag == INKLING_GRAPH_ATTENTION {
        return inkling_graph_attention(tensor, tensor_read_word(tensor + TENSOR_PARAMETERS + 8));
    }
    if tag == INKLING_GRAPH_EPILOGUE {
        let residual = graph_borrow_tensor(decode_source(tensor, 1), packet[19]);
        let weights = graph_borrow_tensor(decode_source(tensor, 2), packet[29]);
        let status = fg_cuda_inkling_moe_epilogue(graph_borrow_tensor(input, packet[9]), residual, weights,
            graph_borrow_tensor(tensor, packet[39]), packet[42] as! i32, packet[43] as! i32,
            ctor_extent(tensor as i64, 1) as! i32);
        return if status == 1 { 1 } else { 0 };
    }
    let status = fg_cuda_inkling_moe_situ(graph_borrow_tensor(input, goose_abi_ggml_nbytes(input) as! i64),
                                        graph_borrow_tensor(tensor, goose_abi_ggml_nbytes(tensor) as! i64));
    return if status == 1 { 1 } else { 0 };
}

fn state_tensor_packet(tensor: u64, packet: i64[:], slot: i64) -> u64 {
    if tensor == 0 { return 0; }
    graph_write_layout(tensor, packet, slot * 10);
    packet[slot * 10 + 9] = goose_abi_ggml_nbytes(tensor) as! i64;
    return tensor_read_word(tensor + TENSOR_DATA);
}

fn state_graph_packet(tensor: u64, packet: i64[:], addresses: u64[:]) {
    for i in addresses.len - 1 {
        addresses[i] = state_tensor_packet(decode_source(tensor, i as! u64), packet, i);
    }
    addresses[addresses.len - 1] = state_tensor_packet(tensor, packet, 7);
    packet[80] = tensor_read_i32(tensor + TENSOR_OPERATION, 0);
    packet[81] = tensor_read_i32(tensor + TENSOR_PARAMETERS, 0);
}

fn graph_gdn(tensor: u64) -> bool {
    var packet: i64[83];
    var addresses: u64[7];
    state_graph_packet(tensor, packet, addresses);
    packet[82] = tensor_read_i32(tensor + TENSOR_PARAMETERS, 4);
    for i in 7 {
        let slot = if i == 6 { 7 } else { i };
        let bytes = packet[slot * 10 + 9];
        if bytes < 0 || bytes % 4 != 0 || addresses[i] % 4 != 0 { return false; }
    }
    return fg143_gdn_run(packet,
        graph_borrow_f32(addresses[0], packet[9] / 4),
        graph_borrow_f32(addresses[1], packet[19] / 4),
        graph_borrow_f32(addresses[2], packet[29] / 4),
        graph_borrow_f32(addresses[3], packet[39] / 4),
        graph_borrow_f32(addresses[4], packet[49] / 4),
        graph_borrow_f32(addresses[5], packet[59] / 4),
        graph_borrow_f32(addresses[6], packet[79] / 4));
}

fn graph_scan(tensor: u64) -> bool {
    var packet: i64[82];
    var addresses: u64[8];
    state_graph_packet(tensor, packet, addresses);
    return fg143_state_run(packet,
        graph_borrow_u8(addresses[0], packet[9]), graph_borrow_u8(addresses[1], packet[19]),
        graph_borrow_u8(addresses[2], packet[29]), graph_borrow_u8(addresses[3], packet[39]),
        graph_borrow_u8(addresses[4], packet[49]), graph_borrow_u8(addresses[5], packet[59]),
        graph_borrow_u8(addresses[6], packet[69]), graph_borrow_u8(addresses[7], packet[79]));
}
