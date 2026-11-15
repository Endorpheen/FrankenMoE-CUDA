// MoE routing: the decision half of build_moe_ffn (llama-graph.cpp:1941-2100).
//
// The expert arithmetic is already migrated (goose/cpu/goose-quantized-moe/), and the
// P1-12 SIMD bridge drives it. What was missing is the selection: which experts this
// token uses and with what weights. That is a decision, not a kernel, so it lives here.
//
// The native sequence, with no expert groups (this model has none):
//
//   logits  = gate_inp * cur                      [n_expert, n_tokens]
//   probs   = softmax(logits)                     over n_expert
//   ids     = argsort_top_k(probs, n_expert_used) [n_used, n_tokens]
//   weights = get_rows(reshape(probs, [1, n_expert, n_tokens]), ids)
//   weights = weights / sum_rows(weights)         if norm_w
//   weights = weights * w_scale
//
// build_moe_ffn is shared by every MoE architecture, so this module is common: no
// architecture gets a private copy of the router.

// Softmax over one token's logits, in place in `out`. Returns 0 on success, -1 when
// the input is degenerate. exp() is applied after subtracting the maximum, which is
// what ggml_soft_max does and what keeps a large logit from overflowing.
export fn moe_softmax(logits: f32[:], out: f32[:]) -> i64 {
    if out.len < logits.len {
        return -1;
    }
    if logits.len == 0 {
        return -1;
    }
    var i: i64 = 0;
    var mx: f32 = logits[0];
    while i < logits.len {
        if logits[i] > mx {
            mx = logits[i];
        }
        i = i + 1;
    }
    var sum: f64 = 0.0;
    i = 0;
    while i < logits.len {
        let e: f64 = exp64(logits[i] - mx);
        out[i] = e as f32;
        sum = sum + e;
        i = i + 1;
    }
    if sum <= 0.0 {
        return -1;
    }
    i = 0;
    while i < logits.len {
        out[i] = (out[i] as f64 / sum) as f32;
        i = i + 1;
    }
    return 0;
}

extern "exp" fn exp64(x: f64) -> f64;

// The n_used largest probabilities, highest first. Ties keep the lower expert id, which
// is what a stable descending argsort gives and what makes the selection reproducible.
export fn moe_top_k(probs: f32[:], n_used: i64, ids: i64[:]) -> i64 {
    if n_used <= 0 || ids.len < n_used {
        return -1;
    }
    if probs.len < n_used {
        return -1;
    }
    const n = probs.len;
    var taken: i64 = 0;
    var used: i64 = 0;
    while taken < n_used {
        var best: i64 = -1;
        var best_v: f32 = 0.0 - 3.4e38 as f32;
        var i: i64 = 0;
        while i < n {
            if !moe_id_used(ids, used, i) {
                if probs[i] > best_v {
                    best_v = probs[i];
                    best = i;
                }
            }
            i = i + 1;
        }
        if best < 0 {
            return -1;
        }
        ids[used] = best;
        used = used + 1;
        taken = taken + 1;
    }
    return used as i64;
}

fn moe_id_used(ids: i64[:], n: i64, id: i64) -> bool {
    var i: i64 = 0;
    while i < n {
        if ids[i] == id {
            return true;
        }
        i = i + 1;
    }
    return false;
}

// Gather the weights of the selected experts, normalise when asked, and scale.
// The scale is a plain f32: passing it as bits only makes sense when an integer-only
// ABI forces it, and this module has no such constraint.
export fn moe_weights(probs: f32[:], ids: i64[:], n_used: i64,
                      do_norm: i64, scale: f32, out: f32[:]) -> i64 {
    if n_used <= 0 || out.len < n_used || ids.len < n_used {
        return -1;
    }
    var i: i64 = 0;
    while i < n_used {
        let id: i64 = ids[i];
        if id < 0 || id >= probs.len {
            return -1;
        }
        out[i] = probs[id];
        i = i + 1;
    }
    if do_norm != 0 {
        var sum: f64 = 0.0;
        i = 0;
        while i < n_used {
            sum = sum + out[i] as f64;
            i = i + 1;
        }
        if sum <= 0.0 {
            return -1;
        }
        i = 0;
        while i < n_used {
            out[i] = (out[i] as f64 / sum) as f32;
            i = i + 1;
        }
    }
    let sc: f32 = scale;
    i = 0;
    while i < n_used {
        out[i] = out[i] * sc;
        i = i + 1;
    }
    return n_used;
}

// One token end to end: logits -> softmax -> top-k -> weighted selection.
export fn moe_route(logits: f32[:], n_used: i64, do_norm: i64, scale: f32,
                    ids: i64[:], weights: f32[:], probs: f32[:]) -> i64 {
    if moe_softmax(logits, probs) != 0 {
        return -1;
    }
    let got: i64 = moe_top_k(probs, n_used, ids);
    if got != n_used {
        return -1;
    }
    return moe_weights(probs, ids, n_used, do_norm, scale, weights);
}

// ---- the shared-expert branch and the combine (qwen35moe.cpp:530-548) ----
//
//   shared_gate = sigmoid(ffn_gate_inp_shexp * cur)     one value per token
//   ffn_shexp   = down(silu(gate_shexp * cur) * (up_shexp * cur))
//   ffn_shexp   = ffn_shexp * shared_gate
//   cur'        = moe_out + ffn_shexp
//
// Three one-line decisions, recorded here because they are ORDER decisions: the sigmoid
// belongs on the shared gate and not on the router, the silu belongs on the gate half of
// the PAR pair and not on the up half, and the two branches are added rather than
// averaged. Each is easy to get subtly wrong while every individual node stays correct.

fn moe_sigmoid(x: f64) -> f64 {
    return 1.0 / (1.0 + exp64(0.0 - x));
}

export fn moe_shared_gate(proj: f32[:], out: f32[:]) -> i64 {
    if out.len < proj.len {
        return -1;
    }
    var i: i64 = 0;
    while i < proj.len {
        out[i] = moe_sigmoid(proj[i] as f64) as f32;
        i = i + 1;
    }
    return proj.len as i64;
}

// LLM_FFN_PAR with SILU: silu(gate) * up, elementwise.
export fn moe_ffn_par(gate: f32[:], up: f32[:], out: f32[:]) -> i64 {
    if up.len < gate.len || out.len < gate.len {
        return -1;
    }
    var i: i64 = 0;
    while i < gate.len {
        let x: f64 = gate[i] as f64;
        let s: f64 = x / (1.0 + exp64(0.0 - x));
        out[i] = (s * (up[i] as f64)) as f32;
        i = i + 1;
    }
    return gate.len as i64;
}

// The gated shared output: ffn_shexp * sigmoid(shared_gate).
export fn moe_shared_apply(shexp: f32[:], sg: f32[:], out: f32[:]) -> i64 {
    if sg.len < shexp.len || out.len < shexp.len {
        return -1;
    }
    var i: i64 = 0;
    while i < shexp.len {
        out[i] = (shexp[i] as f64 * sg[i] as f64) as f32;
        i = i + 1;
    }
    return shexp.len as i64;
}

// The two branches added, not averaged and not replaced.
export fn moe_combine(moe: f32[:], sh: f32[:], out: f32[:]) -> i64 {
    if sh.len < moe.len || out.len < moe.len {
        return -1;
    }
    var i: i64 = 0;
    while i < moe.len {
        out[i] = (moe[i] as f64 + sh[i] as f64) as f32;
        i = i + 1;
    }
    return moe.len as i64;
}
