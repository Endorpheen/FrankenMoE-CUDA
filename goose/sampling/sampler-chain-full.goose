// Bound to the C symbol under a Goose-only name: the composed unit also carries the standard
// library's overloaded `exp`, and two exact matches for an f64 argument made every call ambiguous
// (measured twice, in both directions). One distinct name removes the clash.
extern "gs_exp" fn fg_exp(x: f64) -> f64;
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

export fn fg_sampler_greedy(lv: f32[:], n: i64) -> i64 {
    return sm_greedy(lv, n);
}

fn sm_exponentials(lv: f32[:], p: f32[:], n: i64, sorted: i64) -> f32 {
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
    return cum;
}

fn softmax_in_place(lv: f32[:], p: f32[:], n: i64, sorted: i64) -> f64 {
    let cum = sm_exponentials(lv, p, n, sorted);
    var i: i64 = 0;
    while i < n {
        p[i] = p[i] / cum;
        i = i + 1;
    }
    return cum as f64;
}

// Full chain on a candidate array that starts as the whole vocabulary.
// ip: 0 top_k, 1 min_keep, 2 use_dist (0 = greedy after the temperature).
// fp: 0 top_p, 1 min_p, 2 temp, 3 typical_p.
// out: 0 selected position, 1 token id, 2 surviving candidate count.
fn sm_select(lv: f32[:], id: i64[:], p: f32[:], typical_work: f32[:], n_in: i64,
                    ip: const i64[:], fp: const f32[:], state: i64[:], out: i64[:]) -> i64 {
    if ip.len < 3 || fp.len < 4 || out.len < 3 || lv.len < n_in || id.len < n_in || p.len < n_in || n_in <= 0 {
        return -1;
    }
    let k: i64 = ip[0];
    let min_keep: i64 = ip[1];
    let use_dist: i64 = ip[2];
    let top_p: f32 = fp[0];
    let min_p: f32 = fp[1];
    let temp: f32 = fp[2];
    let typical: f32 = fp[3];

    var n: i64 = n_in;
    var i: i64 = 0;
    while i < n {
        id[i] = i;
        i = i + 1;
    }

    // top_k
    var sorted: i64 = 0;
    if k > 0 || (typical >= 1.0 && (top_p < 1.0 || min_p > 0.0)) {
        let kk: i64 = if k > 0 && k < n { k } else { n };
        if sm_partial_sort(lv, id, n, kk) < 0 {
            return -1;
        }
        n = kk;
        sorted = 1;
    }

    if typical < 1.0 {
        n = sm_typical_filter(lv, p, id, typical_work, n, typical, min_keep, sorted);
        if n <= 0 { return -1; }
        sorted = 0;
    }

    // Restore logit order for the probability cuts after typical sampling.
    if sorted == 0 && (top_p < 1.0 || min_p > 0.0) {
        if sm_partial_sort(lv, id, n, n) < 0 { return -1; }
        sorted = 1;
    }
    n = sm_apply_top_p(lv, p, n, top_p, min_keep);
    n = sm_apply_min_p(lv, n, min_p, min_keep);
    if n <= 0 { return -1; }

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

// Match the server's pre-sampling probabilities: partially sort the raw
// vocabulary, then accumulate the F32 softmax in that same order.
fn sm_pre_sample_logprobs(logits: f32[:], work_ids: i64[:], work_p: f32[:],
                          selected: i64, top: i64, ids: i64[:], logp: f32[:]) -> bool {
    if top <= 0 || top > logits.len || ids.len < top + 1 || logp.len < top + 1 ||
       sm_apply_top_k(logits, work_ids, logits.len, top) != top {
        return false;
    }
    let sum = sm_exponentials(logits, work_p, logits.len, 1);
    if !(sum > 0.0) { return false; }
    ids[0] = selected;
    var found = false;
    for i in logits.len {
        if i < top {
            ids[i + 1] = work_ids[i];
            logp[i + 1] = sm_report_logprob(work_p[i] / sum);
        }
        if work_ids[i] == selected {
            logp[0] = sm_report_logprob(work_p[i] / sum);
            found = true;
        }
        if found && i >= top - 1 { break; }
    }
    return found;
}

fn sm_report_logprob(p: f32) -> f32 {
    return if p == 0.0 { -3.4028234663852886e38 } else { c_logf(p) };
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
fn sm_repeat_logit(logit: f32, repeat: f32) -> f32 {
    return if logit <= 0.0 { logit * repeat } else { logit / repeat };
}

fn sm_repeat_marked(lv: f32[:], recent: const i64[:], seen: u8[:], repeat: f32) {
    for i in recent.len {
        let token = recent[i];
        if seen[token] == 0 {
            lv[token] = sm_repeat_logit(lv[token], repeat);
            seen[token] = 1;
        }
    }
}

// The request owns validated integer IDs and an exact token-indexed logit array.
// Restore only the marks that the two suffixes touched.
fn sm_repeat_history(lv: f32[:], prompt: const i64[:], output: const i64[:], seen: u8[:], repeat: f32) {
    sm_repeat_marked(lv, prompt, seen, repeat);
    sm_repeat_marked(lv, output, seen, repeat);
    for i in prompt.len { seen[prompt[i]] = 0; }
    for i in output.len { seen[output[i]] = 0; }
}

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
            lv[i] = sm_repeat_logit(lv[i], repeat) - ((c as f32) * freq + present);
        }
        i = i + 1;
    }
    return n;
}


