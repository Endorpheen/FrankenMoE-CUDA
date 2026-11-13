// fg176i: Goose arithmetic/control of flash_attn_ext_f16_iter (fattn-mma-f16.cuh L583-1072), fixed
// Apex CUDA Ada target: DKQ=DV=256, ncols1=4 or 8, ncols2=8, nwarps=4, nbatch_fa=32,
// nbatch_K2=nbatch_V2=128, Q_in_reg, nstages=2 cp.async double stage, swizzled tiles,
// cols_per_warp=16, cols_per_thread=2, np=2 (ncols1=4) else 1, softcap/sparse/oob/V_is_K_view=false.
// Turing tile set (mma_tile_sizes<256,ncols>): T_A_KQ/T_B_KQ/T_A_VKQ/T_B_VKQ/T_C_VKQ=tile<16,8,half2>,
// T_C_KQ=tile<16,16,float> (wide column-major: physical I=Q column j, physical J=KV row i).
// Everything except raw single-ISA effects runs here: operand swaps (KQ mma A=Q_B B=K_A; VKQ mma
// A=probs B=transposed-V), f32/f16 pair B-register order 0,2 then 1,3, ldmatrix.x4.trans register
// remap 0,2,1,3, swizzled shared addressing, per-lane fragment index math, mask row/col mapping,
// online softmax (offset 3*0.6931, shfl_xor 2/1, FTZ bit-multiply at diff<-20, exact fused rowsum
// step), F32->half2 prob pack (no transpose on Turing) and the double-stage wait/barrier/load-next
// sequencing (next K+mask issued before the current VKQ mma). K/mask issue loops are the fg175l
// Goose loaders; no native algorithm callback. Shared map (bytes from smem): tile_K [0,16384) 32 rows
// x 128 half2 (row stride 512 B), tile_V [16384,32768), tile_mask [32768,32768+ncols1*80) ncols1 rows
// x 20 half2 (16 B pad). Block dim3(32,4): tx=threadIdx.x lane, ty=threadIdx.y warp.
extern "fg175c_fg176i_bits" fn bits(v: f32) -> u32;
extern "fg175c_fg176i_frombits" fn frombits(v: u32) -> f32;
struct U2 {
    a: u32, b: u32
}
struct U4 {
    a: u32, b: u32, c: u32, d: u32
}
struct F4 {
    x: f32, y: f32, z: f32, w: f32
}
// fg175l Goose loaders (cp.async.cg 16 B issue loops) imported under their raw adapter names.
extern "fg175l_load_kv" fn load_kv(src: u64, dst_shared: u64, stride_kv_h2: i64, kv_row0: i64, tx: i32, ty: i32);
extern "fg175l_load_mask" fn load_mask(src: u64, dst_shared: u64, stride_mask_half: i64, kv_row0: i64, query0: i64, n_queries: i64, ncols1: i32, tx: i32, ty: i32);
// Raw effects: absolute byte addresses (generic for POD state, shared for tiles/ldmatrix).
extern "fg175c_fg176i_ldu" fn ldu(a: u64) -> u32;
extern "fg175c_fg176i_stu" fn stu(a: u64, v: u32);
extern "fg175c_fg176i_ldm4" fn ldm4(a: u64) -> U4;
extern "fg175c_fg176i_ldm4t" fn ldm4t(a: u64) -> U4;
extern "fg175c_fg176i_mma_f32" fn mma_f32(d: F4, a0: u32, a1: u32, a2: u32, a3: u32, b0: u32, b1: u32) -> F4;
extern "fg175c_fg176i_mma_f16" fn mma_f16(d: U2, a0: u32, a1: u32, a2: u32, a3: u32, b0: u32, b1: u32) -> U2;
extern "fg175c_fg176i_hlo" fn hlo(w: u32) -> f32;
extern "fg175c_fg176i_hhi" fn hhi(w: u32) -> f32;
extern "fg175c_fg176i_h2" fn h2(a: f32, b: f32) -> u32;
extern "fg175c_fg176i_mulh2" fn mulh2(w: u32, s: u32) -> u32;
extern "fg175c_fg176i_expf" fn expf(x: f32) -> f32;
extern "fg175c_fg176i_fmaf" fn fmaf(a: f32, b: f32, c: f32) -> f32;
extern "fg175c_fg176i_shflx" fn shflx(x: f32, off: i32) -> f32;
extern "fg175c_fg176i_cpwait" fn cpwait();
extern "fg175c_fg176i_barrier" fn barrier();
// ggml_cuda_mma::swizzle<128,half2>(off,i): stride_bytes=512 -> off ^ ((i%8)<<2) in half2 units.

fn swz_h2(off: i32, row: i32) -> i32 {
    return off ^ ( (row % 8) << 2);
}
// Per-lane ldmatrix source address (load_ldmatrix_swizzled/_trans_swizzled, I=16, J=8):
// i=tx%16, j=(tx/16)*(J/2)=4, offset_ij=off+i*stride+j, swizzled by i; stride_tile=128 half2.

fn ldm_addr(base: u64, off: i32, tx: i32) -> u64 {
    let row = tx % 16;
    return base + ( (swz_h2(off + row * 128 + (tx / 16) * 4, row) as u64) * (4 as u64));
}
// ldmatrix.x4.trans hardware registers h0..h3 bind to tile registers xi[0],xi[2],xi[1],xi[3]
// (native asm order 0,2,1,3); reorder here so downstream uses plain tile order x0..x3.

fn ldm4t_tile(a: u64) -> U4 {
    let z = ldm4t(a);
    return U4 {
        z.a, z.c, z.b, z.d
    }
    ;
}
// tile<16,16,float> fragment mapping (wide KQ_C): get_i(l)=((l/2)%2)*8+tx/4 -> Q column j,
// get_j(l)=(l/4)*8+(tx%4)*2+l%2 -> KV row i.

fn gi_c(l: i32, tx: i32) -> i32 {
    return ( (l / 2) % 2) * 8 + tx / 4;
}

fn gj_c(l: i32, tx: i32) -> i32 {
    return (l / 4) * 8 + (tx % 4) * 2 + l % 2;
}
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
extern "fg175l_cp16" fn cp16(dst_shared_generic: u64, src_global: u64);
// cp.async.cg.shared.global 16B, L2::64B
extern "fg175c_fg175p_gload" fn gload(p: u64, i: u64) -> f32;
// *(f32*)(p+4*i), generic/global element load
extern "fg175c_fg175p_gstore" fn gstore(p: u64, i: u64, v: f32);
// *(f32*)(p+4*i)=v
extern "fg175c_fg175p_ldu32" fn ldu32(p: u64, i: u64) -> u32;
// *(u32*)(p+4*i), generic (POD stack)
extern "fg175c_fg175p_stu32" fn stu32(p: u64, i: u64, v: u32);
// *(u32*)(p+4*i)=v
extern "fg175c_fg175p_smld" fn smld(p: u64, i: u64) -> u32;
// shared u32 word at p+4*i
extern "fg175c_fg175p_smst" fn smst(p: u64, i: u64, v: u32);
// shared u32 word at p+4*i=v
extern "fg175c_fg175p_smldf" fn smldf(p: u64, i: u64) -> f32;
// shared f32 at p+4*i
extern "fg175c_fg175p_smstf" fn smstf(p: u64, i: u64, v: f32);
// shared f32 at p+4*i=v
extern "fg175c_fg175p_h2mul" fn h2mul(a: u32, b: u32) -> u32;
// half2*half2, IEEE RN per half lane
extern "fg175c_fg175p_h2pack" fn h2pack(x: f32, y: f32) -> u32;
// {cvt.rn.f16.f32(x), cvt.rn.f16.f32(y)}
// low half -> f32
// high half -> f32
// ldmatrix.sync.aligned.m8n8.x4.b16, byte address
// __shfl_xor_sync(0xFFFFFFFF,v,o,32)
// __syncthreads
// The single extern device function: actual Goose port of flash_attn_ext_f16_iter for this fixed
// configuration (sibling worker module, symbol fg175m_iter; function named iterate there).
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
struct QueryRegs {
    q0: u32, q1: u32, q2: u32, q3: u32, q4: u32, q5: u32, q6: u32, q7: u32, q8: u32, q9: u32, q10: u32, q11: u32, q12: u32, q13: u32, q14: u32, q15: u32, q16: u32, q17: u32, q18: u32, q19: u32, q20: u32, q21: u32, q22: u32, q23: u32, q24: u32, q25: u32, q26: u32, q27: u32, q28: u32, q29: u32, q30: u32, q31: u32, q32: u32, q33: u32, q34: u32, q35: u32, q36: u32, q37: u32, q38: u32, q39: u32, q40: u32, q41: u32, q42: u32, q43: u32, q44: u32, q45: u32, q46: u32, q47: u32, q48: u32, q49: u32, q50: u32, q51: u32, q52: u32, q53: u32, q54: u32, q55: u32, q56: u32, q57: u32, q58: u32, q59: u32, q60: u32, q61: u32, q62: u32, q63: u32
}
struct Accumulator {
    v0: u32, v1: u32, v2: u32, v3: u32, v4: u32, v5: u32, v6: u32, v7: u32, v8: u32, v9: u32, v10: u32, v11: u32, v12: u32, v13: u32, v14: u32, v15: u32, v16: u32, v17: u32, v18: u32, v19: u32, v20: u32, v21: u32, v22: u32, v23: u32, v24: u32, v25: u32, v26: u32, v27: u32, v28: u32, v29: u32, v30: u32, v31: u32, v32: u32, v33: u32, v34: u32, v35: u32, v36: u32, v37: u32, v38: u32, v39: u32, v40: u32, v41: u32, v42: u32, v43: u32, v44: u32, v45: u32, v46: u32, v47: u32, v48: u32, v49: u32, v50: u32, v51: u32, v52: u32, v53: u32, v54: u32, v55: u32, v56: u32, v57: u32, v58: u32, v59: u32, v60: u32, v61: u32, v62: u32, v63: u32, k0: f32, k1: f32, s0: f32, s1: f32
}

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
    var qr: QueryRegs = QueryRegs {
        0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32
    }
    ;
    var acc: Accumulator = Accumulator {
        0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0.0 as f32, 0.0 as f32, 0.0 as f32, 0.0 as f32
    }
    ;
    let ncols1: i32 = (4 as i32);
    let np: i32 = (2 as i32);
    // ncols2=8 fixed; nbatch_fa=32; DKQ/2=nbatch_K2=nbatch_V2=nbatch_combine=128; tile_stride=132.
    let ncols = ncols1 * 8;
    // KQ_max init -FLT_MAX/2 (L1272) with exact f32 bits: 2^127-2^103 = (2-2^-23)*2^126, computed
    // from exact powers of two (no bitcast primitive exists or is assumed).
    let fltmax2: f32 = 170141173319264429905852091742258332672.0;
    acc.k0 = 0.0 - fltmax2;
    acc.k1 = 0.0 - fltmax2;
    acc.s0 = 0.0;
    acc.s1 = 0.0;
    var zi: i32 = 0;

    if true {
        let zi: i32 = (0 as i32);
        acc.v0 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (1 as i32);
        acc.v1 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (2 as i32);
        acc.v2 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (3 as i32);
        acc.v3 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (4 as i32);
        acc.v4 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (5 as i32);
        acc.v5 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (6 as i32);
        acc.v6 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (7 as i32);
        acc.v7 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (8 as i32);
        acc.v8 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (9 as i32);
        acc.v9 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (10 as i32);
        acc.v10 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (11 as i32);
        acc.v11 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (12 as i32);
        acc.v12 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (13 as i32);
        acc.v13 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (14 as i32);
        acc.v14 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (15 as i32);
        acc.v15 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (16 as i32);
        acc.v16 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (17 as i32);
        acc.v17 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (18 as i32);
        acc.v18 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (19 as i32);
        acc.v19 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (20 as i32);
        acc.v20 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (21 as i32);
        acc.v21 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (22 as i32);
        acc.v22 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (23 as i32);
        acc.v23 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (24 as i32);
        acc.v24 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (25 as i32);
        acc.v25 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (26 as i32);
        acc.v26 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (27 as i32);
        acc.v27 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (28 as i32);
        acc.v28 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (29 as i32);
        acc.v29 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (30 as i32);
        acc.v30 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (31 as i32);
        acc.v31 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (32 as i32);
        acc.v32 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (33 as i32);
        acc.v33 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (34 as i32);
        acc.v34 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (35 as i32);
        acc.v35 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (36 as i32);
        acc.v36 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (37 as i32);
        acc.v37 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (38 as i32);
        acc.v38 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (39 as i32);
        acc.v39 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (40 as i32);
        acc.v40 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (41 as i32);
        acc.v41 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (42 as i32);
        acc.v42 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (43 as i32);
        acc.v43 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (44 as i32);
        acc.v44 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (45 as i32);
        acc.v45 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (46 as i32);
        acc.v46 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (47 as i32);
        acc.v47 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (48 as i32);
        acc.v48 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (49 as i32);
        acc.v49 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (50 as i32);
        acc.v50 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (51 as i32);
        acc.v51 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (52 as i32);
        acc.v52 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (53 as i32);
        acc.v53 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (54 as i32);
        acc.v54 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (55 as i32);
        acc.v55 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (56 as i32);
        acc.v56 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (57 as i32);
        acc.v57 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (58 as i32);
        acc.v58 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (59 as i32);
        acc.v59 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (60 as i32);
        acc.v60 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (61 as i32);
        acc.v61 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (62 as i32);
        acc.v62 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (63 as i32);
        acc.v63 = h2pack(0.0, 0.0);
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
        qr.q0 = r.a;
        qr.q1 = r.b;
        qr.q2 = r.c;
        qr.q3 = r.d;
    }

    if true {
        let q0: i32 = (8 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q4 = r.a;
        qr.q5 = r.b;
        qr.q6 = r.c;
        qr.q7 = r.d;
    }

    if true {
        let q0: i32 = (16 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q8 = r.a;
        qr.q9 = r.b;
        qr.q10 = r.c;
        qr.q11 = r.d;
    }

    if true {
        let q0: i32 = (24 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q12 = r.a;
        qr.q13 = r.b;
        qr.q14 = r.c;
        qr.q15 = r.d;
    }

    if true {
        let q0: i32 = (32 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q16 = r.a;
        qr.q17 = r.b;
        qr.q18 = r.c;
        qr.q19 = r.d;
    }

    if true {
        let q0: i32 = (40 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q20 = r.a;
        qr.q21 = r.b;
        qr.q22 = r.c;
        qr.q23 = r.d;
    }

    if true {
        let q0: i32 = (48 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q24 = r.a;
        qr.q25 = r.b;
        qr.q26 = r.c;
        qr.q27 = r.d;
    }

    if true {
        let q0: i32 = (56 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q28 = r.a;
        qr.q29 = r.b;
        qr.q30 = r.c;
        qr.q31 = r.d;
    }

    if true {
        let q0: i32 = (64 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q32 = r.a;
        qr.q33 = r.b;
        qr.q34 = r.c;
        qr.q35 = r.d;
    }

    if true {
        let q0: i32 = (72 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q36 = r.a;
        qr.q37 = r.b;
        qr.q38 = r.c;
        qr.q39 = r.d;
    }

    if true {
        let q0: i32 = (80 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q40 = r.a;
        qr.q41 = r.b;
        qr.q42 = r.c;
        qr.q43 = r.d;
    }

    if true {
        let q0: i32 = (88 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q44 = r.a;
        qr.q45 = r.b;
        qr.q46 = r.c;
        qr.q47 = r.d;
    }

    if true {
        let q0: i32 = (96 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q48 = r.a;
        qr.q49 = r.b;
        qr.q50 = r.c;
        qr.q51 = r.d;
    }

    if true {
        let q0: i32 = (104 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q52 = r.a;
        qr.q53 = r.b;
        qr.q54 = r.c;
        qr.q55 = r.d;
    }

    if true {
        let q0: i32 = (112 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q56 = r.a;
        qr.q57 = r.b;
        qr.q58 = r.c;
        qr.q59 = r.d;
    }

    if true {
        let q0: i32 = (120 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q60 = r.a;
        qr.q61 = r.b;
        qr.q62 = r.c;
        qr.q63 = r.d;
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
        if true {
            let n_queries: i64 = ne01z;
            let last: bool = false;
            let ncols1: i32 = 4;
            let np: i32 = 2;
            let kt = smem;
            let vt = smem + (16384 as u64);
            let mt = smem + (32768 as u64);
            let kv_row0 = (kb0 as i64) * 32;
            // k_VKQ_0 = kb0*nbatch_fa
            let query0 = (jt as i64) * (ncols1 as i64);
            // j0 for the mask loader
            let cb = (ty / np) * 16;
            // warp Q-column block base (cols_per_warp=16)
            let rh = (ty % np) * 16;
            // warp KV-row half base (np*T_A_KQ::I split)
            let nt = 32 / (np * 16);
            // KQ_C/B tiles per warp: 1 (np=2) or 2 (np=1)
            let kqoff = (3.0 as f32) * (0.6931 as f32);
            // FATTN_KQ_MAX_OFFSET
            var t: i32 = 0;
            // nstages>1 entry: this iteration's K tile + mask were preloaded by the prologue or the previous
            // iter tail; wait for them, then issue the V tile of this iteration into the pipeline.
            cpwait();
            barrier();
            load_kv(v_h2, vt, stride_v, kv_row0, tx, ty);
            // ---- KQ = Q_B @ K_A over the single nbatch_K2 block k in [0,128) half2 (16 x T_A_KQ::J=8) ----
            var kqc: f32[16] = default < f32[16] > ();
            // up to 2 fresh zero tile<16,16,float> C tiles
            var q: i32 = 0;
            // Q_B tile index = k_KQ_0/8

            if true {
                let q: i32 = 0;
                let q0__u1 = qr.q0;
                let q1__u1 = qr.q1;
                let q2__u1 = qr.q2;
                let q3__u1 = qr.q3;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u2 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u2 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u1, q1__u1, q2__u1, q3__u1, ka__u2.a, ka__u2.c);
                    kqc[ (0 as i32)] = lo__u2.x;
                    kqc[ (1 as i32)] = lo__u2.y;
                    kqc[ (2 as i32)] = lo__u2.z;
                    kqc[ (3 as i32)] = lo__u2.w;
                    let hi__u2 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u1, q1__u1, q2__u1, q3__u1, ka__u2.b, ka__u2.d);
                    kqc[ (4 as i32)] = hi__u2.x;
                    kqc[ (5 as i32)] = hi__u2.y;
                    kqc[ (6 as i32)] = hi__u2.z;
                    kqc[ (7 as i32)] = hi__u2.w;
                }
                t = (1 as i32);
            }

            if true {
                let q: i32 = 1;
                let q0__u3 = qr.q4;
                let q1__u3 = qr.q5;
                let q2__u3 = qr.q6;
                let q3__u3 = qr.q7;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u4 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u4 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u3, q1__u3, q2__u3, q3__u3, ka__u4.a, ka__u4.c);
                    kqc[ (0 as i32)] = lo__u4.x;
                    kqc[ (1 as i32)] = lo__u4.y;
                    kqc[ (2 as i32)] = lo__u4.z;
                    kqc[ (3 as i32)] = lo__u4.w;
                    let hi__u4 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u3, q1__u3, q2__u3, q3__u3, ka__u4.b, ka__u4.d);
                    kqc[ (4 as i32)] = hi__u4.x;
                    kqc[ (5 as i32)] = hi__u4.y;
                    kqc[ (6 as i32)] = hi__u4.z;
                    kqc[ (7 as i32)] = hi__u4.w;
                }
                t = (1 as i32);
            }

            if true {
                let q: i32 = 2;
                let q0__u5 = qr.q8;
                let q1__u5 = qr.q9;
                let q2__u5 = qr.q10;
                let q3__u5 = qr.q11;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u6 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u6 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u5, q1__u5, q2__u5, q3__u5, ka__u6.a, ka__u6.c);
                    kqc[ (0 as i32)] = lo__u6.x;
                    kqc[ (1 as i32)] = lo__u6.y;
                    kqc[ (2 as i32)] = lo__u6.z;
                    kqc[ (3 as i32)] = lo__u6.w;
                    let hi__u6 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u5, q1__u5, q2__u5, q3__u5, ka__u6.b, ka__u6.d);
                    kqc[ (4 as i32)] = hi__u6.x;
                    kqc[ (5 as i32)] = hi__u6.y;
                    kqc[ (6 as i32)] = hi__u6.z;
                    kqc[ (7 as i32)] = hi__u6.w;
                }
                t = (1 as i32);
            }

            if true {
                let q: i32 = 3;
                let q0__u7 = qr.q12;
                let q1__u7 = qr.q13;
                let q2__u7 = qr.q14;
                let q3__u7 = qr.q15;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u8 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u8 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u7, q1__u7, q2__u7, q3__u7, ka__u8.a, ka__u8.c);
                    kqc[ (0 as i32)] = lo__u8.x;
                    kqc[ (1 as i32)] = lo__u8.y;
                    kqc[ (2 as i32)] = lo__u8.z;
                    kqc[ (3 as i32)] = lo__u8.w;
                    let hi__u8 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u7, q1__u7, q2__u7, q3__u7, ka__u8.b, ka__u8.d);
                    kqc[ (4 as i32)] = hi__u8.x;
                    kqc[ (5 as i32)] = hi__u8.y;
                    kqc[ (6 as i32)] = hi__u8.z;
                    kqc[ (7 as i32)] = hi__u8.w;
                }
                t = (1 as i32);
            }

            if true {
                let q: i32 = 4;
                let q0__u9 = qr.q16;
                let q1__u9 = qr.q17;
                let q2__u9 = qr.q18;
                let q3__u9 = qr.q19;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u10 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u10 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u9, q1__u9, q2__u9, q3__u9, ka__u10.a, ka__u10.c);
                    kqc[ (0 as i32)] = lo__u10.x;
                    kqc[ (1 as i32)] = lo__u10.y;
                    kqc[ (2 as i32)] = lo__u10.z;
                    kqc[ (3 as i32)] = lo__u10.w;
                    let hi__u10 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u9, q1__u9, q2__u9, q3__u9, ka__u10.b, ka__u10.d);
                    kqc[ (4 as i32)] = hi__u10.x;
                    kqc[ (5 as i32)] = hi__u10.y;
                    kqc[ (6 as i32)] = hi__u10.z;
                    kqc[ (7 as i32)] = hi__u10.w;
                }
                t = (1 as i32);
            }

            if true {
                let q: i32 = 5;
                let q0__u11 = qr.q20;
                let q1__u11 = qr.q21;
                let q2__u11 = qr.q22;
                let q3__u11 = qr.q23;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u12 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u12 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u11, q1__u11, q2__u11, q3__u11, ka__u12.a, ka__u12.c);
                    kqc[ (0 as i32)] = lo__u12.x;
                    kqc[ (1 as i32)] = lo__u12.y;
                    kqc[ (2 as i32)] = lo__u12.z;
                    kqc[ (3 as i32)] = lo__u12.w;
                    let hi__u12 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u11, q1__u11, q2__u11, q3__u11, ka__u12.b, ka__u12.d);
                    kqc[ (4 as i32)] = hi__u12.x;
                    kqc[ (5 as i32)] = hi__u12.y;
                    kqc[ (6 as i32)] = hi__u12.z;
                    kqc[ (7 as i32)] = hi__u12.w;
                }
                t = (1 as i32);
            }

            if true {
                let q: i32 = 6;
                let q0__u13 = qr.q24;
                let q1__u13 = qr.q25;
                let q2__u13 = qr.q26;
                let q3__u13 = qr.q27;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u14 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u14 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u13, q1__u13, q2__u13, q3__u13, ka__u14.a, ka__u14.c);
                    kqc[ (0 as i32)] = lo__u14.x;
                    kqc[ (1 as i32)] = lo__u14.y;
                    kqc[ (2 as i32)] = lo__u14.z;
                    kqc[ (3 as i32)] = lo__u14.w;
                    let hi__u14 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u13, q1__u13, q2__u13, q3__u13, ka__u14.b, ka__u14.d);
                    kqc[ (4 as i32)] = hi__u14.x;
                    kqc[ (5 as i32)] = hi__u14.y;
                    kqc[ (6 as i32)] = hi__u14.z;
                    kqc[ (7 as i32)] = hi__u14.w;
                }
                t = (1 as i32);
            }

            if true {
                let q: i32 = 7;
                let q0__u15 = qr.q28;
                let q1__u15 = qr.q29;
                let q2__u15 = qr.q30;
                let q3__u15 = qr.q31;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u16 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u16 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u15, q1__u15, q2__u15, q3__u15, ka__u16.a, ka__u16.c);
                    kqc[ (0 as i32)] = lo__u16.x;
                    kqc[ (1 as i32)] = lo__u16.y;
                    kqc[ (2 as i32)] = lo__u16.z;
                    kqc[ (3 as i32)] = lo__u16.w;
                    let hi__u16 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u15, q1__u15, q2__u15, q3__u15, ka__u16.b, ka__u16.d);
                    kqc[ (4 as i32)] = hi__u16.x;
                    kqc[ (5 as i32)] = hi__u16.y;
                    kqc[ (6 as i32)] = hi__u16.z;
                    kqc[ (7 as i32)] = hi__u16.w;
                }
                t = (1 as i32);
            }

            if true {
                let q: i32 = 8;
                let q0__u17 = qr.q32;
                let q1__u17 = qr.q33;
                let q2__u17 = qr.q34;
                let q3__u17 = qr.q35;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u18 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u18 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u17, q1__u17, q2__u17, q3__u17, ka__u18.a, ka__u18.c);
                    kqc[ (0 as i32)] = lo__u18.x;
                    kqc[ (1 as i32)] = lo__u18.y;
                    kqc[ (2 as i32)] = lo__u18.z;
                    kqc[ (3 as i32)] = lo__u18.w;
                    let hi__u18 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u17, q1__u17, q2__u17, q3__u17, ka__u18.b, ka__u18.d);
                    kqc[ (4 as i32)] = hi__u18.x;
                    kqc[ (5 as i32)] = hi__u18.y;
                    kqc[ (6 as i32)] = hi__u18.z;
                    kqc[ (7 as i32)] = hi__u18.w;
                }
                t = (1 as i32);
            }

            if true {
                let q: i32 = 9;
                let q0__u19 = qr.q36;
                let q1__u19 = qr.q37;
                let q2__u19 = qr.q38;
                let q3__u19 = qr.q39;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u20 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u20 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u19, q1__u19, q2__u19, q3__u19, ka__u20.a, ka__u20.c);
                    kqc[ (0 as i32)] = lo__u20.x;
                    kqc[ (1 as i32)] = lo__u20.y;
                    kqc[ (2 as i32)] = lo__u20.z;
                    kqc[ (3 as i32)] = lo__u20.w;
                    let hi__u20 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u19, q1__u19, q2__u19, q3__u19, ka__u20.b, ka__u20.d);
                    kqc[ (4 as i32)] = hi__u20.x;
                    kqc[ (5 as i32)] = hi__u20.y;
                    kqc[ (6 as i32)] = hi__u20.z;
                    kqc[ (7 as i32)] = hi__u20.w;
                }
                t = (1 as i32);
            }

            if true {
                let q: i32 = 10;
                let q0__u21 = qr.q40;
                let q1__u21 = qr.q41;
                let q2__u21 = qr.q42;
                let q3__u21 = qr.q43;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u22 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u22 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u21, q1__u21, q2__u21, q3__u21, ka__u22.a, ka__u22.c);
                    kqc[ (0 as i32)] = lo__u22.x;
                    kqc[ (1 as i32)] = lo__u22.y;
                    kqc[ (2 as i32)] = lo__u22.z;
                    kqc[ (3 as i32)] = lo__u22.w;
                    let hi__u22 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u21, q1__u21, q2__u21, q3__u21, ka__u22.b, ka__u22.d);
                    kqc[ (4 as i32)] = hi__u22.x;
                    kqc[ (5 as i32)] = hi__u22.y;
                    kqc[ (6 as i32)] = hi__u22.z;
                    kqc[ (7 as i32)] = hi__u22.w;
                }
                t = (1 as i32);
            }

            if true {
                let q: i32 = 11;
                let q0__u23 = qr.q44;
                let q1__u23 = qr.q45;
                let q2__u23 = qr.q46;
                let q3__u23 = qr.q47;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u24 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u24 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u23, q1__u23, q2__u23, q3__u23, ka__u24.a, ka__u24.c);
                    kqc[ (0 as i32)] = lo__u24.x;
                    kqc[ (1 as i32)] = lo__u24.y;
                    kqc[ (2 as i32)] = lo__u24.z;
                    kqc[ (3 as i32)] = lo__u24.w;
                    let hi__u24 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u23, q1__u23, q2__u23, q3__u23, ka__u24.b, ka__u24.d);
                    kqc[ (4 as i32)] = hi__u24.x;
                    kqc[ (5 as i32)] = hi__u24.y;
                    kqc[ (6 as i32)] = hi__u24.z;
                    kqc[ (7 as i32)] = hi__u24.w;
                }
                t = (1 as i32);
            }

            if true {
                let q: i32 = 12;
                let q0__u25 = qr.q48;
                let q1__u25 = qr.q49;
                let q2__u25 = qr.q50;
                let q3__u25 = qr.q51;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u26 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u26 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u25, q1__u25, q2__u25, q3__u25, ka__u26.a, ka__u26.c);
                    kqc[ (0 as i32)] = lo__u26.x;
                    kqc[ (1 as i32)] = lo__u26.y;
                    kqc[ (2 as i32)] = lo__u26.z;
                    kqc[ (3 as i32)] = lo__u26.w;
                    let hi__u26 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u25, q1__u25, q2__u25, q3__u25, ka__u26.b, ka__u26.d);
                    kqc[ (4 as i32)] = hi__u26.x;
                    kqc[ (5 as i32)] = hi__u26.y;
                    kqc[ (6 as i32)] = hi__u26.z;
                    kqc[ (7 as i32)] = hi__u26.w;
                }
                t = (1 as i32);
            }

            if true {
                let q: i32 = 13;
                let q0__u27 = qr.q52;
                let q1__u27 = qr.q53;
                let q2__u27 = qr.q54;
                let q3__u27 = qr.q55;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u28 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u28 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u27, q1__u27, q2__u27, q3__u27, ka__u28.a, ka__u28.c);
                    kqc[ (0 as i32)] = lo__u28.x;
                    kqc[ (1 as i32)] = lo__u28.y;
                    kqc[ (2 as i32)] = lo__u28.z;
                    kqc[ (3 as i32)] = lo__u28.w;
                    let hi__u28 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u27, q1__u27, q2__u27, q3__u27, ka__u28.b, ka__u28.d);
                    kqc[ (4 as i32)] = hi__u28.x;
                    kqc[ (5 as i32)] = hi__u28.y;
                    kqc[ (6 as i32)] = hi__u28.z;
                    kqc[ (7 as i32)] = hi__u28.w;
                }
                t = (1 as i32);
            }

            if true {
                let q: i32 = 14;
                let q0__u29 = qr.q56;
                let q1__u29 = qr.q57;
                let q2__u29 = qr.q58;
                let q3__u29 = qr.q59;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u30 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u30 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u29, q1__u29, q2__u29, q3__u29, ka__u30.a, ka__u30.c);
                    kqc[ (0 as i32)] = lo__u30.x;
                    kqc[ (1 as i32)] = lo__u30.y;
                    kqc[ (2 as i32)] = lo__u30.z;
                    kqc[ (3 as i32)] = lo__u30.w;
                    let hi__u30 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u29, q1__u29, q2__u29, q3__u29, ka__u30.b, ka__u30.d);
                    kqc[ (4 as i32)] = hi__u30.x;
                    kqc[ (5 as i32)] = hi__u30.y;
                    kqc[ (6 as i32)] = hi__u30.z;
                    kqc[ (7 as i32)] = hi__u30.w;
                }
                t = (1 as i32);
            }

            if true {
                let q: i32 = 15;
                let q0__u31 = qr.q60;
                let q1__u31 = qr.q61;
                let q2__u31 = qr.q62;
                let q3__u31 = qr.q63;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u32 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u32 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u31, q1__u31, q2__u31, q3__u31, ka__u32.a, ka__u32.c);
                    kqc[ (0 as i32)] = lo__u32.x;
                    kqc[ (1 as i32)] = lo__u32.y;
                    kqc[ (2 as i32)] = lo__u32.z;
                    kqc[ (3 as i32)] = lo__u32.w;
                    let hi__u32 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u31, q1__u31, q2__u31, q3__u31, ka__u32.b, ka__u32.d);
                    kqc[ (4 as i32)] = hi__u32.x;
                    kqc[ (5 as i32)] = hi__u32.y;
                    kqc[ (6 as i32)] = hi__u32.z;
                    kqc[ (7 as i32)] = hi__u32.w;
                }
                t = (1 as i32);
            }
            q = (16 as i32);
            // ---- mask add + running max: half2 mask word at row (cb+get_i)/8, half2 col (rb+get_j)/2 ----
            var kqold: f32[2] = default < f32[2] > ();
            // KQ_max[cols_per_thread=2]
            var kqmn: f32[2] = default < f32[2] > ();
            // KQ_max_new
            var rsa: f32[2] = default < f32[2] > ();
            // KQ_rowsum_add
            kqold[ (0 as i32)] = frombits(bits(acc.k0));
            kqold[ (1 as i32)] = frombits(bits(acc.k1));
            kqmn[ (0 as i32)] = kqold[ (0 as i32)];
            kqmn[ (1 as i32)] = kqold[ (1 as i32)];
            t = 0;

            if true {
                let t: i32 = 0;
                let rb__u33 = t * np * 16 + rh;
                var l0__u33: i32 = 0;

                if true {
                    let l0__u33: i32 = 0;
                    // half2 pairs, l0 even: (j,i),(j,i+1)
                    let mh2__u34 = ( (cb + gi_c(l0__u33, tx)) / 8) * 20 + ( (rb__u33 + gj_c(l0__u33, tx)) / 2);
                    let w__u34 = ldu(mt + ( (mh2__u34 as u64) * (4 as u64)));
                    kqc[ (0 as i32)] = fmaf(slope, hlo(w__u34), kqc[ (0 as i32)]);
                    kqc[ (1 as i32)] = fmaf(slope, hhi(w__u34), kqc[ (1 as i32)]);
                }

                if true {
                    let l0__u33: i32 = 2;
                    // half2 pairs, l0 even: (j,i),(j,i+1)
                    let mh2__u35 = ( (cb + gi_c(l0__u33, tx)) / 8) * 20 + ( (rb__u33 + gj_c(l0__u33, tx)) / 2);
                    let w__u35 = ldu(mt + ( (mh2__u35 as u64) * (4 as u64)));
                    kqc[ (2 as i32)] = fmaf(slope, hlo(w__u35), kqc[ (2 as i32)]);
                    kqc[ (3 as i32)] = fmaf(slope, hhi(w__u35), kqc[ (3 as i32)]);
                }

                if true {
                    let l0__u33: i32 = 4;
                    // half2 pairs, l0 even: (j,i),(j,i+1)
                    let mh2__u36 = ( (cb + gi_c(l0__u33, tx)) / 8) * 20 + ( (rb__u33 + gj_c(l0__u33, tx)) / 2);
                    let w__u36 = ldu(mt + ( (mh2__u36 as u64) * (4 as u64)));
                    kqc[ (4 as i32)] = fmaf(slope, hlo(w__u36), kqc[ (4 as i32)]);
                    kqc[ (5 as i32)] = fmaf(slope, hhi(w__u36), kqc[ (5 as i32)]);
                }

                if true {
                    let l0__u33: i32 = 6;
                    // half2 pairs, l0 even: (j,i),(j,i+1)
                    let mh2__u37 = ( (cb + gi_c(l0__u33, tx)) / 8) * 20 + ( (rb__u33 + gj_c(l0__u33, tx)) / 2);
                    let w__u37 = ldu(mt + ( (mh2__u37 as u64) * (4 as u64)));
                    kqc[ (6 as i32)] = fmaf(slope, hlo(w__u37), kqc[ (6 as i32)]);
                    kqc[ (7 as i32)] = fmaf(slope, hhi(w__u37), kqc[ (7 as i32)]);
                }
                l0__u33 = (8 as i32);
                var l__u33: i32 = 0;

                if true {
                    let l__u33: i32 = 0;
                    let idx__u38 = (l__u33 / 2) % 2;
                    let mv__u38 = kqc[ (0 as i32)] + kqoff;

                    if mv__u38 > kqmn[ (0 as i32)] {
                        kqmn[ (0 as i32)] = mv__u38;
                    }
                }

                if true {
                    let l__u33: i32 = 1;
                    let idx__u39 = (l__u33 / 2) % 2;
                    let mv__u39 = kqc[ (1 as i32)] + kqoff;

                    if mv__u39 > kqmn[ (0 as i32)] {
                        kqmn[ (0 as i32)] = mv__u39;
                    }
                }

                if true {
                    let l__u33: i32 = 2;
                    let idx__u40 = (l__u33 / 2) % 2;
                    let mv__u40 = kqc[ (2 as i32)] + kqoff;

                    if mv__u40 > kqmn[ (1 as i32)] {
                        kqmn[ (1 as i32)] = mv__u40;
                    }
                }

                if true {
                    let l__u33: i32 = 3;
                    let idx__u41 = (l__u33 / 2) % 2;
                    let mv__u41 = kqc[ (3 as i32)] + kqoff;

                    if mv__u41 > kqmn[ (1 as i32)] {
                        kqmn[ (1 as i32)] = mv__u41;
                    }
                }

                if true {
                    let l__u33: i32 = 4;
                    let idx__u42 = (l__u33 / 2) % 2;
                    let mv__u42 = kqc[ (4 as i32)] + kqoff;

                    if mv__u42 > kqmn[ (0 as i32)] {
                        kqmn[ (0 as i32)] = mv__u42;
                    }
                }

                if true {
                    let l__u33: i32 = 5;
                    let idx__u43 = (l__u33 / 2) % 2;
                    let mv__u43 = kqc[ (5 as i32)] + kqoff;

                    if mv__u43 > kqmn[ (0 as i32)] {
                        kqmn[ (0 as i32)] = mv__u43;
                    }
                }

                if true {
                    let l__u33: i32 = 6;
                    let idx__u44 = (l__u33 / 2) % 2;
                    let mv__u44 = kqc[ (6 as i32)] + kqoff;

                    if mv__u44 > kqmn[ (1 as i32)] {
                        kqmn[ (1 as i32)] = mv__u44;
                    }
                }

                if true {
                    let l__u33: i32 = 7;
                    let idx__u45 = (l__u33 / 2) % 2;
                    let mv__u45 = kqc[ (7 as i32)] + kqoff;

                    if mv__u45 > kqmn[ (1 as i32)] {
                        kqmn[ (1 as i32)] = mv__u45;
                    }
                }
                l__u33 = (8 as i32);
            }
            t = (1 as i32);
            // Values per KQ column spread across 4 threads: __shfl_xor offsets 2 then 1 (TURING wide).
            var col: i32 = 0;

            if true {
                let col: i32 = 0;
                var s__u46 = shflx(kqmn[ (0 as i32)], 2);

                if s__u46 > kqmn[ (0 as i32)] {
                    kqmn[ (0 as i32)] = s__u46;
                }
                s__u46 = shflx(kqmn[ (0 as i32)], 1);

                if s__u46 > kqmn[ (0 as i32)] {
                    kqmn[ (0 as i32)] = s__u46;
                }
            }

            if true {
                let col: i32 = 1;
                var s__u47 = shflx(kqmn[ (1 as i32)], 2);

                if s__u47 > kqmn[ (1 as i32)] {
                    kqmn[ (1 as i32)] = s__u47;
                }
                s__u47 = shflx(kqmn[ (1 as i32)], 1);

                if s__u47 > kqmn[ (1 as i32)] {
                    kqmn[ (1 as i32)] = s__u47;
                }
            }
            col = (2 as i32);
            // ---- softmax with current max: expf(KQ - KQ_max_new); divisor stays in KQ_rowsum ----
            t = 0;

            if true {
                let t: i32 = 0;
                var l__u48: i32 = 0;

                if true {
                    let l__u48: i32 = 0;
                    let idx__u49 = (l__u48 / 2) % 2;
                    let e__u49 = expf(kqc[ (0 as i32)] - kqmn[ (0 as i32)]);
                    kqc[ (0 as i32)] = e__u49;
                    rsa[ (0 as i32)] += e__u49;
                }

                if true {
                    let l__u48: i32 = 1;
                    let idx__u50 = (l__u48 / 2) % 2;
                    let e__u50 = expf(kqc[ (1 as i32)] - kqmn[ (0 as i32)]);
                    kqc[ (1 as i32)] = e__u50;
                    rsa[ (0 as i32)] += e__u50;
                }

                if true {
                    let l__u48: i32 = 2;
                    let idx__u51 = (l__u48 / 2) % 2;
                    let e__u51 = expf(kqc[ (2 as i32)] - kqmn[ (1 as i32)]);
                    kqc[ (2 as i32)] = e__u51;
                    rsa[ (1 as i32)] += e__u51;
                }

                if true {
                    let l__u48: i32 = 3;
                    let idx__u52 = (l__u48 / 2) % 2;
                    let e__u52 = expf(kqc[ (3 as i32)] - kqmn[ (1 as i32)]);
                    kqc[ (3 as i32)] = e__u52;
                    rsa[ (1 as i32)] += e__u52;
                }

                if true {
                    let l__u48: i32 = 4;
                    let idx__u53 = (l__u48 / 2) % 2;
                    let e__u53 = expf(kqc[ (4 as i32)] - kqmn[ (0 as i32)]);
                    kqc[ (4 as i32)] = e__u53;
                    rsa[ (0 as i32)] += e__u53;
                }

                if true {
                    let l__u48: i32 = 5;
                    let idx__u54 = (l__u48 / 2) % 2;
                    let e__u54 = expf(kqc[ (5 as i32)] - kqmn[ (0 as i32)]);
                    kqc[ (5 as i32)] = e__u54;
                    rsa[ (0 as i32)] += e__u54;
                }

                if true {
                    let l__u48: i32 = 6;
                    let idx__u55 = (l__u48 / 2) % 2;
                    let e__u55 = expf(kqc[ (6 as i32)] - kqmn[ (1 as i32)]);
                    kqc[ (6 as i32)] = e__u55;
                    rsa[ (1 as i32)] += e__u55;
                }

                if true {
                    let l__u48: i32 = 7;
                    let idx__u56 = (l__u48 / 2) % 2;
                    let e__u56 = expf(kqc[ (7 as i32)] - kqmn[ (1 as i32)]);
                    kqc[ (7 as i32)] = e__u56;
                    rsa[ (1 as i32)] += e__u56;
                }
                l__u48 = (8 as i32);
            }
            t = (1 as i32);
            // ---- rescale: KQ_max_scale=expf(KQ_max-KQ_max_new); FTZ bit-multiply for diff < -20;
            //      exact fused step KQ_rowsum = scale*KQ_rowsum + KQ_rowsum_add ----
            var scl: f32[2] = default < f32[2] > ();
            col = 0;

            if true {
                let col: i32 = 0;
                let diff__u57 = kqold[ (0 as i32)] - kqmn[ (0 as i32)];
                let s0__u57 = expf(diff__u57);
                let keep__u57: u32 = if diff__u57 >= -20.0 {
                    1 as u32
                } else {
                    0 as u32
                }
                ;
                scl[ (0 as i32)] = frombits(bits(s0__u57) * keep__u57);
                // *((uint32_t*)&scale) *= diff>=SOFTMAX_FTZ_THRESHOLD
                acc.k0 = frombits(bits(kqmn[ (0 as i32)]));
                let rs__u57 = frombits(bits(acc.s0));
                acc.s0 = frombits(bits(fmaf(scl[ (0 as i32)], rs__u57, rsa[ (0 as i32)])));
            }

            if true {
                let col: i32 = 1;
                let diff__u58 = kqold[ (1 as i32)] - kqmn[ (1 as i32)];
                let s0__u58 = expf(diff__u58);
                let keep__u58: u32 = if diff__u58 >= -20.0 {
                    1 as u32
                } else {
                    0 as u32
                }
                ;
                scl[ (1 as i32)] = frombits(bits(s0__u58) * keep__u58);
                // *((uint32_t*)&scale) *= diff>=SOFTMAX_FTZ_THRESHOLD
                acc.k1 = frombits(bits(kqmn[ (1 as i32)]));
                let rs__u58 = frombits(bits(acc.s1));
                acc.s1 = frombits(bits(fmaf(scl[ (1 as i32)], rs__u58, rsa[ (1 as i32)])));
            }
            col = (2 as i32);
            // ---- VKQ f16 accumulator rescale: VKQ_C[i].x[l0+col] *= make_half2(scale,scale) ----
            var sh2: u32[2] = default < u32[2] > ();
            col = 0;

            if true {
                let col: i32 = 0;
                sh2[ (0 as i32)] = h2(scl[ (0 as i32)], scl[ (0 as i32)]);
            }

            if true {
                let col: i32 = 1;
                sh2[ (1 as i32)] = h2(scl[ (1 as i32)], scl[ (1 as i32)]);
            }
            col = (2 as i32);
            var v: i32 = 0;

            if true {
                let v: i32 = 0;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u62: i32 = 0;

                    if true {
                        let l0__u62: i32 = 0;
                        let e__u63 = v * 4 + l0__u62 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v0 = mulh2(acc.v0, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u62: i32 = 1;
                        let e__u64 = v * 4 + l0__u62 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v2 = mulh2(acc.v2, sh2[ (0 as i32)]);
                    }
                    l0__u62 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u65: i32 = 0;

                    if true {
                        let l0__u65: i32 = 0;
                        let e__u66 = v * 4 + l0__u65 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v1 = mulh2(acc.v1, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u65: i32 = 1;
                        let e__u67 = v * 4 + l0__u65 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v3 = mulh2(acc.v3, sh2[ (1 as i32)]);
                    }
                    l0__u65 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 1;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u69: i32 = 0;

                    if true {
                        let l0__u69: i32 = 0;
                        let e__u70 = v * 4 + l0__u69 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v4 = mulh2(acc.v4, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u69: i32 = 1;
                        let e__u71 = v * 4 + l0__u69 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v6 = mulh2(acc.v6, sh2[ (0 as i32)]);
                    }
                    l0__u69 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u72: i32 = 0;

                    if true {
                        let l0__u72: i32 = 0;
                        let e__u73 = v * 4 + l0__u72 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v5 = mulh2(acc.v5, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u72: i32 = 1;
                        let e__u74 = v * 4 + l0__u72 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v7 = mulh2(acc.v7, sh2[ (1 as i32)]);
                    }
                    l0__u72 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 2;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u76: i32 = 0;

                    if true {
                        let l0__u76: i32 = 0;
                        let e__u77 = v * 4 + l0__u76 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v8 = mulh2(acc.v8, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u76: i32 = 1;
                        let e__u78 = v * 4 + l0__u76 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v10 = mulh2(acc.v10, sh2[ (0 as i32)]);
                    }
                    l0__u76 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u79: i32 = 0;

                    if true {
                        let l0__u79: i32 = 0;
                        let e__u80 = v * 4 + l0__u79 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v9 = mulh2(acc.v9, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u79: i32 = 1;
                        let e__u81 = v * 4 + l0__u79 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v11 = mulh2(acc.v11, sh2[ (1 as i32)]);
                    }
                    l0__u79 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 3;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u83: i32 = 0;

                    if true {
                        let l0__u83: i32 = 0;
                        let e__u84 = v * 4 + l0__u83 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v12 = mulh2(acc.v12, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u83: i32 = 1;
                        let e__u85 = v * 4 + l0__u83 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v14 = mulh2(acc.v14, sh2[ (0 as i32)]);
                    }
                    l0__u83 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u86: i32 = 0;

                    if true {
                        let l0__u86: i32 = 0;
                        let e__u87 = v * 4 + l0__u86 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v13 = mulh2(acc.v13, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u86: i32 = 1;
                        let e__u88 = v * 4 + l0__u86 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v15 = mulh2(acc.v15, sh2[ (1 as i32)]);
                    }
                    l0__u86 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 4;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u90: i32 = 0;

                    if true {
                        let l0__u90: i32 = 0;
                        let e__u91 = v * 4 + l0__u90 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v16 = mulh2(acc.v16, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u90: i32 = 1;
                        let e__u92 = v * 4 + l0__u90 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v18 = mulh2(acc.v18, sh2[ (0 as i32)]);
                    }
                    l0__u90 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u93: i32 = 0;

                    if true {
                        let l0__u93: i32 = 0;
                        let e__u94 = v * 4 + l0__u93 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v17 = mulh2(acc.v17, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u93: i32 = 1;
                        let e__u95 = v * 4 + l0__u93 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v19 = mulh2(acc.v19, sh2[ (1 as i32)]);
                    }
                    l0__u93 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 5;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u97: i32 = 0;

                    if true {
                        let l0__u97: i32 = 0;
                        let e__u98 = v * 4 + l0__u97 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v20 = mulh2(acc.v20, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u97: i32 = 1;
                        let e__u99 = v * 4 + l0__u97 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v22 = mulh2(acc.v22, sh2[ (0 as i32)]);
                    }
                    l0__u97 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u100: i32 = 0;

                    if true {
                        let l0__u100: i32 = 0;
                        let e__u101 = v * 4 + l0__u100 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v21 = mulh2(acc.v21, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u100: i32 = 1;
                        let e__u102 = v * 4 + l0__u100 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v23 = mulh2(acc.v23, sh2[ (1 as i32)]);
                    }
                    l0__u100 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 6;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u104: i32 = 0;

                    if true {
                        let l0__u104: i32 = 0;
                        let e__u105 = v * 4 + l0__u104 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v24 = mulh2(acc.v24, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u104: i32 = 1;
                        let e__u106 = v * 4 + l0__u104 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v26 = mulh2(acc.v26, sh2[ (0 as i32)]);
                    }
                    l0__u104 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u107: i32 = 0;

                    if true {
                        let l0__u107: i32 = 0;
                        let e__u108 = v * 4 + l0__u107 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v25 = mulh2(acc.v25, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u107: i32 = 1;
                        let e__u109 = v * 4 + l0__u107 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v27 = mulh2(acc.v27, sh2[ (1 as i32)]);
                    }
                    l0__u107 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 7;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u111: i32 = 0;

                    if true {
                        let l0__u111: i32 = 0;
                        let e__u112 = v * 4 + l0__u111 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v28 = mulh2(acc.v28, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u111: i32 = 1;
                        let e__u113 = v * 4 + l0__u111 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v30 = mulh2(acc.v30, sh2[ (0 as i32)]);
                    }
                    l0__u111 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u114: i32 = 0;

                    if true {
                        let l0__u114: i32 = 0;
                        let e__u115 = v * 4 + l0__u114 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v29 = mulh2(acc.v29, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u114: i32 = 1;
                        let e__u116 = v * 4 + l0__u114 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v31 = mulh2(acc.v31, sh2[ (1 as i32)]);
                    }
                    l0__u114 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 8;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u118: i32 = 0;

                    if true {
                        let l0__u118: i32 = 0;
                        let e__u119 = v * 4 + l0__u118 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v32 = mulh2(acc.v32, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u118: i32 = 1;
                        let e__u120 = v * 4 + l0__u118 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v34 = mulh2(acc.v34, sh2[ (0 as i32)]);
                    }
                    l0__u118 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u121: i32 = 0;

                    if true {
                        let l0__u121: i32 = 0;
                        let e__u122 = v * 4 + l0__u121 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v33 = mulh2(acc.v33, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u121: i32 = 1;
                        let e__u123 = v * 4 + l0__u121 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v35 = mulh2(acc.v35, sh2[ (1 as i32)]);
                    }
                    l0__u121 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 9;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u125: i32 = 0;

                    if true {
                        let l0__u125: i32 = 0;
                        let e__u126 = v * 4 + l0__u125 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v36 = mulh2(acc.v36, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u125: i32 = 1;
                        let e__u127 = v * 4 + l0__u125 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v38 = mulh2(acc.v38, sh2[ (0 as i32)]);
                    }
                    l0__u125 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u128: i32 = 0;

                    if true {
                        let l0__u128: i32 = 0;
                        let e__u129 = v * 4 + l0__u128 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v37 = mulh2(acc.v37, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u128: i32 = 1;
                        let e__u130 = v * 4 + l0__u128 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v39 = mulh2(acc.v39, sh2[ (1 as i32)]);
                    }
                    l0__u128 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 10;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u132: i32 = 0;

                    if true {
                        let l0__u132: i32 = 0;
                        let e__u133 = v * 4 + l0__u132 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v40 = mulh2(acc.v40, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u132: i32 = 1;
                        let e__u134 = v * 4 + l0__u132 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v42 = mulh2(acc.v42, sh2[ (0 as i32)]);
                    }
                    l0__u132 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u135: i32 = 0;

                    if true {
                        let l0__u135: i32 = 0;
                        let e__u136 = v * 4 + l0__u135 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v41 = mulh2(acc.v41, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u135: i32 = 1;
                        let e__u137 = v * 4 + l0__u135 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v43 = mulh2(acc.v43, sh2[ (1 as i32)]);
                    }
                    l0__u135 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 11;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u139: i32 = 0;

                    if true {
                        let l0__u139: i32 = 0;
                        let e__u140 = v * 4 + l0__u139 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v44 = mulh2(acc.v44, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u139: i32 = 1;
                        let e__u141 = v * 4 + l0__u139 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v46 = mulh2(acc.v46, sh2[ (0 as i32)]);
                    }
                    l0__u139 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u142: i32 = 0;

                    if true {
                        let l0__u142: i32 = 0;
                        let e__u143 = v * 4 + l0__u142 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v45 = mulh2(acc.v45, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u142: i32 = 1;
                        let e__u144 = v * 4 + l0__u142 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v47 = mulh2(acc.v47, sh2[ (1 as i32)]);
                    }
                    l0__u142 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 12;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u146: i32 = 0;

                    if true {
                        let l0__u146: i32 = 0;
                        let e__u147 = v * 4 + l0__u146 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v48 = mulh2(acc.v48, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u146: i32 = 1;
                        let e__u148 = v * 4 + l0__u146 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v50 = mulh2(acc.v50, sh2[ (0 as i32)]);
                    }
                    l0__u146 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u149: i32 = 0;

                    if true {
                        let l0__u149: i32 = 0;
                        let e__u150 = v * 4 + l0__u149 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v49 = mulh2(acc.v49, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u149: i32 = 1;
                        let e__u151 = v * 4 + l0__u149 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v51 = mulh2(acc.v51, sh2[ (1 as i32)]);
                    }
                    l0__u149 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 13;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u153: i32 = 0;

                    if true {
                        let l0__u153: i32 = 0;
                        let e__u154 = v * 4 + l0__u153 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v52 = mulh2(acc.v52, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u153: i32 = 1;
                        let e__u155 = v * 4 + l0__u153 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v54 = mulh2(acc.v54, sh2[ (0 as i32)]);
                    }
                    l0__u153 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u156: i32 = 0;

                    if true {
                        let l0__u156: i32 = 0;
                        let e__u157 = v * 4 + l0__u156 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v53 = mulh2(acc.v53, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u156: i32 = 1;
                        let e__u158 = v * 4 + l0__u156 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v55 = mulh2(acc.v55, sh2[ (1 as i32)]);
                    }
                    l0__u156 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 14;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u160: i32 = 0;

                    if true {
                        let l0__u160: i32 = 0;
                        let e__u161 = v * 4 + l0__u160 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v56 = mulh2(acc.v56, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u160: i32 = 1;
                        let e__u162 = v * 4 + l0__u160 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v58 = mulh2(acc.v58, sh2[ (0 as i32)]);
                    }
                    l0__u160 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u163: i32 = 0;

                    if true {
                        let l0__u163: i32 = 0;
                        let e__u164 = v * 4 + l0__u163 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v57 = mulh2(acc.v57, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u163: i32 = 1;
                        let e__u165 = v * 4 + l0__u163 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v59 = mulh2(acc.v59, sh2[ (1 as i32)]);
                    }
                    l0__u163 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 15;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u167: i32 = 0;

                    if true {
                        let l0__u167: i32 = 0;
                        let e__u168 = v * 4 + l0__u167 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v60 = mulh2(acc.v60, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u167: i32 = 1;
                        let e__u169 = v * 4 + l0__u167 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v62 = mulh2(acc.v62, sh2[ (0 as i32)]);
                    }
                    l0__u167 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u170: i32 = 0;

                    if true {
                        let l0__u170: i32 = 0;
                        let e__u171 = v * 4 + l0__u170 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v61 = mulh2(acc.v61, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u170: i32 = 1;
                        let e__u172 = v * 4 + l0__u170 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v63 = mulh2(acc.v63, sh2[ (1 as i32)]);
                    }
                    l0__u170 = (2 as i32);
                }
                col = (2 as i32);
            }
            v = (16 as i32);
            // ---- B[k] = get_half2(KQ_C[k]): pure F32->half2 pack on Turing, no transpose/shuffle ----
            var b: u32[8] = default < u32[8] > ();
            t = 0;

            if true {
                let t: i32 = 0;
                var l0__u173: i32 = 0;

                if true {
                    let l0__u173: i32 = 0;
                    b[ (0 as i32)] = h2(kqc[ (0 as i32)], kqc[ (1 as i32)]);
                }

                if true {
                    let l0__u173: i32 = 2;
                    b[ (1 as i32)] = h2(kqc[ (2 as i32)], kqc[ (3 as i32)]);
                }

                if true {
                    let l0__u173: i32 = 4;
                    b[ (2 as i32)] = h2(kqc[ (4 as i32)], kqc[ (5 as i32)]);
                }

                if true {
                    let l0__u173: i32 = 6;
                    b[ (3 as i32)] = h2(kqc[ (6 as i32)], kqc[ (7 as i32)]);
                }
                l0__u173 = (8 as i32);
            }
            t = (1 as i32);
            // ---- double-stage tail: wait the V tile, sync, then preload next K + mask before the current
            //      VKQ mma (skipped only on the last iteration; waits/barriers stay unconditional) ----
            cpwait();
            barrier();

            if !last {
                load_mask(mask_h, mt, stride_mask, kv_row0 + 32, query0, n_queries, ncols1, tx, ty);
                load_kv(k_h2, kt, stride_k, kv_row0 + 32, tx, ty);
            }
            // ---- VKQ += probs @ V^T, swapped operands (A=B_probs, B=A_V trans-V fragment); f16
            //      accumulation with B registers 0,2 -> D.x[0..1] then 1,3 -> D.x[2..3] ----
            v = 0;

            if true {
                let v: i32 = 0;
                var d0__u178: U2 = U2 {
                    acc.v0, acc.v1
                }
                ;
                var d1__u178: U2 = U2 {
                    acc.v2, acc.v3
                }
                ;
                var kk__u178: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u178: i32 = 0;
                    let k0__u179 = kk__u178 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u179 = ldm4t_tile(ldm_addr(vt, 2 * k0__u179 * 128 + v * 8, tx));
                    d0__u178 = mma_f16(
                        d0__u178,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u179.a,
                        av__u179.c);
                    d1__u178 = mma_f16(
                        d1__u178,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u179.b,
                        av__u179.d);
                }
                kk__u178 = (1 as i32);
                acc.v0 = d0__u178.a;
                acc.v1 = d0__u178.b;
                acc.v2 = d1__u178.a;
                acc.v3 = d1__u178.b;
            }

            if true {
                let v: i32 = 1;
                var d0__u180: U2 = U2 {
                    acc.v4, acc.v5
                }
                ;
                var d1__u180: U2 = U2 {
                    acc.v6, acc.v7
                }
                ;
                var kk__u180: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u180: i32 = 0;
                    let k0__u181 = kk__u180 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u181 = ldm4t_tile(ldm_addr(vt, 2 * k0__u181 * 128 + v * 8, tx));
                    d0__u180 = mma_f16(
                        d0__u180,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u181.a,
                        av__u181.c);
                    d1__u180 = mma_f16(
                        d1__u180,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u181.b,
                        av__u181.d);
                }
                kk__u180 = (1 as i32);
                acc.v4 = d0__u180.a;
                acc.v5 = d0__u180.b;
                acc.v6 = d1__u180.a;
                acc.v7 = d1__u180.b;
            }

            if true {
                let v: i32 = 2;
                var d0__u182: U2 = U2 {
                    acc.v8, acc.v9
                }
                ;
                var d1__u182: U2 = U2 {
                    acc.v10, acc.v11
                }
                ;
                var kk__u182: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u182: i32 = 0;
                    let k0__u183 = kk__u182 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u183 = ldm4t_tile(ldm_addr(vt, 2 * k0__u183 * 128 + v * 8, tx));
                    d0__u182 = mma_f16(
                        d0__u182,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u183.a,
                        av__u183.c);
                    d1__u182 = mma_f16(
                        d1__u182,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u183.b,
                        av__u183.d);
                }
                kk__u182 = (1 as i32);
                acc.v8 = d0__u182.a;
                acc.v9 = d0__u182.b;
                acc.v10 = d1__u182.a;
                acc.v11 = d1__u182.b;
            }

            if true {
                let v: i32 = 3;
                var d0__u184: U2 = U2 {
                    acc.v12, acc.v13
                }
                ;
                var d1__u184: U2 = U2 {
                    acc.v14, acc.v15
                }
                ;
                var kk__u184: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u184: i32 = 0;
                    let k0__u185 = kk__u184 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u185 = ldm4t_tile(ldm_addr(vt, 2 * k0__u185 * 128 + v * 8, tx));
                    d0__u184 = mma_f16(
                        d0__u184,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u185.a,
                        av__u185.c);
                    d1__u184 = mma_f16(
                        d1__u184,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u185.b,
                        av__u185.d);
                }
                kk__u184 = (1 as i32);
                acc.v12 = d0__u184.a;
                acc.v13 = d0__u184.b;
                acc.v14 = d1__u184.a;
                acc.v15 = d1__u184.b;
            }

            if true {
                let v: i32 = 4;
                var d0__u186: U2 = U2 {
                    acc.v16, acc.v17
                }
                ;
                var d1__u186: U2 = U2 {
                    acc.v18, acc.v19
                }
                ;
                var kk__u186: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u186: i32 = 0;
                    let k0__u187 = kk__u186 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u187 = ldm4t_tile(ldm_addr(vt, 2 * k0__u187 * 128 + v * 8, tx));
                    d0__u186 = mma_f16(
                        d0__u186,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u187.a,
                        av__u187.c);
                    d1__u186 = mma_f16(
                        d1__u186,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u187.b,
                        av__u187.d);
                }
                kk__u186 = (1 as i32);
                acc.v16 = d0__u186.a;
                acc.v17 = d0__u186.b;
                acc.v18 = d1__u186.a;
                acc.v19 = d1__u186.b;
            }

            if true {
                let v: i32 = 5;
                var d0__u188: U2 = U2 {
                    acc.v20, acc.v21
                }
                ;
                var d1__u188: U2 = U2 {
                    acc.v22, acc.v23
                }
                ;
                var kk__u188: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u188: i32 = 0;
                    let k0__u189 = kk__u188 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u189 = ldm4t_tile(ldm_addr(vt, 2 * k0__u189 * 128 + v * 8, tx));
                    d0__u188 = mma_f16(
                        d0__u188,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u189.a,
                        av__u189.c);
                    d1__u188 = mma_f16(
                        d1__u188,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u189.b,
                        av__u189.d);
                }
                kk__u188 = (1 as i32);
                acc.v20 = d0__u188.a;
                acc.v21 = d0__u188.b;
                acc.v22 = d1__u188.a;
                acc.v23 = d1__u188.b;
            }

            if true {
                let v: i32 = 6;
                var d0__u190: U2 = U2 {
                    acc.v24, acc.v25
                }
                ;
                var d1__u190: U2 = U2 {
                    acc.v26, acc.v27
                }
                ;
                var kk__u190: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u190: i32 = 0;
                    let k0__u191 = kk__u190 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u191 = ldm4t_tile(ldm_addr(vt, 2 * k0__u191 * 128 + v * 8, tx));
                    d0__u190 = mma_f16(
                        d0__u190,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u191.a,
                        av__u191.c);
                    d1__u190 = mma_f16(
                        d1__u190,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u191.b,
                        av__u191.d);
                }
                kk__u190 = (1 as i32);
                acc.v24 = d0__u190.a;
                acc.v25 = d0__u190.b;
                acc.v26 = d1__u190.a;
                acc.v27 = d1__u190.b;
            }

            if true {
                let v: i32 = 7;
                var d0__u192: U2 = U2 {
                    acc.v28, acc.v29
                }
                ;
                var d1__u192: U2 = U2 {
                    acc.v30, acc.v31
                }
                ;
                var kk__u192: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u192: i32 = 0;
                    let k0__u193 = kk__u192 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u193 = ldm4t_tile(ldm_addr(vt, 2 * k0__u193 * 128 + v * 8, tx));
                    d0__u192 = mma_f16(
                        d0__u192,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u193.a,
                        av__u193.c);
                    d1__u192 = mma_f16(
                        d1__u192,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u193.b,
                        av__u193.d);
                }
                kk__u192 = (1 as i32);
                acc.v28 = d0__u192.a;
                acc.v29 = d0__u192.b;
                acc.v30 = d1__u192.a;
                acc.v31 = d1__u192.b;
            }

            if true {
                let v: i32 = 8;
                var d0__u194: U2 = U2 {
                    acc.v32, acc.v33
                }
                ;
                var d1__u194: U2 = U2 {
                    acc.v34, acc.v35
                }
                ;
                var kk__u194: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u194: i32 = 0;
                    let k0__u195 = kk__u194 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u195 = ldm4t_tile(ldm_addr(vt, 2 * k0__u195 * 128 + v * 8, tx));
                    d0__u194 = mma_f16(
                        d0__u194,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u195.a,
                        av__u195.c);
                    d1__u194 = mma_f16(
                        d1__u194,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u195.b,
                        av__u195.d);
                }
                kk__u194 = (1 as i32);
                acc.v32 = d0__u194.a;
                acc.v33 = d0__u194.b;
                acc.v34 = d1__u194.a;
                acc.v35 = d1__u194.b;
            }

            if true {
                let v: i32 = 9;
                var d0__u196: U2 = U2 {
                    acc.v36, acc.v37
                }
                ;
                var d1__u196: U2 = U2 {
                    acc.v38, acc.v39
                }
                ;
                var kk__u196: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u196: i32 = 0;
                    let k0__u197 = kk__u196 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u197 = ldm4t_tile(ldm_addr(vt, 2 * k0__u197 * 128 + v * 8, tx));
                    d0__u196 = mma_f16(
                        d0__u196,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u197.a,
                        av__u197.c);
                    d1__u196 = mma_f16(
                        d1__u196,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u197.b,
                        av__u197.d);
                }
                kk__u196 = (1 as i32);
                acc.v36 = d0__u196.a;
                acc.v37 = d0__u196.b;
                acc.v38 = d1__u196.a;
                acc.v39 = d1__u196.b;
            }

            if true {
                let v: i32 = 10;
                var d0__u198: U2 = U2 {
                    acc.v40, acc.v41
                }
                ;
                var d1__u198: U2 = U2 {
                    acc.v42, acc.v43
                }
                ;
                var kk__u198: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u198: i32 = 0;
                    let k0__u199 = kk__u198 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u199 = ldm4t_tile(ldm_addr(vt, 2 * k0__u199 * 128 + v * 8, tx));
                    d0__u198 = mma_f16(
                        d0__u198,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u199.a,
                        av__u199.c);
                    d1__u198 = mma_f16(
                        d1__u198,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u199.b,
                        av__u199.d);
                }
                kk__u198 = (1 as i32);
                acc.v40 = d0__u198.a;
                acc.v41 = d0__u198.b;
                acc.v42 = d1__u198.a;
                acc.v43 = d1__u198.b;
            }

            if true {
                let v: i32 = 11;
                var d0__u200: U2 = U2 {
                    acc.v44, acc.v45
                }
                ;
                var d1__u200: U2 = U2 {
                    acc.v46, acc.v47
                }
                ;
                var kk__u200: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u200: i32 = 0;
                    let k0__u201 = kk__u200 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u201 = ldm4t_tile(ldm_addr(vt, 2 * k0__u201 * 128 + v * 8, tx));
                    d0__u200 = mma_f16(
                        d0__u200,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u201.a,
                        av__u201.c);
                    d1__u200 = mma_f16(
                        d1__u200,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u201.b,
                        av__u201.d);
                }
                kk__u200 = (1 as i32);
                acc.v44 = d0__u200.a;
                acc.v45 = d0__u200.b;
                acc.v46 = d1__u200.a;
                acc.v47 = d1__u200.b;
            }

            if true {
                let v: i32 = 12;
                var d0__u202: U2 = U2 {
                    acc.v48, acc.v49
                }
                ;
                var d1__u202: U2 = U2 {
                    acc.v50, acc.v51
                }
                ;
                var kk__u202: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u202: i32 = 0;
                    let k0__u203 = kk__u202 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u203 = ldm4t_tile(ldm_addr(vt, 2 * k0__u203 * 128 + v * 8, tx));
                    d0__u202 = mma_f16(
                        d0__u202,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u203.a,
                        av__u203.c);
                    d1__u202 = mma_f16(
                        d1__u202,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u203.b,
                        av__u203.d);
                }
                kk__u202 = (1 as i32);
                acc.v48 = d0__u202.a;
                acc.v49 = d0__u202.b;
                acc.v50 = d1__u202.a;
                acc.v51 = d1__u202.b;
            }

            if true {
                let v: i32 = 13;
                var d0__u204: U2 = U2 {
                    acc.v52, acc.v53
                }
                ;
                var d1__u204: U2 = U2 {
                    acc.v54, acc.v55
                }
                ;
                var kk__u204: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u204: i32 = 0;
                    let k0__u205 = kk__u204 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u205 = ldm4t_tile(ldm_addr(vt, 2 * k0__u205 * 128 + v * 8, tx));
                    d0__u204 = mma_f16(
                        d0__u204,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u205.a,
                        av__u205.c);
                    d1__u204 = mma_f16(
                        d1__u204,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u205.b,
                        av__u205.d);
                }
                kk__u204 = (1 as i32);
                acc.v52 = d0__u204.a;
                acc.v53 = d0__u204.b;
                acc.v54 = d1__u204.a;
                acc.v55 = d1__u204.b;
            }

            if true {
                let v: i32 = 14;
                var d0__u206: U2 = U2 {
                    acc.v56, acc.v57
                }
                ;
                var d1__u206: U2 = U2 {
                    acc.v58, acc.v59
                }
                ;
                var kk__u206: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u206: i32 = 0;
                    let k0__u207 = kk__u206 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u207 = ldm4t_tile(ldm_addr(vt, 2 * k0__u207 * 128 + v * 8, tx));
                    d0__u206 = mma_f16(
                        d0__u206,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u207.a,
                        av__u207.c);
                    d1__u206 = mma_f16(
                        d1__u206,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u207.b,
                        av__u207.d);
                }
                kk__u206 = (1 as i32);
                acc.v56 = d0__u206.a;
                acc.v57 = d0__u206.b;
                acc.v58 = d1__u206.a;
                acc.v59 = d1__u206.b;
            }

            if true {
                let v: i32 = 15;
                var d0__u208: U2 = U2 {
                    acc.v60, acc.v61
                }
                ;
                var d1__u208: U2 = U2 {
                    acc.v62, acc.v63
                }
                ;
                var kk__u208: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u208: i32 = 0;
                    let k0__u209 = kk__u208 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u209 = ldm4t_tile(ldm_addr(vt, 2 * k0__u209 * 128 + v * 8, tx));
                    d0__u208 = mma_f16(
                        d0__u208,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u209.a,
                        av__u209.c);
                    d1__u208 = mma_f16(
                        d1__u208,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u209.b,
                        av__u209.d);
                }
                kk__u208 = (1 as i32);
                acc.v60 = d0__u208.a;
                acc.v61 = d0__u208.b;
                acc.v62 = d1__u208.a;
                acc.v63 = d1__u208.b;
            }
            v = (16 as i32);
        }
        kb0 += 1;
    }

    if true {
        let n_queries: i64 = ne01z;
        let last: bool = true;
        let ncols1: i32 = 4;
        let np: i32 = 2;
        let kt = smem;
        let vt = smem + (16384 as u64);
        let mt = smem + (32768 as u64);
        let kv_row0 = (kb0 as i64) * 32;
        // k_VKQ_0 = kb0*nbatch_fa
        let query0 = (jt as i64) * (ncols1 as i64);
        // j0 for the mask loader
        let cb = (ty / np) * 16;
        // warp Q-column block base (cols_per_warp=16)
        let rh = (ty % np) * 16;
        // warp KV-row half base (np*T_A_KQ::I split)
        let nt = 32 / (np * 16);
        // KQ_C/B tiles per warp: 1 (np=2) or 2 (np=1)
        let kqoff = (3.0 as f32) * (0.6931 as f32);
        // FATTN_KQ_MAX_OFFSET
        var t: i32 = 0;
        // nstages>1 entry: this iteration's K tile + mask were preloaded by the prologue or the previous
        // iter tail; wait for them, then issue the V tile of this iteration into the pipeline.
        cpwait();
        barrier();
        load_kv(v_h2, vt, stride_v, kv_row0, tx, ty);
        // ---- KQ = Q_B @ K_A over the single nbatch_K2 block k in [0,128) half2 (16 x T_A_KQ::J=8) ----
        var kqc: f32[16] = default < f32[16] > ();
        // up to 2 fresh zero tile<16,16,float> C tiles
        var q: i32 = 0;
        // Q_B tile index = k_KQ_0/8

        if true {
            let q: i32 = 0;
            let q0__u1 = qr.q0;
            let q1__u1 = qr.q1;
            let q2__u1 = qr.q2;
            let q3__u1 = qr.q3;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u2 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u2 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u1, q1__u1, q2__u1, q3__u1, ka__u2.a, ka__u2.c);
                kqc[ (0 as i32)] = lo__u2.x;
                kqc[ (1 as i32)] = lo__u2.y;
                kqc[ (2 as i32)] = lo__u2.z;
                kqc[ (3 as i32)] = lo__u2.w;
                let hi__u2 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u1, q1__u1, q2__u1, q3__u1, ka__u2.b, ka__u2.d);
                kqc[ (4 as i32)] = hi__u2.x;
                kqc[ (5 as i32)] = hi__u2.y;
                kqc[ (6 as i32)] = hi__u2.z;
                kqc[ (7 as i32)] = hi__u2.w;
            }
            t = (1 as i32);
        }

        if true {
            let q: i32 = 1;
            let q0__u3 = qr.q4;
            let q1__u3 = qr.q5;
            let q2__u3 = qr.q6;
            let q3__u3 = qr.q7;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u4 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u4 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u3, q1__u3, q2__u3, q3__u3, ka__u4.a, ka__u4.c);
                kqc[ (0 as i32)] = lo__u4.x;
                kqc[ (1 as i32)] = lo__u4.y;
                kqc[ (2 as i32)] = lo__u4.z;
                kqc[ (3 as i32)] = lo__u4.w;
                let hi__u4 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u3, q1__u3, q2__u3, q3__u3, ka__u4.b, ka__u4.d);
                kqc[ (4 as i32)] = hi__u4.x;
                kqc[ (5 as i32)] = hi__u4.y;
                kqc[ (6 as i32)] = hi__u4.z;
                kqc[ (7 as i32)] = hi__u4.w;
            }
            t = (1 as i32);
        }

        if true {
            let q: i32 = 2;
            let q0__u5 = qr.q8;
            let q1__u5 = qr.q9;
            let q2__u5 = qr.q10;
            let q3__u5 = qr.q11;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u6 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u6 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u5, q1__u5, q2__u5, q3__u5, ka__u6.a, ka__u6.c);
                kqc[ (0 as i32)] = lo__u6.x;
                kqc[ (1 as i32)] = lo__u6.y;
                kqc[ (2 as i32)] = lo__u6.z;
                kqc[ (3 as i32)] = lo__u6.w;
                let hi__u6 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u5, q1__u5, q2__u5, q3__u5, ka__u6.b, ka__u6.d);
                kqc[ (4 as i32)] = hi__u6.x;
                kqc[ (5 as i32)] = hi__u6.y;
                kqc[ (6 as i32)] = hi__u6.z;
                kqc[ (7 as i32)] = hi__u6.w;
            }
            t = (1 as i32);
        }

        if true {
            let q: i32 = 3;
            let q0__u7 = qr.q12;
            let q1__u7 = qr.q13;
            let q2__u7 = qr.q14;
            let q3__u7 = qr.q15;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u8 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u8 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u7, q1__u7, q2__u7, q3__u7, ka__u8.a, ka__u8.c);
                kqc[ (0 as i32)] = lo__u8.x;
                kqc[ (1 as i32)] = lo__u8.y;
                kqc[ (2 as i32)] = lo__u8.z;
                kqc[ (3 as i32)] = lo__u8.w;
                let hi__u8 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u7, q1__u7, q2__u7, q3__u7, ka__u8.b, ka__u8.d);
                kqc[ (4 as i32)] = hi__u8.x;
                kqc[ (5 as i32)] = hi__u8.y;
                kqc[ (6 as i32)] = hi__u8.z;
                kqc[ (7 as i32)] = hi__u8.w;
            }
            t = (1 as i32);
        }

        if true {
            let q: i32 = 4;
            let q0__u9 = qr.q16;
            let q1__u9 = qr.q17;
            let q2__u9 = qr.q18;
            let q3__u9 = qr.q19;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u10 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u10 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u9, q1__u9, q2__u9, q3__u9, ka__u10.a, ka__u10.c);
                kqc[ (0 as i32)] = lo__u10.x;
                kqc[ (1 as i32)] = lo__u10.y;
                kqc[ (2 as i32)] = lo__u10.z;
                kqc[ (3 as i32)] = lo__u10.w;
                let hi__u10 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u9, q1__u9, q2__u9, q3__u9, ka__u10.b, ka__u10.d);
                kqc[ (4 as i32)] = hi__u10.x;
                kqc[ (5 as i32)] = hi__u10.y;
                kqc[ (6 as i32)] = hi__u10.z;
                kqc[ (7 as i32)] = hi__u10.w;
            }
            t = (1 as i32);
        }

        if true {
            let q: i32 = 5;
            let q0__u11 = qr.q20;
            let q1__u11 = qr.q21;
            let q2__u11 = qr.q22;
            let q3__u11 = qr.q23;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u12 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u12 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u11, q1__u11, q2__u11, q3__u11, ka__u12.a, ka__u12.c);
                kqc[ (0 as i32)] = lo__u12.x;
                kqc[ (1 as i32)] = lo__u12.y;
                kqc[ (2 as i32)] = lo__u12.z;
                kqc[ (3 as i32)] = lo__u12.w;
                let hi__u12 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u11, q1__u11, q2__u11, q3__u11, ka__u12.b, ka__u12.d);
                kqc[ (4 as i32)] = hi__u12.x;
                kqc[ (5 as i32)] = hi__u12.y;
                kqc[ (6 as i32)] = hi__u12.z;
                kqc[ (7 as i32)] = hi__u12.w;
            }
            t = (1 as i32);
        }

        if true {
            let q: i32 = 6;
            let q0__u13 = qr.q24;
            let q1__u13 = qr.q25;
            let q2__u13 = qr.q26;
            let q3__u13 = qr.q27;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u14 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u14 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u13, q1__u13, q2__u13, q3__u13, ka__u14.a, ka__u14.c);
                kqc[ (0 as i32)] = lo__u14.x;
                kqc[ (1 as i32)] = lo__u14.y;
                kqc[ (2 as i32)] = lo__u14.z;
                kqc[ (3 as i32)] = lo__u14.w;
                let hi__u14 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u13, q1__u13, q2__u13, q3__u13, ka__u14.b, ka__u14.d);
                kqc[ (4 as i32)] = hi__u14.x;
                kqc[ (5 as i32)] = hi__u14.y;
                kqc[ (6 as i32)] = hi__u14.z;
                kqc[ (7 as i32)] = hi__u14.w;
            }
            t = (1 as i32);
        }

        if true {
            let q: i32 = 7;
            let q0__u15 = qr.q28;
            let q1__u15 = qr.q29;
            let q2__u15 = qr.q30;
            let q3__u15 = qr.q31;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u16 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u16 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u15, q1__u15, q2__u15, q3__u15, ka__u16.a, ka__u16.c);
                kqc[ (0 as i32)] = lo__u16.x;
                kqc[ (1 as i32)] = lo__u16.y;
                kqc[ (2 as i32)] = lo__u16.z;
                kqc[ (3 as i32)] = lo__u16.w;
                let hi__u16 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u15, q1__u15, q2__u15, q3__u15, ka__u16.b, ka__u16.d);
                kqc[ (4 as i32)] = hi__u16.x;
                kqc[ (5 as i32)] = hi__u16.y;
                kqc[ (6 as i32)] = hi__u16.z;
                kqc[ (7 as i32)] = hi__u16.w;
            }
            t = (1 as i32);
        }

        if true {
            let q: i32 = 8;
            let q0__u17 = qr.q32;
            let q1__u17 = qr.q33;
            let q2__u17 = qr.q34;
            let q3__u17 = qr.q35;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u18 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u18 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u17, q1__u17, q2__u17, q3__u17, ka__u18.a, ka__u18.c);
                kqc[ (0 as i32)] = lo__u18.x;
                kqc[ (1 as i32)] = lo__u18.y;
                kqc[ (2 as i32)] = lo__u18.z;
                kqc[ (3 as i32)] = lo__u18.w;
                let hi__u18 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u17, q1__u17, q2__u17, q3__u17, ka__u18.b, ka__u18.d);
                kqc[ (4 as i32)] = hi__u18.x;
                kqc[ (5 as i32)] = hi__u18.y;
                kqc[ (6 as i32)] = hi__u18.z;
                kqc[ (7 as i32)] = hi__u18.w;
            }
            t = (1 as i32);
        }

        if true {
            let q: i32 = 9;
            let q0__u19 = qr.q36;
            let q1__u19 = qr.q37;
            let q2__u19 = qr.q38;
            let q3__u19 = qr.q39;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u20 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u20 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u19, q1__u19, q2__u19, q3__u19, ka__u20.a, ka__u20.c);
                kqc[ (0 as i32)] = lo__u20.x;
                kqc[ (1 as i32)] = lo__u20.y;
                kqc[ (2 as i32)] = lo__u20.z;
                kqc[ (3 as i32)] = lo__u20.w;
                let hi__u20 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u19, q1__u19, q2__u19, q3__u19, ka__u20.b, ka__u20.d);
                kqc[ (4 as i32)] = hi__u20.x;
                kqc[ (5 as i32)] = hi__u20.y;
                kqc[ (6 as i32)] = hi__u20.z;
                kqc[ (7 as i32)] = hi__u20.w;
            }
            t = (1 as i32);
        }

        if true {
            let q: i32 = 10;
            let q0__u21 = qr.q40;
            let q1__u21 = qr.q41;
            let q2__u21 = qr.q42;
            let q3__u21 = qr.q43;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u22 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u22 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u21, q1__u21, q2__u21, q3__u21, ka__u22.a, ka__u22.c);
                kqc[ (0 as i32)] = lo__u22.x;
                kqc[ (1 as i32)] = lo__u22.y;
                kqc[ (2 as i32)] = lo__u22.z;
                kqc[ (3 as i32)] = lo__u22.w;
                let hi__u22 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u21, q1__u21, q2__u21, q3__u21, ka__u22.b, ka__u22.d);
                kqc[ (4 as i32)] = hi__u22.x;
                kqc[ (5 as i32)] = hi__u22.y;
                kqc[ (6 as i32)] = hi__u22.z;
                kqc[ (7 as i32)] = hi__u22.w;
            }
            t = (1 as i32);
        }

        if true {
            let q: i32 = 11;
            let q0__u23 = qr.q44;
            let q1__u23 = qr.q45;
            let q2__u23 = qr.q46;
            let q3__u23 = qr.q47;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u24 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u24 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u23, q1__u23, q2__u23, q3__u23, ka__u24.a, ka__u24.c);
                kqc[ (0 as i32)] = lo__u24.x;
                kqc[ (1 as i32)] = lo__u24.y;
                kqc[ (2 as i32)] = lo__u24.z;
                kqc[ (3 as i32)] = lo__u24.w;
                let hi__u24 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u23, q1__u23, q2__u23, q3__u23, ka__u24.b, ka__u24.d);
                kqc[ (4 as i32)] = hi__u24.x;
                kqc[ (5 as i32)] = hi__u24.y;
                kqc[ (6 as i32)] = hi__u24.z;
                kqc[ (7 as i32)] = hi__u24.w;
            }
            t = (1 as i32);
        }

        if true {
            let q: i32 = 12;
            let q0__u25 = qr.q48;
            let q1__u25 = qr.q49;
            let q2__u25 = qr.q50;
            let q3__u25 = qr.q51;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u26 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u26 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u25, q1__u25, q2__u25, q3__u25, ka__u26.a, ka__u26.c);
                kqc[ (0 as i32)] = lo__u26.x;
                kqc[ (1 as i32)] = lo__u26.y;
                kqc[ (2 as i32)] = lo__u26.z;
                kqc[ (3 as i32)] = lo__u26.w;
                let hi__u26 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u25, q1__u25, q2__u25, q3__u25, ka__u26.b, ka__u26.d);
                kqc[ (4 as i32)] = hi__u26.x;
                kqc[ (5 as i32)] = hi__u26.y;
                kqc[ (6 as i32)] = hi__u26.z;
                kqc[ (7 as i32)] = hi__u26.w;
            }
            t = (1 as i32);
        }

        if true {
            let q: i32 = 13;
            let q0__u27 = qr.q52;
            let q1__u27 = qr.q53;
            let q2__u27 = qr.q54;
            let q3__u27 = qr.q55;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u28 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u28 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u27, q1__u27, q2__u27, q3__u27, ka__u28.a, ka__u28.c);
                kqc[ (0 as i32)] = lo__u28.x;
                kqc[ (1 as i32)] = lo__u28.y;
                kqc[ (2 as i32)] = lo__u28.z;
                kqc[ (3 as i32)] = lo__u28.w;
                let hi__u28 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u27, q1__u27, q2__u27, q3__u27, ka__u28.b, ka__u28.d);
                kqc[ (4 as i32)] = hi__u28.x;
                kqc[ (5 as i32)] = hi__u28.y;
                kqc[ (6 as i32)] = hi__u28.z;
                kqc[ (7 as i32)] = hi__u28.w;
            }
            t = (1 as i32);
        }

        if true {
            let q: i32 = 14;
            let q0__u29 = qr.q56;
            let q1__u29 = qr.q57;
            let q2__u29 = qr.q58;
            let q3__u29 = qr.q59;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u30 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u30 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u29, q1__u29, q2__u29, q3__u29, ka__u30.a, ka__u30.c);
                kqc[ (0 as i32)] = lo__u30.x;
                kqc[ (1 as i32)] = lo__u30.y;
                kqc[ (2 as i32)] = lo__u30.z;
                kqc[ (3 as i32)] = lo__u30.w;
                let hi__u30 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u29, q1__u29, q2__u29, q3__u29, ka__u30.b, ka__u30.d);
                kqc[ (4 as i32)] = hi__u30.x;
                kqc[ (5 as i32)] = hi__u30.y;
                kqc[ (6 as i32)] = hi__u30.z;
                kqc[ (7 as i32)] = hi__u30.w;
            }
            t = (1 as i32);
        }

        if true {
            let q: i32 = 15;
            let q0__u31 = qr.q60;
            let q1__u31 = qr.q61;
            let q2__u31 = qr.q62;
            let q3__u31 = qr.q63;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u32 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u32 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u31, q1__u31, q2__u31, q3__u31, ka__u32.a, ka__u32.c);
                kqc[ (0 as i32)] = lo__u32.x;
                kqc[ (1 as i32)] = lo__u32.y;
                kqc[ (2 as i32)] = lo__u32.z;
                kqc[ (3 as i32)] = lo__u32.w;
                let hi__u32 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u31, q1__u31, q2__u31, q3__u31, ka__u32.b, ka__u32.d);
                kqc[ (4 as i32)] = hi__u32.x;
                kqc[ (5 as i32)] = hi__u32.y;
                kqc[ (6 as i32)] = hi__u32.z;
                kqc[ (7 as i32)] = hi__u32.w;
            }
            t = (1 as i32);
        }
        q = (16 as i32);
        // ---- mask add + running max: half2 mask word at row (cb+get_i)/8, half2 col (rb+get_j)/2 ----
        var kqold: f32[2] = default < f32[2] > ();
        // KQ_max[cols_per_thread=2]
        var kqmn: f32[2] = default < f32[2] > ();
        // KQ_max_new
        var rsa: f32[2] = default < f32[2] > ();
        // KQ_rowsum_add
        kqold[ (0 as i32)] = frombits(bits(acc.k0));
        kqold[ (1 as i32)] = frombits(bits(acc.k1));
        kqmn[ (0 as i32)] = kqold[ (0 as i32)];
        kqmn[ (1 as i32)] = kqold[ (1 as i32)];
        t = 0;

        if true {
            let t: i32 = 0;
            let rb__u33 = t * np * 16 + rh;
            var l0__u33: i32 = 0;

            if true {
                let l0__u33: i32 = 0;
                // half2 pairs, l0 even: (j,i),(j,i+1)
                let mh2__u34 = ( (cb + gi_c(l0__u33, tx)) / 8) * 20 + ( (rb__u33 + gj_c(l0__u33, tx)) / 2);
                let w__u34 = ldu(mt + ( (mh2__u34 as u64) * (4 as u64)));
                kqc[ (0 as i32)] = fmaf(slope, hlo(w__u34), kqc[ (0 as i32)]);
                kqc[ (1 as i32)] = fmaf(slope, hhi(w__u34), kqc[ (1 as i32)]);
            }

            if true {
                let l0__u33: i32 = 2;
                // half2 pairs, l0 even: (j,i),(j,i+1)
                let mh2__u35 = ( (cb + gi_c(l0__u33, tx)) / 8) * 20 + ( (rb__u33 + gj_c(l0__u33, tx)) / 2);
                let w__u35 = ldu(mt + ( (mh2__u35 as u64) * (4 as u64)));
                kqc[ (2 as i32)] = fmaf(slope, hlo(w__u35), kqc[ (2 as i32)]);
                kqc[ (3 as i32)] = fmaf(slope, hhi(w__u35), kqc[ (3 as i32)]);
            }

            if true {
                let l0__u33: i32 = 4;
                // half2 pairs, l0 even: (j,i),(j,i+1)
                let mh2__u36 = ( (cb + gi_c(l0__u33, tx)) / 8) * 20 + ( (rb__u33 + gj_c(l0__u33, tx)) / 2);
                let w__u36 = ldu(mt + ( (mh2__u36 as u64) * (4 as u64)));
                kqc[ (4 as i32)] = fmaf(slope, hlo(w__u36), kqc[ (4 as i32)]);
                kqc[ (5 as i32)] = fmaf(slope, hhi(w__u36), kqc[ (5 as i32)]);
            }

            if true {
                let l0__u33: i32 = 6;
                // half2 pairs, l0 even: (j,i),(j,i+1)
                let mh2__u37 = ( (cb + gi_c(l0__u33, tx)) / 8) * 20 + ( (rb__u33 + gj_c(l0__u33, tx)) / 2);
                let w__u37 = ldu(mt + ( (mh2__u37 as u64) * (4 as u64)));
                kqc[ (6 as i32)] = fmaf(slope, hlo(w__u37), kqc[ (6 as i32)]);
                kqc[ (7 as i32)] = fmaf(slope, hhi(w__u37), kqc[ (7 as i32)]);
            }
            l0__u33 = (8 as i32);
            var l__u33: i32 = 0;

            if true {
                let l__u33: i32 = 0;
                let idx__u38 = (l__u33 / 2) % 2;
                let mv__u38 = kqc[ (0 as i32)] + kqoff;

                if mv__u38 > kqmn[ (0 as i32)] {
                    kqmn[ (0 as i32)] = mv__u38;
                }
            }

            if true {
                let l__u33: i32 = 1;
                let idx__u39 = (l__u33 / 2) % 2;
                let mv__u39 = kqc[ (1 as i32)] + kqoff;

                if mv__u39 > kqmn[ (0 as i32)] {
                    kqmn[ (0 as i32)] = mv__u39;
                }
            }

            if true {
                let l__u33: i32 = 2;
                let idx__u40 = (l__u33 / 2) % 2;
                let mv__u40 = kqc[ (2 as i32)] + kqoff;

                if mv__u40 > kqmn[ (1 as i32)] {
                    kqmn[ (1 as i32)] = mv__u40;
                }
            }

            if true {
                let l__u33: i32 = 3;
                let idx__u41 = (l__u33 / 2) % 2;
                let mv__u41 = kqc[ (3 as i32)] + kqoff;

                if mv__u41 > kqmn[ (1 as i32)] {
                    kqmn[ (1 as i32)] = mv__u41;
                }
            }

            if true {
                let l__u33: i32 = 4;
                let idx__u42 = (l__u33 / 2) % 2;
                let mv__u42 = kqc[ (4 as i32)] + kqoff;

                if mv__u42 > kqmn[ (0 as i32)] {
                    kqmn[ (0 as i32)] = mv__u42;
                }
            }

            if true {
                let l__u33: i32 = 5;
                let idx__u43 = (l__u33 / 2) % 2;
                let mv__u43 = kqc[ (5 as i32)] + kqoff;

                if mv__u43 > kqmn[ (0 as i32)] {
                    kqmn[ (0 as i32)] = mv__u43;
                }
            }

            if true {
                let l__u33: i32 = 6;
                let idx__u44 = (l__u33 / 2) % 2;
                let mv__u44 = kqc[ (6 as i32)] + kqoff;

                if mv__u44 > kqmn[ (1 as i32)] {
                    kqmn[ (1 as i32)] = mv__u44;
                }
            }

            if true {
                let l__u33: i32 = 7;
                let idx__u45 = (l__u33 / 2) % 2;
                let mv__u45 = kqc[ (7 as i32)] + kqoff;

                if mv__u45 > kqmn[ (1 as i32)] {
                    kqmn[ (1 as i32)] = mv__u45;
                }
            }
            l__u33 = (8 as i32);
        }
        t = (1 as i32);
        // Values per KQ column spread across 4 threads: __shfl_xor offsets 2 then 1 (TURING wide).
        var col: i32 = 0;

        if true {
            let col: i32 = 0;
            var s__u46 = shflx(kqmn[ (0 as i32)], 2);

            if s__u46 > kqmn[ (0 as i32)] {
                kqmn[ (0 as i32)] = s__u46;
            }
            s__u46 = shflx(kqmn[ (0 as i32)], 1);

            if s__u46 > kqmn[ (0 as i32)] {
                kqmn[ (0 as i32)] = s__u46;
            }
        }

        if true {
            let col: i32 = 1;
            var s__u47 = shflx(kqmn[ (1 as i32)], 2);

            if s__u47 > kqmn[ (1 as i32)] {
                kqmn[ (1 as i32)] = s__u47;
            }
            s__u47 = shflx(kqmn[ (1 as i32)], 1);

            if s__u47 > kqmn[ (1 as i32)] {
                kqmn[ (1 as i32)] = s__u47;
            }
        }
        col = (2 as i32);
        // ---- softmax with current max: expf(KQ - KQ_max_new); divisor stays in KQ_rowsum ----
        t = 0;

        if true {
            let t: i32 = 0;
            var l__u48: i32 = 0;

            if true {
                let l__u48: i32 = 0;
                let idx__u49 = (l__u48 / 2) % 2;
                let e__u49 = expf(kqc[ (0 as i32)] - kqmn[ (0 as i32)]);
                kqc[ (0 as i32)] = e__u49;
                rsa[ (0 as i32)] += e__u49;
            }

            if true {
                let l__u48: i32 = 1;
                let idx__u50 = (l__u48 / 2) % 2;
                let e__u50 = expf(kqc[ (1 as i32)] - kqmn[ (0 as i32)]);
                kqc[ (1 as i32)] = e__u50;
                rsa[ (0 as i32)] += e__u50;
            }

            if true {
                let l__u48: i32 = 2;
                let idx__u51 = (l__u48 / 2) % 2;
                let e__u51 = expf(kqc[ (2 as i32)] - kqmn[ (1 as i32)]);
                kqc[ (2 as i32)] = e__u51;
                rsa[ (1 as i32)] += e__u51;
            }

            if true {
                let l__u48: i32 = 3;
                let idx__u52 = (l__u48 / 2) % 2;
                let e__u52 = expf(kqc[ (3 as i32)] - kqmn[ (1 as i32)]);
                kqc[ (3 as i32)] = e__u52;
                rsa[ (1 as i32)] += e__u52;
            }

            if true {
                let l__u48: i32 = 4;
                let idx__u53 = (l__u48 / 2) % 2;
                let e__u53 = expf(kqc[ (4 as i32)] - kqmn[ (0 as i32)]);
                kqc[ (4 as i32)] = e__u53;
                rsa[ (0 as i32)] += e__u53;
            }

            if true {
                let l__u48: i32 = 5;
                let idx__u54 = (l__u48 / 2) % 2;
                let e__u54 = expf(kqc[ (5 as i32)] - kqmn[ (0 as i32)]);
                kqc[ (5 as i32)] = e__u54;
                rsa[ (0 as i32)] += e__u54;
            }

            if true {
                let l__u48: i32 = 6;
                let idx__u55 = (l__u48 / 2) % 2;
                let e__u55 = expf(kqc[ (6 as i32)] - kqmn[ (1 as i32)]);
                kqc[ (6 as i32)] = e__u55;
                rsa[ (1 as i32)] += e__u55;
            }

            if true {
                let l__u48: i32 = 7;
                let idx__u56 = (l__u48 / 2) % 2;
                let e__u56 = expf(kqc[ (7 as i32)] - kqmn[ (1 as i32)]);
                kqc[ (7 as i32)] = e__u56;
                rsa[ (1 as i32)] += e__u56;
            }
            l__u48 = (8 as i32);
        }
        t = (1 as i32);
        // ---- rescale: KQ_max_scale=expf(KQ_max-KQ_max_new); FTZ bit-multiply for diff < -20;
        //      exact fused step KQ_rowsum = scale*KQ_rowsum + KQ_rowsum_add ----
        var scl: f32[2] = default < f32[2] > ();
        col = 0;

        if true {
            let col: i32 = 0;
            let diff__u57 = kqold[ (0 as i32)] - kqmn[ (0 as i32)];
            let s0__u57 = expf(diff__u57);
            let keep__u57: u32 = if diff__u57 >= -20.0 {
                1 as u32
            } else {
                0 as u32
            }
            ;
            scl[ (0 as i32)] = frombits(bits(s0__u57) * keep__u57);
            // *((uint32_t*)&scale) *= diff>=SOFTMAX_FTZ_THRESHOLD
            acc.k0 = frombits(bits(kqmn[ (0 as i32)]));
            let rs__u57 = frombits(bits(acc.s0));
            acc.s0 = frombits(bits(fmaf(scl[ (0 as i32)], rs__u57, rsa[ (0 as i32)])));
        }

        if true {
            let col: i32 = 1;
            let diff__u58 = kqold[ (1 as i32)] - kqmn[ (1 as i32)];
            let s0__u58 = expf(diff__u58);
            let keep__u58: u32 = if diff__u58 >= -20.0 {
                1 as u32
            } else {
                0 as u32
            }
            ;
            scl[ (1 as i32)] = frombits(bits(s0__u58) * keep__u58);
            // *((uint32_t*)&scale) *= diff>=SOFTMAX_FTZ_THRESHOLD
            acc.k1 = frombits(bits(kqmn[ (1 as i32)]));
            let rs__u58 = frombits(bits(acc.s1));
            acc.s1 = frombits(bits(fmaf(scl[ (1 as i32)], rs__u58, rsa[ (1 as i32)])));
        }
        col = (2 as i32);
        // ---- VKQ f16 accumulator rescale: VKQ_C[i].x[l0+col] *= make_half2(scale,scale) ----
        var sh2: u32[2] = default < u32[2] > ();
        col = 0;

        if true {
            let col: i32 = 0;
            sh2[ (0 as i32)] = h2(scl[ (0 as i32)], scl[ (0 as i32)]);
        }

        if true {
            let col: i32 = 1;
            sh2[ (1 as i32)] = h2(scl[ (1 as i32)], scl[ (1 as i32)]);
        }
        col = (2 as i32);
        var v: i32 = 0;

        if true {
            let v: i32 = 0;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u62: i32 = 0;

                if true {
                    let l0__u62: i32 = 0;
                    let e__u63 = v * 4 + l0__u62 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v0 = mulh2(acc.v0, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u62: i32 = 1;
                    let e__u64 = v * 4 + l0__u62 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v2 = mulh2(acc.v2, sh2[ (0 as i32)]);
                }
                l0__u62 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u65: i32 = 0;

                if true {
                    let l0__u65: i32 = 0;
                    let e__u66 = v * 4 + l0__u65 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v1 = mulh2(acc.v1, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u65: i32 = 1;
                    let e__u67 = v * 4 + l0__u65 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v3 = mulh2(acc.v3, sh2[ (1 as i32)]);
                }
                l0__u65 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 1;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u69: i32 = 0;

                if true {
                    let l0__u69: i32 = 0;
                    let e__u70 = v * 4 + l0__u69 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v4 = mulh2(acc.v4, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u69: i32 = 1;
                    let e__u71 = v * 4 + l0__u69 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v6 = mulh2(acc.v6, sh2[ (0 as i32)]);
                }
                l0__u69 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u72: i32 = 0;

                if true {
                    let l0__u72: i32 = 0;
                    let e__u73 = v * 4 + l0__u72 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v5 = mulh2(acc.v5, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u72: i32 = 1;
                    let e__u74 = v * 4 + l0__u72 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v7 = mulh2(acc.v7, sh2[ (1 as i32)]);
                }
                l0__u72 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 2;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u76: i32 = 0;

                if true {
                    let l0__u76: i32 = 0;
                    let e__u77 = v * 4 + l0__u76 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v8 = mulh2(acc.v8, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u76: i32 = 1;
                    let e__u78 = v * 4 + l0__u76 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v10 = mulh2(acc.v10, sh2[ (0 as i32)]);
                }
                l0__u76 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u79: i32 = 0;

                if true {
                    let l0__u79: i32 = 0;
                    let e__u80 = v * 4 + l0__u79 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v9 = mulh2(acc.v9, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u79: i32 = 1;
                    let e__u81 = v * 4 + l0__u79 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v11 = mulh2(acc.v11, sh2[ (1 as i32)]);
                }
                l0__u79 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 3;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u83: i32 = 0;

                if true {
                    let l0__u83: i32 = 0;
                    let e__u84 = v * 4 + l0__u83 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v12 = mulh2(acc.v12, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u83: i32 = 1;
                    let e__u85 = v * 4 + l0__u83 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v14 = mulh2(acc.v14, sh2[ (0 as i32)]);
                }
                l0__u83 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u86: i32 = 0;

                if true {
                    let l0__u86: i32 = 0;
                    let e__u87 = v * 4 + l0__u86 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v13 = mulh2(acc.v13, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u86: i32 = 1;
                    let e__u88 = v * 4 + l0__u86 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v15 = mulh2(acc.v15, sh2[ (1 as i32)]);
                }
                l0__u86 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 4;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u90: i32 = 0;

                if true {
                    let l0__u90: i32 = 0;
                    let e__u91 = v * 4 + l0__u90 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v16 = mulh2(acc.v16, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u90: i32 = 1;
                    let e__u92 = v * 4 + l0__u90 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v18 = mulh2(acc.v18, sh2[ (0 as i32)]);
                }
                l0__u90 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u93: i32 = 0;

                if true {
                    let l0__u93: i32 = 0;
                    let e__u94 = v * 4 + l0__u93 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v17 = mulh2(acc.v17, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u93: i32 = 1;
                    let e__u95 = v * 4 + l0__u93 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v19 = mulh2(acc.v19, sh2[ (1 as i32)]);
                }
                l0__u93 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 5;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u97: i32 = 0;

                if true {
                    let l0__u97: i32 = 0;
                    let e__u98 = v * 4 + l0__u97 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v20 = mulh2(acc.v20, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u97: i32 = 1;
                    let e__u99 = v * 4 + l0__u97 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v22 = mulh2(acc.v22, sh2[ (0 as i32)]);
                }
                l0__u97 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u100: i32 = 0;

                if true {
                    let l0__u100: i32 = 0;
                    let e__u101 = v * 4 + l0__u100 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v21 = mulh2(acc.v21, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u100: i32 = 1;
                    let e__u102 = v * 4 + l0__u100 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v23 = mulh2(acc.v23, sh2[ (1 as i32)]);
                }
                l0__u100 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 6;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u104: i32 = 0;

                if true {
                    let l0__u104: i32 = 0;
                    let e__u105 = v * 4 + l0__u104 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v24 = mulh2(acc.v24, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u104: i32 = 1;
                    let e__u106 = v * 4 + l0__u104 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v26 = mulh2(acc.v26, sh2[ (0 as i32)]);
                }
                l0__u104 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u107: i32 = 0;

                if true {
                    let l0__u107: i32 = 0;
                    let e__u108 = v * 4 + l0__u107 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v25 = mulh2(acc.v25, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u107: i32 = 1;
                    let e__u109 = v * 4 + l0__u107 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v27 = mulh2(acc.v27, sh2[ (1 as i32)]);
                }
                l0__u107 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 7;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u111: i32 = 0;

                if true {
                    let l0__u111: i32 = 0;
                    let e__u112 = v * 4 + l0__u111 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v28 = mulh2(acc.v28, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u111: i32 = 1;
                    let e__u113 = v * 4 + l0__u111 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v30 = mulh2(acc.v30, sh2[ (0 as i32)]);
                }
                l0__u111 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u114: i32 = 0;

                if true {
                    let l0__u114: i32 = 0;
                    let e__u115 = v * 4 + l0__u114 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v29 = mulh2(acc.v29, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u114: i32 = 1;
                    let e__u116 = v * 4 + l0__u114 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v31 = mulh2(acc.v31, sh2[ (1 as i32)]);
                }
                l0__u114 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 8;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u118: i32 = 0;

                if true {
                    let l0__u118: i32 = 0;
                    let e__u119 = v * 4 + l0__u118 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v32 = mulh2(acc.v32, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u118: i32 = 1;
                    let e__u120 = v * 4 + l0__u118 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v34 = mulh2(acc.v34, sh2[ (0 as i32)]);
                }
                l0__u118 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u121: i32 = 0;

                if true {
                    let l0__u121: i32 = 0;
                    let e__u122 = v * 4 + l0__u121 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v33 = mulh2(acc.v33, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u121: i32 = 1;
                    let e__u123 = v * 4 + l0__u121 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v35 = mulh2(acc.v35, sh2[ (1 as i32)]);
                }
                l0__u121 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 9;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u125: i32 = 0;

                if true {
                    let l0__u125: i32 = 0;
                    let e__u126 = v * 4 + l0__u125 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v36 = mulh2(acc.v36, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u125: i32 = 1;
                    let e__u127 = v * 4 + l0__u125 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v38 = mulh2(acc.v38, sh2[ (0 as i32)]);
                }
                l0__u125 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u128: i32 = 0;

                if true {
                    let l0__u128: i32 = 0;
                    let e__u129 = v * 4 + l0__u128 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v37 = mulh2(acc.v37, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u128: i32 = 1;
                    let e__u130 = v * 4 + l0__u128 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v39 = mulh2(acc.v39, sh2[ (1 as i32)]);
                }
                l0__u128 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 10;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u132: i32 = 0;

                if true {
                    let l0__u132: i32 = 0;
                    let e__u133 = v * 4 + l0__u132 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v40 = mulh2(acc.v40, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u132: i32 = 1;
                    let e__u134 = v * 4 + l0__u132 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v42 = mulh2(acc.v42, sh2[ (0 as i32)]);
                }
                l0__u132 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u135: i32 = 0;

                if true {
                    let l0__u135: i32 = 0;
                    let e__u136 = v * 4 + l0__u135 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v41 = mulh2(acc.v41, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u135: i32 = 1;
                    let e__u137 = v * 4 + l0__u135 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v43 = mulh2(acc.v43, sh2[ (1 as i32)]);
                }
                l0__u135 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 11;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u139: i32 = 0;

                if true {
                    let l0__u139: i32 = 0;
                    let e__u140 = v * 4 + l0__u139 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v44 = mulh2(acc.v44, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u139: i32 = 1;
                    let e__u141 = v * 4 + l0__u139 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v46 = mulh2(acc.v46, sh2[ (0 as i32)]);
                }
                l0__u139 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u142: i32 = 0;

                if true {
                    let l0__u142: i32 = 0;
                    let e__u143 = v * 4 + l0__u142 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v45 = mulh2(acc.v45, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u142: i32 = 1;
                    let e__u144 = v * 4 + l0__u142 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v47 = mulh2(acc.v47, sh2[ (1 as i32)]);
                }
                l0__u142 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 12;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u146: i32 = 0;

                if true {
                    let l0__u146: i32 = 0;
                    let e__u147 = v * 4 + l0__u146 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v48 = mulh2(acc.v48, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u146: i32 = 1;
                    let e__u148 = v * 4 + l0__u146 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v50 = mulh2(acc.v50, sh2[ (0 as i32)]);
                }
                l0__u146 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u149: i32 = 0;

                if true {
                    let l0__u149: i32 = 0;
                    let e__u150 = v * 4 + l0__u149 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v49 = mulh2(acc.v49, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u149: i32 = 1;
                    let e__u151 = v * 4 + l0__u149 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v51 = mulh2(acc.v51, sh2[ (1 as i32)]);
                }
                l0__u149 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 13;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u153: i32 = 0;

                if true {
                    let l0__u153: i32 = 0;
                    let e__u154 = v * 4 + l0__u153 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v52 = mulh2(acc.v52, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u153: i32 = 1;
                    let e__u155 = v * 4 + l0__u153 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v54 = mulh2(acc.v54, sh2[ (0 as i32)]);
                }
                l0__u153 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u156: i32 = 0;

                if true {
                    let l0__u156: i32 = 0;
                    let e__u157 = v * 4 + l0__u156 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v53 = mulh2(acc.v53, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u156: i32 = 1;
                    let e__u158 = v * 4 + l0__u156 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v55 = mulh2(acc.v55, sh2[ (1 as i32)]);
                }
                l0__u156 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 14;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u160: i32 = 0;

                if true {
                    let l0__u160: i32 = 0;
                    let e__u161 = v * 4 + l0__u160 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v56 = mulh2(acc.v56, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u160: i32 = 1;
                    let e__u162 = v * 4 + l0__u160 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v58 = mulh2(acc.v58, sh2[ (0 as i32)]);
                }
                l0__u160 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u163: i32 = 0;

                if true {
                    let l0__u163: i32 = 0;
                    let e__u164 = v * 4 + l0__u163 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v57 = mulh2(acc.v57, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u163: i32 = 1;
                    let e__u165 = v * 4 + l0__u163 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v59 = mulh2(acc.v59, sh2[ (1 as i32)]);
                }
                l0__u163 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 15;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u167: i32 = 0;

                if true {
                    let l0__u167: i32 = 0;
                    let e__u168 = v * 4 + l0__u167 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v60 = mulh2(acc.v60, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u167: i32 = 1;
                    let e__u169 = v * 4 + l0__u167 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v62 = mulh2(acc.v62, sh2[ (0 as i32)]);
                }
                l0__u167 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u170: i32 = 0;

                if true {
                    let l0__u170: i32 = 0;
                    let e__u171 = v * 4 + l0__u170 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v61 = mulh2(acc.v61, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u170: i32 = 1;
                    let e__u172 = v * 4 + l0__u170 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v63 = mulh2(acc.v63, sh2[ (1 as i32)]);
                }
                l0__u170 = (2 as i32);
            }
            col = (2 as i32);
        }
        v = (16 as i32);
        // ---- B[k] = get_half2(KQ_C[k]): pure F32->half2 pack on Turing, no transpose/shuffle ----
        var b: u32[8] = default < u32[8] > ();
        t = 0;

        if true {
            let t: i32 = 0;
            var l0__u173: i32 = 0;

            if true {
                let l0__u173: i32 = 0;
                b[ (0 as i32)] = h2(kqc[ (0 as i32)], kqc[ (1 as i32)]);
            }

            if true {
                let l0__u173: i32 = 2;
                b[ (1 as i32)] = h2(kqc[ (2 as i32)], kqc[ (3 as i32)]);
            }

            if true {
                let l0__u173: i32 = 4;
                b[ (2 as i32)] = h2(kqc[ (4 as i32)], kqc[ (5 as i32)]);
            }

            if true {
                let l0__u173: i32 = 6;
                b[ (3 as i32)] = h2(kqc[ (6 as i32)], kqc[ (7 as i32)]);
            }
            l0__u173 = (8 as i32);
        }
        t = (1 as i32);
        // ---- double-stage tail: wait the V tile, sync, then preload next K + mask before the current
        //      VKQ mma (skipped only on the last iteration; waits/barriers stay unconditional) ----
        cpwait();
        barrier();

        if !last {
            load_mask(mask_h, mt, stride_mask, kv_row0 + 32, query0, n_queries, ncols1, tx, ty);
            load_kv(k_h2, kt, stride_k, kv_row0 + 32, tx, ty);
        }
        // ---- VKQ += probs @ V^T, swapped operands (A=B_probs, B=A_V trans-V fragment); f16
        //      accumulation with B registers 0,2 -> D.x[0..1] then 1,3 -> D.x[2..3] ----
        v = 0;

        if true {
            let v: i32 = 0;
            var d0__u178: U2 = U2 {
                acc.v0, acc.v1
            }
            ;
            var d1__u178: U2 = U2 {
                acc.v2, acc.v3
            }
            ;
            var kk__u178: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u178: i32 = 0;
                let k0__u179 = kk__u178 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u179 = ldm4t_tile(ldm_addr(vt, 2 * k0__u179 * 128 + v * 8, tx));
                d0__u178 = mma_f16(
                    d0__u178,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u179.a,
                    av__u179.c);
                d1__u178 = mma_f16(
                    d1__u178,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u179.b,
                    av__u179.d);
            }
            kk__u178 = (1 as i32);
            acc.v0 = d0__u178.a;
            acc.v1 = d0__u178.b;
            acc.v2 = d1__u178.a;
            acc.v3 = d1__u178.b;
        }

        if true {
            let v: i32 = 1;
            var d0__u180: U2 = U2 {
                acc.v4, acc.v5
            }
            ;
            var d1__u180: U2 = U2 {
                acc.v6, acc.v7
            }
            ;
            var kk__u180: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u180: i32 = 0;
                let k0__u181 = kk__u180 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u181 = ldm4t_tile(ldm_addr(vt, 2 * k0__u181 * 128 + v * 8, tx));
                d0__u180 = mma_f16(
                    d0__u180,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u181.a,
                    av__u181.c);
                d1__u180 = mma_f16(
                    d1__u180,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u181.b,
                    av__u181.d);
            }
            kk__u180 = (1 as i32);
            acc.v4 = d0__u180.a;
            acc.v5 = d0__u180.b;
            acc.v6 = d1__u180.a;
            acc.v7 = d1__u180.b;
        }

        if true {
            let v: i32 = 2;
            var d0__u182: U2 = U2 {
                acc.v8, acc.v9
            }
            ;
            var d1__u182: U2 = U2 {
                acc.v10, acc.v11
            }
            ;
            var kk__u182: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u182: i32 = 0;
                let k0__u183 = kk__u182 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u183 = ldm4t_tile(ldm_addr(vt, 2 * k0__u183 * 128 + v * 8, tx));
                d0__u182 = mma_f16(
                    d0__u182,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u183.a,
                    av__u183.c);
                d1__u182 = mma_f16(
                    d1__u182,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u183.b,
                    av__u183.d);
            }
            kk__u182 = (1 as i32);
            acc.v8 = d0__u182.a;
            acc.v9 = d0__u182.b;
            acc.v10 = d1__u182.a;
            acc.v11 = d1__u182.b;
        }

        if true {
            let v: i32 = 3;
            var d0__u184: U2 = U2 {
                acc.v12, acc.v13
            }
            ;
            var d1__u184: U2 = U2 {
                acc.v14, acc.v15
            }
            ;
            var kk__u184: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u184: i32 = 0;
                let k0__u185 = kk__u184 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u185 = ldm4t_tile(ldm_addr(vt, 2 * k0__u185 * 128 + v * 8, tx));
                d0__u184 = mma_f16(
                    d0__u184,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u185.a,
                    av__u185.c);
                d1__u184 = mma_f16(
                    d1__u184,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u185.b,
                    av__u185.d);
            }
            kk__u184 = (1 as i32);
            acc.v12 = d0__u184.a;
            acc.v13 = d0__u184.b;
            acc.v14 = d1__u184.a;
            acc.v15 = d1__u184.b;
        }

        if true {
            let v: i32 = 4;
            var d0__u186: U2 = U2 {
                acc.v16, acc.v17
            }
            ;
            var d1__u186: U2 = U2 {
                acc.v18, acc.v19
            }
            ;
            var kk__u186: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u186: i32 = 0;
                let k0__u187 = kk__u186 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u187 = ldm4t_tile(ldm_addr(vt, 2 * k0__u187 * 128 + v * 8, tx));
                d0__u186 = mma_f16(
                    d0__u186,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u187.a,
                    av__u187.c);
                d1__u186 = mma_f16(
                    d1__u186,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u187.b,
                    av__u187.d);
            }
            kk__u186 = (1 as i32);
            acc.v16 = d0__u186.a;
            acc.v17 = d0__u186.b;
            acc.v18 = d1__u186.a;
            acc.v19 = d1__u186.b;
        }

        if true {
            let v: i32 = 5;
            var d0__u188: U2 = U2 {
                acc.v20, acc.v21
            }
            ;
            var d1__u188: U2 = U2 {
                acc.v22, acc.v23
            }
            ;
            var kk__u188: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u188: i32 = 0;
                let k0__u189 = kk__u188 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u189 = ldm4t_tile(ldm_addr(vt, 2 * k0__u189 * 128 + v * 8, tx));
                d0__u188 = mma_f16(
                    d0__u188,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u189.a,
                    av__u189.c);
                d1__u188 = mma_f16(
                    d1__u188,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u189.b,
                    av__u189.d);
            }
            kk__u188 = (1 as i32);
            acc.v20 = d0__u188.a;
            acc.v21 = d0__u188.b;
            acc.v22 = d1__u188.a;
            acc.v23 = d1__u188.b;
        }

        if true {
            let v: i32 = 6;
            var d0__u190: U2 = U2 {
                acc.v24, acc.v25
            }
            ;
            var d1__u190: U2 = U2 {
                acc.v26, acc.v27
            }
            ;
            var kk__u190: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u190: i32 = 0;
                let k0__u191 = kk__u190 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u191 = ldm4t_tile(ldm_addr(vt, 2 * k0__u191 * 128 + v * 8, tx));
                d0__u190 = mma_f16(
                    d0__u190,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u191.a,
                    av__u191.c);
                d1__u190 = mma_f16(
                    d1__u190,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u191.b,
                    av__u191.d);
            }
            kk__u190 = (1 as i32);
            acc.v24 = d0__u190.a;
            acc.v25 = d0__u190.b;
            acc.v26 = d1__u190.a;
            acc.v27 = d1__u190.b;
        }

        if true {
            let v: i32 = 7;
            var d0__u192: U2 = U2 {
                acc.v28, acc.v29
            }
            ;
            var d1__u192: U2 = U2 {
                acc.v30, acc.v31
            }
            ;
            var kk__u192: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u192: i32 = 0;
                let k0__u193 = kk__u192 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u193 = ldm4t_tile(ldm_addr(vt, 2 * k0__u193 * 128 + v * 8, tx));
                d0__u192 = mma_f16(
                    d0__u192,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u193.a,
                    av__u193.c);
                d1__u192 = mma_f16(
                    d1__u192,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u193.b,
                    av__u193.d);
            }
            kk__u192 = (1 as i32);
            acc.v28 = d0__u192.a;
            acc.v29 = d0__u192.b;
            acc.v30 = d1__u192.a;
            acc.v31 = d1__u192.b;
        }

        if true {
            let v: i32 = 8;
            var d0__u194: U2 = U2 {
                acc.v32, acc.v33
            }
            ;
            var d1__u194: U2 = U2 {
                acc.v34, acc.v35
            }
            ;
            var kk__u194: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u194: i32 = 0;
                let k0__u195 = kk__u194 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u195 = ldm4t_tile(ldm_addr(vt, 2 * k0__u195 * 128 + v * 8, tx));
                d0__u194 = mma_f16(
                    d0__u194,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u195.a,
                    av__u195.c);
                d1__u194 = mma_f16(
                    d1__u194,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u195.b,
                    av__u195.d);
            }
            kk__u194 = (1 as i32);
            acc.v32 = d0__u194.a;
            acc.v33 = d0__u194.b;
            acc.v34 = d1__u194.a;
            acc.v35 = d1__u194.b;
        }

        if true {
            let v: i32 = 9;
            var d0__u196: U2 = U2 {
                acc.v36, acc.v37
            }
            ;
            var d1__u196: U2 = U2 {
                acc.v38, acc.v39
            }
            ;
            var kk__u196: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u196: i32 = 0;
                let k0__u197 = kk__u196 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u197 = ldm4t_tile(ldm_addr(vt, 2 * k0__u197 * 128 + v * 8, tx));
                d0__u196 = mma_f16(
                    d0__u196,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u197.a,
                    av__u197.c);
                d1__u196 = mma_f16(
                    d1__u196,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u197.b,
                    av__u197.d);
            }
            kk__u196 = (1 as i32);
            acc.v36 = d0__u196.a;
            acc.v37 = d0__u196.b;
            acc.v38 = d1__u196.a;
            acc.v39 = d1__u196.b;
        }

        if true {
            let v: i32 = 10;
            var d0__u198: U2 = U2 {
                acc.v40, acc.v41
            }
            ;
            var d1__u198: U2 = U2 {
                acc.v42, acc.v43
            }
            ;
            var kk__u198: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u198: i32 = 0;
                let k0__u199 = kk__u198 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u199 = ldm4t_tile(ldm_addr(vt, 2 * k0__u199 * 128 + v * 8, tx));
                d0__u198 = mma_f16(
                    d0__u198,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u199.a,
                    av__u199.c);
                d1__u198 = mma_f16(
                    d1__u198,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u199.b,
                    av__u199.d);
            }
            kk__u198 = (1 as i32);
            acc.v40 = d0__u198.a;
            acc.v41 = d0__u198.b;
            acc.v42 = d1__u198.a;
            acc.v43 = d1__u198.b;
        }

        if true {
            let v: i32 = 11;
            var d0__u200: U2 = U2 {
                acc.v44, acc.v45
            }
            ;
            var d1__u200: U2 = U2 {
                acc.v46, acc.v47
            }
            ;
            var kk__u200: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u200: i32 = 0;
                let k0__u201 = kk__u200 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u201 = ldm4t_tile(ldm_addr(vt, 2 * k0__u201 * 128 + v * 8, tx));
                d0__u200 = mma_f16(
                    d0__u200,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u201.a,
                    av__u201.c);
                d1__u200 = mma_f16(
                    d1__u200,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u201.b,
                    av__u201.d);
            }
            kk__u200 = (1 as i32);
            acc.v44 = d0__u200.a;
            acc.v45 = d0__u200.b;
            acc.v46 = d1__u200.a;
            acc.v47 = d1__u200.b;
        }

        if true {
            let v: i32 = 12;
            var d0__u202: U2 = U2 {
                acc.v48, acc.v49
            }
            ;
            var d1__u202: U2 = U2 {
                acc.v50, acc.v51
            }
            ;
            var kk__u202: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u202: i32 = 0;
                let k0__u203 = kk__u202 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u203 = ldm4t_tile(ldm_addr(vt, 2 * k0__u203 * 128 + v * 8, tx));
                d0__u202 = mma_f16(
                    d0__u202,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u203.a,
                    av__u203.c);
                d1__u202 = mma_f16(
                    d1__u202,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u203.b,
                    av__u203.d);
            }
            kk__u202 = (1 as i32);
            acc.v48 = d0__u202.a;
            acc.v49 = d0__u202.b;
            acc.v50 = d1__u202.a;
            acc.v51 = d1__u202.b;
        }

        if true {
            let v: i32 = 13;
            var d0__u204: U2 = U2 {
                acc.v52, acc.v53
            }
            ;
            var d1__u204: U2 = U2 {
                acc.v54, acc.v55
            }
            ;
            var kk__u204: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u204: i32 = 0;
                let k0__u205 = kk__u204 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u205 = ldm4t_tile(ldm_addr(vt, 2 * k0__u205 * 128 + v * 8, tx));
                d0__u204 = mma_f16(
                    d0__u204,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u205.a,
                    av__u205.c);
                d1__u204 = mma_f16(
                    d1__u204,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u205.b,
                    av__u205.d);
            }
            kk__u204 = (1 as i32);
            acc.v52 = d0__u204.a;
            acc.v53 = d0__u204.b;
            acc.v54 = d1__u204.a;
            acc.v55 = d1__u204.b;
        }

        if true {
            let v: i32 = 14;
            var d0__u206: U2 = U2 {
                acc.v56, acc.v57
            }
            ;
            var d1__u206: U2 = U2 {
                acc.v58, acc.v59
            }
            ;
            var kk__u206: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u206: i32 = 0;
                let k0__u207 = kk__u206 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u207 = ldm4t_tile(ldm_addr(vt, 2 * k0__u207 * 128 + v * 8, tx));
                d0__u206 = mma_f16(
                    d0__u206,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u207.a,
                    av__u207.c);
                d1__u206 = mma_f16(
                    d1__u206,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u207.b,
                    av__u207.d);
            }
            kk__u206 = (1 as i32);
            acc.v56 = d0__u206.a;
            acc.v57 = d0__u206.b;
            acc.v58 = d1__u206.a;
            acc.v59 = d1__u206.b;
        }

        if true {
            let v: i32 = 15;
            var d0__u208: U2 = U2 {
                acc.v60, acc.v61
            }
            ;
            var d1__u208: U2 = U2 {
                acc.v62, acc.v63
            }
            ;
            var kk__u208: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u208: i32 = 0;
                let k0__u209 = kk__u208 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u209 = ldm4t_tile(ldm_addr(vt, 2 * k0__u209 * 128 + v * 8, tx));
                d0__u208 = mma_f16(
                    d0__u208,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u209.a,
                    av__u209.c);
                d1__u208 = mma_f16(
                    d1__u208,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u209.b,
                    av__u209.d);
            }
            kk__u208 = (1 as i32);
            acc.v60 = d0__u208.a;
            acc.v61 = d0__u208.b;
            acc.v62 = d1__u208.a;
            acc.v63 = d1__u208.b;
        }
        v = (16 as i32);
    }
    barrier();
    // L1394-1396: nstages>1 && nwarps*cols_per_warp(64)>nbatch_fa(32)
    // ---- Sum partial KQ rowsums (L1399-1424): TURING+ path, cols_per_warp!=8 -> offsets {2,1}, width 32.
    var col: i32 = 0;

    if true {
        let col: i32 = (0 as i32);
        var v = acc.s0;
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
        acc.s0 = v;
    }

    if true {
        let col: i32 = (1 as i32);
        var v = acc.s1;
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
        acc.s1 = v;
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
        let n0 = fmaxf2(acc.k0, s0);
        let d0 = acc.k0 - n0;
        ks0 = expf(d0);
        acc.k0 = n0;
        // *((u32*)&scale)*= (d>=SOFTMAX_FTZ_THRESHOLD) with SOFTMAX_FTZ_THRESHOLD=-20.0f, as select.

        if d0 < -20.0 {
            ks0 = 0.0;
        }
        acc.s0 = ks0 * acc.s0 + expf(s0 - n0);
        let n1 = fmaxf2(acc.k1, s1);
        let d1 = acc.k1 - n1;
        ks1 = expf(d1);
        acc.k1 = n1;

        if d1 < -20.0 {
            ks1 = 0.0;
        }
        acc.s1 = ks1 * acc.s1 + expf(s1 - n1);
        // VKQ_C rescale (L1458-1469, cols_per_warp!=8): per col scale x[l0+col], l0 in {0,2}.
        let h0 = h2pack(ks0, ks0);
        let h1 = h2pack(ks1, ks1);
        var i0: i32 = 0;

        if true {
            let i0: i32 = (0 as i32);
            acc.v0 = h2mul(acc.v0, h0);
            acc.v2 = h2mul(acc.v2, h0);
            acc.v1 = h2mul(acc.v1, h1);
            acc.v3 = h2mul(acc.v3, h1);
        }

        if true {
            let i0: i32 = (1 as i32);
            acc.v4 = h2mul(acc.v4, h0);
            acc.v6 = h2mul(acc.v6, h0);
            acc.v5 = h2mul(acc.v5, h1);
            acc.v7 = h2mul(acc.v7, h1);
        }

        if true {
            let i0: i32 = (2 as i32);
            acc.v8 = h2mul(acc.v8, h0);
            acc.v10 = h2mul(acc.v10, h0);
            acc.v9 = h2mul(acc.v9, h1);
            acc.v11 = h2mul(acc.v11, h1);
        }

        if true {
            let i0: i32 = (3 as i32);
            acc.v12 = h2mul(acc.v12, h0);
            acc.v14 = h2mul(acc.v14, h0);
            acc.v13 = h2mul(acc.v13, h1);
            acc.v15 = h2mul(acc.v15, h1);
        }

        if true {
            let i0: i32 = (4 as i32);
            acc.v16 = h2mul(acc.v16, h0);
            acc.v18 = h2mul(acc.v18, h0);
            acc.v17 = h2mul(acc.v17, h1);
            acc.v19 = h2mul(acc.v19, h1);
        }

        if true {
            let i0: i32 = (5 as i32);
            acc.v20 = h2mul(acc.v20, h0);
            acc.v22 = h2mul(acc.v22, h0);
            acc.v21 = h2mul(acc.v21, h1);
            acc.v23 = h2mul(acc.v23, h1);
        }

        if true {
            let i0: i32 = (6 as i32);
            acc.v24 = h2mul(acc.v24, h0);
            acc.v26 = h2mul(acc.v26, h0);
            acc.v25 = h2mul(acc.v25, h1);
            acc.v27 = h2mul(acc.v27, h1);
        }

        if true {
            let i0: i32 = (7 as i32);
            acc.v28 = h2mul(acc.v28, h0);
            acc.v30 = h2mul(acc.v30, h0);
            acc.v29 = h2mul(acc.v29, h1);
            acc.v31 = h2mul(acc.v31, h1);
        }

        if true {
            let i0: i32 = (8 as i32);
            acc.v32 = h2mul(acc.v32, h0);
            acc.v34 = h2mul(acc.v34, h0);
            acc.v33 = h2mul(acc.v33, h1);
            acc.v35 = h2mul(acc.v35, h1);
        }

        if true {
            let i0: i32 = (9 as i32);
            acc.v36 = h2mul(acc.v36, h0);
            acc.v38 = h2mul(acc.v38, h0);
            acc.v37 = h2mul(acc.v37, h1);
            acc.v39 = h2mul(acc.v39, h1);
        }

        if true {
            let i0: i32 = (10 as i32);
            acc.v40 = h2mul(acc.v40, h0);
            acc.v42 = h2mul(acc.v42, h0);
            acc.v41 = h2mul(acc.v41, h1);
            acc.v43 = h2mul(acc.v43, h1);
        }

        if true {
            let i0: i32 = (11 as i32);
            acc.v44 = h2mul(acc.v44, h0);
            acc.v46 = h2mul(acc.v46, h0);
            acc.v45 = h2mul(acc.v45, h1);
            acc.v47 = h2mul(acc.v47, h1);
        }

        if true {
            let i0: i32 = (12 as i32);
            acc.v48 = h2mul(acc.v48, h0);
            acc.v50 = h2mul(acc.v50, h0);
            acc.v49 = h2mul(acc.v49, h1);
            acc.v51 = h2mul(acc.v51, h1);
        }

        if true {
            let i0: i32 = (13 as i32);
            acc.v52 = h2mul(acc.v52, h0);
            acc.v54 = h2mul(acc.v54, h0);
            acc.v53 = h2mul(acc.v53, h1);
            acc.v55 = h2mul(acc.v55, h1);
        }

        if true {
            let i0: i32 = (14 as i32);
            acc.v56 = h2mul(acc.v56, h0);
            acc.v58 = h2mul(acc.v58, h0);
            acc.v57 = h2mul(acc.v57, h1);
            acc.v59 = h2mul(acc.v59, h1);
        }

        if true {
            let i0: i32 = (15 as i32);
            acc.v60 = h2mul(acc.v60, h0);
            acc.v62 = h2mul(acc.v62, h0);
            acc.v61 = h2mul(acc.v61, h1);
            acc.v63 = h2mul(acc.v63, h1);
        }
        i0 = (16 as i32);
    }
    // ---- Combine meta (L1503-1576): swz=true -> barrier first (L1555-1557); jc_cwm uses
    // tile<16,8,half2> get_i(tx%4)=((tx%2)*8)+tx/4; meta floats at tile_Q row padding word 128/129.
    barrier();
    let jc_cwm = ty * 16 + gi8(tx % 4, tx);
    let cmrx = (if tx % 2 == 0 {
        acc.k0
    } else {
        acc.k1
    }
    );
    let cmry = (if tx % 2 == 0 {
        acc.s0
    } else {
        acc.s1
    }
    );
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
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v0);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v1);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v2);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v3);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (8 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v4);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v5);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v6);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v7);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (16 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v8);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v9);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v10);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v11);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (24 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v12);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v13);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v14);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v15);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (32 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v16);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v17);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v18);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v19);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (40 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v20);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v21);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v22);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v23);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (48 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v24);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v25);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v26);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v27);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (56 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v28);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v29);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v30);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v31);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (64 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v32);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v33);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v34);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v35);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (72 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v36);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v37);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v38);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v39);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (80 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v40);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v41);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v42);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v43);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (88 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v44);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v45);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v46);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v47);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (96 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v48);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v49);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v50);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v51);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (104 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v52);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v53);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v54);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v55);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (112 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v56);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v57);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v58);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v59);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (120 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v60);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v61);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v62);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v63);
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
    var qr: QueryRegs = QueryRegs {
        0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32
    }
    ;
    var acc: Accumulator = Accumulator {
        0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0 as u32, 0.0 as f32, 0.0 as f32, 0.0 as f32, 0.0 as f32
    }
    ;
    let ncols1: i32 = (8 as i32);
    let np: i32 = (1 as i32);
    // ncols2=8 fixed; nbatch_fa=32; DKQ/2=nbatch_K2=nbatch_V2=nbatch_combine=128; tile_stride=132.
    let ncols = ncols1 * 8;
    // KQ_max init -FLT_MAX/2 (L1272) with exact f32 bits: 2^127-2^103 = (2-2^-23)*2^126, computed
    // from exact powers of two (no bitcast primitive exists or is assumed).
    let fltmax2: f32 = 170141173319264429905852091742258332672.0;
    acc.k0 = 0.0 - fltmax2;
    acc.k1 = 0.0 - fltmax2;
    acc.s0 = 0.0;
    acc.s1 = 0.0;
    var zi: i32 = 0;

    if true {
        let zi: i32 = (0 as i32);
        acc.v0 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (1 as i32);
        acc.v1 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (2 as i32);
        acc.v2 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (3 as i32);
        acc.v3 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (4 as i32);
        acc.v4 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (5 as i32);
        acc.v5 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (6 as i32);
        acc.v6 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (7 as i32);
        acc.v7 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (8 as i32);
        acc.v8 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (9 as i32);
        acc.v9 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (10 as i32);
        acc.v10 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (11 as i32);
        acc.v11 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (12 as i32);
        acc.v12 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (13 as i32);
        acc.v13 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (14 as i32);
        acc.v14 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (15 as i32);
        acc.v15 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (16 as i32);
        acc.v16 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (17 as i32);
        acc.v17 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (18 as i32);
        acc.v18 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (19 as i32);
        acc.v19 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (20 as i32);
        acc.v20 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (21 as i32);
        acc.v21 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (22 as i32);
        acc.v22 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (23 as i32);
        acc.v23 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (24 as i32);
        acc.v24 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (25 as i32);
        acc.v25 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (26 as i32);
        acc.v26 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (27 as i32);
        acc.v27 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (28 as i32);
        acc.v28 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (29 as i32);
        acc.v29 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (30 as i32);
        acc.v30 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (31 as i32);
        acc.v31 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (32 as i32);
        acc.v32 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (33 as i32);
        acc.v33 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (34 as i32);
        acc.v34 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (35 as i32);
        acc.v35 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (36 as i32);
        acc.v36 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (37 as i32);
        acc.v37 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (38 as i32);
        acc.v38 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (39 as i32);
        acc.v39 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (40 as i32);
        acc.v40 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (41 as i32);
        acc.v41 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (42 as i32);
        acc.v42 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (43 as i32);
        acc.v43 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (44 as i32);
        acc.v44 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (45 as i32);
        acc.v45 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (46 as i32);
        acc.v46 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (47 as i32);
        acc.v47 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (48 as i32);
        acc.v48 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (49 as i32);
        acc.v49 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (50 as i32);
        acc.v50 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (51 as i32);
        acc.v51 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (52 as i32);
        acc.v52 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (53 as i32);
        acc.v53 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (54 as i32);
        acc.v54 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (55 as i32);
        acc.v55 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (56 as i32);
        acc.v56 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (57 as i32);
        acc.v57 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (58 as i32);
        acc.v58 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (59 as i32);
        acc.v59 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (60 as i32);
        acc.v60 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (61 as i32);
        acc.v61 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (62 as i32);
        acc.v62 = h2pack(0.0, 0.0);
    }

    if true {
        let zi: i32 = (63 as i32);
        acc.v63 = h2pack(0.0, 0.0);
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
        qr.q0 = r.a;
        qr.q1 = r.b;
        qr.q2 = r.c;
        qr.q3 = r.d;
    }

    if true {
        let q0: i32 = (8 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q4 = r.a;
        qr.q5 = r.b;
        qr.q6 = r.c;
        qr.q7 = r.d;
    }

    if true {
        let q0: i32 = (16 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q8 = r.a;
        qr.q9 = r.b;
        qr.q10 = r.c;
        qr.q11 = r.d;
    }

    if true {
        let q0: i32 = (24 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q12 = r.a;
        qr.q13 = r.b;
        qr.q14 = r.c;
        qr.q15 = r.d;
    }

    if true {
        let q0: i32 = (32 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q16 = r.a;
        qr.q17 = r.b;
        qr.q18 = r.c;
        qr.q19 = r.d;
    }

    if true {
        let q0: i32 = (40 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q20 = r.a;
        qr.q21 = r.b;
        qr.q22 = r.c;
        qr.q23 = r.d;
    }

    if true {
        let q0: i32 = (48 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q24 = r.a;
        qr.q25 = r.b;
        qr.q26 = r.c;
        qr.q27 = r.d;
    }

    if true {
        let q0: i32 = (56 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q28 = r.a;
        qr.q29 = r.b;
        qr.q30 = r.c;
        qr.q31 = r.d;
    }

    if true {
        let q0: i32 = (64 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q32 = r.a;
        qr.q33 = r.b;
        qr.q34 = r.c;
        qr.q35 = r.d;
    }

    if true {
        let q0: i32 = (72 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q36 = r.a;
        qr.q37 = r.b;
        qr.q38 = r.c;
        qr.q39 = r.d;
    }

    if true {
        let q0: i32 = (80 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q40 = r.a;
        qr.q41 = r.b;
        qr.q42 = r.c;
        qr.q43 = r.d;
    }

    if true {
        let q0: i32 = (88 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q44 = r.a;
        qr.q45 = r.b;
        qr.q46 = r.c;
        qr.q47 = r.d;
    }

    if true {
        let q0: i32 = (96 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q48 = r.a;
        qr.q49 = r.b;
        qr.q50 = r.c;
        qr.q51 = r.d;
    }

    if true {
        let q0: i32 = (104 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q52 = r.a;
        qr.q53 = r.b;
        qr.q54 = r.c;
        qr.q55 = r.d;
    }

    if true {
        let q0: i32 = (112 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q56 = r.a;
        qr.q57 = r.b;
        qr.q58 = r.c;
        qr.q59 = r.d;
    }

    if true {
        let q0: i32 = (120 as i32);
        let w = j0 * 132 + q0 + (tx % 16) * 132 + (tx / 16) * 4;
        let r = ldm4(smem + (w as u64) * (4 as u64));
        qr.q60 = r.a;
        qr.q61 = r.b;
        qr.q62 = r.c;
        qr.q63 = r.d;
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
        if true {
            let n_queries: i64 = ne01z;
            let last: bool = false;
            let ncols1: i32 = 8;
            let np: i32 = 1;
            let kt = smem;
            let vt = smem + (16384 as u64);
            let mt = smem + (32768 as u64);
            let kv_row0 = (kb0 as i64) * 32;
            // k_VKQ_0 = kb0*nbatch_fa
            let query0 = (jt as i64) * (ncols1 as i64);
            // j0 for the mask loader
            let cb = (ty / np) * 16;
            // warp Q-column block base (cols_per_warp=16)
            let rh = (ty % np) * 16;
            // warp KV-row half base (np*T_A_KQ::I split)
            let nt = 32 / (np * 16);
            // KQ_C/B tiles per warp: 1 (np=2) or 2 (np=1)
            let kqoff = (3.0 as f32) * (0.6931 as f32);
            // FATTN_KQ_MAX_OFFSET
            var t: i32 = 0;
            // nstages>1 entry: this iteration's K tile + mask were preloaded by the prologue or the previous
            // iter tail; wait for them, then issue the V tile of this iteration into the pipeline.
            cpwait();
            barrier();
            load_kv(v_h2, vt, stride_v, kv_row0, tx, ty);
            // ---- KQ = Q_B @ K_A over the single nbatch_K2 block k in [0,128) half2 (16 x T_A_KQ::J=8) ----
            var kqc: f32[16] = default < f32[16] > ();
            // up to 2 fresh zero tile<16,16,float> C tiles
            var q: i32 = 0;
            // Q_B tile index = k_KQ_0/8

            if true {
                let q: i32 = 0;
                let q0__u1 = qr.q0;
                let q1__u1 = qr.q1;
                let q2__u1 = qr.q2;
                let q3__u1 = qr.q3;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u2 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u2 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u1, q1__u1, q2__u1, q3__u1, ka__u2.a, ka__u2.c);
                    kqc[ (0 as i32)] = lo__u2.x;
                    kqc[ (1 as i32)] = lo__u2.y;
                    kqc[ (2 as i32)] = lo__u2.z;
                    kqc[ (3 as i32)] = lo__u2.w;
                    let hi__u2 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u1, q1__u1, q2__u1, q3__u1, ka__u2.b, ka__u2.d);
                    kqc[ (4 as i32)] = hi__u2.x;
                    kqc[ (5 as i32)] = hi__u2.y;
                    kqc[ (6 as i32)] = hi__u2.z;
                    kqc[ (7 as i32)] = hi__u2.w;
                }

                if true {
                    let t: i32 = 1;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u3 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u3 = mma_f32(F4 {
                        kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                    }
                    , q0__u1, q1__u1, q2__u1, q3__u1, ka__u3.a, ka__u3.c);
                    kqc[ (8 as i32)] = lo__u3.x;
                    kqc[ (9 as i32)] = lo__u3.y;
                    kqc[ (10 as i32)] = lo__u3.z;
                    kqc[ (11 as i32)] = lo__u3.w;
                    let hi__u3 = mma_f32(F4 {
                        kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                    }
                    , q0__u1, q1__u1, q2__u1, q3__u1, ka__u3.b, ka__u3.d);
                    kqc[ (12 as i32)] = hi__u3.x;
                    kqc[ (13 as i32)] = hi__u3.y;
                    kqc[ (14 as i32)] = hi__u3.z;
                    kqc[ (15 as i32)] = hi__u3.w;
                }
                t = (2 as i32);
            }

            if true {
                let q: i32 = 1;
                let q0__u4 = qr.q4;
                let q1__u4 = qr.q5;
                let q2__u4 = qr.q6;
                let q3__u4 = qr.q7;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u5 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u5 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u4, q1__u4, q2__u4, q3__u4, ka__u5.a, ka__u5.c);
                    kqc[ (0 as i32)] = lo__u5.x;
                    kqc[ (1 as i32)] = lo__u5.y;
                    kqc[ (2 as i32)] = lo__u5.z;
                    kqc[ (3 as i32)] = lo__u5.w;
                    let hi__u5 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u4, q1__u4, q2__u4, q3__u4, ka__u5.b, ka__u5.d);
                    kqc[ (4 as i32)] = hi__u5.x;
                    kqc[ (5 as i32)] = hi__u5.y;
                    kqc[ (6 as i32)] = hi__u5.z;
                    kqc[ (7 as i32)] = hi__u5.w;
                }

                if true {
                    let t: i32 = 1;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u6 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u6 = mma_f32(F4 {
                        kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                    }
                    , q0__u4, q1__u4, q2__u4, q3__u4, ka__u6.a, ka__u6.c);
                    kqc[ (8 as i32)] = lo__u6.x;
                    kqc[ (9 as i32)] = lo__u6.y;
                    kqc[ (10 as i32)] = lo__u6.z;
                    kqc[ (11 as i32)] = lo__u6.w;
                    let hi__u6 = mma_f32(F4 {
                        kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                    }
                    , q0__u4, q1__u4, q2__u4, q3__u4, ka__u6.b, ka__u6.d);
                    kqc[ (12 as i32)] = hi__u6.x;
                    kqc[ (13 as i32)] = hi__u6.y;
                    kqc[ (14 as i32)] = hi__u6.z;
                    kqc[ (15 as i32)] = hi__u6.w;
                }
                t = (2 as i32);
            }

            if true {
                let q: i32 = 2;
                let q0__u7 = qr.q8;
                let q1__u7 = qr.q9;
                let q2__u7 = qr.q10;
                let q3__u7 = qr.q11;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u8 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u8 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u7, q1__u7, q2__u7, q3__u7, ka__u8.a, ka__u8.c);
                    kqc[ (0 as i32)] = lo__u8.x;
                    kqc[ (1 as i32)] = lo__u8.y;
                    kqc[ (2 as i32)] = lo__u8.z;
                    kqc[ (3 as i32)] = lo__u8.w;
                    let hi__u8 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u7, q1__u7, q2__u7, q3__u7, ka__u8.b, ka__u8.d);
                    kqc[ (4 as i32)] = hi__u8.x;
                    kqc[ (5 as i32)] = hi__u8.y;
                    kqc[ (6 as i32)] = hi__u8.z;
                    kqc[ (7 as i32)] = hi__u8.w;
                }

                if true {
                    let t: i32 = 1;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u9 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u9 = mma_f32(F4 {
                        kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                    }
                    , q0__u7, q1__u7, q2__u7, q3__u7, ka__u9.a, ka__u9.c);
                    kqc[ (8 as i32)] = lo__u9.x;
                    kqc[ (9 as i32)] = lo__u9.y;
                    kqc[ (10 as i32)] = lo__u9.z;
                    kqc[ (11 as i32)] = lo__u9.w;
                    let hi__u9 = mma_f32(F4 {
                        kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                    }
                    , q0__u7, q1__u7, q2__u7, q3__u7, ka__u9.b, ka__u9.d);
                    kqc[ (12 as i32)] = hi__u9.x;
                    kqc[ (13 as i32)] = hi__u9.y;
                    kqc[ (14 as i32)] = hi__u9.z;
                    kqc[ (15 as i32)] = hi__u9.w;
                }
                t = (2 as i32);
            }

            if true {
                let q: i32 = 3;
                let q0__u10 = qr.q12;
                let q1__u10 = qr.q13;
                let q2__u10 = qr.q14;
                let q3__u10 = qr.q15;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u11 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u11 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u10, q1__u10, q2__u10, q3__u10, ka__u11.a, ka__u11.c);
                    kqc[ (0 as i32)] = lo__u11.x;
                    kqc[ (1 as i32)] = lo__u11.y;
                    kqc[ (2 as i32)] = lo__u11.z;
                    kqc[ (3 as i32)] = lo__u11.w;
                    let hi__u11 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u10, q1__u10, q2__u10, q3__u10, ka__u11.b, ka__u11.d);
                    kqc[ (4 as i32)] = hi__u11.x;
                    kqc[ (5 as i32)] = hi__u11.y;
                    kqc[ (6 as i32)] = hi__u11.z;
                    kqc[ (7 as i32)] = hi__u11.w;
                }

                if true {
                    let t: i32 = 1;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u12 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u12 = mma_f32(F4 {
                        kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                    }
                    , q0__u10, q1__u10, q2__u10, q3__u10, ka__u12.a, ka__u12.c);
                    kqc[ (8 as i32)] = lo__u12.x;
                    kqc[ (9 as i32)] = lo__u12.y;
                    kqc[ (10 as i32)] = lo__u12.z;
                    kqc[ (11 as i32)] = lo__u12.w;
                    let hi__u12 = mma_f32(F4 {
                        kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                    }
                    , q0__u10, q1__u10, q2__u10, q3__u10, ka__u12.b, ka__u12.d);
                    kqc[ (12 as i32)] = hi__u12.x;
                    kqc[ (13 as i32)] = hi__u12.y;
                    kqc[ (14 as i32)] = hi__u12.z;
                    kqc[ (15 as i32)] = hi__u12.w;
                }
                t = (2 as i32);
            }

            if true {
                let q: i32 = 4;
                let q0__u13 = qr.q16;
                let q1__u13 = qr.q17;
                let q2__u13 = qr.q18;
                let q3__u13 = qr.q19;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u14 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u14 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u13, q1__u13, q2__u13, q3__u13, ka__u14.a, ka__u14.c);
                    kqc[ (0 as i32)] = lo__u14.x;
                    kqc[ (1 as i32)] = lo__u14.y;
                    kqc[ (2 as i32)] = lo__u14.z;
                    kqc[ (3 as i32)] = lo__u14.w;
                    let hi__u14 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u13, q1__u13, q2__u13, q3__u13, ka__u14.b, ka__u14.d);
                    kqc[ (4 as i32)] = hi__u14.x;
                    kqc[ (5 as i32)] = hi__u14.y;
                    kqc[ (6 as i32)] = hi__u14.z;
                    kqc[ (7 as i32)] = hi__u14.w;
                }

                if true {
                    let t: i32 = 1;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u15 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u15 = mma_f32(F4 {
                        kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                    }
                    , q0__u13, q1__u13, q2__u13, q3__u13, ka__u15.a, ka__u15.c);
                    kqc[ (8 as i32)] = lo__u15.x;
                    kqc[ (9 as i32)] = lo__u15.y;
                    kqc[ (10 as i32)] = lo__u15.z;
                    kqc[ (11 as i32)] = lo__u15.w;
                    let hi__u15 = mma_f32(F4 {
                        kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                    }
                    , q0__u13, q1__u13, q2__u13, q3__u13, ka__u15.b, ka__u15.d);
                    kqc[ (12 as i32)] = hi__u15.x;
                    kqc[ (13 as i32)] = hi__u15.y;
                    kqc[ (14 as i32)] = hi__u15.z;
                    kqc[ (15 as i32)] = hi__u15.w;
                }
                t = (2 as i32);
            }

            if true {
                let q: i32 = 5;
                let q0__u16 = qr.q20;
                let q1__u16 = qr.q21;
                let q2__u16 = qr.q22;
                let q3__u16 = qr.q23;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u17 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u17 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u16, q1__u16, q2__u16, q3__u16, ka__u17.a, ka__u17.c);
                    kqc[ (0 as i32)] = lo__u17.x;
                    kqc[ (1 as i32)] = lo__u17.y;
                    kqc[ (2 as i32)] = lo__u17.z;
                    kqc[ (3 as i32)] = lo__u17.w;
                    let hi__u17 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u16, q1__u16, q2__u16, q3__u16, ka__u17.b, ka__u17.d);
                    kqc[ (4 as i32)] = hi__u17.x;
                    kqc[ (5 as i32)] = hi__u17.y;
                    kqc[ (6 as i32)] = hi__u17.z;
                    kqc[ (7 as i32)] = hi__u17.w;
                }

                if true {
                    let t: i32 = 1;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u18 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u18 = mma_f32(F4 {
                        kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                    }
                    , q0__u16, q1__u16, q2__u16, q3__u16, ka__u18.a, ka__u18.c);
                    kqc[ (8 as i32)] = lo__u18.x;
                    kqc[ (9 as i32)] = lo__u18.y;
                    kqc[ (10 as i32)] = lo__u18.z;
                    kqc[ (11 as i32)] = lo__u18.w;
                    let hi__u18 = mma_f32(F4 {
                        kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                    }
                    , q0__u16, q1__u16, q2__u16, q3__u16, ka__u18.b, ka__u18.d);
                    kqc[ (12 as i32)] = hi__u18.x;
                    kqc[ (13 as i32)] = hi__u18.y;
                    kqc[ (14 as i32)] = hi__u18.z;
                    kqc[ (15 as i32)] = hi__u18.w;
                }
                t = (2 as i32);
            }

            if true {
                let q: i32 = 6;
                let q0__u19 = qr.q24;
                let q1__u19 = qr.q25;
                let q2__u19 = qr.q26;
                let q3__u19 = qr.q27;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u20 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u20 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u19, q1__u19, q2__u19, q3__u19, ka__u20.a, ka__u20.c);
                    kqc[ (0 as i32)] = lo__u20.x;
                    kqc[ (1 as i32)] = lo__u20.y;
                    kqc[ (2 as i32)] = lo__u20.z;
                    kqc[ (3 as i32)] = lo__u20.w;
                    let hi__u20 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u19, q1__u19, q2__u19, q3__u19, ka__u20.b, ka__u20.d);
                    kqc[ (4 as i32)] = hi__u20.x;
                    kqc[ (5 as i32)] = hi__u20.y;
                    kqc[ (6 as i32)] = hi__u20.z;
                    kqc[ (7 as i32)] = hi__u20.w;
                }

                if true {
                    let t: i32 = 1;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u21 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u21 = mma_f32(F4 {
                        kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                    }
                    , q0__u19, q1__u19, q2__u19, q3__u19, ka__u21.a, ka__u21.c);
                    kqc[ (8 as i32)] = lo__u21.x;
                    kqc[ (9 as i32)] = lo__u21.y;
                    kqc[ (10 as i32)] = lo__u21.z;
                    kqc[ (11 as i32)] = lo__u21.w;
                    let hi__u21 = mma_f32(F4 {
                        kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                    }
                    , q0__u19, q1__u19, q2__u19, q3__u19, ka__u21.b, ka__u21.d);
                    kqc[ (12 as i32)] = hi__u21.x;
                    kqc[ (13 as i32)] = hi__u21.y;
                    kqc[ (14 as i32)] = hi__u21.z;
                    kqc[ (15 as i32)] = hi__u21.w;
                }
                t = (2 as i32);
            }

            if true {
                let q: i32 = 7;
                let q0__u22 = qr.q28;
                let q1__u22 = qr.q29;
                let q2__u22 = qr.q30;
                let q3__u22 = qr.q31;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u23 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u23 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u22, q1__u22, q2__u22, q3__u22, ka__u23.a, ka__u23.c);
                    kqc[ (0 as i32)] = lo__u23.x;
                    kqc[ (1 as i32)] = lo__u23.y;
                    kqc[ (2 as i32)] = lo__u23.z;
                    kqc[ (3 as i32)] = lo__u23.w;
                    let hi__u23 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u22, q1__u22, q2__u22, q3__u22, ka__u23.b, ka__u23.d);
                    kqc[ (4 as i32)] = hi__u23.x;
                    kqc[ (5 as i32)] = hi__u23.y;
                    kqc[ (6 as i32)] = hi__u23.z;
                    kqc[ (7 as i32)] = hi__u23.w;
                }

                if true {
                    let t: i32 = 1;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u24 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u24 = mma_f32(F4 {
                        kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                    }
                    , q0__u22, q1__u22, q2__u22, q3__u22, ka__u24.a, ka__u24.c);
                    kqc[ (8 as i32)] = lo__u24.x;
                    kqc[ (9 as i32)] = lo__u24.y;
                    kqc[ (10 as i32)] = lo__u24.z;
                    kqc[ (11 as i32)] = lo__u24.w;
                    let hi__u24 = mma_f32(F4 {
                        kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                    }
                    , q0__u22, q1__u22, q2__u22, q3__u22, ka__u24.b, ka__u24.d);
                    kqc[ (12 as i32)] = hi__u24.x;
                    kqc[ (13 as i32)] = hi__u24.y;
                    kqc[ (14 as i32)] = hi__u24.z;
                    kqc[ (15 as i32)] = hi__u24.w;
                }
                t = (2 as i32);
            }

            if true {
                let q: i32 = 8;
                let q0__u25 = qr.q32;
                let q1__u25 = qr.q33;
                let q2__u25 = qr.q34;
                let q3__u25 = qr.q35;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u26 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u26 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u25, q1__u25, q2__u25, q3__u25, ka__u26.a, ka__u26.c);
                    kqc[ (0 as i32)] = lo__u26.x;
                    kqc[ (1 as i32)] = lo__u26.y;
                    kqc[ (2 as i32)] = lo__u26.z;
                    kqc[ (3 as i32)] = lo__u26.w;
                    let hi__u26 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u25, q1__u25, q2__u25, q3__u25, ka__u26.b, ka__u26.d);
                    kqc[ (4 as i32)] = hi__u26.x;
                    kqc[ (5 as i32)] = hi__u26.y;
                    kqc[ (6 as i32)] = hi__u26.z;
                    kqc[ (7 as i32)] = hi__u26.w;
                }

                if true {
                    let t: i32 = 1;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u27 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u27 = mma_f32(F4 {
                        kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                    }
                    , q0__u25, q1__u25, q2__u25, q3__u25, ka__u27.a, ka__u27.c);
                    kqc[ (8 as i32)] = lo__u27.x;
                    kqc[ (9 as i32)] = lo__u27.y;
                    kqc[ (10 as i32)] = lo__u27.z;
                    kqc[ (11 as i32)] = lo__u27.w;
                    let hi__u27 = mma_f32(F4 {
                        kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                    }
                    , q0__u25, q1__u25, q2__u25, q3__u25, ka__u27.b, ka__u27.d);
                    kqc[ (12 as i32)] = hi__u27.x;
                    kqc[ (13 as i32)] = hi__u27.y;
                    kqc[ (14 as i32)] = hi__u27.z;
                    kqc[ (15 as i32)] = hi__u27.w;
                }
                t = (2 as i32);
            }

            if true {
                let q: i32 = 9;
                let q0__u28 = qr.q36;
                let q1__u28 = qr.q37;
                let q2__u28 = qr.q38;
                let q3__u28 = qr.q39;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u29 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u29 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u28, q1__u28, q2__u28, q3__u28, ka__u29.a, ka__u29.c);
                    kqc[ (0 as i32)] = lo__u29.x;
                    kqc[ (1 as i32)] = lo__u29.y;
                    kqc[ (2 as i32)] = lo__u29.z;
                    kqc[ (3 as i32)] = lo__u29.w;
                    let hi__u29 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u28, q1__u28, q2__u28, q3__u28, ka__u29.b, ka__u29.d);
                    kqc[ (4 as i32)] = hi__u29.x;
                    kqc[ (5 as i32)] = hi__u29.y;
                    kqc[ (6 as i32)] = hi__u29.z;
                    kqc[ (7 as i32)] = hi__u29.w;
                }

                if true {
                    let t: i32 = 1;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u30 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u30 = mma_f32(F4 {
                        kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                    }
                    , q0__u28, q1__u28, q2__u28, q3__u28, ka__u30.a, ka__u30.c);
                    kqc[ (8 as i32)] = lo__u30.x;
                    kqc[ (9 as i32)] = lo__u30.y;
                    kqc[ (10 as i32)] = lo__u30.z;
                    kqc[ (11 as i32)] = lo__u30.w;
                    let hi__u30 = mma_f32(F4 {
                        kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                    }
                    , q0__u28, q1__u28, q2__u28, q3__u28, ka__u30.b, ka__u30.d);
                    kqc[ (12 as i32)] = hi__u30.x;
                    kqc[ (13 as i32)] = hi__u30.y;
                    kqc[ (14 as i32)] = hi__u30.z;
                    kqc[ (15 as i32)] = hi__u30.w;
                }
                t = (2 as i32);
            }

            if true {
                let q: i32 = 10;
                let q0__u31 = qr.q40;
                let q1__u31 = qr.q41;
                let q2__u31 = qr.q42;
                let q3__u31 = qr.q43;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u32 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u32 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u31, q1__u31, q2__u31, q3__u31, ka__u32.a, ka__u32.c);
                    kqc[ (0 as i32)] = lo__u32.x;
                    kqc[ (1 as i32)] = lo__u32.y;
                    kqc[ (2 as i32)] = lo__u32.z;
                    kqc[ (3 as i32)] = lo__u32.w;
                    let hi__u32 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u31, q1__u31, q2__u31, q3__u31, ka__u32.b, ka__u32.d);
                    kqc[ (4 as i32)] = hi__u32.x;
                    kqc[ (5 as i32)] = hi__u32.y;
                    kqc[ (6 as i32)] = hi__u32.z;
                    kqc[ (7 as i32)] = hi__u32.w;
                }

                if true {
                    let t: i32 = 1;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u33 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u33 = mma_f32(F4 {
                        kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                    }
                    , q0__u31, q1__u31, q2__u31, q3__u31, ka__u33.a, ka__u33.c);
                    kqc[ (8 as i32)] = lo__u33.x;
                    kqc[ (9 as i32)] = lo__u33.y;
                    kqc[ (10 as i32)] = lo__u33.z;
                    kqc[ (11 as i32)] = lo__u33.w;
                    let hi__u33 = mma_f32(F4 {
                        kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                    }
                    , q0__u31, q1__u31, q2__u31, q3__u31, ka__u33.b, ka__u33.d);
                    kqc[ (12 as i32)] = hi__u33.x;
                    kqc[ (13 as i32)] = hi__u33.y;
                    kqc[ (14 as i32)] = hi__u33.z;
                    kqc[ (15 as i32)] = hi__u33.w;
                }
                t = (2 as i32);
            }

            if true {
                let q: i32 = 11;
                let q0__u34 = qr.q44;
                let q1__u34 = qr.q45;
                let q2__u34 = qr.q46;
                let q3__u34 = qr.q47;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u35 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u35 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u34, q1__u34, q2__u34, q3__u34, ka__u35.a, ka__u35.c);
                    kqc[ (0 as i32)] = lo__u35.x;
                    kqc[ (1 as i32)] = lo__u35.y;
                    kqc[ (2 as i32)] = lo__u35.z;
                    kqc[ (3 as i32)] = lo__u35.w;
                    let hi__u35 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u34, q1__u34, q2__u34, q3__u34, ka__u35.b, ka__u35.d);
                    kqc[ (4 as i32)] = hi__u35.x;
                    kqc[ (5 as i32)] = hi__u35.y;
                    kqc[ (6 as i32)] = hi__u35.z;
                    kqc[ (7 as i32)] = hi__u35.w;
                }

                if true {
                    let t: i32 = 1;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u36 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u36 = mma_f32(F4 {
                        kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                    }
                    , q0__u34, q1__u34, q2__u34, q3__u34, ka__u36.a, ka__u36.c);
                    kqc[ (8 as i32)] = lo__u36.x;
                    kqc[ (9 as i32)] = lo__u36.y;
                    kqc[ (10 as i32)] = lo__u36.z;
                    kqc[ (11 as i32)] = lo__u36.w;
                    let hi__u36 = mma_f32(F4 {
                        kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                    }
                    , q0__u34, q1__u34, q2__u34, q3__u34, ka__u36.b, ka__u36.d);
                    kqc[ (12 as i32)] = hi__u36.x;
                    kqc[ (13 as i32)] = hi__u36.y;
                    kqc[ (14 as i32)] = hi__u36.z;
                    kqc[ (15 as i32)] = hi__u36.w;
                }
                t = (2 as i32);
            }

            if true {
                let q: i32 = 12;
                let q0__u37 = qr.q48;
                let q1__u37 = qr.q49;
                let q2__u37 = qr.q50;
                let q3__u37 = qr.q51;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u38 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u38 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u37, q1__u37, q2__u37, q3__u37, ka__u38.a, ka__u38.c);
                    kqc[ (0 as i32)] = lo__u38.x;
                    kqc[ (1 as i32)] = lo__u38.y;
                    kqc[ (2 as i32)] = lo__u38.z;
                    kqc[ (3 as i32)] = lo__u38.w;
                    let hi__u38 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u37, q1__u37, q2__u37, q3__u37, ka__u38.b, ka__u38.d);
                    kqc[ (4 as i32)] = hi__u38.x;
                    kqc[ (5 as i32)] = hi__u38.y;
                    kqc[ (6 as i32)] = hi__u38.z;
                    kqc[ (7 as i32)] = hi__u38.w;
                }

                if true {
                    let t: i32 = 1;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u39 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u39 = mma_f32(F4 {
                        kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                    }
                    , q0__u37, q1__u37, q2__u37, q3__u37, ka__u39.a, ka__u39.c);
                    kqc[ (8 as i32)] = lo__u39.x;
                    kqc[ (9 as i32)] = lo__u39.y;
                    kqc[ (10 as i32)] = lo__u39.z;
                    kqc[ (11 as i32)] = lo__u39.w;
                    let hi__u39 = mma_f32(F4 {
                        kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                    }
                    , q0__u37, q1__u37, q2__u37, q3__u37, ka__u39.b, ka__u39.d);
                    kqc[ (12 as i32)] = hi__u39.x;
                    kqc[ (13 as i32)] = hi__u39.y;
                    kqc[ (14 as i32)] = hi__u39.z;
                    kqc[ (15 as i32)] = hi__u39.w;
                }
                t = (2 as i32);
            }

            if true {
                let q: i32 = 13;
                let q0__u40 = qr.q52;
                let q1__u40 = qr.q53;
                let q2__u40 = qr.q54;
                let q3__u40 = qr.q55;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u41 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u41 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u40, q1__u40, q2__u40, q3__u40, ka__u41.a, ka__u41.c);
                    kqc[ (0 as i32)] = lo__u41.x;
                    kqc[ (1 as i32)] = lo__u41.y;
                    kqc[ (2 as i32)] = lo__u41.z;
                    kqc[ (3 as i32)] = lo__u41.w;
                    let hi__u41 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u40, q1__u40, q2__u40, q3__u40, ka__u41.b, ka__u41.d);
                    kqc[ (4 as i32)] = hi__u41.x;
                    kqc[ (5 as i32)] = hi__u41.y;
                    kqc[ (6 as i32)] = hi__u41.z;
                    kqc[ (7 as i32)] = hi__u41.w;
                }

                if true {
                    let t: i32 = 1;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u42 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u42 = mma_f32(F4 {
                        kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                    }
                    , q0__u40, q1__u40, q2__u40, q3__u40, ka__u42.a, ka__u42.c);
                    kqc[ (8 as i32)] = lo__u42.x;
                    kqc[ (9 as i32)] = lo__u42.y;
                    kqc[ (10 as i32)] = lo__u42.z;
                    kqc[ (11 as i32)] = lo__u42.w;
                    let hi__u42 = mma_f32(F4 {
                        kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                    }
                    , q0__u40, q1__u40, q2__u40, q3__u40, ka__u42.b, ka__u42.d);
                    kqc[ (12 as i32)] = hi__u42.x;
                    kqc[ (13 as i32)] = hi__u42.y;
                    kqc[ (14 as i32)] = hi__u42.z;
                    kqc[ (15 as i32)] = hi__u42.w;
                }
                t = (2 as i32);
            }

            if true {
                let q: i32 = 14;
                let q0__u43 = qr.q56;
                let q1__u43 = qr.q57;
                let q2__u43 = qr.q58;
                let q3__u43 = qr.q59;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u44 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u44 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u43, q1__u43, q2__u43, q3__u43, ka__u44.a, ka__u44.c);
                    kqc[ (0 as i32)] = lo__u44.x;
                    kqc[ (1 as i32)] = lo__u44.y;
                    kqc[ (2 as i32)] = lo__u44.z;
                    kqc[ (3 as i32)] = lo__u44.w;
                    let hi__u44 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u43, q1__u43, q2__u43, q3__u43, ka__u44.b, ka__u44.d);
                    kqc[ (4 as i32)] = hi__u44.x;
                    kqc[ (5 as i32)] = hi__u44.y;
                    kqc[ (6 as i32)] = hi__u44.z;
                    kqc[ (7 as i32)] = hi__u44.w;
                }

                if true {
                    let t: i32 = 1;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u45 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u45 = mma_f32(F4 {
                        kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                    }
                    , q0__u43, q1__u43, q2__u43, q3__u43, ka__u45.a, ka__u45.c);
                    kqc[ (8 as i32)] = lo__u45.x;
                    kqc[ (9 as i32)] = lo__u45.y;
                    kqc[ (10 as i32)] = lo__u45.z;
                    kqc[ (11 as i32)] = lo__u45.w;
                    let hi__u45 = mma_f32(F4 {
                        kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                    }
                    , q0__u43, q1__u43, q2__u43, q3__u43, ka__u45.b, ka__u45.d);
                    kqc[ (12 as i32)] = hi__u45.x;
                    kqc[ (13 as i32)] = hi__u45.y;
                    kqc[ (14 as i32)] = hi__u45.z;
                    kqc[ (15 as i32)] = hi__u45.w;
                }
                t = (2 as i32);
            }

            if true {
                let q: i32 = 15;
                let q0__u46 = qr.q60;
                let q1__u46 = qr.q61;
                let q2__u46 = qr.q62;
                let q3__u46 = qr.q63;
                t = 0;

                if true {
                    let t: i32 = 0;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u47 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u47 = mma_f32(F4 {
                        kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                    }
                    , q0__u46, q1__u46, q2__u46, q3__u46, ka__u47.a, ka__u47.c);
                    kqc[ (0 as i32)] = lo__u47.x;
                    kqc[ (1 as i32)] = lo__u47.y;
                    kqc[ (2 as i32)] = lo__u47.z;
                    kqc[ (3 as i32)] = lo__u47.w;
                    let hi__u47 = mma_f32(F4 {
                        kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                    }
                    , q0__u46, q1__u46, q2__u46, q3__u46, ka__u47.b, ka__u47.d);
                    kqc[ (4 as i32)] = hi__u47.x;
                    kqc[ (5 as i32)] = hi__u47.y;
                    kqc[ (6 as i32)] = hi__u47.z;
                    kqc[ (7 as i32)] = hi__u47.w;
                }

                if true {
                    let t: i32 = 1;
                    // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                    let ka__u48 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                    // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                    // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                    let lo__u48 = mma_f32(F4 {
                        kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                    }
                    , q0__u46, q1__u46, q2__u46, q3__u46, ka__u48.a, ka__u48.c);
                    kqc[ (8 as i32)] = lo__u48.x;
                    kqc[ (9 as i32)] = lo__u48.y;
                    kqc[ (10 as i32)] = lo__u48.z;
                    kqc[ (11 as i32)] = lo__u48.w;
                    let hi__u48 = mma_f32(F4 {
                        kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                    }
                    , q0__u46, q1__u46, q2__u46, q3__u46, ka__u48.b, ka__u48.d);
                    kqc[ (12 as i32)] = hi__u48.x;
                    kqc[ (13 as i32)] = hi__u48.y;
                    kqc[ (14 as i32)] = hi__u48.z;
                    kqc[ (15 as i32)] = hi__u48.w;
                }
                t = (2 as i32);
            }
            q = (16 as i32);
            // ---- mask add + running max: half2 mask word at row (cb+get_i)/8, half2 col (rb+get_j)/2 ----
            var kqold: f32[2] = default < f32[2] > ();
            // KQ_max[cols_per_thread=2]
            var kqmn: f32[2] = default < f32[2] > ();
            // KQ_max_new
            var rsa: f32[2] = default < f32[2] > ();
            // KQ_rowsum_add
            kqold[ (0 as i32)] = frombits(bits(acc.k0));
            kqold[ (1 as i32)] = frombits(bits(acc.k1));
            kqmn[ (0 as i32)] = kqold[ (0 as i32)];
            kqmn[ (1 as i32)] = kqold[ (1 as i32)];
            t = 0;

            if true {
                let t: i32 = 0;
                let rb__u49 = t * np * 16 + rh;
                var l0__u49: i32 = 0;

                if true {
                    let l0__u49: i32 = 0;
                    // half2 pairs, l0 even: (j,i),(j,i+1)
                    let mh2__u50 = ( (cb + gi_c(l0__u49, tx)) / 8) * 20 + ( (rb__u49 + gj_c(l0__u49, tx)) / 2);
                    let w__u50 = ldu(mt + ( (mh2__u50 as u64) * (4 as u64)));
                    kqc[ (0 as i32)] = fmaf(slope, hlo(w__u50), kqc[ (0 as i32)]);
                    kqc[ (1 as i32)] = fmaf(slope, hhi(w__u50), kqc[ (1 as i32)]);
                }

                if true {
                    let l0__u49: i32 = 2;
                    // half2 pairs, l0 even: (j,i),(j,i+1)
                    let mh2__u51 = ( (cb + gi_c(l0__u49, tx)) / 8) * 20 + ( (rb__u49 + gj_c(l0__u49, tx)) / 2);
                    let w__u51 = ldu(mt + ( (mh2__u51 as u64) * (4 as u64)));
                    kqc[ (2 as i32)] = fmaf(slope, hlo(w__u51), kqc[ (2 as i32)]);
                    kqc[ (3 as i32)] = fmaf(slope, hhi(w__u51), kqc[ (3 as i32)]);
                }

                if true {
                    let l0__u49: i32 = 4;
                    // half2 pairs, l0 even: (j,i),(j,i+1)
                    let mh2__u52 = ( (cb + gi_c(l0__u49, tx)) / 8) * 20 + ( (rb__u49 + gj_c(l0__u49, tx)) / 2);
                    let w__u52 = ldu(mt + ( (mh2__u52 as u64) * (4 as u64)));
                    kqc[ (4 as i32)] = fmaf(slope, hlo(w__u52), kqc[ (4 as i32)]);
                    kqc[ (5 as i32)] = fmaf(slope, hhi(w__u52), kqc[ (5 as i32)]);
                }

                if true {
                    let l0__u49: i32 = 6;
                    // half2 pairs, l0 even: (j,i),(j,i+1)
                    let mh2__u53 = ( (cb + gi_c(l0__u49, tx)) / 8) * 20 + ( (rb__u49 + gj_c(l0__u49, tx)) / 2);
                    let w__u53 = ldu(mt + ( (mh2__u53 as u64) * (4 as u64)));
                    kqc[ (6 as i32)] = fmaf(slope, hlo(w__u53), kqc[ (6 as i32)]);
                    kqc[ (7 as i32)] = fmaf(slope, hhi(w__u53), kqc[ (7 as i32)]);
                }
                l0__u49 = (8 as i32);
                var l__u49: i32 = 0;

                if true {
                    let l__u49: i32 = 0;
                    let idx__u54 = (l__u49 / 2) % 2;
                    let mv__u54 = kqc[ (0 as i32)] + kqoff;

                    if mv__u54 > kqmn[ (0 as i32)] {
                        kqmn[ (0 as i32)] = mv__u54;
                    }
                }

                if true {
                    let l__u49: i32 = 1;
                    let idx__u55 = (l__u49 / 2) % 2;
                    let mv__u55 = kqc[ (1 as i32)] + kqoff;

                    if mv__u55 > kqmn[ (0 as i32)] {
                        kqmn[ (0 as i32)] = mv__u55;
                    }
                }

                if true {
                    let l__u49: i32 = 2;
                    let idx__u56 = (l__u49 / 2) % 2;
                    let mv__u56 = kqc[ (2 as i32)] + kqoff;

                    if mv__u56 > kqmn[ (1 as i32)] {
                        kqmn[ (1 as i32)] = mv__u56;
                    }
                }

                if true {
                    let l__u49: i32 = 3;
                    let idx__u57 = (l__u49 / 2) % 2;
                    let mv__u57 = kqc[ (3 as i32)] + kqoff;

                    if mv__u57 > kqmn[ (1 as i32)] {
                        kqmn[ (1 as i32)] = mv__u57;
                    }
                }

                if true {
                    let l__u49: i32 = 4;
                    let idx__u58 = (l__u49 / 2) % 2;
                    let mv__u58 = kqc[ (4 as i32)] + kqoff;

                    if mv__u58 > kqmn[ (0 as i32)] {
                        kqmn[ (0 as i32)] = mv__u58;
                    }
                }

                if true {
                    let l__u49: i32 = 5;
                    let idx__u59 = (l__u49 / 2) % 2;
                    let mv__u59 = kqc[ (5 as i32)] + kqoff;

                    if mv__u59 > kqmn[ (0 as i32)] {
                        kqmn[ (0 as i32)] = mv__u59;
                    }
                }

                if true {
                    let l__u49: i32 = 6;
                    let idx__u60 = (l__u49 / 2) % 2;
                    let mv__u60 = kqc[ (6 as i32)] + kqoff;

                    if mv__u60 > kqmn[ (1 as i32)] {
                        kqmn[ (1 as i32)] = mv__u60;
                    }
                }

                if true {
                    let l__u49: i32 = 7;
                    let idx__u61 = (l__u49 / 2) % 2;
                    let mv__u61 = kqc[ (7 as i32)] + kqoff;

                    if mv__u61 > kqmn[ (1 as i32)] {
                        kqmn[ (1 as i32)] = mv__u61;
                    }
                }
                l__u49 = (8 as i32);
            }

            if true {
                let t: i32 = 1;
                let rb__u62 = t * np * 16 + rh;
                var l0__u62: i32 = 0;

                if true {
                    let l0__u62: i32 = 0;
                    // half2 pairs, l0 even: (j,i),(j,i+1)
                    let mh2__u63 = ( (cb + gi_c(l0__u62, tx)) / 8) * 20 + ( (rb__u62 + gj_c(l0__u62, tx)) / 2);
                    let w__u63 = ldu(mt + ( (mh2__u63 as u64) * (4 as u64)));
                    kqc[ (8 as i32)] = fmaf(slope, hlo(w__u63), kqc[ (8 as i32)]);
                    kqc[ (9 as i32)] = fmaf(slope, hhi(w__u63), kqc[ (9 as i32)]);
                }

                if true {
                    let l0__u62: i32 = 2;
                    // half2 pairs, l0 even: (j,i),(j,i+1)
                    let mh2__u64 = ( (cb + gi_c(l0__u62, tx)) / 8) * 20 + ( (rb__u62 + gj_c(l0__u62, tx)) / 2);
                    let w__u64 = ldu(mt + ( (mh2__u64 as u64) * (4 as u64)));
                    kqc[ (10 as i32)] = fmaf(slope, hlo(w__u64), kqc[ (10 as i32)]);
                    kqc[ (11 as i32)] = fmaf(slope, hhi(w__u64), kqc[ (11 as i32)]);
                }

                if true {
                    let l0__u62: i32 = 4;
                    // half2 pairs, l0 even: (j,i),(j,i+1)
                    let mh2__u65 = ( (cb + gi_c(l0__u62, tx)) / 8) * 20 + ( (rb__u62 + gj_c(l0__u62, tx)) / 2);
                    let w__u65 = ldu(mt + ( (mh2__u65 as u64) * (4 as u64)));
                    kqc[ (12 as i32)] = fmaf(slope, hlo(w__u65), kqc[ (12 as i32)]);
                    kqc[ (13 as i32)] = fmaf(slope, hhi(w__u65), kqc[ (13 as i32)]);
                }

                if true {
                    let l0__u62: i32 = 6;
                    // half2 pairs, l0 even: (j,i),(j,i+1)
                    let mh2__u66 = ( (cb + gi_c(l0__u62, tx)) / 8) * 20 + ( (rb__u62 + gj_c(l0__u62, tx)) / 2);
                    let w__u66 = ldu(mt + ( (mh2__u66 as u64) * (4 as u64)));
                    kqc[ (14 as i32)] = fmaf(slope, hlo(w__u66), kqc[ (14 as i32)]);
                    kqc[ (15 as i32)] = fmaf(slope, hhi(w__u66), kqc[ (15 as i32)]);
                }
                l0__u62 = (8 as i32);
                var l__u62: i32 = 0;

                if true {
                    let l__u62: i32 = 0;
                    let idx__u67 = (l__u62 / 2) % 2;
                    let mv__u67 = kqc[ (8 as i32)] + kqoff;

                    if mv__u67 > kqmn[ (0 as i32)] {
                        kqmn[ (0 as i32)] = mv__u67;
                    }
                }

                if true {
                    let l__u62: i32 = 1;
                    let idx__u68 = (l__u62 / 2) % 2;
                    let mv__u68 = kqc[ (9 as i32)] + kqoff;

                    if mv__u68 > kqmn[ (0 as i32)] {
                        kqmn[ (0 as i32)] = mv__u68;
                    }
                }

                if true {
                    let l__u62: i32 = 2;
                    let idx__u69 = (l__u62 / 2) % 2;
                    let mv__u69 = kqc[ (10 as i32)] + kqoff;

                    if mv__u69 > kqmn[ (1 as i32)] {
                        kqmn[ (1 as i32)] = mv__u69;
                    }
                }

                if true {
                    let l__u62: i32 = 3;
                    let idx__u70 = (l__u62 / 2) % 2;
                    let mv__u70 = kqc[ (11 as i32)] + kqoff;

                    if mv__u70 > kqmn[ (1 as i32)] {
                        kqmn[ (1 as i32)] = mv__u70;
                    }
                }

                if true {
                    let l__u62: i32 = 4;
                    let idx__u71 = (l__u62 / 2) % 2;
                    let mv__u71 = kqc[ (12 as i32)] + kqoff;

                    if mv__u71 > kqmn[ (0 as i32)] {
                        kqmn[ (0 as i32)] = mv__u71;
                    }
                }

                if true {
                    let l__u62: i32 = 5;
                    let idx__u72 = (l__u62 / 2) % 2;
                    let mv__u72 = kqc[ (13 as i32)] + kqoff;

                    if mv__u72 > kqmn[ (0 as i32)] {
                        kqmn[ (0 as i32)] = mv__u72;
                    }
                }

                if true {
                    let l__u62: i32 = 6;
                    let idx__u73 = (l__u62 / 2) % 2;
                    let mv__u73 = kqc[ (14 as i32)] + kqoff;

                    if mv__u73 > kqmn[ (1 as i32)] {
                        kqmn[ (1 as i32)] = mv__u73;
                    }
                }

                if true {
                    let l__u62: i32 = 7;
                    let idx__u74 = (l__u62 / 2) % 2;
                    let mv__u74 = kqc[ (15 as i32)] + kqoff;

                    if mv__u74 > kqmn[ (1 as i32)] {
                        kqmn[ (1 as i32)] = mv__u74;
                    }
                }
                l__u62 = (8 as i32);
            }
            t = (2 as i32);
            // Values per KQ column spread across 4 threads: __shfl_xor offsets 2 then 1 (TURING wide).
            var col: i32 = 0;

            if true {
                let col: i32 = 0;
                var s__u75 = shflx(kqmn[ (0 as i32)], 2);

                if s__u75 > kqmn[ (0 as i32)] {
                    kqmn[ (0 as i32)] = s__u75;
                }
                s__u75 = shflx(kqmn[ (0 as i32)], 1);

                if s__u75 > kqmn[ (0 as i32)] {
                    kqmn[ (0 as i32)] = s__u75;
                }
            }

            if true {
                let col: i32 = 1;
                var s__u76 = shflx(kqmn[ (1 as i32)], 2);

                if s__u76 > kqmn[ (1 as i32)] {
                    kqmn[ (1 as i32)] = s__u76;
                }
                s__u76 = shflx(kqmn[ (1 as i32)], 1);

                if s__u76 > kqmn[ (1 as i32)] {
                    kqmn[ (1 as i32)] = s__u76;
                }
            }
            col = (2 as i32);
            // ---- softmax with current max: expf(KQ - KQ_max_new); divisor stays in KQ_rowsum ----
            t = 0;

            if true {
                let t: i32 = 0;
                var l__u77: i32 = 0;

                if true {
                    let l__u77: i32 = 0;
                    let idx__u78 = (l__u77 / 2) % 2;
                    let e__u78 = expf(kqc[ (0 as i32)] - kqmn[ (0 as i32)]);
                    kqc[ (0 as i32)] = e__u78;
                    rsa[ (0 as i32)] += e__u78;
                }

                if true {
                    let l__u77: i32 = 1;
                    let idx__u79 = (l__u77 / 2) % 2;
                    let e__u79 = expf(kqc[ (1 as i32)] - kqmn[ (0 as i32)]);
                    kqc[ (1 as i32)] = e__u79;
                    rsa[ (0 as i32)] += e__u79;
                }

                if true {
                    let l__u77: i32 = 2;
                    let idx__u80 = (l__u77 / 2) % 2;
                    let e__u80 = expf(kqc[ (2 as i32)] - kqmn[ (1 as i32)]);
                    kqc[ (2 as i32)] = e__u80;
                    rsa[ (1 as i32)] += e__u80;
                }

                if true {
                    let l__u77: i32 = 3;
                    let idx__u81 = (l__u77 / 2) % 2;
                    let e__u81 = expf(kqc[ (3 as i32)] - kqmn[ (1 as i32)]);
                    kqc[ (3 as i32)] = e__u81;
                    rsa[ (1 as i32)] += e__u81;
                }

                if true {
                    let l__u77: i32 = 4;
                    let idx__u82 = (l__u77 / 2) % 2;
                    let e__u82 = expf(kqc[ (4 as i32)] - kqmn[ (0 as i32)]);
                    kqc[ (4 as i32)] = e__u82;
                    rsa[ (0 as i32)] += e__u82;
                }

                if true {
                    let l__u77: i32 = 5;
                    let idx__u83 = (l__u77 / 2) % 2;
                    let e__u83 = expf(kqc[ (5 as i32)] - kqmn[ (0 as i32)]);
                    kqc[ (5 as i32)] = e__u83;
                    rsa[ (0 as i32)] += e__u83;
                }

                if true {
                    let l__u77: i32 = 6;
                    let idx__u84 = (l__u77 / 2) % 2;
                    let e__u84 = expf(kqc[ (6 as i32)] - kqmn[ (1 as i32)]);
                    kqc[ (6 as i32)] = e__u84;
                    rsa[ (1 as i32)] += e__u84;
                }

                if true {
                    let l__u77: i32 = 7;
                    let idx__u85 = (l__u77 / 2) % 2;
                    let e__u85 = expf(kqc[ (7 as i32)] - kqmn[ (1 as i32)]);
                    kqc[ (7 as i32)] = e__u85;
                    rsa[ (1 as i32)] += e__u85;
                }
                l__u77 = (8 as i32);
            }

            if true {
                let t: i32 = 1;
                var l__u86: i32 = 0;

                if true {
                    let l__u86: i32 = 0;
                    let idx__u87 = (l__u86 / 2) % 2;
                    let e__u87 = expf(kqc[ (8 as i32)] - kqmn[ (0 as i32)]);
                    kqc[ (8 as i32)] = e__u87;
                    rsa[ (0 as i32)] += e__u87;
                }

                if true {
                    let l__u86: i32 = 1;
                    let idx__u88 = (l__u86 / 2) % 2;
                    let e__u88 = expf(kqc[ (9 as i32)] - kqmn[ (0 as i32)]);
                    kqc[ (9 as i32)] = e__u88;
                    rsa[ (0 as i32)] += e__u88;
                }

                if true {
                    let l__u86: i32 = 2;
                    let idx__u89 = (l__u86 / 2) % 2;
                    let e__u89 = expf(kqc[ (10 as i32)] - kqmn[ (1 as i32)]);
                    kqc[ (10 as i32)] = e__u89;
                    rsa[ (1 as i32)] += e__u89;
                }

                if true {
                    let l__u86: i32 = 3;
                    let idx__u90 = (l__u86 / 2) % 2;
                    let e__u90 = expf(kqc[ (11 as i32)] - kqmn[ (1 as i32)]);
                    kqc[ (11 as i32)] = e__u90;
                    rsa[ (1 as i32)] += e__u90;
                }

                if true {
                    let l__u86: i32 = 4;
                    let idx__u91 = (l__u86 / 2) % 2;
                    let e__u91 = expf(kqc[ (12 as i32)] - kqmn[ (0 as i32)]);
                    kqc[ (12 as i32)] = e__u91;
                    rsa[ (0 as i32)] += e__u91;
                }

                if true {
                    let l__u86: i32 = 5;
                    let idx__u92 = (l__u86 / 2) % 2;
                    let e__u92 = expf(kqc[ (13 as i32)] - kqmn[ (0 as i32)]);
                    kqc[ (13 as i32)] = e__u92;
                    rsa[ (0 as i32)] += e__u92;
                }

                if true {
                    let l__u86: i32 = 6;
                    let idx__u93 = (l__u86 / 2) % 2;
                    let e__u93 = expf(kqc[ (14 as i32)] - kqmn[ (1 as i32)]);
                    kqc[ (14 as i32)] = e__u93;
                    rsa[ (1 as i32)] += e__u93;
                }

                if true {
                    let l__u86: i32 = 7;
                    let idx__u94 = (l__u86 / 2) % 2;
                    let e__u94 = expf(kqc[ (15 as i32)] - kqmn[ (1 as i32)]);
                    kqc[ (15 as i32)] = e__u94;
                    rsa[ (1 as i32)] += e__u94;
                }
                l__u86 = (8 as i32);
            }
            t = (2 as i32);
            // ---- rescale: KQ_max_scale=expf(KQ_max-KQ_max_new); FTZ bit-multiply for diff < -20;
            //      exact fused step KQ_rowsum = scale*KQ_rowsum + KQ_rowsum_add ----
            var scl: f32[2] = default < f32[2] > ();
            col = 0;

            if true {
                let col: i32 = 0;
                let diff__u95 = kqold[ (0 as i32)] - kqmn[ (0 as i32)];
                let s0__u95 = expf(diff__u95);
                let keep__u95: u32 = if diff__u95 >= -20.0 {
                    1 as u32
                } else {
                    0 as u32
                }
                ;
                scl[ (0 as i32)] = frombits(bits(s0__u95) * keep__u95);
                // *((uint32_t*)&scale) *= diff>=SOFTMAX_FTZ_THRESHOLD
                acc.k0 = frombits(bits(kqmn[ (0 as i32)]));
                let rs__u95 = frombits(bits(acc.s0));
                acc.s0 = frombits(bits(fmaf(scl[ (0 as i32)], rs__u95, rsa[ (0 as i32)])));
            }

            if true {
                let col: i32 = 1;
                let diff__u96 = kqold[ (1 as i32)] - kqmn[ (1 as i32)];
                let s0__u96 = expf(diff__u96);
                let keep__u96: u32 = if diff__u96 >= -20.0 {
                    1 as u32
                } else {
                    0 as u32
                }
                ;
                scl[ (1 as i32)] = frombits(bits(s0__u96) * keep__u96);
                // *((uint32_t*)&scale) *= diff>=SOFTMAX_FTZ_THRESHOLD
                acc.k1 = frombits(bits(kqmn[ (1 as i32)]));
                let rs__u96 = frombits(bits(acc.s1));
                acc.s1 = frombits(bits(fmaf(scl[ (1 as i32)], rs__u96, rsa[ (1 as i32)])));
            }
            col = (2 as i32);
            // ---- VKQ f16 accumulator rescale: VKQ_C[i].x[l0+col] *= make_half2(scale,scale) ----
            var sh2: u32[2] = default < u32[2] > ();
            col = 0;

            if true {
                let col: i32 = 0;
                sh2[ (0 as i32)] = h2(scl[ (0 as i32)], scl[ (0 as i32)]);
            }

            if true {
                let col: i32 = 1;
                sh2[ (1 as i32)] = h2(scl[ (1 as i32)], scl[ (1 as i32)]);
            }
            col = (2 as i32);
            var v: i32 = 0;

            if true {
                let v: i32 = 0;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u100: i32 = 0;

                    if true {
                        let l0__u100: i32 = 0;
                        let e__u101 = v * 4 + l0__u100 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v0 = mulh2(acc.v0, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u100: i32 = 1;
                        let e__u102 = v * 4 + l0__u100 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v2 = mulh2(acc.v2, sh2[ (0 as i32)]);
                    }
                    l0__u100 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u103: i32 = 0;

                    if true {
                        let l0__u103: i32 = 0;
                        let e__u104 = v * 4 + l0__u103 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v1 = mulh2(acc.v1, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u103: i32 = 1;
                        let e__u105 = v * 4 + l0__u103 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v3 = mulh2(acc.v3, sh2[ (1 as i32)]);
                    }
                    l0__u103 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 1;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u107: i32 = 0;

                    if true {
                        let l0__u107: i32 = 0;
                        let e__u108 = v * 4 + l0__u107 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v4 = mulh2(acc.v4, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u107: i32 = 1;
                        let e__u109 = v * 4 + l0__u107 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v6 = mulh2(acc.v6, sh2[ (0 as i32)]);
                    }
                    l0__u107 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u110: i32 = 0;

                    if true {
                        let l0__u110: i32 = 0;
                        let e__u111 = v * 4 + l0__u110 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v5 = mulh2(acc.v5, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u110: i32 = 1;
                        let e__u112 = v * 4 + l0__u110 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v7 = mulh2(acc.v7, sh2[ (1 as i32)]);
                    }
                    l0__u110 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 2;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u114: i32 = 0;

                    if true {
                        let l0__u114: i32 = 0;
                        let e__u115 = v * 4 + l0__u114 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v8 = mulh2(acc.v8, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u114: i32 = 1;
                        let e__u116 = v * 4 + l0__u114 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v10 = mulh2(acc.v10, sh2[ (0 as i32)]);
                    }
                    l0__u114 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u117: i32 = 0;

                    if true {
                        let l0__u117: i32 = 0;
                        let e__u118 = v * 4 + l0__u117 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v9 = mulh2(acc.v9, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u117: i32 = 1;
                        let e__u119 = v * 4 + l0__u117 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v11 = mulh2(acc.v11, sh2[ (1 as i32)]);
                    }
                    l0__u117 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 3;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u121: i32 = 0;

                    if true {
                        let l0__u121: i32 = 0;
                        let e__u122 = v * 4 + l0__u121 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v12 = mulh2(acc.v12, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u121: i32 = 1;
                        let e__u123 = v * 4 + l0__u121 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v14 = mulh2(acc.v14, sh2[ (0 as i32)]);
                    }
                    l0__u121 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u124: i32 = 0;

                    if true {
                        let l0__u124: i32 = 0;
                        let e__u125 = v * 4 + l0__u124 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v13 = mulh2(acc.v13, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u124: i32 = 1;
                        let e__u126 = v * 4 + l0__u124 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v15 = mulh2(acc.v15, sh2[ (1 as i32)]);
                    }
                    l0__u124 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 4;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u128: i32 = 0;

                    if true {
                        let l0__u128: i32 = 0;
                        let e__u129 = v * 4 + l0__u128 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v16 = mulh2(acc.v16, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u128: i32 = 1;
                        let e__u130 = v * 4 + l0__u128 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v18 = mulh2(acc.v18, sh2[ (0 as i32)]);
                    }
                    l0__u128 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u131: i32 = 0;

                    if true {
                        let l0__u131: i32 = 0;
                        let e__u132 = v * 4 + l0__u131 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v17 = mulh2(acc.v17, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u131: i32 = 1;
                        let e__u133 = v * 4 + l0__u131 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v19 = mulh2(acc.v19, sh2[ (1 as i32)]);
                    }
                    l0__u131 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 5;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u135: i32 = 0;

                    if true {
                        let l0__u135: i32 = 0;
                        let e__u136 = v * 4 + l0__u135 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v20 = mulh2(acc.v20, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u135: i32 = 1;
                        let e__u137 = v * 4 + l0__u135 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v22 = mulh2(acc.v22, sh2[ (0 as i32)]);
                    }
                    l0__u135 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u138: i32 = 0;

                    if true {
                        let l0__u138: i32 = 0;
                        let e__u139 = v * 4 + l0__u138 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v21 = mulh2(acc.v21, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u138: i32 = 1;
                        let e__u140 = v * 4 + l0__u138 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v23 = mulh2(acc.v23, sh2[ (1 as i32)]);
                    }
                    l0__u138 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 6;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u142: i32 = 0;

                    if true {
                        let l0__u142: i32 = 0;
                        let e__u143 = v * 4 + l0__u142 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v24 = mulh2(acc.v24, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u142: i32 = 1;
                        let e__u144 = v * 4 + l0__u142 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v26 = mulh2(acc.v26, sh2[ (0 as i32)]);
                    }
                    l0__u142 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u145: i32 = 0;

                    if true {
                        let l0__u145: i32 = 0;
                        let e__u146 = v * 4 + l0__u145 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v25 = mulh2(acc.v25, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u145: i32 = 1;
                        let e__u147 = v * 4 + l0__u145 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v27 = mulh2(acc.v27, sh2[ (1 as i32)]);
                    }
                    l0__u145 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 7;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u149: i32 = 0;

                    if true {
                        let l0__u149: i32 = 0;
                        let e__u150 = v * 4 + l0__u149 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v28 = mulh2(acc.v28, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u149: i32 = 1;
                        let e__u151 = v * 4 + l0__u149 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v30 = mulh2(acc.v30, sh2[ (0 as i32)]);
                    }
                    l0__u149 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u152: i32 = 0;

                    if true {
                        let l0__u152: i32 = 0;
                        let e__u153 = v * 4 + l0__u152 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v29 = mulh2(acc.v29, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u152: i32 = 1;
                        let e__u154 = v * 4 + l0__u152 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v31 = mulh2(acc.v31, sh2[ (1 as i32)]);
                    }
                    l0__u152 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 8;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u156: i32 = 0;

                    if true {
                        let l0__u156: i32 = 0;
                        let e__u157 = v * 4 + l0__u156 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v32 = mulh2(acc.v32, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u156: i32 = 1;
                        let e__u158 = v * 4 + l0__u156 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v34 = mulh2(acc.v34, sh2[ (0 as i32)]);
                    }
                    l0__u156 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u159: i32 = 0;

                    if true {
                        let l0__u159: i32 = 0;
                        let e__u160 = v * 4 + l0__u159 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v33 = mulh2(acc.v33, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u159: i32 = 1;
                        let e__u161 = v * 4 + l0__u159 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v35 = mulh2(acc.v35, sh2[ (1 as i32)]);
                    }
                    l0__u159 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 9;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u163: i32 = 0;

                    if true {
                        let l0__u163: i32 = 0;
                        let e__u164 = v * 4 + l0__u163 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v36 = mulh2(acc.v36, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u163: i32 = 1;
                        let e__u165 = v * 4 + l0__u163 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v38 = mulh2(acc.v38, sh2[ (0 as i32)]);
                    }
                    l0__u163 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u166: i32 = 0;

                    if true {
                        let l0__u166: i32 = 0;
                        let e__u167 = v * 4 + l0__u166 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v37 = mulh2(acc.v37, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u166: i32 = 1;
                        let e__u168 = v * 4 + l0__u166 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v39 = mulh2(acc.v39, sh2[ (1 as i32)]);
                    }
                    l0__u166 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 10;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u170: i32 = 0;

                    if true {
                        let l0__u170: i32 = 0;
                        let e__u171 = v * 4 + l0__u170 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v40 = mulh2(acc.v40, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u170: i32 = 1;
                        let e__u172 = v * 4 + l0__u170 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v42 = mulh2(acc.v42, sh2[ (0 as i32)]);
                    }
                    l0__u170 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u173: i32 = 0;

                    if true {
                        let l0__u173: i32 = 0;
                        let e__u174 = v * 4 + l0__u173 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v41 = mulh2(acc.v41, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u173: i32 = 1;
                        let e__u175 = v * 4 + l0__u173 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v43 = mulh2(acc.v43, sh2[ (1 as i32)]);
                    }
                    l0__u173 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 11;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u177: i32 = 0;

                    if true {
                        let l0__u177: i32 = 0;
                        let e__u178 = v * 4 + l0__u177 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v44 = mulh2(acc.v44, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u177: i32 = 1;
                        let e__u179 = v * 4 + l0__u177 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v46 = mulh2(acc.v46, sh2[ (0 as i32)]);
                    }
                    l0__u177 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u180: i32 = 0;

                    if true {
                        let l0__u180: i32 = 0;
                        let e__u181 = v * 4 + l0__u180 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v45 = mulh2(acc.v45, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u180: i32 = 1;
                        let e__u182 = v * 4 + l0__u180 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v47 = mulh2(acc.v47, sh2[ (1 as i32)]);
                    }
                    l0__u180 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 12;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u184: i32 = 0;

                    if true {
                        let l0__u184: i32 = 0;
                        let e__u185 = v * 4 + l0__u184 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v48 = mulh2(acc.v48, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u184: i32 = 1;
                        let e__u186 = v * 4 + l0__u184 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v50 = mulh2(acc.v50, sh2[ (0 as i32)]);
                    }
                    l0__u184 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u187: i32 = 0;

                    if true {
                        let l0__u187: i32 = 0;
                        let e__u188 = v * 4 + l0__u187 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v49 = mulh2(acc.v49, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u187: i32 = 1;
                        let e__u189 = v * 4 + l0__u187 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v51 = mulh2(acc.v51, sh2[ (1 as i32)]);
                    }
                    l0__u187 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 13;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u191: i32 = 0;

                    if true {
                        let l0__u191: i32 = 0;
                        let e__u192 = v * 4 + l0__u191 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v52 = mulh2(acc.v52, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u191: i32 = 1;
                        let e__u193 = v * 4 + l0__u191 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v54 = mulh2(acc.v54, sh2[ (0 as i32)]);
                    }
                    l0__u191 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u194: i32 = 0;

                    if true {
                        let l0__u194: i32 = 0;
                        let e__u195 = v * 4 + l0__u194 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v53 = mulh2(acc.v53, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u194: i32 = 1;
                        let e__u196 = v * 4 + l0__u194 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v55 = mulh2(acc.v55, sh2[ (1 as i32)]);
                    }
                    l0__u194 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 14;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u198: i32 = 0;

                    if true {
                        let l0__u198: i32 = 0;
                        let e__u199 = v * 4 + l0__u198 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v56 = mulh2(acc.v56, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u198: i32 = 1;
                        let e__u200 = v * 4 + l0__u198 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v58 = mulh2(acc.v58, sh2[ (0 as i32)]);
                    }
                    l0__u198 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u201: i32 = 0;

                    if true {
                        let l0__u201: i32 = 0;
                        let e__u202 = v * 4 + l0__u201 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v57 = mulh2(acc.v57, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u201: i32 = 1;
                        let e__u203 = v * 4 + l0__u201 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v59 = mulh2(acc.v59, sh2[ (1 as i32)]);
                    }
                    l0__u201 = (2 as i32);
                }
                col = (2 as i32);
            }

            if true {
                let v: i32 = 15;
                col = 0;

                if true {
                    let col: i32 = 0;
                    var l0__u205: i32 = 0;

                    if true {
                        let l0__u205: i32 = 0;
                        let e__u206 = v * 4 + l0__u205 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v60 = mulh2(acc.v60, sh2[ (0 as i32)]);
                    }

                    if true {
                        let l0__u205: i32 = 1;
                        let e__u207 = v * 4 + l0__u205 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v62 = mulh2(acc.v62, sh2[ (0 as i32)]);
                    }
                    l0__u205 = (2 as i32);
                }

                if true {
                    let col: i32 = 1;
                    var l0__u208: i32 = 0;

                    if true {
                        let l0__u208: i32 = 0;
                        let e__u209 = v * 4 + l0__u208 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v61 = mulh2(acc.v61, sh2[ (1 as i32)]);
                    }

                    if true {
                        let l0__u208: i32 = 1;
                        let e__u210 = v * 4 + l0__u208 * 2 + col;
                        // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                        acc.v63 = mulh2(acc.v63, sh2[ (1 as i32)]);
                    }
                    l0__u208 = (2 as i32);
                }
                col = (2 as i32);
            }
            v = (16 as i32);
            // ---- B[k] = get_half2(KQ_C[k]): pure F32->half2 pack on Turing, no transpose/shuffle ----
            var b: u32[8] = default < u32[8] > ();
            t = 0;

            if true {
                let t: i32 = 0;
                var l0__u211: i32 = 0;

                if true {
                    let l0__u211: i32 = 0;
                    b[ (0 as i32)] = h2(kqc[ (0 as i32)], kqc[ (1 as i32)]);
                }

                if true {
                    let l0__u211: i32 = 2;
                    b[ (1 as i32)] = h2(kqc[ (2 as i32)], kqc[ (3 as i32)]);
                }

                if true {
                    let l0__u211: i32 = 4;
                    b[ (2 as i32)] = h2(kqc[ (4 as i32)], kqc[ (5 as i32)]);
                }

                if true {
                    let l0__u211: i32 = 6;
                    b[ (3 as i32)] = h2(kqc[ (6 as i32)], kqc[ (7 as i32)]);
                }
                l0__u211 = (8 as i32);
            }

            if true {
                let t: i32 = 1;
                var l0__u216: i32 = 0;

                if true {
                    let l0__u216: i32 = 0;
                    b[ (4 as i32)] = h2(kqc[ (8 as i32)], kqc[ (9 as i32)]);
                }

                if true {
                    let l0__u216: i32 = 2;
                    b[ (5 as i32)] = h2(kqc[ (10 as i32)], kqc[ (11 as i32)]);
                }

                if true {
                    let l0__u216: i32 = 4;
                    b[ (6 as i32)] = h2(kqc[ (12 as i32)], kqc[ (13 as i32)]);
                }

                if true {
                    let l0__u216: i32 = 6;
                    b[ (7 as i32)] = h2(kqc[ (14 as i32)], kqc[ (15 as i32)]);
                }
                l0__u216 = (8 as i32);
            }
            t = (2 as i32);
            // ---- double-stage tail: wait the V tile, sync, then preload next K + mask before the current
            //      VKQ mma (skipped only on the last iteration; waits/barriers stay unconditional) ----
            cpwait();
            barrier();

            if !last {
                load_mask(mask_h, mt, stride_mask, kv_row0 + 32, query0, n_queries, ncols1, tx, ty);
                load_kv(k_h2, kt, stride_k, kv_row0 + 32, tx, ty);
            }
            // ---- VKQ += probs @ V^T, swapped operands (A=B_probs, B=A_V trans-V fragment); f16
            //      accumulation with B registers 0,2 -> D.x[0..1] then 1,3 -> D.x[2..3] ----
            v = 0;

            if true {
                let v: i32 = 0;
                var d0__u221: U2 = U2 {
                    acc.v0, acc.v1
                }
                ;
                var d1__u221: U2 = U2 {
                    acc.v2, acc.v3
                }
                ;
                var kk__u221: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u221: i32 = 0;
                    let k0__u222 = kk__u221 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u222 = ldm4t_tile(ldm_addr(vt, 2 * k0__u222 * 128 + v * 8, tx));
                    d0__u221 = mma_f16(
                        d0__u221,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u222.a,
                        av__u222.c);
                    d1__u221 = mma_f16(
                        d1__u221,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u222.b,
                        av__u222.d);
                }

                if true {
                    let kk__u221: i32 = 1;
                    let k0__u223 = kk__u221 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u223 = ldm4t_tile(ldm_addr(vt, 2 * k0__u223 * 128 + v * 8, tx));
                    d0__u221 = mma_f16(
                        d0__u221,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u223.a,
                        av__u223.c);
                    d1__u221 = mma_f16(
                        d1__u221,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u223.b,
                        av__u223.d);
                }
                kk__u221 = (2 as i32);
                acc.v0 = d0__u221.a;
                acc.v1 = d0__u221.b;
                acc.v2 = d1__u221.a;
                acc.v3 = d1__u221.b;
            }

            if true {
                let v: i32 = 1;
                var d0__u224: U2 = U2 {
                    acc.v4, acc.v5
                }
                ;
                var d1__u224: U2 = U2 {
                    acc.v6, acc.v7
                }
                ;
                var kk__u224: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u224: i32 = 0;
                    let k0__u225 = kk__u224 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u225 = ldm4t_tile(ldm_addr(vt, 2 * k0__u225 * 128 + v * 8, tx));
                    d0__u224 = mma_f16(
                        d0__u224,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u225.a,
                        av__u225.c);
                    d1__u224 = mma_f16(
                        d1__u224,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u225.b,
                        av__u225.d);
                }

                if true {
                    let kk__u224: i32 = 1;
                    let k0__u226 = kk__u224 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u226 = ldm4t_tile(ldm_addr(vt, 2 * k0__u226 * 128 + v * 8, tx));
                    d0__u224 = mma_f16(
                        d0__u224,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u226.a,
                        av__u226.c);
                    d1__u224 = mma_f16(
                        d1__u224,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u226.b,
                        av__u226.d);
                }
                kk__u224 = (2 as i32);
                acc.v4 = d0__u224.a;
                acc.v5 = d0__u224.b;
                acc.v6 = d1__u224.a;
                acc.v7 = d1__u224.b;
            }

            if true {
                let v: i32 = 2;
                var d0__u227: U2 = U2 {
                    acc.v8, acc.v9
                }
                ;
                var d1__u227: U2 = U2 {
                    acc.v10, acc.v11
                }
                ;
                var kk__u227: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u227: i32 = 0;
                    let k0__u228 = kk__u227 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u228 = ldm4t_tile(ldm_addr(vt, 2 * k0__u228 * 128 + v * 8, tx));
                    d0__u227 = mma_f16(
                        d0__u227,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u228.a,
                        av__u228.c);
                    d1__u227 = mma_f16(
                        d1__u227,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u228.b,
                        av__u228.d);
                }

                if true {
                    let kk__u227: i32 = 1;
                    let k0__u229 = kk__u227 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u229 = ldm4t_tile(ldm_addr(vt, 2 * k0__u229 * 128 + v * 8, tx));
                    d0__u227 = mma_f16(
                        d0__u227,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u229.a,
                        av__u229.c);
                    d1__u227 = mma_f16(
                        d1__u227,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u229.b,
                        av__u229.d);
                }
                kk__u227 = (2 as i32);
                acc.v8 = d0__u227.a;
                acc.v9 = d0__u227.b;
                acc.v10 = d1__u227.a;
                acc.v11 = d1__u227.b;
            }

            if true {
                let v: i32 = 3;
                var d0__u230: U2 = U2 {
                    acc.v12, acc.v13
                }
                ;
                var d1__u230: U2 = U2 {
                    acc.v14, acc.v15
                }
                ;
                var kk__u230: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u230: i32 = 0;
                    let k0__u231 = kk__u230 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u231 = ldm4t_tile(ldm_addr(vt, 2 * k0__u231 * 128 + v * 8, tx));
                    d0__u230 = mma_f16(
                        d0__u230,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u231.a,
                        av__u231.c);
                    d1__u230 = mma_f16(
                        d1__u230,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u231.b,
                        av__u231.d);
                }

                if true {
                    let kk__u230: i32 = 1;
                    let k0__u232 = kk__u230 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u232 = ldm4t_tile(ldm_addr(vt, 2 * k0__u232 * 128 + v * 8, tx));
                    d0__u230 = mma_f16(
                        d0__u230,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u232.a,
                        av__u232.c);
                    d1__u230 = mma_f16(
                        d1__u230,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u232.b,
                        av__u232.d);
                }
                kk__u230 = (2 as i32);
                acc.v12 = d0__u230.a;
                acc.v13 = d0__u230.b;
                acc.v14 = d1__u230.a;
                acc.v15 = d1__u230.b;
            }

            if true {
                let v: i32 = 4;
                var d0__u233: U2 = U2 {
                    acc.v16, acc.v17
                }
                ;
                var d1__u233: U2 = U2 {
                    acc.v18, acc.v19
                }
                ;
                var kk__u233: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u233: i32 = 0;
                    let k0__u234 = kk__u233 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u234 = ldm4t_tile(ldm_addr(vt, 2 * k0__u234 * 128 + v * 8, tx));
                    d0__u233 = mma_f16(
                        d0__u233,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u234.a,
                        av__u234.c);
                    d1__u233 = mma_f16(
                        d1__u233,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u234.b,
                        av__u234.d);
                }

                if true {
                    let kk__u233: i32 = 1;
                    let k0__u235 = kk__u233 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u235 = ldm4t_tile(ldm_addr(vt, 2 * k0__u235 * 128 + v * 8, tx));
                    d0__u233 = mma_f16(
                        d0__u233,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u235.a,
                        av__u235.c);
                    d1__u233 = mma_f16(
                        d1__u233,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u235.b,
                        av__u235.d);
                }
                kk__u233 = (2 as i32);
                acc.v16 = d0__u233.a;
                acc.v17 = d0__u233.b;
                acc.v18 = d1__u233.a;
                acc.v19 = d1__u233.b;
            }

            if true {
                let v: i32 = 5;
                var d0__u236: U2 = U2 {
                    acc.v20, acc.v21
                }
                ;
                var d1__u236: U2 = U2 {
                    acc.v22, acc.v23
                }
                ;
                var kk__u236: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u236: i32 = 0;
                    let k0__u237 = kk__u236 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u237 = ldm4t_tile(ldm_addr(vt, 2 * k0__u237 * 128 + v * 8, tx));
                    d0__u236 = mma_f16(
                        d0__u236,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u237.a,
                        av__u237.c);
                    d1__u236 = mma_f16(
                        d1__u236,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u237.b,
                        av__u237.d);
                }

                if true {
                    let kk__u236: i32 = 1;
                    let k0__u238 = kk__u236 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u238 = ldm4t_tile(ldm_addr(vt, 2 * k0__u238 * 128 + v * 8, tx));
                    d0__u236 = mma_f16(
                        d0__u236,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u238.a,
                        av__u238.c);
                    d1__u236 = mma_f16(
                        d1__u236,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u238.b,
                        av__u238.d);
                }
                kk__u236 = (2 as i32);
                acc.v20 = d0__u236.a;
                acc.v21 = d0__u236.b;
                acc.v22 = d1__u236.a;
                acc.v23 = d1__u236.b;
            }

            if true {
                let v: i32 = 6;
                var d0__u239: U2 = U2 {
                    acc.v24, acc.v25
                }
                ;
                var d1__u239: U2 = U2 {
                    acc.v26, acc.v27
                }
                ;
                var kk__u239: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u239: i32 = 0;
                    let k0__u240 = kk__u239 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u240 = ldm4t_tile(ldm_addr(vt, 2 * k0__u240 * 128 + v * 8, tx));
                    d0__u239 = mma_f16(
                        d0__u239,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u240.a,
                        av__u240.c);
                    d1__u239 = mma_f16(
                        d1__u239,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u240.b,
                        av__u240.d);
                }

                if true {
                    let kk__u239: i32 = 1;
                    let k0__u241 = kk__u239 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u241 = ldm4t_tile(ldm_addr(vt, 2 * k0__u241 * 128 + v * 8, tx));
                    d0__u239 = mma_f16(
                        d0__u239,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u241.a,
                        av__u241.c);
                    d1__u239 = mma_f16(
                        d1__u239,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u241.b,
                        av__u241.d);
                }
                kk__u239 = (2 as i32);
                acc.v24 = d0__u239.a;
                acc.v25 = d0__u239.b;
                acc.v26 = d1__u239.a;
                acc.v27 = d1__u239.b;
            }

            if true {
                let v: i32 = 7;
                var d0__u242: U2 = U2 {
                    acc.v28, acc.v29
                }
                ;
                var d1__u242: U2 = U2 {
                    acc.v30, acc.v31
                }
                ;
                var kk__u242: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u242: i32 = 0;
                    let k0__u243 = kk__u242 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u243 = ldm4t_tile(ldm_addr(vt, 2 * k0__u243 * 128 + v * 8, tx));
                    d0__u242 = mma_f16(
                        d0__u242,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u243.a,
                        av__u243.c);
                    d1__u242 = mma_f16(
                        d1__u242,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u243.b,
                        av__u243.d);
                }

                if true {
                    let kk__u242: i32 = 1;
                    let k0__u244 = kk__u242 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u244 = ldm4t_tile(ldm_addr(vt, 2 * k0__u244 * 128 + v * 8, tx));
                    d0__u242 = mma_f16(
                        d0__u242,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u244.a,
                        av__u244.c);
                    d1__u242 = mma_f16(
                        d1__u242,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u244.b,
                        av__u244.d);
                }
                kk__u242 = (2 as i32);
                acc.v28 = d0__u242.a;
                acc.v29 = d0__u242.b;
                acc.v30 = d1__u242.a;
                acc.v31 = d1__u242.b;
            }

            if true {
                let v: i32 = 8;
                var d0__u245: U2 = U2 {
                    acc.v32, acc.v33
                }
                ;
                var d1__u245: U2 = U2 {
                    acc.v34, acc.v35
                }
                ;
                var kk__u245: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u245: i32 = 0;
                    let k0__u246 = kk__u245 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u246 = ldm4t_tile(ldm_addr(vt, 2 * k0__u246 * 128 + v * 8, tx));
                    d0__u245 = mma_f16(
                        d0__u245,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u246.a,
                        av__u246.c);
                    d1__u245 = mma_f16(
                        d1__u245,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u246.b,
                        av__u246.d);
                }

                if true {
                    let kk__u245: i32 = 1;
                    let k0__u247 = kk__u245 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u247 = ldm4t_tile(ldm_addr(vt, 2 * k0__u247 * 128 + v * 8, tx));
                    d0__u245 = mma_f16(
                        d0__u245,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u247.a,
                        av__u247.c);
                    d1__u245 = mma_f16(
                        d1__u245,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u247.b,
                        av__u247.d);
                }
                kk__u245 = (2 as i32);
                acc.v32 = d0__u245.a;
                acc.v33 = d0__u245.b;
                acc.v34 = d1__u245.a;
                acc.v35 = d1__u245.b;
            }

            if true {
                let v: i32 = 9;
                var d0__u248: U2 = U2 {
                    acc.v36, acc.v37
                }
                ;
                var d1__u248: U2 = U2 {
                    acc.v38, acc.v39
                }
                ;
                var kk__u248: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u248: i32 = 0;
                    let k0__u249 = kk__u248 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u249 = ldm4t_tile(ldm_addr(vt, 2 * k0__u249 * 128 + v * 8, tx));
                    d0__u248 = mma_f16(
                        d0__u248,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u249.a,
                        av__u249.c);
                    d1__u248 = mma_f16(
                        d1__u248,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u249.b,
                        av__u249.d);
                }

                if true {
                    let kk__u248: i32 = 1;
                    let k0__u250 = kk__u248 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u250 = ldm4t_tile(ldm_addr(vt, 2 * k0__u250 * 128 + v * 8, tx));
                    d0__u248 = mma_f16(
                        d0__u248,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u250.a,
                        av__u250.c);
                    d1__u248 = mma_f16(
                        d1__u248,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u250.b,
                        av__u250.d);
                }
                kk__u248 = (2 as i32);
                acc.v36 = d0__u248.a;
                acc.v37 = d0__u248.b;
                acc.v38 = d1__u248.a;
                acc.v39 = d1__u248.b;
            }

            if true {
                let v: i32 = 10;
                var d0__u251: U2 = U2 {
                    acc.v40, acc.v41
                }
                ;
                var d1__u251: U2 = U2 {
                    acc.v42, acc.v43
                }
                ;
                var kk__u251: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u251: i32 = 0;
                    let k0__u252 = kk__u251 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u252 = ldm4t_tile(ldm_addr(vt, 2 * k0__u252 * 128 + v * 8, tx));
                    d0__u251 = mma_f16(
                        d0__u251,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u252.a,
                        av__u252.c);
                    d1__u251 = mma_f16(
                        d1__u251,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u252.b,
                        av__u252.d);
                }

                if true {
                    let kk__u251: i32 = 1;
                    let k0__u253 = kk__u251 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u253 = ldm4t_tile(ldm_addr(vt, 2 * k0__u253 * 128 + v * 8, tx));
                    d0__u251 = mma_f16(
                        d0__u251,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u253.a,
                        av__u253.c);
                    d1__u251 = mma_f16(
                        d1__u251,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u253.b,
                        av__u253.d);
                }
                kk__u251 = (2 as i32);
                acc.v40 = d0__u251.a;
                acc.v41 = d0__u251.b;
                acc.v42 = d1__u251.a;
                acc.v43 = d1__u251.b;
            }

            if true {
                let v: i32 = 11;
                var d0__u254: U2 = U2 {
                    acc.v44, acc.v45
                }
                ;
                var d1__u254: U2 = U2 {
                    acc.v46, acc.v47
                }
                ;
                var kk__u254: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u254: i32 = 0;
                    let k0__u255 = kk__u254 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u255 = ldm4t_tile(ldm_addr(vt, 2 * k0__u255 * 128 + v * 8, tx));
                    d0__u254 = mma_f16(
                        d0__u254,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u255.a,
                        av__u255.c);
                    d1__u254 = mma_f16(
                        d1__u254,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u255.b,
                        av__u255.d);
                }

                if true {
                    let kk__u254: i32 = 1;
                    let k0__u256 = kk__u254 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u256 = ldm4t_tile(ldm_addr(vt, 2 * k0__u256 * 128 + v * 8, tx));
                    d0__u254 = mma_f16(
                        d0__u254,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u256.a,
                        av__u256.c);
                    d1__u254 = mma_f16(
                        d1__u254,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u256.b,
                        av__u256.d);
                }
                kk__u254 = (2 as i32);
                acc.v44 = d0__u254.a;
                acc.v45 = d0__u254.b;
                acc.v46 = d1__u254.a;
                acc.v47 = d1__u254.b;
            }

            if true {
                let v: i32 = 12;
                var d0__u257: U2 = U2 {
                    acc.v48, acc.v49
                }
                ;
                var d1__u257: U2 = U2 {
                    acc.v50, acc.v51
                }
                ;
                var kk__u257: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u257: i32 = 0;
                    let k0__u258 = kk__u257 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u258 = ldm4t_tile(ldm_addr(vt, 2 * k0__u258 * 128 + v * 8, tx));
                    d0__u257 = mma_f16(
                        d0__u257,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u258.a,
                        av__u258.c);
                    d1__u257 = mma_f16(
                        d1__u257,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u258.b,
                        av__u258.d);
                }

                if true {
                    let kk__u257: i32 = 1;
                    let k0__u259 = kk__u257 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u259 = ldm4t_tile(ldm_addr(vt, 2 * k0__u259 * 128 + v * 8, tx));
                    d0__u257 = mma_f16(
                        d0__u257,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u259.a,
                        av__u259.c);
                    d1__u257 = mma_f16(
                        d1__u257,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u259.b,
                        av__u259.d);
                }
                kk__u257 = (2 as i32);
                acc.v48 = d0__u257.a;
                acc.v49 = d0__u257.b;
                acc.v50 = d1__u257.a;
                acc.v51 = d1__u257.b;
            }

            if true {
                let v: i32 = 13;
                var d0__u260: U2 = U2 {
                    acc.v52, acc.v53
                }
                ;
                var d1__u260: U2 = U2 {
                    acc.v54, acc.v55
                }
                ;
                var kk__u260: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u260: i32 = 0;
                    let k0__u261 = kk__u260 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u261 = ldm4t_tile(ldm_addr(vt, 2 * k0__u261 * 128 + v * 8, tx));
                    d0__u260 = mma_f16(
                        d0__u260,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u261.a,
                        av__u261.c);
                    d1__u260 = mma_f16(
                        d1__u260,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u261.b,
                        av__u261.d);
                }

                if true {
                    let kk__u260: i32 = 1;
                    let k0__u262 = kk__u260 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u262 = ldm4t_tile(ldm_addr(vt, 2 * k0__u262 * 128 + v * 8, tx));
                    d0__u260 = mma_f16(
                        d0__u260,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u262.a,
                        av__u262.c);
                    d1__u260 = mma_f16(
                        d1__u260,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u262.b,
                        av__u262.d);
                }
                kk__u260 = (2 as i32);
                acc.v52 = d0__u260.a;
                acc.v53 = d0__u260.b;
                acc.v54 = d1__u260.a;
                acc.v55 = d1__u260.b;
            }

            if true {
                let v: i32 = 14;
                var d0__u263: U2 = U2 {
                    acc.v56, acc.v57
                }
                ;
                var d1__u263: U2 = U2 {
                    acc.v58, acc.v59
                }
                ;
                var kk__u263: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u263: i32 = 0;
                    let k0__u264 = kk__u263 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u264 = ldm4t_tile(ldm_addr(vt, 2 * k0__u264 * 128 + v * 8, tx));
                    d0__u263 = mma_f16(
                        d0__u263,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u264.a,
                        av__u264.c);
                    d1__u263 = mma_f16(
                        d1__u263,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u264.b,
                        av__u264.d);
                }

                if true {
                    let kk__u263: i32 = 1;
                    let k0__u265 = kk__u263 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u265 = ldm4t_tile(ldm_addr(vt, 2 * k0__u265 * 128 + v * 8, tx));
                    d0__u263 = mma_f16(
                        d0__u263,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u265.a,
                        av__u265.c);
                    d1__u263 = mma_f16(
                        d1__u263,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u265.b,
                        av__u265.d);
                }
                kk__u263 = (2 as i32);
                acc.v56 = d0__u263.a;
                acc.v57 = d0__u263.b;
                acc.v58 = d1__u263.a;
                acc.v59 = d1__u263.b;
            }

            if true {
                let v: i32 = 15;
                var d0__u266: U2 = U2 {
                    acc.v60, acc.v61
                }
                ;
                var d1__u266: U2 = U2 {
                    acc.v62, acc.v63
                }
                ;
                var kk__u266: i32 = 0;
                // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

                if true {
                    let kk__u266: i32 = 0;
                    let k0__u267 = kk__u266 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u267 = ldm4t_tile(ldm_addr(vt, 2 * k0__u267 * 128 + v * 8, tx));
                    d0__u266 = mma_f16(
                        d0__u266,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u267.a,
                        av__u267.c);
                    d1__u266 = mma_f16(
                        d1__u266,
                        b[ (0 as i32)],
                        b[ (1 as i32)],
                        b[ (2 as i32)],
                        b[ (3 as i32)],
                        av__u267.b,
                        av__u267.d);
                }

                if true {
                    let kk__u266: i32 = 1;
                    let k0__u268 = kk__u266 * np * 8 + rh / 2;
                    // (ty%np)*T_A_VKQ::J KV-row-pair base
                    // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                    let av__u268 = ldm4t_tile(ldm_addr(vt, 2 * k0__u268 * 128 + v * 8, tx));
                    d0__u266 = mma_f16(
                        d0__u266,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u268.a,
                        av__u268.c);
                    d1__u266 = mma_f16(
                        d1__u266,
                        b[ (4 as i32)],
                        b[ (5 as i32)],
                        b[ (6 as i32)],
                        b[ (7 as i32)],
                        av__u268.b,
                        av__u268.d);
                }
                kk__u266 = (2 as i32);
                acc.v60 = d0__u266.a;
                acc.v61 = d0__u266.b;
                acc.v62 = d1__u266.a;
                acc.v63 = d1__u266.b;
            }
            v = (16 as i32);
        }
        kb0 += 1;
    }

    if true {
        let n_queries: i64 = ne01z;
        let last: bool = true;
        let ncols1: i32 = 8;
        let np: i32 = 1;
        let kt = smem;
        let vt = smem + (16384 as u64);
        let mt = smem + (32768 as u64);
        let kv_row0 = (kb0 as i64) * 32;
        // k_VKQ_0 = kb0*nbatch_fa
        let query0 = (jt as i64) * (ncols1 as i64);
        // j0 for the mask loader
        let cb = (ty / np) * 16;
        // warp Q-column block base (cols_per_warp=16)
        let rh = (ty % np) * 16;
        // warp KV-row half base (np*T_A_KQ::I split)
        let nt = 32 / (np * 16);
        // KQ_C/B tiles per warp: 1 (np=2) or 2 (np=1)
        let kqoff = (3.0 as f32) * (0.6931 as f32);
        // FATTN_KQ_MAX_OFFSET
        var t: i32 = 0;
        // nstages>1 entry: this iteration's K tile + mask were preloaded by the prologue or the previous
        // iter tail; wait for them, then issue the V tile of this iteration into the pipeline.
        cpwait();
        barrier();
        load_kv(v_h2, vt, stride_v, kv_row0, tx, ty);
        // ---- KQ = Q_B @ K_A over the single nbatch_K2 block k in [0,128) half2 (16 x T_A_KQ::J=8) ----
        var kqc: f32[16] = default < f32[16] > ();
        // up to 2 fresh zero tile<16,16,float> C tiles
        var q: i32 = 0;
        // Q_B tile index = k_KQ_0/8

        if true {
            let q: i32 = 0;
            let q0__u1 = qr.q0;
            let q1__u1 = qr.q1;
            let q2__u1 = qr.q2;
            let q3__u1 = qr.q3;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u2 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u2 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u1, q1__u1, q2__u1, q3__u1, ka__u2.a, ka__u2.c);
                kqc[ (0 as i32)] = lo__u2.x;
                kqc[ (1 as i32)] = lo__u2.y;
                kqc[ (2 as i32)] = lo__u2.z;
                kqc[ (3 as i32)] = lo__u2.w;
                let hi__u2 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u1, q1__u1, q2__u1, q3__u1, ka__u2.b, ka__u2.d);
                kqc[ (4 as i32)] = hi__u2.x;
                kqc[ (5 as i32)] = hi__u2.y;
                kqc[ (6 as i32)] = hi__u2.z;
                kqc[ (7 as i32)] = hi__u2.w;
            }

            if true {
                let t: i32 = 1;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u3 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u3 = mma_f32(F4 {
                    kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                }
                , q0__u1, q1__u1, q2__u1, q3__u1, ka__u3.a, ka__u3.c);
                kqc[ (8 as i32)] = lo__u3.x;
                kqc[ (9 as i32)] = lo__u3.y;
                kqc[ (10 as i32)] = lo__u3.z;
                kqc[ (11 as i32)] = lo__u3.w;
                let hi__u3 = mma_f32(F4 {
                    kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                }
                , q0__u1, q1__u1, q2__u1, q3__u1, ka__u3.b, ka__u3.d);
                kqc[ (12 as i32)] = hi__u3.x;
                kqc[ (13 as i32)] = hi__u3.y;
                kqc[ (14 as i32)] = hi__u3.z;
                kqc[ (15 as i32)] = hi__u3.w;
            }
            t = (2 as i32);
        }

        if true {
            let q: i32 = 1;
            let q0__u4 = qr.q4;
            let q1__u4 = qr.q5;
            let q2__u4 = qr.q6;
            let q3__u4 = qr.q7;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u5 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u5 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u4, q1__u4, q2__u4, q3__u4, ka__u5.a, ka__u5.c);
                kqc[ (0 as i32)] = lo__u5.x;
                kqc[ (1 as i32)] = lo__u5.y;
                kqc[ (2 as i32)] = lo__u5.z;
                kqc[ (3 as i32)] = lo__u5.w;
                let hi__u5 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u4, q1__u4, q2__u4, q3__u4, ka__u5.b, ka__u5.d);
                kqc[ (4 as i32)] = hi__u5.x;
                kqc[ (5 as i32)] = hi__u5.y;
                kqc[ (6 as i32)] = hi__u5.z;
                kqc[ (7 as i32)] = hi__u5.w;
            }

            if true {
                let t: i32 = 1;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u6 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u6 = mma_f32(F4 {
                    kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                }
                , q0__u4, q1__u4, q2__u4, q3__u4, ka__u6.a, ka__u6.c);
                kqc[ (8 as i32)] = lo__u6.x;
                kqc[ (9 as i32)] = lo__u6.y;
                kqc[ (10 as i32)] = lo__u6.z;
                kqc[ (11 as i32)] = lo__u6.w;
                let hi__u6 = mma_f32(F4 {
                    kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                }
                , q0__u4, q1__u4, q2__u4, q3__u4, ka__u6.b, ka__u6.d);
                kqc[ (12 as i32)] = hi__u6.x;
                kqc[ (13 as i32)] = hi__u6.y;
                kqc[ (14 as i32)] = hi__u6.z;
                kqc[ (15 as i32)] = hi__u6.w;
            }
            t = (2 as i32);
        }

        if true {
            let q: i32 = 2;
            let q0__u7 = qr.q8;
            let q1__u7 = qr.q9;
            let q2__u7 = qr.q10;
            let q3__u7 = qr.q11;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u8 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u8 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u7, q1__u7, q2__u7, q3__u7, ka__u8.a, ka__u8.c);
                kqc[ (0 as i32)] = lo__u8.x;
                kqc[ (1 as i32)] = lo__u8.y;
                kqc[ (2 as i32)] = lo__u8.z;
                kqc[ (3 as i32)] = lo__u8.w;
                let hi__u8 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u7, q1__u7, q2__u7, q3__u7, ka__u8.b, ka__u8.d);
                kqc[ (4 as i32)] = hi__u8.x;
                kqc[ (5 as i32)] = hi__u8.y;
                kqc[ (6 as i32)] = hi__u8.z;
                kqc[ (7 as i32)] = hi__u8.w;
            }

            if true {
                let t: i32 = 1;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u9 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u9 = mma_f32(F4 {
                    kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                }
                , q0__u7, q1__u7, q2__u7, q3__u7, ka__u9.a, ka__u9.c);
                kqc[ (8 as i32)] = lo__u9.x;
                kqc[ (9 as i32)] = lo__u9.y;
                kqc[ (10 as i32)] = lo__u9.z;
                kqc[ (11 as i32)] = lo__u9.w;
                let hi__u9 = mma_f32(F4 {
                    kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                }
                , q0__u7, q1__u7, q2__u7, q3__u7, ka__u9.b, ka__u9.d);
                kqc[ (12 as i32)] = hi__u9.x;
                kqc[ (13 as i32)] = hi__u9.y;
                kqc[ (14 as i32)] = hi__u9.z;
                kqc[ (15 as i32)] = hi__u9.w;
            }
            t = (2 as i32);
        }

        if true {
            let q: i32 = 3;
            let q0__u10 = qr.q12;
            let q1__u10 = qr.q13;
            let q2__u10 = qr.q14;
            let q3__u10 = qr.q15;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u11 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u11 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u10, q1__u10, q2__u10, q3__u10, ka__u11.a, ka__u11.c);
                kqc[ (0 as i32)] = lo__u11.x;
                kqc[ (1 as i32)] = lo__u11.y;
                kqc[ (2 as i32)] = lo__u11.z;
                kqc[ (3 as i32)] = lo__u11.w;
                let hi__u11 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u10, q1__u10, q2__u10, q3__u10, ka__u11.b, ka__u11.d);
                kqc[ (4 as i32)] = hi__u11.x;
                kqc[ (5 as i32)] = hi__u11.y;
                kqc[ (6 as i32)] = hi__u11.z;
                kqc[ (7 as i32)] = hi__u11.w;
            }

            if true {
                let t: i32 = 1;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u12 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u12 = mma_f32(F4 {
                    kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                }
                , q0__u10, q1__u10, q2__u10, q3__u10, ka__u12.a, ka__u12.c);
                kqc[ (8 as i32)] = lo__u12.x;
                kqc[ (9 as i32)] = lo__u12.y;
                kqc[ (10 as i32)] = lo__u12.z;
                kqc[ (11 as i32)] = lo__u12.w;
                let hi__u12 = mma_f32(F4 {
                    kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                }
                , q0__u10, q1__u10, q2__u10, q3__u10, ka__u12.b, ka__u12.d);
                kqc[ (12 as i32)] = hi__u12.x;
                kqc[ (13 as i32)] = hi__u12.y;
                kqc[ (14 as i32)] = hi__u12.z;
                kqc[ (15 as i32)] = hi__u12.w;
            }
            t = (2 as i32);
        }

        if true {
            let q: i32 = 4;
            let q0__u13 = qr.q16;
            let q1__u13 = qr.q17;
            let q2__u13 = qr.q18;
            let q3__u13 = qr.q19;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u14 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u14 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u13, q1__u13, q2__u13, q3__u13, ka__u14.a, ka__u14.c);
                kqc[ (0 as i32)] = lo__u14.x;
                kqc[ (1 as i32)] = lo__u14.y;
                kqc[ (2 as i32)] = lo__u14.z;
                kqc[ (3 as i32)] = lo__u14.w;
                let hi__u14 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u13, q1__u13, q2__u13, q3__u13, ka__u14.b, ka__u14.d);
                kqc[ (4 as i32)] = hi__u14.x;
                kqc[ (5 as i32)] = hi__u14.y;
                kqc[ (6 as i32)] = hi__u14.z;
                kqc[ (7 as i32)] = hi__u14.w;
            }

            if true {
                let t: i32 = 1;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u15 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u15 = mma_f32(F4 {
                    kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                }
                , q0__u13, q1__u13, q2__u13, q3__u13, ka__u15.a, ka__u15.c);
                kqc[ (8 as i32)] = lo__u15.x;
                kqc[ (9 as i32)] = lo__u15.y;
                kqc[ (10 as i32)] = lo__u15.z;
                kqc[ (11 as i32)] = lo__u15.w;
                let hi__u15 = mma_f32(F4 {
                    kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                }
                , q0__u13, q1__u13, q2__u13, q3__u13, ka__u15.b, ka__u15.d);
                kqc[ (12 as i32)] = hi__u15.x;
                kqc[ (13 as i32)] = hi__u15.y;
                kqc[ (14 as i32)] = hi__u15.z;
                kqc[ (15 as i32)] = hi__u15.w;
            }
            t = (2 as i32);
        }

        if true {
            let q: i32 = 5;
            let q0__u16 = qr.q20;
            let q1__u16 = qr.q21;
            let q2__u16 = qr.q22;
            let q3__u16 = qr.q23;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u17 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u17 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u16, q1__u16, q2__u16, q3__u16, ka__u17.a, ka__u17.c);
                kqc[ (0 as i32)] = lo__u17.x;
                kqc[ (1 as i32)] = lo__u17.y;
                kqc[ (2 as i32)] = lo__u17.z;
                kqc[ (3 as i32)] = lo__u17.w;
                let hi__u17 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u16, q1__u16, q2__u16, q3__u16, ka__u17.b, ka__u17.d);
                kqc[ (4 as i32)] = hi__u17.x;
                kqc[ (5 as i32)] = hi__u17.y;
                kqc[ (6 as i32)] = hi__u17.z;
                kqc[ (7 as i32)] = hi__u17.w;
            }

            if true {
                let t: i32 = 1;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u18 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u18 = mma_f32(F4 {
                    kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                }
                , q0__u16, q1__u16, q2__u16, q3__u16, ka__u18.a, ka__u18.c);
                kqc[ (8 as i32)] = lo__u18.x;
                kqc[ (9 as i32)] = lo__u18.y;
                kqc[ (10 as i32)] = lo__u18.z;
                kqc[ (11 as i32)] = lo__u18.w;
                let hi__u18 = mma_f32(F4 {
                    kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                }
                , q0__u16, q1__u16, q2__u16, q3__u16, ka__u18.b, ka__u18.d);
                kqc[ (12 as i32)] = hi__u18.x;
                kqc[ (13 as i32)] = hi__u18.y;
                kqc[ (14 as i32)] = hi__u18.z;
                kqc[ (15 as i32)] = hi__u18.w;
            }
            t = (2 as i32);
        }

        if true {
            let q: i32 = 6;
            let q0__u19 = qr.q24;
            let q1__u19 = qr.q25;
            let q2__u19 = qr.q26;
            let q3__u19 = qr.q27;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u20 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u20 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u19, q1__u19, q2__u19, q3__u19, ka__u20.a, ka__u20.c);
                kqc[ (0 as i32)] = lo__u20.x;
                kqc[ (1 as i32)] = lo__u20.y;
                kqc[ (2 as i32)] = lo__u20.z;
                kqc[ (3 as i32)] = lo__u20.w;
                let hi__u20 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u19, q1__u19, q2__u19, q3__u19, ka__u20.b, ka__u20.d);
                kqc[ (4 as i32)] = hi__u20.x;
                kqc[ (5 as i32)] = hi__u20.y;
                kqc[ (6 as i32)] = hi__u20.z;
                kqc[ (7 as i32)] = hi__u20.w;
            }

            if true {
                let t: i32 = 1;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u21 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u21 = mma_f32(F4 {
                    kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                }
                , q0__u19, q1__u19, q2__u19, q3__u19, ka__u21.a, ka__u21.c);
                kqc[ (8 as i32)] = lo__u21.x;
                kqc[ (9 as i32)] = lo__u21.y;
                kqc[ (10 as i32)] = lo__u21.z;
                kqc[ (11 as i32)] = lo__u21.w;
                let hi__u21 = mma_f32(F4 {
                    kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                }
                , q0__u19, q1__u19, q2__u19, q3__u19, ka__u21.b, ka__u21.d);
                kqc[ (12 as i32)] = hi__u21.x;
                kqc[ (13 as i32)] = hi__u21.y;
                kqc[ (14 as i32)] = hi__u21.z;
                kqc[ (15 as i32)] = hi__u21.w;
            }
            t = (2 as i32);
        }

        if true {
            let q: i32 = 7;
            let q0__u22 = qr.q28;
            let q1__u22 = qr.q29;
            let q2__u22 = qr.q30;
            let q3__u22 = qr.q31;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u23 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u23 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u22, q1__u22, q2__u22, q3__u22, ka__u23.a, ka__u23.c);
                kqc[ (0 as i32)] = lo__u23.x;
                kqc[ (1 as i32)] = lo__u23.y;
                kqc[ (2 as i32)] = lo__u23.z;
                kqc[ (3 as i32)] = lo__u23.w;
                let hi__u23 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u22, q1__u22, q2__u22, q3__u22, ka__u23.b, ka__u23.d);
                kqc[ (4 as i32)] = hi__u23.x;
                kqc[ (5 as i32)] = hi__u23.y;
                kqc[ (6 as i32)] = hi__u23.z;
                kqc[ (7 as i32)] = hi__u23.w;
            }

            if true {
                let t: i32 = 1;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u24 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u24 = mma_f32(F4 {
                    kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                }
                , q0__u22, q1__u22, q2__u22, q3__u22, ka__u24.a, ka__u24.c);
                kqc[ (8 as i32)] = lo__u24.x;
                kqc[ (9 as i32)] = lo__u24.y;
                kqc[ (10 as i32)] = lo__u24.z;
                kqc[ (11 as i32)] = lo__u24.w;
                let hi__u24 = mma_f32(F4 {
                    kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                }
                , q0__u22, q1__u22, q2__u22, q3__u22, ka__u24.b, ka__u24.d);
                kqc[ (12 as i32)] = hi__u24.x;
                kqc[ (13 as i32)] = hi__u24.y;
                kqc[ (14 as i32)] = hi__u24.z;
                kqc[ (15 as i32)] = hi__u24.w;
            }
            t = (2 as i32);
        }

        if true {
            let q: i32 = 8;
            let q0__u25 = qr.q32;
            let q1__u25 = qr.q33;
            let q2__u25 = qr.q34;
            let q3__u25 = qr.q35;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u26 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u26 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u25, q1__u25, q2__u25, q3__u25, ka__u26.a, ka__u26.c);
                kqc[ (0 as i32)] = lo__u26.x;
                kqc[ (1 as i32)] = lo__u26.y;
                kqc[ (2 as i32)] = lo__u26.z;
                kqc[ (3 as i32)] = lo__u26.w;
                let hi__u26 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u25, q1__u25, q2__u25, q3__u25, ka__u26.b, ka__u26.d);
                kqc[ (4 as i32)] = hi__u26.x;
                kqc[ (5 as i32)] = hi__u26.y;
                kqc[ (6 as i32)] = hi__u26.z;
                kqc[ (7 as i32)] = hi__u26.w;
            }

            if true {
                let t: i32 = 1;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u27 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u27 = mma_f32(F4 {
                    kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                }
                , q0__u25, q1__u25, q2__u25, q3__u25, ka__u27.a, ka__u27.c);
                kqc[ (8 as i32)] = lo__u27.x;
                kqc[ (9 as i32)] = lo__u27.y;
                kqc[ (10 as i32)] = lo__u27.z;
                kqc[ (11 as i32)] = lo__u27.w;
                let hi__u27 = mma_f32(F4 {
                    kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                }
                , q0__u25, q1__u25, q2__u25, q3__u25, ka__u27.b, ka__u27.d);
                kqc[ (12 as i32)] = hi__u27.x;
                kqc[ (13 as i32)] = hi__u27.y;
                kqc[ (14 as i32)] = hi__u27.z;
                kqc[ (15 as i32)] = hi__u27.w;
            }
            t = (2 as i32);
        }

        if true {
            let q: i32 = 9;
            let q0__u28 = qr.q36;
            let q1__u28 = qr.q37;
            let q2__u28 = qr.q38;
            let q3__u28 = qr.q39;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u29 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u29 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u28, q1__u28, q2__u28, q3__u28, ka__u29.a, ka__u29.c);
                kqc[ (0 as i32)] = lo__u29.x;
                kqc[ (1 as i32)] = lo__u29.y;
                kqc[ (2 as i32)] = lo__u29.z;
                kqc[ (3 as i32)] = lo__u29.w;
                let hi__u29 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u28, q1__u28, q2__u28, q3__u28, ka__u29.b, ka__u29.d);
                kqc[ (4 as i32)] = hi__u29.x;
                kqc[ (5 as i32)] = hi__u29.y;
                kqc[ (6 as i32)] = hi__u29.z;
                kqc[ (7 as i32)] = hi__u29.w;
            }

            if true {
                let t: i32 = 1;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u30 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u30 = mma_f32(F4 {
                    kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                }
                , q0__u28, q1__u28, q2__u28, q3__u28, ka__u30.a, ka__u30.c);
                kqc[ (8 as i32)] = lo__u30.x;
                kqc[ (9 as i32)] = lo__u30.y;
                kqc[ (10 as i32)] = lo__u30.z;
                kqc[ (11 as i32)] = lo__u30.w;
                let hi__u30 = mma_f32(F4 {
                    kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                }
                , q0__u28, q1__u28, q2__u28, q3__u28, ka__u30.b, ka__u30.d);
                kqc[ (12 as i32)] = hi__u30.x;
                kqc[ (13 as i32)] = hi__u30.y;
                kqc[ (14 as i32)] = hi__u30.z;
                kqc[ (15 as i32)] = hi__u30.w;
            }
            t = (2 as i32);
        }

        if true {
            let q: i32 = 10;
            let q0__u31 = qr.q40;
            let q1__u31 = qr.q41;
            let q2__u31 = qr.q42;
            let q3__u31 = qr.q43;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u32 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u32 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u31, q1__u31, q2__u31, q3__u31, ka__u32.a, ka__u32.c);
                kqc[ (0 as i32)] = lo__u32.x;
                kqc[ (1 as i32)] = lo__u32.y;
                kqc[ (2 as i32)] = lo__u32.z;
                kqc[ (3 as i32)] = lo__u32.w;
                let hi__u32 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u31, q1__u31, q2__u31, q3__u31, ka__u32.b, ka__u32.d);
                kqc[ (4 as i32)] = hi__u32.x;
                kqc[ (5 as i32)] = hi__u32.y;
                kqc[ (6 as i32)] = hi__u32.z;
                kqc[ (7 as i32)] = hi__u32.w;
            }

            if true {
                let t: i32 = 1;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u33 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u33 = mma_f32(F4 {
                    kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                }
                , q0__u31, q1__u31, q2__u31, q3__u31, ka__u33.a, ka__u33.c);
                kqc[ (8 as i32)] = lo__u33.x;
                kqc[ (9 as i32)] = lo__u33.y;
                kqc[ (10 as i32)] = lo__u33.z;
                kqc[ (11 as i32)] = lo__u33.w;
                let hi__u33 = mma_f32(F4 {
                    kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                }
                , q0__u31, q1__u31, q2__u31, q3__u31, ka__u33.b, ka__u33.d);
                kqc[ (12 as i32)] = hi__u33.x;
                kqc[ (13 as i32)] = hi__u33.y;
                kqc[ (14 as i32)] = hi__u33.z;
                kqc[ (15 as i32)] = hi__u33.w;
            }
            t = (2 as i32);
        }

        if true {
            let q: i32 = 11;
            let q0__u34 = qr.q44;
            let q1__u34 = qr.q45;
            let q2__u34 = qr.q46;
            let q3__u34 = qr.q47;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u35 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u35 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u34, q1__u34, q2__u34, q3__u34, ka__u35.a, ka__u35.c);
                kqc[ (0 as i32)] = lo__u35.x;
                kqc[ (1 as i32)] = lo__u35.y;
                kqc[ (2 as i32)] = lo__u35.z;
                kqc[ (3 as i32)] = lo__u35.w;
                let hi__u35 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u34, q1__u34, q2__u34, q3__u34, ka__u35.b, ka__u35.d);
                kqc[ (4 as i32)] = hi__u35.x;
                kqc[ (5 as i32)] = hi__u35.y;
                kqc[ (6 as i32)] = hi__u35.z;
                kqc[ (7 as i32)] = hi__u35.w;
            }

            if true {
                let t: i32 = 1;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u36 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u36 = mma_f32(F4 {
                    kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                }
                , q0__u34, q1__u34, q2__u34, q3__u34, ka__u36.a, ka__u36.c);
                kqc[ (8 as i32)] = lo__u36.x;
                kqc[ (9 as i32)] = lo__u36.y;
                kqc[ (10 as i32)] = lo__u36.z;
                kqc[ (11 as i32)] = lo__u36.w;
                let hi__u36 = mma_f32(F4 {
                    kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                }
                , q0__u34, q1__u34, q2__u34, q3__u34, ka__u36.b, ka__u36.d);
                kqc[ (12 as i32)] = hi__u36.x;
                kqc[ (13 as i32)] = hi__u36.y;
                kqc[ (14 as i32)] = hi__u36.z;
                kqc[ (15 as i32)] = hi__u36.w;
            }
            t = (2 as i32);
        }

        if true {
            let q: i32 = 12;
            let q0__u37 = qr.q48;
            let q1__u37 = qr.q49;
            let q2__u37 = qr.q50;
            let q3__u37 = qr.q51;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u38 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u38 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u37, q1__u37, q2__u37, q3__u37, ka__u38.a, ka__u38.c);
                kqc[ (0 as i32)] = lo__u38.x;
                kqc[ (1 as i32)] = lo__u38.y;
                kqc[ (2 as i32)] = lo__u38.z;
                kqc[ (3 as i32)] = lo__u38.w;
                let hi__u38 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u37, q1__u37, q2__u37, q3__u37, ka__u38.b, ka__u38.d);
                kqc[ (4 as i32)] = hi__u38.x;
                kqc[ (5 as i32)] = hi__u38.y;
                kqc[ (6 as i32)] = hi__u38.z;
                kqc[ (7 as i32)] = hi__u38.w;
            }

            if true {
                let t: i32 = 1;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u39 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u39 = mma_f32(F4 {
                    kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                }
                , q0__u37, q1__u37, q2__u37, q3__u37, ka__u39.a, ka__u39.c);
                kqc[ (8 as i32)] = lo__u39.x;
                kqc[ (9 as i32)] = lo__u39.y;
                kqc[ (10 as i32)] = lo__u39.z;
                kqc[ (11 as i32)] = lo__u39.w;
                let hi__u39 = mma_f32(F4 {
                    kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                }
                , q0__u37, q1__u37, q2__u37, q3__u37, ka__u39.b, ka__u39.d);
                kqc[ (12 as i32)] = hi__u39.x;
                kqc[ (13 as i32)] = hi__u39.y;
                kqc[ (14 as i32)] = hi__u39.z;
                kqc[ (15 as i32)] = hi__u39.w;
            }
            t = (2 as i32);
        }

        if true {
            let q: i32 = 13;
            let q0__u40 = qr.q52;
            let q1__u40 = qr.q53;
            let q2__u40 = qr.q54;
            let q3__u40 = qr.q55;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u41 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u41 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u40, q1__u40, q2__u40, q3__u40, ka__u41.a, ka__u41.c);
                kqc[ (0 as i32)] = lo__u41.x;
                kqc[ (1 as i32)] = lo__u41.y;
                kqc[ (2 as i32)] = lo__u41.z;
                kqc[ (3 as i32)] = lo__u41.w;
                let hi__u41 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u40, q1__u40, q2__u40, q3__u40, ka__u41.b, ka__u41.d);
                kqc[ (4 as i32)] = hi__u41.x;
                kqc[ (5 as i32)] = hi__u41.y;
                kqc[ (6 as i32)] = hi__u41.z;
                kqc[ (7 as i32)] = hi__u41.w;
            }

            if true {
                let t: i32 = 1;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u42 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u42 = mma_f32(F4 {
                    kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                }
                , q0__u40, q1__u40, q2__u40, q3__u40, ka__u42.a, ka__u42.c);
                kqc[ (8 as i32)] = lo__u42.x;
                kqc[ (9 as i32)] = lo__u42.y;
                kqc[ (10 as i32)] = lo__u42.z;
                kqc[ (11 as i32)] = lo__u42.w;
                let hi__u42 = mma_f32(F4 {
                    kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                }
                , q0__u40, q1__u40, q2__u40, q3__u40, ka__u42.b, ka__u42.d);
                kqc[ (12 as i32)] = hi__u42.x;
                kqc[ (13 as i32)] = hi__u42.y;
                kqc[ (14 as i32)] = hi__u42.z;
                kqc[ (15 as i32)] = hi__u42.w;
            }
            t = (2 as i32);
        }

        if true {
            let q: i32 = 14;
            let q0__u43 = qr.q56;
            let q1__u43 = qr.q57;
            let q2__u43 = qr.q58;
            let q3__u43 = qr.q59;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u44 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u44 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u43, q1__u43, q2__u43, q3__u43, ka__u44.a, ka__u44.c);
                kqc[ (0 as i32)] = lo__u44.x;
                kqc[ (1 as i32)] = lo__u44.y;
                kqc[ (2 as i32)] = lo__u44.z;
                kqc[ (3 as i32)] = lo__u44.w;
                let hi__u44 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u43, q1__u43, q2__u43, q3__u43, ka__u44.b, ka__u44.d);
                kqc[ (4 as i32)] = hi__u44.x;
                kqc[ (5 as i32)] = hi__u44.y;
                kqc[ (6 as i32)] = hi__u44.z;
                kqc[ (7 as i32)] = hi__u44.w;
            }

            if true {
                let t: i32 = 1;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u45 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u45 = mma_f32(F4 {
                    kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                }
                , q0__u43, q1__u43, q2__u43, q3__u43, ka__u45.a, ka__u45.c);
                kqc[ (8 as i32)] = lo__u45.x;
                kqc[ (9 as i32)] = lo__u45.y;
                kqc[ (10 as i32)] = lo__u45.z;
                kqc[ (11 as i32)] = lo__u45.w;
                let hi__u45 = mma_f32(F4 {
                    kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                }
                , q0__u43, q1__u43, q2__u43, q3__u43, ka__u45.b, ka__u45.d);
                kqc[ (12 as i32)] = hi__u45.x;
                kqc[ (13 as i32)] = hi__u45.y;
                kqc[ (14 as i32)] = hi__u45.z;
                kqc[ (15 as i32)] = hi__u45.w;
            }
            t = (2 as i32);
        }

        if true {
            let q: i32 = 15;
            let q0__u46 = qr.q60;
            let q1__u46 = qr.q61;
            let q2__u46 = qr.q62;
            let q3__u46 = qr.q63;
            t = 0;

            if true {
                let t: i32 = 0;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u47 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u47 = mma_f32(F4 {
                    kqc[ (0 as i32)], kqc[ (1 as i32)], kqc[ (2 as i32)], kqc[ (3 as i32)]
                }
                , q0__u46, q1__u46, q2__u46, q3__u46, ka__u47.a, ka__u47.c);
                kqc[ (0 as i32)] = lo__u47.x;
                kqc[ (1 as i32)] = lo__u47.y;
                kqc[ (2 as i32)] = lo__u47.z;
                kqc[ (3 as i32)] = lo__u47.w;
                let hi__u47 = mma_f32(F4 {
                    kqc[ (4 as i32)], kqc[ (5 as i32)], kqc[ (6 as i32)], kqc[ (7 as i32)]
                }
                , q0__u46, q1__u46, q2__u46, q3__u46, ka__u47.b, ka__u47.d);
                kqc[ (4 as i32)] = hi__u47.x;
                kqc[ (5 as i32)] = hi__u47.y;
                kqc[ (6 as i32)] = hi__u47.z;
                kqc[ (7 as i32)] = hi__u47.w;
            }

            if true {
                let t: i32 = 1;
                // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
                let ka__u48 = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
                // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
                // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
                let lo__u48 = mma_f32(F4 {
                    kqc[ (8 as i32)], kqc[ (9 as i32)], kqc[ (10 as i32)], kqc[ (11 as i32)]
                }
                , q0__u46, q1__u46, q2__u46, q3__u46, ka__u48.a, ka__u48.c);
                kqc[ (8 as i32)] = lo__u48.x;
                kqc[ (9 as i32)] = lo__u48.y;
                kqc[ (10 as i32)] = lo__u48.z;
                kqc[ (11 as i32)] = lo__u48.w;
                let hi__u48 = mma_f32(F4 {
                    kqc[ (12 as i32)], kqc[ (13 as i32)], kqc[ (14 as i32)], kqc[ (15 as i32)]
                }
                , q0__u46, q1__u46, q2__u46, q3__u46, ka__u48.b, ka__u48.d);
                kqc[ (12 as i32)] = hi__u48.x;
                kqc[ (13 as i32)] = hi__u48.y;
                kqc[ (14 as i32)] = hi__u48.z;
                kqc[ (15 as i32)] = hi__u48.w;
            }
            t = (2 as i32);
        }
        q = (16 as i32);
        // ---- mask add + running max: half2 mask word at row (cb+get_i)/8, half2 col (rb+get_j)/2 ----
        var kqold: f32[2] = default < f32[2] > ();
        // KQ_max[cols_per_thread=2]
        var kqmn: f32[2] = default < f32[2] > ();
        // KQ_max_new
        var rsa: f32[2] = default < f32[2] > ();
        // KQ_rowsum_add
        kqold[ (0 as i32)] = frombits(bits(acc.k0));
        kqold[ (1 as i32)] = frombits(bits(acc.k1));
        kqmn[ (0 as i32)] = kqold[ (0 as i32)];
        kqmn[ (1 as i32)] = kqold[ (1 as i32)];
        t = 0;

        if true {
            let t: i32 = 0;
            let rb__u49 = t * np * 16 + rh;
            var l0__u49: i32 = 0;

            if true {
                let l0__u49: i32 = 0;
                // half2 pairs, l0 even: (j,i),(j,i+1)
                let mh2__u50 = ( (cb + gi_c(l0__u49, tx)) / 8) * 20 + ( (rb__u49 + gj_c(l0__u49, tx)) / 2);
                let w__u50 = ldu(mt + ( (mh2__u50 as u64) * (4 as u64)));
                kqc[ (0 as i32)] = fmaf(slope, hlo(w__u50), kqc[ (0 as i32)]);
                kqc[ (1 as i32)] = fmaf(slope, hhi(w__u50), kqc[ (1 as i32)]);
            }

            if true {
                let l0__u49: i32 = 2;
                // half2 pairs, l0 even: (j,i),(j,i+1)
                let mh2__u51 = ( (cb + gi_c(l0__u49, tx)) / 8) * 20 + ( (rb__u49 + gj_c(l0__u49, tx)) / 2);
                let w__u51 = ldu(mt + ( (mh2__u51 as u64) * (4 as u64)));
                kqc[ (2 as i32)] = fmaf(slope, hlo(w__u51), kqc[ (2 as i32)]);
                kqc[ (3 as i32)] = fmaf(slope, hhi(w__u51), kqc[ (3 as i32)]);
            }

            if true {
                let l0__u49: i32 = 4;
                // half2 pairs, l0 even: (j,i),(j,i+1)
                let mh2__u52 = ( (cb + gi_c(l0__u49, tx)) / 8) * 20 + ( (rb__u49 + gj_c(l0__u49, tx)) / 2);
                let w__u52 = ldu(mt + ( (mh2__u52 as u64) * (4 as u64)));
                kqc[ (4 as i32)] = fmaf(slope, hlo(w__u52), kqc[ (4 as i32)]);
                kqc[ (5 as i32)] = fmaf(slope, hhi(w__u52), kqc[ (5 as i32)]);
            }

            if true {
                let l0__u49: i32 = 6;
                // half2 pairs, l0 even: (j,i),(j,i+1)
                let mh2__u53 = ( (cb + gi_c(l0__u49, tx)) / 8) * 20 + ( (rb__u49 + gj_c(l0__u49, tx)) / 2);
                let w__u53 = ldu(mt + ( (mh2__u53 as u64) * (4 as u64)));
                kqc[ (6 as i32)] = fmaf(slope, hlo(w__u53), kqc[ (6 as i32)]);
                kqc[ (7 as i32)] = fmaf(slope, hhi(w__u53), kqc[ (7 as i32)]);
            }
            l0__u49 = (8 as i32);
            var l__u49: i32 = 0;

            if true {
                let l__u49: i32 = 0;
                let idx__u54 = (l__u49 / 2) % 2;
                let mv__u54 = kqc[ (0 as i32)] + kqoff;

                if mv__u54 > kqmn[ (0 as i32)] {
                    kqmn[ (0 as i32)] = mv__u54;
                }
            }

            if true {
                let l__u49: i32 = 1;
                let idx__u55 = (l__u49 / 2) % 2;
                let mv__u55 = kqc[ (1 as i32)] + kqoff;

                if mv__u55 > kqmn[ (0 as i32)] {
                    kqmn[ (0 as i32)] = mv__u55;
                }
            }

            if true {
                let l__u49: i32 = 2;
                let idx__u56 = (l__u49 / 2) % 2;
                let mv__u56 = kqc[ (2 as i32)] + kqoff;

                if mv__u56 > kqmn[ (1 as i32)] {
                    kqmn[ (1 as i32)] = mv__u56;
                }
            }

            if true {
                let l__u49: i32 = 3;
                let idx__u57 = (l__u49 / 2) % 2;
                let mv__u57 = kqc[ (3 as i32)] + kqoff;

                if mv__u57 > kqmn[ (1 as i32)] {
                    kqmn[ (1 as i32)] = mv__u57;
                }
            }

            if true {
                let l__u49: i32 = 4;
                let idx__u58 = (l__u49 / 2) % 2;
                let mv__u58 = kqc[ (4 as i32)] + kqoff;

                if mv__u58 > kqmn[ (0 as i32)] {
                    kqmn[ (0 as i32)] = mv__u58;
                }
            }

            if true {
                let l__u49: i32 = 5;
                let idx__u59 = (l__u49 / 2) % 2;
                let mv__u59 = kqc[ (5 as i32)] + kqoff;

                if mv__u59 > kqmn[ (0 as i32)] {
                    kqmn[ (0 as i32)] = mv__u59;
                }
            }

            if true {
                let l__u49: i32 = 6;
                let idx__u60 = (l__u49 / 2) % 2;
                let mv__u60 = kqc[ (6 as i32)] + kqoff;

                if mv__u60 > kqmn[ (1 as i32)] {
                    kqmn[ (1 as i32)] = mv__u60;
                }
            }

            if true {
                let l__u49: i32 = 7;
                let idx__u61 = (l__u49 / 2) % 2;
                let mv__u61 = kqc[ (7 as i32)] + kqoff;

                if mv__u61 > kqmn[ (1 as i32)] {
                    kqmn[ (1 as i32)] = mv__u61;
                }
            }
            l__u49 = (8 as i32);
        }

        if true {
            let t: i32 = 1;
            let rb__u62 = t * np * 16 + rh;
            var l0__u62: i32 = 0;

            if true {
                let l0__u62: i32 = 0;
                // half2 pairs, l0 even: (j,i),(j,i+1)
                let mh2__u63 = ( (cb + gi_c(l0__u62, tx)) / 8) * 20 + ( (rb__u62 + gj_c(l0__u62, tx)) / 2);
                let w__u63 = ldu(mt + ( (mh2__u63 as u64) * (4 as u64)));
                kqc[ (8 as i32)] = fmaf(slope, hlo(w__u63), kqc[ (8 as i32)]);
                kqc[ (9 as i32)] = fmaf(slope, hhi(w__u63), kqc[ (9 as i32)]);
            }

            if true {
                let l0__u62: i32 = 2;
                // half2 pairs, l0 even: (j,i),(j,i+1)
                let mh2__u64 = ( (cb + gi_c(l0__u62, tx)) / 8) * 20 + ( (rb__u62 + gj_c(l0__u62, tx)) / 2);
                let w__u64 = ldu(mt + ( (mh2__u64 as u64) * (4 as u64)));
                kqc[ (10 as i32)] = fmaf(slope, hlo(w__u64), kqc[ (10 as i32)]);
                kqc[ (11 as i32)] = fmaf(slope, hhi(w__u64), kqc[ (11 as i32)]);
            }

            if true {
                let l0__u62: i32 = 4;
                // half2 pairs, l0 even: (j,i),(j,i+1)
                let mh2__u65 = ( (cb + gi_c(l0__u62, tx)) / 8) * 20 + ( (rb__u62 + gj_c(l0__u62, tx)) / 2);
                let w__u65 = ldu(mt + ( (mh2__u65 as u64) * (4 as u64)));
                kqc[ (12 as i32)] = fmaf(slope, hlo(w__u65), kqc[ (12 as i32)]);
                kqc[ (13 as i32)] = fmaf(slope, hhi(w__u65), kqc[ (13 as i32)]);
            }

            if true {
                let l0__u62: i32 = 6;
                // half2 pairs, l0 even: (j,i),(j,i+1)
                let mh2__u66 = ( (cb + gi_c(l0__u62, tx)) / 8) * 20 + ( (rb__u62 + gj_c(l0__u62, tx)) / 2);
                let w__u66 = ldu(mt + ( (mh2__u66 as u64) * (4 as u64)));
                kqc[ (14 as i32)] = fmaf(slope, hlo(w__u66), kqc[ (14 as i32)]);
                kqc[ (15 as i32)] = fmaf(slope, hhi(w__u66), kqc[ (15 as i32)]);
            }
            l0__u62 = (8 as i32);
            var l__u62: i32 = 0;

            if true {
                let l__u62: i32 = 0;
                let idx__u67 = (l__u62 / 2) % 2;
                let mv__u67 = kqc[ (8 as i32)] + kqoff;

                if mv__u67 > kqmn[ (0 as i32)] {
                    kqmn[ (0 as i32)] = mv__u67;
                }
            }

            if true {
                let l__u62: i32 = 1;
                let idx__u68 = (l__u62 / 2) % 2;
                let mv__u68 = kqc[ (9 as i32)] + kqoff;

                if mv__u68 > kqmn[ (0 as i32)] {
                    kqmn[ (0 as i32)] = mv__u68;
                }
            }

            if true {
                let l__u62: i32 = 2;
                let idx__u69 = (l__u62 / 2) % 2;
                let mv__u69 = kqc[ (10 as i32)] + kqoff;

                if mv__u69 > kqmn[ (1 as i32)] {
                    kqmn[ (1 as i32)] = mv__u69;
                }
            }

            if true {
                let l__u62: i32 = 3;
                let idx__u70 = (l__u62 / 2) % 2;
                let mv__u70 = kqc[ (11 as i32)] + kqoff;

                if mv__u70 > kqmn[ (1 as i32)] {
                    kqmn[ (1 as i32)] = mv__u70;
                }
            }

            if true {
                let l__u62: i32 = 4;
                let idx__u71 = (l__u62 / 2) % 2;
                let mv__u71 = kqc[ (12 as i32)] + kqoff;

                if mv__u71 > kqmn[ (0 as i32)] {
                    kqmn[ (0 as i32)] = mv__u71;
                }
            }

            if true {
                let l__u62: i32 = 5;
                let idx__u72 = (l__u62 / 2) % 2;
                let mv__u72 = kqc[ (13 as i32)] + kqoff;

                if mv__u72 > kqmn[ (0 as i32)] {
                    kqmn[ (0 as i32)] = mv__u72;
                }
            }

            if true {
                let l__u62: i32 = 6;
                let idx__u73 = (l__u62 / 2) % 2;
                let mv__u73 = kqc[ (14 as i32)] + kqoff;

                if mv__u73 > kqmn[ (1 as i32)] {
                    kqmn[ (1 as i32)] = mv__u73;
                }
            }

            if true {
                let l__u62: i32 = 7;
                let idx__u74 = (l__u62 / 2) % 2;
                let mv__u74 = kqc[ (15 as i32)] + kqoff;

                if mv__u74 > kqmn[ (1 as i32)] {
                    kqmn[ (1 as i32)] = mv__u74;
                }
            }
            l__u62 = (8 as i32);
        }
        t = (2 as i32);
        // Values per KQ column spread across 4 threads: __shfl_xor offsets 2 then 1 (TURING wide).
        var col: i32 = 0;

        if true {
            let col: i32 = 0;
            var s__u75 = shflx(kqmn[ (0 as i32)], 2);

            if s__u75 > kqmn[ (0 as i32)] {
                kqmn[ (0 as i32)] = s__u75;
            }
            s__u75 = shflx(kqmn[ (0 as i32)], 1);

            if s__u75 > kqmn[ (0 as i32)] {
                kqmn[ (0 as i32)] = s__u75;
            }
        }

        if true {
            let col: i32 = 1;
            var s__u76 = shflx(kqmn[ (1 as i32)], 2);

            if s__u76 > kqmn[ (1 as i32)] {
                kqmn[ (1 as i32)] = s__u76;
            }
            s__u76 = shflx(kqmn[ (1 as i32)], 1);

            if s__u76 > kqmn[ (1 as i32)] {
                kqmn[ (1 as i32)] = s__u76;
            }
        }
        col = (2 as i32);
        // ---- softmax with current max: expf(KQ - KQ_max_new); divisor stays in KQ_rowsum ----
        t = 0;

        if true {
            let t: i32 = 0;
            var l__u77: i32 = 0;

            if true {
                let l__u77: i32 = 0;
                let idx__u78 = (l__u77 / 2) % 2;
                let e__u78 = expf(kqc[ (0 as i32)] - kqmn[ (0 as i32)]);
                kqc[ (0 as i32)] = e__u78;
                rsa[ (0 as i32)] += e__u78;
            }

            if true {
                let l__u77: i32 = 1;
                let idx__u79 = (l__u77 / 2) % 2;
                let e__u79 = expf(kqc[ (1 as i32)] - kqmn[ (0 as i32)]);
                kqc[ (1 as i32)] = e__u79;
                rsa[ (0 as i32)] += e__u79;
            }

            if true {
                let l__u77: i32 = 2;
                let idx__u80 = (l__u77 / 2) % 2;
                let e__u80 = expf(kqc[ (2 as i32)] - kqmn[ (1 as i32)]);
                kqc[ (2 as i32)] = e__u80;
                rsa[ (1 as i32)] += e__u80;
            }

            if true {
                let l__u77: i32 = 3;
                let idx__u81 = (l__u77 / 2) % 2;
                let e__u81 = expf(kqc[ (3 as i32)] - kqmn[ (1 as i32)]);
                kqc[ (3 as i32)] = e__u81;
                rsa[ (1 as i32)] += e__u81;
            }

            if true {
                let l__u77: i32 = 4;
                let idx__u82 = (l__u77 / 2) % 2;
                let e__u82 = expf(kqc[ (4 as i32)] - kqmn[ (0 as i32)]);
                kqc[ (4 as i32)] = e__u82;
                rsa[ (0 as i32)] += e__u82;
            }

            if true {
                let l__u77: i32 = 5;
                let idx__u83 = (l__u77 / 2) % 2;
                let e__u83 = expf(kqc[ (5 as i32)] - kqmn[ (0 as i32)]);
                kqc[ (5 as i32)] = e__u83;
                rsa[ (0 as i32)] += e__u83;
            }

            if true {
                let l__u77: i32 = 6;
                let idx__u84 = (l__u77 / 2) % 2;
                let e__u84 = expf(kqc[ (6 as i32)] - kqmn[ (1 as i32)]);
                kqc[ (6 as i32)] = e__u84;
                rsa[ (1 as i32)] += e__u84;
            }

            if true {
                let l__u77: i32 = 7;
                let idx__u85 = (l__u77 / 2) % 2;
                let e__u85 = expf(kqc[ (7 as i32)] - kqmn[ (1 as i32)]);
                kqc[ (7 as i32)] = e__u85;
                rsa[ (1 as i32)] += e__u85;
            }
            l__u77 = (8 as i32);
        }

        if true {
            let t: i32 = 1;
            var l__u86: i32 = 0;

            if true {
                let l__u86: i32 = 0;
                let idx__u87 = (l__u86 / 2) % 2;
                let e__u87 = expf(kqc[ (8 as i32)] - kqmn[ (0 as i32)]);
                kqc[ (8 as i32)] = e__u87;
                rsa[ (0 as i32)] += e__u87;
            }

            if true {
                let l__u86: i32 = 1;
                let idx__u88 = (l__u86 / 2) % 2;
                let e__u88 = expf(kqc[ (9 as i32)] - kqmn[ (0 as i32)]);
                kqc[ (9 as i32)] = e__u88;
                rsa[ (0 as i32)] += e__u88;
            }

            if true {
                let l__u86: i32 = 2;
                let idx__u89 = (l__u86 / 2) % 2;
                let e__u89 = expf(kqc[ (10 as i32)] - kqmn[ (1 as i32)]);
                kqc[ (10 as i32)] = e__u89;
                rsa[ (1 as i32)] += e__u89;
            }

            if true {
                let l__u86: i32 = 3;
                let idx__u90 = (l__u86 / 2) % 2;
                let e__u90 = expf(kqc[ (11 as i32)] - kqmn[ (1 as i32)]);
                kqc[ (11 as i32)] = e__u90;
                rsa[ (1 as i32)] += e__u90;
            }

            if true {
                let l__u86: i32 = 4;
                let idx__u91 = (l__u86 / 2) % 2;
                let e__u91 = expf(kqc[ (12 as i32)] - kqmn[ (0 as i32)]);
                kqc[ (12 as i32)] = e__u91;
                rsa[ (0 as i32)] += e__u91;
            }

            if true {
                let l__u86: i32 = 5;
                let idx__u92 = (l__u86 / 2) % 2;
                let e__u92 = expf(kqc[ (13 as i32)] - kqmn[ (0 as i32)]);
                kqc[ (13 as i32)] = e__u92;
                rsa[ (0 as i32)] += e__u92;
            }

            if true {
                let l__u86: i32 = 6;
                let idx__u93 = (l__u86 / 2) % 2;
                let e__u93 = expf(kqc[ (14 as i32)] - kqmn[ (1 as i32)]);
                kqc[ (14 as i32)] = e__u93;
                rsa[ (1 as i32)] += e__u93;
            }

            if true {
                let l__u86: i32 = 7;
                let idx__u94 = (l__u86 / 2) % 2;
                let e__u94 = expf(kqc[ (15 as i32)] - kqmn[ (1 as i32)]);
                kqc[ (15 as i32)] = e__u94;
                rsa[ (1 as i32)] += e__u94;
            }
            l__u86 = (8 as i32);
        }
        t = (2 as i32);
        // ---- rescale: KQ_max_scale=expf(KQ_max-KQ_max_new); FTZ bit-multiply for diff < -20;
        //      exact fused step KQ_rowsum = scale*KQ_rowsum + KQ_rowsum_add ----
        var scl: f32[2] = default < f32[2] > ();
        col = 0;

        if true {
            let col: i32 = 0;
            let diff__u95 = kqold[ (0 as i32)] - kqmn[ (0 as i32)];
            let s0__u95 = expf(diff__u95);
            let keep__u95: u32 = if diff__u95 >= -20.0 {
                1 as u32
            } else {
                0 as u32
            }
            ;
            scl[ (0 as i32)] = frombits(bits(s0__u95) * keep__u95);
            // *((uint32_t*)&scale) *= diff>=SOFTMAX_FTZ_THRESHOLD
            acc.k0 = frombits(bits(kqmn[ (0 as i32)]));
            let rs__u95 = frombits(bits(acc.s0));
            acc.s0 = frombits(bits(fmaf(scl[ (0 as i32)], rs__u95, rsa[ (0 as i32)])));
        }

        if true {
            let col: i32 = 1;
            let diff__u96 = kqold[ (1 as i32)] - kqmn[ (1 as i32)];
            let s0__u96 = expf(diff__u96);
            let keep__u96: u32 = if diff__u96 >= -20.0 {
                1 as u32
            } else {
                0 as u32
            }
            ;
            scl[ (1 as i32)] = frombits(bits(s0__u96) * keep__u96);
            // *((uint32_t*)&scale) *= diff>=SOFTMAX_FTZ_THRESHOLD
            acc.k1 = frombits(bits(kqmn[ (1 as i32)]));
            let rs__u96 = frombits(bits(acc.s1));
            acc.s1 = frombits(bits(fmaf(scl[ (1 as i32)], rs__u96, rsa[ (1 as i32)])));
        }
        col = (2 as i32);
        // ---- VKQ f16 accumulator rescale: VKQ_C[i].x[l0+col] *= make_half2(scale,scale) ----
        var sh2: u32[2] = default < u32[2] > ();
        col = 0;

        if true {
            let col: i32 = 0;
            sh2[ (0 as i32)] = h2(scl[ (0 as i32)], scl[ (0 as i32)]);
        }

        if true {
            let col: i32 = 1;
            sh2[ (1 as i32)] = h2(scl[ (1 as i32)], scl[ (1 as i32)]);
        }
        col = (2 as i32);
        var v: i32 = 0;

        if true {
            let v: i32 = 0;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u100: i32 = 0;

                if true {
                    let l0__u100: i32 = 0;
                    let e__u101 = v * 4 + l0__u100 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v0 = mulh2(acc.v0, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u100: i32 = 1;
                    let e__u102 = v * 4 + l0__u100 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v2 = mulh2(acc.v2, sh2[ (0 as i32)]);
                }
                l0__u100 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u103: i32 = 0;

                if true {
                    let l0__u103: i32 = 0;
                    let e__u104 = v * 4 + l0__u103 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v1 = mulh2(acc.v1, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u103: i32 = 1;
                    let e__u105 = v * 4 + l0__u103 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v3 = mulh2(acc.v3, sh2[ (1 as i32)]);
                }
                l0__u103 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 1;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u107: i32 = 0;

                if true {
                    let l0__u107: i32 = 0;
                    let e__u108 = v * 4 + l0__u107 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v4 = mulh2(acc.v4, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u107: i32 = 1;
                    let e__u109 = v * 4 + l0__u107 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v6 = mulh2(acc.v6, sh2[ (0 as i32)]);
                }
                l0__u107 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u110: i32 = 0;

                if true {
                    let l0__u110: i32 = 0;
                    let e__u111 = v * 4 + l0__u110 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v5 = mulh2(acc.v5, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u110: i32 = 1;
                    let e__u112 = v * 4 + l0__u110 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v7 = mulh2(acc.v7, sh2[ (1 as i32)]);
                }
                l0__u110 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 2;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u114: i32 = 0;

                if true {
                    let l0__u114: i32 = 0;
                    let e__u115 = v * 4 + l0__u114 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v8 = mulh2(acc.v8, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u114: i32 = 1;
                    let e__u116 = v * 4 + l0__u114 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v10 = mulh2(acc.v10, sh2[ (0 as i32)]);
                }
                l0__u114 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u117: i32 = 0;

                if true {
                    let l0__u117: i32 = 0;
                    let e__u118 = v * 4 + l0__u117 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v9 = mulh2(acc.v9, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u117: i32 = 1;
                    let e__u119 = v * 4 + l0__u117 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v11 = mulh2(acc.v11, sh2[ (1 as i32)]);
                }
                l0__u117 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 3;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u121: i32 = 0;

                if true {
                    let l0__u121: i32 = 0;
                    let e__u122 = v * 4 + l0__u121 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v12 = mulh2(acc.v12, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u121: i32 = 1;
                    let e__u123 = v * 4 + l0__u121 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v14 = mulh2(acc.v14, sh2[ (0 as i32)]);
                }
                l0__u121 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u124: i32 = 0;

                if true {
                    let l0__u124: i32 = 0;
                    let e__u125 = v * 4 + l0__u124 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v13 = mulh2(acc.v13, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u124: i32 = 1;
                    let e__u126 = v * 4 + l0__u124 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v15 = mulh2(acc.v15, sh2[ (1 as i32)]);
                }
                l0__u124 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 4;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u128: i32 = 0;

                if true {
                    let l0__u128: i32 = 0;
                    let e__u129 = v * 4 + l0__u128 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v16 = mulh2(acc.v16, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u128: i32 = 1;
                    let e__u130 = v * 4 + l0__u128 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v18 = mulh2(acc.v18, sh2[ (0 as i32)]);
                }
                l0__u128 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u131: i32 = 0;

                if true {
                    let l0__u131: i32 = 0;
                    let e__u132 = v * 4 + l0__u131 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v17 = mulh2(acc.v17, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u131: i32 = 1;
                    let e__u133 = v * 4 + l0__u131 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v19 = mulh2(acc.v19, sh2[ (1 as i32)]);
                }
                l0__u131 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 5;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u135: i32 = 0;

                if true {
                    let l0__u135: i32 = 0;
                    let e__u136 = v * 4 + l0__u135 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v20 = mulh2(acc.v20, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u135: i32 = 1;
                    let e__u137 = v * 4 + l0__u135 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v22 = mulh2(acc.v22, sh2[ (0 as i32)]);
                }
                l0__u135 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u138: i32 = 0;

                if true {
                    let l0__u138: i32 = 0;
                    let e__u139 = v * 4 + l0__u138 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v21 = mulh2(acc.v21, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u138: i32 = 1;
                    let e__u140 = v * 4 + l0__u138 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v23 = mulh2(acc.v23, sh2[ (1 as i32)]);
                }
                l0__u138 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 6;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u142: i32 = 0;

                if true {
                    let l0__u142: i32 = 0;
                    let e__u143 = v * 4 + l0__u142 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v24 = mulh2(acc.v24, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u142: i32 = 1;
                    let e__u144 = v * 4 + l0__u142 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v26 = mulh2(acc.v26, sh2[ (0 as i32)]);
                }
                l0__u142 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u145: i32 = 0;

                if true {
                    let l0__u145: i32 = 0;
                    let e__u146 = v * 4 + l0__u145 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v25 = mulh2(acc.v25, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u145: i32 = 1;
                    let e__u147 = v * 4 + l0__u145 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v27 = mulh2(acc.v27, sh2[ (1 as i32)]);
                }
                l0__u145 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 7;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u149: i32 = 0;

                if true {
                    let l0__u149: i32 = 0;
                    let e__u150 = v * 4 + l0__u149 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v28 = mulh2(acc.v28, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u149: i32 = 1;
                    let e__u151 = v * 4 + l0__u149 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v30 = mulh2(acc.v30, sh2[ (0 as i32)]);
                }
                l0__u149 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u152: i32 = 0;

                if true {
                    let l0__u152: i32 = 0;
                    let e__u153 = v * 4 + l0__u152 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v29 = mulh2(acc.v29, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u152: i32 = 1;
                    let e__u154 = v * 4 + l0__u152 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v31 = mulh2(acc.v31, sh2[ (1 as i32)]);
                }
                l0__u152 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 8;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u156: i32 = 0;

                if true {
                    let l0__u156: i32 = 0;
                    let e__u157 = v * 4 + l0__u156 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v32 = mulh2(acc.v32, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u156: i32 = 1;
                    let e__u158 = v * 4 + l0__u156 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v34 = mulh2(acc.v34, sh2[ (0 as i32)]);
                }
                l0__u156 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u159: i32 = 0;

                if true {
                    let l0__u159: i32 = 0;
                    let e__u160 = v * 4 + l0__u159 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v33 = mulh2(acc.v33, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u159: i32 = 1;
                    let e__u161 = v * 4 + l0__u159 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v35 = mulh2(acc.v35, sh2[ (1 as i32)]);
                }
                l0__u159 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 9;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u163: i32 = 0;

                if true {
                    let l0__u163: i32 = 0;
                    let e__u164 = v * 4 + l0__u163 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v36 = mulh2(acc.v36, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u163: i32 = 1;
                    let e__u165 = v * 4 + l0__u163 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v38 = mulh2(acc.v38, sh2[ (0 as i32)]);
                }
                l0__u163 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u166: i32 = 0;

                if true {
                    let l0__u166: i32 = 0;
                    let e__u167 = v * 4 + l0__u166 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v37 = mulh2(acc.v37, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u166: i32 = 1;
                    let e__u168 = v * 4 + l0__u166 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v39 = mulh2(acc.v39, sh2[ (1 as i32)]);
                }
                l0__u166 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 10;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u170: i32 = 0;

                if true {
                    let l0__u170: i32 = 0;
                    let e__u171 = v * 4 + l0__u170 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v40 = mulh2(acc.v40, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u170: i32 = 1;
                    let e__u172 = v * 4 + l0__u170 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v42 = mulh2(acc.v42, sh2[ (0 as i32)]);
                }
                l0__u170 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u173: i32 = 0;

                if true {
                    let l0__u173: i32 = 0;
                    let e__u174 = v * 4 + l0__u173 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v41 = mulh2(acc.v41, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u173: i32 = 1;
                    let e__u175 = v * 4 + l0__u173 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v43 = mulh2(acc.v43, sh2[ (1 as i32)]);
                }
                l0__u173 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 11;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u177: i32 = 0;

                if true {
                    let l0__u177: i32 = 0;
                    let e__u178 = v * 4 + l0__u177 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v44 = mulh2(acc.v44, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u177: i32 = 1;
                    let e__u179 = v * 4 + l0__u177 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v46 = mulh2(acc.v46, sh2[ (0 as i32)]);
                }
                l0__u177 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u180: i32 = 0;

                if true {
                    let l0__u180: i32 = 0;
                    let e__u181 = v * 4 + l0__u180 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v45 = mulh2(acc.v45, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u180: i32 = 1;
                    let e__u182 = v * 4 + l0__u180 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v47 = mulh2(acc.v47, sh2[ (1 as i32)]);
                }
                l0__u180 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 12;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u184: i32 = 0;

                if true {
                    let l0__u184: i32 = 0;
                    let e__u185 = v * 4 + l0__u184 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v48 = mulh2(acc.v48, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u184: i32 = 1;
                    let e__u186 = v * 4 + l0__u184 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v50 = mulh2(acc.v50, sh2[ (0 as i32)]);
                }
                l0__u184 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u187: i32 = 0;

                if true {
                    let l0__u187: i32 = 0;
                    let e__u188 = v * 4 + l0__u187 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v49 = mulh2(acc.v49, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u187: i32 = 1;
                    let e__u189 = v * 4 + l0__u187 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v51 = mulh2(acc.v51, sh2[ (1 as i32)]);
                }
                l0__u187 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 13;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u191: i32 = 0;

                if true {
                    let l0__u191: i32 = 0;
                    let e__u192 = v * 4 + l0__u191 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v52 = mulh2(acc.v52, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u191: i32 = 1;
                    let e__u193 = v * 4 + l0__u191 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v54 = mulh2(acc.v54, sh2[ (0 as i32)]);
                }
                l0__u191 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u194: i32 = 0;

                if true {
                    let l0__u194: i32 = 0;
                    let e__u195 = v * 4 + l0__u194 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v53 = mulh2(acc.v53, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u194: i32 = 1;
                    let e__u196 = v * 4 + l0__u194 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v55 = mulh2(acc.v55, sh2[ (1 as i32)]);
                }
                l0__u194 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 14;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u198: i32 = 0;

                if true {
                    let l0__u198: i32 = 0;
                    let e__u199 = v * 4 + l0__u198 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v56 = mulh2(acc.v56, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u198: i32 = 1;
                    let e__u200 = v * 4 + l0__u198 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v58 = mulh2(acc.v58, sh2[ (0 as i32)]);
                }
                l0__u198 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u201: i32 = 0;

                if true {
                    let l0__u201: i32 = 0;
                    let e__u202 = v * 4 + l0__u201 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v57 = mulh2(acc.v57, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u201: i32 = 1;
                    let e__u203 = v * 4 + l0__u201 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v59 = mulh2(acc.v59, sh2[ (1 as i32)]);
                }
                l0__u201 = (2 as i32);
            }
            col = (2 as i32);
        }

        if true {
            let v: i32 = 15;
            col = 0;

            if true {
                let col: i32 = 0;
                var l0__u205: i32 = 0;

                if true {
                    let l0__u205: i32 = 0;
                    let e__u206 = v * 4 + l0__u205 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v60 = mulh2(acc.v60, sh2[ (0 as i32)]);
                }

                if true {
                    let l0__u205: i32 = 1;
                    let e__u207 = v * 4 + l0__u205 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v62 = mulh2(acc.v62, sh2[ (0 as i32)]);
                }
                l0__u205 = (2 as i32);
            }

            if true {
                let col: i32 = 1;
                var l0__u208: i32 = 0;

                if true {
                    let l0__u208: i32 = 0;
                    let e__u209 = v * 4 + l0__u208 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v61 = mulh2(acc.v61, sh2[ (1 as i32)]);
                }

                if true {
                    let l0__u208: i32 = 1;
                    let e__u210 = v * 4 + l0__u208 * 2 + col;
                    // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                    acc.v63 = mulh2(acc.v63, sh2[ (1 as i32)]);
                }
                l0__u208 = (2 as i32);
            }
            col = (2 as i32);
        }
        v = (16 as i32);
        // ---- B[k] = get_half2(KQ_C[k]): pure F32->half2 pack on Turing, no transpose/shuffle ----
        var b: u32[8] = default < u32[8] > ();
        t = 0;

        if true {
            let t: i32 = 0;
            var l0__u211: i32 = 0;

            if true {
                let l0__u211: i32 = 0;
                b[ (0 as i32)] = h2(kqc[ (0 as i32)], kqc[ (1 as i32)]);
            }

            if true {
                let l0__u211: i32 = 2;
                b[ (1 as i32)] = h2(kqc[ (2 as i32)], kqc[ (3 as i32)]);
            }

            if true {
                let l0__u211: i32 = 4;
                b[ (2 as i32)] = h2(kqc[ (4 as i32)], kqc[ (5 as i32)]);
            }

            if true {
                let l0__u211: i32 = 6;
                b[ (3 as i32)] = h2(kqc[ (6 as i32)], kqc[ (7 as i32)]);
            }
            l0__u211 = (8 as i32);
        }

        if true {
            let t: i32 = 1;
            var l0__u216: i32 = 0;

            if true {
                let l0__u216: i32 = 0;
                b[ (4 as i32)] = h2(kqc[ (8 as i32)], kqc[ (9 as i32)]);
            }

            if true {
                let l0__u216: i32 = 2;
                b[ (5 as i32)] = h2(kqc[ (10 as i32)], kqc[ (11 as i32)]);
            }

            if true {
                let l0__u216: i32 = 4;
                b[ (6 as i32)] = h2(kqc[ (12 as i32)], kqc[ (13 as i32)]);
            }

            if true {
                let l0__u216: i32 = 6;
                b[ (7 as i32)] = h2(kqc[ (14 as i32)], kqc[ (15 as i32)]);
            }
            l0__u216 = (8 as i32);
        }
        t = (2 as i32);
        // ---- double-stage tail: wait the V tile, sync, then preload next K + mask before the current
        //      VKQ mma (skipped only on the last iteration; waits/barriers stay unconditional) ----
        cpwait();
        barrier();

        if !last {
            load_mask(mask_h, mt, stride_mask, kv_row0 + 32, query0, n_queries, ncols1, tx, ty);
            load_kv(k_h2, kt, stride_k, kv_row0 + 32, tx, ty);
        }
        // ---- VKQ += probs @ V^T, swapped operands (A=B_probs, B=A_V trans-V fragment); f16
        //      accumulation with B registers 0,2 -> D.x[0..1] then 1,3 -> D.x[2..3] ----
        v = 0;

        if true {
            let v: i32 = 0;
            var d0__u221: U2 = U2 {
                acc.v0, acc.v1
            }
            ;
            var d1__u221: U2 = U2 {
                acc.v2, acc.v3
            }
            ;
            var kk__u221: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u221: i32 = 0;
                let k0__u222 = kk__u221 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u222 = ldm4t_tile(ldm_addr(vt, 2 * k0__u222 * 128 + v * 8, tx));
                d0__u221 = mma_f16(
                    d0__u221,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u222.a,
                    av__u222.c);
                d1__u221 = mma_f16(
                    d1__u221,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u222.b,
                    av__u222.d);
            }

            if true {
                let kk__u221: i32 = 1;
                let k0__u223 = kk__u221 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u223 = ldm4t_tile(ldm_addr(vt, 2 * k0__u223 * 128 + v * 8, tx));
                d0__u221 = mma_f16(
                    d0__u221,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u223.a,
                    av__u223.c);
                d1__u221 = mma_f16(
                    d1__u221,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u223.b,
                    av__u223.d);
            }
            kk__u221 = (2 as i32);
            acc.v0 = d0__u221.a;
            acc.v1 = d0__u221.b;
            acc.v2 = d1__u221.a;
            acc.v3 = d1__u221.b;
        }

        if true {
            let v: i32 = 1;
            var d0__u224: U2 = U2 {
                acc.v4, acc.v5
            }
            ;
            var d1__u224: U2 = U2 {
                acc.v6, acc.v7
            }
            ;
            var kk__u224: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u224: i32 = 0;
                let k0__u225 = kk__u224 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u225 = ldm4t_tile(ldm_addr(vt, 2 * k0__u225 * 128 + v * 8, tx));
                d0__u224 = mma_f16(
                    d0__u224,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u225.a,
                    av__u225.c);
                d1__u224 = mma_f16(
                    d1__u224,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u225.b,
                    av__u225.d);
            }

            if true {
                let kk__u224: i32 = 1;
                let k0__u226 = kk__u224 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u226 = ldm4t_tile(ldm_addr(vt, 2 * k0__u226 * 128 + v * 8, tx));
                d0__u224 = mma_f16(
                    d0__u224,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u226.a,
                    av__u226.c);
                d1__u224 = mma_f16(
                    d1__u224,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u226.b,
                    av__u226.d);
            }
            kk__u224 = (2 as i32);
            acc.v4 = d0__u224.a;
            acc.v5 = d0__u224.b;
            acc.v6 = d1__u224.a;
            acc.v7 = d1__u224.b;
        }

        if true {
            let v: i32 = 2;
            var d0__u227: U2 = U2 {
                acc.v8, acc.v9
            }
            ;
            var d1__u227: U2 = U2 {
                acc.v10, acc.v11
            }
            ;
            var kk__u227: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u227: i32 = 0;
                let k0__u228 = kk__u227 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u228 = ldm4t_tile(ldm_addr(vt, 2 * k0__u228 * 128 + v * 8, tx));
                d0__u227 = mma_f16(
                    d0__u227,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u228.a,
                    av__u228.c);
                d1__u227 = mma_f16(
                    d1__u227,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u228.b,
                    av__u228.d);
            }

            if true {
                let kk__u227: i32 = 1;
                let k0__u229 = kk__u227 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u229 = ldm4t_tile(ldm_addr(vt, 2 * k0__u229 * 128 + v * 8, tx));
                d0__u227 = mma_f16(
                    d0__u227,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u229.a,
                    av__u229.c);
                d1__u227 = mma_f16(
                    d1__u227,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u229.b,
                    av__u229.d);
            }
            kk__u227 = (2 as i32);
            acc.v8 = d0__u227.a;
            acc.v9 = d0__u227.b;
            acc.v10 = d1__u227.a;
            acc.v11 = d1__u227.b;
        }

        if true {
            let v: i32 = 3;
            var d0__u230: U2 = U2 {
                acc.v12, acc.v13
            }
            ;
            var d1__u230: U2 = U2 {
                acc.v14, acc.v15
            }
            ;
            var kk__u230: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u230: i32 = 0;
                let k0__u231 = kk__u230 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u231 = ldm4t_tile(ldm_addr(vt, 2 * k0__u231 * 128 + v * 8, tx));
                d0__u230 = mma_f16(
                    d0__u230,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u231.a,
                    av__u231.c);
                d1__u230 = mma_f16(
                    d1__u230,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u231.b,
                    av__u231.d);
            }

            if true {
                let kk__u230: i32 = 1;
                let k0__u232 = kk__u230 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u232 = ldm4t_tile(ldm_addr(vt, 2 * k0__u232 * 128 + v * 8, tx));
                d0__u230 = mma_f16(
                    d0__u230,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u232.a,
                    av__u232.c);
                d1__u230 = mma_f16(
                    d1__u230,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u232.b,
                    av__u232.d);
            }
            kk__u230 = (2 as i32);
            acc.v12 = d0__u230.a;
            acc.v13 = d0__u230.b;
            acc.v14 = d1__u230.a;
            acc.v15 = d1__u230.b;
        }

        if true {
            let v: i32 = 4;
            var d0__u233: U2 = U2 {
                acc.v16, acc.v17
            }
            ;
            var d1__u233: U2 = U2 {
                acc.v18, acc.v19
            }
            ;
            var kk__u233: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u233: i32 = 0;
                let k0__u234 = kk__u233 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u234 = ldm4t_tile(ldm_addr(vt, 2 * k0__u234 * 128 + v * 8, tx));
                d0__u233 = mma_f16(
                    d0__u233,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u234.a,
                    av__u234.c);
                d1__u233 = mma_f16(
                    d1__u233,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u234.b,
                    av__u234.d);
            }

            if true {
                let kk__u233: i32 = 1;
                let k0__u235 = kk__u233 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u235 = ldm4t_tile(ldm_addr(vt, 2 * k0__u235 * 128 + v * 8, tx));
                d0__u233 = mma_f16(
                    d0__u233,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u235.a,
                    av__u235.c);
                d1__u233 = mma_f16(
                    d1__u233,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u235.b,
                    av__u235.d);
            }
            kk__u233 = (2 as i32);
            acc.v16 = d0__u233.a;
            acc.v17 = d0__u233.b;
            acc.v18 = d1__u233.a;
            acc.v19 = d1__u233.b;
        }

        if true {
            let v: i32 = 5;
            var d0__u236: U2 = U2 {
                acc.v20, acc.v21
            }
            ;
            var d1__u236: U2 = U2 {
                acc.v22, acc.v23
            }
            ;
            var kk__u236: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u236: i32 = 0;
                let k0__u237 = kk__u236 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u237 = ldm4t_tile(ldm_addr(vt, 2 * k0__u237 * 128 + v * 8, tx));
                d0__u236 = mma_f16(
                    d0__u236,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u237.a,
                    av__u237.c);
                d1__u236 = mma_f16(
                    d1__u236,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u237.b,
                    av__u237.d);
            }

            if true {
                let kk__u236: i32 = 1;
                let k0__u238 = kk__u236 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u238 = ldm4t_tile(ldm_addr(vt, 2 * k0__u238 * 128 + v * 8, tx));
                d0__u236 = mma_f16(
                    d0__u236,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u238.a,
                    av__u238.c);
                d1__u236 = mma_f16(
                    d1__u236,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u238.b,
                    av__u238.d);
            }
            kk__u236 = (2 as i32);
            acc.v20 = d0__u236.a;
            acc.v21 = d0__u236.b;
            acc.v22 = d1__u236.a;
            acc.v23 = d1__u236.b;
        }

        if true {
            let v: i32 = 6;
            var d0__u239: U2 = U2 {
                acc.v24, acc.v25
            }
            ;
            var d1__u239: U2 = U2 {
                acc.v26, acc.v27
            }
            ;
            var kk__u239: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u239: i32 = 0;
                let k0__u240 = kk__u239 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u240 = ldm4t_tile(ldm_addr(vt, 2 * k0__u240 * 128 + v * 8, tx));
                d0__u239 = mma_f16(
                    d0__u239,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u240.a,
                    av__u240.c);
                d1__u239 = mma_f16(
                    d1__u239,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u240.b,
                    av__u240.d);
            }

            if true {
                let kk__u239: i32 = 1;
                let k0__u241 = kk__u239 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u241 = ldm4t_tile(ldm_addr(vt, 2 * k0__u241 * 128 + v * 8, tx));
                d0__u239 = mma_f16(
                    d0__u239,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u241.a,
                    av__u241.c);
                d1__u239 = mma_f16(
                    d1__u239,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u241.b,
                    av__u241.d);
            }
            kk__u239 = (2 as i32);
            acc.v24 = d0__u239.a;
            acc.v25 = d0__u239.b;
            acc.v26 = d1__u239.a;
            acc.v27 = d1__u239.b;
        }

        if true {
            let v: i32 = 7;
            var d0__u242: U2 = U2 {
                acc.v28, acc.v29
            }
            ;
            var d1__u242: U2 = U2 {
                acc.v30, acc.v31
            }
            ;
            var kk__u242: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u242: i32 = 0;
                let k0__u243 = kk__u242 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u243 = ldm4t_tile(ldm_addr(vt, 2 * k0__u243 * 128 + v * 8, tx));
                d0__u242 = mma_f16(
                    d0__u242,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u243.a,
                    av__u243.c);
                d1__u242 = mma_f16(
                    d1__u242,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u243.b,
                    av__u243.d);
            }

            if true {
                let kk__u242: i32 = 1;
                let k0__u244 = kk__u242 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u244 = ldm4t_tile(ldm_addr(vt, 2 * k0__u244 * 128 + v * 8, tx));
                d0__u242 = mma_f16(
                    d0__u242,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u244.a,
                    av__u244.c);
                d1__u242 = mma_f16(
                    d1__u242,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u244.b,
                    av__u244.d);
            }
            kk__u242 = (2 as i32);
            acc.v28 = d0__u242.a;
            acc.v29 = d0__u242.b;
            acc.v30 = d1__u242.a;
            acc.v31 = d1__u242.b;
        }

        if true {
            let v: i32 = 8;
            var d0__u245: U2 = U2 {
                acc.v32, acc.v33
            }
            ;
            var d1__u245: U2 = U2 {
                acc.v34, acc.v35
            }
            ;
            var kk__u245: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u245: i32 = 0;
                let k0__u246 = kk__u245 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u246 = ldm4t_tile(ldm_addr(vt, 2 * k0__u246 * 128 + v * 8, tx));
                d0__u245 = mma_f16(
                    d0__u245,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u246.a,
                    av__u246.c);
                d1__u245 = mma_f16(
                    d1__u245,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u246.b,
                    av__u246.d);
            }

            if true {
                let kk__u245: i32 = 1;
                let k0__u247 = kk__u245 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u247 = ldm4t_tile(ldm_addr(vt, 2 * k0__u247 * 128 + v * 8, tx));
                d0__u245 = mma_f16(
                    d0__u245,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u247.a,
                    av__u247.c);
                d1__u245 = mma_f16(
                    d1__u245,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u247.b,
                    av__u247.d);
            }
            kk__u245 = (2 as i32);
            acc.v32 = d0__u245.a;
            acc.v33 = d0__u245.b;
            acc.v34 = d1__u245.a;
            acc.v35 = d1__u245.b;
        }

        if true {
            let v: i32 = 9;
            var d0__u248: U2 = U2 {
                acc.v36, acc.v37
            }
            ;
            var d1__u248: U2 = U2 {
                acc.v38, acc.v39
            }
            ;
            var kk__u248: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u248: i32 = 0;
                let k0__u249 = kk__u248 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u249 = ldm4t_tile(ldm_addr(vt, 2 * k0__u249 * 128 + v * 8, tx));
                d0__u248 = mma_f16(
                    d0__u248,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u249.a,
                    av__u249.c);
                d1__u248 = mma_f16(
                    d1__u248,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u249.b,
                    av__u249.d);
            }

            if true {
                let kk__u248: i32 = 1;
                let k0__u250 = kk__u248 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u250 = ldm4t_tile(ldm_addr(vt, 2 * k0__u250 * 128 + v * 8, tx));
                d0__u248 = mma_f16(
                    d0__u248,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u250.a,
                    av__u250.c);
                d1__u248 = mma_f16(
                    d1__u248,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u250.b,
                    av__u250.d);
            }
            kk__u248 = (2 as i32);
            acc.v36 = d0__u248.a;
            acc.v37 = d0__u248.b;
            acc.v38 = d1__u248.a;
            acc.v39 = d1__u248.b;
        }

        if true {
            let v: i32 = 10;
            var d0__u251: U2 = U2 {
                acc.v40, acc.v41
            }
            ;
            var d1__u251: U2 = U2 {
                acc.v42, acc.v43
            }
            ;
            var kk__u251: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u251: i32 = 0;
                let k0__u252 = kk__u251 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u252 = ldm4t_tile(ldm_addr(vt, 2 * k0__u252 * 128 + v * 8, tx));
                d0__u251 = mma_f16(
                    d0__u251,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u252.a,
                    av__u252.c);
                d1__u251 = mma_f16(
                    d1__u251,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u252.b,
                    av__u252.d);
            }

            if true {
                let kk__u251: i32 = 1;
                let k0__u253 = kk__u251 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u253 = ldm4t_tile(ldm_addr(vt, 2 * k0__u253 * 128 + v * 8, tx));
                d0__u251 = mma_f16(
                    d0__u251,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u253.a,
                    av__u253.c);
                d1__u251 = mma_f16(
                    d1__u251,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u253.b,
                    av__u253.d);
            }
            kk__u251 = (2 as i32);
            acc.v40 = d0__u251.a;
            acc.v41 = d0__u251.b;
            acc.v42 = d1__u251.a;
            acc.v43 = d1__u251.b;
        }

        if true {
            let v: i32 = 11;
            var d0__u254: U2 = U2 {
                acc.v44, acc.v45
            }
            ;
            var d1__u254: U2 = U2 {
                acc.v46, acc.v47
            }
            ;
            var kk__u254: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u254: i32 = 0;
                let k0__u255 = kk__u254 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u255 = ldm4t_tile(ldm_addr(vt, 2 * k0__u255 * 128 + v * 8, tx));
                d0__u254 = mma_f16(
                    d0__u254,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u255.a,
                    av__u255.c);
                d1__u254 = mma_f16(
                    d1__u254,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u255.b,
                    av__u255.d);
            }

            if true {
                let kk__u254: i32 = 1;
                let k0__u256 = kk__u254 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u256 = ldm4t_tile(ldm_addr(vt, 2 * k0__u256 * 128 + v * 8, tx));
                d0__u254 = mma_f16(
                    d0__u254,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u256.a,
                    av__u256.c);
                d1__u254 = mma_f16(
                    d1__u254,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u256.b,
                    av__u256.d);
            }
            kk__u254 = (2 as i32);
            acc.v44 = d0__u254.a;
            acc.v45 = d0__u254.b;
            acc.v46 = d1__u254.a;
            acc.v47 = d1__u254.b;
        }

        if true {
            let v: i32 = 12;
            var d0__u257: U2 = U2 {
                acc.v48, acc.v49
            }
            ;
            var d1__u257: U2 = U2 {
                acc.v50, acc.v51
            }
            ;
            var kk__u257: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u257: i32 = 0;
                let k0__u258 = kk__u257 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u258 = ldm4t_tile(ldm_addr(vt, 2 * k0__u258 * 128 + v * 8, tx));
                d0__u257 = mma_f16(
                    d0__u257,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u258.a,
                    av__u258.c);
                d1__u257 = mma_f16(
                    d1__u257,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u258.b,
                    av__u258.d);
            }

            if true {
                let kk__u257: i32 = 1;
                let k0__u259 = kk__u257 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u259 = ldm4t_tile(ldm_addr(vt, 2 * k0__u259 * 128 + v * 8, tx));
                d0__u257 = mma_f16(
                    d0__u257,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u259.a,
                    av__u259.c);
                d1__u257 = mma_f16(
                    d1__u257,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u259.b,
                    av__u259.d);
            }
            kk__u257 = (2 as i32);
            acc.v48 = d0__u257.a;
            acc.v49 = d0__u257.b;
            acc.v50 = d1__u257.a;
            acc.v51 = d1__u257.b;
        }

        if true {
            let v: i32 = 13;
            var d0__u260: U2 = U2 {
                acc.v52, acc.v53
            }
            ;
            var d1__u260: U2 = U2 {
                acc.v54, acc.v55
            }
            ;
            var kk__u260: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u260: i32 = 0;
                let k0__u261 = kk__u260 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u261 = ldm4t_tile(ldm_addr(vt, 2 * k0__u261 * 128 + v * 8, tx));
                d0__u260 = mma_f16(
                    d0__u260,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u261.a,
                    av__u261.c);
                d1__u260 = mma_f16(
                    d1__u260,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u261.b,
                    av__u261.d);
            }

            if true {
                let kk__u260: i32 = 1;
                let k0__u262 = kk__u260 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u262 = ldm4t_tile(ldm_addr(vt, 2 * k0__u262 * 128 + v * 8, tx));
                d0__u260 = mma_f16(
                    d0__u260,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u262.a,
                    av__u262.c);
                d1__u260 = mma_f16(
                    d1__u260,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u262.b,
                    av__u262.d);
            }
            kk__u260 = (2 as i32);
            acc.v52 = d0__u260.a;
            acc.v53 = d0__u260.b;
            acc.v54 = d1__u260.a;
            acc.v55 = d1__u260.b;
        }

        if true {
            let v: i32 = 14;
            var d0__u263: U2 = U2 {
                acc.v56, acc.v57
            }
            ;
            var d1__u263: U2 = U2 {
                acc.v58, acc.v59
            }
            ;
            var kk__u263: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u263: i32 = 0;
                let k0__u264 = kk__u263 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u264 = ldm4t_tile(ldm_addr(vt, 2 * k0__u264 * 128 + v * 8, tx));
                d0__u263 = mma_f16(
                    d0__u263,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u264.a,
                    av__u264.c);
                d1__u263 = mma_f16(
                    d1__u263,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u264.b,
                    av__u264.d);
            }

            if true {
                let kk__u263: i32 = 1;
                let k0__u265 = kk__u263 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u265 = ldm4t_tile(ldm_addr(vt, 2 * k0__u265 * 128 + v * 8, tx));
                d0__u263 = mma_f16(
                    d0__u263,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u265.a,
                    av__u265.c);
                d1__u263 = mma_f16(
                    d1__u263,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u265.b,
                    av__u265.d);
            }
            kk__u263 = (2 as i32);
            acc.v56 = d0__u263.a;
            acc.v57 = d0__u263.b;
            acc.v58 = d1__u263.a;
            acc.v59 = d1__u263.b;
        }

        if true {
            let v: i32 = 15;
            var d0__u266: U2 = U2 {
                acc.v60, acc.v61
            }
            ;
            var d1__u266: U2 = U2 {
                acc.v62, acc.v63
            }
            ;
            var kk__u266: i32 = 0;
            // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

            if true {
                let kk__u266: i32 = 0;
                let k0__u267 = kk__u266 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u267 = ldm4t_tile(ldm_addr(vt, 2 * k0__u267 * 128 + v * 8, tx));
                d0__u266 = mma_f16(
                    d0__u266,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u267.a,
                    av__u267.c);
                d1__u266 = mma_f16(
                    d1__u266,
                    b[ (0 as i32)],
                    b[ (1 as i32)],
                    b[ (2 as i32)],
                    b[ (3 as i32)],
                    av__u267.b,
                    av__u267.d);
            }

            if true {
                let kk__u266: i32 = 1;
                let k0__u268 = kk__u266 * np * 8 + rh / 2;
                // (ty%np)*T_A_VKQ::J KV-row-pair base
                // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
                let av__u268 = ldm4t_tile(ldm_addr(vt, 2 * k0__u268 * 128 + v * 8, tx));
                d0__u266 = mma_f16(
                    d0__u266,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u268.a,
                    av__u268.c);
                d1__u266 = mma_f16(
                    d1__u266,
                    b[ (4 as i32)],
                    b[ (5 as i32)],
                    b[ (6 as i32)],
                    b[ (7 as i32)],
                    av__u268.b,
                    av__u268.d);
            }
            kk__u266 = (2 as i32);
            acc.v60 = d0__u266.a;
            acc.v61 = d0__u266.b;
            acc.v62 = d1__u266.a;
            acc.v63 = d1__u266.b;
        }
        v = (16 as i32);
    }
    barrier();
    // L1394-1396: nstages>1 && nwarps*cols_per_warp(64)>nbatch_fa(32)
    // ---- Sum partial KQ rowsums (L1399-1424): TURING+ path, cols_per_warp!=8 -> offsets {2,1}, width 32.
    var col: i32 = 0;

    if true {
        let col: i32 = (0 as i32);
        var v = acc.s0;
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
        acc.s0 = v;
    }

    if true {
        let col: i32 = (1 as i32);
        var v = acc.s1;
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
        acc.s1 = v;
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
        let n0 = fmaxf2(acc.k0, s0);
        let d0 = acc.k0 - n0;
        ks0 = expf(d0);
        acc.k0 = n0;
        // *((u32*)&scale)*= (d>=SOFTMAX_FTZ_THRESHOLD) with SOFTMAX_FTZ_THRESHOLD=-20.0f, as select.

        if d0 < -20.0 {
            ks0 = 0.0;
        }
        acc.s0 = ks0 * acc.s0 + expf(s0 - n0);
        let n1 = fmaxf2(acc.k1, s1);
        let d1 = acc.k1 - n1;
        ks1 = expf(d1);
        acc.k1 = n1;

        if d1 < -20.0 {
            ks1 = 0.0;
        }
        acc.s1 = ks1 * acc.s1 + expf(s1 - n1);
        // VKQ_C rescale (L1458-1469, cols_per_warp!=8): per col scale x[l0+col], l0 in {0,2}.
        let h0 = h2pack(ks0, ks0);
        let h1 = h2pack(ks1, ks1);
        var i0: i32 = 0;

        if true {
            let i0: i32 = (0 as i32);
            acc.v0 = h2mul(acc.v0, h0);
            acc.v2 = h2mul(acc.v2, h0);
            acc.v1 = h2mul(acc.v1, h1);
            acc.v3 = h2mul(acc.v3, h1);
        }

        if true {
            let i0: i32 = (1 as i32);
            acc.v4 = h2mul(acc.v4, h0);
            acc.v6 = h2mul(acc.v6, h0);
            acc.v5 = h2mul(acc.v5, h1);
            acc.v7 = h2mul(acc.v7, h1);
        }

        if true {
            let i0: i32 = (2 as i32);
            acc.v8 = h2mul(acc.v8, h0);
            acc.v10 = h2mul(acc.v10, h0);
            acc.v9 = h2mul(acc.v9, h1);
            acc.v11 = h2mul(acc.v11, h1);
        }

        if true {
            let i0: i32 = (3 as i32);
            acc.v12 = h2mul(acc.v12, h0);
            acc.v14 = h2mul(acc.v14, h0);
            acc.v13 = h2mul(acc.v13, h1);
            acc.v15 = h2mul(acc.v15, h1);
        }

        if true {
            let i0: i32 = (4 as i32);
            acc.v16 = h2mul(acc.v16, h0);
            acc.v18 = h2mul(acc.v18, h0);
            acc.v17 = h2mul(acc.v17, h1);
            acc.v19 = h2mul(acc.v19, h1);
        }

        if true {
            let i0: i32 = (5 as i32);
            acc.v20 = h2mul(acc.v20, h0);
            acc.v22 = h2mul(acc.v22, h0);
            acc.v21 = h2mul(acc.v21, h1);
            acc.v23 = h2mul(acc.v23, h1);
        }

        if true {
            let i0: i32 = (6 as i32);
            acc.v24 = h2mul(acc.v24, h0);
            acc.v26 = h2mul(acc.v26, h0);
            acc.v25 = h2mul(acc.v25, h1);
            acc.v27 = h2mul(acc.v27, h1);
        }

        if true {
            let i0: i32 = (7 as i32);
            acc.v28 = h2mul(acc.v28, h0);
            acc.v30 = h2mul(acc.v30, h0);
            acc.v29 = h2mul(acc.v29, h1);
            acc.v31 = h2mul(acc.v31, h1);
        }

        if true {
            let i0: i32 = (8 as i32);
            acc.v32 = h2mul(acc.v32, h0);
            acc.v34 = h2mul(acc.v34, h0);
            acc.v33 = h2mul(acc.v33, h1);
            acc.v35 = h2mul(acc.v35, h1);
        }

        if true {
            let i0: i32 = (9 as i32);
            acc.v36 = h2mul(acc.v36, h0);
            acc.v38 = h2mul(acc.v38, h0);
            acc.v37 = h2mul(acc.v37, h1);
            acc.v39 = h2mul(acc.v39, h1);
        }

        if true {
            let i0: i32 = (10 as i32);
            acc.v40 = h2mul(acc.v40, h0);
            acc.v42 = h2mul(acc.v42, h0);
            acc.v41 = h2mul(acc.v41, h1);
            acc.v43 = h2mul(acc.v43, h1);
        }

        if true {
            let i0: i32 = (11 as i32);
            acc.v44 = h2mul(acc.v44, h0);
            acc.v46 = h2mul(acc.v46, h0);
            acc.v45 = h2mul(acc.v45, h1);
            acc.v47 = h2mul(acc.v47, h1);
        }

        if true {
            let i0: i32 = (12 as i32);
            acc.v48 = h2mul(acc.v48, h0);
            acc.v50 = h2mul(acc.v50, h0);
            acc.v49 = h2mul(acc.v49, h1);
            acc.v51 = h2mul(acc.v51, h1);
        }

        if true {
            let i0: i32 = (13 as i32);
            acc.v52 = h2mul(acc.v52, h0);
            acc.v54 = h2mul(acc.v54, h0);
            acc.v53 = h2mul(acc.v53, h1);
            acc.v55 = h2mul(acc.v55, h1);
        }

        if true {
            let i0: i32 = (14 as i32);
            acc.v56 = h2mul(acc.v56, h0);
            acc.v58 = h2mul(acc.v58, h0);
            acc.v57 = h2mul(acc.v57, h1);
            acc.v59 = h2mul(acc.v59, h1);
        }

        if true {
            let i0: i32 = (15 as i32);
            acc.v60 = h2mul(acc.v60, h0);
            acc.v62 = h2mul(acc.v62, h0);
            acc.v61 = h2mul(acc.v61, h1);
            acc.v63 = h2mul(acc.v63, h1);
        }
        i0 = (16 as i32);
    }
    // ---- Combine meta (L1503-1576): swz=true -> barrier first (L1555-1557); jc_cwm uses
    // tile<16,8,half2> get_i(tx%4)=((tx%2)*8)+tx/4; meta floats at tile_Q row padding word 128/129.
    barrier();
    let jc_cwm = ty * 16 + gi8(tx % 4, tx);
    let cmrx = (if tx % 2 == 0 {
        acc.k0
    } else {
        acc.k1
    }
    );
    let cmry = (if tx % 2 == 0 {
        acc.s0
    } else {
        acc.s1
    }
    );
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
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v0);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v1);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v2);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v3);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (8 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v4);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v5);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v6);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v7);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (16 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v8);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v9);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v10);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v11);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (24 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v12);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v13);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v14);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v15);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (32 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v16);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v17);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v18);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v19);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (40 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v20);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v21);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v22);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v23);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (48 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v24);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v25);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v26);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v27);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (56 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v28);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v29);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v30);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v31);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (64 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v32);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v33);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v34);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v35);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (72 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v36);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v37);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v38);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v39);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (80 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v40);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v41);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v42);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v43);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (88 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v44);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v45);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v46);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v47);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (96 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v48);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v49);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v50);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v51);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (104 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v52);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v53);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v54);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v55);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (112 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v56);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v57);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v58);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v59);
        }
        l = (4 as i32);
    }

    if true {
        let k1: i32 = (120 as i32);
        var l: i32 = 0;

        if true {
            let l: i32 = (0 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v60);
        }

        if true {
            let l: i32 = (1 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v61);
        }

        if true {
            let l: i32 = (2 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v62);
        }

        if true {
            let l: i32 = (3 as i32);
            smst(smem, ( (js + gi8(l, tx)) * 132 + k1 + gj8(l, tx)) as u64, acc.v63);
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
    if ncols1 == 4 && np == 2 {
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
        if ncols1 == 8 && np == 1 {
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
        }
    }
}

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

