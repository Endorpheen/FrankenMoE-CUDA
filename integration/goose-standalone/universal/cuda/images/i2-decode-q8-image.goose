extern "i2m_load_u8" fn load_byte(address: u64) -> u32;
extern "i2m_load_i8" fn load_signed_byte(address: u64) -> i32;
extern "i2m_load_u16" fn load_word(address: u64) -> u32;
extern "i2m_load_half" fn load_half(address: u64) -> f32;
extern "i2m_load" fn load(address: u64) -> f32;
extern "i2m_store" fn store(address: u64, value: f32);
extern "i2m_fma" fn multiply_add(a: f32, b: f32, sum: f32) -> f32;
extern "i2m_shuffle" fn shuffle(value: f32, distance: i32) -> f32;

fn iq4_code(code: u32) -> f32 {
    let bank: u64 = if code < 8 { 0xf6eaddcfbfad9881 as u64 } else { 0x7159453526190d01 as u64 };
    let byte = (bank >> ((code & 7) * 8)) & 255 as u64;
    let value = byte as i32;
    return (if value < 128 { value } else { value - 256 }) as f32;
}

fn quant_value(weights: u64, group: u64, lane: u64, kind: i64) -> f32 {
    if kind == 8 {
        let at = weights + group * 34;
        return load_half(at) * (load_signed_byte(at + 2 + lane) as f32);
    }
    let at = weights + (if kind == 23 { (group >> 3) * 136 } else { group * 18 });
    var scale = load_half(at);
    var fields = at + 2;
    if kind == 23 {
        let sub = group & 7;
        let low = (load_byte(at + 4 + (sub >> 1)) >> (4 * (sub & 1))) & 15;
        let high = (load_word(at + 2) >> (2 * sub)) & 3;
        scale = scale * (((low + 16 * high) as i32 - 32) as f32);
        fields = at + 8 + 16 * sub;
    }
    let packed = load_byte(fields + (lane & 15));
    return scale * iq4_code((packed >> (4 * (lane >> 4))) & 15);
}

fn reduce_lane(sum: f32) -> f32 {
    var value = sum;
    value += shuffle(value, 16);
    value += shuffle(value, 8);
    value += shuffle(value, 4);
    value += shuffle(value, 2);
    value += shuffle(value, 1);
    return value;
}

export fn i2_matrix_one(weights: u64, input: u64, output: u64, groups: i64,
                        kind: i64, lane: u64) {
    var sum: f32 = 0.0;
    for group in groups {
        let at = 4 * (32 * (group as u64) + lane);
        let value = quant_value(weights, group as u64, lane, kind);
        sum = multiply_add(value, load(input + at), sum);
    }
    sum = reduce_lane(sum);
    if lane == 0 {
        store(output, sum);
    }
}

export fn i2_matrix_four(weights: u64, input0: u64, input1: u64, input2: u64, input3: u64,
                         output0: u64, output1: u64, output2: u64, output3: u64,
                         groups: i64, kind: i64, lane: u64) {
    var a: f32 = 0.0;
    var b: f32 = 0.0;
    var c: f32 = 0.0;
    var d: f32 = 0.0;
    for group in groups {
        let at = 4 * (32 * (group as u64) + lane);
        let value = quant_value(weights, group as u64, lane, kind);
        a = multiply_add(value, load(input0 + at), a);
        b = multiply_add(value, load(input1 + at), b);
        c = multiply_add(value, load(input2 + at), c);
        d = multiply_add(value, load(input3 + at), d);
    }
    a = reduce_lane(a);
    b = reduce_lane(b);
    c = reduce_lane(c);
    d = reduce_lane(d);
    if lane == 0 {
        store(output0, a);
        store(output1, b);
        store(output2, c);
        store(output3, d);
    }
}

export fn i2_matrix_eight(weights: u64, input0: u64, input1: u64, input2: u64, input3: u64, input4: u64, input5: u64, input6: u64, input7: u64,
    output0: u64, output1: u64, output2: u64, output3: u64, output4: u64, output5: u64, output6: u64, output7: u64,
    groups: i64, kind: i64, lane: u64) {
    var a: f32 = 0.0;
    var b: f32 = 0.0;
    var c: f32 = 0.0;
    var d: f32 = 0.0;
    var e: f32 = 0.0;
    var f: f32 = 0.0;
    var g: f32 = 0.0;
    var h: f32 = 0.0;
    for group in groups {
        let at = 4 * (32 * (group as u64) + lane);
        let value = quant_value(weights, group as u64, lane, kind);
        a = multiply_add(value, load(input0 + at), a);
        b = multiply_add(value, load(input1 + at), b);
        c = multiply_add(value, load(input2 + at), c);
        d = multiply_add(value, load(input3 + at), d);
        e = multiply_add(value, load(input4 + at), e);
        f = multiply_add(value, load(input5 + at), f);
        g = multiply_add(value, load(input6 + at), g);
        h = multiply_add(value, load(input7 + at), h);
    }
    a = reduce_lane(a);
    b = reduce_lane(b);
    c = reduce_lane(c);
    d = reduce_lane(d);
    e = reduce_lane(e);
    f = reduce_lane(f);
    g = reduce_lane(g);
    h = reduce_lane(h);
    if lane == 0 {
        store(output0, a);
        store(output1, b);
        store(output2, c);
        store(output3, d);
        store(output4, e);
        store(output5, f);
        store(output6, g);
        store(output7, h);
    }
}
