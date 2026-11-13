// Apex CUDA cumsum in Goose, from the fallback kernel of ggml-cuda/cumsum.cu.
//
// This build defines no GGML_CUDA_USE_CUB, so the native path is the fallback
// kernel. Its float addition order is part of the contract; the Goose body
// mirrors it statement by statement:
//   - thread-local 4-element sequential scan (temp[0..3]),
//   - warp prefix inclusive sum (shfl_up ladder),
//   - shared exchange of per-thread prefix and per-warp sums,
//   - warp 0 exclusive scan of the warp sums into s_warp_sums, chunk total,
//   - final offset = (s_vals + s_warp_sums + carry) - temp[3],
//   - carry accumulation across 4*blockDim chunks.
//
// Shared memory is a primitive: the base pointer, load/store and __syncthreads
// are externs, like the shuffles. Every index and every float operation is Goose.
//
// Layout of the dynamic shared buffer (floats): s_vals[blockDim],
// s_warp_sums[warps], s_carry[1], s_chunk_total[1].
extern "fg279d_ld_f32" fn ld_f32(x: u64, o: u64) -> f32;
extern "fg279d_st_f32" fn st_f32(dst: u64, o: u64, v: f32);
extern "fg279d_smem" fn smem_base() -> u64;
extern "fg279d_sload" fn sload(a: u64) -> f32;
extern "fg279d_sstore" fn sstore(a: u64, v: f32);
extern "fg279d_sync" fn sync();
extern "fg279d_shfl_up" fn shfl_up(v: f32, o: i32) -> f32;
extern "fg279d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn warp_scan(x: f32, lane: i64, ws: i64) -> f32 {
    var v: f32 = x;
    var off: i64 = 1 as i64;

    while off < ws {
        let t: f32 = shfl_up(v, off as i32);

        if lane >= off {
            v = v + t;
        }
        off = off * 2 as i64;
    }
    v
}

fn cumsum(
    src: u64,
    dst: u64,
    ne00: i64,
    ne01: i64,
    ne02: i64,
    ne03: i64,
    s01: i64,
    s02: i64,
    s03: i64,
    s1: i64,
    s2: i64,
    s3: i64,
    blk: i64,
    tid: i64,
    bx: i64,
    by: i64,
    bz: i64,
    ws: i64) {
    ensure(blk > 0 as i64 && ws > 0 as i64, 20);
    let lane: i64 = tid % ws;
    let warp: i64 = tid / ws;
    let wpb: i64 = blk / ws;
    let S: u64 = smem_base();
    let SV: u64 = S;
    let SW: u64 = S + ( (blk * 4 as i64) as u64);
    let SC: u64 = S + ( ( (blk + wpb) * 4 as i64) as u64);
    let ST: u64 = S + ( ( (blk + wpb + 1 as i64) * 4 as i64) as u64);

    if tid == 0 as i64 {
        sstore(SC, 0.0 as f32);
    }
    sync();

    if bz >= ne03 as i64 {
        return;
    }

    if by >= ne02 as i64 {
        return;
    }

    if bx >= ne01 as i64 {
        return;
    }
    let srow: i64 = bx * s01 + by * s02 + bz * s03;
    let drow: i64 = bx * s1 + by * s2 + bz * s3;
    var i: i64 = 0 as i64;

    while i < ne00 {
        let idx: i64 = i + tid * 4 as i64;
        var t0: f32 = 0.0 as f32;

        if idx < ne00 as i64 {
            t0 = ld_f32(src, ( ( (srow + idx) * 4 as i64) as u64));
        }
        var t1: f32 = t0;

        if idx + 1 as i64 < ne00 as i64 {
            t1 = t1 + ld_f32(src, ( ( (srow + idx + 1 as i64) * 4 as i64) as u64));
        }
        var t2: f32 = t1;

        if idx + 2 as i64 < ne00 as i64 {
            t2 = t2 + ld_f32(src, ( ( (srow + idx + 2 as i64) * 4 as i64) as u64));
        }
        var t3: f32 = t2;

        if idx + 3 as i64 < ne00 as i64 {
            t3 = t3 + ld_f32(src, ( ( (srow + idx + 3 as i64) * 4 as i64) as u64));
        }
        var val: f32 = 0.0 as f32;

        if idx < ne00 as i64 {
            val = t3;
        }
        val = warp_scan(val, lane, ws);
        sstore(SV + ( (tid * 4 as i64) as u64), val);

        if lane == ws - 1 as i64 {
            sstore(SW + ( (warp * 4 as i64) as u64), val);
        }
        sync();

        if warp == 0 as i64 {
            var w: f32 = 0.0 as f32;

            if tid < wpb as i64 {
                w = sload(SW + ( (tid * 4 as i64) as u64));
            }
            var inc: f32 = w;
            inc = warp_scan(inc, lane, ws);

            if tid < wpb as i64 {
                sstore(SW + ( (tid * 4 as i64) as u64), inc - w);
            }

            if tid == wpb - 1 as i64 {
                sstore(ST, inc);
            }
        }
        sync();
        let carry: f32 = sload(SC);
        let svi: f32 = sload(SV + ( (tid * 4 as i64) as u64));
        let swi: f32 = sload(SW + ( (warp * 4 as i64) as u64));
        let fo: f32 = (svi + swi + carry) - t3;

        if idx < ne00 as i64 {
            st_f32(dst, ( ( (drow + idx) * 4 as i64) as u64), t0 + fo);
        }

        if idx + 1 as i64 < ne00 as i64 {
            st_f32(dst, ( ( (drow + idx + 1 as i64) * 4 as i64) as u64), t1 + fo);
        }

        if idx + 2 as i64 < ne00 as i64 {
            st_f32(dst, ( ( (drow + idx + 2 as i64) * 4 as i64) as u64), t2 + fo);
        }

        if idx + 3 as i64 < ne00 as i64 {
            st_f32(dst, ( ( (drow + idx + 3 as i64) * 4 as i64) as u64), t3 + fo);
        }
        sync();

        if tid == 0 as i64 {
            sstore(SC, sload(SC) + sload(ST));
        }
        i = i + 4 * blk as i64;
    }
}
// Retained entry; the parent never executes main.

fn main() {
    cumsum(
        0,
        0,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64);
}

