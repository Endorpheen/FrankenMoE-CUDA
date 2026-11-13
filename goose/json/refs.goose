// JSON-schema-to-GBNF reference and rule-name decisions in Goose, from
// common/json-schema-to-grammar.cpp (_resolve_ref, _add_rule, visit,
// is_reserved_name, INVALID_RULE_CHARS_RE).
//
// Owned decisions:
//   - a rule name is sanitized by collapsing every maximal run of bytes
//     outside [a-zA-Z0-9-] into one '-';
//   - a $ref fragment is everything after the first '#', or the whole ref;
//   - the ref rule name is "ref" followed by the sanitized fragment;
//   - a reserved name gets a trailing '-', an empty name becomes "root", and
//     every sub-name is the name plus '-' (nothing when the name is empty);
//   - adding a rule keeps the sanitized name when it is absent or carries the
//     same rule text, otherwise it appends the first free decimal suffix;
//   - a ref is resolved by visiting its target only when the rule does not
//     exist yet and the ref is not already being resolved; without a target the
//     error is recorded and the rule text is empty.
//
// The rule map itself, the rule text and the schema walk stay native.

import std;

fn valid_rule_byte(b: u8) -> bool {
    if b >= 'a' && b <= 'z' {
        return true;
    }

    if b >= 'A' && b <= 'Z' {
        return true;
    }

    if b >= '0' && b <= '9' {
        return true;
    }

    if b == '-' {
        return true;
    }
    return false;
}
// Copy s[start..] into out[dst..], sanitizing invalid runs to one '-'.
// Returns the written length even when out is too small (the caller sizes it).

fn sanitize_into_impl(s: const u8[:], start: i64, out: u8[:], dst: i64) -> i64 {
    var i: i64 = start;
    var n: i64 = 0;
    var in_run: bool = false;

    while i < s.len {
        let b = s[i];

        if valid_rule_byte(b) {
            if dst + n < out.len {
                out[dst + n] = b;
            }
            n = n + 1;
            in_run = false;
        } else {
            if !in_run {
                if dst + n < out.len {
                    out[dst + n] = '-';
                }
                n = n + 1;
                in_run = true;
            }
        }
        i = i + 1;
    }
    return n;
}

export fn sanitize_into(s: const u8[:], start: i64, out: u8[:], dst: i64) -> i64 {
    return sanitize_into_impl(s, start, out, dst);
}

export fn sanitize(s: const u8[:], out: u8[:]) -> i64 {
    return sanitize_into_impl(s, 0, out, 0);
}
// Index just past the first '#', or 0 when the ref has none.

fn ref_cut_impl(ref: const u8[:]) -> i64 {
    var i: i64 = 0;

    while i < ref.len {
        if ref[i] == '#' {
            return i + 1;
        }
        i = i + 1;
    }
    return 0;
}

export fn ref_cut(ref: const u8[:]) -> i64 {
    return ref_cut_impl(ref);
}
// "ref" + sanitized fragment; returns the length written into out.

export fn ref_name(ref: const u8[:], out: u8[:]) -> i64 {
    if out.len >= 3 {
        out[0] = 'r';
        out[1] = 'e';
        out[2] = 'f';
    }
    let cut = ref_cut_impl(ref);
    return 3 as i64 + sanitize_into_impl(ref, cut, out, 3);
}

fn copy_into(s: const u8[:], out: u8[:], dst: i64) -> i64 {
    var i: i64 = 0;

    while i < s.len {
        if dst + i < out.len {
            out[dst + i] = s[i];
        }
        i = i + 1;
    }
    return i;
}

fn is_reserved(name: const u8[:]) -> bool {
    let names: const u8[:][> ..] = ["root", "boolean", "decimal-part", "integral-part", "number", "integer", "value", "object", "array", "uuid", "char", "string", "null", "date", "time", "date-time", "date-string", "time-string", "date-time-string",];
    for & n in names {
        if compare(name, n) == 0 {
            return true;
        }
    }
    return false;
}
// visit's rule_name: reserved -> name + "-", empty -> "root", else the name.

