// C1-02 slice T: qwen4exp tensor map (name patterns, layer arity, suffix,
// bucket and loader flags).
//
// Native anchors (pinned llama.cpp):
//   src/models/qwen4exp.cpp:109-186   global tensors
//   src/models/qwen4exp.cpp:189-284   per-layer tensors
//   src/llama-arch.cpp:425-575        the "<arch>.<tensor>" name table
//   src/llama-arch.cpp:709-780        tensor bucket/op table
//   src/llama-model.cpp:3138-3150     create_tensor_gate_up_exps
//   src/llama-model.cpp:3146-3180     create_tensor_qkv
//
// The canon/qwen4exp.goose slice already assigns shapes by tensor id; what it
// does not carry is the mapping from those ids to the GGUF names, the fact that
// some of them are layer-formatted and some are globals, the suffix each one is
// declared with, and which of them the loader may omit, duplicate or read
// lazily. Without that mapping the id space cannot be joined to a file.
//
// Tensor ids are the canon/qwen4exp.goose ids, so integration is a rename-free
// join: q4_shape(tid) and q4t_pattern(tid) describe the same tensor.
//
// Boundaries recorded in REPORT.md: T_NEXTN_HC_HEAD_* have no entry in the
// pinned llama-arch.cpp; their names come from the integration tree
// (work/llama.cpp-integration/src/llama-arch.cpp:578-580) and the gate reports
// them as a separate group.

extern "q4t_fail" fn q4t_abort(code: i64);

fn q4t_ensure(v: bool, c: i64) {
    if !v {
        q4t_abort(c);
    }
}

fn put_str(s: const u8[:], out: u8[:]) -> i64 {
    if out.len < s.len {
        return -1;
    }
    var i: i64 = 0;
    while i < s.len {
        out[i] = s[i];
        i += 1;
    }
    return s.len;
}

const Q4T_LEN: i64 = 52;

// Which layer the name is formatted with.
const B_INPUT: i64 = 0;     // LLM_TENSOR_LAYER_INPUT, global
const B_REPEATING: i64 = 1; // LLM_TENSOR_LAYER_REPEATING, "blk.<il>."
const B_OUTPUT: i64 = 2;    // LLM_TENSOR_LAYER_OUTPUT, global

// Loader flags, as the bits llama_model_loader declares them.
const F_REQUIRED: i64 = 0;
const F_NOT_REQUIRED: i64 = 1;
const F_DUPLICATED: i64 = 2;
const F_READ_LAZY: i64 = 4;
const F_SKIP_IF_VIRTUAL: i64 = 8;

const SUF_WEIGHT: i64 = 0;
const SUF_BIAS: i64 = 1;
const SUF_NONE: i64 = 2;

export fn q4t_count() -> i64 {
    return Q4T_LEN;
}

// The name as llama-arch.cpp spells it. Layer-formatted entries keep the
// literal "%d" so the caller formats once, exactly like tn().str().
fn pattern(t: i64) -> const u8[:] {
    if t == 0  { return "token_embd"; }
    if t == 1  { return "output_hc_norm"; }
    if t == 2  { return "output_hc_down"; }
    if t == 3  { return "output_hc_up"; }
    if t == 4  { return "output"; }
    if t == 5  { return "per_layer_token_embd"; }
    if t == 6  { return "blk.%d.hc_attn_norm"; }
    if t == 7  { return "blk.%d.hc_attn_down"; }
    if t == 8  { return "blk.%d.hc_attn_up"; }
    if t == 9  { return "blk.%d.hc_attn_inject"; }
    if t == 10 { return "blk.%d.hc_ffn_norm"; }
    if t == 11 { return "blk.%d.hc_ffn_down"; }
    if t == 12 { return "blk.%d.hc_ffn_up"; }
    if t == 13 { return "blk.%d.hc_ffn_inject"; }
    if t == 14 { return "blk.%d.attn_qkv"; }
    if t == 15 { return "blk.%d.attn_output"; }
    if t == 16 { return "blk.%d.attn_q_norm"; }
    if t == 17 { return "blk.%d.attn_k_norm"; }
    if t == 18 { return "blk.%d.indexer.q_proj"; }
    if t == 19 { return "blk.%d.indexer.k_proj"; }
    if t == 20 { return "blk.%d.indexer.q_norm"; }
    if t == 21 { return "blk.%d.indexer.k_norm"; }
    if t == 22 { return "blk.%d.attn_qkv"; }
    if t == 23 { return "blk.%d.attn_gate"; }
    if t == 24 { return "blk.%d.ssm_conv1d"; }
    if t == 25 { return "blk.%d.ssm_dt"; }
    if t == 26 { return "blk.%d.ssm_a"; }
    if t == 27 { return "blk.%d.ssm_beta"; }
    if t == 28 { return "blk.%d.ssm_alpha"; }
    if t == 29 { return "blk.%d.ssm_norm"; }
    if t == 30 { return "blk.%d.ssm_out"; }
    if t == 31 { return "blk.%d.ple_key"; }
    if t == 32 { return "blk.%d.ple_value"; }
    if t == 33 { return "blk.%d.ple_norm_key"; }
    if t == 34 { return "blk.%d.ple_norm_query"; }
    if t == 35 { return "blk.%d.ple_norm_conv"; }
    if t == 36 { return "blk.%d.ple_conv1d"; }
    if t == 37 { return "blk.%d.ffn_gate_inp"; }
    if t == 38 { return "blk.%d.ffn_down_exps"; }
    if t == 39 { return "blk.%d.ffn_gate_up_exps"; }
    if t == 40 { return "blk.%d.ffn_gate_exps"; }
    if t == 41 { return "blk.%d.ffn_up_exps"; }
    if t == 42 { return "blk.%d.ffn_gate_inp_shexp"; }
    if t == 43 { return "blk.%d.ffn_gate_shexp"; }
    if t == 44 { return "blk.%d.ffn_up_shexp"; }
    if t == 45 { return "blk.%d.ffn_down_shexp"; }
    if t == 46 { return "blk.%d.nextn.eh_proj"; }
    if t == 47 { return "blk.%d.nextn.enorm"; }
    if t == 48 { return "blk.%d.nextn.hnorm"; }
    if t == 49 { return "blk.%d.nextn.hc_head_norm"; }
    if t == 50 { return "blk.%d.nextn.hc_head_down"; }
    if t == 51 { return "blk.%d.nextn.hc_head_up"; }
    return "";
}

