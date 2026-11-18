extern fn logf(x: f32) -> f32;
extern fn powf(x: f32, y: f32) -> f32;
// Sampler chain in Goose: MT19937, the canonical double draw and the
// top-k / top-p / min-p / temperature / distribution sequence, ported from
// src/llama-sampler.cpp.
//
// The chain this profile uses is
//   penalties (no-op) -> dry (off) -> top_n_sigma (off) -> top_k 20 ->
//   typical_p (off) -> top_p 0.95 -> min_p 0 -> xtc (off) -> temp 1 -> dist
// so the arithmetic that decides the token is: a partial sort of the logits,
// a softmax, a cumulative cut, a division by the temperature, and one draw.
//
// Exactness details that are not cosmetic:
//   - top_k uses std::partial_sort through libstdc++'s heap_select/sort_heap
//     with `logit > logit` as the comparator. The heap layout decides which
//     equal-logit candidates survive, so the heap operations below are the
//     canonical push_heap/adjust_heap/pop_heap sequence, not any sort;
//   - softmax and the cumulative sums are float probabilities accumulated into
//     a double, exactly like the native code;
//   - the draw is std::generate_canonical<double,53> over MT19937, which is two
//     32-bit draws combined as (r1 + r2 * 2^32) / 2^64;
//   - size == 1 still consumes one draw, because the native code keeps the RNG
//     aligned with backend sampling.
//
// expf/logf come from libm: they are primitives, not decisions, and the native
// code calls the same functions.
//
// MT19937 state: 625 i64 words, state[0..623] are the generator words and
// state[624] is the position (624 = a twist is due).

extern "expf" fn c_expf(x: f32) -> f32;
extern "logf" fn c_logf(x: f32) -> f32;

const MT_N: i64 = 624;
const MT_M: i64 = 397;
const MT_MATRIX_A: i64 = 0x9908B0DF;
const MT_UPPER: i64 = 0x80000000;
const MT_LOWER: i64 = 0x7FFFFFFF;
const M32: i64 = 0xFFFFFFFF;

fn sm_mt_seed(state: i64[:], seed: i64) -> i64 {
    if state.len < 625 {
        return -1;
    }
    state[0] = seed & M32;
    var i: i64 = 1;
    while i < MT_N {
        let prev: i64 = state[i - 1];
        state[i] = (1812433253 * (prev ^ (prev >> 30)) + i) & M32;
        i = i + 1;
    }
    state[624] = MT_N;
    return 0;
}

fn mt_twist(state: i64[:]) {
    var i: i64 = 0;
    while i < MT_N {
        let x: i64 = (state[i] & MT_UPPER) | (state[(i + 1) % MT_N] & MT_LOWER);
        var xa: i64 = x >> 1;
        if (x & 1) != 0 {
            xa = xa ^ MT_MATRIX_A;
        }
        state[i] = state[(i + MT_M) % MT_N] ^ xa;
        i = i + 1;
    }
    state[624] = 0;
}

fn sm_mt_next(state: i64[:]) -> i64 {
    if state.len < 625 {
        return 0;
    }
    if state[624] >= MT_N {
        mt_twist(state);
    }
    var y: i64 = state[state[624]];
    state[624] = state[624] + 1;
    y = y ^ (y >> 11);
    y = y ^ ((y << 7) & 0x9D2C5680);
    y = y ^ ((y << 15) & 0xEFC60000);
    y = y ^ (y >> 18);
    return y & M32;
}

// std::generate_canonical<double, 53> for a 32-bit generator.
fn mt_canonical(state: i64[:]) -> f64 {
    let r1: f64 = sm_mt_next(state) as f64;
    let r2: f64 = sm_mt_next(state) as f64;
    return (r1 + r2 * 4294967296.0) / 18446744073709551616.0;
}

fn less_at(lv: f32[:], a: i64, b: i64) -> bool {
    return lv[a] > lv[b];
}

fn swap_at(lv: f32[:], id: i64[:], a: i64, b: i64) {
    let t: f32 = lv[a];
    lv[a] = lv[b];
    lv[b] = t;
    let u: i64 = id[a];
    id[a] = id[b];
    id[b] = u;
}

// __push_heap(first, hole, top, value)
fn push_heap(lv: f32[:], id: i64[:], first: i64, hole_in: i64, top: i64, v_l: f32, v_id: i64) -> i64 {
    var hole: i64 = hole_in;
    var parent: i64 = (hole - 1) / 2;
    while hole > top && lv[first + parent] > v_l {
        lv[first + hole] = lv[first + parent];
        id[first + hole] = id[first + parent];
        hole = parent;
        parent = (hole - 1) / 2;
    }
    lv[first + hole] = v_l;
    id[first + hole] = v_id;
    return hole;
}

