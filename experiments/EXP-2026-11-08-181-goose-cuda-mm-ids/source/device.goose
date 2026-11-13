// Apex CUDA MM IDs helper (Goose), from ggml-cuda/mmid.cu.
//
// This kernel converts the router's per-token expert ids into the compact
// per-expert layout that mul_mat_id consumes, and records each expert's lower
// bound. The Apex profile instantiates n_expert_used_template = 8 (the census
// shows mm_ids_helper<(int)8>), and that is what this module transfers; the
// generic (template 0) variant and the other widths, together with the HIP
// fallbacks, stay native.
//
// Everything algorithmic is Goose: the warp/token slot decode, the guarded id
// read, the used-expert compare, the power-of-two padded slot count, the
// warp-wide "any" reduction, the inclusive scan over token positions built from
// shfl_up, the running compact counter, the shared-memory store/load round trip
// through the packed (it, iex_used) word, the final index write for both the
// forward and the inverse map, and the expert-bound publication.
//
// Extern symbols are only: the shared-memory buffer accessor, the four shuffle
// primitives, the global int32 load/store, and the sync. No original kernel,
// GGML struct or C++ routine is called.
//
// Packing: mm_ids_helper_store is one uint32 holding it (low 22 bits) and
// iex_used (high 10 bits).
//
// Raw ABI: u64 values are global byte addresses; the shared buffer is addressed by
// (block index, byte offset within the block).
extern "fg181m_ld_i32" fn ld_i32(src: u64, byte: u64) -> i64;
extern "fg181m_st_i32" fn st_i32(dst: u64, byte: u64, v: i32);
extern "fg181m_shfl_up" fn shfl_up(v: i32, mask: u32, off: i32, width: i32) -> i32;
extern "fg181m_shfl" fn shfl(v: i32, mask: u32, src: i32, width: i32) -> i32;
extern "fg181m_shfl_xor" fn shfl_xor(v: i32, mask: u32, off: i32, width: i32) -> i32;
extern "fg181m_shm_st" fn shm_st(slot: i32, byte: i64, v: i32);
extern "fg181m_shm_ld" fn shm_ld(slot: i32, byte: i64) -> i32;
extern "fg181m_syncwarp" fn syncwarp();
extern "fg181m_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// warp_reduce_any<8>: native takes the xor ladder when width != physical warp, so
// the lanes are reduced within their 8-wide group over offsets 4,2,1.

fn red_any8(v: i32) -> i32 {
    var x: i32 = v;
    var p: i32 = 0;
    p = shfl_xor(x, 4294967295 as u32, 4, 8);
    x = x | p;
    p = shfl_xor(x, 4294967295 as u32, 2, 8);
    x = x | p;
    p = shfl_xor(x, 4294967295 as u32, 1, 8);
    x = x | p;
    return x;
}
// warp_reduce_sum<32>: xor ladder over the whole warp, offsets 16,8,4,2,1.

fn red_sum32(v: i32) -> i32 {
    var s: i32 = v;
    var o: i32 = 16;

    while o > 0 {
        s = s + shfl_xor(s, 4294967295 as u32, o, 32);
        o = o / 2;
    }
    return s;
}
// The 8-wide specialisation, one warp per expert.
//   neu_padded = 8, warp = 32, so a warp holds 32/8 = 4 token slots per pass.

fn fg181m_helper(
    ids: u64,
    src1: u64,
    dst: u64,
    bounds: u64,
    n_tokens: i32,
    n_used_var: i32,
    nch: i32,
    si1: i32,
    sis1: i32,
    write_inverse: i32,
    n_expert_used: i32,
    gridm1: i32,
    bx: i32,
    tx: i32,
    warp: i32) {
    ensure(warp >= 8 && n_tokens > 0, 10);
    ensure(n_used_var >= 0 && n_used_var < (1024 as i32), 11);
    ensure(n_tokens < (4194304 as i32), 12);
    let expert: i32 = bx;
    let slot: i32 = 8 as i32;
    var nex_prev: i32 = 0;
    var it_compact: i32 = 0;
    var it0: i32 = 0;

    while it0 < n_tokens {
        let it: i32 = it0 + tx / slot;
        let iex: i32 = tx % slot;
        var expert_used: i32 = 2147483647;

        if iex < n_expert_used && it < n_tokens {
            expert_used = ld_i32(ids, ( ( (it * si1 + iex) as i64) * (4 as i64)) as u64) as i32;
        }
        // INT_MAX compares greater than any expert, so an out-of-range lane contributes
        // nothing to nex_prev, matching the native guarded read.
        var iex_used: i32 = -1;

        if expert_used == expert {
            iex_used = iex;
        }

        if expert_used < expert {
            nex_prev = nex_prev + 1;
        }
        // Whether the lanes at this token position used the expert: __any_sync over the
        // 8-wide group covers exactly one token slot.
        var selfv: i32 = 0;

        if iex_used != -1 {
            selfv = 1;
        }
        let add_self: i32 = red_any8(selfv);
        // Inclusive scan over token positions inside the warp.
        var add_lower: i32 = 0;
        var off: i32 = slot;

        while off < warp {
            let tmp: i32 = shfl_up(add_self, 4294967295 as u32, off, warp);

            if tx >= off {
                add_lower = add_lower + tmp;
            }
            off = off + slot;
        }

        if iex_used != -1 {
            // Pack (it, iex_used) as mm_ids_helper_store does.
            let word: i32 = (it & 4194303) | (iex_used << (22 as i32));
            shm_st(0, ( (it_compact + add_lower) as i64) * (4 as i64), word);
        }
        let total: i32 = shfl(add_lower + add_self, 4294967295 as u32, warp - 1, warp);
        it_compact = it_compact + total;
        it0 = it0 + warp / slot;
    }
    // Native: nex_prev = warp_reduce_sum<warp_size>(nex_prev), an xor ladder.
    let np: i32 = red_sum32(nex_prev);
    syncwarp();
    var itc: i32 = tx;

    while itc < it_compact {
        let word: i32 = shm_ld(0, (itc as i64) * (4 as i64));
        let s_it: i32 = word & 4194303;
        let s_iex: i32 = (word >> (22 as i32)) & 1023;
        let o: i32 = np + itc;
        st_i32(dst, ( (o as i64) * (4 as i64)) as u64, s_it * n_expert_used + s_iex);

        if write_inverse != 0 {
            st_i32(src1, ( ( ( (s_it * n_expert_used + s_iex) as i64)) * (4 as i64)) as u64, o);
        } else {
            st_i32(src1, ( (o as i64) * (4 as i64)) as u64, s_it * sis1 + s_iex % nch);
        }
        itc = itc + warp;
    }

    if tx == 0 {
        st_i32(bounds, ( (expert as i64) * (4 as i64)) as u64, np);
        // Native: only the last expert writes the closing bound.

        if expert == gridm1 {
            st_i32(bounds, ( (expert as i64) * (4 as i64) + (4 as i64)) as u64, np + it_compact);
        }
    }
}
// Retained entry; the parent never executes main. n_tokens=0 makes the body fail
// its first guard, so nothing is read or written.

fn main() {
    let z: u64 = 0;
    fg181m_helper(
        z,
        z,
        z,
        z,
        0 as i32,
        0 as i32,
        1 as i32,
        1 as i32,
        1 as i32,
        0 as i32,
        8 as i32,
        0 as i32,
        0,
        0,
        32 as i32);
}

