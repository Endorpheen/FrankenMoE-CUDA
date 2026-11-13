// fg175m_process_tile: Goose port of flash_attn_ext_f16_process_tile (fattn-mma-f16.cuh L1190-1786),
// active CUDA branch (TURING/AMPERE MMA + CP_ASYNC_AVAILABLE), fixed Apex Ada configuration ONLY:
//   DKQ=DV=256 (D2=128 half2), ncols2=8, ncols1=4|8 (ncols=32|64), nwarps=4 (128 threads),
//   cols_per_warp=16, cols_per_thread=2, np=2 for ncols1=4 else 1, Q_in_reg=true, nstages=2,
//   nbatch_fa=32, nbatch_K2=nbatch_V2=128, nbatch_combine=128, swizzled=true, stride_tile_Q=132,
//   stride_tile_K=stride_tile_V=128, tile_stride=132, use_logit_softcap=false, V_is_K_view=false,
//   use_sparse=false, oob_check=false, warp_size=32. Every other configuration/architecture is
//   DEFERRED: no scalar fallback, no naive kernel, no reduction change.
// Address computation, layout selection, register permutation, guards, loop bounds and barrier
// placement are Goose code mirroring the native lines; the only effects are the raw ISA primitives
// below plus the extern Goose iterate (fg175m_iter = sibling port of flash_attn_ext_f16_iter,
// fattn-mma-f16.cuh L574-1072; NOT the original C++).
// Shared dynamic memory is the native Q_in_reg UNION: tile_Q occupies half2 words [0, ncols*132),
//   tile_K overlays it at byte 0 (32 rows * 128 half2 * 4 B = 16384 B), tile_V at byte 16384,
//   tile_mask (half) at byte 32768 (ncols1*(32+8) half, 80 B per row); allocation 33792 B covers
//   ncols1=8. The two barriers after Q staging and after the Q register load make the K/V overwrite
//   of the Q image safe, exactly as native L1319/L1330.
// Q_B[16] and VKQ_C[16] (tile<16,8,half2>, 4 u32 each) and KQ_max/KQ_rowsum[2] live in
//   parent-supplied per-thread POD stack buffers (qregs/vkqregs/kmax/rowsums byte pointers). They
//   are storage only: initialization, lifetime and every access execute here in Goose via generic
//   word/float primitives. No address-of/local-array intrinsic is used or assumed (none exists in
//   any supplied example source).
struct F2 {
    x: f32, y: f32
}
struct U4 {
    a: u32, b: u32, c: u32, d: u32
}
extern "fg175l_cp16" fn cp16(dst_shared_generic: u64, src_global: u64);
// cp.async.cg.shared.global 16B, L2::64B
extern "fg175p_gload" fn gload(p: u64, i: u64) -> f32;
// *(f32*)(p+4*i), generic/global element load
extern "fg175p_gstore" fn gstore(p: u64, i: u64, v: f32);
// *(f32*)(p+4*i)=v
extern "fg175p_ldu32" fn ldu32(p: u64, i: u64) -> u32;
// *(u32*)(p+4*i), generic (POD stack)
extern "fg175p_stu32" fn stu32(p: u64, i: u64, v: u32);
// *(u32*)(p+4*i)=v
extern "fg175p_smld" fn smld(p: u64, i: u64) -> u32;
// shared u32 word at p+4*i
extern "fg175p_smst" fn smst(p: u64, i: u64, v: u32);
// shared u32 word at p+4*i=v
extern "fg175p_smldf" fn smldf(p: u64, i: u64) -> f32;
// shared f32 at p+4*i
extern "fg175p_smstf" fn smstf(p: u64, i: u64, v: f32);
// shared f32 at p+4*i=v
extern "fg175p_h2mul" fn h2mul(a: u32, b: u32) -> u32;
// half2*half2, IEEE RN per half lane
extern "fg175p_h2pack" fn h2pack(x: f32, y: f32) -> u32;
// {cvt.rn.f16.f32(x), cvt.rn.f16.f32(y)}
extern "fg175p_hlo" fn hlo(w: u32) -> f32;
// low half -> f32
extern "fg175p_hhi" fn hhi(w: u32) -> f32;
// high half -> f32
extern "fg175p_ldm4" fn ldm4(a: u64) -> U4;
// ldmatrix.sync.aligned.m8n8.x4.b16, byte address
extern "fg175p_shflx" fn shflx(v: f32, o: i32) -> f32;
// __shfl_xor_sync(0xFFFFFFFF,v,o,32)
extern "fg175p_expf" fn expf(x: f32) -> f32;
extern "fg175p_barrier" fn barrier();
// __syncthreads
// The single extern device function: actual Goose port of flash_attn_ext_f16_iter for this fixed
// configuration (sibling worker module, symbol fg175m_iter; function named iterate there).
extern "fg175m_iter" fn iterate(k_h2: u64, v_h2: u64, mask_h: u64, stride_k: i64, stride_v: i64, stride_mask: i64, slope: f32, jt: i32, kb0: i32, n_queries: i64, ncols1: i32, qregs: u64, vkqregs: u64, kmax: u64, rowsums: u64, smem: u64, tx: i32, ty: i32, np: i32, last: bool);
// mma.cuh CUDA I_MAJOR element maps, physical warp 32, lane=tx:
// tile<16,8,half2>: ne=4, get_i(l)=((l%2)*8)+lane/4, get_j(l)=((l/2)*4)+lane%4 (mma.cuh L405/421).

fn gi8(l: i32, tx: i32) -> i32 {
    return (l % 2) * 8 + tx / 4;
}

fn gj8(l: i32, tx: i32) -> i32 {
    return (l / 2) * 4 + tx % 4;
}
// tile<16,16,float>: ne=8, get_i(l)=(((l/2)%2)*8)+lane/4 (mma.cuh L247).

fn gi16(l: i32, tx: i32) -> i32 {
    return ( (l / 2) % 2) * 8 + tx / 4;
}

fn fmaxf2(a: f32, b: f32) -> f32 {
    return if a > b {
        a
    } else {
        b
    }
    ;
}
extern "fg175l_load_kv" fn load_kv(src: u64, dst_shared: u64, stride_kv_h2: i64, kv_row0: i64, tx: i32, ty: i32);
extern "fg175l_load_mask" fn load_mask(src: u64, dst_shared: u64, stride_mask_half: i64, kv_row0: i64, query0: i64, n_queries: i64, ncols1: i32, tx: i32, ty: i32);

