// Speculative-decoding decisions in Goose, from runtime/core/src/engine/session.cpp,
// runtime/core/src/engine/ngram_draft.cpp and upstream/llama.cpp/common/speculative.cpp.
//
// Slice A - the draft source and the accept decision:
//   sdp_ngram_draft   ngram_draft (ngram_draft.cpp:10-98) - prompt-lookup proposal
//   sdp_accept        the accept loop (session.cpp:1766-1795)
//   sdp_emit_count    the emit bound (session.cpp:1848-1852)
//
// Slice B - the MTP draft state:
//   sdp_mtp_width       draft_mtp::draft, the p_min width (speculative.cpp:1700-1785)
//   sdp_mtp_head_clamp  the chain-head n_max clamp (speculative.cpp:1455-1470)
//   sdp_mtp_rewind      the draft-KV rewind before catch-up (session.cpp:1700-1706)
//   sdp_mtp_catchup     the catch-up width and the accept seed row
//                       (session.cpp:1810-1830, speculative.cpp:1790-1800)
//
// Slice C - the budget and the rollback:
//   sdp_step_plan      room / n_max / wide / verify width (session.cpp:1676-1740)
//   sdp_rollback       the rejected-tail carve (session.cpp:1818-1824)
//   sdp_step_advance   n_past and the next logits row (session.cpp:1884-1890)
//   sdp_tail_trim      the end-of-turn trim of target and draft KV (session.cpp:1899-1912)
//
// All of these are pure integer policy over token ids, positions and confidences: no
// llama.cpp, no model, no KV. Tokens are i32 (llama_token), positions and widths i64
// (llama_pos and int). The verify pass that supplies the target tokens and the MTP block
// that supplies the confidences stay native; this module owns what the loop decides with
// them. Slice prefixes: every export starts with sdp_.

// The corpus address: ctx[0..len) then `last` at index ctx.len.
fn sdp_at(ctx: const i32[:], last: i32, i: i64) -> i32 {
    if i < ctx.len {
        return ctx[i];
    }
    return last;
}

// Prompt-lookup drafting. Native: ngram_draft.cpp:10-98.
//
// The corpus is ctx followed by `last`; the pattern is the corpus suffix. Every position
// strictly before the corpus end is a candidate match end; the longest common suffix wins
// and among equal lengths the most recent one does, because the scan runs backwards and
// only a strictly longer match replaces the current best. A match may overlap the pattern,
// which is what makes a run of one repeated token predict itself.
//
//   ctx        prompt + tokens generated so far, in order
//   last       the token just confirmed, not yet appended to ctx; part of the pattern
//   n_max      most tokens to draft (the verify batch is 1 + this)
//   min_match  shortest suffix match allowed to draft at all; below it, draft nothing
//   max_match  longest suffix considered (a scan bound, not a target)
//   out        filled with the drafted tokens; its length is the write bound
//
// Returns the number of tokens drafted, in [0, n_max]. 0 means "no confident match" and
// the caller takes the ordinary non-speculative path for this step.
export fn sdp_ngram_draft(ctx: const i32[:], last: i32, n_max: i64, min_match: i64, max_match: i64,
                          out: i32[:]) -> i64 {
    if n_max <= 0 as i64 {
        return 0 as i64;
    }
    var minm: i64 = min_match;
    if minm < 1 as i64 {
        minm = 1 as i64;
    }
    if max_match < minm {
        return 0 as i64;
    }

    // The corpus is ctx ++ [last], addressed without materialising it.
    let len: i64 = ctx.len + 1 as i64;
    // A match needs minm tokens ending strictly before the corpus end.
    if len < minm + 1 as i64 {
        return 0 as i64;
    }

    let end: i64 = len - 1 as i64;          // the pattern's last position
    let j_min: i64 = minm - 1 as i64;       // below this a candidate cannot reach minm

    var best_m: i64 = 0 as i64;
    var best_j: i64 = 0 as i64;

    // Scan backwards from the most recent candidate: the most recent position wins among
    // equal lengths for free.
    var j: i64 = end - 1 as i64;
    while true {
        if sdp_at(ctx, last, j) == sdp_at(ctx, last, end) {
            // Longest common suffix of C[..j] and C[..end], capped by max_match and by how
            // much corpus lies at or before j. Overlap with the pattern is intentional.
            var lim: i64 = max_match;
            if j + 1 as i64 < lim {
                lim = j + 1 as i64;
            }
            var m: i64 = 1 as i64;
            while m < lim {
                if sdp_at(ctx, last, j - m) != sdp_at(ctx, last, end - m) {
                    break;
                }
                m = m + 1 as i64;
            }
            if m > best_m {
                best_m = m;
                best_j = j;
                if best_m == max_match {
                    break; // capped: nothing earlier can beat it
                }
            }
        }
        if j <= j_min {
            break;
        }
        j = j - 1 as i64;
    }

    // The confidence gate, and the whole economics of this source: no confident match
    // means no draft, no widened verify batch, and a step that costs exactly a plain decode.
    if best_m < minm {
        return 0 as i64;
    }

    let avail: i64 = end - best_j; // tokens that followed the match, up to the corpus end
    var n: i64 = n_max;
    if avail < n {
        n = avail;
    }
    if out.len < n {
        n = out.len; // write bound; the gate sizes out to n_max, so this never bites
    }
    var k: i64 = 0 as i64;
    while k < n {
        out[k] = sdp_at(ctx, last, best_j + 1 as i64 + k);
        k = k + 1 as i64;
    }
    return n;
}