// ---- DRY (steps 1-4) -------------------------------------------------------
// step 1: limit the repetition window by the nearest restart sequence
// Exported door for the DRY repetition limit, so the differential gate measures the
// shipped decision instead of re-implementing it in the harness.
export fn fg_dry_rep_limit_entry(last: i64[:], b_head: i64[:], b_ts: i64[:], b_tl: i64[:],
                                 b_tail: i64[:], n: i64, nb: i64) -> i64 {
    return sm_dry_rep_limit(last, b_head, b_ts, b_tl, b_tail, n, nb);
}

fn sm_dry_rep_limit(last: i64[:], b_head: i64[:], b_ts: i64[:], b_tl: i64[:],
                    b_tail: i64[:], n: i64, nb: i64) -> i64 {
    var rep_limit: i64 = n;
    var i: i64 = 0;
    while i < n {
        let tok: i64 = last[i];
        var longest: i64 = -1;
        var b: i64 = 0;
        while b < nb {
            if b_head[b] == tok {
                let sl: i64 = b_tl[b];
                if sl > longest && sl <= i {
                    var ok: bool = true;
                    var off: i64 = 0;
                    while off < sl {
                        if b_tail[b_ts[b] + off] != last[i - off - 1] {
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
            while m + k < n && last[m] == last[m + k] {
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
                while i2 < n && last[i2] == last[i2 - k] {
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
export fn fg_sampler_dry(last: i64[:], rc: i64[:], br_head: i64[:], br_ts: i64[:],
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
// The exported form of fg_sampler_typical cannot be resolved by the compiler
// (TD-S9); this thin door gives the host a stable name.
export fn fg_typical_entry(lv: f32[:], p: f32[:], id: i64[:], scr: f32[:], n: i64,
                           typical_p: f32, min_keep: i64) -> i64 {
    return fg_sampler_typical(lv, p, id, scr, n, typical_p, min_keep);
}

fn fg_sampler_typical(lv: f32[:], p: f32[:], id: i64[:], scr: f32[:], n: i64,
                      typical_p: f32, min_keep: i64) -> i64 {
    return sm_typical_filter(lv, p, id, scr, n, typical_p, min_keep, 0);
}

fn sm_typical_filter(lv: f32[:], p: f32[:], id: i64[:], scr: f32[:], n: i64,
                     typical_p: f32, min_keep: i64, sorted: i64) -> i64 {
    if n <= 0 || lv.len < n || p.len < n || id.len < n || scr.len < n {
        return -1;
    }
    if typical_p >= 1.0 {
        return n;
    }
    if sorted == 0 && sm_partial_sort(lv, id, n, n) < 0 { return -1; }
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
export fn fg_sampler_top_p(lv: f32[:], p: f32[:], id: i64[:], n: i64,
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
export fn fg_sampler_xtc(lv: f32[:], p: f32[:], id: i64[:], n: i64,
                  probability: f32, threshold: f32, min_keep: i64, chance: f32,
                  marker: i64) -> i64 {
    if marker != 172 {
        return -1;
    }
    return sm_apply_xtc(lv, p, id, n, probability, threshold, min_keep, chance);
}


// penalties entry: the recent window arrives as exact token ids in a float slice
export fn fg_sampler_penalties(lv: f32[:], id: i64[:], hist: f32[:],
                        tbl_keys: i64[:], tbl_cnt: i64[:],
                        n: i64, hn: i64, repeat: f32, freq: f32, present: f32) -> i64 {
    return sm_apply_penalties(lv, id, hist, tbl_keys, tbl_cnt, n, hn, repeat, freq, present);
}

// adaptive-p entry: the EMA state arrives as scalars, probabilities are scaled and
// renormalised exactly as the native body does
export fn fg_sampler_adaptive_p(lv: f32[:], p: f32[:], id: i64[:], n: i64,
                         target: f32, weighted_sum: f32, total_weight: f32,
                         marker: i64) -> i64 {
    // The caller's array may already be flagged sorted, and the native normalisation trusts that
    // flag: it uses data[0] as the maximum instead of scanning. The final softmax here has to make
    // the same choice, otherwise the two paths differ by one rounding step - measured as exactly one
    // bit in one probability, which flipped a draw in 21 of 100 rounds.
    if marker != 174 {
        return -1;
    }
    return sm_apply_adaptive_p_state(lv, p, id, n, target, weighted_sum, total_weight);
}

// mirostat entry: the truncation size from the s_hat estimate; the draw and the
// EMA update stay with the caller so the RNG stream is untouched
export fn fg_sampler_mirostat(p: f32[:], n: i64, mu: f32, m: i64, n_vocab: f32, marker: i64) -> i64 {
    if marker != 175 {
        return -1;
    }
    return sm_mirostat_k(p, n, mu, m, n_vocab);
}

// dist: std::discrete_distribution over the probabilities, without storing a
// computed f64 (the stored form reads as zero on this compiler): the two 32-bit
// words stay as i64 and the comparison is a single expression.
export fn fg_sampler_dist(p: f32[:], n: i64, state: i64[:], marker: i64) -> i64 {
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

// dist with the two words supplied by the caller, so the RNG stream stays with the
// host and this function is pure
export fn fg_sampler_dist_words(p: f32[:], n: i64, w1: i64, w2: i64, marker: i64) -> i64 {
    if marker != 177 || n <= 0 {
        return -1;
    }
    var sum: f64 = 0.0;
    var i: i64 = 0;
    while i < n {
        sum = sum + (p[i] as f64);
        i = i + 1;
    }
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

// dist sampler exactly as llama_sampler_dist_apply computes it: float expf
// probabilities, double accumulation, and a double draw formed from the two words
// the caller obtained from its generator
export fn fg_sampler_dist_pick(lv: f32[:], p: f32[:], n: i64, w1: i64, w2: i64, marker: i64) -> i64 {
    if marker != 178 || n <= 0 {
        return -1;
    }
    var max_l: f32 = lv[0];
    var i: i64 = 1;
    while i < n {
        if lv[i] > max_l { max_l = lv[i]; }
        i = i + 1;
    }
    var sum_cum: f64 = 0.0;
    i = 0;
    while i < n {
        let pv: f32 = c_expf(lv[i] - max_l);
        p[i] = pv;
        sum_cum = sum_cum + (pv as f64);
        i = i + 1;
    }
    let rnd: f64 = ((w1 as f64) + (w2 as f64) * 4294967296.0) / 18446744073709551616.0;
    let sum_tgt: f64 = sum_cum * rnd;
    var sum_run: f64 = 0.0;
    var sel: i64 = -1;
    i = 0;
    while i < n {
        if sel < 0 {
            sum_run = sum_run + (p[i] as f64);
            if sum_run >= sum_tgt {
                sel = i;
            }
        }
        p[i] = ((p[i] as f64) / sum_cum) as f32;
        i = i + 1;
    }
    if sel < 0 { sel = n - 1; }
    return sel;
}

// temp_ext: dynamic temperature from the entropy of the candidate set, then a
// double-precision renormalisation. The exp result is not stored in an f64 local
// (that form reads as zero on this compiler); it is used inside the expressions.
export fn fg_sampler_temp_ext(lv: f32[:], p: f32[:], n: i64,
                       temp: f32, delta: f32, exponent: f32, marker: i64) -> i64 {
    if marker != 179 || n <= 0 {
        return -1;
    }
    if delta <= 0.0 || n <= 1 {
        return n;
    }
    var min_temp: f32 = temp - delta;
    if min_temp < 0.0 { min_temp = 0.0; }
    let max_temp: f32 = temp + delta;
    let sm = softmax_in_place(lv, p, n, 1);
    if sm < 0.0 {
        return -1;
    }
    let max_entropy: f32 = 0.0 - logf(1.0 / (n as f32));
    var entropy: f32 = 0.0;
    var i: i64 = 0;
    while i < n {
        if p[i] > 0.0 {
            entropy = entropy - p[i] * logf(p[i]);
        }
        i = i + 1;
    }
    let norm_ent: f32 = entropy / max_entropy;
    let dyn_temp: f32 = min_temp + (max_temp - min_temp) * powf(norm_ent, exponent);
    if dyn_temp <= 0.0 {
        i = 0;
        while i < n {
            lv[i] = -3.4028235e38;
            i = i + 1;
        }
        lv[0] = 0.0;
    } else {
        i = 0;
        while i < n {
            lv[i] = lv[i] / dyn_temp;
            i = i + 1;
        }
    }
    let max_l: f64 = lv[0] as f64;
    var cum: f64 = 0.0;
    i = 0;
    while i < n {
        p[i] = fg_exp((lv[i] as f64) - max_l) as f32;
        cum = cum + fg_exp((lv[i] as f64) - max_l);
        i = i + 1;
    }
    i = 0;
    while i < n {
        p[i] = ((p[i] as f64) / cum) as f32;
        i = i + 1;
    }
    return n;
}


// ---- grammar automaton (gated in EXP-2026-11-18-1110) ---------------------
// Character class matching over the flat element arrays.
// Result encoding: matched -> (next_pos + 1), unmatched -> -(next_pos + 1),
// so the caller learns both the verdict and where the class ends.
fn g_match_char(etype: i64[:], eval: i64[:], pos_in: i64, chr: i64) -> i64 {
    var pos: i64 = pos_in;
    let t0: i64 = etype[pos];
    let positive: bool = t0 == 3 || t0 == 7;
    var found: bool = false;
    var done: bool = false;
    while !done {
        if etype[pos + 1] == 5 {
            // inclusive range
            if eval[pos] <= chr && chr <= eval[pos + 1] {
                found = true;
            }
            pos = pos + 2;
        } else if etype[pos] == 7 {
            found = true;
            pos = pos + 1;
        } else {
            if eval[pos] == chr {
                found = true;
            }
            pos = pos + 1;
        }
        if etype[pos] != 6 {
            done = true;
        }
    }
    if found == positive {
        return pos + 1;
    }
    return 0 - (pos + 1);
}

// match_partial_char: can a partial UTF-8 sequence still satisfy the class at pos?
//   n_remain = number of continuation bytes still expected, value = bits so far
fn g_match_partial(etype: i64[:], eval: i64[:], pos_in: i64, value_in: i64, n_remain: i64) -> i64 {
    var pos: i64 = pos_in;
    let t0: i64 = etype[pos];
    let positive: bool = t0 == 3 || t0 == 7;
    let partial_value: i64 = value_in;
    if n_remain < 0 || (n_remain == 1 && partial_value < 2) {
        return 0;
    }
    var shift: i64 = n_remain * 6;
    var low: i64 = partial_value * pow2(shift);
    var high: i64 = low + (pow2(shift) - 1);
    if low == 0 {
        if n_remain == 2 {
            low = 2048;
        } else if n_remain == 3 {
            low = 65536;
        }
    }
    var done: bool = false;
    while !done {
        if etype[pos + 1] == 5 {
            if eval[pos] <= high && low <= eval[pos + 1] {
                if positive { return 1; }
                return 0;
            }
            pos = pos + 2;
        } else if etype[pos] == 7 {
            return 1;
        } else {
            if low <= eval[pos] && eval[pos] <= high {
                if positive { return 1; }
                return 0;
            }
            pos = pos + 1;
        }
        if etype[pos] != 6 {
            done = true;
        }
    }
    if positive { return 0; }
    return 1;
}

fn pow2(n: i64) -> i64 {
    var v: i64 = 1;
    var i: i64 = 0;
    while i < n {
        v = v * 2;
        i = i + 1;
    }
    return v;
}

// Grammar automaton, index model. A position is a global element index.
// In-progress stacks live in the work pool; only stacks whose top is terminal
// (or that are empty) are appended to the result, as the native code does.
//   etype/eval      element type and value
//   roff/rlen       rule start and element count
//   wp/wpd          work pool, max_stacks rows of max_depth
//   out/outd        result stacks
fn g_advance(etype: i64[:], eval: i64[:], roff: i64[:], rlen: i64[:],
             seed: i64[:], seedd: i64,
             wp: i64[:], wpd: i64[:], out: i64[:], outd: i64[:],
             base: i64,
             max_stacks: i64, max_depth: i64) -> i64 {
    var nwork: i64 = 0;
    var nout: i64 = 0;
    var queue: i64[128] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                           0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
    var qn: i64 = 0;
    var i: i64 = 0;
    while i < seedd && i < max_depth {
        wp[i] = seed[i];
        i = i + 1;
    }
    wpd[0] = seedd;
    nwork = 1;
    queue[0] = 0;
    qn = 1;
    while qn > 0 {
        qn = qn - 1;
        let sid: i64 = queue[qn];
        let depth: i64 = wpd[sid];
        if depth == 0 {
            // empty stack: final, the grammar can end here
            if nout < max_stacks {
                outd[base + nout] = 0;
                nout = nout + 1;
            }
            continue;
        }
        let pos: i64 = wp[sid * max_depth + depth - 1];
        let t: i64 = etype[pos];
        if t == 3 || t == 4 || t == 7 || t == 8 || t == 9 {
            // terminal: this stack is final, copy it to the result
            if nout < max_stacks {
                var d: i64 = 0;
                while d < depth {
                    out[(base + nout) * max_depth + d] = wp[sid * max_depth + d];
                    d = d + 1;
                }
                outd[base + nout] = depth;
                nout = nout + 1;
            }
            continue;
        }
        if t != 2 {
            // only RULE_REF expands at a stack top; anything else is a range
            // modifier that cannot be a top
            continue;
        }
        let rid: i64 = eval[pos];
        var alt_start: i64 = roff[rid];
        let alt_end: i64 = roff[rid] + rlen[rid];
        var done: bool = false;
        while !done && nwork < max_stacks {
            var d: i64 = 0;
            while d < depth - 1 {
                wp[nwork * max_depth + d] = wp[sid * max_depth + d];
                d = d + 1;
            }
            // the continuation of the rule reference, unless it ends the sequence
            if etype[pos + 1] != 0 && etype[pos + 1] != 1 {
                wp[nwork * max_depth + d] = pos + 1;
                d = d + 1;
            }
            // the alternate itself, unless it is empty
            if etype[alt_start] != 0 && etype[alt_start] != 1 {
                wp[nwork * max_depth + d] = alt_start;
                d = d + 1;
            }
            wpd[nwork] = d;
            if qn < 128 {
                queue[qn] = nwork;
                qn = qn + 1;
            }
            nwork = nwork + 1;
            var j: i64 = alt_start;
            var stop: bool = false;
            while j < alt_end && !stop {
                let tj: i64 = etype[j];
                if tj == 0 || tj == 1 {
                    stop = true;
                } else {
                    j = j + 1;
                }
            }
            if j < alt_end && etype[j] == 1 {
                alt_start = j + 1;
            } else {
                done = true;
            }
        }
    }
    return nout;
}

// reject_candidates for one stack, single-codepoint candidates (no partial UTF-8).
// rej[i] = 1 when candidate i is rejected. The stack transition after a class is
// candidate independent, so the descent is iterative instead of recursive.
// cand_cp is ncand rows of maxcp codepoints; curs starts at 0 for every candidate
recursive fn g_reject(etype: i64[:], eval: i64[:], roff: i64[:], rlen: i64[:],
            st: i64[:], std_in: i64,
            cand_cp: i64[:], cp_len: i64[:], curs: i64[:], rej: i64[:], ncand: i64,
            pval: i64[:], pnr: i64[:], cand_id: i64[:],
            scratch: i64[:], maxd: i64, maxcp: i64,
            pool: i64[:], nlevels: i64, level: i64) -> i64 {
    var depth: i64 = std_in;
    var i: i64 = 0;
    // copy the working stack into scratch
    while i < depth && i < maxd {
        scratch[i] = st[i];
        i = i + 1;
    }
    i = 0;
    while i < ncand {
        rej[i] = 0;
        i = i + 1;
    }
    // An empty stack accepts no characters, and the native rule is precise about it: a candidate
    // is rejected only when it still carries codepoints or an incomplete UTF-8 sequence. An
    // earlier form rejected every candidate here, which is wrong for a row that is empty because
    // its sequence already ended (an empty alternate) and right for a piece with content left.
    if depth <= 0 {
        var nrej0: i64 = 0;
        i = 0;
        while i < ncand {
            if curs[i] < cp_len[i] || pnr[i] > 0 {
                rej[i] = 1;
                nrej0 = nrej0 + 1;
            }
            i = i + 1;
        }
        return nrej0;
    }
    var alive: i64 = ncand;
    var steps: i64 = 0;
    while alive > 0 && depth > 0 && steps < 4096 {
        steps = steps + 1;
        let pos: i64 = scratch[depth - 1];
        let t: i64 = etype[pos];
        if t == 3 || t == 4 || t == 7 {
            // char class at the top: test every candidate and descend past the class
            var next_depth: i64 = depth - 1;
            i = 0;
            while i < ncand {
                if rej[i] == 0 && curs[i] >= cp_len[i] && pnr[i] > 0 {
                    // codepoints exhausted but the byte sequence is incomplete: the
                    // class must be able to complete it, otherwise reject
                    if g_match_partial(etype, eval, pos, pval[i], pnr[i]) == 0 {
                        rej[i] = 1;
                        alive = alive - 1;
                    }
                }
                if rej[i] == 0 && cp_len[i] <= 0 && pnr[i] <= 0 {
                    // the piece carries no codepoint at all and no valid partial: an invalid
                    // UTF-8 sequence, e.g. a byte-fallback token. It cannot continue the
                    // grammar. The earlier form of this check also matched candidates whose
                    // codepoints were merely exhausted, which rejected valid continuations and
                    // pushed the disagreement count from 768 to 487116 - measured, not assumed.
                    rej[i] = 1;
                    alive = alive - 1;
                }
                if rej[i] == 0 && curs[i] < cp_len[i] {
                    let r = g_match_char(etype, eval, pos, cand_cp[i * maxcp + curs[i]]);
                    if r > 0 {
                        // the class consumed this codepoint
                        curs[i] = curs[i] + 1;
                    } else {
                        rej[i] = 1;
                        alive = alive - 1;
                    }
                }
                i = i + 1;
            }
            let after2: i64 = match_end(etype, eval, pos);
            // ALT ends a sequence exactly as END does (llama_grammar_is_end_of_sequence)
            if etype[after2] == 0 || etype[after2] == 1 {
                depth = next_depth;
            } else {
                scratch[next_depth] = after2;
                depth = next_depth + 1;
            }
            continue;
        }
        if t == 2 {
            // Native advance_stack replaces a rule reference by the element that follows it
            // and by the start of every alternate of the referenced rule, then intersects the
            // rejects of the resulting stacks. Replacing the reference by the first alternate
            // alone dropped the continuation and the other alternates, and that is how a token
            // the native automaton allows was rejected (EXP-1471).
            let rid: i64 = eval[pos];
            let rstart: i64 = roff[rid];
            if rstart < 0 {
                depth = depth - 1;
                continue;
            }
            // count the alternates of the rule before choosing how to expand
            var altn: i64 = 0;
            var altp: i64 = rstart;
            var altdone: bool = false;
            while !altdone && altn < 4096 {
                altn = altn + 1;
                var j: i64 = altp;
                while etype[j] != 0 && etype[j] != 1 {
                    j = j + 1;
                }
                if etype[j] == 1 {
                    altp = j + 1;
                } else {
                    altdone = true;
                }
            }
            if altn > 1 {
                // Several alternates: the native recursion runs the same candidates against the
                // stack of each alternate and keeps a candidate only when every row rejects it.
                // Each row gets its own cursor snapshot and reject set in the pool, so the rows
                // do not see each other's consumption. Beyond the pool the call is declined.
                if level + 1 >= nlevels {
                    return -2;
                }
                let snap: i64 = level * (3 * ncand + maxd);
                let pre: i64 = snap + ncand;
                let rrow: i64 = pre + ncand;
                let strow: i64 = rrow + ncand;
                // pre holds what was already rejected by the classes above; acc (in rej) starts at
                // "rejected" and is cleared by any row that accepts, which is the native
                // intersection. Seeding the rows with pre instead would let a rejecting row keep
                // a candidate rejected even when a later row accepts it.
                i = 0;
                while i < ncand {
                    pool[snap + i] = curs[i];
                    pool[pre + i] = rej[i];
                    rej[i] = 1;
                    i = i + 1;
                }
                var alt2: i64 = rstart;
                var more: bool = true;
                while more && steps < 4096 {
                    steps = steps + 1;
                    // the row for this alternate: the continuation below it, as advance_stack
                    var d3: i64 = depth - 1;
                    if etype[pos + 1] != 0 && etype[pos + 1] != 1 && d3 < maxd {
                        scratch[d3] = pos + 1;
                        d3 = d3 + 1;
                    }
                    if etype[alt2] != 0 && etype[alt2] != 1 && d3 < maxd {
                        scratch[d3] = alt2;
                        d3 = d3 + 1;
                    }
                    i = 0;
                    while i < ncand {
                        curs[i] = pool[snap + i];
                        i = i + 1;
                    }
                    let rr = g_reject(etype, eval, roff, rlen, scratch, d3,
                                      cand_cp, cp_len, curs, pool[rrow..rrow + ncand], ncand,
                                      pval, pnr, cand_id, pool[strow..strow + maxd], maxd, maxcp,
                                      pool, nlevels, level + 1);
                    if rr < 0 {
                        return rr;
                    }
                    // intersection: a candidate stays accepted only if this row accepts it too
                    i = 0;
                    while i < ncand {
                        if pool[rrow + i] == 0 {
                            rej[i] = 0;
                        }
                        i = i + 1;
                    }
                    var j2: i64 = alt2;
                    while etype[j2] != 0 && etype[j2] != 1 {
                        j2 = j2 + 1;
                    }
                    if etype[j2] == 1 {
                        alt2 = j2 + 1;
                    } else {
                        more = false;
                    }
                }
                var alive2: i64 = 0;
                i = 0;
                while i < ncand {
                    if pool[pre + i] != 0 {
                        rej[i] = 1;
                    }
                    if rej[i] == 0 {
                        alive2 = alive2 + 1;
                    }
                    i = i + 1;
                }
                return alive2;
            }
            // one alternate: the continuation goes below it, as advance_stack does
            var d2: i64 = depth - 1;
            if etype[pos + 1] != 0 && etype[pos + 1] != 1 && d2 < maxd {
                scratch[d2] = pos + 1;
                d2 = d2 + 1;
            }
            if etype[rstart] != 0 && etype[rstart] != 1 && d2 < maxd {
                scratch[d2] = rstart;
                d2 = d2 + 1;
            }
            depth = d2;
            continue;
        }
        if t == 8 || t == 9 {
            i = 0;
            while i < ncand {
                if rej[i] == 0 {
                    if curs[i] >= cp_len[i] {
                        // no codepoints left: a partial sequence cannot satisfy a token rule
                        if pnr[i] > 0 {
                            rej[i] = 1;
                            alive = alive - 1;
                        }
                    } else {
                        let want_tok: bool = t == 8;
                        let is_tok: bool = eval[pos] == cand_id[i];
                        if is_tok != want_tok {
                            rej[i] = 1;
                            alive = alive - 1;
                        } else {
                            // a token rule consumes the whole token, so the rest of
                            // its codepoints are not leftovers
                            curs[i] = cp_len[i];
                        }
                    }
                }
                i = i + 1;
            }
            depth = depth - 1;
            continue;
        }
        // END or unsupported: stop
        break;
    }
    // a candidate with codepoints left when the stack is exhausted cannot be
    // consumed by this grammar state
    i = 0;
    while i < ncand {
        if rej[i] == 0 && curs[i] < cp_len[i] {
            rej[i] = 1;
            alive = alive - 1;
        }
        i = i + 1;
    }
    return alive;
}

// end position of the class at pos (the native uses match_char(pos, 0).second)
fn match_end(etype: i64[:], eval: i64[:], pos_in: i64) -> i64 {
    var pos: i64 = pos_in;
    var done: bool = false;
    while !done {
        if etype[pos + 1] == 5 {
            pos = pos + 2;
        } else if etype[pos] == 7 {
            pos = pos + 1;
        } else {
            pos = pos + 1;
        }
        if etype[pos] != 6 {
            done = true;
        }
    }
    return pos;
}

// reject across several stacks: a candidate is rejected only when every stack
// rejects it, which is the native intersection of per-stack reject sets.
// stacks is nstacks rows of maxd element indexes; stackd holds each depth.
fn g_reject_multi(etype: i64[:], eval: i64[:], roff: i64[:], rlen: i64[:],
                  stacks: i64[:], stackd: i64[:], nstacks: i64,
                  cand_cp: i64[:], cp_len: i64[:], pval: i64[:], pnr: i64[:], cand_id: i64[:],
                  rej: i64[:], scratch: i64[:], tmp_rej: i64[:], tmp_cur: i64[:],
                  rowbuf: i64[:],
                  ncand: i64, maxd: i64, maxcp: i64,
                  pool: i64[:], nlevels: i64) -> i64 {
    var i: i64 = 0;
    while i < ncand {
        rej[i] = 1;
        i = i + 1;
    }
    var s: i64 = 0;
    while s < nstacks {
        // reset the per-candidate cursors and evaluate one stack
        i = 0;
        while i < ncand {
            tmp_cur[i] = 0;
            tmp_rej[i] = 0;
            i = i + 1;
        }
        var d: i64 = 0;
        while d < stackd[s] {
            rowbuf[d] = stacks[s * maxd + d];
            d = d + 1;
        }
        let r_one = g_reject(etype, eval, roff, rlen,
                             rowbuf, stackd[s],
                             cand_cp, cp_len, tmp_cur, tmp_rej, ncand,
                             pval, pnr, cand_id, scratch, maxd, maxcp,
                             pool, nlevels, 0);
        if r_one < 0 {
            // the port declined this call; the caller must let the native matcher decide
            return r_one;
        }
        i = 0;
        while i < ncand {
            if tmp_rej[i] == 0 {
                rej[i] = 0;
            }
            i = i + 1;
        }
        s = s + 1;
    }
    var nrej: i64 = 0;
    i = 0;
    while i < ncand {
        nrej = nrej + rej[i];
        i = i + 1;
    }
    return nrej;
}

// Composed entry: expand the input stack with the automaton, then reject across the
// resulting stacks. This is the shape the sampler filter needs.
fn g_grammar_reject(etype: i64[:], eval: i64[:], roff: i64[:], rlen: i64[:],
                    seed: i64[:], seedd: i64,
                    cand_cp: i64[:], cp_len: i64[:], pval: i64[:], pnr: i64[:],
                    cand_id: i64[:], ncand: i64,
                    wp: i64[:], wpd: i64[:], stacks: i64[:], stackd: i64[:],
                    scratch: i64[:], tmp_rej: i64[:], tmp_cur: i64[:], rowbuf: i64[:],
                    rej: i64[:], maxs: i64, maxd: i64, maxcp: i64,
                    pool: i64[:], nlevels: i64) -> i64 {
    let n = g_advance(etype, eval, roff, rlen, seed, seedd, wp, wpd, stacks, stackd, 0, maxs, maxd);
    if n <= 0 {
        return -1;
    }
    return g_reject_multi(etype, eval, roff, rlen, stacks, stackd, n,
                          cand_cp, cp_len, pval, pnr, cand_id, rej, scratch, tmp_rej, tmp_cur,
                          rowbuf, ncand, maxd, maxcp, pool, nlevels);
}



// One entry for the grammar decision: flat rule arrays, flat stacks and the
// candidate codepoints; returns how many candidates were rejected and sets rej[].
export fn fg_grammar_reject(etype: i64[:], eval: i64[:], roff: i64[:], rlen: i64[:],
                     seed: i64[:], seedd: i64,
                     cand_cp: i64[:], cp_len: i64[:], pval: i64[:], pnr: i64[:],
                     cand_id: i64[:], ncand: i64,
                     wp: i64[:], wpd: i64[:], stacks: i64[:], stackd: i64[:],
                     scratch: i64[:], tmp_rej: i64[:], tmp_cur: i64[:], rowbuf: i64[:],
                     rej: i64[:], maxs: i64, maxd: i64, maxcp: i64,
                     pool: i64[:], nlevels: i64, marker: i64) -> i64 {
    if marker != 180 {
        return -1;
    }
    return g_grammar_reject(etype, eval, roff, rlen, seed, seedd,
                            cand_cp, cp_len, pval, pnr, cand_id, ncand,
                            wp, wpd, stacks, stackd, scratch, tmp_rej, tmp_cur, rowbuf,
                            rej, maxs, maxd, maxcp, pool, nlevels);
}

// Allowed first codepoints at the current stack tops, as inclusive ranges.
// Returns the number of ranges written (each into lo[]/hi[]), or -1 when the
// decision cannot be expressed as ranges (a token rule or an inverse class is at a
// top), in which case the caller must run the full automaton.
fn g_first_set(etype: i64[:], eval: i64[:],
               stacks: i64[:], stackd: i64[:], nstacks: i64,
               lo: i64[:], hi: i64[:], max_ranges: i64) -> i64 {
    var n: i64 = 0;
    var s: i64 = 0;
    while s < nstacks {
        let d: i64 = stackd[s];
        if d == 0 {
            // an empty stack accepts no characters, so skipping it cannot exclude a
            // candidate that this stack would have allowed
            s = s + 1;
            continue;
        }
        let pos: i64 = stacks[s * 8 + d - 1];
        let t: i64 = etype[pos];
        if t == 4 || t == 8 || t == 9 {
            // CHAR_NOT, TOKEN, TOKEN_NOT: not expressible as positive ranges here
            return 0 - t - 100;
        }
        if t != 3 && t != 7 {
            return 0 - t - 200;
        }
        var p: i64 = pos;
        var done: bool = false;
        while !done {
            if etype[p + 1] == 5 {
                if n < max_ranges {
                    lo[n] = eval[p];
                    hi[n] = eval[p + 1];
                    n = n + 1;
                }
                p = p + 2;
            } else if etype[p] == 7 {
                if n < max_ranges {
                    lo[n] = 0;
                    hi[n] = 1114111;
                    n = n + 1;
                }
                p = p + 1;
            } else {
                if n < max_ranges {
                    lo[n] = eval[p];
                    hi[n] = eval[p];
                    n = n + 1;
                }
                p = p + 1;
            }
            if etype[p] != 6 {
                done = true;
            }
        }
        s = s + 1;
    }
    return n;
}


// entry: allowed first-codepoint ranges for the current stacks
export fn fg_grammar_first_set(etype: i64[:], eval: i64[:], stacks: i64[:], stackd: i64[:],
                        nstacks: i64, lo: i64[:], hi: i64[:], max_ranges: i64,
                        marker: i64) -> i64 {
    if marker != 181 {
        return -1;
    }
    return g_first_set(etype, eval, stacks, stackd, nstacks, lo, hi, max_ranges);
}

// Single stable entry point for the shipped library. Generated function ids
// shift whenever this module changes, so callers must reach the chain through
// this one function rather than through a generated id.
export fn fg_sampler_chain(op: i64, lv: f32[:], p: f32[:], id: i64[:], n: i64,
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
