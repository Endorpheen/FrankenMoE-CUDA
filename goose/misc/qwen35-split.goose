// Qwen-family pre-tokenisation: unicode_regex_split_custom_qwen35.
//
// Nine rules applied in order to each caller-supplied segment; the first match consumes a token. The rule list
// and the three traps are in experiments/EXP-2026-11-17-896-qwen35-splitter-spec.
//
// The FFI boundary is one call per invocation, not per character: codepoints, segment lengths, the classification
// view from fg_unicode_flags_view, and the output array. Everything inside loops runs in Goose.
//
// Out-of-range lookahead is a sentinel, and both halves are load-bearing: codepoint 0xFFFFFFFF and flags 0. Every
// rule that looks past the segment end relies on them (rules 3, 5, 7), and rules 5 and 7 additionally test the
// sentinel explicitly because flags 0 would otherwise read as "not whitespace, not letter" and match.

extern "q35s_ld_u32" fn ld32(base: u64, byte: i64) -> i64;
// Packet fields are pointers and counts, so they need a 64-bit load. Reading them with ld32 truncates a pointer
// and turns the walk into an unbounded loop - which is exactly what the first version did.
extern "q35s_ld_i64" fn ld64(base: u64, byte: i64) -> i64;
extern "q35s_ld_u16" fn ld16(base: u64, byte: i64) -> i64;
extern "q35s_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "q35s_fail" fn fail(code: i64);

const OOR: i64 = 0xFFFFFFFF;
const F_NUMBER: i64 = 0x0002;
const F_LETTER: i64 = 0x0004;
const F_ACCENT: i64 = 0x0010;
const F_WHITE: i64 = 0x0100;

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

// Lowercase fold for the contraction rules. ASCII is all the contractions can contain, and the native
// unicode_tolower leaves ASCII unchanged apart from A-Z, so this matches for every reachable case.
fn lower(c: i64) -> i64 {
    if c >= (65 as i64) && c <= (90 as i64) {
        return c + (32 as i64);
    }
    return c;
}

fn is_letter_or_accent(f: i64) -> bool {
    return (f & F_LETTER) != (0 as i64) || (f & F_ACCENT) != (0 as i64);
}

