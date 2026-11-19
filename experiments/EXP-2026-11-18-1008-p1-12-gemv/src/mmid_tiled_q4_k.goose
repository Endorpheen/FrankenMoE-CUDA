// Bit-exact Goose transcription of ggml_compute_forward_mul_mat_id_tiled for
// block_q4_K. The driver keeps the native window/thread split, the q4_K unpack
// and the AVX2 microkernel's integer accumulation and float epilogue.
import cpu_simd;
import packed_simd;

namespace mmid_tiled_q4_k;

// block_q4_K is 144 bytes: f16 d, f16 dmin, u8 scales[12], u8 qs[128].
// block_q8_K is 292 bytes: f32 d, i8 qs[256], i16 bsums[16].

fn min64(a: i64, b: i64) -> i64 {
    return if a < b { a } else { b };
}

fn u32_at(data: const u8[:], off: i64) -> u32 {
    return (data[off] as u32) | ((data[off + 1] as u32) << 8 as u8) | ((data[off + 2] as u32) << 16 as u8) | ((data[off + 3] as u32) << 24 as u8);
}

fn i32_at(data: const u8[:], off: i64) -> i32 {
    return ((data[off] as u32) | ((data[off + 1] as u32) << 8 as u8) | ((data[off + 2] as u32) << 16 as u8) | ((data[off + 3] as u32) << 24 as u8)) as! i32;
}

// ggml_fp16_to_fp32, same bit arithmetic the native conversion uses.
fn f16_at(data: const u8[:], off: i64) -> f32 {
    let half: u32 = (data[off] as u32) | ((data[off + 1] as u32) << 8 as u8);
    let sign: u32 = (half & 32768 as u32) << 16 as u8;
    let exponent: u32 = (half >> 10 as u8) & 31 as u32;
    var fraction: u32 = half & 1023 as u32;
    if exponent == 31 as u32 {
        return cpu_simd::f32_from_bits(sign | 2139095040 as u32 | (fraction << 13 as u8));
    }
    if exponent == 0 as u32 {
        if fraction == 0 as u32 {
            return cpu_simd::f32_from_bits(sign);
        }
        var power: i32 = -14;
        while (fraction & 1024 as u32) == 0 as u32 {
            fraction <<= 1 as u8;
            power -= 1;
        }
        fraction &= 1023 as u32;
        return cpu_simd::f32_from_bits(sign | (((power + 127) as u32) << 23 as u8) | (fraction << 13 as u8));
    }
    return cpu_simd::f32_from_bits(sign | ((exponent + 112 as u32) << 23 as u8) | (fraction << 13 as u8));
}

// 12 packed scale/min bytes -> eight scales and eight mins, exactly as
// tiled_unpack_src0 decodes them.
fn decode_scales(weights: const u8[:], base: i64, scales: i32[8]&, mins: i32[8]&) {
    let kmask1: u32 = 0x3F3F3F3F;
    let kmask2: u32 = 0x0F0F0F0F;
    let kmask3: u32 = 0x03030303;
    var u0: u32 = u32_at(weights, base + 4);
    var u1: u32 = u32_at(weights, base + 8);
    let u2: u32 = u32_at(weights, base + 12);
    var u3: u32 = ((u2 >> 4 as u8) & kmask2) | (((u1 >> 6 as u8) & kmask3) << 4 as u8);
    let aux: u32 = u1 & kmask1;
    u1 = (u2 & kmask2) | (((u0 >> 6 as u8) & kmask3) << 4 as u8);
    u0 &= kmask1;
    // scales = bytes of u0,u1 ; mins = bytes of aux,u3
    for s in 4 {
        scales[s] = ((u0 >> ((8 * s) as u8)) & 255 as u32) as! i32;
        scales[4 + s] = ((u1 >> ((8 * s) as u8)) & 255 as u32) as! i32;
        mins[s] = ((aux >> ((8 * s) as u8)) & 255 as u32) as! i32;
        mins[4 + s] = ((u3 >> ((8 * s) as u8)) & 255 as u32) as! i32;
    }
}

// One 256-value nibble unpack: subblock s lives in qs[32*(s/2)], low nibbles
// for even s and high nibbles for odd s.
fn unpack_q(weights: const u8[:], base: i64, q: u8[:]) {
    for h in 4 {
        let src = base + 16 + 32 * h;
        for b in 32 {
            let v = weights[src + b];
            q[64 * h + b] = v & 15 as u8;
            q[64 * h + 32 + b] = (v >> 4 as u8) & 15 as u8;
        }
    }
}

