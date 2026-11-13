// Apex hybrid indexed memory: the host-side pool inputs in Goose
// (llama_memory_hybrid_idx_context::set_input_kpool), without touching ggml tensors.
//
// The arena carries ONE sequence table, five slots per sequence, so the stream, the cell range and
// the pool range have a single source:
//   seqdesc[s][0] = stream, [1] = cells_off, [2] = cells_cnt, [3] = pools_off, [4] = pools_cnt
//
// tables: for every pool, the representative cell (its last member's row) and the member cells;
//         pools of sequences absent from the batch fall on the sentinel row. Fresh pools also fill
//         the scatter lists, the representative list and the M-RoPE positions (rotated to the first
//         member); padded entries take distinct uncached cells so the scatter never writes one cell
//         from two lanes; unused slots address the K storage through a live cell and a sentinel index.
// masks : a pool is visible for a token when it belongs to the token's sequence and ends at or
//         before it; in order mode the visible count comes from the token's rank. The selection
//         mask keeps the first min(nv, n_top) ranked pools and marks the remaining slots and the
//         tail slots by whether they hold a real cell.
//
// Packet, int64 slots:
//   k=0  0 op, 1 kpool, 2 n_pool, 3 n_pool_real, 4 n_kv, 5 n_stream, 6 n_tokens, 7 n_seq_max,
//        8 by_order, 9 n_sel, 10 n_top, 11 sel_present
//   k=1  0 seqdesc, 1 cells_pos, 2 cells_cell, 3 is_new, 4 generation, 5 seq_in_ub, 6 pos,
//        7 ub_seq, 8 width, 9 rank, 10 dummy_cell, 11 pools_all
//   k=2  0 pcell, 1 pidx, 2 nidx, 3 nrep, 4 npos, 5 mask, 6 selmask, 7 tail, 8 sps, 9 pend
//   k=3  0 done, 1 ret (fresh pools emitted; or tokens filled in the mask step)
extern "fg314k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg314k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg314k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg314k_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "fg314k_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn at(arr: u64, i: i64) -> i64 {
    return ld(arr, i * (8 as i64));
}

fn setat(arr: u64, i: i64, v: i64) {
    st(arr, i * (8 as i64), v);
}

fn neg_inf() -> i64 {
    return (0 as i64) - 16777216 as i64;
}

fn d_strm(d: u64, s: i64) -> i64 {
    return at(d, s * (5 as i64));
}

fn d_coff(d: u64, s: i64) -> i64 {
    return at(d, s * (5 as i64) + 1 as i64);
}

fn d_ccnt(d: u64, s: i64) -> i64 {
    return at(d, s * (5 as i64) + 2 as i64);
}

fn d_poff(d: u64, s: i64) -> i64 {
    return at(d, s * (5 as i64) + 3 as i64);
}

fn d_pcnt(d: u64, s: i64) -> i64 {
    return at(d, s * (5 as i64) + 4 as i64);
}

