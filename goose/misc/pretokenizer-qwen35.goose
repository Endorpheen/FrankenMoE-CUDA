// QWEN35 pre-tokenizer, ported from unicode_regex_split_custom_qwen35 in
// src/unicode.cpp, followed by the byte-encoding step
// (unicode_byte_encoding_process).
//
// The regex in the native constructor is
//   (?:'[sS]|'[tT]|'[rR][eE]|'[vV][eE]|'[mM]|'[lL][lL]|'[dD])
//   |[^\r\n\p{L}\p{N}]?[\p{L}\p{M}]+
//   |\p{N}
//   | ?[^\s\p{L}\p{M}\p{N}]+[\r\n]*
//   |\s*[\r\n]+
//   |\s+(?!\S)
//   |\s+
// and the native matcher tests those alternatives in this order. The order is
// the behaviour: a space before a letter belongs to the letter word, a digit is
// always one codepoint, and a whitespace run keeps its last codepoint for the
// lookahead branch.
//
// One branch keeps a quirk: the punctuation alternative tests the ORIGINAL
// flags of the current codepoint for "is a category at all", while the run is
// scanned with the flags of pos+1 when the codepoint is a space. A port that
// tidied that up would split " ,," differently.
//
// The split works on codepoints of the raw text, exactly as native: byte
// encoding is applied afterwards, per word, not before the split. Every byte of
// the word's UTF-8 becomes the GPT-2 byte codepoint, so the BPE vocabulary
// (which stores Ġ for a space) can be addressed with raw input bytes.
//
// Output: seg[c*4..c*4+3] = codepoint offset, codepoint length, encoded byte
// offset, encoded byte length. The host passes cpts (text.len entries), seg
// (4*text.len entries) and enc (8*text.len bytes).

const PT_OUT: i64 = 0xFFFFFFFF;
const PT_NUM: i64 = 0x0002;
const PT_LETTER: i64 = 0x0004;
const PT_MARK: i64 = 0x0010;
const PT_SPACE: i64 = 0x0100;
const PT_PUNCT: i64 = PT_SPACE | PT_LETTER | PT_MARK | PT_NUM;

fn pt_cpt(cpts: i64[:], n: i64, pos: i64) -> i64 {
    if pos < 0 || pos >= n {
        return PT_OUT;
    }
    return cpts[pos];
}

fn pt_flags(cpts: i64[], n: i64, pos: i64) -> i64 {
    if pos < 0 || pos >= n {
        return 0;
    }
    return uf_flags(cpts[pos]);
}

fn pt_add(seg: i64[:], cap: i64, count: i64, prev_end: i64, end: i64) -> i64 {
    if end > prev_end && count < cap {
        seg[count * 4] = prev_end;
        seg[count * 4 + 1] = end - prev_end;
        return count + 1;
    }
    return count;
}

