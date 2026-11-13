// Apex CUDA IQ2_XXS and IQ2_XS dequantization to half and bfloat16 (Goose).
// Extends EXP255/EXP256, which covered the F32 destinations.
//
// Arithmetic is the F32 modules': for IQ2_XXS the q2 word layout, aux8 grid
// index, aux32 exponent and 7*il sign index; for IQ2_XS the per-thread q2 word,
// 9-bit grid index, 7-bit sign index and the scales[ib] sub-block scale. The
// shared tables iq2xxs_grid, iq2xs_grid and ksigns_iq2xs are imported as
// explicit device constant primitives. Only the store differs.
//
// Layouts: block_iq2_xxs 66 bytes (d at 0, qs[32] u16 at 2);
// block_iq2_xs 74 bytes (d at 0, qs[32] u16 at 2, scales[8] at 66).
extern "fg269d_ld_u8" fn ld_u8(vx: u64, o: u64) -> u32;
extern "fg269d_ld_u16" fn ld_u16(vx: u64, o: u64) -> u32;
extern "fg269d_ld_half" fn ld_half(vx: u64, o: u64) -> f32;
extern "fg269d_grid_xxs" fn grid_xxs(i: u32) -> u64;
extern "fg269d_grid_xs" fn grid_xs(i: u32) -> u64;
extern "fg269d_ksign" fn ksign(i: u32) -> u32;
extern "fg269d_st_half" fn st_half(yy: u64, o: u64, v: f32);
extern "fg269d_st_bf16" fn st_bf16(yy: u64, o: u64, v: f32);
extern "fg269d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn sgn(b: i64) -> f32 {
    if b > 0 as i64 {
        return -1.0 as f32;
    }
    1.0 as f32
}

fn emit8_h(g: u64, sg: u32, d: f32, yy: u64, ybase: i64) {
    var j: i64 = 0;

    while j < 8 as i64 {
        let shk: u64 = (j * 8) as u64;
        let gb: i64 = ( (g >> shk) & (255 as u64)) as i64;
        let m: i64 = (1 as i64) << j;
        let bit: i64 = (sg as i64) & m;
        st_half(yy, ( ( (ybase + j) * (2 as i64)) as u64), d * ( (gb) as f32) * sgn(bit));
        j = j + 1;
    }
}

fn emit8_b(g: u64, sg: u32, d: f32, yy: u64, ybase: i64) {
    var j: i64 = 0;

    while j < 8 as i64 {
        let shk: u64 = (j * 8) as u64;
        let gb: i64 = ( (g >> shk) & (255 as u64)) as i64;
        let m: i64 = (1 as i64) << j;
        let bit: i64 = (sg as i64) & m;
        st_bf16(yy, ( ( (ybase + j) * (2 as i64)) as u64), d * ( (gb) as f32) * sgn(bit));
        j = j + 1;
    }
}

fn q2_xxs_f16(vx: u64, yy: u64, i: i64, tid: i64) {
    ensure(tid >= 0 && tid < 32 as i64, 10);
    let il: i64 = tid / 8;
    let ib: i64 = tid % 8;
    let blk: i64 = i * (66 as i64);
    let ybase: i64 = i * (256 as i64) + 32 * ib + 8 * il;
    let q2: i64 = blk + 2 as i64 + 8 * ib;
    let dhalf: f32 = ld_half(vx, (blk) as u64);
    let gi: u32 = ld_u8(vx, ( (q2 + il) as u64));
    let a2: u32 = ld_u16(vx, ( (q2 + 4 as i64) as u64));
    let a3: u32 = ld_u16(vx, ( (q2 + 6 as i64) as u64));
    let aux: u32 = a2 | (a3 << (16 as u32));
    let e: u32 = aux >> (28 as u32);
    let d: f32 = dhalf * ( (0.5 as f32) + ( (e) as f32)) * (0.25 as f32);
    let sh7: u32 = (7 * il) as u32;
    let sg: u32 = ksign( (aux >> sh7) & (127 as u32));
    emit8_h(grid_xxs(gi), sg, d, yy, ybase);
}

