// CUDA_MMQ: the Q6_K MMA-layout tile loader, df row, in Goose.
//
// block_q6_K = { uint8_t ql[128]; uint8_t qh[64]; int8_t scales[16]; ggml_half d; } = 210 bytes:
//   ql @0, qh @128, scales @192, d @208.
//
// Native (MMA branch), third loop, one store per thread:
//   i = (i0 + threadIdx.y*warp_size + threadIdx.x) % I
//   x_df[i*sram_stride] = bxi->d
//
// x_df is float*, so the slot index scales by 4 bytes. bxi->d is ggml_half and the native
// assignment widens it to float; that widening is a data primitive, the addressing is Goose.
extern "fg258df_fail" fn fail(c: i64);
extern "fg258df_ld_f16_f32" fn ld_h(base: u64, byte: i64) -> f32;
extern "fg258df_st_f32" fn st_f(base: u64, byte: i64, v: f32);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn write_df(x: u64, df: u64, kbx0: i64, stride: i64, i: i64, sram_stride: i64) -> bool {
    let bxi: u64 = x + ( (kbx0 + i * stride) * 210) as u64;
    st_f(df, (i * sram_stride) * 4 as i64, ld_h(bxi, 208 as i64));
    true
}

fn main() {
    write_df(0 as u64, 0 as u64, 0 as i64, 0 as i64, 0 as i64, 0 as i64);
}

