// Qwen2 pre-tokenisation: unicode_regex_split_custom_qwen2 in unicode.cpp.
//
// Differs from GPT-2 in four ways that are easy to get subtly wrong, so each is translated literally:
//   - a digit is its own token, not a run           (\p{N} rather than \p{N}+)
//   - the prefix may be any non-CRLF non-digit, not only a space
//   - letters take marks with them                  ([\p{L}\p{M}]+)
//   - the whitespace branch tracks the LAST \r or \n seen, so \s*[\r\n]+ can jump forward to it
//
// Contractions are case-insensitive, and the native code lowers the following codepoints with unicode_tolower. The
// bridge calls the same function rather than comparing ASCII cases, because the two differ on non-ASCII input and the
// gate would then be comparing different rules.

extern "q2_ld_u32" fn ld32(base: u64, byte: i64) -> i64;
extern "q2_ld_u16" fn ld16(base: u64, byte: i64) -> i64;
extern "q2_ld_i64" fn ld64(base: u64, byte: i64) -> i64;
extern "q2_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "q2_tolower" fn lower(c: i64) -> i64;
extern "q2_fail" fn fail(code: i64);

const NUMBER: i64 = 0x0002;
const LETTER: i64 = 0x0004;
const WHITESPACE: i64 = 0x0100;
const OUT_OF_RANGE: i64 = 0xFFFFFFFF;
const SPACE: i64 = 32;
const QUOTE: i64 = 39;
const CR: i64 = 13;
const LF: i64 = 10;

fn in_seg(pos: i64, ini: i64, end: i64) -> bool {
    return pos >= ini && pos < end;
}

fn cpt_at(cpts: u64, pos: i64, ini: i64, end: i64) -> i64 {
    if !in_seg(pos, ini, end) {
        return OUT_OF_RANGE;
    }
    return ld32(cpts, pos * (4 as i64));
}

fn flags_at(flags: u64, cpts: u64, pos: i64, ini: i64, end: i64) -> i64 {
    if !in_seg(pos, ini, end) {
        return 0;
    }
    let c: i64 = ld32(cpts, pos * (4 as i64));
    if c < (0 as i64) || c >= (1114112 as i64) {
        return 0;
    }
    return ld16(flags, c * (2 as i64));
}

fn is_letter(f: i64) -> bool { return (f & LETTER) != (0 as i64); }
fn is_number(f: i64) -> bool { return (f & NUMBER) != (0 as i64); }
fn is_space(f: i64) -> bool { return (f & WHITESPACE) != (0 as i64); }
fn inside(f: i64) -> bool { return f != (0 as i64); }
fn punct(f: i64) -> bool { return !is_space(f) && !is_letter(f) && !is_number(f) && inside(f); }
fn crlf(c: i64) -> bool { return c == CR || c == LF; }

// Mirrors the native helper: returns the run length and the caller adds it to pos.
fn push_count(out: u64, cap: i64, count: i64, prev_end: i64, at: i64) -> i64 {
    let len: i64 = at - prev_end;
    if len > (0 as i64) && count < cap {
        st(out, count * (8 as i64), len);
        return count + (1 as i64);
    }
    return count;
}

