const B_NE: i64 = 16;
const B_NB: i64 = 48;
const B_OP: i64 = 80;
const B_PARAMS: i64 = 84;
const B_SRC: i64 = 152;
const B_VIEW: i64 = 232;
const B_VIEW_OFFSET: i64 = 240;
const B_DATA: i64 = 248;
const B_NAME: i64 = 256;
const B_CUSTOM: i64 = 95;

extern "memcpy" fn builder_copy(dst: c_ptr<void>, src: c_ptr<const void>, n: u64) -> c_ptr<void>;
extern fn graph_crt_getenv(name: i64) -> i64;
extern fn ggml_new_tensor(c: i64, kind: i32, dims: i32, ne: i64) -> i64;
extern fn ggml_new_tensor_2d(c: i64, kind: i32, n0: i64, n1: i64) -> i64;
extern fn ggml_new_tensor_3d(c: i64, kind: i32, n0: i64, n1: i64, n2: i64) -> i64;
extern fn ggml_new_tensor_4d(c: i64, kind: i32, n0: i64, n1: i64, n2: i64, n3: i64) -> i64;
extern fn ggml_l2_norm(c: i64, a: i64, eps: f32) -> i64;
extern fn ggml_rope_multi(c: i64, a: i64, pos: i64, factors: i64, rotation: i32, sections: i64,
    mode: i32, context: i32, base: f32, scale: f32, ext: f32, attention: f32, fast: f32, slow: f32) -> i64;
extern fn ggml_is_contiguous(a: i64) -> bool;
extern fn ggml_is_contiguous_rows(a: i64) -> bool;
extern fn ggml_are_same_shape(a: i64, b: i64) -> bool;
extern fn ggml_can_repeat(a: i64, b: i64) -> bool;
extern fn ggml_is_scalar(a: i64) -> bool;
extern fn ggml_is_contiguous_1(a: i64) -> bool;
extern fn ggml_is_transposed(a: i64) -> bool;
extern fn ggml_is_matrix(a: i64) -> bool;
extern fn ggml_is_empty(a: i64) -> bool;
extern fn ggml_nelements(a: i64) -> i64;
extern fn ggml_is_vector(a: i64) -> bool;

fn builder_address(pointer: c_ptr<const void>) -> i64 {
    var result: i64 = 0;
    builder_copy(unsafe_c_address<c_ptr<void>>(result), unsafe_c_address<c_ptr<const void>>(pointer), 8);
    return result;
}

fn builder_word(address: i64) -> i64 {
    var result: i64 = 0;
    builder_copy(unsafe_c_address<c_ptr<void>>(result), unsafe_c_ptr<c_ptr<const void>>(address as! u64), 8);
    return result;
}

fn builder_i32(address: i64) -> i32 {
    var result: i32 = 0;
    builder_copy(unsafe_c_address<c_ptr<void>>(result), unsafe_c_ptr<c_ptr<const void>>(address as! u64), 4);
    return result;
}

fn builder_byte(address: i64) -> u8 {
    var result: u8 = 0;
    builder_copy(unsafe_c_address<c_ptr<void>>(result), unsafe_c_ptr<c_ptr<const void>>(address as! u64), 1);
    return result;
}

fn builder_write(address: i64, value: i64) {
    builder_copy(unsafe_c_ptr<c_ptr<void>>(address as! u64), unsafe_c_address<c_ptr<const void>>(value), 8);
}

fn builder_write_i32(address: i64, value: i64) {
    let bits = value as! i32;
    builder_copy(unsafe_c_ptr<c_ptr<void>>(address as! u64), unsafe_c_address<c_ptr<const void>>(bits), 4);
}

fn builder_abort(message: const u8[:]) {
    ggml_abort_text(str("graph-bindings.goose:0: ", message));
}

export fn fg_standalone_option_enabled(name: i64) -> bool {
    let setting = graph_crt_getenv(name);
    return setting == 0 || builder_byte(setting) != 48 || builder_byte(setting + 1) != 0;
}

fn builder_enabled(name: const u8[:]) -> bool {
    var key: u8[32];
    for i in name.len { key[i] = name[i]; }
    key[name.len] = 0;
    return fg_standalone_option_enabled(builder_address(unsafe_c_address<c_ptr<const void>>(key[0])));
}

export fn fg_q35_rms_mul_enabled() -> bool { return builder_enabled("GOOSE_RMS_MUL"); }
export fn fg_q35_ssm_silu_enabled() -> bool { return builder_enabled("GOOSE_SSM_SILU"); }
export fn fg_q35_moe_norm_enabled() -> bool { return builder_enabled("GOOSE_MOE_NORM"); }

