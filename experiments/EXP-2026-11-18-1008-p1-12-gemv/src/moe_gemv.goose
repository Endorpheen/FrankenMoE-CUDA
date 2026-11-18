// Exact Goose equivalent of the AVX2 body of ggml_gemv_q4_K_8x8_q8_K.
// The native kernel is transcribed lane for lane: same lane grouping, same
// integer wrapping, same FMA order, same final permutation and subtraction.
// No ggml function is called.
import cpu_simd;

namespace moe_gemv;

type u8x16 = u8[16];
type i8x32 = i8[32];
type u8x32 = u8[32];
type i16x16 = i16[16];
type i32x4 = i32[4];
type i32x8 = i32[8];
type f32x8 = f32[8];

// _mm256_shuffle_epi32 on a byte vector of eight dwords.
inline fn shuffle_dwords(a: u8x32, imm: i64) -> u8x32 {
    var out: u8x32 = default<u8x32>();
    for i in 32 {
        let half = i / 16;
        let j = ((i % 16) / 4) * 2;
        let b = i % 4;
        let sel = (imm >> j) & 3;
        out[i] = a[half * 16 + sel * 4 + b];
    }
    return out;
}

// _mm256_blend_epi32: dword lane d comes from b when bit d of imm is set.
inline fn blend32(a: u8x32, b: u8x32, imm: i64) -> u8x32 {
    var out: u8x32 = default<u8x32>();
    for i in 32 {
        let d = i / 4;
        let bpos = i % 4;
        out[i] = if ((imm >> d) & 1) != 0 { b[d * 4 + bpos] } else { a[d * 4 + bpos] };
    }
    return out;
}

// _mm256_permute2f128_si256(a, a, 0): low 128 bits into both halves.
inline fn bcast128(a: i8x32) -> i8x32 {
    var out: i8x32 = default<i8x32>();
    for i in 32 {
        out[i] = a[i % 16];
    }
    return out;
}

// _mm256_bsrli_epi128(a, 4): shift each 128-bit lane right two i16 lanes, zero fill.
inline fn bsrli4(a: i16x16) -> i16x16 {
    var out: i16x16 = default<i16x16>();
    for i in 16 {
        let half = i / 8;
        let j = i % 8;
        out[i] = if j < 6 { a[half * 8 + j + 2] } else { 0 as i16 };
    }
    return out;
}

// _mm_hadd_epi16 on two 128-bit halves.
inline fn hadd_i16(lo: i16x16, hi: i16x16) -> i16x16 {
    var out: i16x16 = default<i16x16>();
    for i in 8 {
        out[i] = ((lo[2 * i] as i64) + (lo[2 * i + 1] as i64)) as! i16;
    }
    for i in 8 {
        out[8 + i] = ((hi[2 * i] as i64) + (hi[2 * i + 1] as i64)) as! i16;
    }
    return out;
}

// _mm256_shuffle_epi32(a, 0) on i16 lanes: dword 0 into every dword.
inline fn bcast_dword16(a: i16x16) -> i16x16 {
    var out: i16x16 = default<i16x16>();
    for i in 16 {
        out[i] = a[i % 2];
    }
    return out;
}

// _mm_shuffle_epi32 on one 128-bit register.
inline fn shuffle32x4(a: i32x4, imm: i64) -> i32x4 {
    var out: i32x4 = default<i32x4>();
    for i in 4 {
        out[i] = a[(imm >> (2 * i)) & 3];
    }
    return out;
}

inline fn dwords_to_bytes(a: i32x4) -> u8x16 {
    var out: u8x16 = default<u8x16>();
    for i in 4 {
        let w: u32 = a[i] as! u32;
        for b in 4 {
            out[i * 4 + b] = ((w >> ((8 * b) as u8)) & 255) as! u8;
        }
    }
    return out;
}