// __adjust_heap(first, hole, len, value)
fn adjust_heap(lv: f32[:], id: i64[:], first: i64, hole_in: i64, len: i64, v_l: f32, v_id: i64) {
    var hole: i64 = hole_in;
    var second: i64 = hole;
    while second < (len - 1) / 2 {
        second = 2 * (second + 1);
        if lv[first + second] > lv[first + second - 1] {
            second = second - 1;
        }
        lv[first + hole] = lv[first + second];
        id[first + hole] = id[first + second];
        hole = second;
    }
    if (len & 1) == 0 && second == (len - 2) / 2 {
        second = 2 * (second + 1);
        lv[first + hole] = lv[first + second - 1];
        id[first + hole] = id[first + second - 1];
        hole = second - 1;
    }
    // __push_heap stops at the ORIGINAL hole, not at the root: make_heap calls
    // adjust_heap with a non-root hole, and using 0 there reorders ties.
    push_heap(lv, id, first, hole, hole_in, v_l, v_id);
}

fn make_heap(lv: f32[:], id: i64[:], first: i64, len: i64) {
    if len < 2 {
        return;
    }
    var parent: i64 = (len - 2) / 2;
    while true {
        let v_l: f32 = lv[first + parent];
        let v_id: i64 = id[first + parent];
        adjust_heap(lv, id, first, parent, len, v_l, v_id);
        if parent == 0 {
            return;
        }
        parent = parent - 1;
    }
}

// __pop_heap(first, last, result): moves *first into *result and repairs the heap
fn pop_heap_to(lv: f32[:], id: i64[:], first: i64, last: i64, result: i64) {
    let v_l: f32 = lv[result];
    let v_id: i64 = id[result];
    lv[result] = lv[first];
    id[result] = id[first];
    adjust_heap(lv, id, first, 0, last - first, v_l, v_id);
}

// std::__sort_heap decrements the end BEFORE popping, so the repaired range is
// [first, last) with last already one shorter and the result at that position.
fn sort_heap(lv: f32[:], id: i64[:], first: i64, len: i64) {
    var last: i64 = len;
    while last > 1 {
        last = last - 1;
        pop_heap_to(lv, id, first, last, last);
    }
}

// std::partial_sort(first, first+npartial, first+n, comp)
fn sm_partial_sort(lv: f32[:], id: i64[:], n: i64, npartial: i64) -> i64 {
    if n <= 0 || npartial <= 0 || npartial > n || lv.len < n || id.len < n {
        return -1;
    }
    make_heap(lv, id, 0, npartial);
    var i: i64 = npartial;
    while i < n {
        if lv[i] > lv[0] {
            pop_heap_to(lv, id, 0, npartial, i);
        }
        i = i + 1;
    }
    sort_heap(lv, id, 0, npartial);
    return npartial;
}

// Greedy: the first position with a strictly larger logit wins.
fn sm_greedy(lv: f32[:], n: i64) -> i64 {
    if n <= 0 || lv.len < n {
        return -1;
    }
    var sel: i64 = 0;
    var i: i64 = 1;
    while i < n {
        if lv[i] > lv[sel] {
            sel = i;
        }
        i = i + 1;
    }
    return sel;
}

fn softmax_in_place(lv: f32[:], p: f32[:], n: i64, sorted: i64) -> f64 {
    var max_l: f32 = lv[0];
    if sorted == 0 {
        var i: i64 = 1;
        while i < n {
            if lv[i] > max_l {
                max_l = lv[i];
            }
            i = i + 1;
        }
    }
    var cum: f32 = 0.0;
    var i: i64 = 0;
    while i < n {
        let e: f32 = c_expf(lv[i] - max_l);
        p[i] = e;
        cum = cum + e;
        i = i + 1;
    }
    i = 0;
    while i < n {
        p[i] = p[i] / cum;
        i = i + 1;
    }
    return cum as f64;
}

