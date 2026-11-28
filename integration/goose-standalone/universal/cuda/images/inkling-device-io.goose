extern "inkling_f16_bits" fn inkling_f16_bits(value: f32) -> u16;
extern "inkling_f16_value" fn inkling_f16_value(bits: u16) -> f32;
extern "inkling_exp2_ftz" fn inkling_exp2_ftz(value: f32) -> f32;
extern "inkling_shared_u32" fn inkling_shared_u32(address: u64) -> u32;
extern "fg185q_ld_u16" fn inkling_load_u16_offset(address: u64, offset: u64) -> u32;
extern "fg248d_ld_u8" fn inkling_load_u8(address: u64) -> u32;
extern "fg248d_ld_i8" fn inkling_load_i8(address: u64) -> i32;
extern "fg_mmq_aligned_word" fn inkling_load_u32(address: u64) -> u32;
extern "fg_device_global_load" fn inkling_load_f32(address: u64) -> f32;
extern "fg_cuda_store_u16" fn inkling_store_u16(address: u64, value: u16);
extern "fg_cuda_store_shared_u16" fn inkling_store_shared_u16(address: u64, index: u64, value: u16);
extern "as_float" fn inkling_from_bits(bits: u32) -> f32;

export fn inkling_half(address: u64) -> f32 {
    return inkling_f16_value(inkling_load_u16_offset(address, 0) as! u16);
}

export fn inkling_half_bits(value: f32) -> u32 {
    return inkling_f16_bits(value) as u32;
}

export fn inkling_half_value(bits: u32) -> f32 {
    return inkling_f16_value(bits as! u16);
}

export fn inkling_write_half(address: u64, value: f32) {
    inkling_store_u16(address, inkling_f16_bits(value));
}

export fn inkling_write_half_at(address: u64, index: i64, value: f32) {
    inkling_write_half(address + ((index * 2) as! u64), value);
}

export fn inkling_write_probability(address: u64, index: i64, value: f32) {
    inkling_store_shared_u16(address, index as! u64, inkling_f16_bits(value));
}

export fn inkling_pack_half_pair(lo: f32, hi: f32) -> u32 {
    return inkling_half_bits(lo) | (inkling_half_bits(hi) << 16);
}

export fn inkling_read_half_pair(address: u64, index: i64) -> u32 {
    return inkling_load_u32(address + ((index * 2) as! u64));
}

export fn inkling_read_shared_half_pair(address: u64, index: i64) -> u32 {
    return inkling_shared_u32(address + ((index * 2) as! u64));
}

export fn inkling_pack_element(source: u64, output: u64, index: i64, slot: i64) {
    inkling_write_half_at(output, slot, inkling_load_f32(source + ((4 * index) as! u64)));
}

export fn inkling_q8_value(address: u64, lane: u32) -> f32 {
    return inkling_half(address) * (inkling_load_i8(address + 2 + (lane as u64)) as f32);
}

export fn inkling_negative_infinity() -> f32 {
    return inkling_from_bits(0xff800000);
}

export fn inkling_fast_exp(value: f32) -> f32 {
    return inkling_exp2_ftz(value * 1.4426950408889634);
}

extern "inkling_sub_half2" fn inkling_sub_half2(a: u32, b: u32) -> u32;
extern "inkling_mul_half2" fn inkling_mul_half2(a: u32, b: u32) -> u32;

export fn inkling_q4_product(encoded: u32, scale_pair: u32) -> u32 {
    return inkling_mul_half2(inkling_sub_half2(encoded, 0x64086408), scale_pair);
}