export fn fg158_arg(p: i64, i: i64) -> i64 { return builder_word(p + i * 8); }
export fn fg158_ne(t: i64, i: i64) -> i64 { return builder_word(t + B_NE + i * 8); }
fn builder_nb(t: i64, i: i64) -> i64 { return builder_word(t + B_NB + i * 8); }
fn builder_kind(t: i64) -> i32 { return builder_i32(t); }
export fn fg158_i32(p: i64, i: i64) -> i64 { return builder_i32(p + i * 4) as! i64; }
export fn fg158_src(t: i64, i: i64) -> i64 { return builder_word(t + B_SRC + i * 8); }
export fn fg158_param(t: i64, i: i64) -> i64 { return fg158_i32(t + B_PARAMS, (i as! u32) as! i64); }
export fn fg158_set_op(t: i64, op: i64) { builder_write_i32(t + B_OP, op); }
export fn fg158_set_src(t: i64, i: i64, source: i64) { builder_write(t + B_SRC + i * 8, source); }
export fn fg158_set_param(t: i64, i: i64, value: i64) {
    builder_write_i32(t + B_PARAMS + ((i as! u32) as! i64) * 4, value);
}

export fn fg158_prop(t: i64, k: i64) -> i64 {
    if k == 0 { return builder_kind(t) as! i64; }
    if k == 1 { return if ggml_is_quantized(builder_kind(t)) { 1 } else { 0 }; }
    if k == 2 { return builder_i32(t + B_OP) as! i64; }
    if k == 3 { return if builder_byte(t + B_NAME) != 0 { 1 } else { 0 }; }
    builder_abort("Invalid builder field");
    return 0;
}

export fn fg158_new(c: i64, kind: i64, n0: i64, n1: i64, n2: i64, n3: i64) -> i64 {
    let ne: i64[4] = [n0,n1,n2,n3];
    return ggml_new_tensor(c, kind as! i32, 4, builder_address(unsafe_c_address<c_ptr<const void>>(ne[0])));
}

export fn fg158_float(x: i64) -> f32 {
    let bits = x as! u32;
    var value: f32 = 0.0 as! f32;
    builder_copy(unsafe_c_address<c_ptr<void>>(value), unsafe_c_address<c_ptr<const void>>(bits), 4);
    return value;
}

export fn fg158_l2norm(c: i64, a: i64, eps: i64) -> i64 {
    if c == 0 || a == 0 { return 0; }
    return ggml_l2_norm(c, a, fg158_float(eps));
}

export fn fg158_rope_multi(c: i64, a: i64, pos: i64, rotation: i64,
    s0: i64, s1: i64, s2: i64, s3: i64, mode: i64, context: i64,
    base: i64, scale: i64, ext: i64, attention: i64, fast: i64, slow: i64) -> i64 {
    if c == 0 || a == 0 { return 0; }
    let sections: i32[4] = [s0 as! i32,s1 as! i32,s2 as! i32,s3 as! i32];
    return ggml_rope_multi(c, a, pos, 0, rotation as! i32,
        builder_address(unsafe_c_address<c_ptr<const void>>(sections[0])), mode as! i32, context as! i32,
        fg158_float(base), fg158_float(scale), fg158_float(ext), fg158_float(attention),
        fg158_float(fast), fg158_float(slow));
}

extern fn ggml_dup_tensor(c: i64, a: i64) -> i64;
export fn fg158_dup(c: i64, a: i64) -> i64 {
    return ggml_dup_tensor(c, a);
}

extern fn ggml_transpose(c: i64, a: i64) -> i64;
export fn fg158_transpose(c: i64, a: i64) -> i64 {
    if c == 0 || a == 0 { return 0; }
    return ggml_transpose(c, a);
}

extern fn ggml_concat(c: i64, a: i64, b: i64, dim: i32) -> i64;
export fn fg158_concat(c: i64, a: i64, b: i64, dim: i64) -> i64 {
    if c == 0 || a == 0 || b == 0 { return 0; }
    return ggml_concat(c, a, b, dim as! i32);
}

extern fn ggml_cpy(c: i64, a: i64, b: i64) -> i64;
export fn fg158_cpy(c: i64, a: i64, b: i64) -> i64 {
    if c == 0 || a == 0 || b == 0 { return 0; }
    return ggml_cpy(c, a, b);
}

extern fn ggml_view_2d(c: i64, a: i64, n0: i64, n1: i64, nb1: i64, offset: i64) -> i64;
export fn fg158_view2(c: i64, a: i64, n0: i64, n1: i64, nb1: i64, offset: i64) -> i64 {
    if c == 0 || a == 0 { return 0; }
    return ggml_view_2d(c, a, n0, n1, nb1, offset);
}

