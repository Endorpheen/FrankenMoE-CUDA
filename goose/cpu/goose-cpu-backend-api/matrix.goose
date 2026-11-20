fn vector_dot(
    a: const Tensor &,
    x: const u8[:],
    ax: i64,
    as0: i64,
    b: const Tensor &,
    y: const u8[:],
    by: i64,
    bs0: i64,
    count: i64) -> f32 {
    assert(count >= 0);
    var acc: f32[8][4] = default < f32[8][4] > ();
    let end = (count / 32) * 32;
    var k: i64 = 0;

    while k < end {
        for group in 4 {
            for lane in 8 {
                let i = k + 8 * group + lane;
                let va = load_at(a, x, ax + i * as0);
                let vb = load_at(b, y, by + i * bs0);
                acc[group][lane] = if a.kind == 30 {
                    acc[group][lane] + va * vb
                } else {
                    fma32(va, vb, acc[group][lane])
                }
                ;
            }
        }
        k += 32;
    }
    let s = dot_lanes(acc);

    if a.kind == 0 {
        var result = (s[0] + s[1]) + (s[2] + s[3]);

        while k < count {
            result += load_at(a, x, ax + k * as0) * load_at(b, y, by + k * bs0);
            k += 1;
        }
        return result;
    }
    var result = if a.kind == 30 {
        ((s[0] + s[2]) + (s[1] + s[3])) as f64
    } else {
        ((s[0] + s[1]) + (s[2] + s[3])) as f64
    }
    ;

    while k < count {
        result += (load_at(a, x, ax + k * as0) * load_at(b, y, by + k * bs0)) as f64;
        k += 1;
    }
    return result as f32;
}

fn dot_lanes(acc: const f32[8][4] &) -> f32[4] {
    var v: f32[8] = default <f32[8]>();
    for lane in 8 {
        v[lane] = (acc[0][lane] + acc[2][lane]) + (acc[1][lane] + acc[3][lane]);
    }
    return [v[0] + v[4], v[1] + v[5], v[2] + v[6], v[3] + v[7]];
}

fn f32_row_dot(row: const f32[:], other: const f32[:], stride: i64) -> f32 {
    var acc: f32[8][4] = default <f32[8][4]>();
    let end = row.len / 32 * 32;
    var k: i64 = 0;
    while k < end {
        for group in 4 {
            for lane in 8 {
                let i = k + 8 * group + lane;
                acc[group][lane] = fma32(row[i], other[i * stride], acc[group][lane]);
            }
        }
        k += 32;
    }
    let s = dot_lanes(acc);
    var result = (s[0] + s[1]) + (s[2] + s[3]);
    while k < row.len {
        result += row[k] * other[k * stride];
        k += 1;
    }
    return result;
}

import.codec.decode;

extern "fg_cuda_begin" fn cuda_begin(input: u64, input_bytes: i64, output_bytes: i64) -> i64;
extern "fg_cuda_plane" fn cuda_plane(weights: u64, weight_bytes: i64, stride: i64,
                                    blocks: i64, kind: i64, columns: i64, jobs: const i64[:]) -> i64;
extern "fg_cuda_finish" fn cuda_finish(output: u64, output_bytes: i64) -> i64;

fn cuda_matrix_layout(p: const Node &) -> bool {
    return p.nth == 1 && (p.x.kind == 12 || p.x.kind == 14) &&
        p.y.kind == 0 && p.y.nb[0] == 4 && p.d.kind == 0 &&
        (p.y.nb[1] | p.y.nb[2] | p.y.nb[3]) % 4 == 0 &&
        p.x.nb[1] >= 0 && p.x.nb[1] % 2 == 0 &&
        p.d.nb[0] == 4 && p.d.nb[1] == mul_size(p.d.ne[0], 4) &&
        p.d.nb[2] == mul_size(p.d.nb[1], p.d.ne[1]) &&
        p.d.nb[3] == mul_size(p.d.nb[2], p.d.ne[2]) &&
        p.d.bytes == mul_size(p.d.nb[3], p.d.ne[3]);
}

