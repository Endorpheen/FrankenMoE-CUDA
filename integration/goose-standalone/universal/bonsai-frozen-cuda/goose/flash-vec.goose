// fg175v: Apex FlashAttention decode vector device path as Goose, from fattn-vec.cuh
// flash_attn_ext_vec plus its fattn-common.cuh helpers quantize_q8_1_to_shared<float2,32>,
// vec_dot_fattn_vec_KQ_q4_0<256,32>, dequantize_V_q4_0<float,4> and
// flash_attn_combine_results<256>. Fixed target: D=256, Q=F32, K=V=Q4_0,
// ncols in {1,2} (runtime parameter), WARP_SIZE=32, nthreads=128 (nwarps=4),
// use_logit_softcap=false template instance (softcap variant deferred).
// All math stays here: dp4a Q4_0-vs-Q8_1 dot on raw nibble words, half block scales,
// online softmax, warp/block reductions, KQ/VKQ staging. Only global/shared data
// movement, shuffles, expf/powf/roundf, h2f, dp4a and barriers are effects.
// ggml_cuda_pdl_lc/pdl_sync deferred (parent owns PDL launch attributes).
// No V_DOT2_F32_F16_AVAILABLE (CUDA path): KQ probs and VKQ accumulate in f32.
// Shared word map (f32/u32 words, 1152 words = 4608 B): KQ buffer 0..1024
// (Q q8_1 staging per column j at j*256: 64 i32 words + 16 f32 words of (d,sum) pairs
// at j*256+64; probs KQ[j*128+tid]; fv_combine staging warp slice w at w*256..w*256+255),
// KQ_max_shared[j] at 1024+j*32, KQ_sum_shared[j] at 1088+j*32.
// Byte-vs-element: every address is a byte address; element indices are multiplied
// by explicit typesizes (f32 *4, half *2, i32 *4, block_q4_0 stride 18 = 2 + 16).
// Parent verified the supplied native ggml-common.h block_q4_0 layout and constants.
extern "fg175v_mul_rn" fn mul_rn(a: f32, b: f32) -> f32;
extern "fg175v_fma_rn" fn fma_rn(a: f32, b: f32, c: f32) -> f32;
struct VecFlash4 {
    v0: f32, v1: f32, v2: f32, v3: f32
}
extern "fg175v_ld_f32" fn ld_f32(p: u64, a: u64) -> f32;
extern "fg175v_st_f32" fn st_f32(p: u64, a: u64, v: f32);
extern "fg175v_ld_u16" fn ld_u16(p: u64, a: u64) -> u32;
extern "fg175v_ld_i32" fn ld_i32(p: u64, a: u64) -> i32;
extern "fg175v_st_i32" fn st_i32(p: u64, a: u64, v: i32);
extern "fg175v_h2f" fn h2f(w: u32) -> f32;
extern "fg175v_dp4a" fn dp4a(a: i32, b: i32, c: i32) -> i32;
extern "fg175v_expf" fn expf(x: f32) -> f32;
extern "fg175v_powf" fn powf(b: f32, e: i32) -> f32;
extern "fg175v_fmaxf" fn fmaxf(a: f32, b: f32) -> f32;
extern "fg175v_round_i8" fn round_i8(x: f32) -> i32;
extern "fg175v_shfl_xor" fn shfl_xor(v: f32, o: i32) -> f32;
extern "fg175v_syncwarp" fn syncwarp();
extern "fg175v_barrier" fn barrier();
extern "fg175v_lc" fn lc();
extern "fg175v_sync" fn pdl_sync();
extern "fg175v_sm_f32" fn sm_f32(p: u64, i: u64) -> f32;
extern "fg175v_sm_st_f32" fn sm_st_f32(p: u64, i: u64, v: f32);
extern "fg175v_sm_i32" fn sm_i32(p: u64, i: u64) -> i32;
extern "fg175v_sm_st_i32" fn sm_st_i32(p: u64, i: u64, v: i32);
// -FLT_MAX/2 = -(2^127 - 2^103), exactly representable in f32.

fn fv_negmax() -> f32 {
    return -170141173319264429905852091742258332672.0;
}
// fabsf exact, including -0.0 -> +0.0.

fn fv_absf(v: f32) -> f32 {
    if v <= 0.0 {
        return 0.0 - v;
    }
    return v;
}
// warp_reduce_sum<32>: xor shuffle offsets 16,8,4,2,1.

fn fv_wr_sum(v: f32) -> f32 {
    var x = v;
    x += shfl_xor(x, 16);
    x += shfl_xor(x, 8);
    x += shfl_xor(x, 4);
    x += shfl_xor(x, 2);
    x += shfl_xor(x, 1);
    return x;
}
// warp_reduce_max<32>.

fn fv_wr_max(v: f32) -> f32 {
    var x = v;
    x = fmaxf(x, shfl_xor(x, 16));
    x = fmaxf(x, shfl_xor(x, 8));
    x = fmaxf(x, shfl_xor(x, 4));
    x = fmaxf(x, shfl_xor(x, 2));
    x = fmaxf(x, shfl_xor(x, 1));
    return x;
}
// get_alibi_slope(common.cuh): powf kept as the source libm call via primitive.

fn fv_alibi_slope(max_bias: f32, h: i32, nh2: i32, m0: f32, m1: f32) -> f32 {
    if max_bias <= 0.0 {
        return 1.0;
    }
    let base = if h < nh2 {
        m0
    } else {
        m1
    }
    ;
    let exph = if h < nh2 {
        h + 1
    } else {
        2 * (h - nh2) + 1
    }
    ;
    return powf(base, exph);
}
// ggml_cuda_memcpy_1<4,2> + (v>>4*shift)&0x0F0F0F0F of block_q4_0 qs (16 B at byte 2 of
// the 18-byte block, half d at byte 0). Two aligned-2 halfword loads form the u32 word
// (lo + 65536*hi); per-byte unsigned div/mod extracts nibbles (arithmetic shift + mask
// equals logical shift + mask on these lanes); nibbles repack disjointly by Horner.

