// P9-04 S2: the I/O-compute OVERLAP decisions, from
// runtime/core/src/moe/expert_stream_source.cpp (overlap path only) and
// runtime/core/src/moe/stall_union.h.
//
// Overlap mode does not wait for a layer to be read before it computes. It
// stages the routed experts onto readiness flags and lets the compute threads
// block on exactly the slice they are about to multiply. That turns four
// questions into policy, and all four are pure:
//
//   WHICH BATCH TO WAIT FOR  1089  the previous batch is drained before jobs_
//                                  and the flags are reused — the eviction
//                                  safety guarantee, not a latency choice
//                            371  a worker wakes for a new generation, a GROWN
//                                  batch (the two-wave publish), or spec work
//   WHAT TO COPY AHEAD       1163  the first present projection is the one
//                                  mul_mat_id blocks on first
//                            1168  two-wave publish: commit and publish wave
//                                  one, then commit the rest while the lanes
//                                  already read
//                            1197  wave two appends to a published batch
//   WHEN A SLICE IS READY    1109  one generation per layer in flight; a flag
//                                  counts as ready only when its gen matches
//                            1181  a cache hit is ready with no read at all
//                            1174  the staging loop's commit-only projection
//   WHERE THE WINDOW ENDS    1327  exactly one layer's experts are staged at a
//                                  time; the kernel asking for another layer is
//                                  a fault, not a wait
//                            1291  the stall union brackets the thread's wait
//
// stall_union.h: the interval-union state machine, pure arithmetic over
// injected timestamps — overlap, nesting and the open-interval snapshot.
//
// Not here: pread, O_DIRECT, vm_commit, the LRU list (EXP621), the post-load
// eviction (EXP621), the spec/prefetch queue's own policy, and the choice of
// route-ahead's adoption target. The fixture supplies per-run flags and the
// kernel's answers.

const OV_MAXP: i64 = 3; // MoeRecipe::max_exps

// Which SIDE of the previous batch a caller is on. The drain at 1089 is a hard
// wait for every job of the batch in flight; a worker's wait at 371 is a wait
// for work and must not spin when the batch is complete.
const OV_DRAIN = 0;
const OV_WORKER = 1;

// ov_p0 — 1163-1167. The first present projection of a layer. In a fused-gate_up
// layout the tail slot is empty, so "projection 0" is not a constant.
export fn ov_p0(mask: i64) -> i64 {
    var p: i64 = 0;
    while p < OV_MAXP {
        if (mask & (1 << p)) != 0 {
            return p;
        }
        p += 1;
    }
    return -1;
}

// 1168 — two-wave publish is refused for load_all (nothing is waited on, so
// there is no first slice to shorten the path to) and without a present
// projection to publish.
export fn ov_two_wave(cfg_two_wave: i64, load_all: i64, p0: i64) -> i64 {
    if cfg_two_wave != 0 && load_all == 0 && p0 >= 0 {
        return 1;
    }
    return 0;
}

// 1189-1194 — the commit-only projection handed to touch_entry. Under two-wave
// the miss commits ONLY the first projection's pages up front, so its jobs can
// publish before the other projections' syscalls; without two-wave every
// projection is committed at once and the argument is -1.
export fn ov_commit_only(two_wave: i64, p0: i64, p: i64) -> i64 {
    if two_wave != 0 && p == p0 {
        return p0;
    }
    return -1;
}

// 1119-1135 — the layer's staged set: deduplicated, invalid ids dropped,
// ascending. load_all stages every expert, filling in ascending order.
// `seen` is per-expert scratch, `out` receives the sorted ids.
export fn ov_staged(ids: const i64[:], n_expert: i64, load_all: i64, seen: u8[:],
                    out: i64[:]) -> i64 {
    var e: i64 = 0;
    while e < seen.len {
        seen[e] = 0;
        e += 1;
    }
    var n: i64 = 0;
    var i: i64 = 0;
    while i < ids.len {
        // In load_all the loop index IS the expert: the native stages 0..n_expert-1
        // and ignores what the router said beyond the bank's width.
        var e2: i64 = ids[i];
        if load_all != 0 {
            e2 = -1;
            if i < n_expert {
                e2 = i;
            }
        }
        var take: i64 = 1;
        if (e2 < 0 || e2 >= n_expert || e2 >= seen.len) {
            take = 0;
        } else if (seen[e2] != 0) {
            take = 0;
        }
        if take != 0 {
            seen[e2] = 1;
            if n < out.len {
                out[n] = e2;
            }
            n += 1;
        }
        i += 1;
    }
    if load_all != 0 {
        e = 0;
        while e < n_expert {
            if seen[e] == 0 {
                seen[e] = 1;
                if n < out.len {
                    out[n] = e;
                }
                n += 1;
            }
            e += 1;
        }
    }
    // Ascending. The hook visits experts in ascending order, and the emission
    // below is projection-major over this order.
    var a: i64 = 1;
    while a < n {
        let v: i64 = out[a];
        var b: i64 = a - 1;
        while b >= 0 && out[b] > v {
            out[b + 1] = out[b];
            b -= 1;
        }
        out[b + 1] = v;
        a += 1;
    }
    return n;
}

