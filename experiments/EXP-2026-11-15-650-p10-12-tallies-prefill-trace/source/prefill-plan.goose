// P10-12 S2: the prefill chunk plan (gap G2).
//
// Native source: runtime/core/src/engine/session.cpp at base commit 1b3f4a1a,
// the prompt loop at 1541-1591:
//
//   for (int i = (int) n_common; i < n_prompt; i += im.cfg.n_batch) {
//       const int chunk = std::min(im.cfg.n_batch, n_prompt - i);
//       ... decode this chunk, trace_begin(i, chunk, 0), flush ...
//   }
//
// The decision is which slices of the prompt go into which decode. It is
// index arithmetic and nothing else; the decode, the draft-context walk and
// the trace calls around it stay in the carrier.
//
// The prefix is pf_. `n_batch` is llama.cpp's batching size and the session's
// config validator already refuses anything below 1; the module repeats the
// precondition as a guard so a caller that skips validation cannot hang in the
// plan, and the gate never exercises it (the native loop could not terminate).
//
// Positions are absolute: the reused prefix (`n_common`) is where the loop
// starts and where the trace frame's `base_pos` comes from, which is why the
// plan returns the start index and not a relative offset.

// Fills out[2*k] = i, out[2*k+1] = chunk for every decode of the prompt, in
// order, and returns the number of pairs. A prompt that is already in the KV
// (n_common >= n_prompt) plans no chunk at all.
export fn pf_plan(n_common: i64, n_prompt: i64, n_batch: i64, out: i64[:]) -> i64 {
    var n: i64 = 0;
    if n_batch < 1 {
        return 0;
    }
    var i: i64 = n_common;
    while i < n_prompt {
        var chunk: i64 = n_batch;
        let left: i64 = n_prompt - i;
        if left < n_batch {
            chunk = left;
        }
        if 2 * n + 1 < out.len {
            out[2 * n] = i;
            out[2 * n + 1] = chunk;
        }
        n = n + 1;
        i = i + n_batch;
    }
    return n;
}
