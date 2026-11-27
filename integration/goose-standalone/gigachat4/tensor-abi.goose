const GIGA_TENSOR_SHAPE_OFFSET: u64 = 16;
const GIGA_TENSOR_DATA_OFFSET: u64 = 248;

extern "memcpy" fn giga_copy_raw(dst: c_ptr<void>, src: c_ptr<const void>, bytes: u64) -> c_ptr<void>;
extern "ggml_nbytes" fn giga_tensor_bytes(tensor: u64) -> u64;
extern "ggml_is_contiguous" fn giga_tensor_contiguous(tensor: u64) -> bool;
extern "ggml_new_tensor_3d" fn giga_create_tensor_3d(ctx: u64, kind: i32, x: i64, y: i64, z: i64) -> u64;
extern "fg_cuda_host_view_u8" fn giga_borrow_bytes(address: u64, bytes: i64) -> u8[:];
extern "fg_cuda_host_view_f32" fn giga_borrow_floats(address: u64, count: i64) -> f32[:];
extern "qz_dequant" fn giga_dequantize(kind: i64, source: const u8[:], count: i64, target: f32[:]) -> i64;

fn giga_tensor_word(address: u64) -> u64 {
    var word: u64 = 0;
    giga_copy_raw(unsafe_c_address<c_ptr<void>>(word), unsafe_c_ptr<c_ptr<const void>>(address), 8);
    return word;
}

fn giga_tensor_kind(tensor: u64) -> i32 {
    var kind: i32 = 0;
    giga_copy_raw(unsafe_c_address<c_ptr<void>>(kind), unsafe_c_ptr<c_ptr<const void>>(tensor), 4);
    return kind;
}

fn giga_tensor_extent(tensor: u64, axis: u64) -> u64 {
    return giga_tensor_word(tensor + GIGA_TENSOR_SHAPE_OFFSET + axis * 8);
}

fn giga_tensor_data(tensor: u64) -> u64 { return giga_tensor_word(tensor + GIGA_TENSOR_DATA_OFFSET); }

export fn fg457_st_i64(address: u64, offset: u64, value: i64) {
    giga_copy_raw(unsafe_c_ptr<c_ptr<void>>(address + offset), unsafe_c_address<c_ptr<const void>>(value), 8);
}

export fn giga_bits_to_float(bits: u32) -> f32 {
    var value: f32 = 0.0 as! f32;
    giga_copy_raw(unsafe_c_address<c_ptr<void>>(value), unsafe_c_address<c_ptr<const void>>(bits), 4);
    return value;
}

export fn giga_dense_span(tensor: u64, out: i64[:]) -> i64 {
    if tensor == 0 || out.len < 2 || giga_tensor_extent(tensor, 2) != 1 ||
        giga_tensor_extent(tensor, 3) != 1 { return 0; }
    let data = giga_tensor_data(tensor);
    let kind = giga_tensor_kind(tensor);
    if data == 0 || (kind != 0 && kind != 12 && kind != 14) { return 0; }
    out[0] = data as! i64;
    out[1] = giga_tensor_bytes(tensor) as! i64;
    return 1;
}

export fn giga_state_span(tensor: u64, out: i64[:]) -> i64 {
    if tensor == 0 || out.len < 2 { return 0; }
    let data = giga_tensor_data(tensor);
    let kind = giga_tensor_kind(tensor);
    if data == 0 || (kind != 0 && kind != 1) || !giga_tensor_contiguous(tensor) { return 0; }
    out[0] = data as! i64;
    out[1] = giga_tensor_bytes(tensor) as! i64;
    return 1;
}

export fn giga_expand_mla_descriptor(ctx: u64, source: u64) -> u64 {
    if source == 0 || giga_tensor_kind(source) != 6 || giga_tensor_extent(source, 0) != 128 ||
        giga_tensor_extent(source, 1) != 512 || giga_tensor_extent(source, 2) != 32 ||
        giga_tensor_extent(source, 3) != 1 { return 0; }
    let elements: i64 = 128 * 512 * 32;
    let target = giga_create_tensor_3d(ctx, 0, 128, 512, 32);
    let src = giga_borrow_bytes(giga_tensor_data(source), giga_tensor_bytes(source) as! i64);
    let dst = giga_borrow_floats(giga_tensor_data(target), elements);
    return if giga_dequantize(6, src, elements, dst) == elements { target } else { 0 };
}
