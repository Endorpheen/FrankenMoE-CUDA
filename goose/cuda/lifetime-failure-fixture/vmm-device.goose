// VMM pool allocation arithmetic in Goose, from ggml-cuda.cu:
// ggml_cuda_pool_vmm::alloc and ::free.
//
// This is the GPU memory pool the Apex profile actually uses. Confirmed by probe, not by
// reading: an LD_PRELOAD counter over the driver entry points recorded, in one Apex run,
//
//   cuMemCreate=105 cuMemAddressReserve=2 cuMemMap=105 cuMemRelease=105
//   cuMemUnmap=21  cuMemAddressFree=2
//
// so the pool grows, maps and unmaps during inference and is on the live path.
//
// Everything algorithmic is Goose: the 128-byte alignment rounding, the available-space
// computation, the granularity rounding of the growth request, the 32 GB ceiling check,
// the bump-pointer advance and the reverse-order free check. The driver calls themselves
// (cuMemCreate/cuMemMap/cuMemSetAccess/cuMemRelease) stay with the caller as extern
// effects -- they are the hardware boundary, not arithmetic.
//
// Constants from the native implementation: CUDA_POOL_VMM_MAX_SIZE = 1<<35 (32 GB),
// alignment = 128, and the granularity comes from the device's vmm_granularity.
extern "fg195v_fail" fn fail(code: i64);
extern "fg195v_st_i64" fn st_i64(base: u64, byte: i64, v: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// Result of planning one allocation: how much to reserve from the driver, where the block
// starts inside the pool, and the pool bookkeeping afterwards.
//   r[0] reserve bytes (0 = no growth needed)
//   r[1] pointer offset from pool_addr
//   r[2] new pool_size
//   r[3] new pool_used
//   r[4] actual_size handed back to the caller
//   r[5] mapping offset from pool_addr (old pool_size, where cuMemMap must map)

fn plan_alloc(size: i64, pool_used: i64, pool_size: i64, granularity: i64, r: u64) {
    ensure(size > 0 && pool_used >= 0 && pool_size >= pool_used && granularity > 0, 10);
    let max_size: i64 = (1 as i64) << 35 as i64;
    // round up the allocation size so every block is aligned for all data types
    let aligned: i64 = 128 as i64 * ( (size + 127 as i64) / 128 as i64);
    ensure(aligned >= size, 11);
    let avail: i64 = pool_size - pool_used;
    var reserve: i64 = 0;
    var new_pool_size: i64 = pool_size;

    if aligned > avail {
        reserve = aligned - avail;
        // round the growth up to the device granularity
        reserve = granularity * ( (reserve + granularity - 1 as i64) / granularity);
        ensure(pool_size + reserve <= max_size, 12);
        new_pool_size = pool_size + reserve;
    }
    let ptr_off: i64 = pool_used;
    let new_pool_used: i64 = pool_used + aligned;
    ensure(new_pool_used <= new_pool_size, 13);
    st_i64(r, 0, reserve);
    st_i64(r, 8, ptr_off);
    st_i64(r, 16, new_pool_size);
    st_i64(r, 24, new_pool_used);
    st_i64(r, 32, aligned);
    // The mapping goes at the END of the pool before the growth, which is the caller's
    // current pool_size. Returned so the caller does not re-derive it.
    st_i64(r, 40, pool_size);
}
// Release: the pool is a bump allocator, so a free only rewinds the pointer and the
// caller's pointer must be exactly the top of the pool afterwards. Returns the new
// pool_used and traps if the ordering invariant is violated.

fn plan_free(ptr_off: i64, pool_used: i64, size: i64) -> i64 {
    ensure(size > 0 && size <= pool_used, 20);
    let new_used: i64 = pool_used - size;
    ensure(ptr_off == new_used, 21);
    return new_used;
}
// main must reach both wrappers, otherwise the emitter drops the unused one.

fn main() {
    plan_alloc(1 as i64, 0 as i64, 0 as i64, 1 as i64, 0 as u64);
    plan_free(0 as i64, 1 as i64, 1 as i64);
}