fn matrix(p: const Node &, x: const u8[:], y: const u8[:], ids: const u8[:], dst: u8[:]) -> bool {
    if p.op != OP_MUL_MAT && p.op != OP_MUL_MAT_ID && p.op != OP_OUT_PROD {
        return false;
    }

    if p.y.kind != 0 && p.y.kind != 1 && p.y.kind != 30 {
        return false;
    }
    if p.x.kind != 0 && p.x.kind != 1 && p.x.kind != 30 {
        return false;
    }
    let d0 = p.d.ne[0];
    assert(d0 > 0 && p.d.kind == 0);
    var row = thread_first(p);

    while row < rows(p.d) {
        let i1 = row % p.d.ne[1];
        let i2 = (row / p.d.ne[1]) % p.d.ne[2];
        let i3 = row / (p.d.ne[1] * p.d.ne[2]);
        for i0 in d0 {
            var xa: i64 = 0;
            var ya: i64 = 0;
            var xs: i64 = 0;
            var ys: i64 = 0;
            var count: i64 = 0;

            if p.op == OP_MUL_MAT {
                assert(p.x.ne[0] == p.y.ne[0] && p.x.ne[1] == d0 && p.x.ne[2] > 0 && p.x.ne[3] > 0 && p.y.ne[2] % p.x.ne[2] == 0 && p.y.ne[3] % p.x.ne[3] == 0);
                let x2 = i2 / (p.y.ne[2] / p.x.ne[2]);
                let x3 = i3 / (p.y.ne[3] / p.x.ne[3]);
                xa = i0 * p.x.nb[1] + x2 * p.x.nb[2] + x3 * p.x.nb[3];
                ya = i1 * p.y.nb[1] + i2 * p.y.nb[2] + i3 * p.y.nb[3];
                xs = p.x.nb[0];
                ys = p.y.nb[0];
                count = p.x.ne[0];
            } else

            if p.op == OP_MUL_MAT_ID {
                assert(p.z.kind == 26 && p.x.ne[0] == p.y.ne[0] && p.x.ne[1] == d0 && p.y.ne[1] > 0);
                let id = read32(ids, offset(p.z, i1 + p.z.ne[0] * i2)) as !i32;
                assert(id >= 0 && (id as i64) < p.x.ne[2]);
                xa = i0 * p.x.nb[1] + (id as i64) * p.x.nb[2];
                ya = (i1 % p.y.ne[1]) * p.y.nb[1] + i2 * p.y.nb[2];
                xs = p.x.nb[0];
                ys = p.y.nb[0];
                count = p.x.ne[0];
            } else {
                assert(p.x.ne[1] == p.y.ne[1] && p.x.ne[0] == d0 && p.y.ne[0] == p.d.ne[1]);
                let x2 = i2 % p.x.ne[2];
                let x3 = i3 % p.x.ne[3];
                let y2 = i2 % p.y.ne[2];
                let y3 = i3 % p.y.ne[3];
                xa = i0 * p.x.nb[0] + x2 * p.x.nb[2] + x3 * p.x.nb[3];
                ya = i1 * p.y.nb[0] + y2 * p.y.nb[2] + y3 * p.y.nb[3];
                xs = p.x.nb[1];
                ys = p.y.nb[1];
                count = p.x.ne[1];
            }
            store(p.d, dst, row * d0 + i0, vector_dot(p.x, x, xa, xs, p.y, y, ya, ys, count));
        }
        row += p.nth;
    }
    return true;
}


fn quant_route(p: const Node &, route: i64, plane2: i64, plane3: i64,
               spread2: i64, spread3: i64) -> i64, i64 {
    if p.op == OP_MUL_MAT_ID {
        let slot = route % p.z.ne[0];
        let token = route / p.z.ne[0];
        return (slot % p.y.ne[1]) * p.y.nb[1] + token * p.y.nb[2], route;
    }
    let i1 = route % p.y.ne[1];
    let relative_plane = route / p.y.ne[1];
    let i2 = plane2 * spread2 + relative_plane % spread2;
    let i3 = plane3 * spread3 + relative_plane / spread2;
    return i1 * p.y.nb[1] + i2 * p.y.nb[2] + i3 * p.y.nb[3],
        i1 + p.d.ne[1] * (i2 + p.d.ne[2] * i3);
}