fn run(packet: i64) -> bool {
    let op: i64 = get(packet, 0, 0) as i64;
    let kpool: i64 = get(packet, 0, 1) as i64;
    let n_pool: i64 = get(packet, 0, 2) as i64;
    let n_pool_real: i64 = get(packet, 0, 3) as i64;
    let n_kv: i64 = get(packet, 0, 4) as i64;
    let n_stream: i64 = get(packet, 0, 5) as i64;
    let n_tokens: i64 = get(packet, 0, 6) as i64;
    let n_seq_max: i64 = get(packet, 0, 7) as i64;
    let by_order: bool = get(packet, 0, 8) != (0 as u64);
    let n_sel: i64 = get(packet, 0, 9) as i64;
    let n_top: i64 = get(packet, 0, 10) as i64;
    let sel_present: bool = get(packet, 0, 11) != (0 as u64);
    let d: u64 = get(packet, 1, 0);
    let cpos: u64 = get(packet, 1, 1);
    let ccell: u64 = get(packet, 1, 2);
    let is_new: u64 = get(packet, 1, 3);
    let gen: i64 = get(packet, 1, 4) as i64;
    let seq_in_ub: u64 = get(packet, 1, 5);
    let posarr: u64 = get(packet, 1, 6);
    let ub_seq: u64 = get(packet, 1, 7);
    let width: i64 = get(packet, 1, 8) as i64;
    let rankarr: u64 = get(packet, 1, 9);
    let dummy_cell: i64 = get(packet, 1, 10) as i64;
    let pools_all: u64 = get(packet, 1, 11);
    let pcell: u64 = get(packet, 2, 0);
    let pidx: u64 = get(packet, 2, 1);
    let nidx: u64 = get(packet, 2, 2);
    let nrep: u64 = get(packet, 2, 3);
    let npos: u64 = get(packet, 2, 4);
    let mask: u64 = get(packet, 2, 5);
    let selmask: u64 = get(packet, 2, 6);
    let tail: u64 = get(packet, 2, 7);
    let sps: u64 = get(packet, 2, 8);
    let pend: u64 = get(packet, 2, 9);
    ensure(kpool >= (1 as i64) && n_pool > (0 as i64), 60);
    ensure(width >= (1 as i64) && n_seq_max >= (0 as i64), 61);
    let sentinel: i64 = n_kv;
    var ret: i64 = 0;

    if op == (0 as i64) {
        var ip: i64 = 0;
        var i_new: i64 = 0;
        var s: i64 = 0;

        while s < n_seq_max {
            setat(sps, s, ip);
            let strm_s: i64 = d_strm(d, s);
            let base: i64 = d_coff(d, s);
            let pcnt: i64 = d_pcnt(d, s);
            let poff: i64 = d_poff(d, s);
            var inert: bool = false;

            if n_stream > (1 as i64) {
                if at(seq_in_ub, s) == (0 as i64) {
                    inert = true;
                }
            }
            var pi: i64 = 0;

            while pi < pcnt {
                let j: i64 = at(pools_all, poff + pi);
                ensure(ip + (1 as i64) < n_pool, 62);
                let rep_off: i64 = j + kpool - (1 as i64);
                let rep: i64 = at(ccell, base + rep_off);
                setat(pcell, ip, strm_s * n_kv + rep);
                var k: i64 = 0;

                while k < kpool {
                    var v: i64 = sentinel;

                    if !inert {
                        v = at(ccell, base + j + k);
                    }
                    setat(pidx, ip * kpool + k, v);
                    k = k + 1 as i64;
                }

                if at(is_new, ip) == gen {
                    var k2: i64 = 0;

                    while k2 < kpool {
                        setat(nidx, i_new * kpool + k2, strm_s * n_kv + at(ccell, base + j + k2));
                        k2 = k2 + 1 as i64;
                    }
                    setat(nrep, i_new, strm_s * n_kv + rep);
                    // the native position block strides by n_new_g; that value is not part of this transfer, so the
                    // stride here is the real pool count and the gate compares it the same way
                    setat(npos, 0 as i64 * n_pool_real + i_new, at(cpos, base + j));
                    setat(npos, 1 as i64 * n_pool_real + i_new, 0 as i64);
                    setat(npos, 2 as i64 * n_pool_real + i_new, 0 as i64);
                    setat(npos, 3 as i64 * n_pool_real + i_new, at(cpos, base + j));
                    i_new = i_new + 1 as i64;
                }
                setat(pend, ip, at(cpos, base + rep_off));
                ip = ip + 1 as i64;
                pi = pi + 1 as i64;
            }
            s = s + 1 as i64;
        }
        // padded scatter entries: distinct cells never used as a representative
        var padc: i64 = 0;
        var i: i64 = n_pool_real;

        while i < n_pool {
            var taken: bool = true;

            while taken {
                taken = false;
                var q: i64 = 0;

                while q < ip {
                    if at(pcell, q) == padc {
                        taken = true;
                    }
                    q = q + 1 as i64;
                }

                if taken {
                    padc = padc + 1 as i64;
                }
            }
            var kk: i64 = 0;

            while kk < kpool {
                setat(nidx, i * kpool + kk, padc);
                kk = kk + 1 as i64;
            }
            setat(nrep, i, padc);
            padc = padc + 1 as i64;
            i = i + 1 as i64;
        }
        // unused slots: a live cell for the address, a sentinel member index
        var i2: i64 = n_pool_real;

        while i2 < n_pool {
            setat(pcell, i2, dummy_cell);
            var k3: i64 = 0;

            while k3 < kpool {
                setat(pidx, i2 * kpool + k3, sentinel);
                k3 = k3 + 1 as i64;
            }
            i2 = i2 + 1 as i64;
        }
        ret = i_new;
    } else {
        if op == (1 as i64) {
            var i: i64 = 0;

            while i < n_tokens {
                let s: i64 = at(ub_seq, i * width);
                let p: i64 = at(posarr, i);
                let base: i64 = d_coff(d, s);
                let ccnt: i64 = d_ccnt(d, s);
                let pcnt: i64 = d_pcnt(d, s);
                let p0: i64 = at(sps, s);
                let p1: i64 = p0 + pcnt;
                var nv: i64 = 0;

                if by_order {
                    let r: i64 = at(rankarr, i);
                    nv = (r + (1 as i64)) / kpool;

                    if p1 - p0 < nv {
                        nv = p1 - p0;
                    }
                } else {
                    var lo: i64 = p0;
                    var hi: i64 = p1;

                    while lo < hi {
                        let mid: i64 = (lo + hi) / (2 as i64);

                        if at(pend, mid) <= p {
                            lo = mid + (1 as i64);
                        } else {
                            hi = mid;
                        }
                    }
                    nv = lo - p0;
                }
                var q: i64 = 0;

                while q < n_pool {
                    setat(mask, i * n_pool + q, neg_inf());
                    q = q + 1 as i64;
                }
                var z: i64 = 0;

                while z < nv {
                    setat(mask, i * n_pool + p0 + z, 0 as i64);
                    z = z + 1 as i64;
                }

                if sel_present {
                    var nvc: i64 = nv;

                    if n_top < nvc {
                        nvc = n_top;
                    }
                    var t: i64 = 0;

                    while t < n_top * kpool {
                        setat(selmask, i * n_sel + t, neg_inf());
                        t = t + 1 as i64;
                    }
                    var z2: i64 = 0;

                    while z2 < nvc * kpool {
                        setat(selmask, i * n_sel + z2, 0 as i64);
                        z2 = z2 + 1 as i64;
                    }
                }
                var n_tail: i64 = 0;

                if by_order {
                    let r2: i64 = at(rankarr, i);
                    n_tail = (r2 + (1 as i64)) % kpool;
                } else {
                    var pos_min: i64 = 0 as i64;

                    if ccnt > (0 as i64) {
                        pos_min = at(cpos, base);
                    }
                    n_tail = (p - pos_min + (1 as i64)) % kpool;
                }
                var k: i64 = 0;

                while k < kpool - (1 as i64) {
                    var cell: i64 = sentinel;
                    var real: bool = false;

                    if k < n_tail {
                        if by_order {
                            let r3: i64 = at(rankarr, i);
                            cell = at(ccell, base + r3 - k);
                            real = true;
                        } else {
                            let pt: i64 = p - k;
                            var lo2: i64 = 0;
                            var hi2: i64 = ccnt;

                            while lo2 < hi2 {
                                let mid2: i64 = (lo2 + hi2) / (2 as i64);

                                if at(cpos, base + mid2) < pt {
                                    lo2 = mid2 + (1 as i64);
                                } else {
                                    hi2 = mid2;
                                }
                            }

                            if lo2 < ccnt {
                                if at(cpos, base + lo2) == pt {
                                    cell = at(ccell, base + lo2);
                                    real = true;
                                }
                            }
                        }
                    }
                    setat(tail, i * (kpool - (1 as i64)) + k, cell);

                    if sel_present {
                        if n_sel % kpool != (0 as i64) {
                            var v: i64 = neg_inf();

                            if real {
                                v = 0 as i64;
                            }
                            setat(selmask, i * n_sel + n_top * kpool + k, v);
                        }
                    }
                    k = k + 1 as i64;
                }
                i = i + 1 as i64;
            }
            ret = n_tokens;
        } else {
            ensure(false, 63);
        }
    }
    put(packet, 3, 0, 1 as u64);
    put(packet, 3, 1, ret as u64);
    return true;
}

fn main() {
    run(0);
}

