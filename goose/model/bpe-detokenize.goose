// Detokenisation in Goose: token_to_piece, detokenize and the end-of-generation
// set, ported from src/llama-vocab.cpp.
//
// The vocabulary stores byte-encoded text, so a NORMAL piece is decoded through
// unicode_cpt_to_byte, and a codepoint that is not a byte code point produces
// the native marker "[UNK_BYTE_0x<utf8 hex>" followed by the WHOLE token text
// and "]" — that is what the native catch block appends, and a tidier port
// would emit different bytes.
//
// The attribute test comes first: without `special`, an UNKNOWN or CONTROL token
// contributes nothing at all. A piece is then copied with `lstrip` leading
// spaces skipped. Every function reports an exact byte count; a short output
// buffer returns the size it needed as a negative number, which is how the
// caller sizes its retry (and how detokenize keeps counting after overflow).
//
// add_space_prefix, add_bos and add_eos are inputs: the Apex vocabulary has all
// three clear, and the caller passes its own configured values.

// Length of the byte-encoded -> raw decoding of the token text.
fn dt_decode_len(gguf: const u8[:], toff: i64, tlen: i64) -> i64 {
    var adv: i64[1] = [1];
    var src: i64 = 0;
    var n: i64 = 0;
    while src < tlen {
        let cpt: i64 = uc_cpt_from(gguf, toff + src, adv);
        if cpt < 0 {
            n = n + 3;  // U+FFFD
            src = src + 1;
        } else {
            let byte: i64 = uc_cpt_to_byte(cpt);
            if byte >= 0 {
                n = n + 1;
            } else {
                // "[UNK_BYTE_0x" + 2 hex per utf8 byte + token text + "]"
                n = n + 12 + 2 * uc_utf8_len(cpt) + tlen + 1;
            }
            src = src + adv[0];
        }
    }
    return n;
}

fn dt_decode_text(gguf: const u8[:], toff: i64, tlen: i64, out: u8[:], at_in: i64) -> i64 {
    var adv: i64[1] = [1];
    var src: i64 = 0;
    var at: i64 = at_in;
    while src < tlen {
        let cpt: i64 = uc_cpt_from(gguf, toff + src, adv);
        if cpt < 0 {
            at = uc_encode_body(UC_FFFD, out, at);
            src = src + 1;
        } else {
            let byte: i64 = uc_cpt_to_byte(cpt);
            if byte >= 0 {
                out[at] = byte as u8;
                at = at + 1;
            } else {
                let hex: const u8[:] = "0123456789abcdef";
                let lit: const u8[:] = "[UNK_BYTE_0x";
                var k: i64 = 0;
                while k < lit.len {
                    out[at] = lit[k];
                    at = at + 1;
                    k = k + 1;
                }
                var buf: u8[4] = [0, 0, 0, 0];
                let n: i64 = uc_encode_body(cpt, buf, 0);
                k = 0;
                while k < n {
                    let b: i64 = buf[k];
                    out[at] = hex[b >> 4];
                    at = at + 1;
                    out[at] = hex[b & 0x0F];
                    at = at + 1;
                    k = k + 1;
                }
                k = 0;
                while k < tlen {
                    out[at] = gguf[toff + k];
                    at = at + 1;
                    k = k + 1;
                }
                out[at] = 93;
                at = at + 1;
            }
            src = src + adv[0];
        }
    }
    return at;
}

// llama_unescape_whitespace: U+2581 (E2 96 81) becomes a space.
fn dt_unescape_len(gguf: const u8[:], toff: i64, tlen: i64) -> i64 {
    var i: i64 = 0;
    var n: i64 = 0;
    while i < tlen {
        if i + 2 < tlen && gguf[toff + i] == 0xE2 && gguf[toff + i + 1] == 0x96 && gguf[toff + i + 2] == 0x81 {
            n = n + 1;
            i = i + 3;
        } else {
            n = n + 1;
            i = i + 1;
        }
    }
    return n;
}

