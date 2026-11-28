extern fn i2_epilogue_read(base: u64, index: i64) -> f32;
extern fn i2_epilogue_write(base: u64, index: i64, value: f32);
extern fn i2_epilogue_tanh(value: f32) -> f32;
extern fn i2_epilogue_exp(value: f32) -> f32;
extern "inkling_fast_exp_g" fn i2_epilogue_exp_approx(value: f32) -> f32;
extern fn i2_epilogue_rcp_approx(value: f32) -> f32;


export fn inkling_postconv(input: u64, weights: u64, history: u64, output: u64,
    rows: i64, group: i64, row: i64, lane: i64) {
    if row >= rows { return; }
    let channel = group * 128 + lane;
    let offset = row * 2304 + channel;
    var sum: f32 = 0.0;
    for tap in 4 {
        let source = row + tap - 3;
        let value = if source < 0 { i2_epilogue_read(history, 3 * channel + source + 3) }
                    else { i2_epilogue_read(input, source * 2304 + channel) };
        sum += value * i2_epilogue_read(weights, 4 * channel + tap);
    }
    i2_epilogue_write(output, offset, (i2_epilogue_read(input, offset) + sum) * 0.4472135901451111);
}

export fn inkling_postconv_commit(input: u64, history: u64, rows: i64, group: i64, lane: i64) {
    let channel = group * 128 + lane;
    let offset = 3 * channel;
    for tap in 3 {
        let source = rows + tap - 3;
        let value = if source < 0 { i2_epilogue_read(history, offset + source + 3) }
                    else { i2_epilogue_read(input, source * 2304 + channel) };
        i2_epilogue_write(history, offset + tap, value);
    }
}

export fn inkling_moe_epilogue(input: u64, residual: u64, weights: u64, history: u64, output: u64,
    rows: i64, group: i64, lane: i64) {
    let channel = group * 128 + lane;
    let h = 3 * channel;
    let w = 4 * channel;
    var h0 = i2_epilogue_read(history, h);
    var h1 = i2_epilogue_read(history, h + 1);
    var h2 = i2_epilogue_read(history, h + 2);
    for row in rows {
        let i = row * 2304 + channel;
        let current = i2_epilogue_read(input, i);
        var sum: f32 = 0.0;
        sum += h0 * i2_epilogue_read(weights, w);
        sum += h1 * i2_epilogue_read(weights, w + 1);
        sum += h2 * i2_epilogue_read(weights, w + 2);
        sum += current * i2_epilogue_read(weights, w + 3);
        i2_epilogue_write(output, i, (current + sum) * 0.4472135901451111 + i2_epilogue_read(residual, i));
        h0 = h1;
        h1 = h2;
        h2 = current;
    }
    i2_epilogue_write(history, h, h0);
    i2_epilogue_write(history, h + 1, h1);
    i2_epilogue_write(history, h + 2, h2);
}

export fn inkling_attention_gate(output: u64, gate: u64, count: u32, group: u32, lane: u32) {
    let i = group * 256 + lane;
    if i < count {
        let index = i as i64;
        let sigmoid = 1.0 / (1.0 + i2_epilogue_exp(-i2_epilogue_read(gate, index)));
        i2_epilogue_write(output, index, i2_epilogue_read(output, index) * sigmoid);
    }
}

export fn inkling_moe_situ(gate_up: u64, output: u64, elements: u32, reference: u32, group: u32, lane: u32) {
    let index = 256 * group + lane;
    if index >= elements { return; }
    let source = (index + 1152 * (index / 1152)) as i64;
    let gate = i2_epilogue_read(gate_up, source);
    let up = i2_epilogue_read(gate_up, source + 1152);
    let gate_clip = i2_epilogue_tanh(gate * 0.25 + 0.0) * 4.0 + 0.0;
    let up_clip = i2_epilogue_tanh(up * 0.03999999910593033 + 0.0) * 25.0 + 0.0;
    let denominator = 1.0 + i2_epilogue_exp_approx(-gate);
    let sigmoid = if reference != 0 { i2_epilogue_rcp_approx(denominator) } else { 1.0 / denominator };
    i2_epilogue_write(output, index as i64, (gate_clip * sigmoid) * up_clip);
}

export fn inkling_tanh(source: u64, output: u64, count: u32, group: u32, lane: u32) {
    let i = group * 256 + lane;
    if i < count {
        let index = i as i64;
        i2_epilogue_write(output, index, i2_epilogue_tanh(i2_epilogue_read(source, index)));
    }
}
