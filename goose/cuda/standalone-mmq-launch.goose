export fn mmq_launch_reference(weights: u64, packed: u64, output: u64, jobs: u64, tiles: u64,
    stride: i64, extent: i64, kind: i64, rows: i64, first: i64, first_route: i64, words: i64,
    input_stride: i64, columns: i64, sms: i64, group: i64, tile: i64, thread: i64) {
    let row = 64 * group + 16 * (thread / 32);
    if row >= rows { return; }
    let at = (first + tile) * 3;
    let route = mmq_index(tiles, at) as i64;
    let count = mmq_index(tiles, at + 1) as i64;
    let lane = thread % 32;
    let column = 2 * (lane % 4);
    let out0 = if column < count { mmq_index(jobs, (route + column) * words + 1) } else { 0 };
    let out1 = if column + 1 < count { mmq_index(jobs, (route + column + 1) * words + 1) } else { 0 };
    let q = if input_stride == 0 { packed + (((route - first_route) * 288 * extent) as u64) } else { packed };
    fg_device_mmq(weights + mmq_index(tiles, at + 2), q, output, stride, extent, kind,
        row, rows, count, lane, out0, out1, 0, extent, 0, jobs, route, words, input_stride, columns, sms);
}

export fn mmq_launch_dense_shared(weights: u64, packed: u64, output: u64, jobs: u64,
    stride: i64, extent: i64, kind: i64, rows: i64, first: i64, sms: i64,
    row: i64, column: i64, thread: i64, shared: u64, width: i64) {
    let route = first + width * column;
    if width >= 64 {
        fg_device_mmq_wide(weights, packed, output, jobs, stride, extent, kind,
            16 * row, rows, route, first, width, 2, 0, sms, thread, shared, 2 * width);
    } else {
        fg_device_mmq_dense_shared(weights, packed, output, jobs, stride, extent, kind,
            16 * row, rows, route, first, sms, thread, shared);
    }
}

export fn mmq_launch_routed_shared(weights: u64, packed: u64, output: u64, jobs: u64, tiles: u64,
    stride: i64, extent: i64, kind: i64, rows: i64, first: i64, first_route: i64, input_stride: i64,
    row: i64, tile: i64, thread: i64, shared: u64, width: i64) {
    let at = (first + tile) * 3;
    let plane = weights + mmq_index(tiles, at + 2);
    let route = mmq_index(tiles, at) as i64;
    let routes = mmq_index(tiles, at + 1) as i64;
    if width >= 64 {
        fg_device_mmq_wide(plane, packed, output, jobs, stride, extent, kind, 16 * row, rows,
            route, first_route, routes, 3, input_stride, 0, thread, shared, 2 * width);
    } else {
        fg_device_mmq_routed_shared(plane, packed, output, jobs, stride, extent, kind,
            16 * row, rows, route, first_route, routes, input_stride, thread, shared);
    }
}

export fn mmq_launch_q4_routed(weights: u64, packed: u64, output: u64, jobs: u64, tiles: u64,
    stride: i64, extent: i64, first: i64, first_route: i64, input_stride: i64,
    row: i64, tile: i64, thread: i64, shared: u64, fused: bool) {
    let at = (first + tile) * 3;
    fg_device_mmq_q4_routed_m128(weights + mmq_index(tiles, at + 2), packed, output, jobs,
        stride, extent, 128 * row, mmq_index(tiles, at) as i64, first_route,
        mmq_index(tiles, at + 1) as i64, thread, shared, input_stride, fused);
}

export fn mmq_launch_q4_large(weights: u64, packed: u64, output: u64, jobs: u64,
    stride: i64, extent: i64, first: i64, routes: i64, row: i64, tile: i64, thread: i64,
    shared: u64, height: i64, width: i64, prepared: bool, ahead: bool) {
    let start = width * tile;
    let left = routes - start;
    fg_device_mmq_q4_large(weights, packed, output, jobs, stride, extent,
        height * row, first + start, first, if left < width { left } else { width },
        thread, shared, height, width, prepared, ahead);
}

export fn mmq_launch_q6_dense(weights: u64, packed: u64, output: u64, jobs: u64,
    stride: i64, extent: i64, first: i64, routes: i64, row: i64, tile: i64, thread: i64,
    shared: u64, ahead: bool) {
    let start = 128 * tile;
    let left = routes - start;
    fg_device_mmq_q6_half_k(weights, packed, output, jobs, stride, extent,
        128 * row, first + start, first, if left < 128 { left } else { 128 }, routes, thread, shared, ahead, 0);
}

export fn mmq_launch_q6_routed(weights: u64, packed: u64, output: u64, jobs: u64, tiles: u64,
    stride: i64, extent: i64, first: i64, first_route: i64, input_stride: i64,
    row: i64, tile: i64, thread: i64, shared: u64) {
    let at = (first + tile) * 3;
    fg_device_mmq_q6_half_k(weights + mmq_index(tiles, at + 2), packed, output, jobs,
        stride, extent, 128 * row, mmq_index(tiles, at) as i64, first_route,
        mmq_index(tiles, at + 1) as i64, 0, thread, shared, true, input_stride);
}

export fn mmq_launch_q4_cooperative(weights: u64, packed: u64, output: u64, jobs: u64,
    stride: i64, extent: i64, first: i64, routes: i64, row: i64, tile: i64, thread: i64,
    shared: u64, height: i64) {
    let start = 128 * tile;
    let left = routes - start;
    let count = if left < 128 { left } else { 128 };
    if height == 64 {
        fg_device_mmq_q4_m64(weights, packed, output, jobs, stride, extent, 64 * row,
            first + start, first, count, thread, shared);
    } else {
        fg_device_mmq_q4_m128(weights, packed, output, jobs, stride, extent, 128 * row,
            first + start, first, count, thread, shared);
    }
}
