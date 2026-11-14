// P9-06 S3: thinking control, pure decision parts in Goose.
//
// Ported from runtime/core/src/engine/thinking_control.cpp:
//   - think_control_name (the three stable names)
//   - add_no_think_prefill (the shape of the synthetic continuation)
//   - probe_think_control (the decision tree over three rendered prompts)
//   - the span-closed property used by the tests
//
// Dependency, deliberately not ported: common_chat_templates_apply and
// common_chat_templates_source. The native probe renders three prompts
// (enable_thinking false, true, and the prefill variant) and reads the
// template source and its thinking tags. Those renders are the missing
// interface; this module takes the rendered strings and tags as inputs, so it
// is the pure part of the probe and can be gated as soon as a renderer exists.
//
// The prefill contract, exactly as native:
//   append one assistant message with empty content and reasoning "\n\n"
//   (whitespace only: the engine contributes no words), then set
//   continue_final_message = CONTENT (3), never AUTO: AUTO resolves a
//   reasoning-only message to a REASONING continuation, which opens the span.
//
// ThinkControl: 0 Template, 1 Prefill, 2 None.
// Continuation: 0 NONE, 1 AUTO, 2 REASONING, 3 CONTENT.

import std;

const TC_TEMPLATE: i64 = 0;
const TC_PREFILL: i64 = 1;
const TC_NONE: i64 = 2;

const TC_CONT_NONE: i64 = 0;
const TC_CONT_AUTO: i64 = 1;
const TC_CONT_REASONING: i64 = 2;
const TC_CONT_CONTENT: i64 = 3;

// flags index used by tc_add_prefill
const TC_FLAG_CONTINUATION: i64 = 0;

fn tc_eq(a: const u8[:], b: const u8[:]) -> bool {
    if a.len != b.len {
        return false;
    }
    var i: i64 = 0;
    while i < a.len {
        if a[i] != b[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn tc_find(s: const u8[:], from: i64, t: const u8[:]) -> i64 {
    if t.len == 0 {
        return from;
    }
    var i = from;
    while i + t.len <= s.len {
        var j: i64 = 0;
        var ok: i64 = 1;
        while j < t.len {
            if s[i + j] != t[j] {
                ok = 0;
                j = t.len;
            } else {
                j = j + 1;
            }
        }
        if ok == 1 {
            return i;
        }
        i = i + 1;
    }
    return -1;
}

fn tc_rfind(s: const u8[:], t: const u8[:]) -> i64 {
    if t.len == 0 || t.len > s.len {
        return -1;
    }
    var i = s.len - t.len;
    while i >= 0 {
        var j: i64 = 0;
        var ok: i64 = 1;
        while j < t.len {
            if s[i + j] != t[j] {
                ok = 0;
                j = t.len;
            } else {
                j = j + 1;
            }
        }
        if ok == 1 {
            return i;
        }
        i = i - 1;
    }
    return -1;
}

fn tc_is_ws(c: u8) -> bool {
    return c == 32 || c == 9 || c == 10 || c == 13;
}

// std::string::find_first_not_of(" \t\r\n", from)
fn tc_first_non_ws(s: const u8[:], from: i64) -> i64 {
    var i = from;
    while i < s.len {
        if !tc_is_ws(s[i]) {
            return i;
        }
        i = i + 1;
    }
    return -1;
}

// ---- names -----------------------------------------------------------------

fn tc_name_bytes(c: i64) -> const u8[:] {
    if c == TC_PREFILL {
        return "prefill";
    }
    if c == TC_NONE {
        return "none";
    }
    return "template";
}

export fn tc_name(c: i64, out: u8[:]) -> i64 {
    let s = tc_name_bytes(c);
    var i: i64 = 0;
    while i < s.len {
        if i < out.len {
            out[i] = s[i];
        }
        i = i + 1;
    }
    return s.len;
}

// ---- add_no_think_prefill --------------------------------------------------
// Flat message model: role 0 user / 1 assistant, has_reason 0/1, clen content
// length. The appended reasoning is the constant whitespace "\n\n"; its bytes
// are exposed by tc_prefill_reasoning so a caller can verify them.

export fn tc_prefill_reasoning(out: u8[:]) -> i64 {
    if out.len >= 1 {
        out[0] = 10;
    }
    if out.len >= 2 {
        out[1] = 10;
    }
    return 2;
}

export fn tc_add_prefill(
    roles: i64[:],
    has_reason: i64[:],
    clen: i64[:],
    n: i64,
    flags: i64[:]
) -> i64 {
    if n < roles.len {
        roles[n] = 1;       // assistant
        has_reason[n] = 1;  // reasoning_content is non-empty whitespace
        clen[n] = 0;        // content stays empty
    }
    flags[TC_FLAG_CONTINUATION] = TC_CONT_CONTENT;
    return n + 1;
}

// ---- probe -----------------------------------------------------------------

// off/on/prefilled are the three rendered prompts; source is the template
// source; start_tag is off_p.thinking_start_tag; ends are the end tags.
// is_null mirrors tmpls == nullptr; failed mirrors the catch path.
export fn tc_probe(
    is_null: i64,
    off: const u8[:],
    on: const u8[:],
    prefilled: const u8[:],
    source: const u8[:],
    start_tag: const u8[:],
    ends_blob: const u8[:],
    ends_off: const i64[:],
    ends_len: const i64[:],
    nends: i64,
    failed: i64,
    out_name: u8[:]
) -> i64 {
    var r: i64 = TC_TEMPLATE;
    if is_null != 0 || failed != 0 {
        r = TC_TEMPLATE;
    } else if !tc_eq(on, off) {
        // 1. The template reads enable_thinking; the flag alone is the mechanism.
        r = TC_TEMPLATE;
    } else {
        let owns_span = start_tag.len > 0 && tc_find(source, 0, start_tag) >= 0;
        if owns_span {
            // 3. The model owns its span. If the prefill moves the render past
            // the closing tag into further structure of the format, the model
            // is placed in its answer section and must obey; otherwise the
            // closed empty span is only a suggestion and None is reported.
            r = TC_NONE;
            if !tc_eq(prefilled, off) {
                var close_end: i64 = -1;
                var i: i64 = 0;
                while i < nends {
                    let tag = ends_blob[ends_off[i] .. ends_off[i] + ends_len[i]];
                    if tag.len > 0 {
                        let at = tc_rfind(prefilled, tag);
                        if at >= 0 && at + tag.len > close_end {
                            close_end = at + tag.len;
                        }
                    }
                    i = i + 1;
                }
                if close_end >= 0 && tc_first_non_ws(prefilled, close_end) >= 0 {
                    r = TC_PREFILL;
                }
            }
        } else if !tc_eq(prefilled, off) && start_tag.len == 0 {
            // 2. No declared span, and the prefill lands: reasoning is
            // structural (harmony/gpt-oss), so the turn can start past it.
            r = TC_PREFILL;
        } else {
            // No span it uses and no structural reasoning: nothing to suppress.
            r = TC_TEMPLATE;
        }
    }
    let _ = tc_name(r, out_name);
    return r;
}

// The test's expect_span_closed: the prefilled prompt contains the closing
// marker and no fresh "<think>" opens after it.
export fn tc_span_closed(prefilled: const u8[:], close_marker: const u8[:]) -> i64 {
    let at = tc_rfind(prefilled, close_marker);
    if at < 0 {
        return 0;
    }
    let tail_start = at + close_marker.len;
    let reopen = tc_find(prefilled, tail_start, "<think>");
    return if reopen < 0 { 1 } else { 0 };
}