// The accept decision. Native: session.cpp:1766-1795.
//
// `tok` was settled before the decode and is confirmed unconditionally. Each draft is
// confirmed only while the target's own argmax at that row agrees with it. The confirm is
// counted before the end-of-generation check, so an accepted EOG token is in the KV but not
// in `confirmed` and therefore never emitted - the same rule the plain path follows.
//
//   drafts   the source's proposal, n_draft entries
//   target   the target's argmax at verify row i (row i predicts drafts[i])
//   eog      eog[i] != 0 when target[i] is an end-of-generation token
//   out      out[0] n_acc, out[1] confirmed length, out[2] eog hit,
//            out[3] index the loop stopped at, out[4] n_draft
//
// Returns 0. A target or eog array shorter than drafts stops the loop at its end, which is
// the caller's error and not a native case.
export fn sdp_accept(drafts: const i32[:], target: const i32[:], eog: const i32[:],
                     out: i64[:]) -> i64 {
    let n_draft: i64 = drafts.len;
    var n_acc: i64 = 0 as i64;
    var confirmed: i64 = 1 as i64; // `tok` itself
    var hit: i64 = 0 as i64;
    var stop: i64 = n_draft;
    var i: i64 = 0 as i64;
    while i < n_draft {
        if i >= target.len {
            stop = i; // no row: nothing to confirm, the draft is rejected
            break;
        }
        if target[i] != drafts[i] {
            stop = i;
            break;
        }
        n_acc = n_acc + 1 as i64; // accepted: it is in the KV whether or not the caller sees it
        if i < eog.len {
            if eog[i] != 0 as i32 {
                hit = 1 as i64; // end-of-generation is never emitted
                stop = i;
                break;
            }
        }
        confirmed = confirmed + 1 as i64;
        i = i + 1 as i64;
    }

    if out.len >= 5 as i64 {
        out[0] = n_acc;
        out[1] = confirmed;
        out[2] = hit;
        out[3] = stop;
        out[4] = n_draft;
    }
    return 0 as i64;
}

// How many of the confirmed tokens the caller actually receives. Native: the emit loop
// `for (size_t e = 0; e < confirmed.size() && n_gen < req.n_predict; ++e)` and the
// one-per-row `TokenMetrics`. Returns min(confirmed, n_predict - n_gen), floored at 0.
export fn sdp_emit_count(n_predict: i64, n_gen: i64, confirmed: i64) -> i64 {
    if n_gen >= n_predict {
        return 0 as i64;
    }
    let left: i64 = n_predict - n_gen;
    if confirmed < left {
        return confirmed;
    }
    return left;
}

// The MTP draft width under the confidence floor. Native: draft_mtp::draft,
// speculative.cpp:1700-1785: every step decodes one position, samples, and stops BEFORE
// pushing when the sampled token's own probability is below p_min; a pushed token stops
// the loop when the result reaches n_max; and a result shorter than n_min is cleared.
//
//   conf   the head's probability for the candidate at each step, in order
//   n_max  SpecConfig::draft_max, already capped by the caller's room
//   p_min  SpecConfig::draft_p_min (0 in the measured configuration)
//   n_min  common_params_speculative_draft::n_min (0 from the session)
//   out    out[0] drafted, out[1] cleared by n_min, out[2] decode steps,
//          out[3] conf ran out before the loop would have stopped
export fn sdp_mtp_width(conf: const f64[:], n_max: i64, p_min: f64, n_min: i64, out: i64[:]) -> i64 {
    var n: i64 = 0 as i64;
    var steps: i64 = 0 as i64;
    var ran_out: i64 = 0 as i64;
    var i: i64 = 0 as i64;
    if n_max < 1 as i64 {
        if out.len >= 4 as i64 {
            out[0] = 0 as i64;
            out[1] = 0 as i64;
            out[2] = 0 as i64;
            out[3] = 0 as i64;
        }
        return 0 as i64;
    }
    while true {
        if i >= conf.len {
            ran_out = 1 as i64; // the head would have decoded another step
            break;
        }
        steps = steps + 1 as i64;
        if conf[i] < p_min {
            break; // rejected before the push: this step decoded, produced nothing
        }
        n = n + 1 as i64;
        i = i + 1 as i64;
        if n_max <= n {
            break; // n_max reached: the loop stops right after the push
        }
    }

    var cleared: i64 = 0 as i64;
    if n < n_min {
        cleared = 1 as i64;
        n = 0 as i64;
    }
    if out.len >= 4 as i64 {
        out[0] = n;
        out[1] = cleared;
        out[2] = steps;
        out[3] = ran_out;
    }
    return 0 as i64;
}