fn fv_q4_word(base: u64, byte: i32, sh: i32) -> i32 {
    let lo = ld_u16(base, byte as u64);
    let hi = ld_u16(base, (byte + 2) as u64);
    let word = lo | (hi << (16 as u32));
    let nib = (word >> ( (sh * 4) as u32)) & (252645135 as u32);
    return nib as !i32;
}
// vec_dot_fattn_vec_KQ_q4_0<D=256,nthreads=32>: k_KQ_0 in {0,32}; ib=k_KQ/QI8_1 (QI8_1=8),
// iqs4=k_KQ%QI4_0 (QI4_0=4 ints -> byte 4*iqs4), shift=k_KQ&(QI8_1/2)=k_KQ&4. dp4a of the raw
// K nibble word against the Q q8_1 word; scale = half d * (sumi*Q_ds.x - (8/QI8_1)*Q_ds.y)
// with 8/QI8_1 == 1 exactly.

fn fv_vec_dot_kq4(kb: u64, tx: i32, u0: i32, u1: i32, d0x: f32, d0y: f32, d1x: f32, d1y: f32) -> f32 {
    var sum: f32 = 0.0;
    var h: i32 = 0;

    if true {
        let h: i32 = 0;
        let kk = h * 32 + tx;
        let b = kb + ( ( ( (kk / 8) as i64) * 18) as u64);
        let iq4 = kk % 4;
        let sh = (kk % 8) / 4;
        let v = fv_q4_word(b, 2 + 4 * iq4, sh);
        let sumi = dp4a(v, if h == 0 {
            u0
        } else {
            u1
        }
        , 0);
        let d4 = h2f(ld_u16(b, 0));
        let qx = if h == 0 {
            d0x
        } else {
            d1x
        }
        ;
        let qy = if h == 0 {
            d0y
        } else {
            d1y
        }
        ;
        sum += d4 * ( (sumi as f32) * qx - qy);
    }

    if true {
        let h: i32 = 1;
        let kk = h * 32 + tx;
        let b = kb + ( ( ( (kk / 8) as i64) * 18) as u64);
        let iq4 = kk % 4;
        let sh = (kk % 8) / 4;
        let v = fv_q4_word(b, 2 + 4 * iq4, sh);
        let sumi = dp4a(v, if h == 0 {
            u0
        } else {
            u1
        }
        , 0);
        let d4 = h2f(ld_u16(b, 0));
        let qx = if h == 0 {
            d0x
        } else {
            d1x
        }
        ;
        let qy = if h == 0 {
            d0y
        } else {
            d1y
        }
        ;
        sum += d4 * ( (sumi as f32) * qx - qy);
    }
    h = (2 as i32);
    return sum;
}
// dequantize_V_q4_0<float,4>: ib=i0/QK4_0 (QK4_0=32), iqs=i0%(QK4_0/2) bytes, shift=(i0%QK4_0)/(QK4_0/2).
// Nibbles -8 per lane (__vsub4 + int8 view) times half d, all four elements.

fn fv_dequant_v4(base: u64, i0: i64) -> VecFlash4 {
    let p = base + ( ( (i0 / 32) * 18) as u64);
    let qs = (i0 % 16) as u64;
    let word = ld_u16(p, 2 + qs) | (ld_u16(p, 4 + qs) << (16 as u32));
    let nib = (word >> ( ( ( (i0 % 32) / 16) * 4) as u32)) & (252645135 as u32);
    let d = h2f(ld_u16(p, 0));
    return VecFlash4 {
        d * ( (nib & (15 as u32)) as f32 - 8.0), d * ( ( (nib >> (8 as u32)) & (15 as u32)) as f32 - 8.0), d * ( ( (nib >> (16 as u32)) & (15 as u32)) as f32 - 8.0), d * ( ( (nib >> (24 as u32)) & (15 as u32)) as f32 - 8.0)
    }
    ;
}
// quantize_q8_1_to_shared<float2,32>: ni==WARP_SIZE; per-lane 4 scaled values, amax/sum
// xor-reduced over QI8_1/2=4,2,1 (8 lanes = one 32-element q8_1 block); d=amax/127;
// q8=roundf(v/d) packed little-endian by Horner (exact two's complement bytes);
// yq32[threadIdx.x] and yds[threadIdx.x/QI8_1]=(d,sum) at words yds+2*(tx/8),+1.

fn fv_quant_q8(xb: u64, scale: f32, yq: i64, yds: i64, tx: i32, smem: u64) {
    let v0 = scale * ld_f32(xb, ( (4 * tx) * 4) as u64);
    let v1 = scale * ld_f32(xb, ( (4 * tx + 1) * 4) as u64);
    let v2 = scale * ld_f32(xb, ( (4 * tx + 2) * 4) as u64);
    let v3 = scale * ld_f32(xb, ( (4 * tx + 3) * 4) as u64);
    var amax = fv_absf(v0);
    var sum = v0;
    amax = fmaxf(amax, fv_absf(v1));
    sum += v1;
    amax = fmaxf(amax, fv_absf(v2));
    sum += v2;
    amax = fmaxf(amax, fv_absf(v3));
    sum += v3;
    amax = fmaxf(amax, shfl_xor(amax, 4));
    sum += shfl_xor(sum, 4);
    amax = fmaxf(amax, shfl_xor(amax, 2));
    sum += shfl_xor(sum, 2);
    amax = fmaxf(amax, shfl_xor(amax, 1));
    sum += shfl_xor(sum, 1);
    let d = amax / 127.0;
    var q32: i32 = 0;

    if d != 0.0 {
        let c0 = round_i8(v0 / d);
        let c1 = round_i8(v1 / d);
        let c2 = round_i8(v2 / d);
        let c3 = round_i8(v3 / d);
        let b0 = (c0 as !u32) & (255 as u32);
        let b1 = (c1 as !u32) & (255 as u32);
        let b2 = (c2 as !u32) & (255 as u32);
        let b3 = (c3 as !u32) & (255 as u32);
        q32 = (b0 | (b1 << (8 as u32)) | (b2 << (16 as u32)) | (b3 << (24 as u32))) as !i32;
    }
    sm_st_i32(smem, ( (yq + (tx as i64))) as u64, q32);

    if tx % 8 == 0 {
        sm_st_f32(smem, ( (yds + ( (2 * (tx / 8)) as i64))) as u64, d);
        sm_st_f32(smem, ( (yds + ( (2 * (tx / 8)) as i64) + 1)) as u64, sum);
    }
}