// Runs one segment and returns the new output count.
//
// pos and prev_end move together: a token is [prev_end, pos), so finishing a token means recording pos - prev_end
// and setting prev_end = pos. That is the native _add_token plus the `pos += _add_token(...)` idiom folded into
// one assignment.
fn split_segment(cpts: u64, flags: u64, out: u64, n_out: i64, cap: i64, ini: i64, end: i64) -> i64 {
    var pos: i64 = ini;
    var prev: i64 = ini;
    var count: i64 = n_out;

    while pos < end {
        // Reads honour the segment: outside it the codepoint is the sentinel and the flags are zero.
        let cpt: i64 = if pos >= ini && pos < end { ld32(cpts, pos * (4 as i64)) } else { OOR };
        let f: i64 = if pos >= ini && pos < end { ld16(flags, pos * (2 as i64)) } else { 0 };
        // Native _get_flags(pos) with pos == end returns a default-constructed flags value, i.e. zero.
        let f_next: i64 = if pos + (1 as i64) < end { ld16(flags, (pos + (1 as i64)) * (2 as i64)) } else { 0 };
        let cpt_next: i64 = if pos + (1 as i64) < end { ld32(cpts, (pos + (1 as i64)) * (4 as i64)) } else { OOR };

        // Rule 1: (?i:'s|'t|'m|'d)
        if cpt == (39 as i64) && pos + (1 as i64) < end {
            let n1: i64 = lower(cpt_next);
            if n1 == (115 as i64) || n1 == (116 as i64) || n1 == (109 as i64) || n1 == (100 as i64) {
                if pos + (2 as i64) - prev > (0 as i64) {
                    ensure(count < cap, 20);
                    st(out, count * (8 as i64), pos + (2 as i64) - prev);
                    count = count + (1 as i64);
                }
                prev = pos + (2 as i64);
                pos = pos + (2 as i64);
                continue;
            }
        }

        // Rule 2: (?i:'re|'ve|'ll) - only reachable when rule 1 did not fire.
        if cpt == (39 as i64) && pos + (2 as i64) < end {
            let n1: i64 = lower(cpt_next);
            let n2: i64 = lower(if pos + (2 as i64) < end { ld32(cpts, (pos + (2 as i64)) * (4 as i64)) } else { OOR });
            if (n1 == (114 as i64) && n2 == (101 as i64))
                || (n1 == (118 as i64) && n2 == (101 as i64))
                || (n1 == (108 as i64) && n2 == (108 as i64)) {
                if pos + (3 as i64) - prev > (0 as i64) {
                    ensure(count < cap, 21);
                    st(out, count * (8 as i64), pos + (3 as i64) - prev);
                    count = count + (1 as i64);
                }
                prev = pos + (3 as i64);
                pos = pos + (3 as i64);
                continue;
            }
        }

        // Rule 3: [^\r\n\p{L}\p{N}]?[\p{L}\p{M}]+
        // The optional leading character is what keeps `"xfoo` as two tokens instead of three.
        if !(cpt == (13 as i64) || cpt == (10 as i64) || (f & F_NUMBER) != (0 as i64)) {
            if is_letter_or_accent(f) || is_letter_or_accent(f_next) {
                var p: i64 = pos + (1 as i64);
                while p < end && is_letter_or_accent(ld16(flags, p * (2 as i64))) {
                    p = p + (1 as i64);
                }
                if p - prev > (0 as i64) {
                    ensure(count < cap, 22);
                    st(out, count * (8 as i64), p - prev);
                    count = count + (1 as i64);
                }
                prev = p;
                pos = p;
                continue;
            }
        }

        // Rule 4: \p{N}
        if (f & F_NUMBER) != (0 as i64) {
            if pos + (1 as i64) - prev > (0 as i64) {
                ensure(count < cap, 23);
                st(out, count * (8 as i64), pos + (1 as i64) - prev);
                count = count + (1 as i64);
            }
            prev = pos + (1 as i64);
            pos = pos + (1 as i64);
            continue;
        }

        // Rule 5: <space>?[^\s\p{L}\p{M}\p{N}]+[\r\n]*
        // The `f != 0` test is the native flags.as_uint() guard: without it an out-of-range position, whose flags
        // are zero, would look like a symbol.
        var f2: i64 = if cpt == (32 as i64) { f_next } else { f };
        let is_word: bool = (f2 & F_WHITE) != (0 as i64) || (f2 & F_LETTER) != (0 as i64)
            || (f2 & F_ACCENT) != (0 as i64) || (f2 & F_NUMBER) != (0 as i64);
        if !is_word && f != (0 as i64) {
            var p: i64 = pos;
            if cpt == (32 as i64) {
                p = p + (1 as i64);
            }
            var fq: i64 = if p < end { ld16(flags, p * (2 as i64)) } else { 0 };
            var q: bool = (fq & F_WHITE) != (0 as i64) || (fq & F_LETTER) != (0 as i64)
                || (fq & F_ACCENT) != (0 as i64) || (fq & F_NUMBER) != (0 as i64);
            while !q && fq != (0 as i64) && p < end {
                p = p + (1 as i64);
                fq = if p < end { ld16(flags, p * (2 as i64)) } else { 0 };
                q = (fq & F_WHITE) != (0 as i64) || (fq & F_LETTER) != (0 as i64)
                    || (fq & F_ACCENT) != (0 as i64) || (fq & F_NUMBER) != (0 as i64);
            }
            var c2: i64 = if p < end { ld32(cpts, p * (4 as i64)) } else { OOR };
            while c2 == (13 as i64) || c2 == (10 as i64) {
                p = p + (1 as i64);
                c2 = if p < end { ld32(cpts, p * (4 as i64)) } else { OOR };
            }
            if p - prev > (0 as i64) {
                ensure(count < cap, 24);
                st(out, count * (8 as i64), p - prev);
                count = count + (1 as i64);
            }
            prev = p;
            pos = p;
            continue;
        }

        // Rules 6-8 share a whitespace scan; their order is the whole difference between them.
        var ws: i64 = 0;
        var last_nl: i64 = 0;
        while pos + ws < end && (ld16(flags, (pos + ws) * (2 as i64)) & F_WHITE) != (0 as i64) {
            let cc: i64 = ld32(cpts, (pos + ws) * (4 as i64));
            if cc == (13 as i64) || cc == (10 as i64) {
                last_nl = pos + ws + (1 as i64);
            }
            ws = ws + (1 as i64);
        }

        // Rule 6: \s*[\r\n]+
        if last_nl > (0 as i64) {
            if last_nl - prev > (0 as i64) {
                ensure(count < cap, 25);
                st(out, count * (8 as i64), last_nl - prev);
                count = count + (1 as i64);
            }
            prev = last_nl;
            pos = last_nl;
            continue;
        }

        // Rule 7: \s+(?!\S) - the run must not reach the segment end.
        if ws > (1 as i64) && pos + ws < end {
            let e7: i64 = pos + ws - (1 as i64);
            if e7 - prev > (0 as i64) {
                ensure(count < cap, 26);
                st(out, count * (8 as i64), e7 - prev);
                count = count + (1 as i64);
            }
            prev = e7;
            pos = e7;
            continue;
        }

        // Rule 8: \s+
        if ws > (0 as i64) {
            let e8: i64 = pos + ws;
            if e8 - prev > (0 as i64) {
                ensure(count < cap, 27);
                st(out, count * (8 as i64), e8 - prev);
                count = count + (1 as i64);
            }
            prev = e8;
            pos = e8;
            continue;
        }

        // Rule 9: no match, consume one codepoint.
        if pos + (1 as i64) - prev > (0 as i64) {
            ensure(count < cap, 28);
            st(out, count * (8 as i64), pos + (1 as i64) - prev);
            count = count + (1 as i64);
        }
        prev = pos + (1 as i64);
        pos = pos + (1 as i64);
    }

    return count;
}

// Packet: k=0 {0 cpts, 1 cpts_len, 2 offsets, 3 n_offsets, 4 flags, 5 out, 6 out_cap}
export fn q35s_split(packet: i64) -> i64 {
    let p: u64 = packet as u64;
    let cpts: u64 = ld64(p, 0 as i64) as u64;
    let n_off: i64 = ld64(p, 24 as i64);
    let offsets: u64 = ld64(p, 16 as i64) as u64;
    let flags: u64 = ld64(p, 32 as i64) as u64;
    let out: u64 = ld64(p, 40 as i64) as u64;
    let cap: i64 = ld64(p, 48 as i64);

    ensure(cpts != (0 as u64), 10);
    ensure(offsets != (0 as u64), 11);
    ensure(flags != (0 as u64), 12);
    ensure(out != (0 as u64), 13);
    ensure(n_off >= (0 as i64), 14);

    var count: i64 = 0;
    var start: i64 = 0;
    var i: i64 = 0;
    while i < n_off {
        let len: i64 = ld32(offsets, i * (8 as i64));
        ensure(len >= (0 as i64), 15);
        let end: i64 = start + len;
        count = split_segment(cpts, flags, out, count, cap, start, end);
        start = end;
        i = i + (1 as i64);
    }
    return count;
}
