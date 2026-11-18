// Tokenizer load path: from a parsed GGUF header to ready BPE tables, in Goose.
//
// The engine in bpe-tokenize.goose answers a raw fragment and the reader in
// gguf-reader.goose answers element spans, but nothing joined them: every host
// that wanted a tokenizer had to find three keys, index two string arrays, split
// the pairs, call bt_build, read the token types and run the attribute passes in
// that order. That join is a decision about the format, so it belongs here
// rather than in a C++ carrier.
//
// Buffers are caller-owned and borrowed; nothing is allocated. The caller learns
// the required sizes from the reader (gr_arr_n) and from `stats`.

const BTL_TOKENS: const u8[:] = "tokenizer.ggml.tokens";
const BTL_MERGES: const u8[:] = "tokenizer.ggml.merges";
const BTL_TYPES:  const u8[:] = "tokenizer.ggml.token_type";

// Splits `pairs` (off, len repeated) into two parallel arrays.
fn btl_split(pairs: const i64[:], off: i64[:], len: i64[:], n: i64) -> i64 {
    if pairs.len < 2 * n {
        return -1;
    }
    var i: i64 = 0;
    while i < n {
        off[i] = pairs[i * 2];
        len[i] = pairs[i * 2 + 1];
        i = i + 1;
    }
    return 0;
}

// stats: 0 n_tokens, 1 n_merges, 2 duplicate tokens replaced, 3 duplicate merges
//        ignored, 4 n_special, 5 n_eog.
// Returns 0, or a negative stage code:
//   -1 bad argument or a key missing from the file
//   -2 a caller buffer is too small (stats[6] names the stage that ran short)
//   -3 bt_build refused the tables
export fn btl_load(
    gguf: const u8[:], state: i64[:], kv: i64[:], kvslots: i64[:],
    tok_off: i64[:], tok_len: i64[:], tok_slots: i64[:],
    mrg_off: i64[:], mrg_len: i64[:], mrg_slots: i64[:],
    pairs: i64[:], tok_type: i64[:], attr: i64[:], order: i64[:], eog: i64[:],
    eos_id: i64, eot_id: i64, eom_id: i64,
    stats: i64[:]
) -> i64 {
    if stats.len < 7 {
        return -1;
    }
    var s: i64 = 0;
    while s < 7 {
        stats[s] = 0;
        s = s + 1;
    }

    let ti: i64 = gr_find_kv_impl(gguf, state, kv, kvslots, BTL_TOKENS);
    let mi: i64 = gr_find_kv_impl(gguf, state, kv, kvslots, BTL_MERGES);
    let tyi: i64 = gr_find_kv_impl(gguf, state, kv, kvslots, BTL_TYPES);
    if ti < 0 || mi < 0 || tyi < 0 {
        return -1;
    }
    let n_tok: i64 = gr_arr_n_impl(state, kv, ti);
    let n_mrg: i64 = gr_arr_n_impl(state, kv, mi);
    if n_tok <= 0 || n_mrg < 0 || n_tok >= 9223372036854775807 / 2 {
        return -1;
    }
    stats[0] = n_tok;
    stats[1] = n_mrg;

    // Every consumer below indexes by id or by element, so the buffers must
    // cover the whole array before anything is written.
    if tok_off.len < n_tok || tok_len.len < n_tok || tok_type.len < n_tok ||
       attr.len < n_tok || order.len < n_tok {
        stats[6] = 1;
        return -2;
    }
    if n_mrg > 0 && (mrg_off.len < n_mrg || mrg_len.len < n_mrg) {
        stats[6] = 2;
        return -2;
    }
    if pairs.len < 2 * n_tok {
        stats[6] = 3;
        return -2;
    }
    if mrg_off.len < n_mrg {
        stats[6] = 4;
        return -2;
    }

    // Tokens, then merges: one pairs buffer serves both because the split is
    // finished before the next index pass starts.
    var at: i64 = 0;
    while at < 2 * n_tok {
        pairs[at] = 0;
        at = at + 1;
    }
    if gr_string_array_index_impl(gguf, state, kv, ti, pairs) != n_tok {
        return -1;
    }
    if btl_split(pairs, tok_off, tok_len, n_tok) != 0 {
        stats[6] = 3;
        return -2;
    }

    if n_mrg > 0 {
        if pairs.len < 2 * n_mrg {
            stats[6] = 4;
            return -2;
        }
        at = 0;
        while at < 2 * n_mrg {
            pairs[at] = 0;
            at = at + 1;
        }
        if gr_string_array_index_impl(gguf, state, kv, mi, pairs) != n_mrg {
            return -1;
        }
        if btl_split(pairs, mrg_off, mrg_len, n_mrg) != 0 {
            stats[6] = 4;
            return -2;
        }
    }

    let rc: i64 = bt_build_impl(gguf, tok_off, tok_len, tok_slots,
                           mrg_off, mrg_len, mrg_slots, stats);
    if rc != 0 {
        return -3;
    }

    var i: i64 = 0;
    while i < n_tok {
        var v: i64[1] = [0];
        if gr_array_int_impl(gguf, state, kv, tyi, i, v) != 0 {
            return -1;
        }
        tok_type[i] = v[0];
        i = i + 1;
    }
    if bt_attrs_impl(gguf, tok_off, tok_len, tok_type, attr) != 0 {
        return -1;
    }
    let n_spl: i64 = bt_special_order_impl(gguf, tok_off, tok_len, attr, order);
    if n_spl < 0 {
        return -1;
    }
    stats[4] = n_spl;

    let n_eog: i64 = bt_eog_ids_impl(gguf, tok_off, tok_len, eos_id, eot_id, eom_id, eog);
    if n_eog < 0 {
        // -(needed + 1): the list does not fit, which is not a load failure -
        // the caller can retry with the reported size. Report it as such.
        stats[6] = 5;
        return -2;
    }
    stats[5] = n_eog;
    return 0;
}
