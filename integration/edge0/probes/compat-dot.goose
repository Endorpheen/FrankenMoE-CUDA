// CPU-compatible packed activation dot pieces. Q3 scale decoding reuses EXP251.
extern "e0c_ld_i8" fn co_i8(base: u64, offset: u64) -> i32;
extern "e0c_iq4_value" fn co_iq4(index: i32) -> i32;

export fn e0c_q3_part(w: u64, a: u64, tile: i64, lane: i64) -> i32 {
    let wb: i64 = tile * 110;
    let ab: i64 = tile * 292;
    var sum: i32 = 0;
    var q: i64 = lane;
    while q < 256 {
        let shift: u32 = (2 * ((q % 128) / 32)) as u32;
        let bits: i32 = ((ld_u8(w, (wb + 32 + (q / 128) * 32 + q % 32) as u64) >> shift) & 3) as i32;
        let mask: u32 = (1 as u32) << ((q / 32) as u32);
        let high: u32 = ld_u8(w, (wb + q % 32) as u64) & mask;
        let quant: i32 = bits - (if high != 0 { 0 } else { 4 });
        let scale: i32 = (us_of(w, wb + 96, q / 16) - 32) as i32;
        sum += scale * quant * co_i8(a, (ab + 4 + q) as u64);
        q += 32;
    }
    return sum;
}

export fn e0c_q4_part(w: u64, a: u64, tile: i64, lane: i64) -> i32 {
    let raw: u32 = ld_u8(w, (tile * 18 + 2 + lane % 16) as u64);
    let bits: i32 = (if lane < 16 { raw & 15 } else { raw >> 4 }) as i32;
    return (bits - 8) * co_i8(a, (tile * 34 + 2 + lane) as u64);
}

export fn e0c_iq4_part(w: u64, a: u64, tile: i64, lane: i64) -> i32 {
    let raw: u32 = ld_u8(w, (tile * 18 + 2 + lane % 16) as u64);
    let bits: i32 = (if lane < 16 { raw & 15 } else { raw >> 4 }) as i32;
    return co_iq4(bits) * co_i8(a, (tile * 34 + 2 + lane) as u64);
}
