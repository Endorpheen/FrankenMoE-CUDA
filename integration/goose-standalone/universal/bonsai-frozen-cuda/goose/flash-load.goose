// Goose port of the Apex fattn-mma asynchronous tile loaders: flash_attn_ext_f16_load_tile
// (fattn-mma-f16.cuh L383-446) and flash_attn_ext_f16_load_mask (L487-515), use_cp_async branches.
// Declared FIXED target only; every other configuration is explicitly DEFERRED and not implemented:
//   CUDA Ada (ampere_mma_available config table), DKQ=DV=256 -> D2=128 half2, K/V pre-converted F16,
//   ncols2=8, ncols1=4 or 8, nwarps=4, nbatch_fa=32, stride_tile_K=stride_tile_V=128 half2 (512 B rows),
//   use_cp_async=true, nstages=2, use_sparse=false, oob_check=false, physical warp_size=32.
// Deferred: other DKQ/DV/D2, other ncols1/ncols2/nwarps/nbatch_fa, padded non-swizzled tiles,
//   synchronous nstages<=1 scalar-copy loads, oob_check and use_sparse index gathers, Volta/AMD paths.
// All coalescing, swizzle and mask row wrap arithmetic executes here in Goose; the raw effect below
// is only the 16-byte cp.async instruction (parent converts the shared address space). The native
// callers own cp_async_wait_all + __syncthreads, so no wait/barrier is emitted in this file.
// Exact cp.async.cg.shared.global.L2::64B [dst],[src],16 for raw byte pointers
// (native cp_async_cg_16<preload=64>: 64 for the K/V tiles and 32*sizeof(half)=64 for the mask tile).
extern "fg175l_cp16" fn cp16(dst_shared_generic: u64, src_global: u64);

export fn fg_flash_load_kv(src: u64, dst_shared: u64, stride_kv_h2: i64, kv_row0: i64, tx: i32, ty: i32) {
    // Native specialization: chunk=16 B, h2_per_chunk=4, chunks_per_row=D2/4=32. ggml_cuda_unroll<6>{}(load)
    // calls n=5..0; for n>=1 stride_k=32>>n<=16 makes k0_start=32-32%(2*stride_k)=32==k0_stop (empty), so only
    // n=0 emits: stride_k=32, k0_start=0, k0_stop=32, stride_i=1. Rows i=i0+ty with i0+=nwarps*stride_i=4 and a
    // single inner chunk k=0+tx: every lane moves one 16 B chunk (4*tx half2), a warp covers one 512 B tile row
    // coalesced, 4 warps cover 4 rows per i0 step. The native break (i0+nwarps*stride_i>nbatch_fa && i>=nbatch_fa)
    // is statically false for nbatch_fa=32 (i0<=28, i<=31); use_sparse=false/oob_check=false -> i_kv=kv_row0+i.
    var i0: i32 = 0;

    while i0 < 32 {
        let i = i0 + ty;
        let i_kv = kv_row0 + (i as i64);
        let k = tx;
        // Native: cp_async_cg_16<64>(tile_KV_32+swizzle<stride_tile*sizeof(half2),char>(i*512+k*16,i),KV+i_kv*stride_KV+k*4).
        // swizzle<512,char> (mma.cuh L786, stride_bytes=512): (row*512+lane*16)^((row%8)<<4).
        let dst_off = (i * 512 + k * 16) ^ ( (i % 8) << 4);
        let src_h2 = i_kv * stride_kv_h2 + (k as i64) * 4;
        let src_off = src_h2 * 4;
        cp16(dst_shared + (dst_off as u64), src + (src_off as u64));
        i0 += 4;
    }
}

