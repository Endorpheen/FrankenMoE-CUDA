// CUDA_MMQ: the Q6_K MMA-layout tile loader, scale rows, in Goose.
//
// block_q6_K = { uint8_t ql[128]; uint8_t qh[64]; int8_t scales[16]; ggml_half d; } = 210 bytes:
//   ql @0, qh @128, scales @192, d @208.
//
// Native (MMA branch), fourth loop, one store per thread:
//   i   = (i0 + threadIdx.y*rows_per_warp + threadIdx.x/(MMQ_TILE_NE_K/8)) % I
//   bxi = x + kbx0 + i*stride + (threadIdx.x % (MMQ_TILE_NE_K/8)) / 4
//   x_sc[i*sram_stride + threadIdx.x%4] = get_int_b2(bxi->scales, threadIdx.x % (MMQ_TILE_NE_K/8))
//
// get_int_b2 reads four bytes at 4*index with two 2-byte loads; the byte content is the same as
// a single 4-byte load, so ld_i32 is the right primitive. x_sc is int*, slots scale by 4 bytes.
// Note the destination slot uses %4 while the source index uses %(MMQ_TILE_NE_K/8); they differ
// whenever that ratio is not four, so both are kept as written.
extern "fg258sc_fail" fn fail(c: i64);
extern "fg258sc_ld_i32" fn ld_i32(base: u64, byte: i64) -> i64;
extern "fg258sc_st_i32" fn st_i32(base: u64, byte: i64, v: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn write_sc(x: u64, sc: u64, kbx0: i64, stride: i64, i: i64, sram_stride: i64, tx: i64, tile_ne_k: i64) -> bool {
    let idx: i64 = tx % (tile_ne_k / (8 as i64));
    let bxi: u64 = x + ( (kbx0 + i * stride + idx / (4 as i64)) * 210) as u64;
    let v: i64 = ld_i32(bxi + 192 as u64, idx * 4 as i64);
    st_i32(sc, (i * sram_stride + (tx % (4 as i64))) * 4 as i64, v);
    true
}

fn main() {
    write_sc(0 as u64, 0 as u64, 0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as i64, 32 as i64);
}