// Full chain on a candidate array that starts as the whole vocabulary.
// ip: 0 top_k, 1 min_keep, 2 use_dist (0 = greedy after the temperature).
// fp: 0 top_p, 1 min_p, 2 temp.
// out: 0 selected position, 1 token id, 2 surviving candidate count.
fn sm_select(lv: f32[:], id: i64[:], p: f32[:], n_in: i64,
                    ip: const i64[:], fp: const f32[:], state: i64[:], out: i64[:]) -> i64 {
    if ip.len < 3 || fp.len < 3 || out.len < 3 || lv.len < n_in || id.len < n_in || p.len < n_in || n_in <= 0 {
        return -1;
    }
    let k: i64 = ip[0];
    let min_keep: i64 = ip[1];
    let use_dist: i64 = ip[2];
    let top_p: f32 = fp[0];
    let min_p: f32 = fp[1];
    let temp: f32 = fp[2];

    var n: i64 = n_in;
    var i: i64 = 0;
    while i < n {
        id[i] = i;
        i = i + 1;
    }

    // top_k
    var sorted: i64 = 0;
    if k > 0 {
        let kk: i64 = if k < n { k } else { n };
        if sm_partial_sort(lv, id, n, kk) < 0 {
            return -1;
        }
        n = kk;
        sorted = 1;
    }

    // top_p: the native code sorts when it is not already sorted, which only
    // happens when top_k is disabled. This port requires the sorted path.
    if top_p < 1.0 {
        if sorted == 0 {
            return -2;
        }
        softmax_in_place(lv, p, n, 1);
        var cum: f32 = 0.0;
        var last: i64 = n;
        i = 0;
        while i < n {
            cum = cum + p[i];
            if cum >= top_p && i + 1 >= min_keep {
                last = i + 1;
                break;
            }
            i = i + 1;
        }
        n = last;
    }

    // min_p
    if min_p > 0.0 {
        if sorted == 0 {
            return -3;
        }
        let min_logit: f32 = lv[0] + c_logf(min_p);
        var stop: i64 = 1;
        i = 1;
        while i < n {
            if lv[i] < min_logit && i >= min_keep {
                break;
            }
            stop = i + 1;
            i = i + 1;
        }
        n = stop;
    }

    // temperature
    if temp <= 0.0 {
        let sel: i64 = sm_greedy(lv, n);
        out[0] = sel;
        out[1] = id[sel];
        out[2] = n;
        return 0;
    }
    i = 0;
    while i < n {
        lv[i] = lv[i] / temp;
        i = i + 1;
    }

    // dist
    if n == 1 {
        let ignored: f64 = mt_canonical(state);
        p[0] = 1.0;
        out[0] = 0;
        out[1] = id[0];
        out[2] = n;
        return 0;
    }
    if use_dist == 0 {
        let sel: i64 = sm_greedy(lv, n);
        out[0] = sel;
        out[1] = id[sel];
        out[2] = n;
        return 0;
    }
    var max_l: f32 = lv[0];
    if sorted == 0 {
        i = 1;
        while i < n {
            if lv[i] > max_l {
                max_l = lv[i];
            }
            i = i + 1;
        }
    }
    var sum_cum: f64 = 0.0;
    i = 0;
    while i < n {
        let e: f32 = c_expf(lv[i] - max_l);
        p[i] = e;
        sum_cum = sum_cum + (e as f64);
        i = i + 1;
    }
    let rnd: f64 = mt_canonical(state);
    let tgt: f64 = sum_cum * rnd;
    var run: f64 = 0.0;
    var sel: i64 = 0;
    var found: bool = false;
    i = 0;
    while i < n {
        if !found {
            run = run + (p[i] as f64);
            if run >= tgt {
                sel = i;
                found = true;
            }
        }
        p[i] = (p[i] as f64 / sum_cum) as f32;
        i = i + 1;
    }
    if !found {
        sel = n - 1;
    }
    out[0] = sel;
    out[1] = id[sel];
    out[2] = n;
    return 0;
}

// ---- individual chain steps, for a host whose sampler chain applies them one
// by one. Each works on the same candidate array the chain sees: logits in
// candidate order, ids in parallel, and it reports what it changed.

// top_k: keep the k highest logits, ordered as std::partial_sort leaves them.
// The host re-orders its candidate array by `id` afterwards.
fn sm_apply_top_k(lv: f32[:], id: i64[:], n: i64, k: i64) -> i64 {
    if n <= 0 || lv.len < n || id.len < n {
        return -1;
    }
    var i: i64 = 0;
    while i < n {
        id[i] = i;
        i = i + 1;
    }
    if k <= 0 {
        return n;
    }
    let kk: i64 = if k < n { k } else { n };
    if sm_partial_sort(lv, id, n, kk) < 0 {
        return -1;
    }
    return kk;
}

// top_p: the shortest prefix whose softmax mass reaches p, honouring min_keep.
fn sm_apply_top_p(lv: f32[:], p: f32[:], n: i64, top_p: f32, min_keep: i64) -> i64 {
    if n <= 0 || lv.len < n || p.len < n {
        return -1;
    }
    if top_p >= 1.0 {
        return n;
    }
    softmax_in_place(lv, p, n, 1);
    var cum: f32 = 0.0;
    var i: i64 = 0;
    while i < n {
        cum = cum + p[i];
        if cum >= top_p && i + 1 >= min_keep {
            return i + 1;
        }
        i = i + 1;
    }
    return n;
}

// min_p: drop the tail below max_logit + log(p), honouring min_keep.
fn sm_apply_min_p(lv: f32[:], n: i64, min_p: f32, min_keep: i64) -> i64 {
    if n <= 0 || lv.len < n {
        return -1;
    }
    if min_p <= 0.0 {
        return n;
    }
    let min_logit: f32 = lv[0] + c_logf(min_p);
    var stop: i64 = 1;
    var i: i64 = 1;
    while i < n {
        if lv[i] < min_logit && i >= min_keep {
            break;
        }
        stop = i + 1;
        i = i + 1;
    }
    return stop;
}

