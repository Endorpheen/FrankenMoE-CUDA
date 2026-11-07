namespace packed_simd;
extern "fmaf" fn fmaf(x: f32, y: f32, z: f32) -> f32;

fn and8(a: u8[32], mask: u8) -> u8[32] {
    var out: u8[32] = default<u8[32]>();
    for i in 32 { out[i] = a[i] & mask; }
    return out;
}
fn shr8(a: u8[32], count: u8) -> u8[32] {
    var out: u8[32] = default<u8[32]>();
    for i in 32 { out[i] = a[i] >> count; }
    return out;
}
fn shuffle8(a: u8[32], control: u8[32]) -> u8[32] {
    var out: u8[32] = default<u8[32]>();
    for i in 32 {
        let index = (i / 16) * 16 + ((control[i] & 15) as i64);
        out[i] = if (control[i] & 128) != 0 { 0 } else { a[index] };
    }
    return out;
}
fn maddubs(a: u8[32], b: i8[32]) -> i16[16] {
    var out: i16[16] = default<i16[16]>();
    for i in 16 {
        let value: i32 = (a[2*i] as i32) * (b[2*i] as i32) + (a[2*i+1] as i32) * (b[2*i+1] as i32);
        out[i] = (if value < -32768 { -32768 } else if value > 32767 { 32767 } else { value }) as! i16;
    }
    return out;
}
fn madd16(a: i16[16], b: i16[16]) -> i32[8] {
    var out: i32[8] = default<i32[8]>();
    for i in 8 {
        let value: i64 = (a[2*i] as i64) * (b[2*i] as i64) + (a[2*i+1] as i64) * (b[2*i+1] as i64);
        out[i] = value as! i32;
    }
    return out;
}
fn splat16(value: i16) -> i16[16] {
    var out: i16[16] = default<i16[16]>();
    for i in 16 { out[i] = value; }
    return out;
}
fn add32(a: i32[8], b: i32[8]) -> i32[8] {
    var out: i32[8] = default<i32[8]>();
    for i in 8 { out[i] = a[i] + b[i]; }
    return out;
}
fn fma32(scale: f32, value: i32[8], acc: f32[8]) -> f32[8] {
    var out: f32[8] = default<f32[8]>();
    for i in 8 { out[i] = fmaf(scale, value[i] as f32, acc[i]); }
    return out;
}

fn pair_sum16(a: i16[16]) -> i32[8] {
    var out: i32[8] = default<i32[8]>();
    for i in 8 { out[i] = (a[2*i] as i32) + (a[2*i+1] as i32); }
    return out;
}
fn mul32(a: i32[8], b: i32[8]) -> i32[8] {
    var out: i32[8] = default<i32[8]>();
    for i in 8 { out[i] = a[i] * b[i]; }
    return out;
}
fn pair_sum32(a: i32[8]) -> i32[4] {
    var out: i32[4] = default<i32[4]>();
    for i in 4 { out[i] = a[2*i] + a[2*i+1]; }
    return out;
}
fn fma4(scale: f32, value: i32[4], acc: f32[4]) -> f32[4] {
    var out: f32[4] = default<f32[4]>();
    for i in 4 { out[i] = fmaf(scale, value[i] as f32, acc[i]); }
    return out;
}
