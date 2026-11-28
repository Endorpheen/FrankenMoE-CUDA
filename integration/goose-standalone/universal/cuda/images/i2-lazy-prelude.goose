extern "lazy_load" fn load(base: u64, index: i64) -> f32;
extern "lazy_store" fn store(base: u64, index: i64, value: f32);
extern "lazy_plane" fn plane(base: u64, index: i64) -> u64;
extern "lazy_local_read" fn local_read(base: u64, index: i64) -> f32;
extern "lazy_local_write" fn local_write(base: u64, index: i64, value: f32);
extern "lazy_sum_read" fn sum_read(base: u64, index: i64) -> f64;
extern "lazy_sum_write" fn sum_write(base: u64, index: i64, value: f64);
extern "lazy_float_bits" fn float_bits(value: f32) -> u32;
extern "block_barrier" fn barrier();
extern "divide_rn" fn divide(a: f32, b: f32) -> f32;
extern "sqrt_rn" fn sqrt(value: f32) -> f32;
/*
 * Adapted from the range reduction and exp2 table of Arm optimized-routines.
 * Copyright (c) 2017-2026, Arm Limited.
 * SPDX-License-Identifier: MIT
 *
 * https://github.com/ARM-software/optimized-routines/blob/master/math/expf.c
 * https://github.com/ARM-software/optimized-routines/blob/master/math/exp2f_data.c
 */
extern fn depth_fma(a: f64, b: f64, c: f64) -> f64;
extern fn depth_bits(value: f64) -> u64;
extern fn depth_value(bits: u64) -> f64;

fn depth_exp2_word(index: u64) -> u64 {
    if index == 0 { return 0x3ff0000000000000; }
    if index == 1 { return 0x3fefd9b0d3158574; }
    if index == 2 { return 0x3fefb5586cf9890f; }
    if index == 3 { return 0x3fef9301d0125b51; }
    if index == 4 { return 0x3fef72b83c7d517b; }
    if index == 5 { return 0x3fef54873168b9aa; }
    if index == 6 { return 0x3fef387a6e756238; }
    if index == 7 { return 0x3fef1e9df51fdee1; }
    if index == 8 { return 0x3fef06fe0a31b715; }
    if index == 9 { return 0x3feef1a7373aa9cb; }
    if index == 10 { return 0x3feedea64c123422; }
    if index == 11 { return 0x3feece086061892d; }
    if index == 12 { return 0x3feebfdad5362a27; }
    if index == 13 { return 0x3feeb42b569d4f82; }
    if index == 14 { return 0x3feeab07dd485429; }
    if index == 15 { return 0x3feea47eb03a5585; }
    if index == 16 { return 0x3feea09e667f3bcd; }
    if index == 17 { return 0x3fee9f75e8ec5f74; }
    if index == 18 { return 0x3feea11473eb0187; }
    if index == 19 { return 0x3feea589994cce13; }
    if index == 20 { return 0x3feeace5422aa0db; }
    if index == 21 { return 0x3feeb737b0cdc5e5; }
    if index == 22 { return 0x3feec49182a3f090; }
    if index == 23 { return 0x3feed503b23e255d; }
    if index == 24 { return 0x3feee89f995ad3ad; }
    if index == 25 { return 0x3feeff76f2fb5e47; }
    if index == 26 { return 0x3fef199bdd85529c; }
    if index == 27 { return 0x3fef3720dcef9069; }
    if index == 28 { return 0x3fef5818dcfba487; }
    if index == 29 { return 0x3fef7c97337b9b5f; }
    if index == 30 { return 0x3fefa4afa2a490da; }
    return 0x3fefd0765b6e4540;
}

fn exp(value: f32) -> f32 {
    if value < -103.97207641601562 { return 0.0; }
    let x = value as f64;
    let multiplier: f64 = 46.16624130844683;
    let shift: f64 = 6755399441055744.0;
    let rounded = depth_fma(x, multiplier, shift);
    let encoded = depth_bits(rounded);
    let residue = depth_fma(x, multiplier, shift - rounded);
    let high = depth_fma(1.6938359250920212e-06, residue, 0.00023459809789509004);
    let low = depth_fma(0.021660849396613134, residue, 1.0);
    let scale = depth_value(depth_exp2_word(encoded & 31) + (encoded << 47));
    return (depth_fma(high, residue * residue, low) * scale) as f32;
}

fn ordered_result(products: u64, total: f64, magnitude: f64, divisor: f64) -> f32 {
    // Use the parallel sum only if the whole error interval rounds identically.
    let margin = magnitude / 549755813888.0;
    let lower = ((total - margin) / divisor) as! f32;
    let upper = ((total + margin) / divisor) as! f32;
    if float_bits(lower) == float_bits(upper) { return lower; }
    var exact_order: f64 = 0.0;
    for i in 2304 { exact_order += local_read(products, i) as! f64; }
    return (exact_order / divisor) as! f32;
}