// temperature: divide in place; temp <= 0 keeps only the best candidate.
fn sm_apply_temp(lv: f32[:], n: i64, temp: f32) -> i64 {
    if n <= 0 || lv.len < n {
        return -1;
    }
    if temp <= 0.0 {
        let sel: i64 = sm_greedy(lv, n);
        var i: i64 = 0;
        while i < n {
            if i != sel {
                lv[i] = 0.0 - 3.4028234663852886e38;
            }
            i = i + 1;
        }
        return sel + 1;
    }
    var i: i64 = 0;
    while i < n {
        lv[i] = lv[i] / temp;
        i = i + 1;
    }
    return n;
}

// typical-p: keep the tokens whose surprisal is closest to the entropy of the
// distribution, honouring min_keep. Mirrors llama_sampler_typical_apply:
// sort by logit desc, softmax, entropy, shifted = |(-log p) - entropy|, order by
// shifted ascending, cumulative until > typical_p and i >= min_keep-1, then
// leave the kept set in shifted order (unsorted flag at the caller).
// NOTE: selection sort here is O(n^2); a heap sort is required before shipping
// for the full vocabulary (the module already carries heap primitives).

// ascending order over three parallel arrays (heap sort)
fn sort3_asc(lv: f32[:], id: i64[:], p: f32[:], n: i64) {
    var s: i64 = n / 2 - 1;
    while s >= 0 {
        var root: i64 = s;
        while root * 2 + 1 < n {
            var child: i64 = root * 2 + 1;
            if child + 1 < n && lv[child + 1] > lv[child] {
                child = child + 1;
            }
            if lv[root] >= lv[child] {
                root = n;
            } else {
                let tl: f32 = lv[root]; lv[root] = lv[child]; lv[child] = tl;
                let ti: i64 = id[root]; id[root] = id[child]; id[child] = ti;
                let tp: f32 = p[root]; p[root] = p[child]; p[child] = tp;
                root = child;
            }
        }
        s = s - 1;
    }
    var e: i64 = n - 1;
    while e > 0 {
        let tl2: f32 = lv[0]; lv[0] = lv[e]; lv[e] = tl2;
        let ti2: i64 = id[0]; id[0] = id[e]; id[e] = ti2;
        let tp2: f32 = p[0]; p[0] = p[e]; p[e] = tp2;
        var r2: i64 = 0;
        while r2 * 2 + 1 < e {
            var ch: i64 = r2 * 2 + 1;
            if ch + 1 < e && lv[ch + 1] > lv[ch] {
                ch = ch + 1;
            }
            if lv[r2] >= lv[ch] {
                r2 = e;
            } else {
                let x: f32 = lv[r2]; lv[r2] = lv[ch]; lv[ch] = x;
                let y: i64 = id[r2]; id[r2] = id[ch]; id[ch] = y;
                let z: f32 = p[r2]; p[r2] = p[ch]; p[ch] = z;
                r2 = ch;
            }
        }
        e = e - 1;
    }
}

fn sm_apply_typical(lv: f32[:], p: f32[:], id: i64[:], n: i64,
                    typical_p: f32, min_keep: i64) -> i64 {
    if n <= 0 || lv.len < n || p.len < n || id.len < n {
        return -1;
    }
    if typical_p >= 1.0 {
        return n;
    }
    // logits descending: the top-k sort is descending by logit and O(n log n)
    let srt = sm_partial_sort(lv, id, n, n);
    if srt < 0 {
        return -1;
    }
    let sm = softmax_in_place(lv, p, n, 1);
    if sm < 0.0 {
        return -1;
    }
    var i: i64 = 0;
    var entropy: f32 = 0.0;
    while i < n {
        if p[i] > 0.0 {
            entropy = entropy - p[i] * c_logf(p[i]);
        }
        i = i + 1;
    }
    i = 0;
    while i < n {
        let d: f32 = (-c_logf(p[i])) - entropy;
        lv[i] = if d < 0.0 { -d } else { d };
        i = i + 1;
    }
    // order by the shifted score, keeping lv, id and p aligned
    sort3_asc(lv, id, p, n);
    var cum: f32 = 0.0;
    i = 0;
    while i < n {
        cum = cum + p[i];
        if cum > typical_p && (min_keep <= 0 || i + 1 >= min_keep) {
            return i + 1;
        }
        i = i + 1;
    }
    return n;
}