fn run_segment(cpts: u64, flags: u64, out: u64, cap: i64, count_in: i64, ini: i64, end: i64) -> i64 {
    var count: i64 = count_in;
    var prev_end: i64 = ini;
    var pos: i64 = ini;

    while pos < end {
        let cpt: i64 = cpt_at(cpts, pos, ini, end);
        let f: i64 = flags_at(flags, cpts, pos, ini, end);
        var handled: bool = false;

        // (?i:'s|'t|'re|'ve|'m|'ll|'d)
        if cpt == QUOTE && pos + (1 as i64) < end {
            let n1: i64 = lower(cpt_at(cpts, pos + (1 as i64), ini, end));
            if n1 == 115 || n1 == 116 || n1 == 109 || n1 == 100 {
                let at: i64 = pos + (2 as i64);
                let len: i64 = at - prev_end;
                if len > (0 as i64) && count < cap {
                    st(out, count * (8 as i64), len);
                    count = count + (1 as i64);
                }
                pos = pos + len;
                prev_end = at;
                handled = true;
            } else {
                if pos + (2 as i64) < end {
                    let n2: i64 = lower(cpt_at(cpts, pos + (2 as i64), ini, end));
                    if (n1 == 114 && n2 == 101) || (n1 == 118 && n2 == 101) || (n1 == 108 && n2 == 108) {
                        let at: i64 = pos + (3 as i64);
                        let len: i64 = at - prev_end;
                        if len > (0 as i64) && count < cap {
                            st(out, count * (8 as i64), len);
                            count = count + (1 as i64);
                        }
                        pos = pos + len;
                        prev_end = at;
                        handled = true;
                    }
                }
            }
        }

        // [^\r\n\p{L}\p{N}]?\p{L}+  -- the prefix is any character that is not CR, LF or a digit
        if !handled && !crlf(cpt) && !is_number(f) {
            let next_letter: bool = is_letter(flags_at(flags, cpts, pos + (1 as i64), ini, end));
            if is_letter(f) || next_letter {
                pos = pos + (1 as i64);
                var more: bool = is_letter(flags_at(flags, cpts, pos, ini, end));
                while more {
                    pos = pos + (1 as i64);
                    more = is_letter(flags_at(flags, cpts, pos, ini, end));
                }
                count = push_count(out, cap, count, prev_end, pos);
                prev_end = pos;
                handled = true;
            }
        }

        // \p{N} -- one digit, one token
        if !handled && is_number(f) {
            pos = pos + (1 as i64);
            count = push_count(out, cap, count, prev_end, pos);
            prev_end = pos;
            handled = true;
        }

        // <space>?[^\s\p{L}\p{N}]+[\r\n]*
        if !handled {
            var f2: i64 = f;
            if cpt == SPACE {
                f2 = flags_at(flags, cpts, pos + (1 as i64), ini, end);
            }
            // note: the native condition tests `flags` (the character at pos) for being inside the segment, while
            // testing `flags2` for the category - reproduced as written, not tidied
            if punct(f2) && inside(f) {
                if cpt == SPACE {
                    pos = pos + (1 as i64);
                }
                f2 = flags_at(flags, cpts, pos, ini, end);
                var more: bool = punct(f2);
                while more {
                    pos = pos + (1 as i64);
                    f2 = flags_at(flags, cpts, pos, ini, end);
                    more = punct(f2);
                }
                var c2: i64 = cpt_at(cpts, pos, ini, end);
                var eating: bool = crlf(c2);
                while eating {
                    pos = pos + (1 as i64);
                    c2 = cpt_at(cpts, pos, ini, end);
                    eating = crlf(c2);
                }
                count = push_count(out, cap, count, prev_end, pos);
                prev_end = pos;
                handled = true;
            }
        }

        if !handled {
            var nws: i64 = 0;
            var last_rn: i64 = 0;
            var scanning: bool = true;
            while scanning {
                let at: i64 = pos + nws;
                let ff: i64 = flags_at(flags, cpts, at, ini, end);
                if is_space(ff) {
                    let c2: i64 = cpt_at(cpts, at, ini, end);
                    if crlf(c2) {
                        last_rn = at + (1 as i64);
                    }
                    nws = nws + (1 as i64);
                } else {
                    scanning = false;
                }
            }

            if last_rn > (0 as i64) {
                pos = last_rn;
                count = push_count(out, cap, count, prev_end, pos);
                prev_end = pos;
            } else {
                if nws > (1 as i64) && cpt_at(cpts, pos + nws, ini, end) != OUT_OF_RANGE {
                    pos = pos + (nws - (1 as i64));
                    count = push_count(out, cap, count, prev_end, pos);
                    prev_end = pos;
                } else {
                    if nws > (0 as i64) {
                        pos = pos + nws;
                        count = push_count(out, cap, count, prev_end, pos);
                        prev_end = pos;
                    } else {
                        pos = pos + (1 as i64);
                        count = push_count(out, cap, count, prev_end, pos);
                        prev_end = pos;
                    }
                }
            }
        }
    }
    return count;
}

// Packet: k=0 {0 cpts, 1 n_off, 2 offsets, 3 flags_view, 4 out, 5 cap}
export fn q2_split(packet: i64) -> i64 {
    let p: u64 = packet as u64;
    let cpts: u64 = ld64(p, 0 as i64) as u64;
    let n_off: i64 = ld64(p, 8 as i64);
    let offsets: u64 = ld64(p, 16 as i64) as u64;
    let flags: u64 = ld64(p, 24 as i64) as u64;
    let out: u64 = ld64(p, 32 as i64) as u64;
    let cap: i64 = ld64(p, 40 as i64);

    var count: i64 = 0;
    var start: i64 = 0;
    var i: i64 = 0;
    while i < n_off {
        let len: i64 = ld32(offsets, i * (8 as i64));
        let end: i64 = start + len;
        count = run_segment(cpts, flags, out, cap, count, start, end);
        start = end;
        i = i + (1 as i64);
    }
    return count;
}
