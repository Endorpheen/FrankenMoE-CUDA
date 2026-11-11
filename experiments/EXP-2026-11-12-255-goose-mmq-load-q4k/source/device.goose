// CUDA_MMQ: the Q4_K tile loader for the MMA data layout, in Goose.
//
// block_q4_K is { ggml_half2 dm; uint8_t scales[12]; uint8_t qs[128]; } = 144 bytes:
//   dm @0, scales @4, qs @16.
//
// Native (mmq-load-tiles.cuh, MMA branch):
//   qs0 = get_int_b4(bxi->qs, txi)
//   x_qs[i*sram_stride + 16*(txi/8) + txi%8 + 0] = (qs0 >> 0) & 0x0F0F0F0F
//   x_qs[i*sram_stride + 16*(txi/8) + txi%8 + 8] = (qs0 >> 4) & 0x0F0F0F0F
//   dm  = bxi->dm * make_half2(1.0f, -1.0f)
//   x_dm[i*sram_stride + 4*ksc + l] = dm * make_half2(sc8[l], m8[l]), l = 0..3
// The arithmetic is Goose; packing/unpacking half2 and the 6-bit scale unpack stay primitives.

extern "fg255q_fail" fn fail(c:i64);
extern "fg255q_ld_i32" fn ld_i32(base:u64, idx:i64)->i64;
extern "fg255q_st_i32" fn st_i32(base:u64, idx:i64, v:i64);
extern "fg255q_unpack_scales" fn unpack_scales(scales:u64, k:i64)->i64;
extern "fg255q_dm_scale" fn dm_scale(dm_bits:i64, a:i64, b:i64)->i64;

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn load_row(x:u64, qs_stage:u64, dm_stage:u64, kbx0:i64, stride:i64, i:i64, txi:i64,
            sram_stride:i64, ksc:i64) -> bool {
    let bxi:u64 = x + ((kbx0 + i*stride) * 144) as u64;
    ensure(txi >= 0 as i64 && txi < 32 as i64, 1);

    // four consecutive bytes of qs, indexed as int
    let qs0:i64 = ld_i32(bxi, 16 as i64 + txi*4 as i64);
    let slot:i64 = (i*sram_stride + 16*(txi/8 as i64) + (txi % 8 as i64)) * 4 as i64;
    st_i32(qs_stage, slot,             (qs0 >> 0 as i64) & 0x0F0F0F0F as i64);
    st_i32(qs_stage, slot + 8*4 as i64, (qs0 >> 4 as i64) & 0x0F0F0F0F as i64);

    let sc32:i64 = unpack_scales(bxi + 4 as u64, ksc + 0 as i64);
    let m32:i64  = unpack_scales(bxi + 4 as u64, ksc + 2 as i64);
    let dm_bits:i64 = ld_i32(bxi, 0 as i64);
    var l:i64 = 0 as i64;
    while l < 4 {
        let sc8:i64 = (sc32 >> ((l*8) as i64)) & 0xff as i64;
        let m8:i64  = (m32  >> ((l*8) as i64)) & 0xff as i64;
        st_i32(dm_stage, (i*sram_stride + 4*ksc as i64 + l) * 4 as i64, dm_scale(dm_bits, sc8, m8));
        l = l + 1 as i64;
    }
    true
}

fn main(){ load_row(0 as u64,0 as u64,0 as u64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64); }