// Use the vector path's original query quantizer and word layout.
fn fv_quant_q8_head(q: u64, scale: f32, lane: i32, shared: u64) {
    fv_quant_q8(q, scale, 0, 64, lane, shared);
    fv_quant_q8(q + 512, scale, 32, 72, lane, shared);
}

export fn fv_pack_query(q: u64, dst: u64, scale: f32, lane: i32, shared: u64) {
    fv_quant_q8_head(q, scale, lane, shared);
    syncwarp();
    let word = lane as u64;
    st_i32(dst, 4 * word, sm_i32(shared, word));
    st_i32(dst, 4 * (word + 32), sm_i32(shared, word + 32));
    if lane < 16 { st_i32(dst, 4 * (word + 64), sm_i32(shared, word + 64)); }
}
// flash_attn_ext_vec<256,Q4_0,Q4_0,false>: block dim (32,4,1), lane=tx, warp=ty.
// ncols is the runtime cols_per_block (1 or 2); D/4=64 i32 per row; D/QK8_1=8 ds pairs.

fn fv_bounded_v4(base: u64, column: i64, valid: bool) -> VecFlash4 {
    return if valid { fv_dequant_v4(base, column) } else { VecFlash4 { 0.0, 0.0, 0.0, 0.0 } };
}

export fn fv_fattn_vec_1(
    q_p: u64,
    k_p: u64,
    v_p: u64,
    mask_p: u64,
    sinks_p: u64,
    kv_max_p: u64,
    dst_p: u64,
    dst_meta_p: u64,
    scale: f32,
    max_bias: f32,
    m0: f32,
    m1: f32,
    n_head_log2: i32,
    logit_softcap: f32,
    ne00: i32,
    ne01_z: i32,
    ne02: i32,
    ne03: i32,
    nb01: i64,
    nb02: i64,
    nb03: i64,
    ne10: i32,
    ne11: i32,
    ne12: i32,
    ne13: i32,
    nb11: i64,
    nb12: i64,
    nb13: i64,
    nb21: i64,
    nb22: i64,
    nb23: i64,
    ne31: i32,
    ne32: i32,
    ne33: i32,
    nb31: i64,
    nb32: i64,
    nb33: i64,
    query_mode: i32,
    bx: i32,
    by: i32,
    bz: i32,
    gx: i32,
    gy: i32,
    tx: i32,
    ty: i32,
    smem: u64) {
    let ncols: i32 = 1;
    lc();
    let ic0 = bx * ncols;
    let sequence = bz / ne02;
    let head = bz - sequence * ne02;
    let gqa_ratio = ne02 / ne12;
    let qb = q_p + ( (nb03 * (sequence as i64)) + (nb02 * (head as i64)) + (nb01 * (ic0 as i64))) as u64;
    let kb0 = k_p + ( (nb13 * (sequence as i64)) + (nb12 * ( (head / gqa_ratio) as i64))) as u64;
    let vb0 = v_p + ( (nb23 * (sequence as i64)) + (nb22 * ( (head / gqa_ratio) as i64))) as u64;
    let maskb0 = if mask_p != 0 {
        mask_p + ( (nb33 * ( (sequence % ne33) as i64)) + (nb31 * (ic0 as i64))) as u64
    } else {
        0 as u64
    }
    ;
    let slope = fv_alibi_slope(max_bias, head, n_head_log2, m0, m1);
    let tid = 32 * ty + tx;
    var kmax: f32[2] = default < f32[2] > ();
    var ksum: f32[2] = default < f32[2] > ();
    var vkqx: f32[8] = default < f32[8] > ();
    var vkqy: f32[8] = default < f32[8] > ();
    var vkqscale: f32[2] = default < f32[2] > ();
    var j: i32 = 0;

    if true {
        let j: i32 = 0;
        kmax[j] = fv_negmax();
        ksum[j] = 0.0;
    }
    j = (1 as i32);
    // The standalone single-query path can share a packed Q across partitions.
    pdl_sync();
    let prepacked = query_mode == 0;
    if !prepacked {
        if ty == 0 { fv_quant_q8_head(qb, scale, tx, smem); }
        barrier();
    }
    var qi32: i32[4] = default < i32[4] > ();
    var qdx: f32[4] = default < f32[4] > ();
    var qdy: f32[4] = default < f32[4] > ();
    let packed = q_p + ((320 * (bz as i64)) as u64);
    for half in 2 {
        let word = (32 * half + (tx as i64)) as u64;
        let ds = 64 + 2 * (word / 8);
        if prepacked {
            qi32[half] = ld_i32(packed, 4 * word);
            qdx[half] = ld_f32(packed, 4 * ds);
            qdy[half] = ld_f32(packed, 4 * (ds + 1));
        } else {
            qi32[half] = sm_i32(smem, word);
            qdx[half] = sm_f32(smem, ds);
            qdy[half] = sm_f32(smem, ds + 1);
        }
    }
    if !prepacked { barrier(); }
    var kvlim: i32 = ne11;

    if kv_max_p != 0 {
        kvlim = ld_i32(kv_max_p, ( ( (sequence * gx + bx) * 4)) as u64);
    }
    var kbase = kb0 + ( (nb11 * ( (by * 128) as i64))) as u64;
    var vbase = vb0 + ( (nb21 * ( (by * 128) as i64))) as u64;
    var maskb = maskb0 + ( ( (by * 128) * 2) as u64);
    var k0v: i64 = (by * 128) as i64;

    while k0v < (kvlim as i64) {
        var kreg: f32[2] = default < f32[2] > ();
        var kmaxn: f32[2] = default < f32[2] > ();
        j = 0;

        if true {
            let j: i32 = 0;
            kmaxn[j] = kmax[j];
        }
        j = (1 as i32);
        // All 32 lanes score the same key before its owner keeps the value.
        for key in 32 {
            let iq = key as i32;
            let row = ty * 32 + iq;
            let valid = k0v + (row as i64) < (kvlim as i64);
            var score = fv_negmax();
            if valid {
                score = fv_wr_sum(fv_vec_dot_kq4(kbase + ((nb11 * (row as i64)) as u64), tx,
                    qi32[0], qi32[1], qdx[0], qdy[0], qdx[1], qdy[1]));
                if mask_p != 0 {
                    score += slope * h2f(ld_u16(maskb, ((row * 2) as u64)));
                }
            }
            kmaxn[0] = fmaxf(kmaxn[0], score + (3.0 as f32) * (0.6931 as f32));
            if tx == iq { kreg[0] = score; }
        }
        j = 0;

        if true {
            let j: i32 = 0;
            // nthreads_KQ==WARP_SIZE: source KQ_max_new shfl loop is a no-op here.
            let ksc = expf(kmax[j] - kmaxn[j]);
            kmax[j] = kmaxn[j];
            kreg[j] = if k0v + (tid as i64) < (kvlim as i64) {
                expf(kreg[j] - kmax[j])
            } else { 0.0 };
            ksum[j] = ksum[j] * ksc + kreg[j];
            sm_st_f32(smem, ( (j * 128 + tid)) as u64, kreg[j]);
            vkqscale[j] = ksc;
        }
        j = (1 as i32);
        syncwarp();
        for key in 32 {
            let row = (ty as i64) * 32 + key;
            let probability = sm_f32(smem, row as u64);
            let valid = k0v + row < (kvlim as i64);
            let address = vbase + ((nb21 * row) as u64);
            let low = fv_bounded_v4(address, (tx as i64) * 4, valid);
            let high = fv_bounded_v4(address, 128 + (tx as i64) * 4, valid);
            if key == 0 {
                let prior = vkqscale[0];
                vkqx[0] = fma_rn(prior, vkqx[0], mul_rn(low.v0, probability));
                vkqy[0] = fma_rn(prior, vkqy[0], mul_rn(low.v1, probability));
                vkqx[1] = fma_rn(prior, vkqx[1], mul_rn(low.v2, probability));
                vkqy[1] = fma_rn(prior, vkqy[1], mul_rn(low.v3, probability));
                vkqx[2] = fma_rn(prior, vkqx[2], mul_rn(high.v0, probability));
                vkqy[2] = fma_rn(prior, vkqy[2], mul_rn(high.v1, probability));
                vkqx[3] = fma_rn(prior, vkqx[3], mul_rn(high.v2, probability));
                vkqy[3] = fma_rn(prior, vkqy[3], mul_rn(high.v3, probability));
            } else {
                vkqx[0] = fma_rn(low.v0, probability, vkqx[0]);
                vkqy[0] = fma_rn(low.v1, probability, vkqy[0]);
                vkqx[1] = fma_rn(low.v2, probability, vkqx[1]);
                vkqy[1] = fma_rn(low.v3, probability, vkqy[1]);
                vkqx[2] = fma_rn(high.v0, probability, vkqx[2]);
                vkqy[2] = fma_rn(high.v1, probability, vkqy[2]);
                vkqx[3] = fma_rn(high.v2, probability, vkqx[3]);
                vkqy[3] = fma_rn(high.v3, probability, vkqy[3]);
            }
        }
        syncwarp();
        k0v += ( (gy * 128) as i64);
        kbase += ( ( (gy * 128) as i64) * nb11) as u64;
        vbase += ( ( (gy * 128) as i64) * nb21) as u64;
        maskb += ( ( (gy * 128) * 2) as u64);
    }

    if sinks_p != 0 && by == 0 {
        let sink = ld_f32(sinks_p, ( (head * 4)) as u64);

        if ty == (0 as i32) {
            let js: i32 = 0;
            let kmaxn_j = fmaxf(sink, kmax[js]);
            let ksc = expf(kmax[js] - kmaxn_j);
            kmax[js] = kmaxn_j;
            ksum[js] = ksum[js] * ksc + (if tx == 0 {
                expf(sink - kmax[js])
            } else {
                0.0
            });
            var kk: i32 = 0;

            if true {
                let kk: i32 = 0;
                vkqx[js * 4 + kk] = mul_rn(vkqx[js * 4 + kk], ksc);
                vkqy[js * 4 + kk] = mul_rn(vkqy[js * 4 + kk], ksc);
            }

            if true {
                let kk: i32 = 1;
                vkqx[js * 4 + kk] = mul_rn(vkqx[js * 4 + kk], ksc);
                vkqy[js * 4 + kk] = mul_rn(vkqy[js * 4 + kk], ksc);
            }

            if true {
                let kk: i32 = 2;
                vkqx[js * 4 + kk] = mul_rn(vkqx[js * 4 + kk], ksc);
                vkqy[js * 4 + kk] = mul_rn(vkqy[js * 4 + kk], ksc);
            }

            if true {
                let kk: i32 = 3;
                vkqx[js * 4 + kk] = mul_rn(vkqx[js * 4 + kk], ksc);
                vkqy[js * 4 + kk] = mul_rn(vkqy[js * 4 + kk], ksc);
            }
            kk = (4 as i32);
        }
    }
    j = 0;

    if true {
        let j: i32 = 0;

        if ty == 0 {
            sm_st_f32(smem, ( (1024 + j * 32 + tx)) as u64, fv_negmax());
            sm_st_f32(smem, ( (1088 + j * 32 + tx)) as u64, 0.0);
        }
    }
    j = (1 as i32);
    barrier();
    j = 0;

    if true {
        let j: i32 = 0;

        if tx == 0 {
            sm_st_f32(smem, ( (1024 + j * 32 + ty)) as u64, kmax[j]);
        }
    }
    j = (1 as i32);
    barrier();
    // Effective columns after the source break (ncols>1 && ic0+j>=ne01.z); ic0<ne01_z holds.
    var jc = ncols;

    if ncols > 1 && ic0 + ncols > ne01_z {
        jc = ne01_z - ic0;
    }
    j = 0;

    if (0 as i32) < jc {
        let j: i32 = 0;
        var kmaxn2 = sm_f32(smem, ( (1024 + j * 32 + tx)) as u64);
        kmaxn2 = fv_wr_max(kmaxn2);
        let ksc2 = expf(kmax[j] - kmaxn2);
        kmax[j] = kmaxn2;
        var kk: i32 = 0;

        if true {
            let kk: i32 = 0;
            vkqx[j * 4 + kk] = mul_rn(vkqx[j * 4 + kk], ksc2);
            vkqy[j * 4 + kk] = mul_rn(vkqy[j * 4 + kk], ksc2);
        }

        if true {
            let kk: i32 = 1;
            vkqx[j * 4 + kk] = mul_rn(vkqx[j * 4 + kk], ksc2);
            vkqy[j * 4 + kk] = mul_rn(vkqy[j * 4 + kk], ksc2);
        }

        if true {
            let kk: i32 = 2;
            vkqx[j * 4 + kk] = mul_rn(vkqx[j * 4 + kk], ksc2);
            vkqy[j * 4 + kk] = mul_rn(vkqy[j * 4 + kk], ksc2);
        }

        if true {
            let kk: i32 = 3;
            vkqx[j * 4 + kk] = mul_rn(vkqx[j * 4 + kk], ksc2);
            vkqy[j * 4 + kk] = mul_rn(vkqy[j * 4 + kk], ksc2);
        }
        kk = (4 as i32);
        var iv: i32 = 0;

        if true {
            let iv: i32 = 0;
            let ivk = iv + tx * 2;
            sm_st_f32(smem, ( (ty * 256 + 2 * ivk)) as u64, vkqx[j * 4 + iv / 32]);
            sm_st_f32(smem, ( (ty * 256 + 2 * ivk + 1)) as u64, vkqy[j * 4 + iv / 32]);
            sm_st_f32(smem, ( (ty * 256 + 2 * ivk + 2)) as u64, vkqx[j * 4 + iv / 32 + 1]);
            sm_st_f32(smem, ( (ty * 256 + 2 * ivk + 3)) as u64, vkqy[j * 4 + iv / 32 + 1]);
        }

        if true {
            let iv: i32 = 64;
            let ivk = iv + tx * 2;
            sm_st_f32(smem, ( (ty * 256 + 2 * ivk)) as u64, vkqx[j * 4 + iv / 32]);
            sm_st_f32(smem, ( (ty * 256 + 2 * ivk + 1)) as u64, vkqy[j * 4 + iv / 32]);
            sm_st_f32(smem, ( (ty * 256 + 2 * ivk + 2)) as u64, vkqx[j * 4 + iv / 32 + 1]);
            sm_st_f32(smem, ( (ty * 256 + 2 * ivk + 3)) as u64, vkqy[j * 4 + iv / 32 + 1]);
        }
        iv = (128 as i32);
        ksum[j] *= ksc2;
        ksum[j] = fv_wr_sum(ksum[j]);

        if tx == 0 {
            sm_st_f32(smem, ( (1088 + j * 32 + ty)) as u64, ksum[j]);
        }
        barrier();
        ksum[j] = sm_f32(smem, ( (1088 + j * 32 + tx)) as u64);
        ksum[j] = fv_wr_sum(ksum[j]);
        var i0: i32 = 0;

        if true {
            let i0: i32 = 0;
            var dv: f32 = 0.0;
            var w: i32 = 0;

            if true {
                let w: i32 = 0;
                dv += sm_f32(smem, ( (w * 256 + i0 + tid)) as u64);
            }

            if true {
                let w: i32 = 1;
                dv += sm_f32(smem, ( (w * 256 + i0 + tid)) as u64);
            }

            if true {
                let w: i32 = 2;
                dv += sm_f32(smem, ( (w * 256 + i0 + tid)) as u64);
            }

            if true {
                let w: i32 = 3;
                dv += sm_f32(smem, ( (w * 256 + i0 + tid)) as u64);
            }
            w = (4 as i32);

            if gy == 1 {
                dv /= ksum[j];
            }
            let didx = ( ( ( ( ( (sequence as i64) * (ne01_z as i64)) + ( (ic0 + j) as i64)) * (ne02 as i64)) + ( (head as i64))) * (gy as i64)) + ( (by as i64));
            st_f32(dst_p, ( ( (didx * 256) + ( (i0 + tid) as i64)) * 4) as u64, dv);
        }

        if true {
            let i0: i32 = 128;
            var dv: f32 = 0.0;
            var w: i32 = 0;

            if true {
                let w: i32 = 0;
                dv += sm_f32(smem, ( (w * 256 + i0 + tid)) as u64);
            }

            if true {
                let w: i32 = 1;
                dv += sm_f32(smem, ( (w * 256 + i0 + tid)) as u64);
            }

            if true {
                let w: i32 = 2;
                dv += sm_f32(smem, ( (w * 256 + i0 + tid)) as u64);
            }

            if true {
                let w: i32 = 3;
                dv += sm_f32(smem, ( (w * 256 + i0 + tid)) as u64);
            }
            w = (4 as i32);

            if gy == 1 {
                dv /= ksum[j];
            }
            let didx = ( ( ( ( ( (sequence as i64) * (ne01_z as i64)) + ( (ic0 + j) as i64)) * (ne02 as i64)) + ( (head as i64))) * (gy as i64)) + ( (by as i64));
            st_f32(dst_p, ( ( (didx * 256) + ( (i0 + tid) as i64)) * 4) as u64, dv);
        }
        i0 = (256 as i32);

        if j < jc - 1 {
            barrier();
        }
    }
    j = (1 as i32);

    if gy != 1 && tid == (0 as i32) && (1 == 1 || ic0 + (0 as i32) < ne01_z) {
        let tid: i32 = 0;
        let midx = ( ( ( ( (sequence as i64) * (ne01_z as i64)) + ( (ic0 + tid) as i64)) * (ne02 as i64)) + ( (head as i64))) * (gy as i64) + ( (by as i64));
        st_f32(dst_meta_p, (midx * 8) as u64, kmax[tid]);
        st_f32(dst_meta_p, ( (midx * 8) + 4) as u64, ksum[tid]);
    }
}

