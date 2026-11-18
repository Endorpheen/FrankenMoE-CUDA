// Quantization allow-cuts and tensor-type overrides, ported from the policy
// half of src/llama-quant.cpp.
//
// Owned decisions:
//   - tensor_allows_quantization: the trivial gates (only_copy, tensor rank),
//     the name rules (suffix `weight`, norms, expert routing, small tensors,
//     positional embeddings and token types by architecture-specific names,
//     Mamba/Kimi conv1d, MiniMax indexers, RWKV time-mix, T5 rel-bias,
//     multimodal tensors, audio codebooks), and the two architecture blocks
//     (GLM5_NEXT, VLLM_DECISION2);
//   - the override pattern match used by llama_tensor_get_type: patterns in CLI
//     order, regex_search semantics, first match wins, only when the default
//     type is quantized;
//   - the decision priority: denied tensor, token-embedding type, output type,
//     manual override, standard logic.
//
// Architecture-specific tensor names (LLM_TN(arch)(POS_EMBD/TOKEN_TYPES)) stay
// with the caller, which owns the arch name table: the port receives the two
// names and only compares. Architecture ids are one flag word, bit 0 GLM5_NEXT,
// bit 1 VLLM_DECISION2.
//
// Pattern matching is a hand-written matcher for the ECMAScript subset that
// CLI tensor-type patterns use in practice: literals, `.`, `^`, `$`, character
// classes `[abc]`, `[a-z]`, `[^...]`, escapes `\d \D \w \W \s \S` and escaped
// punctuation, and the quantifiers `* + ? {m} {m,} {m,n}`. Groups, alternation,
// word boundaries and back references are reported as unsupported, and the
// caller must then fall back to std::regex for the whole decision. That keeps
// the port honest instead of silently mis-matching a pattern.

// ---- small helpers ---------------------------------------------------------