// _mm_shuffle_epi8 (pshufb).
inline fn pshufb16(a: u8x16, control: u8x16) -> u8x16 {
    var out: u8x16 = default<u8x16>();
    for i in 16 {
        out[i] = a[(control[i] & 15) as i64];
    }
    return out;
}

// _mm_unpacklo_epi8.
inline fn unpacklo8(a: u8x16, b: u8x16) -> u8x16 {
    var out: u8x16 = default<u8x16>();
    for i in 8 {
        out[2 * i] = a[i];
        out[2 * i + 1] = b[i];
    }
    return out;
}

// _mm256_cvtepu8_epi16.
inline fn widen16(a: u8x16) -> i16x16 {
    var out: i16x16 = default<i16x16>();
    for i in 16 {
        out[i] = a[i] as! i16;
    }
    return out;
}

inline fn to_u8(a: i8x32) -> u8x32 {
    var out: u8x32 = default<u8x32>();
    for i in 32 {
        out[i] = a[i] as! u8;
    }
    return out;
}

inline fn to_i8(a: u8x32) -> i8x32 {
    var out: i8x32 = default<i8x32>();
    for i in 32 {
        out[i] = a[i] as! i8;
    }
    return out;
}

inline fn read_u32(data: const u8[:], offset: i64) -> u32 {
    return (data[offset] as u32) |
           ((data[offset + 1] as u32) << (8 as u8)) |
           ((data[offset + 2] as u32) << (16 as u8)) |
           ((data[offset + 3] as u32) << (24 as u8));
}

// read_f16 over a 16-byte local register.
fn f16_reg(data: u8x16, offset: i64) -> f32 {
    let half: u32 = (data[offset] as u32) | ((data[offset + 1] as u32) << (8 as u8));
    let sign = (half & 32768) << 16;
    let exponent = (half >> 10) & 31;
    var fraction = half & 1023;
    if exponent == 31 {
        return cpu_simd::f32_from_bits(sign | 2139095040 | (fraction << 13));
    }
    if exponent == 0 {
        if fraction == 0 {
            return cpu_simd::f32_from_bits(sign);
        }
        var power: i32 = -14;
        while (fraction & 1024) == 0 {
            fraction <<= 1;
            power -= 1;
        }
        fraction &= 1023;
        return cpu_simd::f32_from_bits(sign | (((power + 127) as u32) << 23) | (fraction << 13));
    }
    return cpu_simd::f32_from_bits(sign | ((exponent + 112) << 23) | (fraction << 13));
}

inline fn read_f32_le(data: const u8[:], offset: i64) -> f32 {
    return cpu_simd::f32_from_bits(read_u32(data, offset));
}

