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

struct I2MoeCodes { first: u32, second: u32, scale: f32 }

fn iq_codes(base: u64, k: i64, column: u64, kind: i32) -> I2MoeCodes {
    let offset = if kind == 23 { (k >> 8) * 136 } else { (k >> 5) * 18 };
    let tile = base + (offset as! u64);
    var scale = iq4_half(tile);
    var values = 2 + column;
    if kind == 23 {
        let sub = ((k as! u32) >> 5) & 7;
        let low = (iq4_byte(tile + 4 + ((sub >> 1) as u64)) >> (4 * (sub & 1))) & 15;
        let high = (iq4_u16(tile, 2) >> (2 * sub)) & 3;
        scale *= (((low + 16 * high) as i32) - 32) as f32;
        values += 6 + (16 * (sub as u64));
    }
    return I2MoeCodes { iq4_u16(tile, values), iq4_u16(tile, values + 8), scale };
}

fn iq_pairs(codes: I2MoeCodes, shift: u32) -> I2MoePair {
    let first = codes.first >> shift;
    let second = codes.second >> shift;
    let scale = codes.scale;
    return I2MoePair {
        iq4_pack(scale * iq4_level(first & 15), scale * iq4_level((first >> 8) & 15)),
        iq4_pack(scale * iq4_level(second & 15), scale * iq4_level((second >> 8) & 15)) };
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
    let no_codes = I2MoeCodes {first:0,second:0,scale:0.0};
    let acc_zero = I2MoeAcc {x:0.0,y:0.0,z:0.0,w:0.0};
    var acc: I2MoeAcc[4] = [acc_zero; 4];
    var k: i64 = 0;
    while k < width {
        let low_codes = if row < rows { iq_codes(w0, k, column as! u64, kind) } else { no_codes };
        let high_codes = if row + 8 < rows { iq_codes(w1, k, column as! u64, kind) } else { no_codes };
        for half in 2 {
            let position = k + 16 * half;
            if position >= width { break; }
            let shift = (4 * half) as! u32;
            let zero = I2MoePair {x:0,y:0};
            let low = if row < rows { iq_pairs(low_codes, shift) } else { zero };
            let high = if row + 8 < rows { iq_pairs(high_codes, shift) } else { zero };
            for g in groups {
                if 8 * g >= count { continue; }
                let index = 8 * g + lane / 4;
                let offset = (first + index) * width + position + column;
                let lo = if index < count { pair(packed, offset) } else { 0 };
                let hi = if index < count { pair(packed, offset + 8) } else { 0 };
                acc[g] = dot(acc[g], low, high, lo, hi);
            }
        }
        k += 32;
    }
    for g in groups {
        let col = column + 8 * g;
        if col < count { moe_scatter(output, jobs, first + col, row, rows, acc[g].x, acc[g].z); }
        if col + 1 < count { moe_scatter(output, jobs, first + col + 1, row, rows, acc[g].y, acc[g].w); }
    }
}

extern "fg_device_global_load" fn dense_input_load(address: u64) -> f32;
extern "inkling_q8_value_g" fn dense_q8_value(base: u64, lane: u32) -> f32;
extern "inkling_f16_bits" fn dense_half_bits(value: f32) -> u16;
extern "inkling_f16_value" fn dense_half_value(bits: u16) -> f32;
extern "fg_cuda_store_u16" fn dense_half_store(address: u64, value: u16);

fn dense_store_split(output: u64, index: i64, plane: i64, value: f32) {
    let high = dense_half_bits(value);
    let low = dense_half_bits((value - dense_half_value(high)) * 4096.0);
    dense_half_store(output + ((2 * index) as! u64), high);
    dense_half_store(output + ((2 * (index + plane)) as! u64), low);
}

export fn i2_dense_split_inputs(input: u64, jobs: u64, output: u64, width: i64,
    tokens: i64, group: i64, token: i64, lane: i64) {
    let k = group * 128 + lane;
    if k >= width { return; }
    let value = dense_input_load(input + word(jobs, 2 * token) + ((4 * k) as! u64));
    dense_store_split(output, token * width + k, tokens * width, value);
}

