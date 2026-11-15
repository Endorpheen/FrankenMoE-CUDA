// P10-12 S1: the carrier's generation and prefill accounting (gaps G1).
//
// Native source: runtime/core/src/engine/session.cpp at base commit 1b3f4a1a.
//
//   GenTally::record                157-251   per-token delta, residual and totals
//   PrefillTally::{begin,end}       259-285   the prompt bracket's counters
//   summary per-token averages      1934-1935, 1945-1952, 1967-1968
//
// Why this is portable: every field is arithmetic over counters the host has
// already measured. The /proc samples (RSS, swap, device memory) belong to
// pr_ (proc-readers.goose) and stay with the host; the module owns the
// decomposition that turns one IExpertSource::Stats into a token's row and
// moves the cursors forward.
//
// The prefix is tl_. The cursor semantics are the native ones and are the
// whole point of the module: the source's stats are CUMULATIVE across a warm
// session, so every per-token figure is a delta against the previous token's
// sample, while the totals are what the summary averages over n_gen.
//
// Domain note: the native stores the byte delta in uint64_t after a signed
// subtraction, so a source that reports a smaller total than the cursor wraps
// to a huge value. The port keeps the signed difference (identical bit pattern
// and identical printed value for every total below 2^63, which is every model
// this carrier can map) and the gate's corpus stays inside that domain.
//
// Column order of `out_f` (23 values) and `out_i` (6 values) is fixed and
// documented in README.md; the gate compares every column.

const TL_MIB: f64 = 1024.0 * 1024.0;

// out_f layout, fixed for the gate and the host:
//   0 wall_ms          1 cpu_ms          2 majflt_mib       3 dense_resident_frac
//   4 cache_budget_mib 5 io_ms           6 mgmt_ms          7 compute_ms
//   8 cache_hit_pct    9 stall_ms       10 drain_ms        11 adopt_ms
//  12 acc_io_seconds  13 acc_mgmt_s     14 acc_stall_s     15 acc_drain_s
//  16 acc_adopt_s     17 acc_cpu_s      18 next prev_io_s  19 next prev_mgmt_s
//  20 next prev_stall_s 21 next prev_drain_s 22 next prev_adopt_s
// out_i layout:
//   0 majflt           1 turn            2 read_bytes delta 3 acc_majflt
//   4 acc_read_bytes   5 next prev_bytes

// session.cpp:157-251 — GenTally::record, the streaming-aware arm.
//
// `overlap` is GenTally::overlap (1 when the expert reader runs overlapped).
// `st_on` is `st != nullptr`: 0 means dense-only, and the native returns after
// the wall/fault/cpu decomposition, leaving the streaming columns at their
// defaults and the cursors untouched.
export fn tl_record(overlap: i64, prev_bytes: i64, prev_io_s: f64, prev_mgmt_s: f64, prev_stall_s: f64,
                    prev_drain_s: f64, prev_adopt_s: f64, wall: f64, faults: u64, cpu_s: f64, turn: i64,
                    st_on: i64, st_read_bytes: u64, st_read_s: f64, st_mgmt_s: f64, st_stall_s: f64,
                    st_cache_hits: i64, st_cache_lookups: i64, st_drain_s: f64, st_adopt_s: f64,
                    st_dense_frac: f64, st_cache_budget_bytes: u64, fault_bytes: i64,
                    acc_majflt: u64, acc_cpu_s: f64, acc_read_bytes: i64,
                    acc_io_s: f64, acc_mgmt_s: f64, acc_stall_s: f64, acc_drain_s: f64, acc_adopt_s: f64,
                    out_f: f64[:], out_i: i64[:]) -> i64 {
    let wall_ms: f64 = wall * 1000.0;
    let majflt: u64 = faults;
    let cpu_ms: f64 = cpu_s * 1000.0;
    let majflt_mib: f64 = (majflt as f64) * fault_bytes / TL_MIB;
    out_f[0] = wall_ms;
    out_f[1] = cpu_ms;
    out_f[2] = majflt_mib;
    out_i[0] = majflt as i64;
    out_i[1] = turn;
    // majflt += m.majflt; cpu_seconds += cpu_s; — both before the null check.
    out_i[3] = (acc_majflt + majflt) as i64;
    out_f[17] = acc_cpu_s + cpu_s;

    // Dense-only: the residual is the whole decode and the cache is unmeasured.
    if st_on == 0 {
        out_f[7] = wall_ms;
        out_f[8] = 0.0 - 1.0;
        out_f[3] = 0.0 - 1.0;
        out_f[9] = 0.0;
        out_f[6] = 0.0;
        out_f[5] = 0.0;
        out_f[4] = 0.0;
        out_f[10] = 0.0;
        out_f[11] = 0.0;
        out_i[2] = 0;
        out_i[4] = acc_read_bytes;
        out_i[5] = prev_bytes;
        out_f[12] = acc_io_s;
        out_f[13] = acc_mgmt_s;
        out_f[14] = acc_stall_s;
        out_f[15] = acc_drain_s;
        out_f[16] = acc_adopt_s;
        out_f[18] = prev_io_s;
        out_f[19] = prev_mgmt_s;
        out_f[20] = prev_stall_s;
        out_f[21] = prev_drain_s;
        out_f[22] = prev_adopt_s;
        return 12;
    }

    out_f[3] = st_dense_frac;
    out_f[4] = st_cache_budget_bytes / TL_MIB;
    let read_delta: i64 = (st_read_bytes as i64) - prev_bytes;
    let io_ms: f64 = (st_read_s - prev_io_s) * 1000.0;
    let mgmt_ms: f64 = (st_mgmt_s - prev_mgmt_s) * 1000.0;
    var stall_ms: f64 = 0.0;
    var compute_ms: f64 = 0.0;
    if overlap != 0 {
        // stall is already wall-additive (the union of stalled intervals).
        stall_ms = (st_stall_s - prev_stall_s) * 1000.0;
        compute_ms = wall_ms - stall_ms - mgmt_ms;
    } else {
        compute_ms = wall_ms - io_ms - mgmt_ms;
    }
    if compute_ms < 0.0 {
        compute_ms = 0.0;
    }
    var cache_hit_pct: f64 = 0.0 - 1.0;
    if st_cache_lookups > 0 {
        cache_hit_pct = 100.0 * st_cache_hits / st_cache_lookups;
    }
    let drain_ms: f64 = (st_drain_s - prev_drain_s) * 1000.0;
    let adopt_ms: f64 = (st_adopt_s - prev_adopt_s) * 1000.0;

    out_f[5] = io_ms;
    out_f[6] = mgmt_ms;
    out_f[7] = compute_ms;
    out_f[8] = cache_hit_pct;
    out_f[9] = stall_ms;
    out_f[10] = drain_ms;
    out_f[11] = adopt_ms;
    out_i[2] = read_delta;

    // Cursors advance to the sample just read; the totals take this token's share.
    out_i[5] = st_read_bytes as i64;
    out_f[18] = st_read_s;
    out_f[19] = st_mgmt_s;
    out_f[20] = st_stall_s;
    out_f[21] = st_drain_s;
    out_f[22] = st_adopt_s;
    out_i[4] = acc_read_bytes + read_delta;
    out_f[12] = acc_io_s + io_ms / 1000.0;
    out_f[13] = acc_mgmt_s + mgmt_ms / 1000.0;
    out_f[14] = acc_stall_s + stall_ms / 1000.0;
    out_f[15] = acc_drain_s + drain_ms / 1000.0;
    out_f[16] = acc_adopt_s + adopt_ms / 1000.0;
    return 12;
}

