extern "i2m_load_u8" fn load_byte(address: u64) -> u32;
extern "i2m_load_i8" fn load_signed_byte(address: u64) -> i32;
extern "i2m_load_u16" fn load_word(address: u64) -> u32;
extern "inkling_half_g" fn load_half(address: u64) -> f32;
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

export fn i2_matrix_xs_blocks(weights: u64, input: u64, output: u64, blocks: i64, lane: u64) {
    var sum: f32 = 0.0;
    let lane_byte = lane & 15;
    let shift = 4 * (lane >> 4);
    for tile in blocks {
        let at = weights + (tile as u64) * 136;
        let scale = load_half(at);
        let high_bits = load_word(at + 2);
        let low01 = load_word(at + 4);
        let low23 = load_word(at + 6);
        for sub in 8 {
            let low_bits = if sub < 4 { low01 } else { low23 };
            let low = (low_bits >> (4 * (sub & 3))) & 15;
            let high = (high_bits >> (2 * sub)) & 3;
            let adjusted = scale * (((low + 16 * high) as i32 - 32) as f32);
            let packed = load_byte(at + 8 + (sub as u64) * 16 + lane_byte);
            let value = adjusted * iq4_code((packed >> shift) & 15);
            let position = 4 * ((tile as u64) * 256 + (sub as u64) * 32 + lane);
            sum = multiply_add(value, load(input + position), sum);
        }
    }
    sum = reduce_lane(sum);
    if lane == 0 { store(output, sum); }
}

fn i2_matrix_xs_pair(weights: u64, input: u64, second_input: u64, output: u64, second_output: u64, blocks: i64, lane: u64) {
    var sum: f32 = 0.0;
    var second: f32 = 0.0;
    let lane_byte = lane & 15;
    let shift = 4 * (lane >> 4);
    for tile in blocks {
        let at = weights + (tile as u64) * 136;
        let scale = load_half(at);
        let high_bits = load_word(at + 2);
        let low01 = load_word(at + 4);
        let low23 = load_word(at + 6);
        for sub in 8 {
            let low_bits = if sub < 4 { low01 } else { low23 };
            let low = (low_bits >> (4 * (sub & 3))) & 15;
            let high = (high_bits >> (2 * sub)) & 3;
            let adjusted = scale * (((low + 16 * high) as i32 - 32) as f32);
            let packed = load_byte(at + 8 + (sub as u64) * 16 + lane_byte);
            let value = adjusted * iq4_code((packed >> shift) & 15);
            let position = 4 * ((tile as u64) * 256 + (sub as u64) * 32 + lane);
            sum = multiply_add(value, load(input + position), sum);
            second = multiply_add(value, load(second_input + position), second);
        }
    }
    sum = reduce_lane(sum);
    second = reduce_lane(second);
    if lane == 0 {
        store(output, sum);
        store(second_output, second);
    }
}

extern "i2m_job_word" fn job_word(base: u64, index: i64) -> u64;

fn matrix_plane(jobs: u64, index: i64, words: i64) -> u64 {
    return if words == 3 { job_word(jobs, index + 2) } else { 0 };
}

export fn matrix_launch(weights: u64, input: u64, output: u64, jobs: u64, stride: u64, width: i64,
    rows: i64, count: i64, base: i64, words: i64, kind: i64, row: i64, first: i64, lane: u64, tile: i64) {
    if row >= rows { return; }
    let groups = width / 32;
    let first_job = words * (base + first);
    let plane = matrix_plane(jobs, first_job, words);
    let weight = weights + (row as! u64) * stride;
    let target = output + ((4 * row) as! u64);
    if kind == 8 && words == 2 && tile == 8 && first + 7 < count {
        i2_matrix_eight(weight,
            input + job_word(jobs, first_job), input + job_word(jobs, first_job + 2),
            input + job_word(jobs, first_job + 4), input + job_word(jobs, first_job + 6),
            input + job_word(jobs, first_job + 8), input + job_word(jobs, first_job + 10),
            input + job_word(jobs, first_job + 12), input + job_word(jobs, first_job + 14),
            target + job_word(jobs, first_job + 1), target + job_word(jobs, first_job + 3),
            target + job_word(jobs, first_job + 5), target + job_word(jobs, first_job + 7),
            target + job_word(jobs, first_job + 9), target + job_word(jobs, first_job + 11),
            target + job_word(jobs, first_job + 13), target + job_word(jobs, first_job + 15), groups, kind, lane);
        return;
    }
    if tile == 4 && first + 3 < count && matrix_plane(jobs, first_job + words, words) == plane &&
        matrix_plane(jobs, first_job + 2 * words, words) == plane && matrix_plane(jobs, first_job + 3 * words, words) == plane {
        i2_matrix_four(weight + plane, input + job_word(jobs, first_job),
            input + job_word(jobs, first_job + words), input + job_word(jobs, first_job + 2 * words),
            input + job_word(jobs, first_job + 3 * words), target + job_word(jobs, first_job + 1),
            target + job_word(jobs, first_job + words + 1), target + job_word(jobs, first_job + 2 * words + 1),
            target + job_word(jobs, first_job + 3 * words + 1), groups, kind, lane);
        return;
    }
    var route = first;
    var job = first_job;
    while route < count && route < first + tile {
        i2_matrix_one(weight + matrix_plane(jobs, job, words), input + job_word(jobs, job),
            target + job_word(jobs, job + 1), groups, kind, lane);
        route += 1;
        job += words;
    }
}

