import packed_simd;

fn main() {
    var hash: u64 = 1469598103934665603;
    for test in 1024 {
        var a: u8[32] = default<u8[32]>();
        var b: i8[32] = default<i8[32]>();
        var c: u8[32] = default<u8[32]>();
        for i in 32 {
            a[i] = (test*31+i*17) as! u8;
            b[i] = (test*13-i*29) as! i8;
            c[i] = (test+i*7) as! u8;
        }
        let low = packed_simd::and8(a, 15);
        let high = packed_simd::shr8(a, (test%256) as! u8);
        let shuffled = packed_simd::shuffle8(a, c);
        let p = packed_simd::maddubs(a, b);
        let scales = packed_simd::splat16((test*97) as! i16);
        let products = packed_simd::madd16(p, scales);
        let doubled = packed_simd::add32(products, products);
        var acc: f32[8] = default<f32[8]>();
        for i in 8 { acc[i] = (i-test) as f32; }
        let f = packed_simd::fma32(0.125, products, acc);
        for i in 32 {
            assert(low[i] == (a[i] & 15));
            assert(high[i] == (a[i] >> (test%256)));
            let j = (i/16)*16 + ((c[i]&15) as i64);
            assert(shuffled[i] == (if (c[i]&128)!=0 { 0 } else { a[j] }));
            hash = (hash ^ (shuffled[i] as u64)) * 1099511628211;
        }
        for i in 16 {
            let v = (a[2*i] as i32)*(b[2*i] as i32)+(a[2*i+1] as i32)*(b[2*i+1] as i32);
            let clipped = if v < -32768 { -32768 } else if v > 32767 { 32767 } else { v };
            assert(p[i] == (clipped as! i16));
            hash = (hash ^ (p[i] as! u64)) * 1099511628211;
        }
        for i in 8 {
            let v = (p[2*i] as i64)*(scales[2*i] as i64)+(p[2*i+1] as i64)*(scales[2*i+1] as i64);
            assert(products[i] == (v as! i32));
            assert(doubled[i] == products[i]+products[i]);
            assert(f[i] == packed_simd::fmaf(0.125, products[i] as f32, acc[i]));
            hash = (hash ^ (doubled[i] as! u64)) * 1099511628211;
        }
    }
    print("PASS packed operations ", hash);
}
