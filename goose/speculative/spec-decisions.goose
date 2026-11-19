// Speculative-decoding decisions ported from common/speculative.cpp, ngram-cache.cpp,
// ngram-map.cpp and common.h, composed from the experiments that gated them.
// The prefix sp_ marks the area. Each function keeps the behaviour its gate measured.

fn sp_compatible(vtype_t: i64, add_bos_t: i64, bos_t: i64, add_eos_t: i64, eos_t: i64, nvocab_t: i64,
                     vtype_d: i64, add_bos_d: i64, bos_d: i64, add_eos_d: i64, eos_d: i64, nvocab_d: i64,
                     max_diff: i64, marker: i64) -> i64 {
    if marker != 190 {
        return -1;
    }
    if vtype_t != vtype_d {
        return 0;
    }
    if add_bos_t != add_bos_d {
        return 0;
    }
    if add_bos_t != 0 && bos_t != bos_d {
        return 0;
    }
    if add_eos_t != add_eos_d {
        return 0;
    }
    if add_eos_t != 0 && eos_t != eos_d {
        return 0;
    }
    var diff: i64 = nvocab_t - nvocab_d;
    if diff < 0 {
        diff = 0 - diff;
    }
    if diff > max_diff {
        return 0;
    }
    return 1;
}

fn sp_prefix(draft: i64[:], target: i64[:], k: i64, out: i64[:], marker: i64) -> i64 {
    if marker != 191 {
        return -1;
    }
    if k < 0 || draft.len < k || target.len < k {
        return -1;
    }
    var i: i64 = 0;
    while i < k {
        if draft[i] != target[i] {
            out[0] = i;
            return i;
        }
        i = i + 1;
    }
    out[0] = k;
    return k;
}

fn sp_ngram_accept(n_accepted: i64, n_draft_last: i64, n_low: i64, out: i64[:], marker: i64) -> i64 {
    if marker != 192 {
        return -1;
    }
    if n_draft_last <= 0 {
        out[0] = n_low;
        out[1] = 0;
        return 0;
    }
    // f_acc < 0.25 compared without division: 4 * n_accepted < n_draft_last
    if (4 * n_accepted) < n_draft_last {
        let nl = n_low + 1;
        if nl >= 5 {
            out[0] = 0;
            out[1] = 1;
        } else {
            out[0] = nl;
            out[1] = 0;
        }
    } else {
        out[0] = 0;
        out[1] = 0;
    }
    return 0;
}

fn sp_eagle_accept(seq_id: i64, n_seq: i64, n_rows: i64, n_accepted: i64,
                  verify_pos_first: i64, out: i64[:], marker: i64) -> i64 {
    if marker != 193 {
        return -1;
    }
    if seq_id < 0 || seq_id >= n_seq {
        return 0;
    }
    if n_rows <= 0 {
        return 0;
    }
    var i_g = n_accepted;
    if i_g > n_rows - 1 {
        i_g = n_rows - 1;
    }
    if i_g < 0 {
        i_g = 0;
    }
    out[0] = i_g;
    out[1] = verify_pos_first + i_g;
    return 2;
}

fn sp_ngram_pick(vnum: i64[:], nacc: i64[:], m: i64, nslots: i64, out: i64[:], marker: i64) -> i64 {
    if marker != 194 {
        return -1;
    }
    if nslots <= 0 || vnum.len < nslots || nacc.len < nslots {
        return -1;
    }
    var max_occur: i64 = 0;
    var slot_max: i64 = 0;
    var v: i64 = 0;
    while v < nslots {
        if vnum[v] > max_occur {
            max_occur = vnum[v];
            slot_max = v;
        }
        v = v + 1;
    }
    var sum_occur: i64 = 0;
    v = 0;
    while v < nslots {
        if v != slot_max {
            sum_occur = sum_occur + vnum[v];
        }
        v = v + 1;
    }
    if sum_occur > 0 && max_occur < 2 * sum_occur {
        return 0;
    }
    var ntok = m;
    if nacc[slot_max] < ntok {
        ntok = nacc[slot_max];
    }
    out[0] = slot_max;
    out[1] = ntok;
    return 1;
}