export fn rule_name(name: const u8[:], out: u8[:]) -> i64 {
    if is_reserved(name) {
        let n = copy_into(name, out, 0);

        if n < out.len {
            out[n] = '-';
        }
        return n + 1;
    }

    if name.len == 0 {
        if out.len >= 4 {
            out[0] = 'r';
            out[1] = 'o';
            out[2] = 'o';
            out[3] = 't';
        }
        return 4;
    }
    return copy_into(name, out, 0);
}
// visit's sub_name: the name plus '-' when it is not empty.

export fn sub_name(name: const u8[:], out: u8[:]) -> i64 {
    let n = copy_into(name, out, 0);

    if name.len != 0 {
        if n < out.len {
            out[n] = '-';
        }
        return n + 1;
    }
    return n;
}

fn key_end(offs: const i64[:], i: i64, total: i64) -> i64 {
    if i + 1 < offs.len {
        return offs[i + 1];
    }
    return total;
}

fn range_eq(buf: const u8[:], start: i64, end: i64, cand: const u8[:], cand_len: i64) -> bool {
    if end - start != cand_len {
        return false;
    }
    var i: i64 = 0;

    while i < cand_len {
        if buf[start + i] != cand[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn find_key(keys: const u8[:], offs: const i64[:], cand: const u8[:], cand_len: i64) -> i64 {
    var i: i64 = 0;

    while i < offs.len {
        if range_eq(keys, offs[i], key_end(offs, i, keys.len), cand, cand_len) {
            return i;
        }
        i = i + 1;
    }
    return -1;
}
// Write v as decimal digits at out[dst..]; returns the digit count.

fn append_decimal(out: u8[:], dst: i64, v: i64) -> i64 {
    var digits: i64 = 1;
    var x: i64 = v;

    while x >= 10 {
        digits = digits + 1;
        x = x / 10;
    }
    var pos: i64 = dst + digits - 1;
    x = v;

    while x > 0 {
        if pos < out.len {
            out[pos] = (48 as u8) + ( (x % 10) as u8);
        }
        pos = pos - 1;
        x = x / 10;
    }

    if v == 0 {
        if dst < out.len {
            out[dst] = '0';
        }
    }
    return digits;
}
// _add_rule: returns the chosen key length in out. keys/offs/rule_ids describe
// the current rule map (rule text identity is the numeric id).

export fn add_rule(
    name: const u8[:],
    rule_id: i64,
    keys: const u8[:],
    offs: const i64[:],
    rule_ids: const i64[:],
    out: u8[:]) -> i64 {
    let esc_len = sanitize_into_impl(name, 0, out, 0);
    let found = find_key(keys, offs, out, esc_len);

    if found < 0 {
        return esc_len;
    }

    if rule_ids[found] == rule_id {
        return esc_len;
    }
    var i: i64 = 0;

    while i >= 0 {
        let total = esc_len + append_decimal(out, esc_len, i);
        let f = find_key(keys, offs, out, total);

        if f < 0 {
            return total;
        }

        if rule_ids[f] == rule_id {
            return total;
        }
        i = i + 1;
    }
    return esc_len;
}
// _resolve_ref condition: 0 use the existing name, 1 unresolved error (empty
// rule text), 2 visit the target under the ref name.

export fn resolve_action(rule_exists: i64, resolving: i64, has_target: i64, out: u8[:]) -> i64 {
    var action: i64 = 0;
    var error: i64 = 0;

    if rule_exists == 0 && resolving == 0 {
        if has_target == 0 {
            action = 1;
            error = 1;
        } else {
            action = 2;
        }
    }

    if out.len >= 2 {
        out[0] = action as u8;
        out[1] = error as u8;
    }
    return action;
}

