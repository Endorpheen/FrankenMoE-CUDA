struct I2MoePair { x: u32, y: u32 }
struct I2MoeAcc { x: f32, y: f32, z: f32, w: f32 }

extern "moe_word" fn word(base: u64, index: i64) -> u64;
extern "moe_pair" fn pair(base: u64, index: i64) -> u32;
extern "moe_store" fn store(base: u64, index: i64, value: f32);
extern "moe_pack" fn pack(src: u64, dst: u64, index: i64, output: i64);
extern "moe_iq_pairs" fn iq_pairs(base: u64, k: i32, col: u32, kind: i32) -> I2MoePair;
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