// The chained-head width clamp and the single-head case. Native: the draft_mtp ctor,
// speculative.cpp:1445-1470: n_mtp_layers is max(1, the model's nextn layer count),
// chain_heads is "more than one head and not shared memory", and only a chained model
// clamps n_max down to the head count. qwen35/qwen35moe - the measured target - is one
// head, so the clamp does not move it.
export fn sdp_mtp_head_clamp(n_max: i64, n_mtp_layers: i64, is_mem_shared: i64) -> i64 {
    var layers: i64 = n_mtp_layers;
    if layers < 1 as i64 {
        layers = 1 as i64;
    }
    var chain: i64 = 0 as i64;
    if layers > 1 as i64 {
        if is_mem_shared == 0 as i64 {
            chain = 1 as i64;
        }
    }
    if chain != 0 as i64 {
        if layers < n_max {
            return layers;
        }
    }
    return n_max;
}

// The draft-context rewind that must precede the catch-up. Native: session.cpp:1700-1706,
// `llama_memory_seq_rm(memory(ctx_dft), 0, n_past, -1)`; drafting wrote into the draft KV at
// the very positions the catch-up is about to occupy, and a batch must begin strictly after
// the last stored position.
//
//   seq_pos_max  the draft context's highest stored position for the sequence (-1 empty)
//   n_past       where the next decode starts
//
//   out[0] rows the rewind removes, out[1] rows the draft context is missing below n_past
//
// `missing` is the condition begin() only warns about (speculative.cpp:1508-1520): a
// prefill ubatch the draft context never saw leaves a hole, and the reward is degraded
// drafts rather than a failure. Returns 0.
export fn sdp_mtp_rewind(seq_pos_max: i64, n_past: i64, out: i64[:]) -> i64 {
    var removed: i64 = 0 as i64;
    var missing: i64 = 0 as i64;
    if n_past < 0 as i64 {
        if out.len >= 2 as i64 {
            out[0] = 0 as i64;
            out[1] = 0 as i64;
        }
        return 0 as i64;
    }
    if seq_pos_max >= n_past {
        removed = seq_pos_max - n_past + 1 as i64;
    } else {
        missing = n_past - 1 as i64 - seq_pos_max;
    }
    if out.len >= 2 as i64 {
        out[0] = removed;
        out[1] = missing;
    }
    return 0 as i64;
}

// The catch-up width and the accept seed. Native: session.cpp:1812-1830 hands
// common_speculative_process a batch of 1 + n_acc rows - the accepted prefix only, the
// rejected tail is never submitted - and speculative.cpp:1792-1800 seeds the next draft
// from verify_h row min(n_accepted, n_rows - 1).
//
//   n_acc    accepted drafts
//   n_rows   rows the last process() batch stored (0 before the first one)
//
//   out[0] catch-up rows (1 + n_acc), out[1] seed row, out[2] 1 when n_rows is too small
//          to hold the seed the session relies on
//
// Returns 0. n_rows <= 0 is the native early return: no seed at all.
export fn sdp_mtp_catchup(n_acc: i64, n_rows: i64, out: i64[:]) -> i64 {
    let rows: i64 = 1 as i64 + n_acc;
    var seed: i64 = 0 as i64;
    var short_rows: i64 = 0 as i64;
    if n_rows > 0 as i64 {
        seed = n_acc;
        if n_rows - 1 as i64 < seed {
            seed = n_rows - 1 as i64; // std::min(n_accepted, n_rows - 1)
        }
        if seed != n_acc {
            short_rows = 1 as i64;
        }
    } else {
        short_rows = 1 as i64;
    }
    if out.len >= 3 as i64 {
        out[0] = rows;
        out[1] = seed;
        out[2] = short_rows;
    }
    return 0 as i64;
}