extern fn ggml_view_4d(c: i64, a: i64, n0: i64, n1: i64, n2: i64, n3: i64, nb1: i64, nb2: i64, nb3: i64, offset: i64) -> i64;
export fn fg158_view4(c: i64, a: i64, n0: i64, n1: i64, n2: i64, n3: i64, nb1: i64, nb2: i64, nb3: i64, offset: i64) -> i64 {
    if c == 0 || a == 0 { return 0; }
    return ggml_view_4d(c, a, n0, n1, n2, n3, nb1, nb2, nb3, offset);
}

extern fn ggml_reshape_3d(c: i64, a: i64, n0: i64, n1: i64, n2: i64) -> i64;
export fn fg158_reshape3(c: i64, a: i64, n0: i64, n1: i64, n2: i64) -> i64 {
    if c == 0 || a == 0 { return 0; }
    return ggml_reshape_3d(c, a, n0, n1, n2);
}

extern fn ggml_repeat_4d(c: i64, a: i64, n0: i64, n1: i64, n2: i64, n3: i64) -> i64;
export fn fg158_repeat4(c: i64, a: i64, n0: i64, n1: i64, n2: i64, n3: i64) -> i64 {
    if c == 0 || a == 0 { return 0; }
    return ggml_repeat_4d(c, a, n0, n1, n2, n3);
}

extern fn ggml_view_tensor(c: i64, a: i64) -> i64;
export fn fg158_view(c: i64, a: i64) -> i64 {
    return ggml_view_tensor(c, a);
}

fn builder_custom(dst: i64, source: i64, kind: i64) {
    fg158_set_op(dst, B_CUSTOM);
    fg158_set_src(dst, 0, source);
    fg158_set_param(dst, 0, kind);
}

export fn fg_q35_fwht_node(c: i64, a: i64, width: i64) -> i64 {
    if c == 0 || a == 0 || builder_kind(a) != 0 || !ggml_is_contiguous(a) || width < 64 { return 0; }
    let n0 = fg158_ne(a, 0);
    if width > n0 || n0 % width != 0 || (width & (width - 1)) != 0 { return 0; }
    let dst = ggml_new_tensor_4d(c, 0, n0, fg158_ne(a, 1), fg158_ne(a, 2), fg158_ne(a, 3));
    builder_custom(dst, a, 0x46574854);
    fg158_set_param(dst, 1, width);
    return dst;
}

export fn fg_q35_moe_weighted_node(c: i64, a: i64, weights: i64) -> i64 {
    if c == 0 || a == 0 || weights == 0 { return 0; }
    let dst = ggml_new_tensor_2d(c, 0, fg158_ne(a, 0), fg158_ne(a, 2));
    builder_custom(dst, a, 0x4d4f4557);
    fg158_set_src(dst, 1, weights);
    return dst;
}

export fn fg_q35_moe_norm_node(c: i64, a: i64, ids: i64, scale: i64) -> i64 {
    if c == 0 || a == 0 || ids == 0 || builder_kind(a) != 0 || builder_kind(ids) != 26 ||
        fg158_ne(ids, 1) != fg158_ne(a, 1) { return 0; }
    let dst = ggml_new_tensor_3d(c, 0, 1, fg158_ne(ids, 0), fg158_ne(ids, 1));
    builder_custom(dst, a, 0x4d4f454e);
    fg158_set_src(dst, 1, ids);
    fg158_set_param(dst, 1, scale);
    return dst;
}

export fn fg158_pred(which: i64, a: i64, b: i64) -> i64 {
    if which == 0 { return if ggml_is_contiguous(a) { 1 } else { 0 }; }
    if which == 1 { return if ggml_is_contiguous_rows(a) { 1 } else { 0 }; }
    if which == 3 { return if ggml_are_same_shape(a, b) { 1 } else { 0 }; }
    if which == 4 { return if ggml_can_repeat(a, b) { 1 } else { 0 }; }
    if which == 5 { return if ggml_is_scalar(a) { 1 } else { 0 }; }
    if which == 7 { return if ggml_is_contiguous_1(a) { 1 } else { 0 }; }
    if which == 10 { return if ggml_is_transposed(a) { 1 } else { 0 }; }
    if which == 11 { return if ggml_is_matrix(a) { 1 } else { 0 }; }
    if which == 12 { return if ggml_is_empty(a) { 1 } else { 0 }; }
    if which == 13 { return ggml_nelements(a); }
    if which == 14 { return if ggml_is_vector(a) { 1 } else { 0 }; }
    if which == 2 {
        return if builder_nb(a, 0) == (ggml_type_size(builder_kind(a)) as! i64) &&
            builder_nb(a, 2) == builder_nb(a, 1) * fg158_ne(a, 1) &&
            builder_nb(a, 3) == builder_nb(a, 2) * fg158_ne(a, 2) { 1 } else { 0 };
    }
    if which == 6 {
        return if fg158_ne(a, 0) == fg158_ne(b, 0) && ggml_can_repeat(a, b) { 1 } else { 0 };
    }
    if which == 8 || which == 9 {
        let dim: i64 = if which == 8 { 0 } else { 1 };
        return if fg158_ne(a, dim) == fg158_ne(b, dim) &&
            fg158_ne(b, 2) % fg158_ne(a, 2) == 0 && fg158_ne(b, 3) % fg158_ne(a, 3) == 0 { 1 } else { 0 };
    }
    builder_abort("Invalid builder predicate");
    return 0;
}

