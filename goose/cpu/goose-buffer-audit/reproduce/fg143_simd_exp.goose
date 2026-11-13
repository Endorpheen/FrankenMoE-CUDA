fn avx2_exp_compat(x: f32) -> f32 {
    let z = fma32(x, from_bits(C_LOG2E), 12582912 as f32);
    let n = z - (12582912 as f32);
    let b = fma32(-n, from_bits(C_LN2_LO), fma32(-n, from_bits(C_LN2_HI), x));
    let e = bits(z) << 23;
    let k = from_bits( ( (e as u64) + (0x3f800000 as u64)) as !u32);
    let u = b * b;
    let j = fma32(
        fma32(fma32(from_bits(C_P0), b, from_bits(C_P1)), u, fma32(from_bits(C_P2), b, from_bits(C_P3))),
        u,
        from_bits(C_P4) * b);

    if ! (abs32(n) > (126 as f32)) {
        return fma32(j, k, k);
    }
    let g: u32 = if n <= (0 as f32) {
        0x82000000 as u32
    } else {
        0 as u32
    }
    ;
    let s1 = from_bits( ( (g as u64) + (0x7f000000 as u64)) as !u32);
    let s2 = from_bits( ( (e as u64) + (0x100000000 as u64) - (g as u64)) as !u32);

    if abs32(n) > (192 as f32) {
        return s1 * s1;
    }
    return fma32(s2, j, s2) * s1;
}

fn silu_compat(p: const Node &, x: f32, col: i64, width: i64) -> f32 {
    if p.ip[12] != 0 && p.x.kind == 0 && p.d.kind == 0 && col < (width / 8) * 8 {
        return x / ( (1 as f32) + avx2_exp_compat( (0 as f32) - x));
    }
    return silu(x);
}