export fn fv_combine(
    parts_p: u64,
    meta_p: u64,
    dst_p: u64,
    pb: i32,
    bx: i32,
    by: i32,
    bz: i32,
    gx: i32,
    gy: i32,
    tx: i32,
    smem: u64) {
    let j_dst = (bz * gx + bx) * gy + by;
    let pb64 = pb as i64;
    let parts = parts_p + ( ( ( (j_dst as i64) * pb64) * 1024)) as u64;
    let meta = meta_p + ( ( ( (j_dst as i64) * pb64) * 8)) as u64;
    let dstb = dst_p + ( ( ( (j_dst as i64)) * 1024)) as u64;
    var i: i32 = tx;

    while i < 2 * pb {
        sm_st_f32(smem, (i as u64), ld_f32(meta, ( (i * 4)) as u64));
        i += 256;
    }
    barrier();
    var kqmax = sm_f32(smem, 0 as u64);
    var l: i32 = 1;

    while l < pb {
        kqmax = fmaxf(kqmax, sm_f32(smem, ( (2 * l)) as u64));
        l += 1;
    }
    var num: f32 = 0.0;
    var den: f32 = 0.0;
    l = 0;

    while l < pb {
        let sc = expf(sm_f32(smem, ( (2 * l)) as u64) - kqmax);
        num += sc * ld_f32(parts, ( ( (l * 256 + tx) * 4)) as u64);
        den += sc * sm_f32(smem, ( (2 * l + 1)) as u64);
        l += 1;
    }
    st_f32(dstb, ( (tx * 4)) as u64, num / den);
}
// Retained entries; parent never executes main.