// XTC: with the given chance, drop the prefix of tokens at or above the
// probability threshold while at least min_keep survive. The uniform value is
// passed in so this stays pure; the RNG stream is the caller's.
fn sm_apply_xtc(lv: f32[:], p: f32[:], id: i64[:], n: i64,
                probability: f32, threshold: f32, min_keep: i64, chance: f32) -> i64 {
    if probability <= 0.0 || threshold > 0.5 || n < 2 {
        return n;
    }
    if chance > probability {
        return n;
    }
    let sm = softmax_in_place(lv, p, n, 1);
    if sm < 0.0 {
        return -1;
    }
    var pos_last: i64 = 0;
    var i: i64 = 0;
    while i < n {
        if p[i] >= threshold {
            pos_last = i;
        } else {
            break;
        }
        i = i + 1;
    }
    if n - pos_last >= min_keep && pos_last > 0 {
        i = 0;
        while i < n - pos_last {
            lv[i] = lv[i + pos_last];
            id[i] = id[i + pos_last];
            p[i] = p[i + pos_last];
            i = i + 1;
        }
        return n - pos_last;
    }
    return n;
}


// penalties: repeat, frequency and presence over the recent window. The window
// arrives through the float slice (token ids are exact below 2^24) because the
// dispatch carries one integer slice, and p is free before the softmax.
fn sm_apply_penalties(lv: f32[:], id: i64[:], hist: f32[:],
                      tbl_keys: i64[:], tbl_cnt: i64[:],
                      n: i64, hn: i64, repeat: f32, freq: f32, present: f32) -> i64 {
    if hn == 0 || (repeat == 1.0 && freq == 0.0 && present == 0.0) {
        return n;
    }
    let slots: i64 = tbl_keys.len;
    if slots <= 0 || tbl_cnt.len < slots {
        return n;
    }
    var h: i64 = 0;
    while h < hn {
        let tok: i64 = hist[h] as i64;
        var slot: i64 = (tok * 2654435761) % slots;
        if slot < 0 { slot = slot + slots; }
        var probe: i64 = 0;
        while probe < slots {
            if tbl_keys[slot] == tok {
                tbl_cnt[slot] = tbl_cnt[slot] + 1;
                break;
            }
            if tbl_cnt[slot] == 0 {
                tbl_keys[slot] = tok;
                tbl_cnt[slot] = 1;
                break;
            }
            slot = slot + 1;
            if slot >= slots { slot = 0; }
            probe = probe + 1;
        }
        h = h + 1;
    }
    var i: i64 = 0;
    while i < n {
        let tok: i64 = id[i];
        var slot: i64 = (tok * 2654435761) % slots;
        if slot < 0 { slot = slot + slots; }
        var c: i64 = 0;
        var probe: i64 = 0;
        while probe < slots {
            if tbl_cnt[slot] == 0 {
                break;
            }
            if tbl_keys[slot] == tok {
                c = tbl_cnt[slot];
                break;
            }
            slot = slot + 1;
            if slot >= slots { slot = 0; }
            probe = probe + 1;
        }
        if c > 0 {
            if lv[i] <= 0.0 {
                lv[i] = lv[i] * repeat;
            } else {
                lv[i] = lv[i] / repeat;
            }
            lv[i] = lv[i] - ((c as f32) * freq + present);
        }
        i = i + 1;
    }
    return n;
}


// ---- DRY (steps 1-4) -------------------------------------------------------
// step 1: limit the repetition window by the nearest restart sequence
fn sm_dry_rep_limit(last: i64[:], b_head: i64[:], b_ts: i64[:], b_tl: i64[:],
                    b_tail: i64[:], n: i64, nb: i64) -> i64 {
    var rep_limit: i64 = n;
    var i: i64 = 0;
    while i < n {
        let tok: i64 = last[n - 1 - i];
        var longest: i64 = -1;
        var b: i64 = 0;
        while b < nb {
            if b_head[b] == tok {
                let sl: i64 = b_tl[b];
                if sl > longest && sl <= i {
                    var ok: bool = true;
                    var off: i64 = 0;
                    while off < sl {
                        if b_tail[b_ts[b] + off] != last[n - 1 - (i - off - 1)] {
                            ok = false;
                            break;
                        }
                        off = off + 1;
                    }
                    if ok {
                        longest = sl;
                    }
                }
            }
            b = b + 1;
        }
        if longest >= 0 {
            rep_limit = i - longest;
            break;
        }
        i = i + 1;
    }
    return rep_limit;
}

// step 2: reverse Z-algorithm repeat counts
fn sm_dry_repeat_counts(last: i64[:], rc: i64[:], n: i64, rep_limit: i64) -> i64 {
    let lastidx: i64 = n - 1;
    var rt: i64 = 0;
    var lt: i64 = 0;
    var k: i64 = 1;
    while k < n {
        if k > rt {
            var m: i64 = 0;
            while m + k < n && last[n - 1 - m] == last[n - 1 - (m + k)] {
                m = m + 1;
            }
            var v: i64 = m;
            if v > rep_limit { v = rep_limit; }
            rc[lastidx - k] = v;
            if m > 0 {
                lt = k;
                rt = k + m - 1;
            }
        } else {
            let pp: i64 = k - lt;
            let rpl: i64 = rt - k + 1;
            if rc[lastidx - pp] < rpl {
                var v: i64 = rc[lastidx - pp];
                if v > rep_limit { v = rep_limit; }
                rc[lastidx - k] = v;
            } else {
                var i2: i64 = rt + 1;
                while i2 < n && last[n - 1 - i2] == last[n - 1 - (i2 - k)] {
                    i2 = i2 + 1;
                }
                var v: i64 = i2 - k;
                if v > rep_limit { v = rep_limit; }
                rc[lastidx - k] = v;
                lt = k;
                rt = i2 - 1;
            }
        }
        k = k + 1;
    }
    return 0;
}