export fn matrix_launch_xs(weights: u64, input: u64, output: u64, jobs: u64, stride: u64,
    width: i64, rows: i64, count: i64, base: i64, words: i64, group: i64, route: i64, thread: i64) {
    let row = 4 * group + thread / 32;
    if row >= rows || route >= count { return; }
    let job = words * (base + route);
    let weight = weights + (row as! u64) * stride + matrix_plane(jobs, job, words);
    i2_matrix_xs_blocks(weight, input + job_word(jobs, job),
        output + job_word(jobs, job + 1) + ((4 * row) as! u64), width / 256, (thread % 32) as! u64);
}

export fn matrix_launch_xs_pair(weights: u64, input: u64, output: u64, jobs: u64, stride: u64,
    width: i64, rows: i64, count: i64, base: i64, words: i64, group: i64, pair: i64, thread: i64) {
    let row = 4 * group + thread / 32;
    let route = 2 * pair;
    if row >= rows || route >= count { return; }
    let lane = (thread % 32) as! u64;
    let job = words * (base + route);
    let plane = matrix_plane(jobs, job, words);
    let row_weight = weights + (row as! u64) * stride;
    let row_output = output + ((4 * row) as! u64);
    let first_input = input + job_word(jobs, job);
    let first_output = row_output + job_word(jobs, job + 1);
    let blocks = width / 256;
    if route + 1 < count {
        let next_job = job + words;
        let next_plane = matrix_plane(jobs, next_job, words);
        let next_input = input + job_word(jobs, next_job);
        let next_output = row_output + job_word(jobs, next_job + 1);
        if next_plane == plane {
            i2_matrix_xs_pair(row_weight + plane, first_input, next_input, first_output, next_output, blocks, lane);
            return;
        }
        i2_matrix_xs_blocks(row_weight + next_plane, next_input, next_output, blocks, lane);
    }
    i2_matrix_xs_blocks(row_weight + plane, first_input, first_output, blocks, lane);
}

export fn matrix_launch_projections(input_q: u64, input_k: u64, input_v: u64, output: u64,
    weight_q: u64, weight_k: u64, weight_v: u64, weight_r: u64, weight_sink: u64,
    weight_gate: u64, weight_key_taps: u64, weight_value_taps: u64, group: i64, thread: i64) {
    let index = group * 4 + thread / 32;
    var row = index;
    var weight = weight_q;
    var input = input_q;
    if index >= 8632 { row -= 8632; weight = weight_value_taps; input = input_v; }
    else if index >= 8600 { row -= 8600; weight = weight_key_taps; input = input_k; }
    else if index >= 5528 { row -= 5528; weight = weight_gate; }
    else if index >= 5504 { row -= 5504; weight = weight_sink; }
    else if index >= 5120 { row -= 5120; weight = weight_r; }
    else if index >= 4096 { row -= 4096; weight = weight_v; input = input_v; }
    else if index >= 3072 { row -= 3072; weight = weight_k; input = input_k; }
    i2_matrix_one(weight + ((row * 2448) as! u64), input, output + ((index * 4) as! u64), 72, 8, (thread % 32) as! u64);
}

export fn matrix_entry(weights: u64, input: u64, output: u64, jobs: u64, stride: u64, width: i64,
    rows: i64, count: i64, base: i64, words: i64, kind: i64, group_x: u32, group_y: u32,
    thread: u32, job_tile: i32) {
    let tile = job_tile as! u32;
    let row = 4 * group_x + (thread >> 5);
    let first = tile * group_y;
    matrix_launch(weights, input, output, jobs, stride, width, rows, count, base, words,
        kind, row as i64, first as i64, (thread & 31) as u64, tile as i64);
}