fn sp_n_max(types: i64[:], size_m: i64[:], draft_nmax: i64[:], ntypes: i64, marker: i64) -> i64 {
    if marker != 195 {
        return -1;
    }
    if ntypes < 0 || types.len < ntypes {
        return -1;
    }
    var nmax: i64 = 0;
    var i: i64 = 0;
    while i < ntypes {
        let t = types[i];
        if t == 1 || t == 2 || t == 3 || t == 4 || t == 5 {
            var d = draft_nmax[i];
            if d < 0 {
                d = 0;
            }
            if d > nmax {
                nmax = d;
            }
        } else {
            if t == 6 || t == 7 || t == 8 {
                if size_m[i] > nmax {
                    nmax = size_m[i];
                }
            } else {
                if t == 9 {
                    var d2 = draft_nmax[i];
                    if d2 < 0 {
                        d2 = 0;
                    }
                    if d2 > nmax {
                        nmax = d2;
                    }
                } else {
                    if t == 10 {
                        if 8 > nmax {
                            nmax = 8;
                        }
                    }
                }
            }
        }
        i = i + 1;
    }
    return nmax;
}

fn sp_eq(s: const u8[:], lit: const u8[:]) -> i64 {
    if s.len != lit.len {
        return 0;
    }
    var i: i64 = 0;
    while i < s.len {
        if s[i] != lit[i] {
            return 0;
        }
        i = i + 1;
    }
    return 1;
}

fn sp_type_id(name: const u8[:], marker: i64) -> i64 {
    if marker != 196 {
        return -1;
    }
    if sp_eq(name, "none") == 1 { return 0; }
    if sp_eq(name, "draft-simple") == 1 { return 1; }
    if sp_eq(name, "draft-eagle3") == 1 { return 2; }
    if sp_eq(name, "draft-mtp") == 1 { return 3; }
    if sp_eq(name, "draft-dflash") == 1 { return 4; }
    if sp_eq(name, "draft-dspark") == 1 { return 5; }
    if sp_eq(name, "ngram-simple") == 1 { return 6; }
    if sp_eq(name, "ngram-map-k") == 1 { return 7; }
    if sp_eq(name, "ngram-map-k4v") == 1 { return 8; }
    if sp_eq(name, "ngram-mod") == 1 { return 9; }
    if sp_eq(name, "ngram-cache") == 1 { return 10; }
    return 11;
}

fn sp_try_draft(tok: i64[:], cp: i64[:], cs: i64[:], n: i64,
               min_sample: i64, min_percent: i64, out: i64[:], marker: i64) -> i64 {
    if marker != 197 {
        return -1;
    }
    if n <= 0 || tok.len < n || cp.len < n || cs.len < n {
        return -1;
    }
    var best_tok: i64 = -1;
    var max_cp: i64 = 0;
    var max_cs: i64 = 0;
    var sum_cp: i64 = 0;
    var i: i64 = 0;
    while i < n {
        var w: i64 = 1;
        if cs[i] >= 0 {
            w = 100 * cs[i];
        }
        if cp[i] * w > max_cp * max_cs {
            best_tok = tok[i];
            max_cp = cp[i];
            max_cs = w;
        }
        sum_cp = sum_cp + cp[i];
        i = i + 1;
    }
    out[0] = max_cp;
    out[1] = sum_cp;
    if sum_cp < min_sample {
        return -1;
    }
    if 100 * max_cp < min_percent * sum_cp {
        return -1;
    }
    return best_tok;
}

fn sp_static_thresholds(level: i64, out: i64[:], marker: i64) -> i64 {
    if marker != 198 {
        return -1;
    }
    if level < 0 || level > 3 {
        return -1;
    }
    // { 2, 2, 1, 1 } and { 66, 50, 50, 50 }
    var s: i64 = 1;
    if level == 0 || level == 1 {
        s = 2;
    }
    var p: i64 = 50;
    if level == 0 {
        p = 66;
    }
    out[0] = s;
    out[1] = p;
    return 0;
}