// One speculative step's budget. Native: session.cpp:1676-1740.
//
// `room` is what is still wanted after the confirmed token; the cap goes in BEFORE
// drafting so no source is asked for tokens with nowhere to go. A step that drafts nothing
// takes the plain path (wide = 0, one token, one logits row, no rollback), which is the
// loop's floor and applies to the head too whenever p_min stops it.
//
//   out[0] room, out[1] n_max asked of the source, out[2] 1 when the source is called,
//   out[3] wide, out[4] drafts after the defensive cap, out[5] verify width
export fn sdp_step_plan(n_predict: i64, n_gen: i64, draft_max: i64, spec_on: i64, n_draft_in: i64,
                        out: i64[:]) -> i64 {
    let room: i64 = n_predict - n_gen - 1 as i64;
    var n_max: i64 = 0 as i64;
    var call: i64 = 0 as i64;
    var n_draft: i64 = n_draft_in;
    if spec_on != 0 as i64 {
        if room > 0 as i64 {
            call = 1 as i64;
            n_max = draft_max;
            if room < n_max {
                n_max = room;
            }
            // The defensive resize the MTP arm performs on the source's result.
            if n_draft > room {
                n_draft = room;
            }
        } else {
            n_draft = 0 as i64; // the source is not called, so nothing was proposed
        }
    } else {
        n_draft = 0 as i64;
    }

    var wide: i64 = 0 as i64;
    if spec_on != 0 as i64 {
        if n_draft > 0 as i64 {
            wide = 1 as i64;
        }
    }
    var width: i64 = 1 as i64;
    if wide != 0 as i64 {
        width = 1 as i64 + n_draft;
    }
    if out.len >= 6 as i64 {
        out[0] = room;
        out[1] = n_max;
        out[2] = call;
        out[3] = wide;
        out[4] = n_draft;
        out[5] = width;
    }
    return 0 as i64;
}

// The rejected-tail carve. Native: session.cpp:1818-1824, `keep = n_past + 1 + n_acc`,
// then seq_rm(ctx, 0, keep, -1) only when the whole draft was not accepted. The bounded
// rollback snapshots asked for at context creation (n_rs_seq = draft_max) are what make
// this a restore rather than a replay.
//
//   out[0] 1 when a rollback is needed, out[1] the first dropped position (keep),
//   out[2] rejected drafts
export fn sdp_rollback(n_past: i64, n_draft: i64, n_acc: i64, out: i64[:]) -> i64 {
    var needed: i64 = 0 as i64;
    var keep: i64 = n_past + 1 as i64 + n_acc;
    var rejected: i64 = 0 as i64;
    if n_acc < n_draft {
        needed = 1 as i64;
        rejected = n_draft - n_acc;
    }
    if out.len >= 3 as i64 {
        out[0] = needed;
        out[1] = keep;
        out[2] = rejected;
    }
    return 0 as i64;
}

// Where the next step starts and which logits row seeds it. Native: session.cpp:1884-1890,
// `n_past += 1 + n_acc` and `row = wide ? n_acc : -1`. A wide step leaves the logits at the
// first unverified position holding the target's own continuation, so the next token is
// already there for free; a plain step keeps row -1.
//
//   out[0] next n_past, out[1] logits row
export fn sdp_step_advance(n_past: i64, n_acc: i64, wide: i64, out: i64[:]) -> i64 {
    var row: i64 = 0 as i64 - 1 as i64;
    if wide != 0 as i64 {
        row = n_acc;
    }
    if out.len >= 2 as i64 {
        out[0] = n_past + 1 as i64 + n_acc;
        out[1] = row;
    }
    return 0 as i64;
}

// The end-of-turn trim. Native: session.cpp:1899-1912. An accepted end-of-generation token
// is decoded but never emitted, and a group can be cut short by n_predict, so the KV can
// stand ahead of what the caller received; both contexts are trimmed back to the last
// emitted position or the next turn would reuse a prefix that never produced this answer.
//
//   n_prompt  prompt tokens in the context
//   n_gen     tokens actually emitted this turn
//   kv_end    stored rows after the last step, exclusive (the native `pos_max + 1`)
//
//   out[0] emitted_end (the trim point), out[1] rows removed from each context
export fn sdp_tail_trim(n_prompt: i64, n_gen: i64, kv_end: i64, out: i64[:]) -> i64 {
    let emitted_end: i64 = n_prompt + n_gen;
    var removed: i64 = 0 as i64;
    if kv_end > emitted_end {
        removed = kv_end - emitted_end;
    }
    if out.len >= 2 as i64 {
        out[0] = emitted_end;
        out[1] = removed;
    }
    return 0 as i64;
}