fn builder_name_part(output: u8[:], start: i64, tensor: i64) -> i64 {
    var at = start;
    let base = tensor + B_NAME;
    while at < 63 {
        let byte = builder_byte(base + at - start);
        if byte == 0 { break; }
        output[at] = byte;
        at += 1;
    }
    return at;
}

fn builder_suffix(output: u8[:], start: i64, suffix: const u8[:]) -> i64 {
    var at = start;
    for i in suffix.len {
        if at == 63 { break; }
        output[at] = suffix[i];
        at += 1;
    }
    return at;
}

fn builder_set_name(t: i64, text: u8[:], end: i64) {
    text[end] = 0;
    builder_copy(unsafe_c_ptr<c_ptr<void>>((t + B_NAME) as! u64),
        unsafe_c_slice_data<c_ptr<const void>>(text), (end + 1) as! u64);
}

export fn fg158_name(t: i64, a: i64, b: i64, style: i64) {
    var text: u8[64];
    var end: i64 = 0;
    if style == 0 {
        end = builder_name_part(text, 0, b);
        end = builder_suffix(text, end, " (copy of ");
        end = builder_name_part(text, end, a);
        end = builder_suffix(text, end, ")");
    } else {
        end = builder_name_part(text, 0, a);
        end = builder_suffix(text, end, if style == 1 { " (copy)" } else { " (cont)" });
    }
    builder_set_name(t, text, end);
}

export fn fg156_name(t: i64, source: i64, style: i64) {
    var suffix: const u8[:] = "";
    if style == 0 { suffix = " (view)"; }
    else if style == 1 { suffix = " (reshaped)"; }
    else if style == 2 { suffix = " (permuted)"; }
    else if style == 3 { suffix = " (transposed)"; }
    else { builder_abort("Invalid constructor name style"); return; }
    var text: u8[64];
    let end = builder_name_part(text, 0, source);
    builder_set_name(t, text, builder_suffix(text, end, suffix));
}

export fn fg156_tget(t: i64, kind: i64, i: i64) -> u64 {
    if kind == 0 { return fg158_ne(t, i) as! u64; }
    if kind == 1 { return builder_nb(t, i) as! u64; }
    if kind == 2 { return builder_word(t + B_VIEW) as! u64; }
    if kind == 3 { return builder_word(t + B_VIEW_OFFSET) as! u64; }
    if kind == 4 { return builder_word(t + B_DATA) as! u64; }
    if kind == 5 { return builder_kind(t) as! u64; }
    if kind == 6 { return (t + B_NE) as! u64; }
    builder_abort("Invalid tensor projection");
    return 0;
}

export fn fg156_tset(t: i64, kind: i64, i: i64, raw: u64) {
    let value = raw as! i64;
    if kind == 0 { builder_write(t + B_NE + i * 8, value); }
    else if kind == 1 { builder_write(t + B_NB + i * 8, value); }
    else if kind == 2 { builder_write(t + B_VIEW, value); }
    else if kind == 3 { builder_write(t + B_VIEW_OFFSET, value); }
    else if kind == 4 { builder_write(t + B_DATA, value); }
    else if kind == 5 { builder_write_i32(t, value); }
    else if kind == 7 { fg158_set_op(t, value); }
    else if kind == 8 { fg158_set_src(t, i, value); }
    else if kind == 9 { builder_write(t + B_PARAMS, value); }
    else if kind == 10 { builder_write_i32(t + B_PARAMS + i * 4, value); }
    else { builder_abort("Invalid tensor projection write"); }
}

export fn fg158_fail(code: i64) { builder_abort(str("Goose graph builder validation failed:code=", code)); }
export fn fg200g_fail(code: i64) { builder_abort(str("model graph refused:code=", code)); }
export fn fg202t_fail(code: i64) { builder_abort(str("tokenizer decision refused:code=", code)); }
export fn fg203d_fail(code: i64) { builder_abort(str("mmq dispatch refused:code=", code)); }
export fn q4t_fail(code: i64) { builder_abort(str("tensor map refused:code=", code)); }