fn dt_unescape_text(gguf: const u8[:], toff: i64, tlen: i64, out: u8[:], at_in: i64) -> i64 {
    var i: i64 = 0;
    var at: i64 = at_in;
    while i < tlen {
        if i + 2 < tlen && gguf[toff + i] == 0xE2 && gguf[toff + i + 1] == 0x96 && gguf[toff + i + 2] == 0x81 {
            out[at] = 32;
            at = at + 1;
            i = i + 3;
        } else {
            out[at] = gguf[toff + i];
            at = at + 1;
            i = i + 1;
        }
    }
    return at;
}

// One BYTE-attribute token: the native BPE path parses "<0xXX>" from the text.
fn dt_byte_of(gguf: const u8[:], toff: i64, tlen: i64) -> i64 {
    if tlen < 6 {
        return -1;
    }
    if gguf[toff] != 60 || gguf[toff + 1] != 48 || gguf[toff + 2] != 120 || gguf[toff + 5] != 62 {
        return -1;
    }
    var v: i64 = 0;
    var k: i64 = 3;
    while k < 5 {
        let c: i64 = gguf[toff + k];
        var d: i64 = -1;
        if c >= 48 && c <= 57 {
            d = c - 48;
        } else if c >= 97 && c <= 102 {
            d = c - 87;
        } else if c >= 65 && c <= 70 {
            d = c - 55;
        }
        if d < 0 {
            return -1;
        }
        v = v * 16 + d;
        k = k + 1;
    }
    return v;
}

// Size of one piece in bytes, and the piece itself when `cap` allows it.
// Returns the size, or -(size) when it does not fit; `out` is written only in
// the fitting case. This split keeps an overflowing caller from touching bytes
// past its buffer while still learning the exact size.
fn dt_piece(out: u8[:], off: i64, cap: i64,
            gguf: const u8[:], toff: i64, tlen: i64, a: i64,
            lstrip: i64, special: i64, escape_ws: i64) -> i64 {
    if special == 0 && (a & 9) != 0 {
        return 0;
    }
    if tlen <= 0 {
        return 0;
    }
    var verbatim: bool = false;
    var decode: bool = false;
    var escape: bool = false;
    var is_byte: bool = false;
    if (a & (BT_UNKNOWN | BT_CONTROL | BT_USER_DEFINED)) != 0 {
        verbatim = true;
    } else if (a & BT_NORMAL) != 0 {
        if escape_ws != 0 {
            escape = true;
        } else {
            decode = true;
        }
    } else if (a & BT_BYTE) != 0 {
        is_byte = true;
    } else {
        return 0;
    }

    var skip: i64 = lstrip;
    var voff: i64 = toff;
    var vlen: i64 = tlen;
    if verbatim || escape {
        while skip > 0 && vlen > 0 && gguf[voff] == 32 {
            voff = voff + 1;
            vlen = vlen - 1;
            skip = skip - 1;
        }
    }

    var size: i64 = 0;
    if verbatim {
        size = vlen;
    } else if (escape) {
        size = dt_unescape_len(gguf, voff, vlen);
    } else if (decode) {
        size = dt_decode_len(gguf, voff, vlen);
    } else {
        size = 1;
    }
    if size > cap {
        return 0 - size;
    }
    if verbatim {
        var i: i64 = 0;
        while i < vlen {
            out[off + i] = gguf[voff + i];
            i = i + 1;
        }
        return size;
    }
    if escape {
        dt_unescape_text(gguf, voff, vlen, out, off);
        return size;
    }
    if decode {
        // lstrip applies to the decoded bytes, not to the encoded text
        var at: i64 = dt_decode_text(gguf, voff, vlen, out, off);
        var s: i64 = off;
        var sk: i64 = lstrip;
        while sk > 0 && s < at && out[s] == 32 {
            s = s + 1;
            sk = sk - 1;
        }
        if s > off {
            var i: i64 = s;
            while i < at {
                out[off + (i - s)] = out[i];
                i = i + 1;
            }
            at = at - (s - off);
        }
        return at - off;
    }
    let byte: i64 = dt_byte_of(gguf, voff, vlen);
    if byte < 0 {
        return 0;
    }
    out[off] = byte as u8;
    return 1;
}

