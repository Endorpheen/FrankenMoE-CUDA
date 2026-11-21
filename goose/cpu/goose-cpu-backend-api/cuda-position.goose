extern "fg_cuda_position" fn cuda_position(config: const i64[:], x: const u8[:], y: const u8[:],
    z: const u8[:], dst: u8[:]) -> i64;

fn rope_cuda(p: const Node &, x: const u8[:], positions: const u8[:], factors: const u8[:], dst: u8[:]) -> i64 {
    let dims = p.ip[0];
    let mode = p.ip[1];
    let offs = p.ip[3];
    let width = p.x.ne[0];
    let size = scalar_bytes(p.x.kind);
    if p.nth != 1 || (p.x.kind != 0 && p.x.kind != 1) || p.x.kind != p.d.kind ||
       !same_shape(p.x, p.d) || !dense_scalar(p.d, size) || p.x.nb[0] != size ||
       (p.x.nb[1] | p.x.nb[2] | p.x.nb[3]) % size != 0 ||
       (mode != 0 && mode != 2 && mode != 8 && mode != 24 && mode != 40) ||
       width <= 0 || width % 2 != 0 || dims <= 0 || dims % 2 != 0 ||
       offs < 0 || offs % 2 != 0 || offs > width || dims > width - offs ||
       (mode == 24 && (offs != 0 || dims != width / 2)) ||
       !(p.fp[0] >= 1.0) || !(p.fp[1] > 0.0 && p.fp[1] <= 1.0) ||
       !(p.fp[2] >= 0.0 && p.fp[2] <= 1.0) {
        return 0;
    }
    let multi = (mode & 8) != 0;
    if multi {
        var total: i64 = 0;
        for i in 4 {
            let segment = p.ip[4 + i];
            if segment < 0 || segment > width - total { return 0; }
            total += segment;
        }
        if total <= 0 { return 0; }
    }
    let tokens = p.x.ne[2];
    let positions_needed = mul_size(tokens, if multi { 4 } else { 1 });
    if tokens <= 0 || p.y.kind != 26 || !dense_scalar(p.y, 4) ||
       p.y.bytes < mul_size(4, positions_needed) { return 0; }
    validate(p.x, x.len);
    validate(p.d, dst.len);
    validate(p.y, positions.len);
    let has_factors = p.ip[8] != 0;
    if has_factors {
        if !cv_layout(p.z, factors.len) || !dense_scalar(p.z, 4) ||
           elements(p.z) < width / 2 { return 0; }
        if activation_read(factors) != 0 { return -1; }
        for i in width / 2 {
            if !(load_at(p.z, factors, 4 * i) >= 0.001) { return 0; }
        }
    }
    let count = mul_size(rows(p.x), width / 2);
    if count <= 0 { return 0; }
    let blocks = 1 + (count - 1) / 128;
    let phases = 1 + (tokens - 1) / 128;
    if blocks > 2147483647 || phases > 2147483647 { return 0; }
    let step = pow32(p.fp[0], -2.0 / (dims as f32));
    let corr = rope_correction(p, dims);
    let forward = if p.op == OP_ROPE { 1 } else { 0 };
    let workspace = mul_size(mul_size(tokens, width), 4);
    let decode = mode == 40 && tokens == 1 && p.x.ne[3] == 1 && width <= 512 && dims <= 64;
    let config: i64[27] = [if decode { 2 } else { 0 }, blocks, 1, phases, workspace,
        width, p.x.ne[1], tokens, p.x.ne[3], dims, mode, offs,
        p.ip[4], p.ip[5], p.ip[6], p.ip[7], size, p.x.nb[1], p.x.nb[2], p.x.nb[3],
        forward, bits(step) as i64, bits(p.fp[1]) as i64, bits(p.fp[2]) as i64,
        bits(p.fp[3]) as i64, bits(corr[0]) as i64, bits(corr[1]) as i64];
    let factor_bytes = if has_factors { p.z.bytes } else { 0 };
    return cuda_position(config, x[0 .. p.x.bytes], positions[0 .. p.y.bytes],
        factors[0 .. factor_bytes], dst[0 .. p.d.bytes]);
}

fn conv_cuda(p: const Node &, x: const u8[:], weights: const u8[:], dst: u8[:]) -> i64 {
    if p.nth != 1 || !cv_layout(p.x, x.len) || !cv_layout(p.y, weights.len) ||
       !cv_layout(p.d, dst.len) || !dense_scalar(p.d, 4) ||
       p.x.nb[0] != 4 || p.y.nb[0] != 4 || p.x.ne[3] != 1 ||
       p.y.ne[2] != 1 || p.y.ne[3] != 1 || p.d.ne[3] != 1 {
        return 0;
    }
    let kernel = p.y.ne[0];
    let channels = p.x.ne[1];
    let tokens = p.d.ne[1];
    let sequences = p.d.ne[2];
    if (kernel != 3 && kernel != 4 && kernel != 5 && kernel != 7 && kernel != 9 && kernel != 15) ||
       p.x.ne[0] != add_size(kernel - 1, tokens) || p.y.ne[1] != channels ||
       p.d.ne[0] != channels || p.x.ne[2] != sequences || tokens > 2147483647 - kernel ||
       p.x.nb[1] > 2147483647 || p.x.nb[2] > 2147483647 || p.y.nb[1] > 2147483647 ||
       p.d.nb[1] > 2147483647 || p.d.nb[2] > 2147483647 {
        return 0;
    }
    let groups = 1 + (channels - 1) / 128;
    if sequences > 2147483647 || groups > 65535 { return 0; }
    var config: i64[27] = [0; 27];
    config[0] = 1;
    config[1] = sequences;
    config[2] = groups;
    config[5] = channels;
    config[6] = kernel;
    config[7] = tokens;
    config[8] = p.x.nb[0];
    config[9] = p.x.nb[1];
    config[10] = p.x.nb[2];
    config[11] = p.y.nb[1];
    config[12] = p.d.nb[0];
    config[13] = p.d.nb[1];
    config[14] = p.d.nb[2];
    config[15] = if p.ip[0] == 1 { 1 } else { 0 };
    return cuda_position(config, x[0 .. p.x.bytes], weights[0 .. p.y.bytes], weights[0 .. 0], dst[0 .. p.d.bytes]);
}