fn pt_scan(cpts: i64[:], n: i64, seg: i64[:], cap: i64) -> i64 {
    var count: i64 = 0;
    var prev_end: i64 = 0;
    var pos: i64 = 0;

    while pos < n {
        let cpt: i64 = cpts[pos];
        let f: i64 = uf_flags(cpt);
        var handled: bool = false;

        // 1. (?:'[sS]|'[tT]|'[rR][eE]|'[vV][eE]|'[mM]|'[lL][lL]|'[dD])
        if cpt == 39 && pos + 1 < n {
            let a: i64 = ul_lower(cpts[pos + 1]);
            if a == 115 || a == 116 || a == 109 || a == 100 {
                count = pt_add(seg, cap, count, prev_end, pos + 2);
                prev_end = pos + 2;
                pos = pos + 2;
                handled = true;
            } else if pos + 2 < n {
                let b: i64 = ul_lower(cpts[pos + 2]);
                if (a == 114 && b == 101) || (a == 118 && b == 101) || (a == 108 && b == 108) {
                    count = pt_add(seg, cap, count, prev_end, pos + 3);
                    prev_end = pos + 3;
                    pos = pos + 3;
                    handled = true;
                }
            }
        }

        // 2. [^\r\n\p{L}\p{N}]?[\p{L}\p{M}]+
        if !handled && cpt != 13 && cpt != 10 && (f & PT_NUM) == 0 {
            let nf: i64 = pt_flags(cpts, n, pos + 1);
            if (f & (PT_LETTER | PT_MARK)) != 0 || (nf & (PT_LETTER | PT_MARK)) != 0 {
                var p: i64 = pos + 1;
                var g: i64 = pt_flags(cpts, n, p);
                while (g & (PT_LETTER | PT_MARK)) != 0 {
                    p = p + 1;
                    g = pt_flags(cpts, n, p);
                }
                count = pt_add(seg, cap, count, prev_end, p);
                prev_end = p;
                pos = p;
                handled = true;
            }
        }

        // 3. \p{N}
        if !handled && (f & PT_NUM) != 0 {
            pos = pos + 1;
            count = pt_add(seg, cap, count, prev_end, pos);
            prev_end = pos;
            handled = true;
        }

        // 4. <space>?[^\s\p{L}\p{M}\p{N}]+[\r\n]*
        if !handled {
            var f2: i64 = f;
            var skip: i64 = 0;
            if cpt == 32 {
                f2 = pt_flags(cpts, n, pos + 1);
                skip = 1;
            }
            if (f2 & PT_PUNCT) == 0 && f != 0 {
                var p: i64 = pos + skip;
                var g: i64 = pt_flags(cpts, n, p);
                while (g & PT_PUNCT) == 0 && g != 0 {
                    p = p + 1;
                    g = pt_flags(cpts, n, p);
                }
                var c2: i64 = pt_cpt(cpts, n, p);
                while c2 == 13 || c2 == 10 {
                    p = p + 1;
                    c2 = pt_cpt(cpts, n, p);
                }
                count = pt_add(seg, cap, count, prev_end, p);
                prev_end = p;
                pos = p;
                handled = true;
            }
        }

        // 5-7. \s*[\r\n]+, \s+(?!\S), \s+
        if !handled {
            var nws: i64 = 0;
            var last_rn: i64 = 0;
            while (pt_flags(cpts, n, pos + nws) & PT_SPACE) != 0 {
                let c2: i64 = pt_cpt(cpts, n, pos + nws);
                if c2 == 13 || c2 == 10 {
                    last_rn = pos + nws + 1;
                }
                nws = nws + 1;
            }
            if last_rn > 0 {
                pos = last_rn;
                count = pt_add(seg, cap, count, prev_end, pos);
                prev_end = pos;
            } else if nws > 1 && pt_cpt(cpts, n, pos + nws) != PT_OUT {
                pos = pos + nws - 1;
                count = pt_add(seg, cap, count, prev_end, pos);
                prev_end = pos;
            } else if nws > 0 {
                pos = pos + nws;
                count = pt_add(seg, cap, count, prev_end, pos);
                prev_end = pos;
            } else {
                pos = pos + 1;
                count = pt_add(seg, cap, count, prev_end, pos);
                prev_end = pos;
            }
        }
        // Every branch moves pos forward; no shared increment exists in native.
    }
    return count;
}

// Encodes every codepoint of a segment through the byte map into `enc`.
fn pt_put_word(cpts: i64[:], cpt_off: i64, cpt_len: i64, enc: u8[:], at_in: i64) -> i64 {
    var at: i64 = at_in;
    var buf: u8[4] = [0, 0, 0, 0];
    var row: u8[4] = [0, 0, 0, 0];
    var i: i64 = 0;
    while i < cpt_len {
        let cpt: i64 = cpts[cpt_off + i];
        let n: i64 = uc_encode_body(cpt, buf, 0);
        var k: i64 = 0;
        while k < n {
            let m: i64 = uc_encode_body(uc_byte_to_cpt(buf[k]), row, 0);
            var j: i64 = 0;
            while j < m {
                enc[at] = row[j];
                at = at + 1;
                j = j + 1;
            }
            k = k + 1;
        }
        i = i + 1;
    }
    return at;
}

fn pt_qwen35_body(text: const u8[:], cpts: i64[:], seg: i64[:], enc: u8[:], info: i64[:]) -> i64 {
    if cpts.len < text.len || seg.len < text.len * 4 || enc.len < text.len * 8 || info.len < 2 {
        return -1;
    }
    let n: i64 = uc_decode_body(text, cpts);
    if n < 0 {
        return -1;
    }
    info[0] = n;
    let count: i64 = pt_scan(cpts, n, seg, text.len);
    var at: i64 = 0;
    var c: i64 = 0;
    while c < count {
        let off: i64 = seg[c * 4];
        let len: i64 = seg[c * 4 + 1];
        seg[c * 4 + 2] = at;
        at = pt_put_word(cpts, off, len, enc, at);
        seg[c * 4 + 3] = at - seg[c * 4 + 2];
        c = c + 1;
    }
    info[1] = at;
    return count;
}

export fn pt_qwen35(text: const u8[:], cpts: i64[:], seg: i64[:], enc: u8[:], info: i64[:]) -> i64 {
    return pt_qwen35_body(text, cpts, seg, enc, info);
}
