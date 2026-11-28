extern "as_uint" fn cuda_float_bits(value: f32) -> u32;
extern "as_float" fn cuda_bits_float(bits: u32) -> f32;
extern "as_ulong" fn cuda_double_bits(value: f64) -> u64;
extern "as_double" fn cuda_bits_double(bits: u64) -> f64;
extern "fg_mmq_half_word" fn cuda_half_word(address: u64) -> u32;
extern "fg_cuda_half_to_f32" fn cuda_half_to_f32(bits: u16) -> f32;
extern "fg_cuda_log2" fn cuda_log2(value: f32) -> f32;
extern "fg_cuda_exp2_ftz" fn cuda_exp2_ftz(value: f32) -> f32;
extern "fg_cuda_f32_to_i32_rz" fn cuda_f32_to_i32_rz(value: f32) -> i32;
extern "fg_cuda_shuffle_u32" fn cuda_shuffle_u32(value: u32, delta: i32) -> u32;
extern "fg_cuda_shuffle_bounded" fn cuda_shuffle_bounded(value: u32, delta: i32, bound: i32) -> u32;
extern "fg248d_ld_u8" fn cuda_load_byte(address: u64) -> u32;
extern "fg248d_st_f32" fn cuda_store_shared_float(address: u64, value: f32);
extern "fg_device_global_load" fn cuda_load_float(address: u64) -> f32;
extern "fg_device_global_store" fn cuda_store_float(address: u64, value: f32);
extern "fg_attn_word" fn cuda_config_word(cfg: u64, index: i64) -> i64;

export fn cuda_load_half(address: u64) -> f32 {
    return cuda_half_to_f32(cuda_half_word(address) as! u16);
}

export fn cuda_half_value(bits: u32) -> f32 {
    return cuda_half_to_f32(bits as! u16);
}

export fn cuda_load_byte_offset(address: u64, offset: u64) -> u32 {
    return cuda_load_byte(address + offset);
}

export fn cuda_load_half_offset(address: u64, offset: u64) -> f32 {
    return cuda_load_half(address + offset);
}

export fn cuda_store_shared_float_offset(address: u64, offset: u64, value: f32) {
    cuda_store_shared_float(address + offset, value);
}

export fn cuda_load_float_offset(address: u64, offset: u64) -> f32 {
    return cuda_load_float(address + offset);
}

export fn cuda_store_float_offset(address: u64, offset: u64, value: f32) {
    cuda_store_float(address + offset, value);
}

export fn cuda_load_float_index(address: u64, index: u64) -> f32 {
    return cuda_load_float(address + 4 * index);
}

export fn cuda_store_float_index(address: u64, index: u64, value: f32) {
    cuda_store_float(address + 4 * index, value);
}

export fn cuda_natural_log(value: f32) -> f32 {
    return cuda_log2(value) * 0.6931471805599453;
}

export fn cuda_natural_exp(value: f32) -> f32 {
    return cuda_exp2_ftz(value * 1.4426950408889634);
}

export fn cuda_negative_infinity() -> f32 {
    return cuda_bits_float(0xff800000);
}

export fn cuda_float_abs(value: f32) -> f32 {
    return cuda_bits_float(cuda_float_bits(value) & 0x7fffffff);
}

export fn cuda_float_to_i8(value: f32) -> i32 {
    if !(value >= -2147483648.0 && value < 2147483648.0) { return 0; }
    return (cuda_f32_to_i32_rz(value) as! i8) as i32;
}

export fn cuda_shuffle_float(value: f32, delta: i32) -> f32 {
    return cuda_bits_float(cuda_shuffle_u32(cuda_float_bits(value), delta));
}

export fn cuda_shuffle_float_bounded(value: f32, delta: i32, width: i32) -> f32 {
    let bound = ((32 - width) << 8) | 31;
    return cuda_bits_float(cuda_shuffle_bounded(cuda_float_bits(value), delta, bound));
}

export fn cuda_shuffle_double(value: f64, delta: i32) -> f64 {
    let bits = cuda_double_bits(value);
    let lo = cuda_shuffle_u32(bits as! u32, delta);
    let hi = cuda_shuffle_u32((bits >> 32) as u32, delta);
    return cuda_bits_double(((hi as u64) << 32) | (lo as u64));
}

export fn cuda_binary_exponent(exponent: i64) -> f64 {
    return cuda_bits_double(((exponent + 1023) as! u64) << 52);
}

export fn cuda_config_float(cfg: u64, index: i64) -> f32 {
    return cuda_bits_float(cuda_config_word(cfg, index) as! u32);
}

extern "fg_device_half_bits" fn cuda_f32_to_half(value: f32) -> u16;
extern "fg_cuda_f32_to_bf16" fn cuda_f32_to_bf16(value: f32) -> u16;
extern "fg_cuda_store_u16" fn cuda_store_u16(address: u64, value: u16);
extern "fg_cuda_store_shared_u16" fn cuda_store_shared_u16(address: u64, index: u64, value: u16);
extern "fg_cuda_load_shared_u16" fn cuda_load_shared_u16(address: u64, index: u64) -> u16;
extern "fg248d_ld_i8" fn cuda_load_i8(address: u64) -> i32;
extern "fg_cuda_shuffle_masked" fn cuda_shuffle_masked(value: u32, delta: i32, mask: u32) -> u32;
extern "fg_cuda_shuffle_up" fn cuda_shuffle_up(value: u32, delta: i32) -> u32;
extern "fg_cuda_shuffle_index" fn cuda_shuffle_index(value: u32, lane: i32) -> u32;

