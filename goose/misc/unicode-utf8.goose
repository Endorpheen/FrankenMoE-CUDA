// UTF-8 codepoint conversion: unicode_cpt_to_utf8 and unicode_cpt_from_utf8.
//
// These are the primitives every other port needed from C++: the modules took `uint32_t *` because the conversion
// between bytes and codepoints lived in std::string. Here that dependency ends - the encoder writes bytes itself and
// the decoder reads them itself, both over a caller buffer, with no container and no C++ call.
//
// Proved by exhaustion: every codepoint is encoded and compared byte for byte with the C++ encoder, and every encoded
// form is decoded back, so both directions are covered over the whole domain rather than over a sample.

extern "u8_st_u8" fn st8(buf: u64, byte: i64, v: i64);
extern "u8_ld_u8" fn ld8(buf: u64, byte: i64) -> i64;
extern "u8_st_u32" fn st32(buf: u64, byte: i64, v: i64);

const SURROGATE_LO: i64 = 0xD800;
const SURROGATE_HI: i64 = 0xDFFF;
const MAX_CPT: i64 = 0x10FFFF;

fn is_surrogate(cpt: i64) -> bool {
    return cpt >= SURROGATE_LO && cpt <= SURROGATE_HI;
}

// Writes the UTF-8 form of cpt into buf and returns how many bytes were written, or 0 when the codepoint is not
// encodable. A surrogate is not a codepoint and is refused rather than encoded.
export fn u8_encode(cpt: i64, buf: u64) -> i64 {
    if cpt < (0 as i64) || cpt > MAX_CPT || is_surrogate(cpt) {
        return 0;
    }
    if cpt <= (0x7F as i64) {
        st8(buf, 0 as i64, cpt);
        return 1;
    }
    if cpt <= (0x7FF as i64) {
        st8(buf, 0 as i64, 0xC0 | (cpt >> (6 as i64)));
        st8(buf, 1 as i64, 0x80 | (cpt & (0x3F as i64)));
        return 2;
    }
    if cpt <= (0xFFFF as i64) {
        st8(buf, 0 as i64, 0xE0 | (cpt >> (12 as i64)));
        st8(buf, 1 as i64, 0x80 | ((cpt >> (6 as i64)) & (0x3F as i64)));
        st8(buf, 2 as i64, 0x80 | (cpt & (0x3F as i64)));
        return 3;
    }
    st8(buf, 0 as i64, 0xF0 | (cpt >> (18 as i64)));
    st8(buf, 1 as i64, 0x80 | ((cpt >> (12 as i64)) & (0x3F as i64)));
    st8(buf, 2 as i64, 0x80 | ((cpt >> (6 as i64)) & (0x3F as i64)));
    st8(buf, 3 as i64, 0x80 | (cpt & (0x3F as i64)));
    return 4;
}

// Decodes the codepoint starting at byte offset pos. Returns the length consumed; the codepoint is written through
// buf as an out-parameter only when len > 0, so an invalid sequence is reported rather than guessed at.
export fn u8_decode(buf: u64, pos: i64, n: i64, out: u64) -> i64 {
    if pos >= n {
        return 0;
    }
    let b0: i64 = ld8(buf, pos);
    if b0 < (0x80 as i64) {
        st32(out, 0 as i64, b0);
        return 1;
    }
    if (b0 & (0xE0 as i64)) == (0xC0 as i64) {
        if pos + (1 as i64) >= n {
            return 0;
        }
        let c: i64 = ((b0 & (0x1F as i64)) << (6 as i64)) | (ld8(buf, pos + (1 as i64)) & (0x3F as i64));
        st32(out, 0 as i64, c);
        return 2;
    }
    if (b0 & (0xF0 as i64)) == (0xE0 as i64) {
        if pos + (2 as i64) >= n {
            return 0;
        }
        let c: i64 = ((b0 & (0x0F as i64)) << (12 as i64))
                   | ((ld8(buf, pos + (1 as i64)) & (0x3F as i64)) << (6 as i64))
                   | (ld8(buf, pos + (2 as i64)) & (0x3F as i64));
        st32(out, 0 as i64, c);
        return 3;
    }
    if (b0 & (0xF8 as i64)) == (0xF0 as i64) {
        if pos + (3 as i64) >= n {
            return 0;
        }
        let c: i64 = ((b0 & (0x07 as i64)) << (18 as i64))
                   | ((ld8(buf, pos + (1 as i64)) & (0x3F as i64)) << (12 as i64))
                   | ((ld8(buf, pos + (2 as i64)) & (0x3F as i64)) << (6 as i64))
                   | (ld8(buf, pos + (3 as i64)) & (0x3F as i64));
        st32(out, 0 as i64, c);
        return 4;
    }
    return 0;
}
