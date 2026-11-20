fn flash_value_type(t: const Tensor &) -> bool {
    return t.kind == 0 || t.kind == 1 || t.kind == 30 ||
        (t.kind == 2 && t.ne[0] % 32 == 0 && t.nb[0] == 18);
}

fn flash_cache_value(t: const Tensor &, data: const u8[:], base: i64, column: i64,
                     palette: const i8[:]) -> f32 {
    if t.kind == 2 {
        return decode :: compact_value(t.kind, data, base + (column / 32) * t.nb[0], column % 32, palette);
    }
    return load_at(t, data, base + column * t.nb[0]);
}

extern "fg_cuda_q4_attention" fn cuda_q4_attention(config: const i64[:], q: const u8[:], k: const u8[:],
    v: const u8[:], mask: const u8[:], out: u8[:], scale: f32) -> i64;
extern "fg_cuda_kv_read" fn cuda_kv_read(k: const u8[:], v: const u8[:]) -> i64;

export fn fg143_flash_vector_parts(keys: i64) -> i64 {
    return if keys > 0 { low(64, 1 + (keys - 1) / 128) } else { 0 };
}

export fn fg143_flash_stream_workers(queries: i64, groups: i64, keys: i64, sms: i64) -> i64 {
    if queries < 8 || queries > 2147483647 || groups <= 0 || groups > 65535 ||
       keys <= 0 || keys > 2147483647 || sms <= 0 || sms > 1024 { return 0; }
    let tiles = ((queries + 7) / 8) * groups;
    let key_tiles = (keys + 31) / 32;
    if tiles > 2147483647 / key_tiles || key_tiles == 1 { return 0; }
    let raw = low(2 * sms, tiles * key_tiles);
    let rounded = (raw / tiles) * tiles;
    let workers = if rounded > 0 && 100 * (raw - rounded) <= 5 * raw { rounded } else { raw };
    return if workers > 1 && workers != tiles { workers } else { 0 };
}

fn cuda_q4_kv_layout(t: const Tensor &, available: i64) -> bool {
    if t.kind != 2 || t.ne[0] > 256 || t.ne[3] != 1 ||
       t.nb[0] != 18 || t.nb[1] <= 0 || t.nb[1] % 2 != 0 || t.nb[2] < 0 || t.nb[2] % 2 != 0 {
        return false;
    }
    let last = add_size(mul_size(t.ne[0] / 32, 18),
        add_size(mul_size(t.ne[1] - 1, t.nb[1]), mul_size(t.ne[2] - 1, t.nb[2])));
    return last <= t.bytes && t.bytes <= available;
}

fn flash_cuda(p: const Node &, mask: const Tensor &, q: const u8[:], k: const u8[:],
              v: const u8[:], masks: const u8[:], dst: u8[:]) -> i64 {
    let masked = mask.bytes > 0;
    if p.x.ne[3] != 1 || p.fp[1] != 0.0 || p.fp[2] != 0.0 ||
       p.x.nb[0] != 4 || p.x.nb[1] % 4 != 0 || p.x.nb[2] % 4 != 0 || !dense_scalar(p.d, 4) ||
       !cuda_q4_kv_layout(p.y, k.len) || !cuda_q4_kv_layout(p.z, v.len) ||
       (masked && (mask.ne[2] != 1 || mask.ne[3] != 1 || mask.nb[0] != 2 || mask.nb[1] % 2 != 0)) {
        return 0;
    }
    let positions = p.x.ne[1];
    let heads = p.x.ne[2];
    let tasks = mul_size(positions, heads);
    if tasks > 2147483647 { return 0; }
    let segment: i64 = 256;
    let tiles = 1 + (p.y.ne[1] - 1) / segment;
    if tiles > 65535 { return 0; }
    let per_task = mul_size(tiles, (p.z.ne[0] + 2) * 4);
    let batch = low(tasks, high(1, (32 * 1024 * 1024) / per_task));
    // Words 18..20 size the launches and bound the partial softmax buffer.
    // Word 21 gives the first query's causal key count, or zero for an explicit
    // mask or ordinary unmasked attention.
    let config: i64[22] = [p.x.ne[0], p.z.ne[0], positions, heads, p.y.ne[2], p.z.ne[2], p.y.ne[1],
        p.x.nb[1], p.x.nb[2], p.y.nb[1], p.y.nb[2], p.z.nb[1], p.z.nb[2], mask.nb[1],
        p.d.nb[1], p.d.nb[2], segment, tiles, tasks, batch, mul_size(batch, per_task), p.ip[5]];
    return cuda_q4_attention(config, q[0 .. p.x.bytes], k[0 .. p.y.bytes], v[0 .. p.z.bytes],
        masks[0 .. mask.bytes], dst[0 .. p.d.bytes], p.fp[0]);
}

