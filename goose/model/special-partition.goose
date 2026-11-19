// Special-token partitioning, ported from llama_vocab::impl::tokenizer_st_partition
// in src/llama-vocab.cpp.
//
// Owned decisions:
//   - the special tokens are tried in the order the loader hands over (by text
//     length, longest first), and every raw text fragment is scanned for each
//     of them in turn;
//   - a match splits the fragment into the text before it, the token itself and
//     the text after it, and the remainder is scanned again for the same token,
//     so two adjacent identical special tokens both become tokens;
//   - when parse_special is false, control and unknown tokens are skipped, but
//     user-defined tokens are still split out. A token that is both control and
//     user-defined is skipped by that test;
//   - LLAMA_TOKEN_ATTR_LSTRIP removes the whitespace that ends the text before
//     the token, LLAMA_TOKEN_ATTR_RSTRIP removes the whitespace that starts the
//     text after it, and a part that becomes empty is not emitted;
//   - the fragment order is the text order: whatever survives on the left of a
//     match stays where it was, the token follows it, the right remainder
//     follows the token.
//
// Not owned: the sort itself (the loader builds the order), the token-to-text
// table, the pretokenizers that run after this step, and the case where a
// token's text is empty: the native loop cannot advance past a zero-length
// match, so such a vocab is refused upstream and is out of the corpus.
//
// Whitespace is the C-locale isspace set (space, \t, \n, \v, \f, \r). Native
// calls isspace on a possibly negative char, which is undefined for bytes above
// 127; strip cases therefore stay on ASCII and bytes above 127 are only used
// where no strip attribute is set.
//
// Output: one fragment per entry in out_kind (0 token, 1 raw text), with the
// token id in out_tok and the offset/length into the input text otherwise.
// Returns the fragment count.

fn stp_is_space(c: u8) -> bool {
    return c == 32 || c == 9 || c == 10 || c == 11 || c == 12 || c == 13;
}

// First occurrence of data in text[from, to), -1 when there is none.
fn stp_find(text: const u8[:], pos_from: i64, to: i64, data: const u8[:]) -> i64 {
    if data.len == 0 || data.len > to - pos_from {
        return 0 - 1;
    }
    var i: i64 = pos_from;
    while i + data.len <= to {
        var k: i64 = 0;
        var hit: i64 = 1;
        while k < data.len {
            if text[i + k] != data[k] {
                hit = 0;
                break;
            }
            k = k + 1;
        }
        if hit == 1 {
            return i;
        }
        i = i + 1;
    }
    return 0 - 1;
}

fn stp_insert(kind: i64[:], tok: i64[:], off: i64[:], ln: i64[:], cnt: i64,
             at: i64, k: i64, t: i64, o: i64, l: i64) -> i64 {
    var i: i64 = cnt;
    while i > at {
        kind[i] = kind[i - 1];
        tok[i] = tok[i - 1];
        off[i] = off[i - 1];
        ln[i] = ln[i - 1];
        i = i - 1;
    }
    kind[at] = k;
    tok[at] = t;
    off[at] = o;
    ln[at] = l;
    return cnt + 1;
}

fn stp_remove(kind: i64[:], tok: i64[:], off: i64[:], ln: i64[:], cnt: i64,
             at: i64) -> i64 {
    var i: i64 = at;
    while i + 1 < cnt {
        kind[i] = kind[i + 1];
        tok[i] = tok[i + 1];
        off[i] = off[i + 1];
        ln[i] = ln[i + 1];
        i = i + 1;
    }
    return cnt - 1;
}

export fn stp_partition(
    text: const u8[:],
    n_tok: i64,
    tok_off: const i64[:],
    tok_len: const i64[:],
    tok_text: const u8[:],
    tok_attr: const i64[:],
    order: const i64[:],
    parse_special: i64,
    out_kind: i64[:],
    out_tok: i64[:],
    out_off: i64[:],
    out_len: i64[:]
) -> i64 {
    var cnt: i64 = 0;
    if text.len > 0 {
        cnt = stp_insert(out_kind, out_tok, out_off, out_len, cnt, 0,
                        1, 0 - 1, 0, text.len);
    }

    var s: i64 = 0;
    while s < order.len {
        var id: i64 = order[s];
        s = s + 1;

        if id < 0 || id >= n_tok {
            continue;
        }

        var attr: i64 = tok_attr[id];
        var to0: i64 = tok_off[id];
        var tl: i64 = tok_len[id];
        var data: const u8[:] = tok_text[to0..to0 + tl];

        if parse_special == 0 && (attr & 9) != 0 {
            // LLAMA_TOKEN_ATTR_UNKNOWN (1) and LLAMA_TOKEN_ATTR_CONTROL (8).
            continue;
        }

        var i: i64 = 0;
        while i < cnt {
            if out_kind[i] != 1 {
                i = i + 1;
                continue;
            }

            var bo: i64 = out_off[i];
            var bl: i64 = out_len[i];

            while true {
                var m: i64 = stp_find(text, bo, bo + bl, data);
                if m < 0 {
                    break;
                }

                var src: i64 = i;
                var ins: i64 = i + 1;
                var it: i64 = i;

                if m > bo {
                    var lo: i64 = bo;
                    var ll: i64 = m - bo;
                    if (attr & 128) != 0 {
                        // LLAMA_TOKEN_ATTR_LSTRIP
                        while ll > 0 && stp_is_space(text[lo + ll - 1]) {
                            ll = ll - 1;
                        }
                    }
                    if ll > 0 {
                        cnt = stp_insert(out_kind, out_tok, out_off, out_len, cnt,
                                        ins, 1, 0 - 1, lo, ll);
                        ins = ins + 1;
                    }
                }

                cnt = stp_insert(out_kind, out_tok, out_off, out_len, cnt,
                                ins, 0, id, 0, 0);
                it = ins;
                ins = ins + 1;

                var ro: i64 = bo;
                var rl: i64 = bl;
                var has_right: bool = false;

                if m + tl < bo + bl {
                    ro = m + tl;
                    rl = bl - ((m - bo) + tl);
                    if (attr & 256) != 0 {
                        // LLAMA_TOKEN_ATTR_RSTRIP
                        while rl > 0 && stp_is_space(text[ro]) {
                            ro = ro + 1;
                            rl = rl - 1;
                        }
                    }
                    if rl > 0 {
                        cnt = stp_insert(out_kind, out_tok, out_off, out_len, cnt,
                                        ins, 1, 0 - 1, ro, rl);
                        it = ins;
                        ins = ins + 1;
                    }
                    has_right = true;
                }

                cnt = stp_remove(out_kind, out_tok, out_off, out_len, cnt, src);
                if src <= it {
                    it = it - 1;
                }

                if !has_right {
                    // The outer increment moves past the token.
                    i = it;
                    break;
                }

                i = it;
                bo = ro;
                bl = rl;
            }

            i = i + 1;
        }
    }

    return cnt;
}
