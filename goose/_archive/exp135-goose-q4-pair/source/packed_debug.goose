import packed_simd;
fn main() {
    var bytes: u8[32] = [255; 32];
    var signs: i8[32] = [1; 32];
    let masked = packed_simd::and8(bytes, 15);
    let shifted = packed_simd::shr8(bytes, 8);
    let shuffled = packed_simd::shuffle8(bytes, bytes);
    let pairs = packed_simd::maddubs(bytes, signs);
    let scales = packed_simd::splat16(1);
    let values = packed_simd::madd16(pairs, scales);
    let doubled = packed_simd::add32(values, values);
    let sums = packed_simd::pair_sum16(pairs);
    let multiplied = packed_simd::mul32(values, doubled);
    let reduced = packed_simd::pair_sum32(multiplied);
    var previous: f32[8] = [0; 8];
    var previous4: f32[4] = [0; 4];
    let fused = packed_simd::fma32(0.5, values, previous);
    let fused4 = packed_simd::fma4(0.5, reduced, previous4);
    for i in 32 { assert(masked[i] == 15 && shifted[i] == 255 && shuffled[i] == 0); }
    for i in 8 { assert(values[i] == 1020 && doubled[i] == 2040 && sums[i] == 1020 && fused[i] == 510); }
    for i in 4 { assert(reduced[i] == 4161600 && fused4[i] == 2080800); }
    print("PASS debug fallback");
}
