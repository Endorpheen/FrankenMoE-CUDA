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
    let tok_bytes: i64 = te_array_bytes(data, state, kv, tok_index, n_tok);
    let mrg_bytes: i64 = te_array_bytes(data, state, kv, mrg_index, n_mrg);
    if tok_bytes < 0 || mrg_bytes < 0 {
        return -1;
    }
    need[0] = tok_bytes + mrg_bytes;
    need[1] = n_tok;
    need[2] = n_mrg;
    need[3] = te_slot_count(n_tok);
    need[4] = te_slot_count(n_mrg);
    return 0;
}

// Bytes of one string array, walked once from its first element.
fn te_array_bytes(data: const u8[:], state: i64[:], kv: i64[:], index: i64, n: i64) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 ||
       kv[index * 8 + 2] != 9 || kv[index * 8 + 3] != 8 || kv[index * 8 + 5] != n {
        return -1;
    }
    var at: i64 = kv[index * 8 + 4];
    var total: i64 = 0;
    var i: i64 = 0;
    while i < n {
        if !gr_fits(data, at, 8) {
            return -1;
        }
        let l: u64 = gr_bits(data, at, 8);
        if l > 9223372036854775807 || !gr_fits(data, at + 8, l as! i64) {
            return -1;
        }
        total = total + (l as! i64);
        at = at + 8 + (l as! i64);
        i = i + 1;
    }
    return total;
}
export fn bt_prepare_need(data: const u8[:], state: i64[:], kv: i64[:], tok_index: i64, mrg_index: i64, need: i64[:]) -> i64 {
    return te_need_impl(data, state, kv, tok_index, mrg_index, need);
}

// Copy one string array into `blob` from `base`, walking the entries once and
// recording each offset and length.
fn te_fill(data: const u8[:], state: i64[:], kv: i64[:], index: i64, n: i64,
           blob: u8[:], base: i64, off: i64[:], len: i64[:]) -> i64 {
    if state.len < 8 || state[0] != 919 || index < 0 || index >= state[2] || index >= kv.len / 8 ||
       kv[index * 8 + 2] != 9 || kv[index * 8 + 3] != 8 || kv[index * 8 + 5] != n {
        return -1;
    }
    var at: i64 = kv[index * 8 + 4];
    var pos: i64 = base;
    var i: i64 = 0;
    while i < n {
        if !gr_fits(data, at, 8) {
            return -1;
        }
        let l: u64 = gr_bits(data, at, 8);
        if l > 9223372036854775807 || !gr_fits(data, at + 8, l as! i64) {
            return -1;
        }
        let ln: i64 = l as! i64;
        if pos + ln > blob.len {
            return -2;
        }
        off[i] = pos;
        len[i] = ln;
        var k: i64 = 0;
        while k < ln {
            blob[pos + k] = data[at + 8 + k];
            k = k + 1;
        }
        pos = pos + ln;
        at = at + 8 + ln;
        i = i + 1;
    }
    return pos;
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
    let after_tok: i64 = te_fill(data, state, kv, tok_index, n_tok, blob, 0, tok_off, tok_len);
    if after_tok < 0 {
        return after_tok;
    }
    let end: i64 = te_fill(data, state, kv, mrg_index, n_mrg, blob, after_tok, mrg_off, mrg_len);
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

// Longest special-token match at `at`, or -1. `order` is the special-token id
// list sorted by descending text length (bt_special_order), so the first hit is
// the longest one.
fn te_match_special(text: const u8[:], at: i64, blob: const u8[:],
                    tok_off: const i64[:], tok_len: const i64[:], order: const i64[:], n_special: i64) -> i64 {
    var i: i64 = 0;
    while i < n_special {
        let id: i64 = order[i];
        let ln: i64 = tok_len[id];
        if ln > 0 && at + ln <= text.len {
            var k: i64 = 0;
            var same: bool = true;
            while k < ln {
                if text[at + k] != blob[tok_off[id] + k] {
                    same = false;
                    k = ln;
                } else {
                    k = k + 1;
                }
            }
            if same {
                return id;
            }
        }
        i = i + 1;
    }
    return -1;
}

// Tokenize `text` the way llama_tokenize does: when `special` is set, special
// tokens are matched first and the gaps between them go through bt_bpe;
// otherwise the whole text goes through bt_bpe.
export fn bt_tokenize(blob: const u8[:], tok_off: const i64[:], tok_len: const i64[:], tok_slots: const i64[:],
                      mrg_off: const i64[:], mrg_len: const i64[:], mrg_slots: const i64[:],
                      order: const i64[:], n_special: i64, text: const u8[:], ids: i64[:], special: i64,
                      cpts: i64[:], seg: i64[:], enc: u8[:], sym: i64[:], heap: i64[:], hc: i64[:], info: i64[:]) -> i64 {
    if special == 0 || n_special == 0 {
        return bt_bpe_body(text, blob, tok_off, tok_len, tok_slots, mrg_off, mrg_len, mrg_slots,
                      ids, 0, cpts, seg, enc, sym, heap, hc, info);
    }
    var total: i64 = 0;
    var plain: i64 = 0;
    var at: i64 = 0;
    while at <= text.len {
        var hit: i64 = -1;
        if at < text.len {
            hit = te_match_special(text, at, blob, tok_off, tok_len, order, n_special);
        }
        if hit < 0 && at < text.len {
            at = at + 1;
        } else {
            if at > plain {
                if total >= ids.len {
                    return -1;
                }
                let got: i64 = bt_bpe_body(text[plain .. at], blob, tok_off, tok_len, tok_slots,
                                      mrg_off, mrg_len, mrg_slots, ids[total .. ids.len], 0,
                                      cpts, seg, enc, sym, heap, hc, info);
                if got < 0 {
                    return -1;
                }
                total = total + got;
            }
            if hit < 0 {
                return total;
            }
            if total >= ids.len {
                return -1;
            }
            ids[total] = hit;
            total = total + 1;
            at = at + tok_len[hit];
            plain = at;
        }
    }
    return total;
}