fn sp_static_draft(tok: i64[:], cnt: i64[:], n: i64, marker: i64) -> i64 {
    if marker != 199 {
        return -1;
    }
    if n <= 0 || tok.len < n || cnt.len < n {
        return -1;
    }
    var best: i64 = -1;
    var max_c: i64 = 0;
    var sum_c: i64 = 0;
    var i: i64 = 0;
    while i < n {
        if cnt[i] > max_c {
            best = tok[i];
            max_c = cnt[i];
        }
        sum_c = sum_c + cnt[i];
        i = i + 1;
    }
    // lax table at index LLAMA_NGRAM_STATIC-1
    var idx: i64 = 1;
    if idx == 0 {
        idx = 0;
    }
    var min_sample: i64 = 2;
    var min_pct: i64 = 50;
    if idx == 0 {
        min_sample = 2;
        min_pct = 66;
    }
    if sum_c < min_sample {
        return -1;
    }
    if 100 * max_c < min_pct * sum_c {
        return -1;
    }
    return best;
}

fn sp_cache_order(mask: i64[:], nmask: i64, ngram_min: i64, ngram_max: i64, out: i64[:], marker: i64) -> i64 {
    if marker != 200 {
        return -1;
    }
    if ngram_max < ngram_min || ngram_min < 0 || ngram_max >= nmask || mask.len < nmask {
        return -1;
    }
    var lvl = ngram_max;
    while lvl >= ngram_min {
        let m = mask[lvl];
        var b0 = m - 2 * (m / 2);
        var b1 = (m / 2) - 2 * (m / 4);
        var b2 = (m / 4) - 2 * (m / 8);
        if b0 == 1 {
            out[0] = lvl;
            out[1] = 0;
            return 1;
        }
        if b1 == 1 {
            out[0] = lvl;
            out[1] = 1;
            return 1;
        }
        if b2 == 1 {
            out[0] = lvl;
            out[1] = 2;
            return 1;
        }
        lvl = lvl - 1;
    }
    out[0] = -1;
    out[1] = -1;
    return 0;
}

fn sp_cache_update(inp: i64[:], n_new: i64, ng_min: i64, ng_max: i64,
                  keys: i64[:], vals: i64[:], counts: i64[:], sizes: i64[:], maxpairs: i64,
                  marker: i64) -> i64 {
    if marker != 205 {
        return -1;
    }
    if ng_max < ng_min || ng_min < 1 || n_new < 0 {
        return -1;
    }
    var ndone: i64 = 0;
    var size: i64 = ng_min;
    while size <= ng_max {
        var start: i64 = inp.len - n_new;
        if start < size {
            start = size;
        }
        var i: i64 = start;
        while i < inp.len {
            if ndone >= maxpairs {
                return ndone;
            }
            // key = inp[i-size .. i-1], value = inp[i]
            var k: i64 = 0;
            while k < size {
                keys[ndone * ng_max + k] = inp[i - size + k];
                k = k + 1;
            }
            vals[ndone] = inp[i];
            sizes[ndone] = size;
            // count: number of earlier pairs with the same key and value
            var c: i64 = 1;
            var j: i64 = 0;
            while j < ndone {
                // the key length is part of the key: comparing a size-2 key against a
                // size-1 key by their shared prefix merged distinct pairs (measured: 53 of
                // 200 random cases came out with fewer unique pairs than the native map)
                var same: i64 = 1;
                if vals[j] != vals[ndone] || sizes[j] != size {
                    same = 0;
                } else {
                    var q: i64 = 0;
                    while q < size {
                        if keys[j * ng_max + q] != keys[ndone * ng_max + q] {
                            same = 0;
                        }
                        q = q + 1;
                    }
                }
                if same == 1 {
                    c = c + counts[j];
                    counts[j] = 0;
                }
                j = j + 1;
            }
            if j > ndone {
                j = ndone;
            }
            counts[ndone] = c;
            ndone = ndone + 1;
            i = i + 1;
        }
        size = size + 1;
    }
    return ndone;
}