// steps 3-4: penalties from the maximum repeat of each candidate
fn sm_apply_dry(last: i64[:], rc: i64[:], lv: f32[:], id: i64[:],
                br_head: i64[:], br_tl: i64[:],
                n_last: i64, nc: i64, allowed: i64, nb: i64,
                multiplier: f32, base: f32) -> i64 {
    var max_exp_f: f32 = 0.0;
    if base > 1.000001 {
        max_exp_f = 88.7228391 / logf(base);
    }
    var c: i64 = 0;
    while c < nc {
        let tok: i64 = id[c];
        var max_rep: i64 = -1;
        var i: i64 = 0;
        while i < n_last - 1 {
            if rc[i] >= allowed {
                if last[n_last - 2 - i] == tok {
                    if rc[i] > max_rep { max_rep = rc[i]; }
                }
            }
            i = i + 1;
        }
        if max_rep >= 0 {
            var single: bool = false;
            var b: i64 = 0;
            while b < nb {
                if br_head[b] == tok && br_tl[b] == 0 {
                    single = true;
                    break;
                }
                b = b + 1;
            }
            if !single {
                var e_f: f32 = 0.0;
                var t: i64 = 0;
                while t < max_rep - allowed {
                    if max_exp_f > 0.0 && e_f + 1.0 > max_exp_f {
                        break;
                    }
                    e_f = e_f + 1.0;
                    t = t + 1;
                }
                let penalty: f32 = multiplier * powf(base, e_f);
                lv[c] = lv[c] - penalty;
            }
        }
        c = c + 1;
    }
    return nc;
}

// single entry for the whole DRY stage
fn fg_sampler_dry(last: i64[:], rc: i64[:], br_head: i64[:], br_ts: i64[:],
                  br_tl: i64[:], br_tail: i64[:], lv: f32[:], id: i64[:],
                  n_last: i64, nc: i64, allowed: i64, nb: i64,
                  multiplier: f32, base: f32) -> i64 {
    if multiplier == 0.0 || base < 1.0 || n_last == 0 {
        return nc;
    }
    let rl = sm_dry_rep_limit(last, br_head, br_ts, br_tl, br_tail, n_last, nb);
    if rl < allowed {
        return nc;
    }
    let rrc = sm_dry_repeat_counts(last, rc, n_last, rl);
    if rrc < 0 {
        return -1;
    }
    return sm_apply_dry(last, rc, lv, id, br_head, br_tl, n_last, nc, allowed, nb, multiplier, base);
}


// adaptive-p: clamped EMA target and the probability transform
fn sm_adaptive_target(target: f32, weighted_sum: f32, total_weight: f32, out: f32[:]) -> i64 {
    var t: f32 = target;
    if t < 0.0 { t = 0.0; }
    if t > 1.0 { t = 1.0; }
    var v: f32 = t;
    if total_weight != 0.0 {
        v = 2.0 * t - (weighted_sum / total_weight);
    }
    if v < 0.0 { v = 0.0; }
    if v > 1.0 { v = 1.0; }
    out[0] = v;
    return 0;
}

// compute the adapted target from the EMA state and apply the transform, so the
// probability array is never used as scratch
fn sm_apply_adaptive_p_state(lv: f32[:], p: f32[:], id: i64[:], n: i64,
                             target: f32, weighted_sum: f32, total_weight: f32) -> i64 {
    var t: f32 = target;
    if t < 0.0 { t = 0.0; }
    if t > 1.0 { t = 1.0; }
    var adapted: f32 = t;
    if total_weight != 0.0 {
        adapted = 2.0 * t - (weighted_sum / total_weight);
    }
    if adapted < 0.0 { adapted = 0.0; }
    if adapted > 1.0 { adapted = 1.0; }
    return sm_apply_adaptive_p(lv, p, id, n, adapted);
}

fn sm_apply_adaptive_p(lv: f32[:], p: f32[:], id: i64[:], n: i64, adapted: f32) -> i64 {
    let inv_width: f32 = 1.0 / 0.3;
    var i: i64 = 0;
    while i < n {
        if lv[i] < -1.0e38 {
            i = i + 1;
            continue;
        }
        var dist: f32 = (p[i] - adapted) * inv_width;
        if dist < 0.0 { dist = -dist; }
        lv[i] = 5.0 - (10.0 * dist * dist) / (1.0 + dist);
        i = i + 1;
    }
    let sm = softmax_in_place(lv, p, n, 0);
    if sm < 0.0 {
        return -1;
    }
    return n;
}