struct FvHeadPair { a: f32, b: f32 }
struct FvPackedQuery { w0: i32, w1: i32, d0: f32, s0: f32, d1: f32, s1: f32 }

fn fv_load_packed_head(q: u64, lane: i64) -> FvPackedQuery {
    let word = lane as u64;
    let ds = 64 + 2 * (word / 8);
    return FvPackedQuery { ld_i32(q, 4 * word), ld_i32(q, 4 * (word + 32)),
        ld_f32(q, 4 * ds), ld_f32(q, 4 * (ds+1)), ld_f32(q, 4 * (ds+8)), ld_f32(q, 4 * (ds+9)) };
}

fn fv_dot_head_pair(k: u64, lane: i32, a: FvPackedQuery, b: FvPackedQuery) -> FvHeadPair {
    let block_address = k + ((18 * (lane / 8)) as u64);
    let nibble = lane % 8 / 4;
    let column = 2 + 4 * (lane % 4);
    let k0 = fv_q4_word(block_address, column, nibble);
    let k1 = fv_q4_word(block_address + 72, column, nibble);
    let scale0 = h2f(ld_u16(block_address, 0));
    let scale1 = h2f(ld_u16(block_address + 72, 0));
    var x: f32 = 0.0;
    var y: f32 = 0.0;
    x += scale0 * ((dp4a(k0, a.w0, 0) as f32) * a.d0 - a.s0);
    y += scale0 * ((dp4a(k0, b.w0, 0) as f32) * b.d0 - b.s0);
    x += scale1 * ((dp4a(k1, a.w1, 0) as f32) * a.d1 - a.s1);
    y += scale1 * ((dp4a(k1, b.w1, 0) as f32) * b.d1 - b.s1);
    return FvHeadPair { fv_wr_sum(x), fv_wr_sum(y) };
}