export fn cuda_round_to_half(value: f32) -> f32 {
    return cuda_half_to_f32(cuda_f32_to_half(value));
}

export fn cuda_store_half(address: u64, value: f32) {
    cuda_store_u16(address, cuda_f32_to_half(value));
}

export fn cuda_store_half_offset(address: u64, offset: u64, value: f32) {
    cuda_store_half(address + offset, value);
}

export fn cuda_store_half_pair(address: u64, offset: u64, lo: f32, hi: f32) {
    let at = address + offset;
    cuda_store_half(at, lo);
    cuda_store_half(at + 2, hi);
}

export fn cuda_pack_half_pair(lo: f32, hi: f32) -> u32 {
    return (cuda_f32_to_half(lo) as u32) | ((cuda_f32_to_half(hi) as u32) << 16);
}

export fn cuda_unpack_half_high(word: u32) -> f32 {
    return cuda_half_value(word >> 16);
}

export fn cuda_load_half_offset_high(address: u64, offset: u64) -> f32 {
    return cuda_load_half(address + offset + 2);
}

export fn cuda_load_i8_offset(address: u64, offset: u64) -> i32 {
    return cuda_load_i8(address + offset);
}

export fn cuda_load_shared_bf16(address: u64, index: u64) -> f32 {
    return cuda_bits_float((cuda_load_shared_u16(address, index) as u32) << 16);
}

export fn cuda_store_shared_bf16(address: u64, index: u64, value: f32) {
    cuda_store_shared_u16(address, index, cuda_f32_to_bf16(value));
}

export fn cuda_unpack_bf16_low(word: u32) -> f32 {
    return cuda_bits_float(word << 16);
}

export fn cuda_unpack_bf16_high(word: u32) -> f32 {
    return cuda_bits_float(word & 0xffff0000);
}

export fn cuda_shuffle_float_masked(value: f32, mask: u32, delta: i32) -> f32 {
    return cuda_bits_float(cuda_shuffle_masked(cuda_float_bits(value), delta, mask));
}

export fn cuda_shuffle_float_up(value: f32, delta: i32) -> f32 {
    return cuda_bits_float(cuda_shuffle_up(cuda_float_bits(value), delta));
}

export fn cuda_shuffle_float_index(value: f32, lane: i32) -> f32 {
    return cuda_bits_float(cuda_shuffle_index(cuda_float_bits(value), lane));
}

export fn cuda_shuffle_i32(value: i32, delta: i32) -> i32 {
    return cuda_shuffle_u32(value as! u32, delta) as! i32;
}

export fn cuda_shuffle_mmvq(value: f32, delta: i32, width: i32) -> f32 {
    return cuda_shuffle_float(value, delta);
}

export fn cuda_shuffle_fwht(value: f32, mask: u32, delta: i32, width: i32) -> f32 {
    return cuda_shuffle_float(value, delta);
}

export fn cuda_round_mmq(value: f32) -> f32 {
    if (cuda_float_bits(value) & 0x7f800000) == 0x7f800000 { return 0.0; }
    let integral = cuda_f32_to_i32_rz(value);
    let rest = value - (integral as f32);
    let correction = if rest >= 0.5 { 1 } else { if rest <= -0.5 { -1 } else { 0 } };
    return (integral + correction) as f32;
}

export fn cuda_load_float_offset_y(address: u64, offset: u64) -> f32 {
    return cuda_load_float(address + offset + 4);
}

export fn cuda_load_float_offset_z(address: u64, offset: u64) -> f32 {
    return cuda_load_float(address + offset + 8);
}

export fn cuda_load_float_offset_w(address: u64, offset: u64) -> f32 {
    return cuda_load_float(address + offset + 12);
}

export fn cuda_pow_integer(base: f32, exponent: i32) -> f32 {
    return cuda_natural_exp(cuda_natural_log(base) * (exponent as f32));
}

export fn cuda_round_i8(value: f32) -> i32 {
    return cuda_f32_to_i32_rz(cuda_round_mmq(value));
}

extern "fg_cuda_atomic_add_one" fn cuda_atomic_add_one(address: u32) -> u32;
extern "fg_cuda_raw_load_f32" fn cuda_raw_load_f32(address: u64) -> f32;
extern "fg_cuda_raw_store_f32" fn cuda_raw_store_f32(address: u64, value: f32);

export fn cuda_atomic_bin_increment(base: u64, index: i64) -> u32 {
    return cuda_atomic_add_one((base + ((4 * index) as! u64)) as! u32);
}

export fn cuda_raw_load_offset(address: u64, offset: u64) -> f32 {
    return cuda_raw_load_f32(address + offset);
}

export fn cuda_raw_store_offset(address: u64, offset: u64, value: f32) {
    cuda_raw_store_f32(address + offset, value);
}