export fn q4t_pattern(t: i64, out: u8[:]) -> i64 {
    if t < 0 || t >= Q4T_LEN {
        return -1;
    }
    return put_str(pattern(t), out);
}

export fn q4t_has_layer(t: i64) -> i64 {
    if t < 0 || t >= Q4T_LEN {
        return -1;
    }
    if t <= 5 {
        return 0;
    }
    return 1;
}

// Bucket from the llama-arch.cpp tensor table.
export fn q4t_bucket(t: i64) -> i64 {
    if t < 0 || t >= Q4T_LEN {
        return -1;
    }
    if t == 0 || t == 5 {
        return B_INPUT;
    }
    if t == 1 || t == 2 || t == 3 || t == 4 {
        return B_OUTPUT;
    }
    return B_REPEATING;
}

export fn q4t_suffix(t: i64) -> i64 {
    if t < 0 || t >= Q4T_LEN {
        return -1;
    }
    // ssm_dt is the only bias, ssm_a is declared with no suffix at all
    // (tn(LLM_TENSOR_SSM_A_NOSCAN, il)).
    if t == 25 {
        return SUF_BIAS;
    }
    if t == 26 {
        return SUF_NONE;
    }
    return SUF_WEIGHT;
}

// What the loader is allowed to do with the entry.
export fn q4t_flags(t: i64) -> i64 {
    if t < 0 || t >= Q4T_LEN {
        return -1;
    }
    if t == 4 {
        return F_NOT_REQUIRED;   // output falls back to a duplicated token_embd
    }
    if t == 5 {
        return F_READ_LAZY;      // its row count is only known after the read
    }
    if t == 39 {
        return F_NOT_REQUIRED;   // gate_up is optional: gate + up are the fallback
    }
    return F_REQUIRED;
}

// create_tensor_qkv passes TENSOR_SKIP_IF_VIRTUAL on the fused form.
export fn q4t_skip_if_virtual(t: i64) -> i64 {
    if t == 14 || t == 22 {
        return 1;
    }
    return 0;
}

// qwen4exp.cpp:115-118 -- a missing output re-registers token_embd as
// duplicated rather than adding a second name.
export fn q4t_output_fallback() -> i64 {
    return 1;
}

// Which tensors a layer kind declares (q4_shape answers the same question
// through its return value; this is the presence contract without the
// arithmetic, so a table diff of both is a free cross-check).
// The per-layer PLE table is global but only exists when the PLE key group is
// present, so it is not a function of the layer kind.
export fn q4t_present_ple_table(ple_heads: i64) -> i64 {
    if ple_heads > 0 {
        return 1;
    }
    return 0;
}

export fn q4t_present(kind: i64, t: i64) -> i64 {
    q4t_ensure(t >= 0 && t < Q4T_LEN, 210);
    if kind < 0 || kind > 2 {
        return 0;
    }
    // globals; the PLE table is answered by q4t_present_ple_table
    if t <= 5 {
        if t == 5 {
            return 0;
        }
        return 1;
    }
    // hyper-connections: two modules on every built layer
    if t >= 6 && t <= 13 {
        return 1;
    }
    if t >= 14 && t <= 21 {
        if kind == 1 {
            return 0; // recurrent layers own no attention and no indexer
        }
        return 1;
    }
    if t >= 22 && t <= 30 {
        if kind == 1 {
            return 1; // the gated delta net belongs to the recurrent layer only
        }
        return 0;
    }
    if t >= 31 && t <= 36 {
        if kind == 1 {
            return 1; // PLE rides the recurrent layer, gated by q4_is_ple
        }
        return 0;
    }
    if t >= 46 {
        if kind == 2 {
            return 1; // nextn head tensors exist only on the MTP layer
        }
        return 0;
    }
    return 1;         // experts on every built layer
}