fn sum_local(values: u64, sums: u64, magnitudes: u64, lane: i64) {
    var sum: f64 = 0.0;
    var magnitude: f64 = 0.0;
    var index = lane;
    while index < 2304 {
        let value = local_read(values, index) as! f64;
        sum += value;
        magnitude += if value < 0.0 { -value } else { value };
        index += 128;
    }
    sum_write(sums, lane, sum);
    sum_write(magnitudes, lane, magnitude);
    barrier();
    var stride: i64 = 64;
    while stride != 0 {
        if lane < stride {
            sum_write(sums, lane, sum_read(sums, lane) + sum_read(sums, lane + stride));
            sum_write(magnitudes, lane, sum_read(magnitudes, lane) + sum_read(magnitudes, lane + stride));
        }
        barrier();
        stride /= 2;
    }
}

export fn lazy_prepare(attention: u64, residual: u64, next: u64, normalized: u64, scales: u64,
    add_residual: bool, squares: u64, sums: u64, magnitudes: u64, inverse: u64, token: i64, lane: i64) {
    let base = token * 2304;
    var index = lane;
    while index < 2304 {
        var value = load(attention, base + index);
        if add_residual { value += load(residual, base + index); }
        store(next, base + index, value);
        local_write(squares, index, value * value);
        index += 128;
    }
    barrier();
    sum_local(squares, sums, magnitudes, lane);
    if lane == 0 {
        let mean = ordered_result(squares, sum_read(sums, 0), sum_read(magnitudes, 0), 2304.0);
        let factor = divide(1.0, sqrt(mean + 0.000001));
        local_write(inverse, 0, factor);
        store(scales, token, factor);
    }
    barrier();
    if normalized != 0 {
        let factor = local_read(inverse, 0);
        index = lane;
        while index < 2304 {
            store(normalized, base + index, load(next, base + index) * factor);
            index += 128;
        }
    }
}

fn history_value(planes: u64, scales: u64, token: i64, index: i64, lazy: i64, channel: i64, linear: bool) -> f32 {
    if linear { return load(planes, index * 2304 + channel); }
    let value = load(plane(planes, index), token * 2304 + channel);
    return if index == lazy { value * load(scales, token) } else { value };
}

export fn lazy_score(planes: u64, current: u64, query: u64, scores: u64, rows: i64, lazy: i64,
    scales: u64, products: u64, sums: u64, magnitudes: u64, token: i64, row: i64, lane: i64, linear: bool) {
    var index = lane;
    while index < 2304 {
        let value = if row == rows { load(current, token * 2304 + index) }
            else { history_value(planes, scales, token, rows - 1 - row, lazy, index, linear) };
        local_write(products, index, value * load(query, index));
        index += 128;
    }
    barrier();
    sum_local(products, sums, magnitudes, lane);
    if lane == 0 {
        let dot = ordered_result(products, sum_read(sums, 0), sum_read(magnitudes, 0), 1.0);
        store(scores, token * (rows + 1) + row, dot * divide(1.0, sqrt(2304.0)));
    }
}

export fn lazy_mix(planes: u64, current: u64, scores: u64, output: u64, rows: i64, lazy: i64,
    scales: u64, weights: u64, token: i64, group: i64, lane: i64, linear: bool) {
    if lane == 0 {
        let base = token * (rows + 1);
        var maximum: f32 = -3.4028234663852886e38;
        for row in rows + 1 {
            let score = load(scores, base + row);
            if score > maximum { maximum = score; }
        }
        var total: f64 = 0.0;
        for row in rows + 1 {
            let weight = exp(load(scores, base + row) - maximum);
            local_write(weights, row, weight);
            total += weight as! f64;
        }
        let inverse = divide(1.0, total as! f32);
        for row in rows + 1 { local_write(weights, row, local_read(weights, row) * inverse); }
    }
    barrier();
    let channel = group * 128 + lane;
    let index = token * 2304 + channel;
    var sum: f64 = 0.0;
    for row in rows {
        let term = local_read(weights, row) * history_value(planes, scales, token, rows - 1 - row, lazy, channel, linear);
        sum += term as! f64;
    }
    let term = local_read(weights, rows) * load(current, index);
    sum += term as! f64;
    store(output, index, sum as! f32);
}

export fn bridge_prepare(attention: u64, weights: u64, history: u64, residual: u64,
    next: u64, normalized: u64, add_residual: bool, squares: u64, sums: u64,
    magnitudes: u64, inverse: u64, lane: i64) {
    var index = lane;
    while index < 2304 {
        let h = 3 * index;
        let w = 4 * index;
        let current = load(attention, index);
        let h0 = load(history, h);
        let h1 = load(history, h + 1);
        let h2 = load(history, h + 2);
        var sum: f32 = 0.0;
        sum += h0 * load(weights, w);
        sum += h1 * load(weights, w + 1);
        sum += h2 * load(weights, w + 2);
        sum += current * load(weights, w + 3);
        var value = (current + sum) * 0.4472135901451111;
        if add_residual { value += load(residual, index); }
        store(history, h, h1);
        store(history, h + 1, h2);
        store(history, h + 2, current);
        store(next, index, value);
        local_write(squares, index, value * value);
        index += 128;
    }
    barrier();
    sum_local(squares, sums, magnitudes, lane);
    if lane == 0 {
        let mean = ordered_result(squares, sum_read(sums, 0), sum_read(magnitudes, 0), 2304.0);
        local_write(inverse, 0, divide(1.0, sqrt(mean + 0.000001)));
    }
    barrier();
    let factor = local_read(inverse, 0);
    index = lane;
    while index < 2304 {
        store(normalized, index, load(next, index) * factor);
        index += 128;
    }
}
