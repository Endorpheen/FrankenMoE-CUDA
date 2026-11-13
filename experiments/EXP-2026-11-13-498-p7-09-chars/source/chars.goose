// PEG `chars` pattern parsing in Goose, from common/peg-parser.cpp
// (parse_char_classes, parse_char_class_char, parse_hex_escape, is_hex_digit).
//
// Owned decisions:
//   - a leading '[' is dropped, then a trailing ']', then a leading '^' sets
//     negation; each step only when the byte is present;
//   - the content is walked byte by byte: an escape is `\xNN`, `\uNNNN` or
//     `\UNNNNNNNN` (any malformed hex falls back to the literal letter with the
//     escape consuming two bytes), `\n \t \r \\ \] \[`, or any other escaped
//     byte taken literally; a trailing backslash is a literal backslash;
//   - a plain byte is its own codepoint, so a multi-byte UTF-8 literal becomes
//     one range per byte (the native quirk);
//   - a '-' starts a range only when a byte follows it, otherwise it is a
//     literal and the next byte starts a new entry.
//
// An empty pattern, or a lone '[', is undefined in the native code (front()/
// back() on an empty string), so those are out of the corpus.

import std;

fn is_hex(c: u8) -> bool {
    if c >= '0' && c <= '9' {
        return true;
    }

    if c >= 'a' && c <= 'f' {
        return true;
    }

    if c >= 'A' && c <= 'F' {
        return true;
    }
    return false;
}

fn hexv(c: u8) -> i64 {
    if c >= '0' && c <= '9' {
        return (c - 48) as i64;
    }

    if c >= 'a' && c <= 'f' {
        return (c - 87) as i64;
    }

    if c >= 'A' && c <= 'F' {
        return (c - 55) as i64;
    }
    return 0;
}
// (value, consumed); consumed 0 means the escape did not parse

fn hex_escape(s: const u8[:], pos: i64, count: i64) -> i64, i64 {
    if pos + count > s.len {
        return 0, 0;
    }
    var v: i64 = 0;
    var i: i64 = 0;

    while i < count {
        let c = s[pos + i];

        if !is_hex(c) {
            return 0, 0;
        }
        v = v * 16 + hexv(c);
        i = i + 1;
    }
    return v, count;
}
// (codepoint, bytes consumed)

fn class_char(s: const u8[:], pos: i64) -> i64, i64 {
    if s[pos] == (92 as u8) && pos + 1 < s.len {
        let e = s[pos + 1];

        if e == 'x' {
            let v, n = hex_escape(s, pos + 2, 2);

            if n > 0 {
                return v, 2 + n;
            }
            return 120, 2;
        }

        if e == 'u' {
            let v, n = hex_escape(s, pos + 2, 4);

            if n > 0 {
                return v, 2 + n;
            }
            return 117, 2;
        }

        if e == 'U' {
            let v, n = hex_escape(s, pos + 2, 8);

            if n > 0 {
                return v, 2 + n;
            }
            return 85, 2;
        }

        if e == 'n' {
            return 10, 2;
        }

        if e == 't' {
            return 9, 2;
        }

        if e == 'r' {
            return 13, 2;
        }

        if e == (92 as u8) {
            return 92, 2;
        }

        if e == (93 as u8) {
            return 93, 2;
        }

        if e == (91 as u8) {
            return 91, 2;
        }
        return (e as i64), 2;
    }
    return (s[pos] as i64), 1;
}
// state: 0 negated

export fn chars_parse(pat: const u8[:], starts: i64[:], ends: i64[:], st: i64[:]) -> i64 {
    st[0] = 0;
    var off: i64 = 0;
    var len: i64 = pat.len;

    if len > 0 && pat[off] == (91 as u8) {
        off = off + 1;
        len = len - 1;
    }

    if len > 0 && pat[off + len - 1] == (93 as u8) {
        len = len - 1;
    }

    if len > 0 && pat[off] == (94 as u8) {
        st[0] = 1;
        off = off + 1;
        len = len - 1;
    }
    let s = pat[off .. off + len];
    var n: i64 = 0;
    var i: i64 = 0;

    while i < s.len {
        let start, used = class_char(s, i);
        i = i + used;

        if i + 1 < s.len && s[i] == (45 as u8) {
            let e2, used2 = class_char(s, i + 1);
            starts[n] = start;
            ends[n] = e2;
            n = n + 1;
            i = i + 1 + used2;
        } else {
            starts[n] = start;
            ends[n] = start;
            n = n + 1;
        }
    }
    return n;
}

