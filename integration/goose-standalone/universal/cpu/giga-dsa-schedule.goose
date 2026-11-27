const DSA_TILE_ROWS: i64 = 512;
const DSA_PACK: i32 = 0;
const DSA_METADATA: i32 = 1;
const DSA_BOUND: i32 = 2;
const DSA_SCORE: i32 = 3;
const DSA_CUTOFF: i32 = 4;
const DSA_COMPACT: i32 = 5;
const DSA_REPLAY: i32 = 6;
const DSA_FINALIZE: i32 = 7;
const DSA_FALLBACK_SCORE: i32 = 8;
const DSA_FALLBACK_TOP: i32 = 9;

extern "giga_dsa_schedule_zero" fn dsa_zero(address: u64, bytes: u64) -> i32;
extern "giga_dsa_schedule_launch" fn dsa_launch(kernel: i32, columns: i64,
    rows: i64, threads: i64, args: const u64[:]) -> i32;

fn dsa_tile_rows(rows: i64) -> i64 {
    return if rows < DSA_TILE_ROWS { rows } else { DSA_TILE_ROWS };
}

export fn giga_dsa_schedule_workspace(cells: i64, rows: i64) -> u64 {
    if cells <= 4096 || cells > 32768 || cells % 16 != 0 || rows < 1 || rows > 4096 { return 0; }
    let tile = dsa_tile_rows(rows);
    return (cells * 260 + tile * (64 * 264 + 8212) + 16) as u64;
}

export fn giga_dsa_schedule(config: const i64[:], buffers: const u64[:], workspace_bytes: u64) -> i32 {
    if config.len != 6 || buffers.len != 8 { return 0; }
    let cells = config[0];
    let rows = config[1];
    let required = giga_dsa_schedule_workspace(cells, rows);
    let workspace = buffers[7];
    if required == 0 || workspace_bytes < required || workspace % 16 != 0 ||
       0xffffffffffffffff - workspace < required || config[2] < 256 || config[2] % 2 != 0 ||
       config[3] != 32768 || config[4] != 256 || config[5] != cells * 2 { return 0; }
    for i in buffers.len { if buffers[i] == 0 { return 0; } }
    let tile = dsa_tile_rows(rows);
    let heads_capacity = tile * 64;
    let c = cells as u64;
    let h = heads_capacity as u64;
    let t = tile as u64;
    let khi = workspace;
    let klo = khi + c * 128;
    let qhi = klo + c * 128;
    let qlo = qhi + h * 128;
    let ks = qlo + h * 128;
    let qs = ks + c * 4;
    let norms = qs + h * 4;
    let maxima = norms + h * 4;
    let bounds = maxima + 16;
    let cutoffs = bounds + t * 4;
    let band = cutoffs + t * 4;
    let replay = band + t * 4096;
    let counts = replay + t * 4096;
    let status = counts + t * 8;
    let key = buffers[1];
    let scores = buffers[4];
    let device_config = buffers[6];

    if dsa_zero(maxima, 16) != 0 { return -1; }
    var pack_key: u64[6] = [key, khi, klo, ks, config[2] as u64, 1];
    var meta_key: u64[6] = [key, ks, 0, maxima, config[2] as u64, 1];
    if dsa_launch(DSA_PACK, cells, 1, 32, pack_key[..]) != 0 ||
       dsa_launch(DSA_METADATA, cells, 1, 32, meta_key[..]) != 0 { return -1; }
    var first: i64 = 0;
    while first < rows {
        let active = dsa_tile_rows(rows - first);
        let heads = active * 64;
        let query = buffers[0] + ((first * config[3]) as u64);
        let weights = buffers[2] + ((first * config[4]) as u64);
        let mask = buffers[3] + ((first * config[5]) as u64);
        let ids = buffers[5] + ((first * 8192) as u64);
        var pack_query: u64[6] = [query, qhi, qlo, qs, 512, 0];
        var meta_query: u64[6] = [query, qs, norms, maxima, 512, 0];
        var bound: u64[5] = [norms, qs, maxima, weights, bounds];
        var score: u64[10] = [khi, klo, qhi, qlo, ks, qs, weights, mask, scores, c];
        var cutoff: u64[3] = [scores, cutoffs, c];
        var compact: u64[9] = [scores, mask, cutoffs, bounds, ids, band, counts, c, 1024];
        var exact: u64[9] = [query, key, weights, mask, replay, device_config, band, counts, 1024];
        var finish: u64[6] = [replay, band, counts, ids, status, c];
        var fallback_score: u64[7] = [query, key, weights, mask, scores, device_config, status];
        var fallback_top: u64[4] = [scores, ids, status, c];
        if dsa_launch(DSA_PACK, heads, 1, 32, pack_query[..]) != 0 ||
           dsa_launch(DSA_METADATA, heads, 1, 32, meta_query[..]) != 0 ||
           dsa_launch(DSA_BOUND, active, 1, 32, bound[..]) != 0 ||
           dsa_launch(DSA_SCORE, cells / 16, active, 256, score[..]) != 0 ||
           dsa_launch(DSA_CUTOFF, active, 1, 256, cutoff[..]) != 0 ||
           dsa_launch(DSA_COMPACT, active, 1, 256, compact[..]) != 0 ||
           dsa_launch(DSA_REPLAY, 256, active, 128, exact[..]) != 0 ||
           dsa_launch(DSA_FINALIZE, active, 1, 256, finish[..]) != 0 ||
           dsa_launch(DSA_FALLBACK_SCORE, 128, active, 128, fallback_score[..]) != 0 ||
           dsa_launch(DSA_FALLBACK_TOP, active, 1, 256, fallback_top[..]) != 0 { return -1; }
        first += tile;
    }
    return 1;
}