fn sp_match_pos(inp: i64[:], key: i64[:], n: i64, m: i64, cur_len: i64,
                     size_last_begin: i64, key_map_last_idx: i64, marker: i64) -> i64 {
    if marker != 206 {
        return -1;
    }
    if n <= 0 || m < 0 || key.len < n {
        return -1;
    }
    var lo: i64 = key_map_last_idx + 1;
    // lower range first
    if size_last_begin > n + m + 1 {
        var j: i64 = size_last_begin - n - m - 1;
        while j > lo - 1 {
            if j >= 0 && j + n <= inp.len {
                var k: i64 = 0;
                var ok: i64 = 1;
                while k < n {
                    if inp[j + k] != key[k] {
                        ok = 0;
                    }
                    k = k + 1;
                }
                if ok == 1 {
                    return j;
                }
            }
            j = j - 1;
        }
    }
    // then the upper range
    var u: i64 = cur_len - n - m - 1;
    while u > size_last_begin && u > key_map_last_idx {
        if u >= 0 && u + n <= inp.len {
            var k: i64 = 0;
            var ok: i64 = 1;
            while k < n {
                if inp[u + k] != key[k] {
                    ok = 0;
                }
                k = k + 1;
            }
            if ok == 1 {
                return u;
            }
        }
        u = u - 1;
    }
    return -1;
}

fn sp_hash(tokens: i64[:], start: i64, len: i64, lcg: i64, marker: i64) -> i64 {
    if marker != 207 {
        return -1;
    }
    if len < 0 || start < 0 || start + len > tokens.len || lcg <= 0 {
        return -1;
    }
    var h: i64 = 0;
    var i: i64 = 0;
    while i < len {
        // (h * lcg) mod 2^32 without overflowing: h can be up to 2^32-1 and lcg is
        // 2654435761, whose product does not fit in a signed 64-bit integer.
        let hi: i64 = h / 65536;
        let lo: i64 = h - 65536 * hi;
        var p: i64 = lo * lcg;
        p = p - 4294967296 * (p / 4294967296);
        var q: i64 = hi * lcg;
        q = q - 65536 * (q / 65536);       // only the low 16 bits of this term can reach bit 32
        q = q * 65536;
        q = q - 4294967296 * (q / 4294967296);
        h = p + q;
        h = h - 4294967296 * (h / 4294967296);
        h = h + tokens[start + i];
        h = h - 4294967296 * (h / 4294967296);
        i = i + 1;
    }
    return h;
}

fn sp_slot(hash: i64, slots: i64) -> i64 {
    if slots <= 0 {
        return -1;
    }
    return hash - slots * (hash / slots);
}


// Synthetic acceptance of a drafted token (server_sample_and_accept_synth).
// accept = is_replay || u < prob, and a drafted end-of-sequence is refused unless replaying.
fn sp_synth_accept(prob: f64, u: f64, is_replay: i64, is_eog: i64, marker: i64) -> i64 {
    if marker != 208 {
        return -1;
    }
    var take: i64 = 0;
    if is_replay == 1 {
        take = 1;
    } else {
        if u < prob {
            take = 1;
        }
    }
    if take == 1 && is_replay == 0 && is_eog == 1 {
        take = 0;
    }
    return take;
}

// Rejection acceptance of a drafted token. q_x <= 0 never accepts: the native code guards the
// division rather than dividing by zero, and a draft the draft model never proposed has no ratio.
fn sp_reject_accept(p_x: f64, q_x: f64, u: f64, marker: i64) -> i64 {
    if marker != 209 {
        return -1;
    }
    if q_x <= 0.0 {
        return 0;
    }
    if p_x >= q_x {
        return 1;
    }
    let ratio: f64 = p_x / q_x;
    if u < ratio {
        return 1;
    }
    return 0;
}
