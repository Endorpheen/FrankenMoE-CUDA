// C1-02 slice E: qwen4exp expert and token-mixer layout.
//
// Native anchors (pinned llama.cpp):
//   src/models/qwen4exp.cpp:139-145   per-layer n_ff_exp / n_ff_shexp resolution
//   src/models/qwen4exp.cpp:240-243   fused attention QKV call
//   src/models/qwen4exp.cpp:272-279   routed and shared expert tensors
//   src/models/qwen4exp.cpp:281       the head-width equality assert
//   src/llama-model.cpp:3138-3144     create_tensor_gate_up_exps
//   src/llama-model.cpp:3146-3180     create_tensor_qkv
//
// This slice owns the two layout decisions the loader takes from the file and
// not from the metadata: whether the routed experts are stored fused
// (ffn_gate_up_exps) or split (ffn_gate_exps + ffn_up_exps), and whether the
// attention projection is fused (attn_qkv) or split (attn_q/attn_k/attn_v).
// Both are "try the fused name first, fall back" in the native code, which is a
// presence probe, not a shape rule, so it has no home in canon/qwen4exp.goose.
//
// The width arithmetic reuses the accepted base/model_graph.goose helpers
// (n_ff_exp, n_ff_shexp, q_full_width) rather than restating them, so the
// expert widths cannot drift from the family baseline.

import base.model_graph;

const Q4E_HEAD_K: i64 = 0;
const Q4E_HEAD_V: i64 = 1;
const Q4E_Q: i64 = 2;
const Q4E_K: i64 = 3;
const Q4E_V: i64 = 4;

fn e_ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

// qwen4exp.cpp:139-140, delegated to the family baseline.
export fn q4e_n_ff_exp(declared: i64, n_ff: i64, n_expert_used: i64) -> i64 {
    return n_ff_exp(declared, n_ff, n_expert_used);
}

export fn q4e_n_ff_shexp(declared: i64, n_ff: i64) -> i64 {
    return n_ff_shexp(declared, n_ff);
}

// The routed-expert set: down is always present, gate_up is optional.
export fn q4e_down_shape(n_ff_exp: i64, n_embd: i64, n_expert: i64, out: i64[:]) -> i64 {
    e_ensure(out.len >= 3, 300);
    e_ensure(n_ff_exp >= 0 && n_embd >= 0 && n_expert >= 0, 301);
    out[0] = n_ff_exp;
    out[1] = n_embd;
    out[2] = n_expert;
    return 1;
}

// llama-model.cpp:3139 -- { n_embd, n_ff * 2, n_expert }, the fused pair.
export fn q4e_gate_up_shape(n_embd: i64, n_ff_exp: i64, n_expert: i64, out: i64[:]) -> i64 {
    e_ensure(out.len >= 3, 302);
    e_ensure(n_embd >= 0 && n_ff_exp >= 0 && n_expert >= 0, 303);
    out[0] = n_embd;
    out[1] = n_ff_exp * 2 as i64;
    out[2] = n_expert;
    return 1;
}

// llama-model.cpp:3141-3143 -- the split fallback, one tensor each.
export fn q4e_gate_shape(n_embd: i64, n_ff_exp: i64, n_expert: i64, out: i64[:]) -> i64 {
    e_ensure(out.len >= 3, 304);
    e_ensure(n_embd >= 0 && n_ff_exp >= 0 && n_expert >= 0, 305);
    out[0] = n_embd;
    out[1] = n_ff_exp;
    out[2] = n_expert;
    return 1;
}

// The presence probe result: 1 when ffn_gate_up_exps was found, 0 when the
// loader must take the split pair.
export fn q4e_gate_up_mode(has_fused: i64) -> i64 {
    if has_fused != 0 {
        return 1;
    }
    return 0;
}

// How many routed-expert tensors a layer declares for the given mode.
export fn q4e_routed_count(mode: i64) -> i64 {
    if mode != 0 {
        return 2; // gate_up + down
    }
    return 3; // gate + up + down
}

// qwen4exp.cpp:272-279 -- the shared expert set, in native declaration order:
// 0 gate_inp (1-D), 1 gate, 2 up, 3 down.
export fn q4e_shexp_shape(slot: i64, n_embd: i64, n_ff_shexp: i64, out: i64[:]) -> i64 {
    e_ensure(out.len >= 3, 306);
    e_ensure(n_embd >= 0 && n_ff_shexp >= 0, 307);
    if slot == 0 {
        out[0] = n_embd;
        out[1] = 1;
    } else if slot == 1 || slot == 2 {
        out[0] = n_embd;
        out[1] = n_ff_shexp;
    } else if slot == 3 {
        out[0] = n_ff_shexp;
        out[1] = n_embd;
    } else {
        return 0;
    }
    out[2] = 1;
    return 1;
}

export fn q4e_shexp_elems(n_embd: i64, n_ff_shexp: i64) -> i64 {
    e_ensure(n_embd >= 0 && n_ff_shexp >= 0, 308);
    return n_embd * n_ff_shexp;
}

export fn q4e_routed_elems(n_ff_exp: i64, n_embd: i64, n_expert: i64) -> i64 {
    e_ensure(n_ff_exp >= 0 && n_embd >= 0 && n_expert >= 0, 309);
    return n_ff_exp * n_embd * n_expert;
}

// qwen4exp.cpp:240 -- create_tensor_qkv(layer, il, n_embd,
// n_embd_head_k * n_head * 2, n_embd_k_gqa, n_embd_v_gqa, 0). The query
// projection is twice the head width because query and gate are interleaved.
export fn q4e_attn_widths(head_k: i64, n_head: i64, n_head_kv: i64, out: i64[:]) -> i64 {
    e_ensure(out.len >= 5, 310);
    e_ensure(head_k > 0 && n_head > 0 && n_head_kv > 0, 311);
    let q: i64 = q_full_width(head_k) * n_head;
    let k: i64 = head_k * n_head_kv;
    out[Q4E_HEAD_K] = head_k;
    out[Q4E_HEAD_V] = head_k;
    out[Q4E_Q] = q;
    out[Q4E_K] = k;
    out[Q4E_V] = k;
    return 1;
}

// llama-model.cpp:3149 -- the fused row width.
export fn q4e_qkv_width(n_embd_q: i64, n_embd_k: i64, n_embd_v: i64) -> i64 {
    e_ensure(n_embd_q >= 0 && n_embd_k >= 0 && n_embd_v >= 0, 312);
    return n_embd_q + n_embd_k + n_embd_v;
}

// llama-model.cpp:3152-3179 -- fused name first, split names otherwise.
export fn q4e_qkv_mode(has_fused: i64) -> i64 {
    if has_fused != 0 {
        return 1;
    }
    return 0;
}

// qwen4exp.cpp:281 -- GGML_ASSERT(n_embd_head_v() == n_embd_head_k()). The
// attention block refuses the file when this fails; the port reports it.
export fn q4e_head_dim_ok(n_embd_head_v: i64, n_embd_head_k: i64) -> i64 {
    if n_embd_head_v == n_embd_head_k {
        return 1;
    }
    return 0;
}

// Every built layer carries experts: the native loop creates them outside the
// attention/recurrent branch.
export fn q4e_experts_on(kind: i64) -> i64 {
    if kind >= 0 && kind <= 2 {
        return 1;
    }
    return 0;
}