fn fv_pair_value(acc: VecFlash4, value: VecFlash4, p: f32, prior: f32, first: bool) -> VecFlash4 {
    if first {
        return VecFlash4 { fma_rn(prior, acc.v0, mul_rn(value.v0, p)),
            fma_rn(prior, acc.v1, mul_rn(value.v1, p)), fma_rn(prior, acc.v2, mul_rn(value.v2, p)),
            fma_rn(prior, acc.v3, mul_rn(value.v3, p)) };
    }
    return VecFlash4 { fma_rn(value.v0, p, acc.v0), fma_rn(value.v1, p, acc.v1),
        fma_rn(value.v2, p, acc.v2), fma_rn(value.v3, p, acc.v3) };
}

fn fv_pair_spill_value(smem: u64, offset: u64, values: VecFlash4, scale: f32) {
    sm_st_f32(smem, offset, mul_rn(values.v0, scale));
    sm_st_f32(smem, offset + 1, mul_rn(values.v1, scale));
    sm_st_f32(smem, offset + 2, mul_rn(values.v2, scale));
    sm_st_f32(smem, offset + 3, mul_rn(values.v3, scale));
}

fn fv_pair_finish(out: u64, meta: u64, low: VecFlash4, high: VecFlash4,
                   maximum: f32, total: f32, lane: i32, warp: i32, smem: u64) {
    if lane == 0 { sm_st_f32(smem, (1024 + warp) as u64, maximum); }
    if warp == 0 && lane >= 4 {
        sm_st_f32(smem, (1024 + lane) as u64, fv_negmax());
        sm_st_f32(smem, (1088 + lane) as u64, 0.0);
    }
    barrier();
    let block_max = fv_wr_max(sm_f32(smem, (1024 + lane) as u64));
    let scale = expf(maximum - block_max);
    let index = (warp * 256 + 4 * lane) as u64;
    fv_pair_spill_value(smem, index, low, scale);
    fv_pair_spill_value(smem, index + 128, high, scale);
    let warp_total = fv_wr_sum(total * scale);
    if lane == 0 { sm_st_f32(smem, (1088 + warp) as u64, warp_total); }
    barrier();
    let block_total = fv_wr_sum(sm_f32(smem, (1088 + lane) as u64));
    let tid = (warp * 32 + lane) as u64;
    for half in 2 {
        let column = tid + 128 * (half as u64);
        var sum: f32 = 0.0;
        for w in 4 { sum += sm_f32(smem, column + 256 * (w as u64)); }
        st_f32(out, 4 * column, sum);
    }
    if tid == 0 {
        st_f32(meta, 0, block_max);
        st_f32(meta, 4, block_total);
    }
    barrier();
}

