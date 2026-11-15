// P10-09 S2: the tiny-MoE fixture plan, ported from the generator the runtime
// tests depend on.
//
// Native source at base commit 70de805c:
//   runtime/tests/CMakeLists.txt   the two fixtures the gates need
//                                  (qwen3moe, gemma4), the split-shard case
//                                  (51 tensors at 20 per shard), and the
//                                  python3-present skip;
//   upstream/BigMoeOnEdge/scripts/make-tiny-moe.py
//                                  the whole fixture: architecture constants,
//                                  the ordered tensor plan with its RNG seeds,
//                                  the metadata and tokenizer calls, the
//                                  writer construction (split_max_tensors,
//                                  small_first_shard), the refusal of a split
//                                  gemma4, and the "wrote ..." banner.
//
// Scope: the PLAN, not the bytes. Weight values are random by construction and
// belong to the gguf writer and the RNG; this module decides which tensors the
// fixture has, in which order, with which shapes and seeds, which metadata the
// loader will see, and whether the requested mode is legal. The gate runs the
// real generator with recording stand-ins for gguf and numpy and compares every
// row, so the port is checked against the native code path rather than against
// a transcription.
//
// Determinism matters here: the byte-identity gates compare a streamed run with
// a resident run of the SAME file, and the CMake comment relies on 51 tensors
// at 20 per shard producing exactly -00001-of-00004. Both facts are asserted
// below (fx_tensor_count, fx_shards) and gated.

import std;

const FX_QWEN: i64 = 0;
const FX_GEMMA: i64 = 1;

// Architecture constants, make-tiny-moe.py:31-44.
const FX_N_LAYER: i64 = 4;
const FX_N_EMBD: i64 = 128;
const FX_N_HEAD: i64 = 4;
const FX_N_HEAD_KV: i64 = 2;
const FX_N_EMBD_HEAD: i64 = 32;  // N_EMBD // N_HEAD
const FX_N_EMBD_GQA: i64 = 64;   // N_HEAD_KV * N_EMBD_HEAD
const FX_N_FF: i64 = 256;
const FX_N_EXPERT: i64 = 8;
const FX_N_EXPERT_USED: i64 = 2;
const FX_N_FF_EXP: i64 = 128;
const FX_N_CTX: i64 = 256;
const FX_VOCAB: i64 = 259;       // 3 specials + 256 byte tokens

const FX_DENSE_LAYER: i64 = 0;   // the one dense gemma4 layer
const FX_FULL_ATTN_LAYER: i64 = 2;

// ── text helpers ────────────────────────────────────────────────────────────

fn fx_put_s(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    var i: i64 = 0;
    while i < s.len {
        dst[at + i] = s[i];
        i = i + 1;
    }
    return at + s.len;
}

fn fx_put_int(dst: u8[:], at: i64, v: i64) -> i64 {
    var p = at;
    var x = v;
    if x < 0 {
        dst[p] = 45;
        p = p + 1;
        x = 0 - x;
    }
    if x == 0 {
        dst[p] = 48;
        return p + 1;
    }
    var buf: u8[24] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var n: i64 = 0;
    while x > 0 {
        buf[n] = (48 + (x % 10)) as u8;
        x = x / 10;
        n = n + 1;
    }
    var i = n;
    while i > 0 {
        i = i - 1;
        dst[p] = buf[i];
        p = p + 1;
    }
    return p;
}

fn fx_put_bool(dst: u8[:], at: i64, v: i64) -> i64 {
    if v != 0 {
        return fx_put_s(dst, at, "True");
    }
    return fx_put_s(dst, at, "False");
}

