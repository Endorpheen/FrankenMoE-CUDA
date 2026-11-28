struct I2NativeAcc { x: f32, y: f32, z: f32, w: f32 }

extern "i2_native_pair" fn native_pair(address: u64, offset: i64) -> u32;
extern "i2_native_q4_pair" fn native_kv_pair(address: u64, first: i64, second: i64, q4: i32) -> u32;
extern "i2_native_shared_pair" fn native_shared_pair(address: u64, offset: i64) -> u32;
extern "i2_native_mma" fn native_mma(acc: I2NativeAcc, a0: u32, a1: u32, a2: u32, a3: u32, b0: u32, b1: u32) -> I2NativeAcc;
extern "sync_group" fn native_sync();
extern "i2_native_warp_sync" fn native_warp_sync();
extern "i2_native_shuffle" fn native_shuffle(value: f32, distance: i32) -> f32;
extern "i2_native_store" fn native_store(address: u64, index: i64, value: f32);

extern "i2_native_relative" fn native_relative(score: f32, query: i64, key: i64, window: i64,
    relative: u64, bias: u64, sink: f32, scale: f32) -> f32;
extern "i2_native_tiles_columns" fn native_tiles_columns(data: u64, element: i64) -> u32;
extern "i2_native_tiles_rows" fn native_tiles_rows(data: u64, a: i64, b: i64, end: i64, channel: i64) -> u32;

fn native_zero() -> I2NativeAcc { return I2NativeAcc {x:0.0,y:0.0,z:0.0,w:0.0}; }

fn native_q_fragment(q: u64, k: u64, query: i64, key: i64, dim: i64, tail: i64,
    group: i64, col: i64, end: i64, q4: i32, key_column: i64, packed: bool, acc: I2NativeAcc) -> I2NativeAcc {
    let a0 = if group < tail { native_pair(q, query + dim + col) } else { 0 };
    let a2 = if group < tail { native_pair(q, query + dim + col + 8) } else { 0 };
    let a1 = if group + 8 < tail { native_pair(q, query + 24576 + dim + col) } else { 0 };
    let a3 = if group + 8 < tail { native_pair(q, query + 24576 + dim + col + 8) } else { 0 };
    var b0: u32 = 0;
    var b1: u32 = 0;
    if key < end {
        let offset = key * 1024 + key_column + dim + col;
        if packed {
            b0 = native_tiles_columns(k, offset);
            b1 = native_tiles_columns(k, offset + 8);
        } else if q4 != 0 {
            b0 = native_kv_pair(k, offset, offset + 1, q4);
            b1 = native_kv_pair(k, offset + 8, offset + 9, q4);
        } else {
            b0 = native_pair(k, offset);
            b1 = native_pair(k, offset + 8);
        }
    }
    return native_mma(acc, a0, a1, a2, a3, b0, b1);
}

fn native_decode_softmax(scores: u64, probabilities: u64, maxima: u64, totals: u64, rescale: u64, thread: i64) {
    native_warp_sync();
    let before = shared_read(maxima, 0);
    let score = shared_read(scores, thread);
    var maximum = if score > before { score } else { before };
    var distance: i32 = 16;
    while distance > 0 {
        let other = native_shuffle(maximum, distance);
        if other > maximum { maximum = other; }
        distance = distance >> 1;
    }
    let value = exp(score - maximum);
    shared_write(scores, thread, value);
    probability_write(probabilities, thread, value);
    native_warp_sync();
    if thread == 0 {
        let alpha = exp(before - maximum);
        var total = shared_read(totals, 0) * alpha;
        for index in 32 { total += shared_read(scores, index); }
        shared_write(maxima, 0, maximum);
        shared_write(totals, 0, total);
        shared_write(rescale, 0, alpha);
    }
}