fn next_quant_route(indexed: bool, indices: const i64[:], next: i64, route: i64, count: i64) -> i64 {
    if indexed { return indices[next + route]; }
    return if route + 1 < count { route + 1 } else { -1 };
}

fn cuda_quant_planes(p: const Node &, weights: u64, weight_bytes: i64,
                     planes: i64, row_bytes: i64, row_count: i64,
                     spread2: i64, spread3: i64, heads: i64, next: i64,
                     routes: const i64[:], jobs: i64[:]) -> bool {
    let indexed = p.op == OP_MUL_MAT_ID;
    let span = add_size(mul_size(p.d.ne[0] - 1, p.x.nb[1]), row_bytes);
    for plane in planes {
        var route = if indexed { routes[heads + plane] } else { 0 };
        if route < 0 { continue; }
        let plane2 = plane % p.x.ne[2];
        let plane3 = plane / p.x.ne[2];
        let at = add_size(mul_size(plane2, p.x.nb[2]), mul_size(plane3, p.x.nb[3]));
        if at > weight_bytes || span > weight_bytes - at { return false; }
        var words: i64 = 0;
        while route >= 0 {
            let ya, dst_row = quant_route(p, route, plane2, plane3, spread2, spread3);
            jobs[words] = ya;
            jobs[words + 1] = dst_row * p.d.nb[1];
            words += 2;
            route = next_quant_route(indexed, routes, next, route, row_count);
        }
        if cuda_plane(weights + (at as u64), span, p.x.nb[1], p.x.ne[0] / 256,
                      p.x.kind, p.d.ne[0], jobs[0 .. words]) != 0 {
            return false;
        }
    }
    return true;
}