fn flash_attention(p: const Node &, mask: const Tensor &, q: const u8[:], k: const u8[:],
                   v: const u8[:], masks: const u8[:], dst: u8[:], palette: const i8[:]) -> bool {
    if p.x.kind != 0 || p.d.kind != 0 || !flash_value_type(p.y) || !flash_value_type(p.z) ||
       p.x.ne[0] != p.y.ne[0] || p.x.ne[0] <= 0 || p.z.ne[0] <= 0 ||
       p.y.ne[1] != p.z.ne[1] || p.y.ne[1] <= 0 ||
       p.x.ne[1] <= 0 || p.x.ne[2] <= 0 || p.x.ne[3] <= 0 ||
       p.y.ne[2] <= 0 || p.y.ne[3] <= 0 || p.z.ne[2] <= 0 || p.z.ne[3] <= 0 ||
       p.x.ne[2] % p.y.ne[2] != 0 || p.x.ne[2] % p.z.ne[2] != 0 ||
       p.x.ne[3] % p.y.ne[3] != 0 || p.x.ne[3] % p.z.ne[3] != 0 ||
       p.d.ne[0] != p.z.ne[0] || p.d.ne[1] != p.x.ne[2] ||
       p.d.ne[2] != p.x.ne[1] || p.d.ne[3] != p.x.ne[3] ||
       (p.fp[1] > 0.0 && mask.bytes == 0) {
        return false;
    }
    let has_mask = mask.bytes > 0;
    let first_query_keys = p.ip[5];
    if first_query_keys < 0 || (first_query_keys != 0 &&
        (has_mask || first_query_keys > p.y.ne[1] - p.x.ne[1] + 1)) { return false; }
    if has_mask {
        if mask.kind != 1 || mask.ne[0] < p.y.ne[1] || mask.ne[1] < p.x.ne[1] ||
           mask.ne[2] <= 0 || mask.ne[3] <= 0 ||
           p.x.ne[2] % mask.ne[2] != 0 || p.x.ne[3] % mask.ne[3] != 0 {
            return false;
        }
        validate(mask, masks.len);
    }
    validate(p.x, q.len);
    validate(p.d, dst.len);
    let device = flash_cuda(p, mask, q, k, v, masks, dst);
    if device != 0 { return device > 0; }
    if activation_read(q) != 0 || activation_read(k) != 0 ||
       activation_read(v) != 0 || activation_read(masks) != 0 { return false; }
    if cuda_kv_read(k, v) != 0 { return false; }
    let heads = p.x.ne[2];
    let positions = p.x.ne[1];
    var head_power: i64 = 1;
    while head_power <= heads / 2 { head_power *= 2; }
    let neg_inf = from_bits(0xff800000);
    for batch in p.x.ne[3] {
        let kb = (batch / (p.x.ne[3] / p.y.ne[3])) * p.y.nb[3];
        let vb = (batch / (p.x.ne[3] / p.z.ne[3])) * p.z.nb[3];
        for head in heads {
            let kh = kb + (head / (heads / p.y.ne[2])) * p.y.nb[2];
            let vh = vb + (head / (heads / p.z.ne[2])) * p.z.nb[2];
            var slope: f32 = 1.0;
            if p.fp[1] > 0.0 {
                let exponent: f32 = if head < head_power { (head + 1) as f32 } else { ((head - head_power) as f32) + 0.5 };
                slope = pow32(2.0, -p.fp[1] * (exponent as f32) / (head_power as f32));
            }
            for pos in positions {
                let qb = pos * p.x.nb[1] + head * p.x.nb[2] + batch * p.x.nb[3];
                let db = head * p.d.nb[1] + pos * p.d.nb[2] + batch * p.d.nb[3];
                var maximum: f32 = neg_inf;
                var total: f32 = 0.0;
                for column in p.d.ne[0] {
                    store_at(p.d, dst, db + column * p.d.nb[0], 0.0);
                }
                let count = if first_query_keys != 0 { first_query_keys + pos } else { p.y.ne[1] };
                for key in count {
                    var bias: f32 = 0.0;
                    if has_mask {
                        let at = key * mask.nb[0] + pos * mask.nb[1] +
                            (head % mask.ne[2]) * mask.nb[2] + (batch % mask.ne[3]) * mask.nb[3];
                        bias = slope * load_at(mask, masks, at);
                    }
                    if bias == neg_inf { continue; }
                    let kp = kh + key * p.y.nb[1];
                    var score: f32 = 0.0;
                    for column in p.x.ne[0] {
                        score = fma32(load_at(p.x, q, qb + column * p.x.nb[0]),
                            flash_cache_value(p.y, k, kp, column, palette), score);
                    }
                    score *= p.fp[0];
                    if p.fp[2] != 0.0 { score = p.fp[2] * tanh32(score / p.fp[2]); }
                    score += bias;
                    let updated = max32(maximum, score);
                    let previous = exp32(maximum - updated);
                    let weight = exp32(score - updated);
                    total = total * previous + weight;
                    maximum = updated;
                    let vp = vh + key * p.z.nb[1];
                    for column in p.d.ne[0] {
                        let at = db + column * p.d.nb[0];
                        store_at(p.d, dst, at, load_at(p.d, dst, at) * previous +
                            weight * flash_cache_value(p.z, v, vp, column, palette));
                    }
                }
                if total > 0.0 {
                    for column in p.d.ne[0] {
                        let at = db + column * p.d.nb[0];
                        store_at(p.d, dst, at, load_at(p.d, dst, at) / total);
                    }
                }
            }
        }
    }
    return true;
}
