extern "fg176r_ld_f32" fn ld_f32(src: u64, byte: u64) -> f32;
extern "fg176r_st_f16" fn st_f16(dst: u64, byte: u64, v: f32);
extern "fg176r_st_u8" fn st_u8(dst: u64, byte: u64, v: u32);
extern "fg176r_fabs" fn fabsv(v: f32) -> f32;
extern "fg176r_i8trunc" fn i8trunc(v: f32) -> i32;
export fn fg176r_quant_q4_0(src: u64, sb: u64, dst: u64, db: u64) {
    let zero: f32 = 0.0;
    let neg8: f32 = -8.0;
    let bias: f32 = 8.5;
    let one: f32 = 1.0;
    let four: u64 = 4;
    let nqk: i32 = 32;
    let halfn: i32 = 16;
    let nbyte: i32 = 16;
    let maxq: i32 = 15;
    let mask: i32 = 255;
    var amax: f32 = zero;
    var vmax: f32 = zero;
    var j: i32 = 0;

    while j < nqk {
        let o: u64 = sb + ( (j as i64) as u64) * four;
        let v: f32 = ld_f32(src, o);
        let a: f32 = fabsv(v);

        if amax < a {
            amax = a;
            vmax = v;
        }
        j += 1;
    }
    let d: f32 = vmax / neg8;
    var id: f32 = zero;

    if d != zero {
        id = one / d;
    }
    st_f16(dst, db, d);
    var k: i32 = 0;

    while k < nbyte {
        let o0: u64 = sb + ( (k as i64) as u64) * four;
        let o1: u64 = sb + ( ( (k + halfn) as i64) as u64) * four;
        let x0: f32 = ld_f32(src, o0) * id;
        let x1: f32 = ld_f32(src, o1) * id;
        let t0: i32 = i8trunc(x0 + bias);
        let t1: i32 = i8trunc(x1 + bias);
        let q0: i32 = if t0 > maxq {
            maxq
        } else {
            t0
        }
        ;
        let q1: i32 = if t1 > maxq {
            maxq
        } else {
            t1
        }
        ;
        let lo: u32 = (q0 & mask) as u32;
        let hi: u32 = (q1 & mask) as u32;
        let h4: u32 = (hi << (4 as u32)) & (mask as u32);
        st_u8(dst, db + (2 as u64) + ( (k as i64) as u64), lo | h4);
        k += 1;
    }
}