// mirostat v1: s_hat estimate and the truncation size
fn sm_mirostat_k(p: f32[:], n: i64, mu: f32, m: i64, n_vocab: f32) -> i64 {
    var sum_ti_bi: f32 = 0.0;
    var sum_ti_sq: f32 = 0.0;
    var limit: i64 = m - 1;
    if limit > n - 1 { limit = n - 1; }
    var i: i64 = 0;
    while i < limit {
        let t_i: f32 = logf((i + 2) * 1.0 / (i + 1));
        let b_i: f32 = logf(p[i] / p[i + 1]);
        sum_ti_bi = sum_ti_bi + t_i * b_i;
        sum_ti_sq = sum_ti_sq + t_i * t_i;
        i = i + 1;
    }
    let s_hat: f32 = sum_ti_bi / sum_ti_sq;
    let eps: f32 = s_hat - 1.0;
    let k_f: f32 = powf((eps * powf(2.0, mu)) / (1.0 - powf(n_vocab, -eps)), 1.0 / s_hat);
    var ki: i64 = 0;
    while ki + 1 <= k_f && ki < 1000000 {
        ki = ki + 1;
    }
    if ki < 1 { ki = 1; }
    return ki;
}

fn sm_mirostat_update(mu: f32, eta: f32, tau: f32, observed: f32) -> f32 {
    return mu - eta * (observed - tau);
}


// descending/ascending heap sort over four parallel arrays, ordered by the first
fn swap4(scr: f32[:], lv: f32[:], id: i64[:], p: f32[:], a: i64, b: i64) {
    let t0: f32 = scr[a]; scr[a] = scr[b]; scr[b] = t0;
    let t1: f32 = lv[a];  lv[a]  = lv[b];  lv[b]  = t1;
    let t2: i64 = id[a];  id[a]  = id[b];  id[b]  = t2;
    let t3: f32 = p[a];   p[a]   = p[b];   p[b]   = t3;
}

fn sift4(scr: f32[:], lv: f32[:], id: i64[:], p: f32[:], root_in: i64, end: i64) {
    var r: i64 = root_in;
    while r * 2 + 1 < end {
        var c: i64 = r * 2 + 1;
        if c + 1 < end && scr[c + 1] > scr[c] {
            c = c + 1;
        }
        if scr[r] >= scr[c] {
            return;
        }
        swap4(scr, lv, id, p, r, c);
        r = c;
    }
}

// ascending order by scr, carrying lv, id and p along
fn sort4_asc(scr: f32[:], lv: f32[:], id: i64[:], p: f32[:], n: i64) {
    var s: i64 = n / 2 - 1;
    while s >= 0 {
        sift4(scr, lv, id, p, s, n);
        s = s - 1;
    }
    var e: i64 = n - 1;
    while e > 0 {
        swap4(scr, lv, id, p, 0, e);
        sift4(scr, lv, id, p, 0, e);
        e = e - 1;
    }
}

// typical-p with the caller's logits left intact: the shifted scores live in
// scr, and the four arrays move together so logits keep their own values
fn fg_sampler_typical(lv: f32[:], p: f32[:], id: i64[:], scr: f32[:], n: i64,
                      typical_p: f32, min_keep: i64) -> i64 {
    if n <= 0 || lv.len < n || p.len < n || id.len < n || scr.len < n {
        return -1;
    }
    if typical_p >= 1.0 {
        return n;
    }
    let srt = sm_partial_sort(lv, id, n, n);
    if srt < 0 {
        return -1;
    }
    let sm = softmax_in_place(lv, p, n, 1);
    if sm < 0.0 {
        return -1;
    }
    var i: i64 = 0;
    var entropy: f32 = 0.0;
    while i < n {
        if p[i] > 0.0 {
            entropy = entropy - p[i] * c_logf(p[i]);
        }
        i = i + 1;
    }
    i = 0;
    while i < n {
        let d: f32 = (-c_logf(p[i])) - entropy;
        scr[i] = if d < 0.0 { -d } else { d };
        i = i + 1;
    }
    sort4_asc(scr, lv, id, p, n);
    var cum: f32 = 0.0;
    i = 0;
    while i < n {
        cum = cum + p[i];
        if cum > typical_p && (min_keep <= 0 || i + 1 >= min_keep) {
            return i + 1;
        }
        i = i + 1;
    }
    return n;
}