fn row_q4_k_x8(w: const u8[:], a: const u8[:], nb: i64, nc: i64, out: f32[:]) -> bool {
    assert(nb > 0 && nc > 0 && (nc % 8) == 0);
    assert(w.len == nc * nb * 144);
    assert(a.len == nb * 292);
    assert(out.len == nc);

    let mask4: u8x32 = cpu_simd::splat_u8(15);
    let kmask1: u32 = 0x3F3F3F3F;
    let kmask2: u32 = 0x0F0F0F0F;
    let kmask3: u32 = 0x03030303;

    let deltamask: u8x16 = [0, 1, 8, 9, 2, 3, 10, 11, 4, 5, 12, 13, 6, 7, 14, 15];
    let scalemask: u8x16 = [0, 0, 4, 4, 1, 1, 5, 5, 2, 2, 6, 6, 3, 3, 7, 7];
    let finalidx: i32x8 = [0, 2, 4, 6, 1, 3, 5, 7];

    for x in nc / 8 {
        var acc: f32x8 = default<f32x8>();
        var acc_min: f32x8 = default<f32x8>();

        for b in nb {
            let wb: i64 = (x * nb + b) * 1152;
            let ab: i64 = b * 292;

            var d_bytes: u8x16 = default<u8x16>();
            var dmin_bytes: u8x16 = default<u8x16>();
            for i in 16 {
                d_bytes[i] = w[wb + i] as! u8;
                dmin_bytes[i] = w[wb + 16 + i] as! u8;
            }
            let scale_bytes = pshufb16(d_bytes, deltamask);

            var col_scale: f32x8 = default<f32x8>();
            var col_dmin: f32x8 = default<f32x8>();
            for i in 8 {
                col_scale[i] = f16_reg(scale_bytes, i * 2);
                col_dmin[i] = f16_reg(dmin_bytes, i * 2);
            }

            let row_scale: f32 = read_f32_le(a, ab);

            var iacc_b: i32x8 = default<i32x8>();
            var iacc_min_b: i32x8 = default<i32x8>();

            var bsums: i16x16 = default<i16x16>();
            for i in 16 {
                bsums[i] = ((a[ab + 260 + i * 2] as u16) | ((a[ab + 261 + i * 2] as u16) << (8 as u8))) as! i16;
            }
            let q8s_h = hadd_i16(bsums, bsums);
            var q8s: i16x16 = default<i16x16>();
            for i in 16 {
                q8s[i] = q8s_h[i % 8];
            }

            for sb in 4 {
                let qo: i64 = wb + 128 + sb * 256;
                let w00 = cpu_simd::and_u8(cpu_simd::loadu_u8(w, qo + 0), mask4);
                let w10 = cpu_simd::and_u8(cpu_simd::loadu_u8(w, qo + 32), mask4);
                let w20 = cpu_simd::and_u8(cpu_simd::loadu_u8(w, qo + 64), mask4);
                let w30 = cpu_simd::and_u8(cpu_simd::loadu_u8(w, qo + 96), mask4);
                let w40 = cpu_simd::and_u8(cpu_simd::loadu_u8(w, qo + 128), mask4);
                let w50 = cpu_simd::and_u8(cpu_simd::loadu_u8(w, qo + 160), mask4);
                let w60 = cpu_simd::and_u8(cpu_simd::loadu_u8(w, qo + 192), mask4);
                let w70 = cpu_simd::and_u8(cpu_simd::loadu_u8(w, qo + 224), mask4);

                let w01 = cpu_simd::and_u8(cpu_simd::shr_u8(cpu_simd::loadu_u8(w, qo + 0), 4), mask4);
                let w11 = cpu_simd::and_u8(cpu_simd::shr_u8(cpu_simd::loadu_u8(w, qo + 32), 4), mask4);
                let w21 = cpu_simd::and_u8(cpu_simd::shr_u8(cpu_simd::loadu_u8(w, qo + 64), 4), mask4);
                let w31 = cpu_simd::and_u8(cpu_simd::shr_u8(cpu_simd::loadu_u8(w, qo + 96), 4), mask4);
                let w41 = cpu_simd::and_u8(cpu_simd::shr_u8(cpu_simd::loadu_u8(w, qo + 128), 4), mask4);
                let w51 = cpu_simd::and_u8(cpu_simd::shr_u8(cpu_simd::loadu_u8(w, qo + 160), 4), mask4);
                let w61 = cpu_simd::and_u8(cpu_simd::shr_u8(cpu_simd::loadu_u8(w, qo + 192), 4), mask4);
                let w71 = cpu_simd::and_u8(cpu_simd::shr_u8(cpu_simd::loadu_u8(w, qo + 224), 4), mask4);

                let u0_0 = read_u32(w, wb + 32 + 24 * sb);
                let u0_1 = read_u32(w, wb + 36 + 24 * sb);
                let u0_2 = read_u32(w, wb + 40 + 24 * sb);
                var ut0: i32x4 = default<i32x4>();
                ut0[3] = (((u0_2 >> 4) & kmask2) | (((u0_1 >> 6) & kmask3) << 4)) as! i32;
                let aux0: u32 = u0_1 & kmask1;
                ut0[1] = ((u0_2 & kmask2) | (((u0_0 >> 6) & kmask3) << 4)) as! i32;
                ut0[2] = aux0 as! i32;
                ut0[0] = (u0_0 & kmask1) as! i32;

                let u1_0 = read_u32(w, wb + 44 + 24 * sb);
                let u1_1 = read_u32(w, wb + 48 + 24 * sb);
                let u1_2 = read_u32(w, wb + 52 + 24 * sb);
                var ut1: i32x4 = default<i32x4>();
                ut1[3] = (((u1_2 >> 4) & kmask2) | (((u1_1 >> 6) & kmask3) << 4)) as! i32;
                let aux1: u32 = u1_1 & kmask1;
                ut1[1] = ((u1_2 & kmask2) | (((u1_0 >> 6) & kmask3) << 4)) as! i32;
                ut1[2] = aux1 as! i32;
                ut1[0] = (u1_0 & kmask1) as! i32;

                let ms0 = dwords_to_bytes(ut0);
                let ms1 = dwords_to_bytes(ut1);
                let scales_0: i16x16 = widen16(pshufb16(ms0, scalemask));
                let scales_1: i16x16 = widen16(pshufb16(ms1, scalemask));
                let mins_01: i16x16 = widen16(unpacklo8(dwords_to_bytes(shuffle32x4(ut0, 78)),
                                                       dwords_to_bytes(shuffle32x4(ut1, 78))));

                let l00 = bcast128(cpu_simd::loadu_i8(a, ab + 4 + sb * 64));
                let l01 = bcast128(cpu_simd::loadu_i8(a, ab + 20 + sb * 64));
                let l10 = bcast128(cpu_simd::loadu_i8(a, ab + 36 + sb * 64));
                let l11 = bcast128(cpu_simd::loadu_i8(a, ab + 52 + sb * 64));

                let a00 = to_i8(shuffle_dwords(to_u8(l00), 0));
                let a01 = to_i8(shuffle_dwords(to_u8(l00), 85));
                let a02 = to_i8(shuffle_dwords(to_u8(l00), 170));
                let a03 = to_i8(shuffle_dwords(to_u8(l00), 255));
                let a04 = to_i8(shuffle_dwords(to_u8(l01), 0));
                let a05 = to_i8(shuffle_dwords(to_u8(l01), 85));
                let a06 = to_i8(shuffle_dwords(to_u8(l01), 170));
                let a07 = to_i8(shuffle_dwords(to_u8(l01), 255));
                let a10 = to_i8(shuffle_dwords(to_u8(l10), 0));
                let a11 = to_i8(shuffle_dwords(to_u8(l10), 85));
                let a12 = to_i8(shuffle_dwords(to_u8(l10), 170));
                let a13 = to_i8(shuffle_dwords(to_u8(l10), 255));
                let a14 = to_i8(shuffle_dwords(to_u8(l11), 0));
                let a15 = to_i8(shuffle_dwords(to_u8(l11), 85));
                let a16 = to_i8(shuffle_dwords(to_u8(l11), 170));
                let a17 = to_i8(shuffle_dwords(to_u8(l11), 255));

                let s00 = shuffle_dwords(w00, 177);
                let s10 = shuffle_dwords(w10, 177);
                let s20 = shuffle_dwords(w20, 177);
                let s30 = shuffle_dwords(w30, 177);
                let s40 = shuffle_dwords(w40, 177);
                let s50 = shuffle_dwords(w50, 177);
                let s60 = shuffle_dwords(w60, 177);
                let s70 = shuffle_dwords(w70, 177);

                var iacc_0: i16x16 = default<i16x16>();
                iacc_0 = cpu_simd::add_i16(iacc_0, cpu_simd::maddubs(blend32(w00, s10, 170), a00));
                iacc_0 = cpu_simd::add_i16(iacc_0, cpu_simd::maddubs(blend32(s00, w10, 170), a01));
                iacc_0 = cpu_simd::add_i16(iacc_0, cpu_simd::maddubs(blend32(w20, s30, 170), a02));
                iacc_0 = cpu_simd::add_i16(iacc_0, cpu_simd::maddubs(blend32(s20, w30, 170), a03));
                iacc_0 = cpu_simd::add_i16(iacc_0, cpu_simd::maddubs(blend32(w40, s50, 170), a04));
                iacc_0 = cpu_simd::add_i16(iacc_0, cpu_simd::maddubs(blend32(s40, w50, 170), a05));
                iacc_0 = cpu_simd::add_i16(iacc_0, cpu_simd::maddubs(blend32(w60, s70, 170), a06));
                iacc_0 = cpu_simd::add_i16(iacc_0, cpu_simd::maddubs(blend32(s60, w70, 170), a07));

                let t01 = shuffle_dwords(w01, 177);
                let t11 = shuffle_dwords(w11, 177);
                let t21 = shuffle_dwords(w21, 177);
                let t31 = shuffle_dwords(w31, 177);
                let t41 = shuffle_dwords(w41, 177);
                let t51 = shuffle_dwords(w51, 177);
                let t61 = shuffle_dwords(w61, 177);
                let t71 = shuffle_dwords(w71, 177);
                var iacc_1: i16x16 = default<i16x16>();
                iacc_1 = cpu_simd::add_i16(iacc_1, cpu_simd::maddubs(blend32(w01, t11, 170), a10));
                iacc_1 = cpu_simd::add_i16(iacc_1, cpu_simd::maddubs(blend32(t01, w11, 170), a11));
                iacc_1 = cpu_simd::add_i16(iacc_1, cpu_simd::maddubs(blend32(w21, t31, 170), a12));
                iacc_1 = cpu_simd::add_i16(iacc_1, cpu_simd::maddubs(blend32(t21, w31, 170), a13));
                iacc_1 = cpu_simd::add_i16(iacc_1, cpu_simd::maddubs(blend32(w41, t51, 170), a14));
                iacc_1 = cpu_simd::add_i16(iacc_1, cpu_simd::maddubs(blend32(t41, w51, 170), a15));
                iacc_1 = cpu_simd::add_i16(iacc_1, cpu_simd::maddubs(blend32(w61, t71, 170), a16));
                iacc_1 = cpu_simd::add_i16(iacc_1, cpu_simd::maddubs(blend32(t61, w71, 170), a17));

                let p0: i32x8 = cpu_simd::madd16(iacc_0, scales_0);
                let p1: i32x8 = cpu_simd::madd16(iacc_1, scales_1);
                let iacc_sb: i32x8 = cpu_simd::add_i32(p0, p1);

                let q8s_sb: i16x16 = bcast_dword16(q8s);
                let iacc_min_sb: i32x8 = cpu_simd::madd16(q8s_sb, mins_01);
                q8s = bsrli4(q8s);

                iacc_b = cpu_simd::add_i32(iacc_b, iacc_sb);
                iacc_min_b = cpu_simd::add_i32(iacc_min_b, iacc_min_sb);
            }

            let prod_scale: f32x8 = cpu_simd::mul_f32(col_scale, cpu_simd::splat_f32(row_scale));
            let prod_dmin: f32x8 = cpu_simd::mul_f32(col_dmin, cpu_simd::splat_f32(row_scale));
            acc = cpu_simd::fma_f32(cpu_simd::cvt_i32_f32(iacc_b), prod_scale, acc);
            acc_min = cpu_simd::fma_f32(cpu_simd::cvt_i32_f32(iacc_min_b), prod_dmin, acc_min);
        }

        let permuted: f32x8 = cpu_simd::permute_f32(acc, finalidx);
        for i in 8 {
            out[x * 8 + i] = permuted[i] - acc_min[i];
        }
    }
    return true;
}

fn main() {
    print("moe_gemv smoke");
}