fn store_f32(dst: u8[:], off: i64, value: f32) {
    let bits: u32 = cpu_simd::f32_to_bits(value);
    for b in 4 {
        dst[off + b] = ((bits >> ((8 * b) as u8)) & 255 as u32) as! u8;
    }
}

fn mmid_tiled_q4_k(src0: const u8[:], ne00: i64, ne01: i64, nb01: i64,
                          act: const u8[:], nbw1: i64,
                          routes: const i32[:], cne1: i64,
                          nb0: i64, nb1: i64, nb2: i64,
                          dst: u8[:], ith: i32, nth: i32,
                          acc: f32[:], qbuf: u8[:]) -> i32 {
    let ngroups: i64 = (ne01 + 64 - 1) / 64;
    let g0: i64 = (ngroups * (ith as i64)) / (nth as i64);
    let g1: i64 = (ngroups * ((ith as i64) + 1)) / (nth as i64);
    if g0 >= g1 {
        return 0;
    }
    var k: i64 = 0;
    while k < cne1 {
        let nrows: i64 = min64(256, cne1 - k);
        var gi = g0;
        while gi < g1 {
            let r: i64 = gi * 64;
            let r_end: i64 = min64(r + 64, ne01);
            let n_src0: i64 = r_end - r;
            for i in n_src0 {
                for j in nrows {
                    acc[i * 256 + j] = 0.0;
                }
            }
            var ib: i64 = 0;
            while ib < ne00 {
                let kblk: i64 = ib / 256;
                var ii: i64 = 0;
                while ii < n_src0 {
                    let i: i64 = ii;
                    let wbase: i64 = (r + i) * nb01 + kblk * 144;
                    var scales: i32[8] = default<i32[8]>();
                    var mins: i32[8] = default<i32[8]>();
                    decode_scales(src0, wbase, scales, mins);
                    unpack_q(src0, wbase, qbuf);
                    let d: f32 = f16_at(src0, wbase);
                    let dmin: f32 = f16_at(src0, wbase + 2);
                    var jj: i64 = 0;
                    while jj < nrows {
                        let j: i64 = jj;
                        let abase: i64 = (k + j) * nbw1 + kblk * 292;
                        let q1: i64 = abase + 4;
                        var s1: i32 = 0;
                        var s2: i32 = 0;
                        for s in 8 {
                            var sub: i32 = 0;
                            for e in 32 {
                                let off: i64 = s * 32 + e;
                                sub += (qbuf[off] as i32) * ((act[q1 + off] as! i8) as i32);
                            }
                            s1 = (s1 as u32 + ((scales[s] * sub) as u32)) as! i32;
                            let bs: i32 = (act[abase + 260 + (2 * s) * 2] as i32) | (((act[abase + 261 + (2 * s) * 2] as! i8) as i32) << 8);
                            let bs2: i32 = (act[abase + 260 + (2 * s + 1) * 2] as i32) | (((act[abase + 261 + (2 * s + 1) * 2] as! i8) as i32) << 8);
                            let bsum: i32 = (bs as u32 + bs2 as u32) as! i32;
                            s2 = (s2 as u32 + ((mins[s] * bsum) as u32)) as! i32;
                        }
                        let dj: f32 = cpu_simd::f32_from_bits(u32_at(act, abase));
                        var res: f32 = d * (s1 as f32);
                        res = packed_simd::fmaf(-dmin, s2 as f32, res);
                        let slot: i64 = i * 256 + j;
                        acc[slot] = packed_simd::fmaf(dj, res, acc[slot]);
                        jj += 1;
                    }
                    ii += 1;
                }
                ib += 256;
            }
            for i in n_src0 {
                for m in nrows {
                    let off: i64 = (r + i) * nb0 + (routes[2 * (k + m)] as i64) * nb1 + (routes[2 * (k + m) + 1] as i64) * nb2;
                    store_f32(dst, off, acc[i * 256 + m]);
                }
            }
            gi += 1;
        }
        k += 256;
    }
    return 0;
}

fn main() {
    print("mmid tiled q4_k smoke");
}
