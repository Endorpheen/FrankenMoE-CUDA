// Tokenizer engine: the vocabulary is read out of the parsed GGUF header and
// laid out exactly as bt_build expects, so the host never arranges tokens.
//
// gguf-header state comes from the Goose reader (gr_*): `state` is the parser
// state, `kv` the key table. Entry `tok_index` holds the token texts, entry
// `mrg_index` the merges as "left right" strings, both string arrays.
//
// Outputs, all caller-owned:
//   blob     token texts followed by the merge texts, back to back;
//   tok_off  offset of each token text inside blob, tok_len its length;
//   mrg_off  offset of each merge text inside blob, mrg_len its length;
//   tok_slots / mrg_slots  hash tables, sized to the next power of two above
//                          twice the entry count and zeroed here.
//
// `need` receives, in order: blob bytes, token count, merge count, token slots,
// merge slots. Returns 0 when everything was written, -2 when a buffer is too
// small (call `need` first), -1 for a bad array.

fn te_next_pow2(v: i64) -> i64 {
    var n: i64 = 1;
    while n < v {
        n = n * 2;
    }
    return n;
}

fn te_slot_count(n: i64) -> i64 {
    return te_next_pow2(n * 2);
}

fn te_span_len(data: const u8[:], state: i64[:], kv: i64[:], index: i64, element: i64, out: i64[:]) -> i64 {
    return gr_span_impl(data, state, kv, index, element, out);
}

// Total bytes the two arrays occupy once each entry carries a terminator slot.
fn te_need_impl(data: const u8[:], state: i64[:], kv: i64[:], tok_index: i64, mrg_index: i64, need: i64[:]) -> i64 {
    if need.len < 5 {
        return -1;
    }
    let n_tok: i64 = gr_arr_n(state, kv, tok_index);
    let n_mrg: i64 = gr_arr_n(state, kv, mrg_index);
    if n_tok < 0 || n_mrg < 0 {
        return -1;
    }
    var bytes: i64 = 0;
    var i: i64 = 0;
    var span: i64[:] = need[0 .. 2];
    while i < n_tok {
        if te_span_len(data, state, kv, tok_index, i, span) != 0 {
            return -1;
        }
        bytes = bytes + span[1];
        i = i + 1;
    }
    i = 0;
    while i < n_mrg {
        if te_span_len(data, state, kv, mrg_index, i, span) != 0 {
            return -1;
        }
        bytes = bytes + span[1];
        i = i + 1;
    }
    need[0] = bytes;
    need[1] = n_tok;
    need[2] = n_mrg;
    need[3] = te_slot_count(n_tok);
    need[4] = te_slot_count(n_mrg);
    return 0;
}

export fn bt_prepare_need(data: const u8[:], state: i64[:], kv: i64[:], tok_index: i64, mrg_index: i64, need: i64[:]) -> i64 {
    return te_need_impl(data, state, kv, tok_index, mrg_index, need);
}

// Copy one string array into `blob` from `base`, recording offsets and lengths.
fn te_fill(data: const u8[:], state: i64[:], kv: i64[:], index: i64, n: i64,
           blob: u8[:], base: i64, off: i64[:], len: i64[:], span: i64[:]) -> i64 {
    var at: i64 = base;
    var i: i64 = 0;
    while i < n {
        if te_span_len(data, state, kv, index, i, span) != 0 {
            return -1;
        }
        let ln: i64 = span[1];
        if at + ln > blob.len {
            return -2;
        }
        off[i] = at;
        len[i] = ln;
        var k: i64 = 0;
        while k < ln {
            blob[at + k] = data[span[0] + k];
            k = k + 1;
        }
        at = at + ln;
        i = i + 1;
    }
    return at;
}

export fn bt_prepare_from_gguf(data: const u8[:], state: i64[:], kv: i64[:], tok_index: i64, mrg_index: i64,
                               blob: u8[:], tok_off: i64[:], tok_len: i64[:], tok_slots: i64[:],
                               mrg_off: i64[:], mrg_len: i64[:], mrg_slots: i64[:], need: i64[:]) -> i64 {
    if te_need_impl(data, state, kv, tok_index, mrg_index, need) != 0 {
        return -1;
    }
    let bytes: i64 = need[0];
    let n_tok: i64 = need[1];
    let n_mrg: i64 = need[2];
    let n_tok_slots: i64 = need[3];
    let n_mrg_slots: i64 = need[4];
    if blob.len < bytes || tok_off.len < n_tok || tok_len.len < n_tok || tok_slots.len < n_tok_slots ||
       mrg_off.len < n_mrg || mrg_len.len < n_mrg || mrg_slots.len < n_mrg_slots {
        return -2;
    }
    let after_tok: i64 = te_fill(data, state, kv, tok_index, n_tok, blob, 0, tok_off, tok_len, need[0 .. 2]);
    if after_tok < 0 {
        return after_tok;
    }
    let end: i64 = te_fill(data, state, kv, mrg_index, n_mrg, blob, after_tok, mrg_off, mrg_len, need[0 .. 2]);
    if end < 0 {
        return end;
    }
    var i: i64 = 0;
    while i < n_tok_slots {
        tok_slots[i] = 0;
        i = i + 1;
    }
    i = 0;
    while i < n_mrg_slots {
        mrg_slots[i] = 0;
        i = i + 1;
    }
    return 0;
}