// top-p as the real path uses it: sort by logit descending first (the logits
// keep their values), then keep the smallest prefix whose mass reaches top_p
fn fg_sampler_top_p(lv: f32[:], p: f32[:], id: i64[:], n: i64,
                    top_p: f32, min_keep: i64, marker: i64) -> i64 {
    if n <= 0 || lv.len < n || p.len < n || id.len < n || marker != 171 {
        return -1;
    }
    if top_p >= 1.0 {
        return n;
    }
    let srt = sm_partial_sort(lv, id, n, n);
    if srt < 0 {
        return -1;
    }
    let sm = softmax_in_place(lv, p, n, 1);
    if sm < 0.0 {
        return -1;
    }
    var cum: f32 = 0.0;
    var i: i64 = 0;
    while i < n {
        cum = cum + p[i];
        if cum >= top_p && i + 1 >= min_keep {
            return i + 1;
        }
        i = i + 1;
    }
    return n;
}


// XTC entry for the real path: the caller passes the uniform value it drew, so
// the RNG stream stays with the caller and this stays pure.
fn fg_sampler_xtc(lv: f32[:], p: f32[:], id: i64[:], n: i64,
                  probability: f32, threshold: f32, min_keep: i64, chance: f32,
                  marker: i64) -> i64 {
    if marker != 172 {
        return -1;
    }
    return sm_apply_xtc(lv, p, id, n, probability, threshold, min_keep, chance);
}


// penalties entry: the recent window arrives as exact token ids in a float slice
fn fg_sampler_penalties(lv: f32[:], id: i64[:], hist: f32[:],
                        tbl_keys: i64[:], tbl_cnt: i64[:],
                        n: i64, hn: i64, repeat: f32, freq: f32, present: f32) -> i64 {
    return sm_apply_penalties(lv, id, hist, tbl_keys, tbl_cnt, n, hn, repeat, freq, present);
}

// adaptive-p entry: the EMA state arrives as scalars, probabilities are scaled and
// renormalised exactly as the native body does
fn fg_sampler_adaptive_p(lv: f32[:], p: f32[:], id: i64[:], n: i64,
                         target: f32, weighted_sum: f32, total_weight: f32,
                         marker: i64) -> i64 {
    if marker != 174 {
        return -1;
    }
    return sm_apply_adaptive_p_state(lv, p, id, n, target, weighted_sum, total_weight);
}

// mirostat entry: the truncation size from the s_hat estimate; the draw and the
// EMA update stay with the caller so the RNG stream is untouched
fn fg_sampler_mirostat(p: f32[:], n: i64, mu: f32, m: i64, n_vocab: f32, marker: i64) -> i64 {
    if marker != 175 {
        return -1;
    }
    return sm_mirostat_k(p, n, mu, m, n_vocab);
}

// dist: std::discrete_distribution over the probabilities, without storing a
// computed f64 (the stored form reads as zero on this compiler): the two 32-bit
// words stay as i64 and the comparison is a single expression.
fn fg_sampler_dist(p: f32[:], n: i64, state: i64[:], marker: i64) -> i64 {
    if marker != 176 || n <= 0 {
        return -1;
    }
    var sum: f64 = 0.0;
    var i: i64 = 0;
    while i < n {
        sum = sum + (p[i] as f64);
        i = i + 1;
    }
    let w1: i64 = sm_mt_next(state);
    let w2: i64 = sm_mt_next(state);
    var prefix: f64 = 0.0;
    i = 0;
    while i < n {
        prefix = prefix + (p[i] as f64);
        if (prefix / sum) * 18446744073709551616.0 >= (w1 as f64) + (w2 as f64) * 4294967296.0 {
            return i;
        }
        i = i + 1;
    }
    return n - 1;
}

// Single stable entry point for the shipped library. Generated function ids
// shift whenever this module changes, so callers must reach the chain through
// this one function rather than through a generated id.
fn fg_sampler_chain(op: i64, lv: f32[:], p: f32[:], id: i64[:], n: i64,
                    f: f32, k: i64, f2: f32, f3: f32) -> i64 {
    if op == 0 {
        return sm_apply_typical(lv, p, id, n, f, k);
    }
    if op == 12 {
        return fg_sampler_typical(lv, p, id, lv, n, f, k);
    }
    if op == 1 {
        return sm_apply_top_p(lv, p, n, f, k);
    }
    if op == 2 {
        return sm_apply_top_k(lv, id, n, k);
    }
    if op == 3 {
        return sm_apply_min_p(lv, n, f, k);
    }
    if op == 4 {
        return sm_apply_temp(lv, n, f);
    }
    if op == 5 {
        return sm_apply_xtc(lv, p, id, n, f, f2, k, f3);
    }
    if op == 6 {
        return sm_apply_penalties(lv, id, p, id, id, n, k, f, f2, f3);
    }
    if op == 7 {
        return sm_apply_adaptive_p_state(lv, p, id, n, f, f2, f3);
    }
    if op == 8 {
        return sm_mirostat_k(lv, n, f, k, f2);
    }
    if op == 9 {
        p[0] = sm_mirostat_update(f, f2, f3, lv[0]);
        return 0;
    }
    if op == 10 {
        return sm_mt_seed(id, k);
    }
    if op == 11 {
        return sm_mt_next(id);
    }
    return -2;
}