export fn fv_packed_heads_pair(q: u64, k: u64, v: u64, out0: u64, out1: u64,
                               meta0: u64, meta1: u64, keys: i64, kr: i64, vr: i64,
                               part: i32, parts: i32, lane: i32, warp: i32, smem: u64) {
    let q0 = fv_load_packed_head(q, lane as i64);
    let q1 = fv_load_packed_head(q + 320, lane as i64);
    var max0 = fv_negmax();
    var max1 = fv_negmax();
    var sum0: f32 = 0.0;
    var sum1: f32 = 0.0;
    var low0 = VecFlash4 {0.0, 0.0, 0.0, 0.0};
    var high0 = VecFlash4 {0.0, 0.0, 0.0, 0.0};
    var low1 = VecFlash4 {0.0, 0.0, 0.0, 0.0};
    var high1 = VecFlash4 {0.0, 0.0, 0.0, 0.0};
    let tid = (warp * 32 + lane) as i64;
    let column = (lane as i64) * 4;
    var first = (part as i64) * 128;
    while first < keys {
        var own0 = fv_negmax();
        var own1 = fv_negmax();
        var next0 = max0;
        var next1 = max1;
        for r in 32 {
            let index = first + (warp as i64) * 32 + r;
            let scores = if index < keys { fv_dot_head_pair(k + ((index * kr) as u64), lane, q0, q1) }
                else { FvHeadPair { fv_negmax(), fv_negmax() } };
            next0 = fmaxf(next0, scores.a + (3.0 as f32) * (0.6931 as f32));
            next1 = fmaxf(next1, scores.b + (3.0 as f32) * (0.6931 as f32));
            if lane == r as i32 { own0 = scores.a; own1 = scores.b; }
        }
        let prior0 = expf(max0 - next0);
        let prior1 = expf(max1 - next1);
        max0 = next0;
        max1 = next1;
        let prob0 = if first + tid < keys { expf(own0 - next0) } else { 0.0 };
        let prob1 = if first + tid < keys { expf(own1 - next1) } else { 0.0 };
        sum0 = sum0 * prior0 + prob0;
        sum1 = sum1 * prior1 + prob1;
        sm_st_f32(smem, tid as u64, prob0);
        sm_st_f32(smem, (tid + 128) as u64, prob1);
        syncwarp();
        for r in 32 {
            let row = (warp as i64) * 32 + r;
            let index = first + row;
            let address = v + ((vr * index) as u64);
            let low = fv_bounded_v4(address, column, index < keys);
            let high = fv_bounded_v4(address, column + 128, index < keys);
            let p0 = sm_f32(smem, row as u64);
            let p1 = sm_f32(smem, (row + 128) as u64);
            low0 = fv_pair_value(low0, low, p0, prior0, r == 0);
            high0 = fv_pair_value(high0, high, p0, prior0, r == 0);
            low1 = fv_pair_value(low1, low, p1, prior1, r == 0);
            high1 = fv_pair_value(high1, high, p1, prior1, r == 0);
        }
        syncwarp();
        first += (parts as i64) * 128;
    }
    fv_pair_finish(out0, meta0, low0, high0, max0, sum0, lane, warp, smem);
    fv_pair_finish(out1, meta1, low1, high1, max1, sum1, lane, warp, smem);
}

struct FvHeadQuad { a: f32, b: f32, c: f32, d: f32 }

fn fv_dot_head_quad(k: u64, lane: i32, q0: FvPackedQuery, q1: FvPackedQuery, q2: FvPackedQuery, q3: FvPackedQuery) -> FvHeadQuad {
    let block_address = k + ((18 * (lane / 8)) as u64);
    let nibble = lane % 8 / 4;
    let column = 2 + 4 * (lane % 4);
    let k0 = fv_q4_word(block_address, column, nibble);
    let k1 = fv_q4_word(block_address + 72, column, nibble);
    let scale0 = h2f(ld_u16(block_address, 0));
    let scale1 = h2f(ld_u16(block_address + 72, 0));
    var x0: f32 = 0.0;
    x0 += scale0 * ((dp4a(k0, q0.w0, 0) as f32) * q0.d0 - q0.s0);
    x0 += scale1 * ((dp4a(k1, q0.w1, 0) as f32) * q0.d1 - q0.s1);
    var x1: f32 = 0.0;
    x1 += scale0 * ((dp4a(k0, q1.w0, 0) as f32) * q1.d0 - q1.s0);
    x1 += scale1 * ((dp4a(k1, q1.w1, 0) as f32) * q1.d1 - q1.s1);
    var x2: f32 = 0.0;
    x2 += scale0 * ((dp4a(k0, q2.w0, 0) as f32) * q2.d0 - q2.s0);
    x2 += scale1 * ((dp4a(k1, q2.w1, 0) as f32) * q2.d1 - q2.s1);
    var x3: f32 = 0.0;
    x3 += scale0 * ((dp4a(k0, q3.w0, 0) as f32) * q3.d0 - q3.s0);
    x3 += scale1 * ((dp4a(k1, q3.w1, 0) as f32) * q3.d1 - q3.s1);
    return FvHeadQuad { fv_wr_sum(x0), fv_wr_sum(x1), fv_wr_sum(x2), fv_wr_sum(x3) };
}

