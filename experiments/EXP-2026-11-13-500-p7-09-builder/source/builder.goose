// PEG builder decisions in Goose, from common/peg-parser.cpp:
// rule_name, sequence/choice flattening and the two rule() overloads'
// registration policy.
//
// Owned decisions:
//   - a rule name is cleaned by collapsing every maximal run of bytes outside
//     [a-zA-Z0-9-] into one '-';
//   - sequence(children) and choice(children) splice a nested parser of the same
//     kind into the parent's child list, in order;
//   - rule(name, parser) always creates one rule parser and one ref, and
//     registers the clean name to the rule;
//   - rule(name, builder_fn) returns a ref when the clean name is already
//     registered, otherwise it registers a placeholder rule (any), runs the
//     builder and registers a second, real rule parser over it, then returns a
//     ref. The registry therefore ends on the second parser.
//
// Every other builder method only stores its arguments (literal, any, space,
// until, string_content, chars, repeat, ac, gbnf, schema, peek, negate, tag,
// atomic, ref, eps, start, end and the json_*/python_* compositions).

import std;
const K_SEQ: i64 = 0;
const K_CHOICE: i64 = 1;
// rule_name: maximal invalid runs become one '-'

export fn rule_name(name: const u8[:], out: u8[:]) -> i64 {
    var i: i64 = 0;
    var n: i64 = 0;
    var run: bool = false;

    while i < name.len {
        let b = name[i];
        var ok: bool = false;

        if b >= 'a' && b <= 'z' {
            ok = true;
        }

        if b >= 'A' && b <= 'Z' {
            ok = true;
        }

        if b >= '0' && b <= '9' {
            ok = true;
        }

        if b == '-' {
            ok = true;
        }

        if ok {
            if n < out.len {
                out[n] = b;
            }
            n = n + 1;
            run = false;
        } else {
            if !run {
                if n < out.len {
                    out[n] = '-';
                }
                n = n + 1;
                run = true;
            }
        }
        i = i + 1;
    }
    return n;
}
// flatten: splice nested parsers of `want` kind (0 sequence, 1 choice) into the
// parent's child list, keeping order; other parsers are kept as they are.

export fn flatten(
    ids: const i64[:],
    n: i64,
    want: i64,
    kind: const i64[:],
    cfirst: const i64[:],
    ccount: const i64[:],
    children: const i64[:],
    out: i64[:]) -> i64 {
    var k: i64 = 0;
    var i: i64 = 0;

    while i < n {
        let id = ids[i];

        if kind[id] == want {
            var j: i64 = 0;

            while j < ccount[id] {
                if k < out.len {
                    out[k] = children[cfirst[id] + j];
                }
                k = k + 1;
                j = j + 1;
            }
        } else {
            if k < out.len {
                out[k] = id;
            }
            k = k + 1;
        }
        i = i + 1;
    }
    return k;
}
// rule registration policy. mode 0 = rule(name, parser), mode 1 = rule(name, fn).
// out: [rule parsers created, 1 when a rule parser was created (the registry is
// replaced in mode 1), refs returned]

export fn rule_plan(has_rule: i64, mode: i64, out: i64[:]) -> i64 {
    if mode == 0 {
        out[0] = 1;
        out[1] = 1;
        out[2] = 1;
        return 0;
    }

    if has_rule != 0 {
        out[0] = 0;
        out[1] = 0;
        out[2] = 1;
        return 0;
    }
    out[0] = 2;
    out[1] = 1;
    out[2] = 1;
    return 0;
}

