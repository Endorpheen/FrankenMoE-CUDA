// CUDA_MMQ: the Q4_K MMA-layout tile loader in Goose.
// mode 0 writes only the qs staging (native first loop), mode 1 only the dm rows (native second loop).
// block_q4_K = { ggml_half2 dm; uint8_t scales[12]; uint8_t qs[128]; } = 144 bytes (dm@0, scales@4, qs@16).

extern "fg255q_fail" fn fail(c:i64);
extern "fg255q_ld_i32" fn ld_i32(base:u64, byte:i64)->i64;
extern "fg255q_st_i32" fn st_i32(base:u64, byte:i64, v:i64);
extern "fg255q_unpack_scales" fn unpack_scales(scales:u64, k:i64)->i64;
extern "fg255q_dm_scale" fn dm_scale(dm_bits:i64, a:i64, b:i64)->i64;

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn load_row(x:u64, qs_stage:u64, dm_stage:u64, mode:i64, kbx0:i64, stride:i64, i:i64,
            txi:i64, sram_stride:i64) -> bool {
    let bxi:u64 = x + ((kbx0 + i*stride) * 144) as u64;
    if mode == 0 as i64 {
        ensure(txi >= 0 as i64 && txi < 32 as i64, 1);
        let qs0:i64 = ld_i32(bxi, 16 as i64 + txi*4 as i64);
        let slot:i64 = (i*sram_stride + 16*(txi/8 as i64) + (txi % 8 as i64)) * 4 as i64;
        st_i32(qs_stage, slot,              (qs0 >> 0 as i64) & 0x0F0F0F0F as i64);
        st_i32(qs_stage, slot + 8*4 as i64, (qs0 >> 4 as i64) & 0x0F0F0F0F as i64);
        return true;
    }
    let ksc:i64 = txi % 2 as i64;
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
