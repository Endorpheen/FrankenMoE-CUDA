extern "memcpy" fn host_quant_copy(dst: c_ptr<void>, src: c_ptr<const void>, bytes: u64) -> c_ptr<void>;
extern "fabsf" fn host_quant_abs(value: f32) -> f32;
extern "floorf" fn host_quant_floor(value: f32) -> f32;
extern "ldexpf" fn host_quant_ldexp(value: f32, exponent: i32) -> f32;
extern "log2f" fn host_quant_log(value: f32) -> f32;
extern "sqrtf" fn host_quant_sqrt(value: f32) -> f32;
extern "gs_os_write_stderr" fn host_quant_error(message: const u8[:]);
extern "abort" fn host_quant_abort();

var host_quant_scratch: u64[2050];

fn host_quant_bits(value: f32) -> u32 {
    var bits: u32 = 0;
    host_quant_copy(unsafe_c_address<c_ptr<void>>(bits), unsafe_c_address<c_ptr<const void>>(value), 4);
    return bits;
}

fn host_quant_float(bits: u32) -> f32 {
    var value: f32 = 0.0 as! f32;
    host_quant_copy(unsafe_c_address<c_ptr<void>>(value), unsafe_c_address<c_ptr<const void>>(bits), 4);
    return value;
}

fn host_quant_half_to_float(half: u16) -> f32 {
    let word = (half as! u32) << 16;
    let sign = word & 0x80000000;
    let twice = word + word;
    let normal = host_quant_float((twice >> 4) + 0x70000000) * host_quant_float(0x07800000);
    let subnormal = host_quant_float((twice >> 17) | 0x3f000000) - (0.5 as! f32);
    let bits = if twice < 0x08000000 { host_quant_bits(subnormal) } else { host_quant_bits(normal) };
    return host_quant_float(sign | bits);
}

fn host_quant_float_to_half(value: f32) -> u16 {
    let scale_to_inf = host_quant_float(0x77800000);
    let scale_to_zero = host_quant_float(0x08800000);
    var base = (host_quant_abs(value) * scale_to_inf) * scale_to_zero;
    let word = host_quant_bits(value);
    let twice = word + word;
    let sign = word & 0x80000000;
    var bias = twice & 0xff000000;
    if bias < 0x71000000 { bias = 0x71000000; }
    base = host_quant_float((bias >> 1) + 0x07800000) + base;
    let bits = host_quant_bits(base);
    let fraction = ((bits >> 13) & 0x7c00) + (bits & 0x0fff);
    return ((sign >> 16) | (if twice > 0xff000000 { 0x7e00 } else { fraction })) as! u16;
}

fn host_quant_load_f16(address: u64) -> f32 {
    var bits: u16 = 0;
    host_quant_copy(unsafe_c_address<c_ptr<void>>(bits), unsafe_c_ptr<c_ptr<const void>>(address), 2);
    return host_quant_half_to_float(bits);
}

fn host_quant_load_f32(address: u64) -> f32 {
    var value: f32 = 0.0 as! f32;
    host_quant_copy(unsafe_c_address<c_ptr<void>>(value), unsafe_c_ptr<c_ptr<const void>>(address), 4);
    return value;
}

fn host_quant_load_u8(address: u64) -> i64 {
    var value: u8 = 0;
    host_quant_copy(unsafe_c_address<c_ptr<void>>(value), unsafe_c_ptr<c_ptr<const void>>(address), 1);
    return value as! i64;
}

fn host_quant_store_f16(address: u64, value: f32) {
    let bits = host_quant_float_to_half(value);
    host_quant_copy(unsafe_c_ptr<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(bits), 2);
}

fn host_quant_store_f32(address: u64, value: f32) {
    host_quant_copy(unsafe_c_ptr<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(value), 4);
}

fn host_quant_store_u8(address: u64, value: i64) {
    let bits = value as! u8;
    host_quant_copy(unsafe_c_ptr<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(bits), 1);
}

fn host_quant_store_u16(address: u64, value: i64) {
    let bits = value as! u16;
    host_quant_copy(unsafe_c_ptr<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(bits), 2);
}

fn host_quant_store_u32(address: u64, value: i64) {
    let bits = value as! u32;
    host_quant_copy(unsafe_c_ptr<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(bits), 4);
}

