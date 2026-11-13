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
// Goose loaders; no native algorithm callback. Shared map (bytes from smem): tile_K [0,8192) 32 rows
// x 128 half2 (row stride 512 B), tile_V [8192,16384), tile_mask [16384,16384+ncols1*80) ncols1 rows
// x 20 half2 (16 B pad). Block dim3(32,4): tx=threadIdx.x lane, ty=threadIdx.y warp.
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
extern "fg176i_ldu" fn ldu(a: u64) -> u32;
extern "fg176i_stu" fn stu(a: u64, v: u32);
extern "fg176i_ldm4" fn ldm4(a: u64) -> U4;
extern "fg176i_ldm4t" fn ldm4t(a: u64) -> U4;
extern "fg176i_mma_f32" fn mma_f32(d: F4, a0: u32, a1: u32, a2: u32, a3: u32, b0: u32, b1: u32) -> F4;
extern "fg176i_mma_f16" fn mma_f16(d: U2, a0: u32, a1: u32, a2: u32, a3: u32, b0: u32, b1: u32) -> U2;
extern "fg176i_hlo" fn hlo(w: u32) -> f32;
extern "fg176i_hhi" fn hhi(w: u32) -> f32;
extern "fg176i_h2" fn h2(a: f32, b: f32) -> u32;
extern "fg176i_mulh2" fn mulh2(w: u32, s: u32) -> u32;
extern "fg176i_expf" fn expf(x: f32) -> f32;
extern "fg176i_fmaf" fn fmaf(a: f32, b: f32, c: f32) -> f32;
extern "fg176i_shflx" fn shflx(x: f32, off: i32) -> f32;
extern "fg176i_cpwait" fn cpwait();
extern "fg176i_barrier" fn barrier();
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

