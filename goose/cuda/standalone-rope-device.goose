extern "fg_device_global_load" fn rp_load32(address: u64) -> f32;
extern "fg248d_ld_half" fn rp_load16(address: u64) -> f32;
extern "fg_device_global_store" fn rp_store32(address: u64, value: f32);
extern "fg176r_st_f16" fn rp_store16(address: u64, offset: u64, value: f32);
extern fn fg_rope_copy32(src: u64, dst: u64);
extern fn fg_rope_copy16(src: u64, dst: u64);
extern fn fg_rope_round(value: f64) -> i64;
extern fn fg_rope_fma64(a: f64, b: f64, sum: f64) -> f64;
extern "as_uint" fn rp_float_bits(value: f32) -> u32;
extern "as_float" fn rp_bits_float(bits: u32) -> f32;
extern "fg_rope_sin" fn rp_sin_reduced(value: f32) -> f32;
extern "fg_rope_cos" fn rp_cos_reduced(value: f32) -> f32;

fn rp_load(address: u64, size: i64) -> f32 {
    return if size == 4 { rp_load32(address) } else { rp_load16(address) };
}

fn rp_store(address: u64, size: i64, value: f32) {
    if size == 4 { rp_store32(address, value); }
    else { rp_store16(address, 0, value); }
}

fn rp_copy(src: u64, dst: u64, size: i64) {
    if size == 4 {
        fg_rope_copy32(src, dst);
        fg_rope_copy32(src + 4, dst + 4);
    } else {
        fg_rope_copy16(src, dst);
        fg_rope_copy16(src + 2, dst + 2);
    }
}

// Split 2*pi into two doubles to preserve the remainder at long positions.
fn rp_reduce(theta: f32) -> f32 {
    if (rp_float_bits(theta) & 0x7f800000) == 0x7f800000 { return rp_bits_float(0x7fc00000); }
    let value = theta as f64;
    let turns = fg_rope_round(value * 0.15915494309189533577);
    let negative = -(turns as f64);
    let first = fg_rope_fma64(negative, 6.283185307179586, value);
    return fg_rope_fma64(negative, 2.4492935982947064e-16, first) as f32;
}

fn sin32(value: f32) -> f32 { return rp_sin_reduced(rp_reduce(value)); }
fn cos32(value: f32) -> f32 { return rp_cos_reduced(rp_reduce(value)); }