// Each worker owns output columns and decodes a weight row once for all
// input vectors using that row. Indexed multiplication groups only routes
// that exist, so weights of inactive experts are never touched.
fn quantized_matrix(p: const Node &, x: const u8[:], y: const u8[:], ids: const u8[:], dst: u8[:],
                    scratch: f32[:], indices: i64[:], addresses: const i64[3] &,
                    gxx: const u8[8][:], gxs: const u8[8][:], gs: const u8[8][:],
                    g3x: const u8[4][:], g3s: const u8[4][:], g1: const i8[8][:],
                    signs: const u8[:], palette: const i8[:]) -> bool {
    let indexed = p.op == OP_MUL_MAT_ID;
    if (p.op != OP_MUL_MAT && !indexed) || p.d.kind != 0 ||
       (p.y.kind != 0 && p.y.kind != 1 && p.y.kind != 30) {
        return false;
    }
    let width = p.x.ne[0];
    let kind = decode :: super_kind(p.x.kind);
    let block_width = if kind >= 0 { 256 } else { decode :: compact_width(p.x.kind) };
    let block_bytes = if kind >= 0 { decode :: tile_traits(kind).bytes } else { decode :: compact_bytes(p.x.kind) };
    if width <= 0 || block_bytes == 0 || width % block_width != 0 || p.x.nb[0] != block_bytes ||
       p.x.ne[0] != p.y.ne[0] || p.d.ne[0] != p.x.ne[1] || p.d.ne[0] <= 0 ||
       p.x.ne[2] <= 0 || p.x.ne[3] <= 0 || p.y.ne[1] <= 0 || p.y.ne[2] <= 0 || p.y.ne[3] <= 0 ||
       scratch.len < mul_size(width, p.nth) {
        return false;
    }

    validate(p.y, y.len);
    validate(p.d, dst.len);
    let row_bytes = mul_size(width / block_width, block_bytes);
    let planes = mul_size(p.x.ne[2], p.x.ne[3]);
    var spread2: i64 = 1;
    var spread3: i64 = 1;
    var heads: i64 = 0;
    var next: i64 = 0;
    var route_words: i64 = 0;

    if indexed {
        if p.z.kind != 26 || p.x.ne[3] != 1 || p.y.ne[3] != 1 ||
           p.z.ne[0] <= 0 || p.z.ne[1] != p.y.ne[2] || p.z.ne[2] != 1 || p.z.ne[3] != 1 ||
           p.z.ne[0] % p.y.ne[1] != 0 || p.d.ne[1] != p.z.ne[0] ||
           p.d.ne[2] != p.z.ne[1] || p.d.ne[3] != 1 {
            return false;
        }
        validate(p.z, ids.len);
        let routes = mul_size(p.z.ne[0], p.z.ne[1]);
        let words = add_size(planes, routes);
        route_words = mul_size(words, p.nth);
        if indices.len < route_words { return false; }
        heads = p.ith * words;
        next = heads + planes;
        for expert in planes { indices[heads + expert] = -1; }
        for route in routes {
            let expert = (read32(ids, offset(p.z, route)) as! i32) as i64;
            if expert < 0 || expert >= planes { return false; }
            indices[next + route] = indices[heads + expert];
            indices[heads + expert] = route;
        }
    } else {
        if p.y.ne[2] % p.x.ne[2] != 0 || p.y.ne[3] % p.x.ne[3] != 0 ||
           p.d.ne[1] != p.y.ne[1] || p.d.ne[2] != p.y.ne[2] || p.d.ne[3] != p.y.ne[3] {
            return false;
        }
        spread2 = p.y.ne[2] / p.x.ne[2];
        spread3 = p.y.ne[3] / p.x.ne[3];
    }

    let row_count = mul_size(mul_size(p.y.ne[1], spread2), spread3);
    if addresses[0] != 0 && addresses[1] != 0 && addresses[2] != 0 && cuda_matrix_layout(p) {
        let job_words = mul_size(2, rows(p.d));
        if indices.len - route_words < job_words || p.x.nb[2] < 0 || p.x.nb[3] < 0 { return false; }
        let status = cuda_begin(addresses[1] as! u64, y.len, dst.len);
        if status < 0 { return false; }
        if status > 0 {
            let ok = cuda_quant_planes(p, addresses[0] as! u64, x.len, planes, row_bytes, row_count,
                spread2, spread3, heads, next, indices[0 .. route_words],
                indices[route_words .. route_words + job_words]);
            let output = if ok { addresses[2] as! u64 } else { 0 };
            return cuda_finish(output, dst.len) == 0 && ok;
        }
    }
    let decoded = scratch[p.ith * width .. (p.ith + 1) * width];
    var decoder: decode :: WeightTile = default < decode :: WeightTile > ();
    for plane in planes {
        let first = if indexed { indices[heads + plane] } else { 0 };
        if first < 0 { continue; }
        let plane2 = plane % p.x.ne[2];
        let plane3 = plane / p.x.ne[2];
        let xb = plane2 * p.x.nb[2] + plane3 * p.x.nb[3];
        var col = p.ith;
        while col < p.d.ne[0] {
            let xa = xb + col * p.x.nb[1];
            if xa < 0 || xa > x.len || row_bytes > x.len - xa { return false; }
            if !decode :: row_entry_with_workspace(p.x.kind, x[xa .. xa + row_bytes], decoded,
                                                  gxx, gxs, gs, g3x, g3s, g1, signs, palette, decoder) {
                return false;
            }

            var route = first;
            while route >= 0 {
                let ya, dst_row = quant_route(p, route, plane2, plane3, spread2, spread3);
                var sum: f32 = 0.0;
                for j in width {
                    sum = fma32(decoded[j], load_at(p.y, y, ya + j * p.y.nb[0]), sum);
                }
                store(p.d, dst, dst_row * p.d.ne[0] + col, sum);
                route = next_quant_route(indexed, indices, next, route, row_count);
            }
            col += p.nth;
        }
    }
    return true;
}