fn host_quant_nearest(value: f32) -> i64 {
    let bits = host_quant_bits(value + (12582912.0 as! f32)) & 0x007fffff;
    return (bits as! i64) - 0x00400000;
}

fn host_quant_tmp(offset: i64) -> u64 {
    let pointer = unsafe_c_address<c_ptr<const void>>(host_quant_scratch[0]);
    var address: u64 = 0;
    host_quant_copy(unsafe_c_address<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(pointer), 8);
    return ((address + 15) & 0xfffffffffffffff0) + (offset as! u64);
}

fn host_quant_fail(code: i64) {
    host_quant_error(str("fg-goose: module failure ", code, "\n"));
    host_quant_abort();
}

export fn qd_bits(v: f32) -> i64 { return host_quant_bits(v) as! i64; }
export fn qd_of_bits(b: i64) -> f32 { return host_quant_float(b as! u32); }

export fn qz2_abs(v: f32) -> f32 { return host_quant_abs(v); }
export fn qz2_bits(v: f32) -> i64 { return host_quant_bits(v) as! i64; }
export fn qz2_fail(code: i64) { host_quant_fail(code); }
export fn qz2_floorf(v: f32) -> f32 { return host_quant_floor(v); }
export fn qz2_ld_f16(a: u64) -> f32 { return host_quant_load_f16(a); }
export fn qz2_ld_f32(a: u64) -> f32 { return host_quant_load_f32(a); }
export fn qz2_ld_u8(a: u64) -> i64 { return host_quant_load_u8(a); }
export fn qz2_ldexpf(v: f32, e: i64) -> f32 { return host_quant_ldexp(v, e as! i32); }
export fn qz2_log2f(v: f32) -> f32 { return host_quant_log(v); }
export fn qz2_lroundf(v: f32) -> i64 { return host_quant_lround(v) as! i64; }
export fn qz2_nearest(v: f32) -> i64 { return host_quant_nearest(v); }
export fn qz2_of_bits(b: i64) -> f32 { return host_quant_float(b as! u32); }
export fn qz2_sqrtf(v: f32) -> f32 { return host_quant_sqrt(v); }
export fn qz2_st_f16(a: u64, v: f32) { host_quant_store_f16(a, v); }
export fn qz2_st_f32(a: u64, v: f32) { host_quant_store_f32(a, v); }
export fn qz2_st_u16(a: u64, v: i64) { host_quant_store_u16(a, v); }
export fn qz2_st_u32(a: u64, v: i64) { host_quant_store_u32(a, v); }
export fn qz2_st_u8(a: u64, v: i64) { host_quant_store_u8(a, v); }
export fn qz2_tmp(off: i64) -> u64 { return host_quant_tmp(off); }

export fn qz_abs(v: f32) -> f32 { return host_quant_abs(v); }
export fn qz_bits(v: f32) -> i64 { return host_quant_bits(v) as! i64; }
export fn qz_fail(code: i64) { host_quant_fail(code); }
export fn qz_floorf(v: f32) -> f32 { return host_quant_floor(v); }
export fn qz_ld_f16(a: u64) -> f32 { return host_quant_load_f16(a); }
export fn qz_ld_f32(a: u64) -> f32 { return host_quant_load_f32(a); }
export fn qz_ld_u8(a: u64) -> i64 { return host_quant_load_u8(a); }
export fn qz_ldexpf(v: f32, e: i64) -> f32 { return host_quant_ldexp(v, e as! i32); }
export fn qz_log2f(v: f32) -> f32 { return host_quant_log(v); }
export fn qz_lroundf(v: f32) -> i64 { return host_quant_lround(v) as! i64; }
export fn qz_nearest(v: f32) -> i64 { return host_quant_nearest(v); }
export fn qz_of_bits(b: i64) -> f32 { return host_quant_float(b as! u32); }
export fn qz_sqrtf(v: f32) -> f32 { return host_quant_sqrt(v); }
export fn qz_st_f16(a: u64, v: f32) { host_quant_store_f16(a, v); }
export fn qz_st_f32(a: u64, v: f32) { host_quant_store_f32(a, v); }
export fn qz_st_u16(a: u64, v: i64) { host_quant_store_u16(a, v); }
export fn qz_st_u32(a: u64, v: i64) { host_quant_store_u32(a, v); }
export fn qz_st_u8(a: u64, v: i64) { host_quant_store_u8(a, v); }
export fn qz_tmp(off: i64) -> u64 { return host_quant_tmp(off); }