fn fx_eq(a: const u8[:], b: const u8[:]) -> bool {
    if a.len != b.len {
        return false;
    }
    var i: i64 = 0;
    while i < a.len {
        if a[i] != b[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

// ── architecture selection and mode guards ──────────────────────────────────

export fn fx_arch(name: const u8[:]) -> i64 {
    if fx_eq(name, "qwen3moe") {
        return FX_QWEN;
    }
    if fx_eq(name, "gemma4") {
        return FX_GEMMA;
    }
    return 0 - 1;
}

// make-tiny-moe.py:236-238 — a split gemma4 fixture is refused outright, so a
// caller asking for one gets an error, not a silently unsplit file.
// Returns 0 when the mode is legal, 1 when it is refused; the message goes to
// out when refused.
export fn fx_split_guard(arch: i64, split_max_tensors: i64, out: u8[:]) -> i64 {
    if arch == FX_GEMMA && split_max_tensors != 0 {
        let msg: const u8[:] = "--split-max-tensors is exercised via the qwen3moe fixture only";
        fx_put_s(out, 0, msg);
        return 1;
    }
    return 0;
}

// make-tiny-moe.py:83-90 — a sharded writer always requests a metadata-only
// first shard (small_first_shard=True), which is the layout the fixtures rely
// on for the split-shard gate.
export fn fx_small_first(split_max_tensors: i64) -> i64 {
    if split_max_tensors != 0 {
        return 1;
    }
    return 0;
}

// runtime/tests/CMakeLists.txt: the qwen3moe fixture is 51 tensors; with 20 per
// shard and a metadata-only first shard that is exactly four files.
export fn fx_tensor_count(arch: i64) -> i64 {
    if arch == FX_QWEN {
        return 3 + FX_N_LAYER * 12;
    }
    return 3 + 13 + (FX_N_LAYER - 1) * 20;
}

export fn fx_shards(n_tensors: i64, per_shard: i64, small_first: i64) -> i64 {
    if per_shard <= 0 {
        return 0;
    }
    var n: i64 = n_tensors / per_shard;
    if n_tensors % per_shard != 0 {
        n = n + 1;
    }
    if n == 0 {
        n = 1;
    }
    if small_first != 0 {
        return n + 1;
    }
    return n;
}

// ── the tensor plan ─────────────────────────────────────────────────────────

fn fx_put_blk(dst: u8[:], at: i64, layer: i64) -> i64 {
    var p = fx_put_s(dst, at, "blk.");
    p = fx_put_int(dst, p, layer);
    return fx_put_s(dst, p, ".");
}

// One row: `<name><suffix> <d0>x<d1>[x<d2>] <seed>`.
fn fx_row(
    dst: u8[:],
    layer: i64,
    suffix: const u8[:],
    d0: i64,
    d1: i64,
    d2: i64,
    seed: i64,
) -> i64 {
    var p: i64 = 0;
    if layer >= 0 {
        p = fx_put_blk(dst, p, layer);
    }
    p = fx_put_s(dst, p, suffix);
    dst[p] = 32;
    p = p + 1;
    p = fx_put_int(dst, p, d0);
    if d1 > 0 {
        dst[p] = 120;
        p = p + 1;
        p = fx_put_int(dst, p, d1);
    }
    if d2 > 0 {
        dst[p] = 120;
        p = p + 1;
        p = fx_put_int(dst, p, d2);
    }
    dst[p] = 32;
    p = p + 1;
    p = fx_put_int(dst, p, seed);
    return p;
}

// The tensor at position i, layer entry k within its layer. Shapes are numpy
// shapes, exactly as the native calls construct them (ggml reverses them when
// the writer consumes them).
export fn fx_tensor(arch: i64, i: i64, out: u8[:]) -> i64 {
    let n: i64 = fx_tensor_count(arch);
    if arch < 0 || i < 0 || i >= n {
        return 0 - 1;
    }
    // Globals come first and differ per architecture.
    if i == 0 {
        return fx_row(out, 0 - 1, "token_embd.weight", FX_VOCAB, FX_N_EMBD, 0, 1);
    }
    if i == 1 {
        return fx_row(out, 0 - 1, "output_norm.weight", FX_N_EMBD, 0, 0, 2);
    }
    if i == 2 {
        if arch == FX_QWEN {
            return fx_row(out, 0 - 1, "output.weight", FX_VOCAB, FX_N_EMBD, 0, 3);
        }
        // gemma4 ties its output to token_embd and shares one rope_freqs table.
        return fx_row(out, 0 - 1, "rope_freqs.weight", FX_N_EMBD_HEAD / 2, 0, 0, 3);
    }
    var layer: i64 = 0;
    var k: i64 = 0;
    if arch == FX_QWEN {
        let j: i64 = i - 3;
        layer = j / 12;
        k = j % 12;
    } else {
        let j: i64 = i - 3;
        if j < 13 {
            layer = 0;
            k = j;
        } else {
            let t: i64 = j - 13;
            layer = 1 + t / 20;
            k = t % 20;
        }
    }
    let s: i64 = 100 + 100 * layer;
    // The first 13 entries of a gemma4 layer and the 6 attention entries of a
    // qwen3moe layer are the same in both architectures.
    if k == 0 {
        return fx_row(out, layer, "attn_norm.weight", FX_N_EMBD, 0, 0, s);
    }
    if k == 1 {
        return fx_row(out, layer, "attn_q.weight", FX_N_EMBD_HEAD * FX_N_HEAD, FX_N_EMBD, 0, s + 1);
    }
    if k == 2 {
        return fx_row(out, layer, "attn_k.weight", FX_N_EMBD_GQA, FX_N_EMBD, 0, s + 2);
    }
    if k == 3 {
        return fx_row(out, layer, "attn_v.weight", FX_N_EMBD_GQA, FX_N_EMBD, 0, s + 3);
    }
    if k == 4 {
        return fx_row(out, layer, "attn_output.weight", FX_N_EMBD, FX_N_EMBD_HEAD * FX_N_HEAD, 0, s + 4);
    }
    if k == 5 {
        return fx_row(out, layer, "attn_q_norm.weight", FX_N_EMBD_HEAD, 0, 0, s + 5);
    }
    if k == 6 {
        return fx_row(out, layer, "attn_k_norm.weight", FX_N_EMBD_HEAD, 0, 0, s + 6);
    }
    if k == 7 {
        // qwen3moe: ffn_norm; gemma4: post_attention_norm.
        if arch == FX_QWEN {
            return fx_row(out, layer, "ffn_norm.weight", FX_N_EMBD, 0, 0, s + 7);
        }
        return fx_row(out, layer, "post_attention_norm.weight", FX_N_EMBD, 0, 0, s + 7);
    }
    if k == 8 {
        // qwen3moe: the router; gemma4: ffn_norm.
        if arch == FX_QWEN {
            return fx_row(out, layer, "ffn_gate_inp.weight", FX_N_EXPERT, FX_N_EMBD, 0, s + 8);
        }
        return fx_row(out, layer, "ffn_norm.weight", FX_N_EMBD, 0, 0, s + 8);
    }
    if arch == FX_QWEN {
        // k 9,10,11 are the split expert tensors.
        if k == 9 {
            return fx_row(out, layer, "ffn_gate_exps.weight", FX_N_EXPERT, FX_N_FF_EXP, FX_N_EMBD, s + 9);
        }
        if k == 10 {
            return fx_row(out, layer, "ffn_down_exps.weight", FX_N_EXPERT, FX_N_EMBD, FX_N_FF_EXP, s + 10);
        }
        return fx_row(out, layer, "ffn_up_exps.weight", FX_N_EXPERT, FX_N_FF_EXP, FX_N_EMBD, s + 11);
    }
    // gemma4: the shared/dense FFN, then the resident extras, then the two
    // fused streamed expert tensors.
    if k == 9 {
        return fx_row(out, layer, "ffn_gate.weight", FX_N_FF, FX_N_EMBD, 0, s + 9);
    }
    if k == 10 {
        return fx_row(out, layer, "ffn_up.weight", FX_N_FF, FX_N_EMBD, 0, s + 10);
    }
    if k == 11 {
        return fx_row(out, layer, "ffn_down.weight", FX_N_EMBD, FX_N_FF, 0, s + 11);
    }
    if k == 12 {
        return fx_row(out, layer, "post_ffw_norm.weight", FX_N_EMBD, 0, 0, s + 12);
    }
    if k == 13 {
        return fx_row(out, layer, "ffn_gate_inp.weight", FX_N_EXPERT, FX_N_EMBD, 0, s + 13);
    }
    if k == 14 {
        return fx_row(out, layer, "ffn_gate_inp.scale", FX_N_EMBD, 0, 0, s + 14);
    }
    if k == 15 {
        return fx_row(out, layer, "pre_ffw_norm_2.weight", FX_N_EMBD, 0, 0, s + 15);
    }
    if k == 16 {
        return fx_row(out, layer, "post_ffw_norm_1.weight", FX_N_EMBD, 0, 0, s + 16);
    }
    if k == 17 {
        return fx_row(out, layer, "post_ffw_norm_2.weight", FX_N_EMBD, 0, 0, s + 17);
    }
    if k == 18 {
        return fx_row(out, layer, "ffn_gate_up_exps.weight", FX_N_EXPERT, 2 * FX_N_FF_EXP, FX_N_EMBD, s + 18);
    }
    return fx_row(out, layer, "ffn_down_exps.weight", FX_N_EXPERT, FX_N_EMBD, FX_N_FF_EXP, s + 19);
}

// ── the tokenizer ───────────────────────────────────────────────────────────

// make-tiny-moe.py:46-56 — 3 specials then 256 SPM byte tokens spelled
// `<0xNN>` with upper-case hex.
fn fx_vocab_token(i: i64, out: u8[:]) -> i64 {
    if i == 0 {
        return fx_put_s(out, 0, "<unk>");
    }
    if i == 1 {
        return fx_put_s(out, 0, "<s>");
    }
    if i == 2 {
        return fx_put_s(out, 0, "</s>");
    }
    let b: i64 = i - 3;
    var p: i64 = fx_put_s(out, 0, "<0x");
    let hi: i64 = b / 16;
    let lo: i64 = b % 16;
    if hi < 10 {
        out[p] = (48 + hi) as u8;
    } else {
        out[p] = (55 + hi) as u8;
    }
    p = p + 1;
    if lo < 10 {
        out[p] = (48 + lo) as u8;
    } else {
        out[p] = (55 + lo) as u8;
    }
    p = p + 1;
    out[p] = 62;
    return p + 1;
}

// gguf TokenType: UNKNOWN 2, CONTROL 3, BYTE 6.
fn fx_vocab_type(i: i64) -> i64 {
    if i == 0 {
        return 2;
    }
    if i < 3 {
        return 3;
    }
    return 6;
}

// ── the metadata plan ───────────────────────────────────────────────────────

export fn fx_meta_count(arch: i64) -> i64 {
    if arch == FX_QWEN {
        return 25 + 3; // 15 scalars, tokenizer, 7 ids/flags, 3 writes
    }
    return 20 + 10 + 3;
}

fn fx_put_vocab_list(dst: u8[:], p0: i64) -> i64 {
    var p: i64 = p0;
    dst[p] = 91;
    p = p + 1;
    var i: i64 = 0;
    while i < FX_VOCAB {
        if i != 0 {
            dst[p] = 44;
            p = p + 1;
        }
        var tok: u8[16] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
        let n: i64 = fx_vocab_token(i, tok);
        p = fx_put_s(dst, p, tok[0..n]);
        i = i + 1;
    }
    dst[p] = 93;
    return p + 1;
}

fn fx_put_scores_list(dst: u8[:], p0: i64) -> i64 {
    var p: i64 = p0;
    dst[p] = 91;
    p = p + 1;
    var i: i64 = 0;
    while i < FX_VOCAB {
        if i != 0 {
            dst[p] = 44;
            p = p + 1;
        }
        p = fx_put_s(dst, p, "0.0");
        i = i + 1;
    }
    dst[p] = 93;
    return p + 1;
}

fn fx_put_types_list(dst: u8[:], p0: i64) -> i64 {
    var p: i64 = p0;
    dst[p] = 91;
    p = p + 1;
    var i: i64 = 0;
    while i < FX_VOCAB {
        if i != 0 {
            dst[p] = 44;
            p = p + 1;
        }
        p = fx_put_int(dst, p, fx_vocab_type(i));
        i = i + 1;
    }
    dst[p] = 93;
    return p + 1;
}

// The scalar metadata every architecture writes first, index for index.
fn fx_put_common(dst: u8[:], i: i64) -> i64 {
    var p: i64 = 0;
    if i == 0 {
        return fx_put_s(dst, 0, "add_name tiny-moe");
    }
    if i == 1 {
        p = fx_put_s(dst, 0, "add_context_length ");
        return fx_put_int(dst, p, FX_N_CTX);
    }
    if i == 2 {
        p = fx_put_s(dst, 0, "add_embedding_length ");
        return fx_put_int(dst, p, FX_N_EMBD);
    }
    if i == 3 {
        p = fx_put_s(dst, 0, "add_block_count ");
        return fx_put_int(dst, p, FX_N_LAYER);
    }
    if i == 4 {
        p = fx_put_s(dst, 0, "add_feed_forward_length ");
        return fx_put_int(dst, p, FX_N_FF);
    }
    if i == 5 {
        p = fx_put_s(dst, 0, "add_head_count ");
        return fx_put_int(dst, p, FX_N_HEAD);
    }
    if i == 6 {
        p = fx_put_s(dst, 0, "add_head_count_kv ");
        return fx_put_int(dst, p, FX_N_HEAD_KV);
    }
    if i == 7 {
        p = fx_put_s(dst, 0, "add_key_length ");
        return fx_put_int(dst, p, FX_N_EMBD_HEAD);
    }
    if i == 8 {
        p = fx_put_s(dst, 0, "add_value_length ");
        return fx_put_int(dst, p, FX_N_EMBD_HEAD);
    }
    if i == 9 {
        return fx_put_s(dst, 0, "add_rope_freq_base 1000000.0");
    }
    if i == 10 {
        return fx_put_s(dst, 0, "add_layer_norm_rms_eps 1e-06");
    }
    if i == 11 {
        p = fx_put_s(dst, 0, "add_expert_count ");
        return fx_put_int(dst, p, FX_N_EXPERT);
    }
    if i == 12 {
        p = fx_put_s(dst, 0, "add_expert_used_count ");
        return fx_put_int(dst, p, FX_N_EXPERT_USED);
    }
    if i == 13 {
        p = fx_put_s(dst, 0, "add_expert_feed_forward_length ");
        return fx_put_int(dst, p, FX_N_FF_EXP);
    }
    // gguf.LlamaFileType.ALL_F32, make-tiny-moe.py:126.
    return fx_put_s(dst, 0, "add_file_type 0");
}

export fn fx_meta(arch: i64, i: i64, out: u8[:]) -> i64 {
    let n: i64 = fx_meta_count(arch);
    if arch < 0 || i < 0 || i >= n {
        return 0 - 1;
    }
    var p: i64 = 0;
    let base: i64 = 15;
    if i < base {
        return fx_put_common(out, i);
    }
    var k: i64 = i - base;
    if arch == FX_GEMMA {
        if k == 0 {
            p = fx_put_s(out, 0, "add_sliding_window_pattern [");
            var l: i64 = 0;
            while l < FX_N_LAYER {
                if l != 0 {
                    out[p] = 44;
                    p = p + 1;
                }
                p = fx_put_bool(out, p, if l != FX_FULL_ATTN_LAYER { 1 } else { 0 });
                l = l + 1;
            }
            out[p] = 93;
            return p + 1;
        }
        if k == 1 {
            p = fx_put_s(out, 0, "add_sliding_window ");
            return fx_put_int(out, p, FX_N_CTX);
        }
        if k == 2 {
            p = fx_put_s(out, 0, "add_key_length_swa ");
            return fx_put_int(out, p, FX_N_EMBD_HEAD);
        }
        if k == 3 {
            p = fx_put_s(out, 0, "add_value_length_swa ");
            return fx_put_int(out, p, FX_N_EMBD_HEAD);
        }
        if k == 4 {
            return fx_put_s(out, 0, "add_embedding_length_per_layer_input 0");
        }
        k = k - 5;
    }
    // add_tokenizer_* — the same seven calls for both architectures, the three
    // list-valued ones expanded from the vocabulary rules above.
    if k == 0 {
        return fx_put_s(out, 0, "add_tokenizer_model llama");
    }
    if k == 1 {
        return fx_put_s(out, 0, "add_tokenizer_pre default");
    }
    if k == 2 {
        p = fx_put_s(out, 0, "add_token_list ");
        return fx_put_vocab_list(out, p);
    }
    if k == 3 {
        p = fx_put_s(out, 0, "add_token_scores ");
        return fx_put_scores_list(out, p);
    }
    if k == 4 {
        p = fx_put_s(out, 0, "add_token_types ");
        return fx_put_types_list(out, p);
    }
    if k == 5 {
        return fx_put_s(out, 0, "add_unk_token_id 0");
    }
    if k == 6 {
        return fx_put_s(out, 0, "add_bos_token_id 1");
    }
    if k == 7 {
        return fx_put_s(out, 0, "add_eos_token_id 2");
    }
    if k == 8 {
        return fx_put_s(out, 0, "add_add_bos_token True");
    }
    if k == 9 {
        return fx_put_s(out, 0, "add_add_eos_token False");
    }
    if k == 10 {
        return fx_put_s(out, 0, "write_header_to_file ");
    }
    if k == 11 {
        return fx_put_s(out, 0, "write_kv_data_to_file ");
    }
    return fx_put_s(out, 0, "write_tensors_to_file ");
}

// ── the banner ──────────────────────────────────────────────────────────────

// The line the generator prints for the file it wrote, so the harness can name
// what it produced in a log without parsing the gguf.
export fn fx_banner(arch: i64, out_path: const u8[:], out: u8[:]) -> i64 {
    var p: i64 = fx_put_s(out, 0, "wrote ");
    p = fx_put_s(out, p, out_path);
    p = fx_put_s(out, p, ": ");
    if arch == FX_QWEN {
        p = fx_put_s(out, p, "qwen3moe, ");
        p = fx_put_int(out, p, FX_N_LAYER);
        p = fx_put_s(out, p, " layers, ");
        p = fx_put_int(out, p, FX_N_EXPERT);
        p = fx_put_s(out, p, " experts (top-");
        p = fx_put_int(out, p, FX_N_EXPERT_USED);
        p = fx_put_s(out, p, "), vocab ");
        return fx_put_int(out, p, FX_VOCAB);
    }
    p = fx_put_s(out, p, "gemma4, ");
    p = fx_put_int(out, p, FX_N_LAYER);
    p = fx_put_s(out, p, " layers (");
    p = fx_put_int(out, p, FX_N_LAYER - 1);
    p = fx_put_s(out, p, " MoE, fused gate_up), ");
    p = fx_put_int(out, p, FX_N_EXPERT);
    p = fx_put_s(out, p, " experts (top-");
    p = fx_put_int(out, p, FX_N_EXPERT_USED);
    p = fx_put_s(out, p, "), vocab ");
    return fx_put_int(out, p, FX_VOCAB);
}
