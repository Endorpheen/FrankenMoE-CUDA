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

fn sift3(lv: f32[:], id: i64[:], p: f32[:], root_in: i64, end: i64) {
    var root: i64 = root_in;
    while 2 * root + 1 < end {
        var c: i64 = 2 * root + 1;
        if c + 1 < end && lv[c] < lv[c + 1] { c = c + 1; }
        if lv[root] >= lv[c] { return; }
        let tl: f32 = lv[root]; lv[root] = lv[c]; lv[c] = tl;
        let ti: i64 = id[root]; id[root] = id[c]; id[c] = ti;
        let tp: f32 = p[root]; p[root] = p[c]; p[c] = tp;
        root = c;
    }
}

// ascending by lv: build a max-heap, then move the root to the tail
fn sort3(lv: f32[:], id: i64[:], p: f32[:], n: i64) {
    var start: i64 = (n - 2) / 2;
    while start >= 0 {
        sift3(lv, id, p, start, n);
        start = start - 1;
    }
    var end: i64 = n - 1;
    while end > 0 {
        let tl: f32 = lv[0]; lv[0] = lv[end]; lv[end] = tl;
        let ti: i64 = id[0]; id[0] = id[end]; id[end] = ti;
        let tp: f32 = p[0]; p[0] = p[end]; p[end] = tp;
        sift3(lv, id, p, 0, end);
        end = end - 1;
    }
}

// typical-p: keep the tokens whose surprisal is closest to the entropy of the
// distribution, honouring min_keep. Mirrors llama_sampler_typical_apply:
// sort by logit desc, softmax, entropy, shifted = |(-log p) - entropy|, order by
// shifted ascending, cumulative until > typical_p and i >= min_keep-1, then
// leave the kept set in shifted order (unsorted flag at the caller).
// Sorting is a heap sort (O(n log n)) over the three parallel arrays.
fn sm_apply_typical(lv: f32[:], p: f32[:], id: i64[:], n: i64,
                           typical_p: f32, min_keep: i64) -> i64 {
    if n <= 0 || lv.len < n || p.len < n || id.len < n {
        return -1;
    }
    if typical_p >= 1.0 {
        return n;
    }
    // sort by logit descending (lv, id, p move together)
    var i: i64 = 1;
    while i < n {
        var j: i64 = i;
        while j > 0 {
            if lv[j - 1] >= lv[j] {
                break;
            }
            let tl: f32 = lv[j - 1]; lv[j - 1] = lv[j]; lv[j] = tl;
            let ti: i64 = id[j - 1]; id[j - 1] = id[j]; id[j] = ti;
            j = j - 1;
        }
        i = i + 1;
    }
    softmax_in_place(lv, p, n, 1);
    var entropy: f32 = 0.0;
    i = 0;
    while i < n {
        entropy = entropy - p[i] * c_logf(p[i]);
        i = i + 1;
    }
    i = 0;
    while i < n {
        let d: f32 = (-c_logf(p[i])) - entropy;
        lv[i] = if d < 0.0 { -d } else { d };
        i = i + 1;
    }
    // ascending by shifted score; lv, id and p move together so the
    // cumulative below stays aligned with the order
    sort3(lv, id, p, n);
    var cum: f32 = 0.0;
    var last: i64 = n;
    i = 0;
    while i < n {
        cum = cum + p[i];
        if cum > typical_p && (min_keep <= 0 || i + 1 >= min_keep) {
            last = i + 1;
            break;
        }
        i = i + 1;
    }
    return last;
}

// gate entry: typical-p on caller arrays, returns the new size
fn gate_typical(lv: f32[:], p: f32[:], id: i64[:], n: i64, tp: f32, mk: i64) -> i64 {
    return sm_apply_typical(lv, p, id, n, tp, mk);
}


// probe: expose the module's top-k sort so its order can be checked directly
fn gate_sort(lv: f32[:], id: i64[:], n: i64) -> i64 {
    return sm_partial_sort(lv, id, n, n);
}