fn process_tile_apex_nc4_np2(
    q_f2: u64,
    k_h2: u64,
    v_h2: u64,
    mask_h: u64,
    sinks_f: u64,
    dstk: u64,
    dstk_fixup: u64,
    scale: f32,
    slope: f32,
    ne01z: i64,
    ne02: i32,
    gqa_ratio: i32,
    stride_q1: i64,
    stride_q2: i64,
    stride_k: i64,
    stride_v: i64,
    stride_mask: i64,
    jt: i32,
    zt_gqa: i32,
    kb0_start: i32,
    kb0_stop: i32,
    ncols1_abi: i32,
    np_abi: i32,
    needs_fixup: bool,
    is_fixup: bool,
    bx: i32,
    gx: i32,
    tx: i32,
    ty: i32,
    qregs: u64,
    vkqregs: u64,
    kmax: u64,
    rowsums: u64,
    smem: u64) {
    let ncols1: i32 = (4 as i32);
    let np: i32 = (2 as i32);
    // ncols2=8 fixed; nbatch_fa=32; DKQ/2=nbatch_K2=nbatch_V2=nbatch_combine=128; tile_stride=132.
    let ncols = ncols1 * 8;
    // KQ_max init -FLT_MAX/2 (L1272) with exact f32 bits: 2^127-2^103 = (2-2^-23)*2^126, computed
    // from exact powers of two (no bitcast primitive exists or is assumed).
    let fltmax2: f32 = 170141173319264429905852091742258332672.0;
    gstore(kmax, 0, 0.0 - fltmax2);
    gstore(kmax, 1, 0.0 - fltmax2);
    gstore(rowsums, 0, 0.0);
    gstore(rowsums, 1, 0.0);
    var zi: i32 = 0;

    if true {
        let zi: i32 = (0 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (1 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (2 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (3 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (4 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (5 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (6 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (7 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (8 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (9 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (10 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (11 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (12 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (13 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (14 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (15 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (16 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (17 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (18 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (19 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (20 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (21 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (22 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (23 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (24 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (25 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (26 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (27 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (28 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (29 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (30 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (31 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (32 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (33 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (34 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (35 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (36 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (37 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (38 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (39 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (40 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (41 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (42 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (43 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (44 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (45 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (46 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (47 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (48 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (49 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (50 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (51 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (52 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (53 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (54 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (55 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (56 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (57 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (58 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (59 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (60 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (61 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (62 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (63 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }
    zi = (64 as i32);
    // VKQ_C zero init (mma.cuh tile ctor)
    // ---- Q staging, F32 -> scaled half2 into tile_Q (L1278-1317). The {32,16,8,4} granularity loop
    // emits only stride_k=32 for DKQ/2=128 (k0_start==k0_stop for the rest); stride_jc=1; the native
    // break (jc0+4>ncols && jc>=ncols) is statically false for ncols=32|64.
    let scale_h2 = h2pack(scale, scale);
    var jc0: i32 = 0;

    if true {
        let jc0: i32 = (0 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }

    if true {
        let jc0: i32 = (4 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }

    if true {
        let jc0: i32 = (8 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }

    if true {
        let jc0: i32 = (12 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }

    if true {
        let jc0: i32 = (16 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }

    if true {
        let jc0: i32 = (20 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }

    if true {
        let jc0: i32 = (24 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }

    if true {
        let jc0: i32 = (28 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }
    jc0 = (32 as i32);
    barrier();
    // L1319
    // ---- Q_B[16] register tiles via ldmatrix.x4 from tile_Q (L1321-1328); j0=(ty/np)*cols_per_warp.
    // load_ldmatrix<tile<16,8,half2>> int-word address (mma.cuh L857): + (tx%16)*132 + (tx/16)*4.
    let j0 = (ty / np) * 16;
    var q0: i32 = 0;

    if true {
        let q0: i32 = (0 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (8 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (16 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (24 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (32 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (40 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (48 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (56 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (64 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (72 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (80 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (88 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (96 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (104 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (112 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (120 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }
    q0 = (128 as i32);
    barrier();
    // L1330; the K/V cp.async preload may now overwrite the Q image (shared union)
    // ---- Initial async preload of mask and K for kb0=kb0_start (L1334-1347, nstages>1 branch).
    // ncols2=8>1 makes the mask load unconditional; K tile at smem+0, tile_mask at smem+32768.
    load_mask(
        mask_h,
        smem + 32768,
        stride_mask,
        (kb0_start * 32) as i64,
        (jt as i64) * (ncols1 as i64),
        ne01z,
        ncols1,
        tx,
        ty);
    load_kv(k_h2, smem, stride_k, (kb0_start * 32) as i64, tx, ty);
    // ---- kb0 loop (L1349-1390): ncols2>1 && !use_sparse selects the oob_check=false path with
    // constexpr k_VKQ_sup=nbatch_fa=32; the first iterate waits the preload cp.asyncs (iter L634-635).
    var kb0: i32 = kb0_start;

    while kb0 < kb0_stop - 1 {
        iterate(
            k_h2,
            v_h2,
            mask_h,
            stride_k,
            stride_v,
            stride_mask,
            slope,
            jt,
            kb0,
            ne01z,
            ncols1,
            qregs,
            vkqregs,
            kmax,
            rowsums,
            smem,
            tx,
            ty,
            np,
            false);
        kb0 += 1;
    }
    iterate(
        k_h2,
        v_h2,
        mask_h,
        stride_k,
        stride_v,
        stride_mask,
        slope,
        jt,
        kb0,
        ne01z,
        ncols1,
        qregs,
        vkqregs,
        kmax,
        rowsums,
        smem,
        tx,
        ty,
        np,
        true);
    barrier();
    // L1394-1396: nstages>1 && nwarps*cols_per_warp(64)>nbatch_fa(32)
    // ---- Sum partial KQ rowsums (L1399-1424): TURING+ path, cols_per_warp!=8 -> offsets {2,1}, width 32.
    var col: i32 = 0;

    if true {
        let col: i32 = (0 as i32);
        var v = gload(rowsums, col as u64);
        var o: i32 = 2;

        if true {
            let o: i32 = (2 as i32);
            v += shflx(v, o);
        }

        if true {
            let o: i32 = (1 as i32);
            v += shflx(v, o);
        }
        o = (0 as i32);
        gstore(rowsums, col as u64, v);
    }

    if true {
        let col: i32 = (1 as i32);
        var v = gload(rowsums, col as u64);
        var o: i32 = 2;

        if true {
            let o: i32 = (2 as i32);
            v += shflx(v, o);
        }

        if true {
            let o: i32 = (1 as i32);
            v += shflx(v, o);
        }
        o = (0 as i32);
        gstore(rowsums, col as u64, v);
    }
    col = (2 as i32);
    // ---- Attention sinks (L1429-1501): !is_fixup, leading warp(s) only (ty%np==0), optional pointer.
    var ks0: f32 = 0.0;
    var ks1: f32 = 0.0;

    if ( (!is_fixup) && ( (np == 1) || (ty % np == 0)) && (sinks_f != 0)) {
        // jc=(threadIdx.y/np)*16 + T_C_KQ::get_i(2*col) (tile<16,16,float>); sink index jc%ncols2=jc%8.
        let jc0s = (ty / np) * 16 + gi16(0, tx);
        let jc1s = (ty / np) * 16 + gi16(2, tx);
        let s0 = gload(sinks_f, (jc0s % 8) as u64);
        let s1 = gload(sinks_f, (jc1s % 8) as u64);
        let n0 = fmaxf2(gload(kmax, 0), s0);
        let d0 = gload(kmax, 0) - n0;
        ks0 = expf(d0);
        gstore(kmax, 0, n0);
        // *((u32*)&scale)*= (d>=SOFTMAX_FTZ_THRESHOLD) with SOFTMAX_FTZ_THRESHOLD=-20.0f, as select.

        if d0 < -20.0 {
            ks0 = 0.0;
        }
        gstore(rowsums, 0, ks0 * gload(rowsums, 0) + expf(s0 - n0));
        let n1 = fmaxf2(gload(kmax, 1), s1);
        let d1 = gload(kmax, 1) - n1;
        ks1 = expf(d1);
        gstore(kmax, 1, n1);

        if d1 < -20.0 {
            ks1 = 0.0;
        }
        gstore(rowsums, 1, ks1 * gload(rowsums, 1) + expf(s1 - n1));
        // VKQ_C rescale (L1458-1469, cols_per_warp!=8): per col scale x[l0+col], l0 in {0,2}.
        let h0 = h2pack(ks0, ks0);
        let h1 = h2pack(ks1, ks1);
        var i0: i32 = 0;

        if true {
            let i0: i32 = (0 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (1 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (2 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (3 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (4 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (5 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (6 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (7 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (8 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (9 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (10 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (11 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (12 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (13 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (14 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (15 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }
        i0 = (16 as i32);
    }
    // ---- Combine meta (L1503-1576): swz=true -> barrier first (L1555-1557); jc_cwm uses
    // tile<16,8,half2> get_i(tx%4)=((tx%2)*8)+tx/4; meta floats at tile_Q row padding word 128/129.
    barrier();
    let jc_cwm = ty * 16 + gi8(tx % 4, tx);
    let cmrx = gload(kmax, (tx % 2) as u64);
    let cmry = gload(rowsums, (tx % 2) as u64);
    let tsw = tx % 4 < 2;

    if ( ( ( (!needs_fixup) && (!is_fixup)) || (np > 1)) && tsw) {
        smstf(smem, (jc_cwm * 132 + 128) as u64, cmrx);
        // ((float2*)tile_Q)[jc_cwm*66+64]
        smstf(smem, (jc_cwm * 132 + 129) as u64, cmry);
    }
    barrier();
    // L1563

    if np == 1 {
        // No combination: meta straight to VRAM fixup slots (L1565-1575).

        if needs_fixup && tsw {
            let fb = ( (bx as i64) * (ncols as i64) + (jc_cwm as i64)) * 2;
            gstore(dstk_fixup, fb as u64, cmrx);
            gstore(dstk_fixup, (fb + 1) as u64, cmry);
        }

        if is_fixup && tsw {
            let fb = ( ( (gx as i64) + (bx as i64)) * (ncols as i64) + (jc_cwm as i64)) * 2;
            gstore(dstk_fixup, fb as u64, cmrx);
            gstore(dstk_fixup, (fb + 1) as u64, cmry);
        }
    }
    // ---- Parallel-warp meta combine (L1578-1649, np>1 only; nmeta=1 because np*16==warp_size).
    var cmn: f32 = 0.0;
    var cms0: f32 = 0.0;
    var crs0: f32 = 0.0;

    if np > 1 {
        let mfp = ty * 16 + tx;
        // np*cols_per_warp>=warp_size -> plain threadIdx.x
        let mf = mfp * 132 + 128;

        if ty % np == 0 {
            let m0x = smldf(smem, mf as u64);
            let m0y = smldf(smem, (mf + 1) as u64);
            cmn = m0x;
            var o: i32 = np * 16 / 2;

            if true {
                let o: i32 = (16 as i32);

                if o < 32 {
                    cmn = fmaxf2(cmn, shflx(cmn, o));
                }
            }
            o = (8 as i32);
            cms0 = expf(m0x - cmn);
            crs0 = cms0 * m0y;
            o = np * 16 / 2;

            if true {
                let o: i32 = (16 as i32);

                if o < 32 {
                    crs0 += shflx(crs0, o);
                }
            }
            o = (8 as i32);
        }
        barrier();
        // L1626

        if ty % np == 0 {
            smstf(smem, mf as u64, cms0);
            // combined scale+rowsum write-back (L1631-1636)
            smstf(smem, (mf + 1) as u64, crs0);

            if needs_fixup && (tx < 16) {
                let fb = ( (bx as i64) * (ncols as i64) + ( (ty / np) * 16 + tx) as i64) * 2;
                gstore(dstk_fixup, fb as u64, cmn);
                gstore(dstk_fixup, (fb + 1) as u64, crs0);
            }

            if is_fixup && (tx < 16) {
                let fb = ( ( (gx as i64) + (bx as i64)) * (ncols as i64) + ( (ty / np) * 16 + tx) as i64) * 2;
                gstore(dstk_fixup, fb as u64, cmn);
                gstore(dstk_fixup, (fb + 1) as u64, crs0);
            }
        }
    }
    // ---- VKQ staging + output (L1651-1777): single k00 pass because DV/2==nbatch_combine==128.
    // T_C_VKQ is half2 DATA_LAYOUT_I_MAJOR (Turing+): j=j0+get_i(l), k=k1+get_j(l), direct store.
    let js = ty * 16;
    var k1: i32 = 0;

    if true {
        let k1: i32 = (0 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (8 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (16 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (24 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (32 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (40 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (48 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (56 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (64 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (72 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (80 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (88 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (96 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (104 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (112 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (120 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }
    k1 = (128 as i32);
    barrier();
    // L1712

    if (np == 1) || (ty % np == 0) {
        let fix0 = ( (gx as i64) * 2 * (ncols as i64) + (bx as i64) * (ncols as i64) * 128) * 2;
        // float2 base -> float idx
        // {32,16,8,4} stride loop emits only stride_k=32 for nbatch_combine=128; stride_jc=1; the native
        // break is statically false for ncols=32|64.
        var jc0d: i32 = 0;

        if true {
            let jc0d: i32 = (0 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (2 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (4 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (6 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (8 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (10 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (12 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (14 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (16 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (18 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (20 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (22 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (24 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (26 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (28 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (30 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }

                    if true {
                        let ip: i32 = (1 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (2 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }
        jc0d = (32 as i32);
    }

    if np > 1 {
        barrier();
    }
    // L1775-1777: skipped when np==1 && nbatch_combine==DKQ/2
}

fn process_tile_apex_nc8_np1(
    q_f2: u64,
    k_h2: u64,
    v_h2: u64,
    mask_h: u64,
    sinks_f: u64,
    dstk: u64,
    dstk_fixup: u64,
    scale: f32,
    slope: f32,
    ne01z: i64,
    ne02: i32,
    gqa_ratio: i32,
    stride_q1: i64,
    stride_q2: i64,
    stride_k: i64,
    stride_v: i64,
    stride_mask: i64,
    jt: i32,
    zt_gqa: i32,
    kb0_start: i32,
    kb0_stop: i32,
    ncols1_abi: i32,
    np_abi: i32,
    needs_fixup: bool,
    is_fixup: bool,
    bx: i32,
    gx: i32,
    tx: i32,
    ty: i32,
    qregs: u64,
    vkqregs: u64,
    kmax: u64,
    rowsums: u64,
    smem: u64) {
    let ncols1: i32 = (8 as i32);
    let np: i32 = (1 as i32);
    // ncols2=8 fixed; nbatch_fa=32; DKQ/2=nbatch_K2=nbatch_V2=nbatch_combine=128; tile_stride=132.
    let ncols = ncols1 * 8;
    // KQ_max init -FLT_MAX/2 (L1272) with exact f32 bits: 2^127-2^103 = (2-2^-23)*2^126, computed
    // from exact powers of two (no bitcast primitive exists or is assumed).
    let fltmax2: f32 = 170141173319264429905852091742258332672.0;
    gstore(kmax, 0, 0.0 - fltmax2);
    gstore(kmax, 1, 0.0 - fltmax2);
    gstore(rowsums, 0, 0.0);
    gstore(rowsums, 1, 0.0);
    var zi: i32 = 0;

    if true {
        let zi: i32 = (0 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (1 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (2 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (3 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (4 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (5 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (6 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (7 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (8 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (9 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (10 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (11 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (12 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (13 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (14 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (15 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (16 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (17 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (18 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (19 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (20 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (21 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (22 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (23 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (24 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (25 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (26 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (27 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (28 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (29 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (30 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (31 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (32 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (33 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (34 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (35 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (36 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (37 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (38 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (39 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (40 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (41 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (42 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (43 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (44 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (45 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (46 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (47 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (48 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (49 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (50 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (51 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (52 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (53 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (54 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (55 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (56 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (57 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (58 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (59 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (60 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (61 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (62 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }

    if true {
        let zi: i32 = (63 as i32);
        stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
    }
    zi = (64 as i32);
    // VKQ_C zero init (mma.cuh tile ctor)
    // ---- Q staging, F32 -> scaled half2 into tile_Q (L1278-1317). The {32,16,8,4} granularity loop
    // emits only stride_k=32 for DKQ/2=128 (k0_start==k0_stop for the rest); stride_jc=1; the native
    // break (jc0+4>ncols && jc>=ncols) is statically false for ncols=32|64.
    let scale_h2 = h2pack(scale, scale);
    var jc0: i32 = 0;

    if true {
        let jc0: i32 = (0 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }

    if true {
        let jc0: i32 = (4 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }

    if true {
        let jc0: i32 = (8 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }

    if true {
        let jc0: i32 = (12 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }

    if true {
        let jc0: i32 = (16 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }

    if true {
        let jc0: i32 = (20 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }

    if true {
        let jc0: i32 = (24 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }

    if true {
        let jc0: i32 = (28 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }

    if true {
        let jc0: i32 = (32 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }

    if true {
        let jc0: i32 = (36 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }

    if true {
        let jc0: i32 = (40 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }

    if true {
        let jc0: i32 = (44 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }

    if true {
        let jc0: i32 = (48 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }

    if true {
        let jc0: i32 = (52 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }

    if true {
        let jc0: i32 = (56 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }

    if true {
        let jc0: i32 = (60 as i32);
        let jc = jc0 + ty;
        let j = jc / 8;
        let c = jc % 8;
        let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
        var k0: i32 = 0;

        if true {
            let k0: i32 = (0 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (32 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (64 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }

        if true {
            let k0: i32 = (96 as i32);
            let k = k0 + tx;

            if valid {
                let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                smst(
                    smem,
                    (jc * 132 + k) as u64,
                    h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
            } else {
                smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
            }
        }
        k0 = (128 as i32);
    }
    jc0 = (64 as i32);
    barrier();
    // L1319
    // ---- Q_B[16] register tiles via ldmatrix.x4 from tile_Q (L1321-1328); j0=(ty/np)*cols_per_warp.
    // load_ldmatrix<tile<16,8,half2>> int-word address (mma.cuh L857): + (tx%16)*132 + (tx/16)*4.
    let j0 = (ty / np) * 16;
    var q0: i32 = 0;

    if true {
        let q0: i32 = (0 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (8 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (16 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (24 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (32 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (40 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (48 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (56 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (64 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (72 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (80 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (88 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (96 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (104 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (112 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }

    if true {
        let q0: i32 = (120 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
        stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
        stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
        stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
    }
    q0 = (128 as i32);
    barrier();
    // L1330; the K/V cp.async preload may now overwrite the Q image (shared union)
    // ---- Initial async preload of mask and K for kb0=kb0_start (L1334-1347, nstages>1 branch).
    // ncols2=8>1 makes the mask load unconditional; K tile at smem+0, tile_mask at smem+32768.
    load_mask(
        mask_h,
        smem + 32768,
        stride_mask,
        (kb0_start * 32) as i64,
        (jt as i64) * (ncols1 as i64),
        ne01z,
        ncols1,
        tx,
        ty);
    load_kv(k_h2, smem, stride_k, (kb0_start * 32) as i64, tx, ty);
    // ---- kb0 loop (L1349-1390): ncols2>1 && !use_sparse selects the oob_check=false path with
    // constexpr k_VKQ_sup=nbatch_fa=32; the first iterate waits the preload cp.asyncs (iter L634-635).
    var kb0: i32 = kb0_start;

    while kb0 < kb0_stop - 1 {
        iterate(
            k_h2,
            v_h2,
            mask_h,
            stride_k,
            stride_v,
            stride_mask,
            slope,
            jt,
            kb0,
            ne01z,
            ncols1,
            qregs,
            vkqregs,
            kmax,
            rowsums,
            smem,
            tx,
            ty,
            np,
            false);
        kb0 += 1;
    }
    iterate(
        k_h2,
        v_h2,
        mask_h,
        stride_k,
        stride_v,
        stride_mask,
        slope,
        jt,
        kb0,
        ne01z,
        ncols1,
        qregs,
        vkqregs,
        kmax,
        rowsums,
        smem,
        tx,
        ty,
        np,
        true);
    barrier();
    // L1394-1396: nstages>1 && nwarps*cols_per_warp(64)>nbatch_fa(32)
    // ---- Sum partial KQ rowsums (L1399-1424): TURING+ path, cols_per_warp!=8 -> offsets {2,1}, width 32.
    var col: i32 = 0;

    if true {
        let col: i32 = (0 as i32);
        var v = gload(rowsums, col as u64);
        var o: i32 = 2;

        if true {
            let o: i32 = (2 as i32);
            v += shflx(v, o);
        }

        if true {
            let o: i32 = (1 as i32);
            v += shflx(v, o);
        }
        o = (0 as i32);
        gstore(rowsums, col as u64, v);
    }

    if true {
        let col: i32 = (1 as i32);
        var v = gload(rowsums, col as u64);
        var o: i32 = 2;

        if true {
            let o: i32 = (2 as i32);
            v += shflx(v, o);
        }

        if true {
            let o: i32 = (1 as i32);
            v += shflx(v, o);
        }
        o = (0 as i32);
        gstore(rowsums, col as u64, v);
    }
    col = (2 as i32);
    // ---- Attention sinks (L1429-1501): !is_fixup, leading warp(s) only (ty%np==0), optional pointer.
    var ks0: f32 = 0.0;
    var ks1: f32 = 0.0;

    if ( (!is_fixup) && ( (np == 1) || (ty % np == 0)) && (sinks_f != 0)) {
        // jc=(threadIdx.y/np)*16 + T_C_KQ::get_i(2*col) (tile<16,16,float>); sink index jc%ncols2=jc%8.
        let jc0s = (ty / np) * 16 + gi16(0, tx);
        let jc1s = (ty / np) * 16 + gi16(2, tx);
        let s0 = gload(sinks_f, (jc0s % 8) as u64);
        let s1 = gload(sinks_f, (jc1s % 8) as u64);
        let n0 = fmaxf2(gload(kmax, 0), s0);
        let d0 = gload(kmax, 0) - n0;
        ks0 = expf(d0);
        gstore(kmax, 0, n0);
        // *((u32*)&scale)*= (d>=SOFTMAX_FTZ_THRESHOLD) with SOFTMAX_FTZ_THRESHOLD=-20.0f, as select.

        if d0 < -20.0 {
            ks0 = 0.0;
        }
        gstore(rowsums, 0, ks0 * gload(rowsums, 0) + expf(s0 - n0));
        let n1 = fmaxf2(gload(kmax, 1), s1);
        let d1 = gload(kmax, 1) - n1;
        ks1 = expf(d1);
        gstore(kmax, 1, n1);

        if d1 < -20.0 {
            ks1 = 0.0;
        }
        gstore(rowsums, 1, ks1 * gload(rowsums, 1) + expf(s1 - n1));
        // VKQ_C rescale (L1458-1469, cols_per_warp!=8): per col scale x[l0+col], l0 in {0,2}.
        let h0 = h2pack(ks0, ks0);
        let h1 = h2pack(ks1, ks1);
        var i0: i32 = 0;

        if true {
            let i0: i32 = (0 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (1 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (2 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (3 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (4 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (5 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (6 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (7 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (8 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (9 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (10 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (11 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (12 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (13 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (14 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }

        if true {
            let i0: i32 = (15 as i32);
            stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
            stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
            stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
            stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
        }
        i0 = (16 as i32);
    }
    // ---- Combine meta (L1503-1576): swz=true -> barrier first (L1555-1557); jc_cwm uses
    // tile<16,8,half2> get_i(tx%4)=((tx%2)*8)+tx/4; meta floats at tile_Q row padding word 128/129.
    barrier();
    let jc_cwm = ty * 16 + gi8(tx % 4, tx);
    let cmrx = gload(kmax, (tx % 2) as u64);
    let cmry = gload(rowsums, (tx % 2) as u64);
    let tsw = tx % 4 < 2;

    if ( ( ( (!needs_fixup) && (!is_fixup)) || (np > 1)) && tsw) {
        smstf(smem, (jc_cwm * 132 + 128) as u64, cmrx);
        // ((float2*)tile_Q)[jc_cwm*66+64]
        smstf(smem, (jc_cwm * 132 + 129) as u64, cmry);
    }
    barrier();
    // L1563

    if np == 1 {
        // No combination: meta straight to VRAM fixup slots (L1565-1575).

        if needs_fixup && tsw {
            let fb = ( (bx as i64) * (ncols as i64) + (jc_cwm as i64)) * 2;
            gstore(dstk_fixup, fb as u64, cmrx);
            gstore(dstk_fixup, (fb + 1) as u64, cmry);
        }

        if is_fixup && tsw {
            let fb = ( ( (gx as i64) + (bx as i64)) * (ncols as i64) + (jc_cwm as i64)) * 2;
            gstore(dstk_fixup, fb as u64, cmrx);
            gstore(dstk_fixup, (fb + 1) as u64, cmry);
        }
    }
    // ---- Parallel-warp meta combine (L1578-1649, np>1 only; nmeta=1 because np*16==warp_size).
    var cmn: f32 = 0.0;
    var cms0: f32 = 0.0;
    var crs0: f32 = 0.0;

    if np > 1 {
        let mfp = ty * 16 + tx;
        // np*cols_per_warp>=warp_size -> plain threadIdx.x
        let mf = mfp * 132 + 128;

        if ty % np == 0 {
            let m0x = smldf(smem, mf as u64);
            let m0y = smldf(smem, (mf + 1) as u64);
            cmn = m0x;
            var o: i32 = np * 16 / 2;
            o = (8 as i32);
            cms0 = expf(m0x - cmn);
            crs0 = cms0 * m0y;
            o = np * 16 / 2;
            o = (8 as i32);
        }
        barrier();
        // L1626

        if ty % np == 0 {
            smstf(smem, mf as u64, cms0);
            // combined scale+rowsum write-back (L1631-1636)
            smstf(smem, (mf + 1) as u64, crs0);

            if needs_fixup && (tx < 16) {
                let fb = ( (bx as i64) * (ncols as i64) + ( (ty / np) * 16 + tx) as i64) * 2;
                gstore(dstk_fixup, fb as u64, cmn);
                gstore(dstk_fixup, (fb + 1) as u64, crs0);
            }

            if is_fixup && (tx < 16) {
                let fb = ( ( (gx as i64) + (bx as i64)) * (ncols as i64) + ( (ty / np) * 16 + tx) as i64) * 2;
                gstore(dstk_fixup, fb as u64, cmn);
                gstore(dstk_fixup, (fb + 1) as u64, crs0);
            }
        }
    }
    // ---- VKQ staging + output (L1651-1777): single k00 pass because DV/2==nbatch_combine==128.
    // T_C_VKQ is half2 DATA_LAYOUT_I_MAJOR (Turing+): j=j0+get_i(l), k=k1+get_j(l), direct store.
    let js = ty * 16;
    var k1: i32 = 0;

    if true {
        let k1: i32 = (0 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (8 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (16 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (24 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (32 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (40 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (48 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (56 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (64 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (72 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (80 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (88 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (96 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (104 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (112 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (120 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
        }
        l = (4 as i32);
    }
    k1 = (128 as i32);
    barrier();
    // L1712

    if (np == 1) || (ty % np == 0) {
        let fix0 = ( (gx as i64) * 2 * (ncols as i64) + (bx as i64) * (ncols as i64) * 128) * 2;
        // float2 base -> float idx
        // {32,16,8,4} stride loop emits only stride_k=32 for nbatch_combine=128; stride_jc=1; the native
        // break is statically false for ncols=32|64.
        var jc0d: i32 = 0;

        if true {
            let jc0d: i32 = (0 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (4 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (8 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (12 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (16 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (20 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (24 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (28 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (32 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (36 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (40 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (44 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (48 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (52 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (56 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }

        if true {
            let jc0d: i32 = (60 as i32);
            let jc_dst = jc0d + (ty / np);
            let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
            let j_dst = jc_dst / 8;
            let c_dst = jc_dst % 8;
            let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

            if !skip {
                let metaf = jc_tile_k * 132 + 128;
                var k0: i32 = 0;

                if true {
                    let k0: i32 = (0 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (32 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (64 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }

                if true {
                    let k0: i32 = (96 as i32);
                    let k = k0 + tx;
                    var vx: f32 = 0.0;
                    var vy: f32 = 0.0;
                    var ip: i32 = 0;

                    if true {
                        let ip: i32 = (0 as i32);
                        let crs: f32 = if np == 1 {
                            1.0 as f32
                        } else {
                            smldf(smem, (metaf + ip * 16 * 132) as u64)
                        }
                        ;
                        let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                        vx += hlo(w) * crs;
                        vy += hhi(w) * crs;
                    }
                    ip = (1 as i32);

                    if (!needs_fixup) && (!is_fixup) {
                        let rj = smldf(smem, (metaf + 1) as u64);
                        vx = vx / rj;
                        vy = vy / rj;
                    }

                    if is_fixup {
                        let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                        gstore(dstk_fixup, f as u64, vx);
                        gstore(dstk_fixup, (f + 1) as u64, vy);
                    } else {
                        let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                        gstore(dstk, f as u64, vx);
                        gstore(dstk, (f + 1) as u64, vy);
                    }
                }
                k0 = (128 as i32);
            }
        }
        jc0d = (64 as i32);
    }

    if np > 1 {
        barrier();
    }
    // L1775-1777: skipped when np==1 && nbatch_combine==DKQ/2
}

fn process_tile(
    q_f2: u64,
    k_h2: u64,
    v_h2: u64,
    mask_h: u64,
    sinks_f: u64,
    dstk: u64,
    dstk_fixup: u64,
    scale: f32,
    slope: f32,
    ne01z: i64,
    ne02: i32,
    gqa_ratio: i32,
    stride_q1: i64,
    stride_q2: i64,
    stride_k: i64,
    stride_v: i64,
    stride_mask: i64,
    jt: i32,
    zt_gqa: i32,
    kb0_start: i32,
    kb0_stop: i32,
    ncols1: i32,
    np: i32,
    needs_fixup: bool,
    is_fixup: bool,
    bx: i32,
    gx: i32,
    tx: i32,
    ty: i32,
    qregs: u64,
    vkqregs: u64,
    kmax: u64,
    rowsums: u64,
    smem: u64) {
    if ( ( (ncols1 == (4 as i32)) && (np == (2 as i32)))) {
        process_tile_apex_nc4_np2(
            q_f2,
            k_h2,
            v_h2,
            mask_h,
            sinks_f,
            dstk,
            dstk_fixup,
            scale,
            slope,
            ne01z,
            ne02,
            gqa_ratio,
            stride_q1,
            stride_q2,
            stride_k,
            stride_v,
            stride_mask,
            jt,
            zt_gqa,
            kb0_start,
            kb0_stop,
            ncols1,
            np,
            needs_fixup,
            is_fixup,
            bx,
            gx,
            tx,
            ty,
            qregs,
            vkqregs,
            kmax,
            rowsums,
            smem);
    } else {
        if ( ( (ncols1 == (8 as i32)) && (np == (1 as i32)))) {
            process_tile_apex_nc8_np1(
                q_f2,
                k_h2,
                v_h2,
                mask_h,
                sinks_f,
                dstk,
                dstk_fixup,
                scale,
                slope,
                ne01z,
                ne02,
                gqa_ratio,
                stride_q1,
                stride_q2,
                stride_k,
                stride_v,
                stride_mask,
                jt,
                zt_gqa,
                kb0_start,
                kb0_stop,
                ncols1,
                np,
                needs_fixup,
                is_fixup,
                bx,
                gx,
                tx,
                ty,
                qregs,
                vkqregs,
                kmax,
                rowsums,
                smem);
        } else {
            // ncols2=8 fixed; nbatch_fa=32; DKQ/2=nbatch_K2=nbatch_V2=nbatch_combine=128; tile_stride=132.
            let ncols = ncols1 * 8;
            // KQ_max init -FLT_MAX/2 (L1272) with exact f32 bits: 2^127-2^103 = (2-2^-23)*2^126, computed
            // from exact powers of two (no bitcast primitive exists or is assumed).
            let fltmax2: f32 = 170141173319264429905852091742258332672.0;
            gstore(kmax, 0, 0.0 - fltmax2);
            gstore(kmax, 1, 0.0 - fltmax2);
            gstore(rowsums, 0, 0.0);
            gstore(rowsums, 1, 0.0);
            var zi: i32 = 0;

            while zi < 64 {
                stu32(vkqregs, zi as u64, h2pack(0.0, 0.0));
                zi += 1;
            }
            // VKQ_C zero init (mma.cuh tile ctor)
            // ---- Q staging, F32 -> scaled half2 into tile_Q (L1278-1317). The {32,16,8,4} granularity loop
            // emits only stride_k=32 for DKQ/2=128 (k0_start==k0_stop for the rest); stride_jc=1; the native
            // break (jc0+4>ncols && jc>=ncols) is statically false for ncols=32|64.
            let scale_h2 = h2pack(scale, scale);
            var jc0: i32 = 0;

            while jc0 < ncols {
                let jc = jc0 + ty;
                let j = jc / 8;
                let c = jc % 8;
                let valid = ( (ncols1 == 1) || ( ( (jt as i64) * (ncols1 as i64) + (j as i64)) < ne01z)) && (zt_gqa * 8 + c < gqa_ratio);
                var k0: i32 = 0;

                while k0 < 128 {
                    let k = k0 + tx;

                    if valid {
                        let qidx = ( (jt as i64) * (ncols1 as i64) + (j as i64)) * stride_q1 + (c as i64) * stride_q2 + (k as i64);
                        smst(
                            smem,
                            (jc * 132 + k) as u64,
                            h2mul(h2pack(gload(q_f2, (qidx * 2) as u64), gload(q_f2, (qidx * 2 + 1) as u64)), scale_h2));
                    } else {
                        smst(smem, (jc * 132 + k) as u64, h2pack(0.0, 0.0));
                    }
                    k0 += 32;
                }
                jc0 += 4;
            }
            barrier();
            // L1319
            // ---- Q_B[16] register tiles via ldmatrix.x4 from tile_Q (L1321-1328); j0=(ty/np)*cols_per_warp.
            // load_ldmatrix<tile<16,8,half2>> int-word address (mma.cuh L857): + (tx%16)*132 + (tx/16)*4.
            let j0 = (ty / np) * 16;
            var q0: i32 = 0;

            while q0 < 128 {
                let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
                let r = ldm4(smem + (w as u64) * (4 as u64));
                stu32(qregs, (4 * (q0 / 8)) as u64, r.a);
                stu32(qregs, (4 * (q0 / 8) + 1) as u64, r.b);
                stu32(qregs, (4 * (q0 / 8) + 2) as u64, r.c);
                stu32(qregs, (4 * (q0 / 8) + 3) as u64, r.d);
                q0 += 8;
            }
            barrier();
            // L1330; the K/V cp.async preload may now overwrite the Q image (shared union)
            // ---- Initial async preload of mask and K for kb0=kb0_start (L1334-1347, nstages>1 branch).
            // ncols2=8>1 makes the mask load unconditional; K tile at smem+0, tile_mask at smem+32768.
            load_mask(
                mask_h,
                smem + 32768,
                stride_mask,
                (kb0_start * 32) as i64,
                (jt as i64) * (ncols1 as i64),
                ne01z,
                ncols1,
                tx,
                ty);
            load_kv(k_h2, smem, stride_k, (kb0_start * 32) as i64, tx, ty);
            // ---- kb0 loop (L1349-1390): ncols2>1 && !use_sparse selects the oob_check=false path with
            // constexpr k_VKQ_sup=nbatch_fa=32; the first iterate waits the preload cp.asyncs (iter L634-635).
            var kb0: i32 = kb0_start;

            while kb0 < kb0_stop - 1 {
                iterate(
                    k_h2,
                    v_h2,
                    mask_h,
                    stride_k,
                    stride_v,
                    stride_mask,
                    slope,
                    jt,
                    kb0,
                    ne01z,
                    ncols1,
                    qregs,
                    vkqregs,
                    kmax,
                    rowsums,
                    smem,
                    tx,
                    ty,
                    np,
                    false);
                kb0 += 1;
            }
            iterate(
                k_h2,
                v_h2,
                mask_h,
                stride_k,
                stride_v,
                stride_mask,
                slope,
                jt,
                kb0,
                ne01z,
                ncols1,
                qregs,
                vkqregs,
                kmax,
                rowsums,
                smem,
                tx,
                ty,
                np,
                true);
            barrier();
            // L1394-1396: nstages>1 && nwarps*cols_per_warp(64)>nbatch_fa(32)
            // ---- Sum partial KQ rowsums (L1399-1424): TURING+ path, cols_per_warp!=8 -> offsets {2,1}, width 32.
            var col: i32 = 0;

            while col < 2 {
                var v = gload(rowsums, col as u64);
                var o: i32 = 2;

                while o >= 1 {
                    v += shflx(v, o);
                    o = o >> 1;
                }
                gstore(rowsums, col as u64, v);
                col += 1;
            }
            // ---- Attention sinks (L1429-1501): !is_fixup, leading warp(s) only (ty%np==0), optional pointer.
            var ks0: f32 = 0.0;
            var ks1: f32 = 0.0;

            if ( (!is_fixup) && ( (np == 1) || (ty % np == 0)) && (sinks_f != 0)) {
                // jc=(threadIdx.y/np)*16 + T_C_KQ::get_i(2*col) (tile<16,16,float>); sink index jc%ncols2=jc%8.
                let jc0s = (ty / np) * 16 + gi16(0, tx);
                let jc1s = (ty / np) * 16 + gi16(2, tx);
                let s0 = gload(sinks_f, (jc0s % 8) as u64);
                let s1 = gload(sinks_f, (jc1s % 8) as u64);
                let n0 = fmaxf2(gload(kmax, 0), s0);
                let d0 = gload(kmax, 0) - n0;
                ks0 = expf(d0);
                gstore(kmax, 0, n0);
                // *((u32*)&scale)*= (d>=SOFTMAX_FTZ_THRESHOLD) with SOFTMAX_FTZ_THRESHOLD=-20.0f, as select.

                if d0 < -20.0 {
                    ks0 = 0.0;
                }
                gstore(rowsums, 0, ks0 * gload(rowsums, 0) + expf(s0 - n0));
                let n1 = fmaxf2(gload(kmax, 1), s1);
                let d1 = gload(kmax, 1) - n1;
                ks1 = expf(d1);
                gstore(kmax, 1, n1);

                if d1 < -20.0 {
                    ks1 = 0.0;
                }
                gstore(rowsums, 1, ks1 * gload(rowsums, 1) + expf(s1 - n1));
                // VKQ_C rescale (L1458-1469, cols_per_warp!=8): per col scale x[l0+col], l0 in {0,2}.
                let h0 = h2pack(ks0, ks0);
                let h1 = h2pack(ks1, ks1);
                var i0: i32 = 0;

                while i0 < 16 {
                    stu32(vkqregs, (4 * i0 + 0) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 0) as u64), h0));
                    stu32(vkqregs, (4 * i0 + 2) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 2) as u64), h0));
                    stu32(vkqregs, (4 * i0 + 1) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 1) as u64), h1));
                    stu32(vkqregs, (4 * i0 + 3) as u64, h2mul(ldu32(vkqregs, (4 * i0 + 3) as u64), h1));
                    i0 += 1;
                }
            }
            // ---- Combine meta (L1503-1576): swz=true -> barrier first (L1555-1557); jc_cwm uses
            // tile<16,8,half2> get_i(tx%4)=((tx%2)*8)+tx/4; meta floats at tile_Q row padding word 128/129.
            barrier();
            let jc_cwm = ty * 16 + gi8(tx % 4, tx);
            let cmrx = gload(kmax, (tx % 2) as u64);
            let cmry = gload(rowsums, (tx % 2) as u64);
            let tsw = tx % 4 < 2;

            if ( ( ( (!needs_fixup) && (!is_fixup)) || (np > 1)) && tsw) {
                smstf(smem, (jc_cwm * 132 + 128) as u64, cmrx);
                // ((float2*)tile_Q)[jc_cwm*66+64]
                smstf(smem, (jc_cwm * 132 + 129) as u64, cmry);
            }
            barrier();
            // L1563

            if np == 1 {
                // No combination: meta straight to VRAM fixup slots (L1565-1575).

                if needs_fixup && tsw {
                    let fb = ( (bx as i64) * (ncols as i64) + (jc_cwm as i64)) * 2;
                    gstore(dstk_fixup, fb as u64, cmrx);
                    gstore(dstk_fixup, (fb + 1) as u64, cmry);
                }

                if is_fixup && tsw {
                    let fb = ( ( (gx as i64) + (bx as i64)) * (ncols as i64) + (jc_cwm as i64)) * 2;
                    gstore(dstk_fixup, fb as u64, cmrx);
                    gstore(dstk_fixup, (fb + 1) as u64, cmry);
                }
            }
            // ---- Parallel-warp meta combine (L1578-1649, np>1 only; nmeta=1 because np*16==warp_size).
            var cmn: f32 = 0.0;
            var cms0: f32 = 0.0;
            var crs0: f32 = 0.0;

            if np > 1 {
                let mfp = ty * 16 + tx;
                // np*cols_per_warp>=warp_size -> plain threadIdx.x
                let mf = mfp * 132 + 128;

                if ty % np == 0 {
                    let m0x = smldf(smem, mf as u64);
                    let m0y = smldf(smem, (mf + 1) as u64);
                    cmn = m0x;
                    var o: i32 = np * 16 / 2;

                    while o >= 16 {
                        if o < 32 {
                            cmn = fmaxf2(cmn, shflx(cmn, o));
                        }
                        o = o >> 1;
                    }
                    cms0 = expf(m0x - cmn);
                    crs0 = cms0 * m0y;
                    o = np * 16 / 2;

                    while o >= 16 {
                        if o < 32 {
                            crs0 += shflx(crs0, o);
                        }
                        o = o >> 1;
                    }
                }
                barrier();
                // L1626

                if ty % np == 0 {
                    smstf(smem, mf as u64, cms0);
                    // combined scale+rowsum write-back (L1631-1636)
                    smstf(smem, (mf + 1) as u64, crs0);

                    if needs_fixup && (tx < 16) {
                        let fb = ( (bx as i64) * (ncols as i64) + ( (ty / np) * 16 + tx) as i64) * 2;
                        gstore(dstk_fixup, fb as u64, cmn);
                        gstore(dstk_fixup, (fb + 1) as u64, crs0);
                    }

                    if is_fixup && (tx < 16) {
                        let fb = ( ( (gx as i64) + (bx as i64)) * (ncols as i64) + ( (ty / np) * 16 + tx) as i64) * 2;
                        gstore(dstk_fixup, fb as u64, cmn);
                        gstore(dstk_fixup, (fb + 1) as u64, crs0);
                    }
                }
            }
            // ---- VKQ staging + output (L1651-1777): single k00 pass because DV/2==nbatch_combine==128.
            // T_C_VKQ is half2 DATA_LAYOUT_I_MAJOR (Turing+): j=j0+get_i(l), k=k1+get_j(l), direct store.
            let js = ty * 16;
            var k1: i32 = 0;

            while k1 < 128 {
                var l: i32 = 0;

                while l < 4 {
                    smst(
                        smem,
                        ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64,
                        ldu32(vkqregs, (4 * (k1 / 8) + l) as u64));
                    l += 1;
                }
                k1 += 8;
            }
            barrier();
            // L1712

            if (np == 1) || (ty % np == 0) {
                let fix0 = ( (gx as i64) * 2 * (ncols as i64) + (bx as i64) * (ncols as i64) * 128) * 2;
                // float2 base -> float idx
                // {32,16,8,4} stride loop emits only stride_k=32 for nbatch_combine=128; stride_jc=1; the native
                // break is statically false for ncols=32|64.
                var jc0d: i32 = 0;

                while jc0d < ncols {
                    let jc_dst = jc0d + (ty / np);
                    let jc_tile_k = (jc_dst / 16) * (np * 16) + jc_dst % 16;
                    let j_dst = jc_dst / 8;
                    let c_dst = jc_dst % 8;
                    let skip = (!is_fixup) && ( ( (ncols1 > 1) && ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) >= ne01z)) || ( (zt_gqa * 8 + c_dst) >= gqa_ratio));

                    if !skip {
                        let metaf = jc_tile_k * 132 + 128;
                        var k0: i32 = 0;

                        while k0 < 128 {
                            let k = k0 + tx;
                            var vx: f32 = 0.0;
                            var vy: f32 = 0.0;
                            var ip: i32 = 0;

                            while ip < np {
                                let crs: f32 = if np == 1 {
                                    1.0 as f32
                                } else {
                                    smldf(smem, (metaf + ip * 16 * 132) as u64)
                                }
                                ;
                                let w = smld(smem, ( (jc_tile_k + ip * 16) * 132 + k) as u64);
                                vx += hlo(w) * crs;
                                vy += hhi(w) * crs;
                                ip += 1;
                            }

                            if (!needs_fixup) && (!is_fixup) {
                                let rj = smldf(smem, (metaf + 1) as u64);
                                vx = vx / rj;
                                vy = vy / rj;
                            }

                            if is_fixup {
                                let f = fix0 + ( (jc_dst as i64) * 128 + (k as i64)) * 2;
                                gstore(dstk_fixup, f as u64, vx);
                                gstore(dstk_fixup, (f + 1) as u64, vy);
                            } else {
                                let f = ( ( ( (jt as i64) * (ncols1 as i64) + (j_dst as i64)) * (ne02 as i64) + (c_dst as i64)) * 128 + (k as i64)) * 2;
                                gstore(dstk, f as u64, vx);
                                gstore(dstk, (f + 1) as u64, vy);
                            }
                            k0 += 32;
                        }
                    }
                    jc0d += 4 / np;
                }
            }

            if np > 1 {
                barrier();
            }
            // L1775-1777: skipped when np==1 && nbatch_combine==DKQ/2
        }
    }
}
// Retention entry; the parent never executes main.

fn main() {
    process_tile(
        0 as u64,
        0 as u64,
        0 as u64,
        0 as u64,
        0 as u64,
        0 as u64,
        0 as u64,
        1.0,
        1.0,
        0 as i64,
        0,
        0,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0,
        0,
        0,
        0,
        4,
        2,
        false,
        false,
        0,
        0,
        0,
        0,
        0 as u64,
        0 as u64,
        0 as u64,
        0 as u64,
        0 as u64);
}