export fn i2_native_flash(q: u64, k: u64, v: u64, relative: u64, bias: u64, sinks: u64,
    key_pos: u64, query_pos: u64, partials: u64, metadata: u64, scores: u64, maxima: u64, totals: u64, rescale: u64,
    probabilities: u64, keys: i64, queries: i64, parts: i64, window: i64, scale: f32,
    flags: i32, query_tile: i64, head: i64, part: i64, thread: i64, decode: bool, prefill: bool, packed: bool) {
    let warp = thread / 32;
    let lane = thread % 32;
    let group = lane / 4;
    let col = 2 * (lane % 4);
    let query = 16 * query_tile;
    let begin = part * 2048;
    let end = if keys < begin + 2048 { keys } else { begin + 2048 };
    let tail = if decode { 1 } else { if queries - query < 16 { queries - query } else { 16 } };
    let qhead = query * 24 + head;
    let parallel_scores = !prefill && (flags & 2) != 0 && queries == 1;
    let q4 = if decode || prefill { 1 } else { flags & 1 };
    let group_column = head / 3 * 128;
    var acc: I2NativeAcc[4] = [native_zero(); 4];
    if thread < 16 {
        shared_write(maxima, thread, 0.0);
        shared_write(totals, thread, 0.0);
        if decode { shared_write(rescale, thread, 0.0); }
    }
    if decode {
        var index = thread + 32;
        while index < 512 {
            probability_write(probabilities, index, 0.0);
            index += 128;
        }
    }
    native_sync();
    let earliest_query = if packed { position(query_pos, query) } else { 0 };
    let latest_query = if packed { position(query_pos, query + tail - 1) } else { 0 };
    var start = begin;
    while start < end {
        let missing = if end - start < 32 { end - start } else { 32 };
        var no_relative = false;
        if packed {
            let first_key = position(key_pos, start);
            let last_key = position(key_pos, start + missing - 1);
            if first_key > latest_query || (window != 0 && earliest_query - last_key >= window) {
                start += 32;
                continue;
            }
            no_relative = window == 0 && earliest_query - last_key >= 1024 && missing == 32;
        }
        let key = start + warp * 8 + group;
        let query_address = (qhead + group * 24) * 128;
        var qk = native_zero();
        for dimension in 8 {
            qk = native_q_fragment(q, k, query_address, key, dimension * 16, tail, group, col, end,
                q4, group_column, packed, qk);
        }
        if no_relative {
            let sink0 = if group < tail { load(sinks, qhead + group * 24) } else { 0.0 };
            let sink1 = if group + 8 < tail { load(sinks, qhead + (group + 8) * 24) } else { 0.0 };
            qk = I2NativeAcc {x:scale*qk.x+(0.0-sink0),y:scale*qk.y+(0.0-sink0),
                z:scale*qk.z+(0.0-sink1),w:scale*qk.w+(0.0-sink1)};
        }
        let index = group * 32 + warp * 8 + col;
        shared_write(scores, index, qk.x);
        shared_write(scores, index + 1, qk.y);
        shared_write(scores, index + 256, qk.z);
        shared_write(scores, index + 257, qk.w);
        native_sync();
        if decode || parallel_scores {
            if thread < 32 {
                i2_online_adjust_key(scores, key_pos, relative, bias, sinks, missing, start,
                    position(query_pos, query), qhead, thread, window, scale);
                if decode { native_decode_softmax(scores, probabilities, maxima, totals, rescale, thread); }
            }
            if !decode { native_sync(); }
        }
        if packed {
            native_tiles_softmax(scores, probabilities, maxima, totals, rescale, relative, bias, sinks,
                key_pos, query_pos, query, qhead, missing, start, tail, window, scale, thread, no_relative);
        } else if prefill {
            var at = thread;
            while at < tail * 32 {
                let row = at / 32;
                i2_online_adjust_key(scores + ((row * 128) as u64), key_pos, relative, bias, sinks, missing, start,
                    position(query_pos, query + row), qhead + row * 24, at % 32, window, scale);
                at += 128;
            }
            native_sync();
        }
        if !packed && !decode && thread < 16 {
            if thread < tail {
                if prefill { i2_online_prepared_row_at(scores, probabilities, maxima, totals, rescale, thread); }
                else if parallel_scores { i2_online_prepared_row(scores, probabilities, maxima, totals, rescale); }
                else {
                    i2_online_row(scores, probabilities, maxima, totals, rescale, key_pos, relative, bias, sinks,
                        missing, start, position(query_pos, query + thread), qhead + thread * 24, thread, window, scale);
                }
            } else {
                shared_write(rescale, thread, 0.0);
                for column in 32 { probability_write(probabilities, thread * 32 + column, 0.0); }
            }
        }
        if !packed { native_sync(); }
        let w0 = shared_read(rescale, group);
        let w1 = shared_read(rescale, group + 8);
        for index in 4 {
            let value = acc[index];
            acc[index] = I2NativeAcc {x:value.x*w0,y:value.y*w0,z:value.z*w1,w:value.w*w1};
        }
        for tile in 2 {
            let row = group * 32 + tile * 16 + col;
            let a0 = native_shared_pair(probabilities, row);
            let a1 = native_shared_pair(probabilities, row + 256);
            let a2 = native_shared_pair(probabilities, row + 8);
            let a3 = native_shared_pair(probabilities, row + 264);
            let first = start + tile * 16 + col;
            for index in 4 {
                let column = group_column + 32 * warp + 8 * index + group;
                let k0 = first;
                let k1 = first + 1;
                let k2 = first + 8;
                let k3 = first + 9;
                let b0 = if packed { native_tiles_rows(v, k0, k1, end, column) } else { native_value_pair(v, k0, k1, column, end, q4) };
                let b1 = if packed { native_tiles_rows(v, k2, k3, end, column) } else { native_value_pair(v, k2, k3, column, end, q4) };
                acc[index] = native_mma(acc[index], a0, a1, a2, a3, b0, b1);
            }
        }
        native_sync();
        start += 32;
    }
    let slot = ((qhead + group * 24) * parts + part) * 128;
    let row_stride = 24 * parts * 128;
    for index in 4 {
        let column = warp * 32 + index * 8 + col;
        let value = acc[index];
        if group < tail {
            native_store(partials, slot + column, value.x);
            native_store(partials, slot + column + 1, value.y);
        }
        if group + 8 < tail {
            native_store(partials, slot + 8 * row_stride + column, value.z);
            native_store(partials, slot + 8 * row_stride + column + 1, value.w);
        }
    }
    if thread < tail {
        let offset = ((qhead + thread * 24) * parts + part) * 2;
        native_store(metadata, offset, shared_read(maxima, thread));
        native_store(metadata, offset + 1, shared_read(totals, thread));
    }
}

