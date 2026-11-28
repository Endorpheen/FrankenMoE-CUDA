fn ggml_meta_address(text: const u8[:]) -> u64 {
    return builder_address(unsafe_c_slice_data<c_ptr<const void>>(text)) as! u64;
}

fn ggml_meta_type(kind: i32) -> i64 {
    let index=kind as! i64;
    assert(index>=0 && index<GGML_META_TYPE_COUNT);
    return index;
}

export fn ggml_blck_size(kind: i32) -> i64 {
    return GGML_META_BLOCK[ggml_meta_type(kind)];
}

export fn ggml_type_size(kind: i32) -> u64 {
    return GGML_META_SIZE[ggml_meta_type(kind)];
}

export fn ggml_row_size(kind: i32, elements: i64) -> u64 {
    let index=ggml_meta_type(kind);
    let quant_block=GGML_META_BLOCK[index];
    assert(quant_block>0 && elements%quant_block==0);
    return GGML_META_SIZE[index]*(elements as! u64)/(quant_block as! u64);
}

export fn ggml_type_sizef(kind: i32) -> f64 {
    let index=ggml_meta_type(kind);
    return (GGML_META_SIZE[index] as f64)/(GGML_META_BLOCK[index] as f64);
}

export fn ggml_type_name(kind: i32) -> u64 {
    return ggml_meta_address(GGML_META_TYPE_NAME[ggml_meta_type(kind)]);
}

export fn ggml_is_quantized(kind: i32) -> bool {
    return GGML_META_QUANTIZED[ggml_meta_type(kind)]!=0;
}

export fn ggml_op_name(op: i32) -> u64 {
    return ggml_meta_address(GGML_META_OP_NAME[op as! i64]);
}

export fn ggml_unary_op_name(op: i32) -> u64 {
    return ggml_meta_address(GGML_META_UNARY_NAME[op as! i64]);
}

export fn ggml_glu_op_name(op: i32) -> u64 {
    return ggml_meta_address(GGML_META_GLU_NAME[op as! i64]);
}

export fn ggml_status_to_string(status: i32) -> u64 {
    if status==-2 { return ggml_meta_address("GGML status: error (failed to allocate memory)\x00"); }
    if status==-1 { return ggml_meta_address("GGML status: error (operation failed)\x00"); }
    if status==0 { return ggml_meta_address("GGML status: success\x00"); }
    if status==1 { return ggml_meta_address("GGML status: warning (operation aborted)\x00"); }
    return ggml_meta_address("GGML status: unknown\x00");
}