// 1176-1186 — one expert's staging outcome. A hit marks EVERY present
// projection ready immediately (no read exists to wait for); a miss marks the
// expert as needing the wave's jobs. -1 is the native touch_entry's false: a
// page commit failed, and nothing waits on a flag that will never be published.
export fn ov_stage_verdict(commit_ok: i64, hit: i64) -> i64 {
    if commit_ok == 0 {
        return -1;
    }
    if hit != 0 {
        return 1;
    }
    return 0;
}

// 1188-1196 — how many (projection, expert) jobs one wave of one projection
// carries: a job per staged expert that MISSED. Hits are already ready.
export fn ov_wave_jobs(wave_mask: i64, miss: const u8[:], n_staged: i64) -> i64 {
    var n: i64 = 0;
    var p: i64 = 0;
    while p < OV_MAXP {
        if (wave_mask & (1 << p)) != 0 {
            var i: i64 = 0;
            while i < n_staged {
                if miss[i] != 0 {
                    n += 1;
                }
                i += 1;
            }
        }
        p += 1;
    }
    return n;
}

// 1188-1196 — the job list of a wave, in emission order: projection-major, then
// staged order. `out` receives (p, e) pairs; returns the pair count. This is
// what "copy ahead" actually schedules, and the order is what the two-wave
// publish depends on: the first projection of wave one is what the kernel
// blocks on first.
export fn ov_wave_list(wave_mask: i64, staged: const i64[:], n_staged: i64, miss: const u8[:],
                       out: i64[:]) -> i64 {
    var k: i64 = 0;
    var p: i64 = 0;
    while p < OV_MAXP {
        if (wave_mask & (1 << p)) != 0 {
            var i: i64 = 0;
            while i < n_staged && i < staged.len {
                if miss[i] != 0 {
                    if 2 * k + 1 < out.len {
                        out[2 * k] = p;
                        out[2 * k + 1] = staged[i];
                    }
                    k += 1;
                }
                i += 1;
            }
        }
        p += 1;
    }
    return k;
}

// 1189-1212, 1266-1280 — the batch snapshot published to the lanes. Wave one
// resets the counters and fixes batch_flag_gen_ to this layer's generation, so
// every job of BOTH waves publishes the same gen; the second publish only
// grows batch_njobs_. next_idx_/done_cnt_ are NOT reset by wave two: the lanes
// are already inside the batch.
export fn ov_publish(jobs_reset: i64, gen: i64, flag_gen_in: i64, out: i64[:]) -> i64 {
    var ni: i64 = -1;
    var dc: i64 = -1;
    var fg: i64 = flag_gen_in;
    if jobs_reset != 0 {
        ni = 0;
        dc = 0;
        fg = gen;
    }
    if out.len >= 3 {
        out[0] = ni;
        out[1] = dc;
        out[2] = fg;
    }
    return fg;
}

// 1089-1103 — the wait at the top of load_layer_async. A single predicate, and
// it is about the PREVIOUS batch: jobs_ and the flags may only be reused when
// nothing is still reading into the pages this layer is about to evict.
export fn ov_drain_done(done_cnt: i64, batch_njobs: i64, io_stop: i64) -> i64 {
    if done_cnt == batch_njobs || io_stop != 0 {
        return 1;
    }
    return 0;
}

// 371-380 — a lane's wait. A new generation wakes it, and so does a batch that
// GREW in the generation it already drained: a two-wave publish appends jobs
// after wave one, and the gen comparison alone would never bring the lane back.
export fn ov_worker_wait(io_stop: i64, batch_gen: i64, seen: i64, next_idx: i64,
                         batch_njobs: i64, spec_next: i64, spec_n: i64) -> i64 {
    if io_stop != 0 {
        return 1;
    }
    if batch_gen > seen || next_idx < batch_njobs || spec_next < spec_n {
        return 1;
    }
    return 0;
}

