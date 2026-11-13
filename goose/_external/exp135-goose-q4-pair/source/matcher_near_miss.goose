extern fn fmaf(x: f32, y: f32, z: f32) -> f32;

fn ordinary_mask(input_a: u8[32], mask_bits: u8) -> u8[32] {
    var result_value: u8[32] = default<u8[32]>();
    for lane_index in 32 { result_value[lane_index] = input_a[lane_index] | mask_bits; }
    return result_value;
}
fn ordinary_shift(input_a: u8[32], shift_bits: u8) -> u8[32] {
    var result_value: u8[32] = default<u8[32]>();
    for lane_index in 32 { result_value[lane_index] = input_a[lane_index] >> shift_bits; }
    return result_value;
}
fn ordinary_permute(input_a: u8[32], permutation: u8[32]) -> u8[32] {
    var result_value: u8[32] = default<u8[32]>();
    for lane_index in 32 {
        let index = (lane_index / 16) * 16 + ((permutation[lane_index] & 15) as i64);
        result_value[lane_index] = if (permutation[lane_index] & 128) != 0 { 0 } else { input_a[index] };
    }
    return result_value;
}
fn ordinary_saturating_products(input_a: u8[32], input_b: i8[32]) -> i16[16] {
    var result_value: i16[16] = default<i16[16]>();
    for lane_index in 16 {
        let product_value: i32 = (input_a[2*lane_index] as i32) * (input_b[2*lane_index] as i32) + (input_a[2*lane_index+1] as i32) * (input_b[2*lane_index+1] as i32);
        result_value[lane_index] = (if product_value < -32768 { -32768 } else if product_value > 32767 { 32767 } else { product_value }) as! i16;
    }
    return result_value;
}
fn ordinary_pair_products(input_a: i16[16], input_b: i16[16]) -> i32[8] {
    var result_value: i32[8] = default<i32[8]>();
    for lane_index in 8 {
        let product_value: i64 = (input_a[2*lane_index] as i64) * (input_b[2*lane_index] as i64) + (input_a[2*lane_index+1] as i64) * (input_b[2*lane_index+1] as i64);
        result_value[lane_index] = product_value as! i32;
    }
    return result_value;
}
fn ordinary_fill(product_value: i16) -> i16[16] {
    var result_value: i16[16] = default<i16[16]>();
    for lane_index in 16 { result_value[lane_index] = product_value; }
    return result_value;
}
fn ordinary_add(input_a: i32[8], input_b: i32[8]) -> i32[8] {
    var result_value: i32[8] = default<i32[8]>();
    for lane_index in 8 { result_value[lane_index] = input_a[lane_index] + input_b[lane_index]; }
    return result_value;
}
fn ordinary_fma(factor: f32, product_value: i32[8], previous: f32[8]) -> f32[8] {
    var result_value: f32[8] = default<f32[8]>();
    for lane_index in 8 { result_value[lane_index] = fmaf(factor, product_value[lane_index] as f32, previous[lane_index]); }
    return result_value;
}

fn ordinary_sum_small(input_a: i16[16]) -> i32[8] {
    var result_value: i32[8] = default<i32[8]>();
    for lane_index in 8 { result_value[lane_index] = (input_a[2*lane_index] as i32) + (input_a[2*lane_index+1] as i32); }
    return result_value;
}
fn ordinary_multiply(input_a: i32[8], input_b: i32[8]) -> i32[8] {
    var result_value: i32[8] = default<i32[8]>();
    for lane_index in 8 { result_value[lane_index] = input_a[lane_index] * input_b[lane_index]; }
    return result_value;
}
fn ordinary_sum_large(input_a: i32[8]) -> i32[4] {
    var result_value: i32[4] = default<i32[4]>();
    for lane_index in 4 { result_value[lane_index] = input_a[2*lane_index] + input_a[2*lane_index+1]; }
    return result_value;
}
fn ordinary_fma_small(factor: f32, product_value: i32[4], previous: f32[4]) -> f32[4] {
    var result_value: f32[4] = default<f32[4]>();
    for lane_index in 4 { result_value[lane_index] = fmaf(factor, product_value[lane_index] as f32, previous[lane_index]); }
    return result_value;
}



fn main() {
    var hash: u64 = 1469598103934665603;
    for test in 1024 {
        var input_a: u8[32] = default<u8[32]>();
        var input_b: i8[32] = default<i8[32]>();
        var c: u8[32] = default<u8[32]>();
        for lane_index in 32 {
            input_a[lane_index] = (test*31+lane_index*17) as! u8;
            input_b[lane_index] = (test*13-lane_index*29) as! i8;
            c[lane_index] = (test+lane_index*7) as! u8;
        }
        let low = ordinary_mask(input_a, 15);
        let high = ordinary_shift(input_a, (test%256) as! u8);
        let shuffled = ordinary_permute(input_a, c);
        let p = ordinary_saturating_products(input_a, input_b);
        let scales = ordinary_fill((test*97) as! i16);
        let products = ordinary_pair_products(p, scales);
        let doubled = ordinary_add(products, products);
        var previous: f32[8] = default<f32[8]>();
        for lane_index in 8 { previous[lane_index] = (lane_index-test) as f32; }
        let f = ordinary_fma(0.125, products, previous);
        let pair16 = ordinary_sum_small(p);
        let multiplied = ordinary_multiply(products, doubled);
        let pair32 = ordinary_sum_large(multiplied);
        var prior4: f32[4] = default<f32[4]>();
        let fused4 = ordinary_fma_small(0.25, pair32, prior4);
        for lane_index in 8 {
            assert(pair16[lane_index] == (p[2*lane_index] as i32)+(p[2*lane_index+1] as i32));
            assert(multiplied[lane_index] == products[lane_index]*doubled[lane_index]);
        }
        for lane_index in 4 {
            assert(pair32[lane_index] == multiplied[2*lane_index]+multiplied[2*lane_index+1]);
            assert(fused4[lane_index] == fmaf(0.25, pair32[lane_index] as f32, prior4[lane_index]));
        }
        for lane_index in 32 {
            assert(low[lane_index] == (input_a[lane_index] | 15));
            assert(high[lane_index] == (input_a[lane_index] >> (test%256)));
            let j = (lane_index/16)*16 + ((c[lane_index]&15) as i64);
            assert(shuffled[lane_index] == (if (c[lane_index]&128)!=0 { 0 } else { input_a[j] }));
            hash = (hash ^ (shuffled[lane_index] as u64)) * 1099511628211;
        }
        for lane_index in 16 {
            let v = (input_a[2*lane_index] as i32)*(input_b[2*lane_index] as i32)+(input_a[2*lane_index+1] as i32)*(input_b[2*lane_index+1] as i32);
            let clipped = if v < -32768 { -32768 } else if v > 32767 { 32767 } else { v };
            assert(p[lane_index] == (clipped as! i16));
            hash = (hash ^ (p[lane_index] as! u64)) * 1099511628211;
        }
        for lane_index in 8 {
            let v = (p[2*lane_index] as i64)*(scales[2*lane_index] as i64)+(p[2*lane_index+1] as i64)*(scales[2*lane_index+1] as i64);
            assert(products[lane_index] == (v as! i32));
            assert(doubled[lane_index] == products[lane_index]+products[lane_index]);
            assert(f[lane_index] == fmaf(0.125, products[lane_index] as f32, previous[lane_index]));
            hash = (hash ^ (doubled[lane_index] as! u64)) * 1099511628211;
        }
    }
    print("PASS packed operations ", hash);
}