export fn i2_dense_split_weights(input: u64, output: u64, stride: u64, width: i64,
    rows: i64, group: i64, row: i64, lane: i64) {
    let k = group * 128 + lane;
    if k >= width { return; }
    let quant = input + (row as! u64) * stride + ((k / 32 * 34) as! u64);
    let value = dense_q8_value(quant, (k % 32) as! u32);
    dense_store_split(output, row * width + k, rows * width, value);
}

struct I2DenseAcc { main: I2MoeAcc, correction: I2MoeAcc }

fn dense_split_step(acc: I2DenseAcc, packed: u64, lo: i64, source: i64,
        w0: I2MoePair, w1: I2MoePair, l0: I2MoePair, l1: I2MoePair) -> I2DenseAcc {
    let x0 = pair(packed,source);
    let x1 = pair(packed,source+8);
    let y0 = pair(packed,lo+source);
    let y1 = pair(packed,lo+source+8);
    let main = dot(acc.main,w0,w1,x0,x1);
    let cross = dot(acc.correction,w0,w1,y0,y1);
    return I2DenseAcc {main:main, correction:dot(cross,l0,l1,x0,x1)};
}

fn dense_split_scatter(output: u64, jobs: u64, token: i64, row: i64, acc: I2DenseAcc) {
    let first = output + word(jobs,2*token+1);
    let second = output + word(jobs,2*token+3);
    let a = acc.main;
    let b = acc.correction;
    store(first, row, a.x + b.x * 0.000244140625);
    store(second, row, a.y + b.y * 0.000244140625);
    store(first, row+8, a.z + b.z * 0.000244140625);
    store(second, row+8, a.w + b.w * 0.000244140625);
}

export fn i2_dense_split_mma(weights: u64, packed: u64, output: u64, jobs: u64,
    width: i64, rows: i64, tokens: i64, group: i64, tile: i64, thread: i64) {
    let lane = thread % 32;
    let row = 64 * group + 16 * (thread / 32) + lane / 4;
    let column = 2 * (lane % 4);
    let weight_lo = rows * width;
    let packed_lo = tokens * width;
    let zero = I2MoeAcc {x:0.0,y:0.0,z:0.0,w:0.0};
    var a = I2DenseAcc {main:zero,correction:zero};
    var b = a;
    var c = a;
    var d = a;
    let row0 = row * width + column;
    let row1 = row0 + 8 * width;
    let base = (tile * 32 + lane / 4) * width + column;
    var k: i64 = 0;
    while k < width {
        let w0 = I2MoePair {pair(weights,row0+k),pair(weights,row0+k+8)};
        let w1 = I2MoePair {pair(weights,row1+k),pair(weights,row1+k+8)};
        let l0 = I2MoePair {pair(weights,weight_lo+row0+k),pair(weights,weight_lo+row0+k+8)};
        let l1 = I2MoePair {pair(weights,weight_lo+row1+k),pair(weights,weight_lo+row1+k+8)};
        let source = base + k;
        a = dense_split_step(a,packed,packed_lo,source,w0,w1,l0,l1);
        b = dense_split_step(b,packed,packed_lo,source+8*width,w0,w1,l0,l1);
        c = dense_split_step(c,packed,packed_lo,source+16*width,w0,w1,l0,l1);
        d = dense_split_step(d,packed,packed_lo,source+24*width,w0,w1,l0,l1);
        k += 16;
    }
    let token = tile * 32 + column;
    dense_split_scatter(output,jobs,token,row,a);
    dense_split_scatter(output,jobs,token+8,row,b);
    dense_split_scatter(output,jobs,token+16,row,c);
    dense_split_scatter(output,jobs,token+24,row,d);
}