// 338-340 — a lane's drain loop. It belongs to one generation: a batch that has
// been superseded silently ends the drain, and an exhausted index ends it too.
export fn ov_drain_more(batch_gen: i64, my_gen: i64, next_idx: i64, batch_njobs: i64) -> i64 {
    if batch_gen != my_gen {
        return 0;
    }
    if next_idx >= batch_njobs {
        return 0;
    }
    return 1;
}

// 1327-1335 — the gate's scope check. Graph order guarantees exactly one
// layer's experts are staged at a time, so a request for another layer (or an
// expert outside the bank) is a fault: the alternative is waiting forever on a
// flag nobody will publish.
export fn ov_gate_scope(il: i64, cur_il: i64, expert: i64, n_expert: i64) -> i64 {
    if il != cur_il || expert < 0 || expert >= n_expert {
        return 1; // fatal
    }
    return 0;
}

// 1315-1365 — the gate's path. 0 = not a streamed expert tensor (return), 1 =
// fault published (return), 2 = already ready (return, never stalls), 3 = stall
// until the gen matches. `want` is snapshotted ONCE, before the spin: it names
// the layer in flight, and a gen that moved mid-wait belongs to the next layer.
export fn ov_gate_path(ours: i64, fault: i64, flag_gen: i64, want: i64) -> i64 {
    if ours == 0 {
        return 0;
    }
    if fault != 0 {
        return 1;
    }
    if flag_gen == want {
        return 2;
    }
    return 3;
}

// 336-365 — publishing one finished slice. The flag is always stored; the mutex
// and the notify happen only when a compute thread is actually registered as a
// waiter. Publish first, then look for waiters: the compute side registers
// before its last look at the flag and both are seq_cst, so the two cannot miss
// each other, and with nobody waiting this costs one atomic load instead of a
// notify_all that wakes every thread to re-check a foreign predicate.
export fn ov_publish_mode(has_flag: i64, waiters: i64) -> i64 {
    if has_flag == 0 {
        return 0; // spec job: no flag, no waiter
    }
    if waiters != 0 {
        return 2; // store, then lock + notify
    }
    return 1; // store only
}

// 342-346 — a read that failed. The error is recorded either way; only overlap
// mode has compute threads that could read a half-filled slice, so only it
// aborts the graph.
export fn ov_read_failure(overlap: i64) -> i64 {
    if overlap != 0 {
        return 1; // fatal
    }
    return 0; // io_err only
}

// 1197-1233 — a page commit that failed, split by whether the batch was already
// published. Before publication nothing waits, so returning false is enough.
// After wave one published, compute threads may already be blocked on flags
// this batch will now never flip: only going fatal AND waking them ends the
// wait safely.
export fn ov_commit_failure(published: i64) -> i64 {
    if published != 0 {
        return 1; // fatal + wake every waiter
    }
    return 0; // return false, nothing published
}

// 1106 — the speculative-prefetch window closes at this layer's real staging.
// Only route-ahead adopts a loading layer's committed reads; every other run
// passes -1, which cancels rather than adopts.
export fn ov_quiesce_arg(spec_adopt: i64, il: i64) -> i64 {
    if spec_adopt != 0 {
        return il;
    }
    return -1;
}

// stall_union.h:11-20 — the union state machine. `st[0]` = threads currently
// stalled, `st[1]` = open interval start (meaningful only while st[0] > 0),
// `st[2]` = accumulated wall. The 0→1 transition opens, the 1→0 transition
// accumulates: overlapping waits count once.
export fn ov_stall_enter(st: i64[:], now_ns: i64) -> i64 {
    if st.len < 3 {
        return 0;
    }
    let before: i64 = st[0];
    st[0] = before + 1;
    if before == 0 {
        st[1] = now_ns;
        return 1; // opened
    }
    return 0; // nested
}

export fn ov_stall_exit(st: i64[:], now_ns: i64) -> i64 {
    if st.len < 3 {
        return 0;
    }
    if st[0] == 0 {
        return 0; // a stray exit must not drive the count negative
    }
    st[0] = st[0] - 1;
    if st[0] == 0 {
        st[2] = st[2] + (now_ns - st[1]);
        st[1] = 0;
        return 1; // closed
    }
    return 0; // still nested
}

// A snapshot mid-stall must include the interval in progress, or a token-level
// delta spanning it undercounts by the part already elapsed.
export fn ov_stall_total(st: const i64[:], now_ns: i64) -> i64 {
    if st.len < 3 {
        return 0;
    }
    if st[0] > 0 {
        return st[2] + (now_ns - st[1]);
    }
    return st[2];
}