fn sm_apply_top_p_unsorted(lv: f32[:], p: f32[:], id: i64[:], n: i64, top_p: f32, min_keep: i64) -> i64 {
    if n <= 0 || lv.len < n || p.len < n || id.len < n {
        return -1;
    }
    if top_p >= 1.0 {
        return n;
    }
    var max_l: f32 = lv[0];
    var i: i64 = 1;
    while i < n {
        if lv[i] > max_l { max_l = lv[i]; }
        i = i + 1;
    }
    var z: f32 = 0.0;
    i = 0;
    while i < n {
        z = z + c_expf(lv[i] - max_l);
        i = i + 1;
    }
    let sorted_rc = sm_partial_sort(lv, id, n, n);
    if sorted_rc < 0 {
        return -1;
    }
    var cum: f32 = 0.0;
    i = 0;
    while i < n {
        let pi: f32 = c_expf(lv[i] - max_l) / z;
        cum = cum + pi;
        if cum >= top_p && i + 1 >= min_keep {
            return i + 1;
        }
        i = i + 1;
    }
    return n;
}

fn gate_top_p(lv: f32[:], p: f32[:], id: i64[:], n: i64, tp: f32, mk: i64) -> i64 {
    return sm_apply_top_p_unsorted(lv, p, id, n, tp, mk);
}


// single exported entry: op 0 typical-p, op 1 unsorted top-p
fn gate_dispatch(op: i64, lv: f32[:], p: f32[:], id: i64[:], n: i64, f: f32, k: i64, f2: f32, f3: f32) -> i64 {
    if op == 0 {
        return sm_apply_typical(lv, p, id, n, f, k);
    }
    if op == 1 {
        return sm_apply_top_p_unsorted(lv, p, id, n, f, k);
    }
    if op == 2 {
        return sm_apply_xtc(lv, p, id, n, f, f2, k, f3);
    }
    return -2;
}

// xtc: with probability `probability` (the caller draws the uniform value),
// drop the prefix of tokens whose probability is at or above `threshold`, as
// long as a tail of at least min_keep tokens remains.
fn sm_apply_xtc(lv: f32[:], p: f32[:], id: i64[:], n: i64,
                probability: f32, threshold: f32, min_keep: i64, chance: f32) -> i64 {
    if probability <= 0.0 || threshold > 0.5 || n < 2 {
        return n;
    }
    if chance > probability {
        return n;
    }
    softmax_in_place(lv, p, n, 1);
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

// dry step 1: limit the repetition window by the nearest restart sequence.
// last[] is the context in natural order; rat(i) = last[n-1-i].
// breakers are flattened: head, tail start, tail length, tail tokens.
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

fn gate_dry(last: i64[:], b_head: i64[:], b_ts: i64[:], b_tl: i64[:], b_tail: i64[:], n: i64, nb: i64) -> i64 {
    return sm_dry_rep_limit(last, b_head, b_ts, b_tl, b_tail, n, nb);
}

// dry step 2: reverse Z-algorithm over the context, repeat counts per position
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

fn gate_dry2(last: i64[:], rc: i64[:], n: i64, rep_limit: i64) -> i64 {
    return sm_dry_repeat_counts(last, rc, n, rep_limit);
}

// dry steps 3-4: turn repeat counts into logit penalties. The maximum repeat
// for a candidate token is computed by scanning the context, which is
// equivalent to the native per-token map and needs no container.
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
    return 0;
}

fn gate_dry3(last: i64[:], rc: i64[:], lv: f32[:], id: i64[:], br_head: i64[:], br_tl: i64[:],
             n_last: i64, nc: i64, allowed: i64, nb: i64, multiplier: f32, base: f32) -> i64 {
    return sm_apply_dry(last, rc, lv, id, br_head, br_tl, n_last, nc, allowed, nb, multiplier, base);
}
fn main() {
    print("p6-10 typical gate");
    let lv: f32[4] = [0.0, 0.0, 0.0, 0.0];
    let p: f32[4] = [0.0, 0.0, 0.0, 0.0];
    let id: i64[4] = [0, 1, 2, 3];
    let ignored = gate_typical(lv, p, id, 4, 1.0, 0);
    let ignored3 = gate_sort(lv, id, 4);
    let ignored4 = gate_top_p(lv, p, id, 4, 1.0, 0);
    let ignored5 = gate_dispatch(9, lv, p, id, 4, 1.0, 0, 0.0, 0.0);
    let ignored6 = gate_dry(id, id, id, id, id, 4, 0);
    let ignored7 = gate_dry2(id, id, 4, 4);
    let ignored8 = gate_dry3(id, id, lv, id, id, id, 4, 4, 1, 0, 1.0, 1.5);
}
