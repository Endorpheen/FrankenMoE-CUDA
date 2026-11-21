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
    let block_width = if kind >= 0 {
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
    assert(width > 0 && width % block_width == 0);
    let rowbytes = mul_size(width / block_width, bytes);
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

extern "fg143_cpu_gather_threads" fn cpu_gather_threads() -> i64;
extern "fg143_cpu_prefill" fn cpu_prefill() -> bool;
extern "fg143_gather_affinity" fn gather_affinity(prefill: bool, task: i64) -> i64;
extern "fg143_gather_index" fn gather_index(address: u64) -> i64;
extern "fg143_gather_fetch" fn gather_fetch(address: u64, row: u8[:]);
extern "fg143_gather_store" fn gather_store(address: u64, row: const f32[:]);
extern "fg143_gather_witness" fn gather_witness(rows: i64, tasks: i64);

struct CodecGather {
    weights: u64, ids: u64, dst: u64, width: i64, rows: i64,
    weight_rows: i64, stride: i64, row_bytes: i64, kind: i64, tasks: i64, prefill: bool
}

fn codec_gather_partition(job: CodecGather, task: i64) -> bool {
    var raw: u8[>..] = [];
    raw.resize(job.row_bytes, 0);
    var values: f32[>..] = [];
    values.resize(job.width, 0.0);
    var tile: decode :: WeightTile = default <decode :: WeightTile>();
    let g8: u8[8][0] = [];
    let g4: u8[4][0] = [];
    let g1: i8[8][0] = [];
    let signs: u8[0] = [];
    let palette: i8[0] = [];
    let first = job.rows * task / job.tasks;
    let last = job.rows * (task + 1) / job.tasks;
    for row in first .. last {
        let id = gather_index(job.ids + ((4 * row) as u64));
        if id < 0 || id >= job.weight_rows { return false; }
        gather_fetch(job.weights + ((id * job.stride) as u64), raw[..]);
        if !decode :: row_entry_with_workspace(job.kind, raw[..], values[..],
            g8, g8, g8, g4, g4, g1, signs, palette, tile) { return false; }
        gather_store(job.dst + ((4 * job.width * row) as u64), values[..]);
    }
    return true;
}

thread_fn codec_gather_worker(job: CodecGather, task: i64) {
    assert(gather_affinity(job.prefill, task) == 0);
    assert(codec_gather_partition(job, task));
}

export fn fg143_cpu_batch_supported(tasks: i64) -> bool {
    return tasks > 0 && tasks - 1 <= hardware_threads();
}

fn codec_parallel_gather(p: const Node &, addresses: const i64[3] &) -> i64 {
    if p.op != OP_GET_ROWS || p.nth != 1 || (p.x.kind != 12 && p.x.kind != 14) ||
       p.d.kind != 0 || p.y.kind != 26 || !dense_scalar(p.d, 4) || !dense_scalar(p.y, 4) ||
       p.x.ne[2] != 1 || p.x.ne[3] != 1 || p.y.ne[1] != 1 || p.y.ne[2] != 1 ||
       p.y.ne[3] != 1 || p.d.ne[0] != p.x.ne[0] || p.y.ne[0] != rows(p.d) ||
       p.x.ne[0] <= 0 || p.x.ne[0] % 256 != 0 || p.x.ne[1] <= 0 ||
       addresses[0] == 0 || addresses[1] == 0 || addresses[2] == 0 { return 0; }
    let tasks = cpu_gather_threads();
    let count = p.y.ne[0];
    if tasks <= 1 || count < tasks || count > 2147483647 { return 0; }
    let weights = addresses[0] as! u64;
    let ids = addresses[1] as! u64;
    let dst = addresses[2] as! u64;
    let traits = decode :: tile_traits(decode :: super_kind(p.x.kind));
    let row_bytes = mul_size(p.x.ne[0] / 256, traits.bytes);
    if p.x.nb[0] != traits.bytes || p.x.nb[1] < row_bytes || p.x.bytes < row_bytes ||
       p.x.ne[1] - 1 > (p.x.bytes - row_bytes) / p.x.nb[1] ||
       p.y.bytes != 4 * count || p.d.bytes != mul_size(mul_size(p.x.ne[0], count), 4) ||
       !blit_disjoint(weights, p.x.bytes, dst, p.d.bytes) ||
       !blit_disjoint(ids, p.y.bytes, dst, p.d.bytes) { return 0; }

    let job = CodecGather { weights, ids, dst, p.x.ne[0], count, p.x.ne[1], p.x.nb[1], row_bytes, p.x.kind, tasks, cpu_prefill() };
    var workers: i64[>..] = [];
    for task in 1 .. tasks { workers.push(thread_spawn_contained(codec_gather_worker, job, task)); }
    var ok = codec_gather_partition(job, 0);
    for id in workers {
        if !thread_wait(id) { ok = false; }
    }
    if !ok { return -1; }
    gather_witness(count, tasks);
    return 1;
}