extern "fg_cuda_cp16" fn dense_split_copy(destination: u64, source: u64);
extern "fg_cuda_cpwait" fn dense_split_wait();
extern "input_sync" fn dense_split_sync();
extern "inkling_read_shared_half_pair_g" fn inkling_shared_pair(address: u64, index: i64) -> u32;

fn dense_split_stage(shared: u64, packed: u64, low: i64, width: i64, tile: i64, channel: i64, thread: i64) {
    for page in 4 {
        let vector = thread + page * 128;
        let plane = vector / 256;
        let token = (vector % 256) / 8;
        let col = 8 * (vector % 8);
        let src = plane * low + (tile * 32 + token) * width + channel + col;
        let dst = (plane * 32 + token) * 72 + col;
        dense_split_copy(shared + ((2 * dst) as! u64), packed + ((2 * src) as! u64));
    }
}
fn dense_split_shared_step(acc: I2DenseAcc, packed: u64, lo: i64, source: i64,
        w0: I2MoePair, w1: I2MoePair, l0: I2MoePair, l1: I2MoePair) -> I2DenseAcc {
    let x0 = inkling_shared_pair(packed,source);
    let x1 = inkling_shared_pair(packed,source+8);
    let y0 = inkling_shared_pair(packed,lo+source);
    let y1 = inkling_shared_pair(packed,lo+source+8);
    let main = dot(acc.main,w0,w1,x0,x1);
    let cross = dot(acc.correction,w0,w1,y0,y1);
    return I2DenseAcc {main:main, correction:dot(cross,l0,l1,x0,x1)};
}


export fn i2_dense_split_mma_shared(weights: u64, packed: u64, output: u64, jobs: u64,
    width: i64, rows: i64, tokens: i64, group: i64, tile: i64, thread: i64, shared: u64) {
    let lane = thread % 32;
    let row = 64 * group + 16 * (thread / 32) + lane / 4;
    let column = 2 * (lane % 4);
    let weight_lo = rows * width;
    let packed_lo = tokens * width;
    let zero = I2MoeAcc {x:0.0,y:0.0,z:0.0,w:0.0};
    var a = I2DenseAcc {main:zero,correction:zero};
    var b = a;
    var c = a;
    var d = a;
    let row0 = row * width + column;
    let row1 = row0 + 8 * width;
    let base = (lane / 4) * 72 + column;
    var channel: i64 = 0;
    var current: u64 = shared;
    var next: u64 = shared + 9216;
    dense_split_stage(current, packed, packed_lo, width, tile, 0, thread);
    dense_split_wait();
    dense_split_sync();
    while channel < width {
        if channel + 64 < width { dense_split_stage(next, packed, packed_lo, width, tile, channel + 64, thread); }
        for step in 4 {
            let k = channel + 16 * step;
            let w0 = I2MoePair {pair(weights,row0+k),pair(weights,row0+k+8)};
            let w1 = I2MoePair {pair(weights,row1+k),pair(weights,row1+k+8)};
            let l0 = I2MoePair {pair(weights,weight_lo+row0+k),pair(weights,weight_lo+row0+k+8)};
            let l1 = I2MoePair {pair(weights,weight_lo+row1+k),pair(weights,weight_lo+row1+k+8)};
            let source = base + 16 * step;
            a = dense_split_shared_step(a,current,2304,source,w0,w1,l0,l1);
            b = dense_split_shared_step(b,current,2304,source+576,w0,w1,l0,l1);
            c = dense_split_shared_step(c,current,2304,source+1152,w0,w1,l0,l1);
            d = dense_split_shared_step(d,current,2304,source+1728,w0,w1,l0,l1);
        }
        dense_split_wait();
        dense_split_sync();
        channel += 64;
        let previous = current;
        current = next;
        next = previous;
    }
    let token = tile * 32 + column;
    dense_split_scatter(output,jobs,token,row,a);
    dense_split_scatter(output,jobs,token+8,row,b);
    dense_split_scatter(output,jobs,token+16,row,c);
    dense_split_scatter(output,jobs,token+24,row,d);
}
