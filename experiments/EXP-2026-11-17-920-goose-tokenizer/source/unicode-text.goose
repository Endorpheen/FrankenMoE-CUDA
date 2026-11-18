// UTF-8 text primitives for the tokenizer, in Goose: decoding with the native
// recovery policy, encoding, the GPT-2 byte-to-codepoint map and its inverse.
//
// Ported from src/unicode.cpp: unicode_cpt_from_utf8, unicode_cpts_from_utf8,
// unicode_cpt_to_utf8, unicode_byte_to_utf8_map, unicode_cpt_to_byte_map.
//
// The recovery policy is the behaviour that matters: one bad byte produces
// exactly one U+FFFD and the offset advances by one byte, never by the length
// the lead byte claims. unicode_cpts_from_utf8 calls a trapping decoder from a
// loop that turns the trap into that recovery, so the loop below is the same
// decision written once.

const UC_FFFD: i64 = 0xFFFD;
const UC_MAX: i64 = 0x10FFFF;

// Codepoints that keep their own byte value (unicode_byte_to_utf8_map's three
// printable ranges).
fn uc_printable(ch: i64) -> bool {
    if ch >= 0x21 && ch <= 0x7E {
        return true;
    }
    if ch >= 0xA1 && ch <= 0xAC {
        return true;
    }
    if ch >= 0xAE && ch <= 0xFF {
        return true;
    }
    return false;
}

// Byte b -> the codepoint that represents it. Non-printable bytes take the next
// codepoint from 256 upward in byte order.
export fn uc_byte_to_cpt(b: i64) -> i64 {
    if b < 0 || b > 255 {
        return -1;
    }
    if uc_printable(b) {
        return b;
    }
    var n: i64 = 0;
    var ch: i64 = 0;
    while ch < b {
        if !uc_printable(ch) {
            n = n + 1;
        }
        ch = ch + 1;
    }
    return 256 + n;
}

// Codepoint -> byte, or -1 when the codepoint is not a byte code point
// (unicode_cpt_to_byte throws there; the caller reports it).
export fn uc_cpt_to_byte(cpt: i64) -> i64 {
    if cpt < 0 {
        return -1;
    }
    if uc_printable(cpt) {
        return cpt;
    }
    if cpt < 256 {
        return -1;
    }
    let want: i64 = cpt - 256;
    var n: i64 = 0;
    var ch: i64 = 0;
    while ch < 256 {
        if !uc_printable(ch) {
            if n == want {
                return ch;
            }
            n = n + 1;
        }
        ch = ch + 1;
    }
    return -1;
}

export fn uc_utf8_len(cpt: i64) -> i64 {
    if cpt < 0x80 {
        return 1;
    }
    if cpt < 0x800 {
        return 2;
    }
    if cpt < 0x10000 {
        return 3;
    }
    return 4;
}

// Appends the UTF-8 encoding of cpt at `at` and returns the new offset.
// The private body keeps the exported signature concrete: internal callers pass
// a fixed 4-byte buffer, and a view specialization would make the export
// ambiguous (the same limitation EXP919 hit).
fn uc_encode_body(cpt: i64, out: u8[:], at: i64) -> i64 {
    if cpt < 0x80 {
        out[at] = cpt as u8;
        return at + 1;
    }
    if cpt < 0x800 {
        out[at] = (0xC0 | (cpt >> 6)) as u8;
        out[at + 1] = (0x80 | (cpt & 0x3F)) as u8;
        return at + 2;
    }
    if cpt < 0x10000 {
        out[at] = (0xE0 | (cpt >> 12)) as u8;
        out[at + 1] = (0x80 | ((cpt >> 6) & 0x3F)) as u8;
        out[at + 2] = (0x80 | (cpt & 0x3F)) as u8;
        return at + 3;
    }
    out[at] = (0xF0 | (cpt >> 18)) as u8;
    out[at + 1] = (0x80 | ((cpt >> 12) & 0x3F)) as u8;
    out[at + 2] = (0x80 | ((cpt >> 6) & 0x3F)) as u8;
    out[at + 3] = (0x80 | (cpt & 0x3F)) as u8;
    return at + 4;
}

fn uc_cont(b: i64) -> bool {
    return (b & 0xC0) == 0x80;
}

// One codepoint at text[at..]. Returns the codepoint, or -1 for invalid input;
// `adv` receives the consumed length (1 on failure, as unicode_cpts_from_utf8
// advances past the single bad byte).
fn uc_cpt_from(text: const u8[:], at: i64, adv: i64[:]) -> i64 {
    if at < 0 || at >= text.len {
        adv[0] = 1;
        return -1;
    }
    // Every byte is widened to i64 BEFORE masking: inside a u8 expression the
    // mask and the shift stay 8-bit, and `(byte & 0x3f) << 12` would be zero.
    let b0: i64 = text[at];
    if (b0 & 0x80) == 0 {
        adv[0] = 1;
        return b0;
    }
    if (b0 & 0x40) == 0 {
        adv[0] = 1;
        return -1;
    }
    if (b0 & 0x20) == 0 {
        if at + 1 >= text.len {
            adv[0] = 1;
            return -1;
        }
        let b1: i64 = text[at + 1];
        if !uc_cont(b1) {
            adv[0] = 1;
            return -1;
        }
        let cpt: i64 = ((b0 & 0x1F) << 6) | (b1 & 0x3F);
        if cpt < 0x80 {
            adv[0] = 1;
            return -1;
        }
        adv[0] = 2;
        return cpt;
    }
    if (b0 & 0x10) == 0 {
        if at + 2 >= text.len {
            adv[0] = 1;
            return -1;
        }
        let b1: i64 = text[at + 1];
        let b2: i64 = text[at + 2];
        if !uc_cont(b1) || !uc_cont(b2) {
            adv[0] = 1;
            return -1;
        }
        let cpt: i64 = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
        if cpt < 0x800 {
            adv[0] = 1;
            return -1;
        }
        adv[0] = 3;
        return cpt;
    }
    if (b0 & 0x08) == 0 {
        if at + 3 >= text.len {
            adv[0] = 1;
            return -1;
        }
        let b1: i64 = text[at + 1];
        let b2: i64 = text[at + 2];
        let b3: i64 = text[at + 3];
        if !uc_cont(b1) || !uc_cont(b2) || !uc_cont(b3) {
            adv[0] = 1;
            return -1;
        }
        let cpt: i64 = ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
        if cpt < 0x10000 || cpt > UC_MAX {
            adv[0] = 1;
            return -1;
        }
        adv[0] = 4;
        return cpt;
    }
    adv[0] = 1;
    return -1;
}

// Whole-string decode. `cpts` must hold text.len entries. Returns the count, or
// -1 when the output is too small (no partial result is published then).
fn uc_decode_body(text: const u8[:], cpts: i64[:]) -> i64 {
    if cpts.len < text.len {
        return -1;
    }
    var adv: i64[1] = [1];
    var at: i64 = 0;
    var n: i64 = 0;
    while at < text.len {
        let cpt: i64 = uc_cpt_from(text, at, adv);
        if cpt < 0 {
            cpts[n] = UC_FFFD;
            at = at + 1;
        } else {
            cpts[n] = cpt;
            at = at + adv[0];
        }
        n = n + 1;
    }
    return n;
}

export fn uc_decode(text: const u8[:], cpts: i64[:]) -> i64 {
    return uc_decode_body(text, cpts);
}