// session.cpp:259-285 — PrefillTally::end(). `begin()` only pins the sample and
// the process CPU clock, so the port takes both as inputs. The moe flag zeroes
// the stats sample exactly as `moe_on ? src.stats() : Stats{}` does; the CPU
// delta is measured whether streaming is on or not.
export fn tl_prefill(moe_on: i64, cpu0: f64, cpu_now: f64,
                     pre_read_bytes: u64, pre_read_s: f64, pre_mgmt_s: f64, pre_stall_s: f64,
                     post_read_bytes: u64, post_read_s: f64, post_mgmt_s: f64, post_stall_s: f64,
                     out: f64[:]) -> i64 {
    var pre_rb: i64 = 0;
    var pre_rs: f64 = 0.0;
    var pre_mg: f64 = 0.0;
    var pre_st: f64 = 0.0;
    var post_rb: i64 = 0;
    var post_rs: f64 = 0.0;
    var post_mg: f64 = 0.0;
    var post_st: f64 = 0.0;
    if moe_on != 0 {
        pre_rb = pre_read_bytes as i64;
        pre_rs = pre_read_s;
        pre_mg = pre_mgmt_s;
        pre_st = pre_stall_s;
        post_rb = post_read_bytes as i64;
        post_rs = post_read_s;
        post_mg = post_mgmt_s;
        post_st = post_stall_s;
    }
    out[0] = cpu_now - cpu0;
    let diff: i64 = post_rb - pre_rb;
    out[1] = diff / TL_MIB;
    out[2] = post_rs - pre_rs;
    out[3] = post_st - pre_st;
    out[4] = post_mg - pre_mg;
    return 5;
}

// session.cpp:1934-1935, 1945-1952, 1967-1968 — the summary's per-token
// averages and the moe residual. `s_per_token` is the run's wall per generated
// token; `overlap` picks which of stall/io is subtracted from it. The moe
// figures stay 0 when the run has no expert source, exactly as the native
// summary leaves them (`if (moe.enabled)`), so the gate compares that branch too.
export fn tl_summary(moe_on: i64, n_gen: i64, majflt: u64, cpu_seconds: f64, read_bytes: i64, io_s: f64,
                     mgmt_s: f64, stall_s: f64, drain_s: f64, adopt_s: f64, s_per_token: f64,
                     overlap: i64, out: f64[:]) -> i64 {
    out[0] = 0.0;
    out[1] = 0.0;
    if n_gen != 0 {
        out[0] = (majflt as f64) / n_gen;
        out[1] = cpu_seconds / n_gen;
    }
    out[2] = 0.0;
    out[3] = 0.0;
    out[4] = 0.0;
    out[5] = 0.0;
    out[6] = 0.0;
    out[7] = 0.0;
    out[8] = 0.0;
    if moe_on == 0 {
        return 9;
    }
    out[2] = read_bytes / TL_MIB;
    if n_gen != 0 {
        out[3] = io_s / n_gen;
        out[4] = mgmt_s / n_gen;
        out[5] = stall_s / n_gen;
        out[7] = drain_s / n_gen;
        out[8] = adopt_s / n_gen;
    }
    var compute: f64 = 0.0;
    if overlap != 0 {
        compute = s_per_token - out[5] - out[4];
    } else {
        compute = s_per_token - out[3] - out[4];
    }
    if compute < 0.0 {
        compute = 0.0;
    }
    out[6] = compute;
    return 9;
}
