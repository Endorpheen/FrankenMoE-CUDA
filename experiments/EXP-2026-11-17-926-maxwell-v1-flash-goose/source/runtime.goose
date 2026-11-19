// Maxwell-V1-Flash ("maxwell_moe") decision module.
//
// Oracle: huggingface.co/maxwell-model/Maxwell-V1-Flash, sha ca536c43,
//   modeling_maxwell_moe.py (block-sparse GQA route, mHC Sinkhorn, sigmoid
//   router with expert bias, MTP roll) and configuration_maxwell_moe.py.
//
// The module owns the decisions that have no llama.cpp equivalent: block-route
// geometry, per-group block-max selection, Sinkhorn projection, hyper-connection
// gating and MTP index rolling. Matrix products and gathers stay on the host;
// they are ggml ops, not decisions.
//
// Every entry point returns 0 on success and a negative code on a rejected
// input; the host treats a negative return as a refusal and must not fall back
// to a different decision.

extern fn expf(x: f32) -> f32;
extern fn sqrtf(x: f32) -> f32;

const MX_NONE: i64 = -1;

fn mx_isfinite(x: f32) -> i64 {
    // NaN fails both comparisons; +-inf is rejected by the caller's range check.
    if x != x {
        return 0;
    }
    return 1;
}

fn mx_sigmoid(x: f32) -> f32 {
    let one: f32 = 1.0;
    return one / (one + expf(-x));
}

// ---------------------------------------------------------------------------
// Block-route geometry (modeling_maxwell_moe.py:432-446).
//
//   num_prefix_blocks = prefix_window_size // block_size
//   num_local_blocks  = local_window_size  // block_size
//
// The native __init__ rejects a non-positive block size and a window that is
// not block aligned; the same refusals are reproduced here.
//
// out[0] = num_prefix_blocks
// out[1] = num_local_blocks
// out[2] = query chunk size (positive)
// out[3] = num_remote_blocks (positive)
// ---------------------------------------------------------------------------
export fn mx_geom(prefix_win: i64, local_win: i64, blk: i64,
                  qchunk: i64, remote: i64, out: i64[:]) -> i64 {
    if out.len < 4 {
        return -1;
    }
    if blk <= 0 {
        return -2;
    }
    if qchunk <= 0 {
        return -3;
    }
    if prefix_win <= 0 {
        return -4;
    }
    if local_win <= 0 {
        return -5;
    }
    if prefix_win % blk != 0 {
        return -6;
    }
    if local_win % blk != 0 {
        return -7;
    }
    if remote <= 0 {
        return -8;
    }
    out[0] = prefix_win / blk;
    out[1] = local_win / blk;
    out[2] = qchunk;
    out[3] = remote;
    return 0;
}

// selected_count = min(max(num_remote_blocks, 1), num_blocks)
export fn mx_selected_count(remote: i64, num_blocks: i64) -> i64 {
    if num_blocks <= 0 {
        return 0;
    }
    var r: i64 = remote;
    if r < 1 {
        r = 1;
    }
    if r > num_blocks {
        r = num_blocks;
    }
    return r;
}

// ---------------------------------------------------------------------------
// Router (modeling_maxwell_moe.py:657-690).
//
//   scores = sigmoid(router_logits)                      // fp32
//   routing_score = scores + expert_bias                 // bias only selects
//   selected = topk(routing_score, k)
//   weights  = scores[selected] / sum(scores[selected])  // bias never scales
//
// logits.len == bias.len == n. idx and w hold k entries in descending score
// order; ties keep the lower expert index (the HF oracle's CPU order).
// ---------------------------------------------------------------------------
fn mx_sig(i: i64, logits: f32[:]) -> f32 {
    return mx_sigmoid(logits[i]);
}