fn q2_xxs_bf16(vx: u64, yy: u64, i: i64, tid: i64) {
    ensure(tid >= 0 && tid < 32 as i64, 10);
    let il: i64 = tid / 8;
    let ib: i64 = tid % 8;
    let blk: i64 = i * (66 as i64);
    let ybase: i64 = i * (256 as i64) + 32 * ib + 8 * il;
    let q2: i64 = blk + 2 as i64 + 8 * ib;
    let dhalf: f32 = ld_half(vx, (blk) as u64);
    let gi: u32 = ld_u8(vx, ( (q2 + il) as u64));
    let a2: u32 = ld_u16(vx, ( (q2 + 4 as i64) as u64));
    let a3: u32 = ld_u16(vx, ( (q2 + 6 as i64) as u64));
    let aux: u32 = a2 | (a3 << (16 as u32));
    let e: u32 = aux >> (28 as u32);
    let d: f32 = dhalf * ( (0.5 as f32) + ( (e) as f32)) * (0.25 as f32);
    let sh7: u32 = (7 * il) as u32;
    let sg: u32 = ksign( (aux >> sh7) & (127 as u32));
    emit8_b(grid_xxs(gi), sg, d, yy, ybase);
}

fn q2_xs_f16(vx: u64, yy: u64, i: i64, tid: i64) {
    ensure(tid >= 0 && tid < 32 as i64, 10);
    let il: i64 = tid / 8;
    let ib: i64 = tid % 8;
    let blk: i64 = i * (74 as i64);
    let ybase: i64 = i * (256 as i64) + 32 * ib + 8 * il;
    let q2: i64 = blk + 2 as i64 + 8 * ib;
    let dhalf: f32 = ld_half(vx, (blk) as u64);
    let qv: u32 = ld_u16(vx, ( (q2 + 2 * il) as u64));
    let gi: u32 = qv & (511 as u32);
    let sc: u32 = ld_u8(vx, ( (blk + 66 as i64 + ib) as u64));
    let sh: u32 = (4 * (il / 2)) as u32;
    let d: f32 = dhalf * ( (0.5 as f32) + ( ( (sc >> sh) & (15 as u32)) as f32)) * (0.25 as f32);
    let sg: u32 = ksign(qv >> (9 as u32));
    emit8_h(grid_xs(gi), sg, d, yy, ybase);
}

fn q2_xs_bf16(vx: u64, yy: u64, i: i64, tid: i64) {
    ensure(tid >= 0 && tid < 32 as i64, 10);
    let il: i64 = tid / 8;
    let ib: i64 = tid % 8;
    let blk: i64 = i * (74 as i64);
    let ybase: i64 = i * (256 as i64) + 32 * ib + 8 * il;
    let q2: i64 = blk + 2 as i64 + 8 * ib;
    let dhalf: f32 = ld_half(vx, (blk) as u64);
    let qv: u32 = ld_u16(vx, ( (q2 + 2 * il) as u64));
    let gi: u32 = qv & (511 as u32);
    let sc: u32 = ld_u8(vx, ( (blk + 66 as i64 + ib) as u64));
    let sh: u32 = (4 * (il / 2)) as u32;
    let d: f32 = dhalf * ( (0.5 as f32) + ( ( (sc >> sh) & (15 as u32)) as f32)) * (0.25 as f32);
    let sg: u32 = ksign(qv >> (9 as u32));
    emit8_b(grid_xs(gi), sg, d, yy, ybase);
}

fn main() {
    q2_xxs_f16(0, 0, 0 as i64, 0 as i64);
    q2_xxs_bf16(0, 0, 0 as i64, 0 as i64);
    q2_xs_f16(0, 0, 0 as i64, 0 as i64);
    q2_xs_bf16(0, 0, 0 as i64, 0 as i64);
}