fn dt_piece_body(gguf: const u8[:], tok_off: const i64[:], tok_len: const i64[:], attr: const i64[:],
                 token: i64, out: u8[:], off: i64, cap: i64, lstrip: i64, special: i64, escape_ws: i64) -> i64 {
    if token < 0 || token >= tok_off.len || attr.len < tok_off.len || off < 0 || cap < 0 {
        return 0;
    }
    // `off` may be past the end of the buffer once the caller has started accounting for a shortfall: the
    // native loop keeps asking each remaining piece for its size so the total it reports is the whole
    // needed length. Refusing that asked-for size here made the door report where it stopped rather than
    // what was needed (-18 against -1015, measured in EXP-2026-11-19-1356).
    var room: i64 = 0;
    if off < out.len {
        room = out.len - off;
    }
    var use: i64 = cap;
    if use > room {
        use = room;
    }
    return dt_piece(out, off, use, gguf, tok_off[token], tok_len[token], attr[token], lstrip, special, escape_ws);
}

export fn bt_token_to_piece(
    gguf: const u8[:], tok_off: const i64[:], tok_len: const i64[:], attr: const i64[:],
    token: i64, out: u8[:], at: i64, lstrip: i64, special: i64, escape_ws: i64
) -> i64 {
    // the caller's buffer starts at `at`; native's `length` is what remains
    if at < 0 || at > out.len {
        return 0;
    }
    return dt_piece_body(gguf, tok_off, tok_len, attr, token, out, at, out.len - at, lstrip, special, escape_ws);
}

// Native detokenize: leading-space removal for the first piece, optional
// BOS/EOS removal, and a running total that survives an overflow.
fn dt_detokenize_body(
    gguf: const u8[:], tok_off: const i64[:], tok_len: const i64[:], attr: const i64[:],
    ids: const i64[:], n: i64, out: u8[:],
    remove_special: i64, unparse_special: i64,
    add_space_prefix: i64, add_bos: i64, bos_id: i64, add_eos: i64, eos_id: i64,
    escape_ws: i64
) -> i64 {
    if n < 0 || n > ids.len || attr.len < tok_off.len {
        return -1;
    }
    var first: i64 = 0;
    var last: i64 = n;
    var remove_space: i64 = add_space_prefix;
    if remove_special != 0 && add_bos != 0 && n > 0 && ids[0] == bos_id {
        remove_space = 0;
        first = 1;
    }
    if remove_special != 0 && add_eos != 0 && last > first && ids[last - 1] == eos_id {
        last = last - 1;
    }
    var total: i64 = 0;
    var room: i64 = out.len;
    var i: i64 = first;
    while i < last {
        let r: i64 = dt_piece_body(gguf, tok_off, tok_len, attr, ids[i], out, total, room,
                                   remove_space, unparse_special, escape_ws);
        remove_space = 0;
        if r < 0 {
            room = 0;
            total = total - r;
        } else {
            total = total + r;
            room = room - r;
        }
        i = i + 1;
    }
    if total > out.len {
        return 0 - total;
    }
    return total;
}

export fn bt_detokenize(
    gguf: const u8[:], tok_off: const i64[:], tok_len: const i64[:], attr: const i64[:],
    ids: const i64[:], n: i64, out: u8[:],
    remove_special: i64, unparse_special: i64,
    add_space_prefix: i64, add_bos: i64, bos_id: i64, add_eos: i64, eos_id: i64,
    escape_ws: i64
) -> i64 {
    return dt_detokenize_body(gguf, tok_off, tok_len, attr, ids, n, out, remove_special,
                              unparse_special, add_space_prefix, add_bos, bos_id, add_eos, eos_id, escape_ws);
}

// End of generation membership over the id list built by bt_eog_ids.
export fn bt_is_eog(eog: const i64[:], n: i64, token: i64) -> i64 {
    return bt_is_eog_impl(eog, n, token);
}

fn bt_is_eog_impl(eog: const i64[:], n: i64, token: i64) -> i64 {
    var i: i64 = 0;
    while i < n && i < eog.len {
        if eog[i] == token {
            return 1;
        }
        i = i + 1;
    }
    return 0;
}