export fn fv_packed_heads_quad(q: u64, k: u64, v: u64, out: u64, meta: u64,
                               keys: i64, kr: i64, vr: i64,
                               part: i32, parts: i32, lane: i32, warp: i32, smem: u64) {
    let q0 = fv_load_packed_head(q + 0, lane as i64);
    var max0 = fv_negmax();
    var sum0: f32 = 0.0;
    var low0 = VecFlash4 {0.0, 0.0, 0.0, 0.0};
    var high0 = VecFlash4 {0.0, 0.0, 0.0, 0.0};
    let q1 = fv_load_packed_head(q + 320, lane as i64);
    var max1 = fv_negmax();
    var sum1: f32 = 0.0;
    var low1 = VecFlash4 {0.0, 0.0, 0.0, 0.0};
    var high1 = VecFlash4 {0.0, 0.0, 0.0, 0.0};
    let q2 = fv_load_packed_head(q + 640, lane as i64);
    var max2 = fv_negmax();
    var sum2: f32 = 0.0;
    var low2 = VecFlash4 {0.0, 0.0, 0.0, 0.0};
    var high2 = VecFlash4 {0.0, 0.0, 0.0, 0.0};
    let q3 = fv_load_packed_head(q + 960, lane as i64);
    var max3 = fv_negmax();
    var sum3: f32 = 0.0;
    var low3 = VecFlash4 {0.0, 0.0, 0.0, 0.0};
    var high3 = VecFlash4 {0.0, 0.0, 0.0, 0.0};
    let tid = (warp * 32 + lane) as i64;
    let column = (lane as i64) * 4;
    var first = (part as i64) * 128;
    while first < keys {
        var own0 = fv_negmax();
        var next0 = max0;
        var own1 = fv_negmax();
        var next1 = max1;
        var own2 = fv_negmax();
        var next2 = max2;
        var own3 = fv_negmax();
        var next3 = max3;
        for r in 32 {
            let index = first + (warp as i64) * 32 + r;
            let scores = if index < keys { fv_dot_head_quad(k + ((index * kr) as u64), lane, q0, q1, q2, q3) }
                else { FvHeadQuad { fv_negmax(), fv_negmax(), fv_negmax(), fv_negmax() } };
            next0 = fmaxf(next0, scores.a + (3.0 as f32) * (0.6931 as f32));
            next1 = fmaxf(next1, scores.b + (3.0 as f32) * (0.6931 as f32));
            next2 = fmaxf(next2, scores.c + (3.0 as f32) * (0.6931 as f32));
            next3 = fmaxf(next3, scores.d + (3.0 as f32) * (0.6931 as f32));
            if lane == r as i32 {
                own0 = scores.a;
                own1 = scores.b;
                own2 = scores.c;
                own3 = scores.d;
            }
        }
        let prior0 = expf(max0 - next0);
        max0 = next0;
        let prob0 = if first + tid < keys { expf(own0 - next0) } else { 0.0 };
        sum0 = sum0 * prior0 + prob0;
        sm_st_f32(smem, (tid + 0) as u64, prob0);
        let prior1 = expf(max1 - next1);
        max1 = next1;
        let prob1 = if first + tid < keys { expf(own1 - next1) } else { 0.0 };
        sum1 = sum1 * prior1 + prob1;
        sm_st_f32(smem, (tid + 128) as u64, prob1);
        let prior2 = expf(max2 - next2);
        max2 = next2;
        let prob2 = if first + tid < keys { expf(own2 - next2) } else { 0.0 };
        sum2 = sum2 * prior2 + prob2;
        sm_st_f32(smem, (tid + 256) as u64, prob2);
        let prior3 = expf(max3 - next3);
        max3 = next3;
        let prob3 = if first + tid < keys { expf(own3 - next3) } else { 0.0 };
        sum3 = sum3 * prior3 + prob3;
        sm_st_f32(smem, (tid + 384) as u64, prob3);
        syncwarp();
        for r in 32 {
            let row = (warp as i64) * 32 + r;
            let index = first + row;
            let address = v + ((vr * index) as u64);
            let low = fv_bounded_v4(address, column, index < keys);
            let high = fv_bounded_v4(address, column + 128, index < keys);
            let p0 = sm_f32(smem, (row + 0) as u64);
            low0 = fv_pair_value(low0, low, p0, prior0, r == 0);
            high0 = fv_pair_value(high0, high, p0, prior0, r == 0);
            let p1 = sm_f32(smem, (row + 128) as u64);
            low1 = fv_pair_value(low1, low, p1, prior1, r == 0);
            high1 = fv_pair_value(high1, high, p1, prior1, r == 0);
            let p2 = sm_f32(smem, (row + 256) as u64);
            low2 = fv_pair_value(low2, low, p2, prior2, r == 0);
            high2 = fv_pair_value(high2, high, p2, prior2, r == 0);
            let p3 = sm_f32(smem, (row + 384) as u64);
            low3 = fv_pair_value(low3, low, p3, prior3, r == 0);
            high3 = fv_pair_value(high3, high, p3, prior3, r == 0);
        }
        syncwarp();
        first += (parts as i64) * 128;
    }
    fv_pair_finish(out + (0 * (parts as u64)), meta + (0 * (parts as u64)),
        low0, high0, max0, sum0, lane, warp, smem);
    fv_pair_finish(out + (1024 * (parts as u64)), meta + (8 * (parts as u64)),
        low1, high1, max1, sum1, lane, warp, smem);
    fv_pair_finish(out + (2048 * (parts as u64)), meta + (16 * (parts as u64)),
        low2, high2, max2, sum2, lane, warp, smem);
    fv_pair_finish(out + (3072 * (parts as u64)), meta + (24 * (parts as u64)),
        low3, high3, max3, sum3, lane, warp, smem);
}

extern "flash_vector_plan_word" fn bonsai_flash_plan(address: u64, index: i32) -> i64;

export fn bonsai_flash_packed(query: u64, key: u64, value: u64, parts: u64, metadata: u64,
    plan: u64, keys: i64, head_group: u32, part: i32, tiles: i32, thread: i32, shared: u64, heads_per_group: u32) {
    let head = head_group * heads_per_group;
    let group = head / ((bonsai_flash_plan(plan, 3) as! u32) / (bonsai_flash_plan(plan, 4) as! u32));
    let slot = (head as u64) * (tiles as u64) + (part as u64);
    let query_address = query + (head as u64) * 320;
    let key_address = key + (group as u64) * (bonsai_flash_plan(plan, 10) as! u64);
    let value_address = value + (group as u64) * (bonsai_flash_plan(plan, 12) as! u64);
    let key_stride = bonsai_flash_plan(plan, 9);
    let value_stride = bonsai_flash_plan(plan, 11);
    let lane = thread % 32;
    let warp = thread / 32;
    if heads_per_group == 2 {
        let next = slot + (tiles as u64);
        fv_packed_heads_pair(query_address, key_address, value_address, parts + slot * 1024,
            parts + next * 1024, metadata + slot * 8, metadata + next * 8, keys, key_stride,
            value_stride, part, tiles, lane, warp, shared);
    } else {
        fv_packed_heads_quad(query_address, key_address, value_address, parts + slot * 1024,
            metadata + slot * 8, keys, key_stride, value_stride, part, tiles, lane, warp, shared);
    }
}