fn iterate(
    k_h2: u64,
    v_h2: u64,
    mask_h: u64,
    stride_k: i64,
    stride_v: i64,
    stride_mask: i64,
    slope: f32,
    jt: i32,
    kb0: i32,
    n_queries: i64,
    ncols1: i32,
    qregs: u64,
    vkqregs: u64,
    kmax: u64,
    rowsums: u64,
    smem: u64,
    tx: i32,
    ty: i32,
    np: i32,
    last: bool) {
    let kt = smem;
    let vt = smem + (8192 as u64);
    let mt = smem + (16384 as u64);
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
    let kqoff = 3.0 * 0.6931;
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

    while q < 16 {
        let q0 = ldu(qregs + ( (q * 16) as u64));
        let q1 = ldu(qregs + ( (q * 16 + 4) as u64));
        let q2 = ldu(qregs + ( (q * 16 + 8) as u64));
        let q3 = ldu(qregs + ( (q * 16 + 12) as u64));
        t = 0;

        while t < nt {
            // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
            let ka = ldm4(ldm_addr(kt, (t * np * 16 + rh) * 128 + q * 8, tx));
            // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
            // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
            let lo = mma_f32(F4 {
                kqc[t * 8], kqc[t * 8 + 1], kqc[t * 8 + 2], kqc[t * 8 + 3]
            }
            , q0, q1, q2, q3, ka.a, ka.c);
            kqc[t * 8] = lo.x;
            kqc[t * 8 + 1] = lo.y;
            kqc[t * 8 + 2] = lo.z;
            kqc[t * 8 + 3] = lo.w;
            let hi = mma_f32(F4 {
                kqc[t * 8 + 4], kqc[t * 8 + 5], kqc[t * 8 + 6], kqc[t * 8 + 7]
            }
            , q0, q1, q2, q3, ka.b, ka.d);
            kqc[t * 8 + 4] = hi.x;
            kqc[t * 8 + 5] = hi.y;
            kqc[t * 8 + 6] = hi.z;
            kqc[t * 8 + 7] = hi.w;
            t += 1;
        }
        q += 1;
    }
    // ---- mask add + running max: half2 mask word at row (cb+get_i)/8, half2 col (rb+get_j)/2 ----
    var kqold: f32[2] = default < f32[2] > ();
    // KQ_max[cols_per_thread=2]
    var kqmn: f32[2] = default < f32[2] > ();
    // KQ_max_new
    var rsa: f32[2] = default < f32[2] > ();
    // KQ_rowsum_add
    kqold[0] = (ldu(kmax) as !f32);
    kqold[1] = (ldu(kmax + (4 as u64)) as !f32);
    kqmn[0] = kqold[0];
    kqmn[1] = kqold[1];
    t = 0;

    while t < nt {
        let rb = t * np * 16 + rh;
        var l0: i32 = 0;

        while l0 < 8 {
            // half2 pairs, l0 even: (j,i),(j,i+1)
            let mh2 = ( (cb + gi_c(l0, tx)) / 8) * 20 + ( (rb + gj_c(l0, tx)) / 2);
            let w = ldu(mt + ( (mh2 as u64) * (4 as u64)));
            kqc[t * 8 + l0] = fmaf(slope, hlo(w), kqc[t * 8 + l0]);
            kqc[t * 8 + l0 + 1] = fmaf(slope, hhi(w), kqc[t * 8 + l0 + 1]);
            l0 += 2;
        }
        var l: i32 = 0;

        while l < 8 {
            let idx = (l / 2) % 2;
            let mv = kqc[t * 8 + l] + kqoff;

            if mv > kqmn[idx] {
                kqmn[idx] = mv;
            }
            l += 1;
        }
        t += 1;
    }
    // Values per KQ column spread across 4 threads: __shfl_xor offsets 2 then 1 (TURING wide).
    var col: i32 = 0;

    while col < 2 {
        var s = shflx(kqmn[col], 2);

        if s > kqmn[col] {
            kqmn[col] = s;
        }
        s = shflx(kqmn[col], 1);

        if s > kqmn[col] {
            kqmn[col] = s;
        }
        col += 1;
    }
    // ---- softmax with current max: expf(KQ - KQ_max_new); divisor stays in KQ_rowsum ----
    t = 0;

    while t < nt {
        var l: i32 = 0;

        while l < 8 {
            let idx = (l / 2) % 2;
            let e = expf(kqc[t * 8 + l] - kqmn[idx]);
            kqc[t * 8 + l] = e;
            rsa[idx] += e;
            l += 1;
        }
        t += 1;
    }
    // ---- rescale: KQ_max_scale=expf(KQ_max-KQ_max_new); FTZ bit-multiply for diff < -20;
    //      exact fused step KQ_rowsum = scale*KQ_rowsum + KQ_rowsum_add ----
    var scl: f32[2] = default < f32[2] > ();
    col = 0;

    while col < 2 {
        let diff = kqold[col] - kqmn[col];
        let s0 = expf(diff);
        let keep: u32 = if diff >= -20.0 {
            1 as u32
        } else {
            0 as u32
        }
        ;
        scl[col] = ( (s0 as !u32) * keep) as !f32;
        // *((uint32_t*)&scale) *= diff>=SOFTMAX_FTZ_THRESHOLD
        stu(kmax + ( (col * 4) as u64), (kqmn[col] as !u32));
        let rs = (ldu(rowsums + ( (col * 4) as u64)) as !f32);
        stu(rowsums + ( (col * 4) as u64), (fmaf(scl[col], rs, rsa[col]) as !u32));
        col += 1;
    }
    // ---- VKQ f16 accumulator rescale: VKQ_C[i].x[l0+col] *= make_half2(scale,scale) ----
    var sh2: u32[2] = default < u32[2] > ();
    col = 0;

    while col < 2 {
        sh2[col] = h2(scl[col], scl[col]);
        col += 1;
    }
    var v: i32 = 0;

    while v < 16 {
        col = 0;

        while col < 2 {
            var l0: i32 = 0;

            while l0 < 2 {
                let e = v * 4 + l0 * 2 + col;
                // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
                stu(vkqregs + ( (e * 4) as u64), mulh2(ldu(vkqregs + ( (e * 4) as u64)), sh2[col]));
                l0 += 1;
            }
            col += 1;
        }
        v += 1;
    }
    // ---- B[k] = get_half2(KQ_C[k]): pure F32->half2 pack on Turing, no transpose/shuffle ----
    var b: u32[8] = default < u32[8] > ();
    t = 0;

    while t < nt {
        var l0: i32 = 0;

        while l0 < 8 {
            b[t * 4 + l0 / 2] = h2(kqc[t * 8 + l0], kqc[t * 8 + l0 + 1]);
            l0 += 2;
        }
        t += 1;
    }
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

    while v < 16 {
        var d0: U2 = U2 {
            ldu(vkqregs + ( (v * 16) as u64)), ldu(vkqregs + ( (v * 16 + 4) as u64))
        }
        ;
        var d1: U2 = U2 {
            ldu(vkqregs + ( (v * 16 + 8) as u64)), ldu(vkqregs + ( (v * 16 + 12) as u64))
        }
        ;
        var kk: i32 = 0;
        // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8

        while kk < 16 / (np * 8) {
            let k0 = kk * np * 8 + rh / 2;
            // (ty%np)*T_A_VKQ::J KV-row-pair base
            // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
            let av = ldm4t_tile(ldm_addr(vt, 2 * k0 * 128 + v * 8, tx));
            d0 = mma_f16(d0, b[kk * 4], b[kk * 4 + 1], b[kk * 4 + 2], b[kk * 4 + 3], av.a, av.c);
            d1 = mma_f16(d1, b[kk * 4], b[kk * 4 + 1], b[kk * 4 + 2], b[kk * 4 + 3], av.b, av.d);
            kk += 1;
        }
        stu(vkqregs + ( (v * 16) as u64), d0.a);
        stu(vkqregs + ( (v * 16 + 4) as u64), d0.b);
        stu(vkqregs + ( (v * 16 + 8) as u64), d1.a);
        stu(vkqregs + ( (v * 16 + 12) as u64), d1.b);
        v += 1;
    }
}
// Retained entry; the parent never executes main.

fn main() {
    iterate(
        0 as u64,
        0 as u64,
        0 as u64,
        0 as i64,
        0 as i64,
        0 as i64,
        0.0,
        0,
        0,
        0 as i64,
        4,
        0 as u64,
        0 as u64,
        0 as u64,
        0 as u64,
        0 as u64,
        0,
        0,
        2,
        true);
}