extern "i2_native_load_half" fn native_load_half(address: u64, offset: i64, q4: i32) -> u32;

fn native_value_pair(v: u64, first: i64, second: i64, column: i64, end: i64, q4: i32) -> u32 {
    let low = if first < end { native_load_half(v, first * 1024 + column, q4) } else { 0 };
    let high = if second < end { native_load_half(v, second * 1024 + column, q4) } else { 0 };
    return low | (high << 16);
}

fn native_tiles_softmax(scores: u64, probabilities: u64, maxima: u64, totals: u64, rescale: u64,
    relative: u64, bias: u64, sinks: u64, key_pos: u64, query_pos: u64,
    query: i64, qhead: i64, missing: i64, start: i64, tail: i64, window: i64, scale: f32,
    thread: i64, no_relative: bool) {
    var at = thread;
    if !no_relative {
        while at < tail * 32 {
            let row = at / 32;
            let lane = at % 32;
            let head = qhead + row * 24;
            let score = if lane < missing {
                native_relative(shared_read(scores, at), position(query_pos, query + row), position(key_pos, start + lane),
                    window, relative + ((head * 64) as u64), bias, load(sinks, head), scale)
            } else { neg_inf() };
            shared_write(scores, at, score);
            at += 128;
        }
        native_sync();
    }
    if thread < 16 {
        if thread < tail {
            let before = shared_read(maxima, thread);
            var maximum = before;
            for index in 32 {
                let score = shared_read(scores, thread * 32 + index);
                if score > maximum { maximum = score; }
            }
            let alpha = exp(before - maximum);
            shared_write(rescale, thread, alpha);
            shared_write(maxima, thread, maximum);
            shared_write(totals, thread, shared_read(totals, thread) * alpha);
        } else { shared_write(rescale, thread, 0.0); }
    }
    native_sync();
    at = thread;
    while at < 512 {
        let row = at / 32;
        let weight = if row < tail { exp(shared_read(scores, at) - shared_read(maxima, row)) } else { 0.0 };
        shared_write(scores, at, weight);
        probability_write(probabilities, at, weight);
        at += 128;
    }
    native_sync();
    if thread < tail {
        var total = shared_read(totals, thread);
        for index in 32 { total += shared_read(scores, thread * 32 + index); }
        shared_write(totals, thread, total);
    }
}
