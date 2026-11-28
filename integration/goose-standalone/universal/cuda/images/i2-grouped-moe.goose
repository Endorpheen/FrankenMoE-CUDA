struct I2MoePair { x: u32, y: u32 }
struct I2MoeAcc { x: f32, y: f32, z: f32, w: f32 }

extern "moe_word" fn word(base: u64, index: i64) -> u64;
extern "inkling_read_half_pair_g" fn pair(base: u64, index: i64) -> u32;
extern "moe_store" fn store(base: u64, index: i64, value: f32);
extern "inkling_pack_element_g" fn pack(src: u64, dst: u64, index: i64, output: i64);
extern "inkling_half_g" fn iq4_half(address: u64) -> f32;
extern "inkling_pack_half_pair_g" fn iq4_pack(lo: f32, hi: f32) -> u32;
extern "fg248d_ld_u8" fn iq4_byte(address: u64) -> u32;
extern "fg185q_ld_u16" fn iq4_u16(address: u64, offset: u64) -> u32;

fn iq4_level(index: u32) -> f32 {
    let codes: u64 = if index < 8 { 0xf6eaddcfbfad9881 } else { 0x7159453526190d01 };
    return (((codes >> (8 * (index & 7))) as! i8) as f32);
}

fn iq_pairs(base: u64, k: i32, column: u32, kind: i32) -> I2MoePair {
    let offset = if kind == 23 { (k >> 8) * 136 } else { (k >> 5) * 18 };
    let tile = base + (offset as! u64);
    var scale = iq4_half(tile);
    var values = tile + 2 + (column as u64);
    if kind == 23 {
        let sub = ((k as! u32) >> 5) & 7;
        let low = (iq4_byte(tile + 4 + ((sub >> 1) as u64)) >> (4 * (sub & 1))) & 15;
        let high = (iq4_u16(tile, 2) >> (2 * sub)) & 3;
        scale *= (((low + 16 * high) as i32) - 32) as f32;
        values += 6 + (16 * (sub as u64));
    }
    let shift = ((k as! u32) & 16) >> 2;
    let a = scale * iq4_level((iq4_byte(values) >> shift) & 15);
    let b = scale * iq4_level((iq4_byte(values + 1) >> shift) & 15);
    let c = scale * iq4_level((iq4_byte(values + 8) >> shift) & 15);
    let d = scale * iq4_level((iq4_byte(values + 9) >> shift) & 15);
    return I2MoePair { iq4_pack(a, b), iq4_pack(c, d) };
}
extern "moe_dot" fn dot(acc: I2MoeAcc, low: I2MoePair, high: I2MoePair, lo: u32, hi: u32) -> I2MoeAcc;

export fn i2_moe_pack(input: u64, jobs: u64, packed: u64, width: i64, count: i64, group: i64, job: i64, lane: i64) {
    let index = 256 * group + lane;
    if job < count && index < width { pack(input + word(jobs, 3 * job), packed, index, job * width + index); }
}

fn moe_scatter(output: u64, jobs: u64, route: i64, row: i64, rows: i64, low: f32, high: f32) {
    let dst = output + word(jobs, 3 * route + 1);
    if row < rows { store(dst, row, low); }
    if row + 8 < rows { store(dst, row + 8, high); }
}

export fn i2_grouped_moe(weights: u64, packed: u64, output: u64, jobs: u64, tiles: u64, stride: u64,
    width: i64, rows: i64, kind: i32, groups: i64, group: i64, tile: i64, thread: i64) {
    let lane = thread % 32;
    let row = 64 * group + 16 * (thread / 32) + lane / 4;
    let column = 2 * (lane % 4);
    let first = word(tiles, tile * 3) as! i64;
    let count = word(tiles, tile * 3 + 1) as! i64;
    let w0 = weights + word(tiles, tile * 3 + 2) + (row as! u64) * stride;
    let w1 = w0 + 8 * stride;
    let zero = I2MoePair {x:0,y:0};
    let acc_zero = I2MoeAcc {x:0.0,y:0.0,z:0.0,w:0.0};
    var acc: I2MoeAcc[4] = [acc_zero; 4];
    var k: i64 = 0;
    while k < width {
        let low = if row < rows { iq_pairs(w0, k as! i32, column as! u32, kind) } else { zero };
        let high = if row + 8 < rows { iq_pairs(w1, k as! i32, column as! u32, kind) } else { zero };
        for g in groups {
            if 8 * g >= count { continue; }
            let index = 8 * g + lane / 4;
            let offset = (first + index) * width + k + column;
            let lo = if index < count { pair(packed, offset) } else { 0 };
            let hi = if index < count { pair(packed, offset + 8) } else { 0 };
            acc[g] = dot(acc[g], low, high, lo, hi);
        }
        k += 16;
    }
    for g in groups {
        let col = column + 8 * g;
        if col < count { moe_scatter(output, jobs, first + col, row, rows, acc[g].x, acc[g].z); }
        if col + 1 < count { moe_scatter(output, jobs, first + col + 1, row, rows, acc[g].y, acc[g].w); }
    }
}
