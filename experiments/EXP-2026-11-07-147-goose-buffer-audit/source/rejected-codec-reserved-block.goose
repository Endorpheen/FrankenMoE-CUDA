import.codec.decode;

fn codec_transform(
    p: const Node &,
    x: const u8[:],
    ids: const u8[:],
    dst: u8[:],
    scratch: f32[:],
    gxx: const u8[8][:],
    gxs: const u8[8][:],
    gs: const u8[8][:],
    g3x: const u8[4][:],
    g3s: const u8[4][:],
    g1: const i8[8][:],
    signs: const u8[:],
    palette: const i8[:]) -> bool {
    if p.op != OP_DUP && p.op != OP_CPY && p.op != OP_CONT && p.op != OP_GET_ROWS {
        return false;
    }

    if p.d.kind != 0 || scalar_type(p.x) {
        return false;
    }
    let kind = decode :: super_kind(p.x.kind);
    let block = if kind >= 0 {
        256
    } else {
        decode :: compact_width(p.x.kind)
    }
    ;
    let bytes = if kind >= 0 {
        decode :: tile_traits(kind).bytes
    } else {
        decode :: compact_bytes(p.x.kind)
    }
    ;

    if bytes == 0 {
        return false;
    }
    let width = p.x.ne[0];
    assert(width > 0 && width % block == 0);
    let rowbytes = mul_size(width / block, bytes);
    assert(p.x.bytes <= x.len && scratch.len >= mul_size(width, p.nth));
    validate(p.d, dst.len);
    let temp = p.ith * width;

    if p.op == OP_GET_ROWS {
        assert(p.y.kind == 26 && p.d.ne[0] == width);
        validate(p.y, ids.len);
    } else {
        assert(elements(p.x) == elements(p.d));
    }
    let nrows = if p.op == OP_GET_ROWS {
        rows(p.d)
    } else {
        rows(p.x)
    }
    ;
    var row = thread_first(p);

    while row < nrows {
        var source: i64 = 0;

        if p.op == OP_GET_ROWS {
            let i0 = row % p.y.ne[0];
            let i1 = (row / p.y.ne[0]) % p.y.ne[1];
            let i2 = row / (p.y.ne[0] * p.y.ne[1]);
            let id = read_index(p.y, ids, row);
            assert(id >= 0 && id < p.x.ne[1]);
            source = id * p.x.nb[1] + i1 * p.x.nb[2] + i2 * p.x.nb[3];
        } else {
            let i1 = row % p.x.ne[1];
            let i2 = (row / p.x.ne[1]) % p.x.ne[2];
            let i3 = row / (p.x.ne[1] * p.x.ne[2]);
            source = i1 * p.x.nb[1] + i2 * p.x.nb[2] + i3 * p.x.nb[3];
        }
        assert(source >= 0 && source <= p.x.bytes && rowbytes <= p.x.bytes - source);
        let output = scratch[temp .. temp + width];
        assert(decode :: row_entry(p.x.kind, x[source .. source + rowbytes], output, gxx, gxs, gs, g3x, g3s, g1, signs, palette));
        for col in width {
            store(p.d, dst, row * width + col, output[col]);
        }
        row += p.nth;
    }
    return true;
}