export fn mx_router(logits: f32[:], bias: f32[:], topk: i64,
                    idx: i64[:], w: f32[:]) -> i64 {
    let n: i64 = logits.len;
    if n <= 0 {
        return -1;
    }
    if bias.len != n {
        return -2;
    }
    if topk <= 0 || topk > n {
        return -3;
    }
    if idx.len < topk || w.len < topk {
        return -4;
    }
    var k: i64 = 0;
    // Insertion selection: for each slot take the best not-yet-taken expert.
    while k < topk {
        var best: i64 = MX_NONE;
        var best_v: f32 = 0.0;
        var i: i64 = 0;
        while i < n {
            var taken: i64 = 0;
            var j: i64 = 0;
            while j < k {
                if idx[j] == i {
                    taken = 1;
                }
                j += 1;
            }
            if taken == 0 {
                let s: f32 = mx_sig(i, logits);
                let v: f32 = s + bias[i];
                if best == MX_NONE {
                    best = i;
                    best_v = v;
                } else {
                    if v > best_v {
                        best = i;
                        best_v = v;
                    } else {
                        if v == best_v && i < best {
                            best = i;
                        }
                    }
                }
            }
            i += 1;
        }
        if best == MX_NONE {
            return -5;
        }
        idx[k] = best;
        k += 1;
    }
    var sum: f32 = 0.0;
    k = 0;
    while k < topk {
        sum = sum + mx_sig(idx[k], logits);
        k += 1;
    }
    if !(sum > 0.0) {
        return -6;
    }
    k = 0;
    while k < topk {
        w[k] = mx_sig(idx[k], logits) / sum;
        k += 1;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// mHC Sinkhorn projection (modeling_maxwell_moe.py:421-455 + HyperConnectionModule).
//
//   M = softmax(logits (n x n, rows), dim=-1) + eps
//   M = M / (col_sum + eps)
//   repeat iters-1:  M = M / (row_sum + eps); M = M / (col_sum + eps)
//
// logits.len == n*n, out.len >= n*n. The softmax row is the HF view(b, s, n, n)
// last axis.
// ---------------------------------------------------------------------------
export fn mx_sinkhorn(logits: f32[:], n: i64, iters: i64, eps: f32,
                      out: f32[:]) -> i64 {
    let nn: i64 = n * n;
    if n <= 0 || iters <= 0 {
        return -1;
    }
    if logits.len < nn || out.len < nn {
        return -2;
    }
    var r: i64 = 0;
    while r < n {
        var m: f32 = logits[r * n];
        var c: i64 = 1;
        while c < n {
            let v: f32 = logits[r * n + c];
            if v > m {
                m = v;
            }
            c += 1;
        }
        var sum: f32 = 0.0;
        c = 0;
        while c < n {
            sum = sum + expf(logits[r * n + c] - m);
            c += 1;
        }
        if !(sum > 0.0) {
            return -3;
        }
        c = 0;
        while c < n {
            out[r * n + c] = expf(logits[r * n + c] - m) / sum + eps;
            c += 1;
        }
        r += 1;
    }
    // col norm
    var c2: i64 = 0;
    while c2 < n {
        var s: f32 = 0.0;
        r = 0;
        while r < n {
            s = s + out[r * n + c2];
            r += 1;
        }
        if !(s + eps > 0.0) {
            return -4;
        }
        r = 0;
        while r < n {
            out[r * n + c2] = out[r * n + c2] / (s + eps);
            r += 1;
        }
        c2 += 1;
    }
    var it: i64 = 1;
    while it < iters {
        r = 0;
        while r < n {
            var s: f32 = 0.0;
            var c: i64 = 0;
            while c < n {
                s = s + out[r * n + c];
                c += 1;
            }
            c = 0;
            while c < n {
                out[r * n + c] = out[r * n + c] / (s + eps);
                c += 1;
            }
            r += 1;
        }
        c2 = 0;
        while c2 < n {
            var s: f32 = 0.0;
            r = 0;
            while r < n {
                s = s + out[r * n + c2];
                r += 1;
            }
            r = 0;
            while r < n {
                out[r * n + c2] = out[r * n + c2] / (s + eps);
                r += 1;
            }
            c2 += 1;
        }
        it += 1;
    }
    return 0;
}

// r = 1 / (||x|| / sqrt(nC) + eps)   (HyperConnectionModule._compute_mappings)
export fn mx_hc_xnorm(x: f32[:], eps: f32) -> f32 {
    let nC: i64 = x.len;
    var sum: f32 = 0.0;
    var i: i64 = 0;
    while i < nC {
        sum = sum + x[i] * x[i];
        i += 1;
    }
    let norm: f32 = sqrtf(sum);
    return 1.0 / (norm / sqrtf(nC) + eps);
}

// h = xnorm * proj * alpha + bias, then split into the three gates.
//   h_pre  = sigmoid(h[:n]) + eps
//   h_post = sigmoid(h[n:2n]) * 2
//   h_res_logits = h[2n:]            (softmax rows happen in mx_sinkhorn)
// proj/alpha/bias hold n*n+2n entries. pre/post hold n, res_logits n*n.
export fn mx_hc_mix(xnorm: f32, proj: f32[:], alpha: f32[:], bias: f32[:],
                    n: i64, eps: f32, pre: f32[:], post: f32[:],
                    res_logits: f32[:]) -> i64 {
    if n <= 0 {
        return -1;
    }
    let mix: i64 = n * n + 2 * n;
    if proj.len < mix || alpha.len < mix || bias.len < mix {
        return -2;
    }
    if pre.len < n || post.len < n || res_logits.len < n * n {
        return -3;
    }
    var i: i64 = 0;
    while i < n {
        let hp: f32 = xnorm * proj[i] * alpha[i] + bias[i];
        pre[i] = mx_sigmoid(hp) + eps;
        let hq: f32 = xnorm * proj[n + i] * alpha[n + i] + bias[n + i];
        post[i] = mx_sigmoid(hq) * 2.0;
        i += 1;
    }
    i = 0;
    while i < n * n {
        res_logits[i] = xnorm * proj[2 * n + i] * alpha[2 * n + i] + bias[2 * n + i];
        i += 1;
    }
    return 0;
}

// weights = sigmoid(mixes * scale + base) + eps   (HCHead.forward)
export fn mx_hc_head(mixes: f32[:], scale: f32, base: f32, eps: f32,
                     out: f32[:]) -> i64 {
    let n: i64 = mixes.len;
    if out.len < n {
        return -1;
    }
    var i: i64 = 0;
    while i < n {
        out[i] = mx_sigmoid(mixes[i] * scale + base) + eps;
        i += 1;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Per-group block-max remote selection (_select_blocks).
//
// scores layout: [chunk][groups][blocks], already block-max reduced by the
// host. For query q (absolute position qpos[q]) a block is eligible when
//   block_start >= prefix_blocks * block_size
//   block_end   <= local_start_block * block_size
// where local_start_block = max(cur_block - local_blocks + 1, 0).
// Selected blocks keep descending score; -inf (and a missing block) yields -1.
// out layout: [chunk][groups][remote], zero-padded with -1.
// ---------------------------------------------------------------------------
fn mx_local_start(cur_block: i64, local_blocks: i64) -> i64 {
    var s: i64 = cur_block - local_blocks + 1;
    if s < 0 {
        s = 0;
    }
    return s;
}

export fn mx_route(scores: f32[:], n_blocks: i64, chunk_len: i64, n_groups: i64,
                   remote: i64, prefix_blocks: i64, local_blocks: i64,
                   qpos: i64[:], block_size: i64, out: i64[:]) -> i64 {
    if n_blocks <= 0 || chunk_len <= 0 || n_groups <= 0 || block_size <= 0 {
        return -1;
    }
    if remote <= 0 {
        return -2;
    }
    if qpos.len < chunk_len {
        return -3;
    }
    if scores.len < chunk_len * n_groups * n_blocks {
        return -4;
    }
    if out.len < chunk_len * n_groups * remote {
        return -5;
    }
    let selected: i64 = mx_selected_count(remote, n_blocks);
    var q: i64 = 0;
    while q < chunk_len {
        let cur_block: i64 = qpos[q] / block_size;
        let local_start: i64 = mx_local_start(cur_block, local_blocks) * block_size;
        let prefix_end: i64 = prefix_blocks * block_size;
        var g: i64 = 0;
        while g < n_groups {
            var k: i64 = 0;
            while k < remote {
                out[(q * n_groups + g) * remote + k] = MX_NONE;
                k += 1;
            }
            k = 0;
            while k < selected {
                var best: i64 = MX_NONE;
                var best_v: f32 = 0.0;
                var b: i64 = 0;
                while b < n_blocks {
                    var taken: i64 = 0;
                    var j: i64 = 0;
                    while j < k {
                        if out[(q * n_groups + g) * remote + j] == b {
                            taken = 1;
                        }
                        j += 1;
                    }
                    let bstart: i64 = b * block_size;
                    let bend: i64 = bstart + block_size;
                    var eligible: i64 = 0;
                    if bstart >= prefix_end && bend <= local_start {
                        eligible = 1;
                    }
                    if taken == 0 && eligible == 1 {
                        let v: f32 = scores[(q * n_groups + g) * n_blocks + b];
                        if mx_isfinite(v) == 1 && v > -1.0e38 {
                            if best == MX_NONE || v > best_v ||
                               (v == best_v && b < best) {
                                best = b;
                                best_v = v;
                            }
                        }
                    }
                    b += 1;
                }
                if best == MX_NONE {
                    // no eligible block with a finite score: keep -1 padding
                    k = selected;
                } else {
                    out[(q * n_groups + g) * remote + k] = best;
                    k += 1;
                }
            }
            g += 1;
        }
        q += 1;
    }
    return 0;
}

// Final route: cat(prefix, remote, local) per query and group.
//   prefix[i] = i                        if i <= cur_block else -1
//   local[j]  = cur_block + (1 - local_blocks + j)  if >= prefix_blocks else -1
// out layout: [chunk][groups][prefix_blocks + remote + local_blocks].
export fn mx_route_final(remote: i64[:], chunk_len: i64, n_groups: i64,
                         remote_n: i64, prefix_blocks: i64, local_blocks: i64,
                         qpos: i64[:], block_size: i64, out: i64[:]) -> i64 {
    if chunk_len <= 0 || n_groups <= 0 || remote_n <= 0 || block_size <= 0 {
        return -1;
    }
    let width: i64 = prefix_blocks + remote_n + local_blocks;
    if remote.len < chunk_len * n_groups * remote_n {
        return -2;
    }
    if qpos.len < chunk_len {
        return -3;
    }
    if out.len < chunk_len * n_groups * width {
        return -4;
    }
    var q: i64 = 0;
    while q < chunk_len {
        let cur_block: i64 = qpos[q] / block_size;
        var g: i64 = 0;
        while g < n_groups {
            let base: i64 = (q * n_groups + g) * width;
            var i: i64 = 0;
            while i < prefix_blocks {
                if i <= cur_block {
                    out[base + i] = i;
                } else {
                    out[base + i] = MX_NONE;
                }
                i += 1;
            }
            var j: i64 = 0;
            while j < remote_n {
                out[base + prefix_blocks + j] =
                    remote[(q * n_groups + g) * remote_n + j];
                j += 1;
            }
            j = 0;
            while j < local_blocks {
                let blk: i64 = cur_block + (1 - local_blocks + j);
                if blk >= prefix_blocks {
                    out[base + prefix_blocks + remote_n + j] = blk;
                } else {
                    out[base + prefix_blocks + remote_n + j] = MX_NONE;
                }
                j += 1;
            }
            g += 1;
        }
        q += 1;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// MTP input rolling (MaxwellMoeMTPLayer.forward).
//   rolled = roll(ids, -1); rolled[-1] = pad_token_id
// ---------------------------------------------------------------------------
export fn mx_mtp_roll(ids: i64[:], pad_id: i64, out: i64[:]) -> i64 {
    let n: i64 = ids.len;
    if n <= 0 {
        return -1;
    }
    if out.len < n {
        return -2;
    }
    var i: i64 = 0;
    while i < n - 1 {
        out[i] = ids[i + 1];
        i += 1;
    }
    out[n - 1] = pad_id;
    return 0;
}

// ---------------------------------------------------------------------------
// MBSA route bias: the blocks the reference always attends.
//
//   prefix: blocks 0 .. min(prefix_blocks-1, cur_block)
//   local : cur_block-local_blocks+1 .. cur_block, keeping only >= prefix_blocks
//
// out layout [chunk][n_blocks]; forced blocks get +1e30 so a top-k over block
// scores that already contain this bias reproduces the reference route.
// ---------------------------------------------------------------------------
export fn mx_route_bias(n_blocks: i64, chunk_len: i64, prefix_blocks: i64,
                        local_blocks: i64, qpos: i64[:], block_size: i64,
                        out: f32[:]) -> i64 {
    if n_blocks <= 0 || chunk_len <= 0 || block_size <= 0 {
        return -1;
    }
    if prefix_blocks < 0 || local_blocks <= 0 {
        return -2;
    }
    if qpos.len < chunk_len {
        return -3;
    }
    if out.len < chunk_len * n_blocks {
        return -4;
    }
    var q: i64 = 0;
    while q < chunk_len {
        var i: i64 = 0;
        while i < n_blocks {
            out[q * n_blocks + i] = 0.0;
            i += 1;
        }
        let cur: i64 = qpos[q] / block_size;
        i = 0;
        while i < prefix_blocks && i <= cur && i < n_blocks {
            out[q * n_blocks + i] = 1.0e30;
            i += 1;
        }
        var l: i64 = 0;
        while l < local_blocks {
            let b: i64 = cur - l;
            if b >= prefix_blocks && b >= 0 && b < n_blocks {
                out[q * n_blocks + b] = 1.0e30;
            }
            l += 1;
        }
        q += 1;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Configuration validation and derived MBSA geometry.
//
// Returns 0 when the configuration is self-consistent; negative codes name the
// first violated rule. The host calls this once at load time instead of
// re-deriving the checks in C++.
//   -1 short buffer
//   -2 head_dim <= 0
//   -3 n_head <= 0 or n_head_kv <= 0 or n_head % n_head_kv != 0
//   -4 indexer heads must equal the KV head count (one per GQA group)
//   -5 indexer head_dim must equal attention head_dim
//   -6 leading dense blocks exceed the layer count
//   -7 block size / prefix / local / remote invalid
//   -8 expert feed-forward sizes invalid
// ---------------------------------------------------------------------------
export fn mx_cfg_check(n_embd: i64, n_head: i64, n_head_kv: i64, head_dim: i64,
                       idx_heads: i64, idx_hd: i64, n_layer: i64, lead: i64,
                       blk: i64, prefix_win: i64, local_win: i64, remote: i64,
                       n_ff_exp: i64, n_ff_shexp: i64, out: i64[:]) -> i64 {
    if out.len < 4 {
        return -1;
    }
    if head_dim <= 0 || n_embd <= 0 {
        return -2;
    }
    if n_head <= 0 || n_head_kv <= 0 || n_head % n_head_kv != 0 {
        return -3;
    }
    if idx_heads != n_head_kv {
        return -4;
    }
    if idx_hd != head_dim {
        return -5;
    }
    if lead < 0 || lead > n_layer {
        return -6;
    }
    if blk <= 0 || prefix_win <= 0 || local_win <= 0 || remote <= 0 {
        return -7;
    }
    if prefix_win % blk != 0 || local_win % blk != 0 {
        return -7;
    }
    if n_ff_exp <= 0 || n_ff_shexp <= 0 {
        return -8;
    }
    out[0] = prefix_win / blk;   // n_prefix_blocks
    out[1] = local_win / blk;    // n_local_blocks
    out[2] = remote;               // requested remote blocks
    out[3] = (2 + 4) * 4;          // mHC mix width for four streams
    return 0;
}

// ---------------------------------------------------------------------------
// MBSA gather plan: token positions and validity from a selected block route.
//
// route holds block ids in reference order (prefix, remote, local), with -1 for
// padding. For every slot the block contributes block_size consecutive token
// positions; a position is valid when its block is non-negative and the
// position is below the current KV length.
// out_pos/out_valid hold route.len*block_size entries.
// ---------------------------------------------------------------------------
export fn mx_route_tokens(route: i64[:], block_size: i64, n_kv: i64,
                          out_pos: i64[:], out_valid: i64[:]) -> i64 {
    if block_size <= 0 || n_kv < 0 {
        return -1;
    }
    let n: i64 = route.len * block_size;
    if out_pos.len < n || out_valid.len < n {
        return -2;
    }
    var b: i64 = 0;
    while b < route.len {
        let blk: i64 = route[b];
        var t: i64 = 0;
        while t < block_size {
            let pos: i64 = blk * block_size + t;
            out_pos[b * block_size + t] = pos;
            if blk >= 0 && pos >= 0 && pos < n_kv {
                out_valid[b * block_size + t] = 1;
            } else {
                out_valid[b * block_size + t] = 0;
            }
            t += 1;
        }
        b += 1;
    }
    return n;
}

// ---------------------------------------------------------------------------
// Reasoning split: separate "<think>...</think>" from the visible answer.
//
// out = [think_off, think_len, answer_off, answer_len].
//   0  tags present and closed
//   1  no tags: the whole text is the answer (answer 0..len), think empty
//  -1  opening tag without a closing one (unterminated reasoning)
//  -2  short buffer
// ---------------------------------------------------------------------------
fn mx_find(hay: const u8[:], needle: const u8[:], from: i64) -> i64 {
    if needle.len == 0 {
        return from;
    }
    var i: i64 = from;
    while i + needle.len <= hay.len {
        var k: i64 = 0;
        var same: i64 = 1;
        while k < needle.len {
            if hay[i + k] != needle[k] {
                same = 0;
            }
            k += 1;
        }
        if same == 1 {
            return i;
        }
        i += 1;
    }
    return -1;
}

export fn mx_split_reasoning(s: const u8[:], out: i64[:]) -> i64 {
    if out.len < 4 {
        return -2;
    }
    let open: const u8[:] = "<think>";
    let close: const u8[:] = "</think>";
    out[0] = 0; out[1] = 0; out[2] = 0; out[3] = s.len;
    let a: i64 = mx_find(s, open, 0);
    if a < 0 {
        return 1;
    }
    let body: i64 = a + open.len;
    let b: i64 = mx_find(s, close, body);
    if b < 0 {
        return -1;
    }
    out[0] = body;
    out[1] = b - body;
    var tail: i64 = b + close.len;
    // skip one leading newline after the closing tag, like the template does
    if tail < s.len && s[tail] == 10 {
        tail += 1;
    }
    out[2] = tail;
    out[3] = s.len - tail;
    return 0;
}