fn q_eq(a: const u8[:], b: const u8[:]) -> bool {
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

fn q_ends(s: const u8[:], suf: const u8[:]) -> bool {
    if suf.len > s.len {
        return false;
    }
    var i: i64 = 0;

    while i < suf.len {
        if s[s.len - suf.len + i] != suf[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn q_find(s: const u8[:], pat: const u8[:]) -> i64 {
    if pat.len == 0 || pat.len > s.len {
        return 0 - 1;
    }
    var i: i64 = 0;

    while i + pat.len <= s.len {
        var ok: bool = true;
        var k: i64 = 0;

        while k < pat.len {
            if s[i + k] != pat[k] {
                ok = false;
                k = pat.len;
            } else {
                k = k + 1;
            }
        }
        if ok {
            return i;
        }
        i = i + 1;
    }
    return 0 - 1;
}

fn q_starts(s: const u8[:], pre: const u8[:]) -> bool {
    if pre.len > s.len {
        return false;
    }
    var i: i64 = 0;

    while i < pre.len {
        if s[i] != pre[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn q_is_digit(c: u8) -> bool {
    return c >= 48 && c <= 57;
}

fn q_is_word(c: u8) -> bool {
    return q_is_digit(c) || (c >= 65 && c <= 90) || (c >= 97 && c <= 122) || c == 95;
}

fn q_is_space(c: u8) -> bool {
    return c == 32 || c == 9 || c == 10 || c == 13 || c == 11 || c == 12;
}

// ---- flat pattern matcher --------------------------------------------------

// Atom at p[pi]: its byte length and whether it matches c.
fn q_atom_len(p: const u8[:], pi: i64) -> i64 {
    if pi >= p.len {
        return 0;
    }
    if p[pi] == 92 {
        if pi + 1 < p.len {
            return 2;
        }
        return 1;
    }
    if p[pi] == 91 {
        var i: i64 = pi + 1;

        if i < p.len && p[i] == 94 {
            i = i + 1;
        }
        if i < p.len && p[i] == 93 {
            i = i + 1;
        }
        while i < p.len {
            if p[i] == 92 {
                i = i + 2;
            } else if p[i] == 93 {
                return i + 1 - pi;
            } else {
                i = i + 1;
            }
        }
        return 0 - 1;
    }
    return 1;
}

fn q_esc_match(c: u8, e: u8) -> bool {
    if e == 100 {
        return q_is_digit(c);
    }
    if e == 68 {
        return !q_is_digit(c);
    }
    if e == 119 {
        return q_is_word(c);
    }
    if e == 87 {
        return !q_is_word(c);
    }
    if e == 115 {
        return q_is_space(c);
    }
    if e == 83 {
        return !q_is_space(c);
    }
    return c == e;
}

fn q_class_match(p: const u8[:], pi: i64, alen: i64, c: u8) -> bool {
    var i: i64 = pi + 1;
    let end: i64 = pi + alen - 1;
    var neg: bool = false;

    if i < end && p[i] == 94 {
        neg = true;
        i = i + 1;
    }
    if i < end && p[i] == 93 {
        if c == 93 {
            return !neg;
        }
        i = i + 1;
    }
    var hit: bool = false;

    while i < end {
        if p[i] == 92 && i + 1 < end {
            if q_esc_match(c, p[i + 1]) {
                hit = true;
            }
            i = i + 2;
        } else if i + 2 < end && p[i + 1] == 45 {
            // range a-z
            var x: i64 = p[i] as i64;
            let y: i64 = p[i + 2] as i64;
            var v: i64 = c as i64;

            if v >= x && v <= y {
                hit = true;
            }
            i = i + 3;
        } else {
            if p[i] == c {
                hit = true;
            }
            i = i + 1;
        }
    }
    if neg {
        return !hit;
    }
    return hit;
}

fn q_atom_match(p: const u8[:], pi: i64, c: u8) -> bool {
    let alen = q_atom_len(p, pi);

    if alen <= 0 {
        return false;
    }
    if p[pi] == 46 {
        return c != 10 && c != 13;
    }
    if p[pi] == 91 {
        return q_class_match(p, pi, alen, c);
    }
    if p[pi] == 92 && alen == 2 {
        return q_esc_match(c, p[pi + 1]);
    }
    return c == p[pi];
}

// Parse `{m}`, `{m,}`, `{m,n}` starting at pi; returns the byte length or -1.
fn q_brace_len(p: const u8[:], pi: i64, out_min: i64[:], out_max: i64[:]) -> i64 {
    var i: i64 = pi + 1;
    var mn: i64 = 0;
    var mx: i64 = 0;
    var has_mn: bool = false;
    var has_mx: bool = false;

    while i < p.len && q_is_digit(p[i]) {
        mn = mn * 10 + (p[i] as i64 - 48);
        has_mn = true;
        i = i + 1;
    }
    if !has_mn {
        return 0 - 1;
    }
    mx = mn;
    has_mx = true;
    if i < p.len && p[i] == 44 {
        i = i + 1;
        if i < p.len && p[i] == 125 {
            has_mx = false;
        } else {
            mx = 0;
            var any: bool = false;

            while i < p.len && q_is_digit(p[i]) {
                mx = mx * 10 + (p[i] as i64 - 48);
                any = true;
                i = i + 1;
            }
            if !any {
                return 0 - 1;
            }
        }
    }
    if i >= p.len || p[i] != 125 {
        return 0 - 1;
    }
    out_min[0] = mn;
    if has_mx {
        out_max[0] = mx;
    } else {
        out_max[0] = 0 - 1;
    }
    return i + 1 - pi;
}

// Is the pattern inside the supported subset?
fn q_supported(p: const u8[:]) -> i64 {
    var i: i64 = 0;

    while i < p.len {
        let c = p[i];

        if c == 40 || c == 41 || c == 124 {
            return 0;
        }
        if c == 92 {
            if i + 1 >= p.len {
                return 0;
            }
            let e = p[i + 1];

            if e == 98 || e == 66 || e == 65 || e == 122 || e == 90 || q_is_digit(e) {
                return 0;
            }
            i = i + 2;
        } else if c == 91 {
            let alen = q_atom_len(p, i);

            if alen < 0 {
                return 0;
            }
            var k: i64 = i + 1;

            while k < i + alen - 1 {
                if p[k] == 92 && k + 1 < i + alen - 1 {
                    k = k + 2;
                } else {
                    if p[k] == 91 && k + 1 < p.len && p[k + 1] == 58 {
                        return 0; // POSIX class
                    }
                    k = k + 1;
                }
            }
            i = i + alen;
        } else if c == 123 {
            var mn: i64[1] = [0];
            var mx: i64[1] = [0];
            let bl = q_brace_len(p, i, mn, mx);

            if bl < 0 {
                return 0;
            }
            i = i + bl;
        } else if c == 36 {
            if i + 1 != p.len {
                return 0;
            }
            i = i + 1;
        } else if c == 42 || c == 43 || c == 63 {
            // a quantifier may not start the pattern or follow another quantifier
            if i == 0 {
                return 0;
            }
            let prev = p[i - 1];

            if prev == 42 || prev == 43 || prev == 63 || prev == 125 {
                return 0;
            }
            i = i + 1;
        } else {
            i = i + 1;
        }
    }
    return 1;
}

// Match p[pi..] against s[si..]; the rest of the subject may stay unmatched
// (regex_search semantics). Quantifiers try every repetition count.
recursive fn q_here(p: const u8[:], pi: i64, s: const u8[:], si0: i64) -> i64 {
    var i: i64 = pi;
    var si: i64 = si0;

    while i < p.len {
        if p[i] == 94 {
            if si != 0 {
                return 0;
            }
            i = i + 1;
            continue;
        }
        if p[i] == 36 {
            if si == s.len {
                return 1;
            }
            return 0;
        }
        let alen = q_atom_len(p, i);

        if alen <= 0 {
            return 0;
        }
        var qlen: i64 = 0;
        var mn: i64 = 1;
        var mx: i64 = 1;
        var quant: bool = false;

        if i + alen < p.len {
            let q = p[i + alen];

            if q == 42 {
                mn = 0;
                mx = 0 - 1;
                quant = true;
                qlen = 1;
            } else if q == 43 {
                mn = 1;
                mx = 0 - 1;
                quant = true;
                qlen = 1;
            } else if q == 63 {
                mn = 0;
                mx = 1;
                quant = true;
                qlen = 1;
            } else if q == 123 {
                var mnb: i64[1] = [0];
                var mxb: i64[1] = [0];
                let bl = q_brace_len(p, i + alen, mnb, mxb);

                if bl < 0 {
                    return 0;
                }
                mn = mnb[0];
                mx = mxb[0];
                quant = true;
                qlen = bl;
            }
        }
        let rest: i64 = i + alen + qlen;

        if !quant {
            if si < s.len && q_atom_match(p, i, s[si]) {
                i = rest;
                si = si + 1;
                continue;
            }
            return 0;
        }
        // count how many times the atom matches in a row
        var k: i64 = 0;

        while si + k < s.len && q_atom_match(p, i, s[si + k]) {
            k = k + 1;
        }
        if mx >= 0 && k > mx {
            k = mx;
        }
        // try the longest repetition first, then shorter ones
        while k >= mn {
            if q_here(p, rest, s, si + k) != 0 {
                return 1;
            }
            k = k - 1;
        }
        return 0;
    }
    // ran off the end of the pattern: it matched
    return 1;
}

// regex_search over the supported subset: 1 match, 0 no match, -1 unsupported.
fn q_search(p: const u8[:], s: const u8[:]) -> i64 {
    if q_supported(p) == 0 {
        return 0 - 1;
    }
    if p.len > 0 && p[0] == 94 {
        return q_here(p, 0, s, 0);
    }
    var si: i64 = 0;

    while si <= s.len {
        if q_here(p, 0, s, si) != 0 {
            return 1;
        }
        si = si + 1;
    }
    return 0;
}

// ---- allow-cuts ------------------------------------------------------------

fn q_allows(
    only_copy: i64,
    quantize_output: i64,
    n_dims: i64,
    arch_flags: i64,
    pos_embd: const u8[:],
    token_types: const u8[:],
    name: const u8[:]) -> i64 {
    if only_copy != 0 {
        return 0;
    }
    if n_dims < 2 {
        return 0;
    }
    var ok: bool = q_ends(name, "weight");

    if q_find(name, "_norm.weight") >= 0 {
        ok = false;
    }
    if quantize_output == 0 && q_eq(name, "output.weight") {
        ok = false;
    }
    if q_find(name, "ffn_gate_inp.weight") >= 0 {
        ok = false;
    }
    if q_find(name, "ffn_gate_tid2eid.weight") >= 0 {
        ok = false;
    }
    if q_find(name, "altup") >= 0 {
        ok = false;
    }
    if q_find(name, "laurel") >= 0 {
        ok = false;
    }
    if q_find(name, "per_layer_model_proj") >= 0 {
        ok = false;
    }
    if q_eq(name, pos_embd) || q_eq(name, token_types) {
        ok = false;
    }
    if q_find(name, "ssm_conv1d") >= 0 {
        ok = false;
    }
    if q_find(name, "shortconv.conv.weight") >= 0 {
        ok = false;
    }
    if q_find(name, "indexer.k_proj.weight") >= 0 || q_find(name, "indexer.q_proj.weight") >= 0 {
        ok = false;
    }
    // GLM5_NEXT
    if (arch_flags & 1) != 0 {
        if q_find(name, "hc_") >= 0 ||
           q_find(name, "indexer.attn_q_b") >= 0 ||
           q_find(name, "indexer.attn_k") >= 0 ||
           q_find(name, "indexer.proj") >= 0 ||
           q_find(name, "indexer_compressor_gate") >= 0 ||
           q_find(name, "indexer_compressor_ape") >= 0 ||
           q_find(name, "ssm_f_a.weight") >= 0 ||
           q_find(name, "ssm_f_b.weight") >= 0 ||
           q_find(name, "ssm_g_a.weight") >= 0 ||
           q_find(name, "ssm_g_b.weight") >= 0 ||
           q_find(name, "ssm_beta.weight") >= 0 ||
           q_find(name, "attn_kv_a_mqa.weight") >= 0 ||
           q_find(name, "attn_k_b.weight") >= 0 ||
           q_find(name, "attn_v_b.weight") >= 0 {
            ok = false;
        }
    }
    // VLLM_DECISION2
    if (arch_flags & 2) != 0 {
        if q_starts(name, "decision.") {
            ok = false;
        }
    }
    if q_find(name, "time_mix_first.weight") >= 0 ||
       q_find(name, "time_mix_w0.weight") >= 0 ||
       q_find(name, "time_mix_w1.weight") >= 0 ||
       q_find(name, "time_mix_w2.weight") >= 0 ||
       q_find(name, "time_mix_v0.weight") >= 0 ||
       q_find(name, "time_mix_v1.weight") >= 0 ||
       q_find(name, "time_mix_v2.weight") >= 0 ||
       q_find(name, "time_mix_a0.weight") >= 0 ||
       q_find(name, "time_mix_a1.weight") >= 0 ||
       q_find(name, "time_mix_a2.weight") >= 0 ||
       q_find(name, "time_mix_g1.weight") >= 0 ||
       q_find(name, "time_mix_g2.weight") >= 0 ||
       q_find(name, "time_mix_decay_w1.weight") >= 0 ||
       q_find(name, "time_mix_decay_w2.weight") >= 0 ||
       q_find(name, "time_mix_lerp_fused.weight") >= 0 {
        ok = false;
    }
    if q_find(name, "attn_rel_b.weight") >= 0 {
        ok = false;
    }
    if q_find(name, ".position_embd") >= 0 ||
       q_find(name, "sam.pos_embd") >= 0 ||
       q_find(name, "sam.neck.") >= 0 ||
       q_find(name, "sam.net_") >= 0 ||
       q_find(name, ".rel_pos") >= 0 ||
       q_find(name, ".patch_embd") >= 0 ||
       q_find(name, ".patch_merger") >= 0 {
        ok = false;
    }
    if q_find(name, "a.rvq.codebook") >= 0 || q_find(name, "mm.a.code_embd") >= 0 {
        ok = false;
    }
    if ok {
        return 1;
    }
    return 0;
}

// First matching override in CLI order; -1 none, -2 unsupported pattern seen
// before any match (the caller falls back to std::regex for the whole decision).
fn q_override(
    name: const u8[:],
    blob: const u8[:],
    off: i64[:],
    len: i64[:],
    ty: i64[:],
    n: i64,
    out_type: i64[:]) -> i64 {
    var i: i64 = 0;

    while i < n {
        let pat = blob[off[i] .. off[i] + len[i]];
        let m = q_search(pat, name);

        if m < 0 {
            return 0 - 2;
        }
        if m != 0 {
            out_type[0] = ty[i];
            return i;
        }
        i = i + 1;
    }
    return 0 - 1;
}

// Decision source of llama_tensor_get_type, before shape fallback:
// 0 tensor type (denied), 1 token_embedding_type, 2 output_tensor_type,
// 3 manual override, 4 default/standard logic.
fn q_priority(
    allows: i64,
    category: i64,
    token_embd_type: i64,
    output_type: i64,
    default_is_quantized: i64,
    is_per_layer_token_embd: i64,
    name: const u8[:],
    blob: const u8[:],
    off: i64[:],
    len: i64[:],
    ty: i64[:],
    n: i64,
    out_type: i64[:]) -> i64 {
    let TYPE_COUNT: i64 = 43;
    let CAT_TOKEN_EMBD: i64 = 0;
    let CAT_OUTPUT: i64 = 12;

    if allows == 0 {
        return 0;
    }
    if token_embd_type < TYPE_COUNT && category == CAT_TOKEN_EMBD {
        var named: bool = false;

        if is_per_layer_token_embd != 0 {
            let r = q_override(name, blob, off, len, ty, n, out_type);

            if r >= 0 {
                named = true;
            }
        }
        if !named {
            out_type[0] = token_embd_type;
            return 1;
        }
    }
    if output_type < TYPE_COUNT && category == CAT_OUTPUT {
        out_type[0] = output_type;
        return 2;
    }
    if default_is_quantized != 0 {
        let r = q_override(name, blob, off, len, ty, n, out_type);

        if r >= 0 {
            return 3;
        }
        if r == 0 - 2 {
            return 0 - 2;
        }
    }
    return 4;
}

// ---- exports ---------------------------------------------------------------

export fn qr_search(pattern: const u8[:], subject: const u8[:]) -> i64 {
    return q_search(pattern, subject);
}

export fn qa_allows(
    only_copy: i64,
    quantize_output: i64,
    n_dims: i64,
    arch_flags: i64,
    pos_embd: const u8[:],
    token_types: const u8[:],
    name: const u8[:]) -> i64 {
    return q_allows(only_copy, quantize_output, n_dims, arch_flags, pos_embd, token_types, name);
}

export fn qa_override(
    name: const u8[:],
    blob: const u8[:],
    off: i64[:],
    len: i64[:],
    ty: i64[:],
    n: i64,
    out_type: i64[:]) -> i64 {
    return q_override(name, blob, off, len, ty, n, out_type);
}

export fn qa_priority(
    allows: i64,
    category: i64,
    token_embd_type: i64,
    output_type: i64,
    default_is_quantized: i64,
    is_per_layer_token_embd: i64,
    name: const u8[:],
    blob: const u8[:],
    off: i64[:],
    len: i64[:],
    ty: i64[:],
    n: i64,
    out_type: i64[:]) -> i64 {
    return q_priority(allows, category, token_embd_type, output_type, default_is_quantized,
                      is_per_layer_token_embd, name, blob, off, len, ty, n, out_type);
}
